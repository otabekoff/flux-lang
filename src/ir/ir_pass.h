#ifndef FLUX_IR_PASS_H
#define FLUX_IR_PASS_H

#include "ir/ir.h"

#include <string>
#include <vector>

namespace flux::ir {

/// Base class for IR transformation passes.
/// Subclasses implement `run()` which mutates the IR module in-place.
struct IRPass {
    virtual ~IRPass() = default;

    /// Human-readable name of this pass (for logging).
    virtual std::string name() const = 0;

    /// Run the pass on the given module.
    /// Returns true if the module was modified.
    virtual bool run(IRModule& module) = 0;
};

/// Run a sequence of passes on a module.
/// Returns the total number of passes that modified the module.
inline int run_passes(IRModule& module, std::vector<std::unique_ptr<IRPass>>& passes) {
    int modifications = 0;
    for (auto& pass : passes) {
        if (pass->run(module))
            ++modifications;
    }
    return modifications;
}

} // namespace flux::ir

#endif // FLUX_IR_PASS_H
