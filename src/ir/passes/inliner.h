#ifndef FLUX_IR_PASSES_INLINER_H
#define FLUX_IR_PASSES_INLINER_H

#include "ir/ir_pass.h"
#include <unordered_map>

namespace flux::ir {

class InlinerPass : public IRPass {
  public:
    std::string name() const override {
        return "Inliner";
    }
    bool run(IRModule& module) override;

  private:
    bool should_inline(const IRFunction& callee);
    bool inline_calls_in_function(IRFunction& caller, const IRModule& module);

    // Simplification: only inline single-block functions for now
    bool try_inline(Instruction& call_inst, IRFunction& caller, const IRFunction& callee);
};

} // namespace flux::ir

#endif // FLUX_IR_PASSES_INLINER_H
