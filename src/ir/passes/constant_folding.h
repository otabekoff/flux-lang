#ifndef FLUX_IR_CONSTANT_FOLDING_H
#define FLUX_IR_CONSTANT_FOLDING_H

#include "ir/ir_pass.h"

namespace flux::ir {

/// Constant Folding Pass
/// Evaluates arithmetic/comparison/logical instructions on constant operands
/// at compile time, replacing them with constant values.
class ConstantFoldingPass : public IRPass {
  public:
    std::string name() const override {
        return "ConstantFolding";
    }
    bool run(IRModule& module) override;

  private:
    bool fold_function(IRFunction& fn);
    bool fold_block(BasicBlock& bb);
    bool try_fold(Instruction& inst);
};

} // namespace flux::ir

#endif // FLUX_IR_CONSTANT_FOLDING_H
