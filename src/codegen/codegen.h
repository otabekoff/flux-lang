#ifndef FLUX_CODEGEN_H
#define FLUX_CODEGEN_H

#include "ir/ir.h"
#include <llvm-c/Core.h>
#include <string>

#include <unordered_map>
#include <variant>

namespace flux::codegen {

class CodeGenerator {
  public:
    CodeGenerator();
    ~CodeGenerator();

    // Compile the Flux IR module into an LLVM Module
    void compile(const ir::IRModule& ir_module);

    std::string to_string() const;

  private:
    LLVMContextRef context;
    LLVMModuleRef llvm_module;
    LLVMBuilderRef builder;

    // Map Flux IR Value ID to LLVM Value
    std::unordered_map<uint32_t, LLVMValueRef> value_map;
    // Map Flux IR BasicBlock pointer to LLVM BasicBlock
    std::unordered_map<const ir::BasicBlock*, LLVMBasicBlockRef> block_map;

    LLVMValueRef get_value(const ir::ValuePtr& val);
    void compile_instruction(const ir::Instruction& inst);
};

} // namespace flux::codegen

#endif // FLUX_CODEGEN_H
