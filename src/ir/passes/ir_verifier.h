#ifndef FLUX_IR_PASSES_VERIFIER_H
#define FLUX_IR_PASSES_VERIFIER_H

#include "ir/ir_pass.h"
#include <string>
#include <vector>

namespace flux::ir {

class IRVerifierPass : public IRPass {
  public:
    std::string name() const override {
        return "IR Verifier";
    }
    bool run(IRModule& module) override;

  private:
    void verify_function(const IRFunction& fn);
    void verify_block(const BasicBlock& bb, const IRFunction& fn);
    void verify_instruction(const Instruction& inst, const BasicBlock& bb, const IRFunction& fn);

    void error(const std::string& msg, const Instruction* inst = nullptr);

    std::vector<std::string> errors_;
};

} // namespace flux::ir

#endif // FLUX_IR_PASSES_VERIFIER_H
