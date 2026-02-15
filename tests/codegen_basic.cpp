#include "codegen/codegen.h"
#include "ir/ir.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace flux::ir;
    using namespace flux::codegen;

    std::cout << "Running codegen_basic test..." << std::endl;

    // 1. Create a dummy IR Module with an add function
    IRModule module;
    module.name = "test_module";

    auto func = std::make_unique<IRFunction>();
    func->name = "add_test";
    func->return_type = make_i32();
    func->params.push_back(std::make_shared<Value>());
    func->params.back()->id = 1;
    func->params.back()->type = make_i32();
    func->params.back()->name = "a";

    func->params.push_back(std::make_shared<Value>());
    func->params.back()->id = 2;
    func->params.back()->type = make_i32();
    func->params.back()->name = "b";

    auto block = std::make_unique<BasicBlock>();
    block->label = "entry";

    // add i32 %a, %b
    auto res_val = std::make_shared<Value>();
    res_val->id = 3;
    res_val->type = make_i32();
    res_val->name = "sum";

    auto add_inst = std::make_unique<Instruction>();
    add_inst->opcode = Opcode::Add;
    add_inst->operands = {func->params[0], func->params[1]};
    add_inst->result = res_val;
    block->instructions.push_back(std::move(add_inst));

    // ret i32 %sum
    auto ret_inst = std::make_unique<Instruction>();
    ret_inst->opcode = Opcode::Ret;
    ret_inst->operands = {res_val};
    block->instructions.push_back(std::move(ret_inst));

    func->blocks.push_back(std::move(block));
    module.functions.push_back(std::move(func));

    // 2. Run CodeGenerator
    CodeGenerator generator;
    generator.compile(module);

    // 3. Verify output
    std::string llvm_ir = generator.to_string();
    std::cout << "Generated LLVM IR:\n" << llvm_ir << std::endl;

    assert(llvm_ir.find("define i32 @add_test(i32 %0, i32 %1)") != std::string::npos);
    assert(llvm_ir.find("addtmp = add i32 %0, %1") != std::string::npos);
    assert(llvm_ir.find("ret i32 %addtmp") != std::string::npos);

    std::cout << "codegen_basic test passed!" << std::endl;
    return 0;
}
