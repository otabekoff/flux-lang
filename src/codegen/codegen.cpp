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

    // Pass 1: Declare all functions and create their basic blocks
    for (const auto& ir_func : ir_module.functions) {
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

        if (!ir_func->is_external) {
            for (const auto& ir_block : ir_func->blocks) {
                block_map[ir_block.get()] =
                    LLVMAppendBasicBlockInContext(context, llvm_func, ir_block->label.c_str());
            }
        }
    }

    // Pass 2: Compile function bodies
    for (const auto& ir_func : ir_module.functions) {
        if (ir_func->is_external) {
            continue;
        }

        // 1. Create all instructions (and Phi nodes without incoming edges)
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

        // 2. Populate Phi incoming edges
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

LLVMValueRef CodeGenerator::get_value(ir::ValuePtr val) {
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
        if (auto p = std::get_if<std::string>(&val->constant_value))
            return LLVMBuildGlobalStringPtr(builder, p->c_str(), "strtmp");
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

    case ir::Opcode::Eq:
        result = LLVMBuildICmp(builder, LLVMIntEQ, get_value(inst.operands[0]),
                               get_value(inst.operands[1]), "eqtmp");
        break;
    case ir::Opcode::Ne:
        result = LLVMBuildICmp(builder, LLVMIntNE, get_value(inst.operands[0]),
                               get_value(inst.operands[1]), "netmp");
        break;
    case ir::Opcode::Lt:
        result = LLVMBuildICmp(builder, LLVMIntSLT, get_value(inst.operands[0]),
                               get_value(inst.operands[1]), "lttmp");
        break;
    case ir::Opcode::Le:
        result = LLVMBuildICmp(builder, LLVMIntSLE, get_value(inst.operands[0]),
                               get_value(inst.operands[1]), "letmp");
        break;
    case ir::Opcode::Gt:
        result = LLVMBuildICmp(builder, LLVMIntSGT, get_value(inst.operands[0]),
                               get_value(inst.operands[1]), "gttmp");
        break;
    case ir::Opcode::Ge:
        result = LLVMBuildICmp(builder, LLVMIntSGE, get_value(inst.operands[0]),
                               get_value(inst.operands[1]), "getmp");
        break;

    case ir::Opcode::LogicAnd:
        result = LLVMBuildAnd(builder, get_value(inst.operands[0]), get_value(inst.operands[1]),
                              "andtmp");
        break;
    case ir::Opcode::LogicOr:
        result =
            LLVMBuildOr(builder, get_value(inst.operands[0]), get_value(inst.operands[1]), "ortmp");
        break;
    case ir::Opcode::LogicNot:
        result = LLVMBuildNot(builder, get_value(inst.operands[0]), "nottmp");
        break;

    case ir::Opcode::Alloca: {
        TypeConverter tc(context);
        LLVMTypeRef allocated_type = tc.convert(*inst.type);
        result = LLVMBuildAlloca(builder, allocated_type, "allocatmp");
        break;
    }
    case ir::Opcode::Load: {
        TypeConverter tc(context);
        LLVMTypeRef type = tc.convert(*inst.type);
        result = LLVMBuildLoad2(builder, type, get_value(inst.operands[0]), "loadtmp");
        break;
    }
    case ir::Opcode::Store: {
        LLVMBuildStore(builder, get_value(inst.operands[0]), get_value(inst.operands[1]));
        break;
    }
    case ir::Opcode::Bitcast: {
        TypeConverter tc(context);
        result = LLVMBuildBitCast(builder, get_value(inst.operands[0]), tc.convert(*inst.type),
                                  "bitcasttmp");
        break;
    }
    case ir::Opcode::IntCast: {
        TypeConverter tc(context);
        result = LLVMBuildIntCast2(builder, get_value(inst.operands[0]), tc.convert(*inst.type),
                                   true, "intcasttmp");
        break;
    }
    case ir::Opcode::FloatCast: {
        TypeConverter tc(context);
        result = LLVMBuildFPCast(builder, get_value(inst.operands[0]), tc.convert(*inst.type),
                                 "fpcasttmp");
        break;
    }
    case ir::Opcode::IntToFloat: {
        TypeConverter tc(context);
        result = LLVMBuildSIToFP(builder, get_value(inst.operands[0]), tc.convert(*inst.type),
                                 "itofptmp");
        break;
    }
    case ir::Opcode::FloatToInt: {
        TypeConverter tc(context);
        result = LLVMBuildFPToSI(builder, get_value(inst.operands[0]), tc.convert(*inst.type),
                                 "fptointtmp");
        break;
    }

    case ir::Opcode::GetField: {
        TypeConverter tc(context);
        LLVMTypeRef struct_type = tc.convert(*inst.operands[0]->type->pointee);
        result = LLVMBuildStructGEP2(builder, struct_type, get_value(inst.operands[0]),
                                     inst.field_index, "fieldtmp");
        break;
    }
    case ir::Opcode::GetElementPtr: {
        TypeConverter tc(context);
        LLVMTypeRef elem_type = tc.convert(*inst.type->pointee);
        std::vector<LLVMValueRef> indices = {
            LLVMConstInt(LLVMInt32TypeInContext(context), 0, false)};
        for (const auto& op : inst.operands) {
            if (op != inst.operands[0])
                indices.push_back(get_value(op));
        }
        result = LLVMBuildGEP2(builder, elem_type, get_value(inst.operands[0]), indices.data(),
                               static_cast<unsigned>(indices.size()), "geptmp");
        break;
    }
    case ir::Opcode::ExtractValue: {
        result = LLVMBuildExtractValue(builder, get_value(inst.operands[0]), inst.field_index,
                                       "extracttmp");
        break;
    }
    case ir::Opcode::InsertValue: {
        result = LLVMBuildInsertValue(builder, get_value(inst.operands[0]),
                                      get_value(inst.operands[1]), inst.field_index, "inserttmp");
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
