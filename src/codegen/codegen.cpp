#include "codegen/codegen.h"
#include "codegen/type_converter.h"
#include <cstdlib>
#include <iostream>
#include <llvm-c/Core.h>
#include <variant>
#include <vector>

namespace flux::codegen {

CodeGenerator::CodeGenerator() {
    context = LLVMContextCreate();
    builder = LLVMCreateBuilderInContext(context);
    llvm_module = nullptr;
}

CodeGenerator::~CodeGenerator() {
    if (builder)
        LLVMDisposeBuilder(builder);
    if (llvm_module)
        LLVMDisposeModule(llvm_module);
    if (context)
        LLVMContextDispose(context);
}

void CodeGenerator::compile(const ir::IRModule& ir_module) {
    if (llvm_module) {
        LLVMDisposeModule(llvm_module);
    }
    llvm_module = LLVMModuleCreateWithNameInContext(ir_module.name.c_str(), context);

    TypeConverter type_converter(context);
    value_map.clear();
    block_map.clear();

    for (const auto& ir_func : ir_module.functions) {
        // 1. Create function prototype
        std::vector<LLVMTypeRef> param_types;
        for (const auto& param : ir_func->params) {
            param_types.push_back(type_converter.convert(*param->type));
        }
        LLVMTypeRef ret_type = type_converter.convert(*ir_func->return_type);
        LLVMTypeRef func_type = LLVMFunctionType(ret_type, param_types.data(),
                                                 static_cast<unsigned>(param_types.size()), 0);
        LLVMValueRef llvm_func = LLVMAddFunction(llvm_module, ir_func->name.c_str(), func_type);

        // Map parameters
        for (size_t i = 0; i < ir_func->params.size(); ++i) {
            value_map[ir_func->params[i]->id] = LLVMGetParam(llvm_func, static_cast<unsigned>(i));
        }

        // 2. Create basic blocks
        for (const auto& ir_block : ir_func->blocks) {
            block_map[ir_block.get()] =
                LLVMAppendBasicBlockInContext(context, llvm_func, ir_block->label.c_str());
        }

        // 3. Pass 1: Create all instructions (and Phi nodes without incoming edges)
        for (const auto& ir_block : ir_func->blocks) {
            LLVMPositionBuilderAtEnd(builder, block_map[ir_block.get()]);
            for (const auto& inst : ir_block->instructions) {
                if (inst->opcode == ir::Opcode::Phi) {
                    LLVMValueRef phi =
                        LLVMBuildPhi(builder, type_converter.convert(*inst->type), "phitmp");
                    value_map[inst->result->id] = phi;
                } else {
                    compile_instruction(*inst);
                }
            }
        }

        // 4. Pass 2: Populate Phi incoming edges
        for (const auto& ir_block : ir_func->blocks) {
            for (const auto& inst : ir_block->instructions) {
                if (inst->opcode == ir::Opcode::Phi) {
                    LLVMValueRef phi = value_map[inst->result->id];
                    std::vector<LLVMValueRef> incoming_values;
                    std::vector<LLVMBasicBlockRef> incoming_blocks;

                    for (const auto& [val, bb] : inst->phi_incoming) {
                        incoming_values.push_back(get_value(val));
                        incoming_blocks.push_back(block_map.at(bb));
                    }

                    LLVMAddIncoming(phi, incoming_values.data(), incoming_blocks.data(),
                                    static_cast<unsigned>(incoming_values.size()));
                }
            }
        }
    }
}

LLVMValueRef CodeGenerator::get_value(const ir::ValuePtr& val) {
    if (val->is_constant) {
        TypeConverter tc(context);
        LLVMTypeRef type = tc.convert(*val->type);
        if (auto p = std::get_if<int64_t>(&val->constant_value))
            return LLVMConstInt(type, *p, true);
        if (auto p = std::get_if<uint64_t>(&val->constant_value))
            return LLVMConstInt(type, *p, false);
        if (auto p = std::get_if<double>(&val->constant_value))
            return LLVMConstReal(type, *p);
        if (auto p = std::get_if<bool>(&val->constant_value))
            return LLVMConstInt(type, *p ? 1 : 0, false);
    }
    return value_map.at(val->id);
}

void CodeGenerator::compile_instruction(const ir::Instruction& inst) {
    LLVMValueRef result = nullptr;
    switch (inst.opcode) {
    case ir::Opcode::Add:
        result = LLVMBuildAdd(builder, get_value(inst.operands[0]), get_value(inst.operands[1]),
                              "addtmp");
        break;
    case ir::Opcode::Sub:
        result = LLVMBuildSub(builder, get_value(inst.operands[0]), get_value(inst.operands[1]),
                              "subtmp");
        break;
    case ir::Opcode::Mul:
        result = LLVMBuildMul(builder, get_value(inst.operands[0]), get_value(inst.operands[1]),
                              "multmp");
        break;
    case ir::Opcode::Div:
        result = LLVMBuildSDiv(builder, get_value(inst.operands[0]), get_value(inst.operands[1]),
                               "divtmp");
        break;

    case ir::Opcode::Alloca: {
        TypeConverter tc(context);
        LLVMTypeRef type = tc.convert(*inst.type->pointee);
        result = LLVMBuildAlloca(builder, type, "allocatmp");
        break;
    }
    case ir::Opcode::Load: {
        TypeConverter tc(context);
        LLVMTypeRef type = tc.convert(*inst.type);
        result = LLVMBuildLoad2(builder, type, get_value(inst.operands[0]), "loadtmp");
        break;
    }
    case ir::Opcode::Store: {
        LLVMBuildStore(builder, get_value(inst.operands[1]), get_value(inst.operands[0]));
        break;
    }

    case ir::Opcode::Call: {
        LLVMValueRef func = LLVMGetNamedFunction(llvm_module, inst.callee_name.c_str());
        std::vector<LLVMValueRef> args;
        for (const auto& op : inst.operands) {
            args.push_back(get_value(op));
        }

        TypeConverter tc(context);
        LLVMTypeRef ret_type = tc.convert(*inst.type);
        std::vector<LLVMTypeRef> param_types;
        for (const auto& op : inst.operands) {
            param_types.push_back(tc.convert(*op->type));
        }
        LLVMTypeRef ft = LLVMFunctionType(ret_type, param_types.data(),
                                          static_cast<unsigned>(param_types.size()), 0);

        result = LLVMBuildCall2(builder, ft, func, args.data(), static_cast<unsigned>(args.size()),
                                "calltmp");
        break;
    }

    case ir::Opcode::Br: {
        LLVMBasicBlockRef target = block_map.at(inst.true_block);
        LLVMBuildBr(builder, target);
        break;
    }

    case ir::Opcode::CondBr: {
        LLVMValueRef cond = get_value(inst.operands[0]);
        LLVMBasicBlockRef t_block = block_map.at(inst.true_block);
        LLVMBasicBlockRef f_block = block_map.at(inst.false_block);
        LLVMBuildCondBr(builder, cond, t_block, f_block);
        break;
    }

    case ir::Opcode::Ret:
        if (inst.operands.empty()) {
            LLVMBuildRetVoid(builder);
        } else {
            LLVMBuildRet(builder, get_value(inst.operands[0]));
        }
        break;
    default:
        break;
    }

    if (result && inst.result) {
        value_map[inst.result->id] = result;
    }
}

std::string CodeGenerator::to_string() const {
    if (!llvm_module)
        return "";
    char* ir = LLVMPrintModuleToString(llvm_module);
    std::string result(ir);
    LLVMDisposeMessage(ir);
    return result;
}

} // namespace flux::codegen
