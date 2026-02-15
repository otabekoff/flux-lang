#ifndef FLUX_IR_DEAD_CODE_ELIMINATION_H
#define FLUX_IR_DEAD_CODE_ELIMINATION_H

#include "ir/ir_pass.h"

namespace flux::ir {

/// Dead Code Elimination Pass
/// 1. Removes unreachable basic blocks (not reachable from entry via CFG traversal)
/// 2. Removes instructions whose results are never used (except side-effectful ones)
class DeadCodeEliminationPass : public IRPass {
  public:
    std::string name() const override {
        return "DeadCodeElimination";
    }
    bool run(IRModule& module) override;

  private:
    bool eliminate_in_function(IRFunction& fn);
    bool remove_unreachable_blocks(IRFunction& fn);
    bool remove_unused_instructions(IRFunction& fn);

    static bool has_side_effects(Opcode op);
};

} // namespace flux::ir

#endif // FLUX_IR_DEAD_CODE_ELIMINATION_H
