#include "ir/passes/dead_code_elimination.h"

#include <algorithm>
#include <queue>
#include <unordered_set>

namespace flux::ir {

bool DeadCodeEliminationPass::run(IRModule& module) {
    bool modified = false;
    for (auto& fn : module.functions) {
        if (eliminate_in_function(*fn))
            modified = true;
    }
    return modified;
}

bool DeadCodeEliminationPass::eliminate_in_function(IRFunction& fn) {
    bool modified = false;
    if (remove_unreachable_blocks(fn))
        modified = true;
    if (remove_unused_instructions(fn))
        modified = true;
    return modified;
}

// ── Remove unreachable blocks ───────────────────────────────

bool DeadCodeEliminationPass::remove_unreachable_blocks(IRFunction& fn) {
    if (fn.blocks.empty())
        return false;

    // BFS from entry to find reachable blocks
    std::unordered_set<BasicBlock*> reachable;
    std::queue<BasicBlock*> worklist;

    if (fn.entry) {
        worklist.push(fn.entry);
        reachable.insert(fn.entry);
    }

    while (!worklist.empty()) {
        auto* bb = worklist.front();
        worklist.pop();
        for (auto* succ : bb->successors) {
            if (reachable.insert(succ).second) {
                worklist.push(succ);
            }
        }
    }

    // Remove unreachable blocks
    size_t original_size = fn.blocks.size();
    fn.blocks.erase(std::remove_if(fn.blocks.begin(), fn.blocks.end(),
                                   [&](const BasicBlockPtr& bb) {
                                       return reachable.find(bb.get()) == reachable.end();
                                   }),
                    fn.blocks.end());

    return fn.blocks.size() != original_size;
}

// ── Remove unused instructions ──────────────────────────────

bool DeadCodeEliminationPass::remove_unused_instructions(IRFunction& fn) {
    // Build a set of all used value IDs
    std::unordered_set<ValueID> used_values;

    // First pass: collect all value IDs that are used as operands
    for (auto& bb : fn.blocks) {
        for (auto& inst : bb->instructions) {
            for (const auto& op : inst->operands) {
                if (op)
                    used_values.insert(op->id);
            }
            // Phi incoming
            for (const auto& [val, _] : inst->phi_incoming) {
                if (val)
                    used_values.insert(val->id);
            }
        }
    }

    // Second pass: remove instructions whose results are unused
    // (except side-effectful instructions)
    bool modified = false;
    for (auto& bb : fn.blocks) {
        auto it = bb->instructions.begin();
        while (it != bb->instructions.end()) {
            auto& inst = *it;
            if (inst->result && !has_side_effects(inst->opcode) &&
                used_values.find(inst->result->id) == used_values.end()) {
                it = bb->instructions.erase(it);
                modified = true;
            } else {
                ++it;
            }
        }
    }

    return modified;
}

// ── Side effects check ──────────────────────────────────────

bool DeadCodeEliminationPass::has_side_effects(Opcode op) {
    switch (op) {
    case Opcode::Store:
    case Opcode::Call:
    case Opcode::CallIndirect:
    case Opcode::Br:
    case Opcode::CondBr:
    case Opcode::Switch:
    case Opcode::Ret:
    case Opcode::Unreachable:
        return true;
    default:
        return false;
    }
}

} // namespace flux::ir
