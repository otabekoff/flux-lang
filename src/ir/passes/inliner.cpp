#include "ir/passes/inliner.h"
#include "ir/ir_builder.h"
#include <algorithm>
#include <iostream>
#include <iterator> // for make_move_iterator
#include <vector>

namespace flux::ir {

bool InlinerPass::run(IRModule& module) {
    bool modified = false;
    for (auto& fn : module.functions) {
        if (fn->blocks.empty())
            continue;

        bool changed = true;
        while (changed) {
            changed = inline_calls_in_function(*fn, module);
            if (changed)
                modified = true;
        }
    }
    return modified;
}

bool InlinerPass::should_inline(const IRFunction& callee) {
    if (callee.blocks.size() != 1)
        return false;
    if (callee.blocks[0]->instructions.size() > 10)
        return false;
    return true;
}

static const IRFunction* find_function(const IRModule& module, const std::string& name) {
    for (const auto& fn : module.functions) {
        if (fn->name == name)
            return fn.get();
    }
    return nullptr;
}

static ValueID get_max_value_id(const IRFunction& fn) {
    ValueID max_id = 0;
    auto check_val = [&](const ValuePtr& v) {
        if (v && v->id > max_id)
            max_id = v->id;
    };

    for (const auto& param : fn.params)
        check_val(param);

    for (const auto& block : fn.blocks) {
        for (const auto& inst : block->instructions) {
            check_val(inst->result);
            for (const auto& op : inst->operands)
                check_val(op);
        }
    }
    return max_id;
}

bool InlinerPass::inline_calls_in_function(IRFunction& caller, const IRModule& module) {
    // Collect specific call instructions
    struct CallSite {
        Instruction* inst;
        BasicBlock* block;
    };
    std::vector<CallSite> sites;

    for (auto& block : caller.blocks) {
        for (auto& inst : block->instructions) {
            if (inst->opcode == Opcode::Call) {
                sites.push_back({inst.get(), block.get()});
            }
        }
    }

    for (const auto& site : sites) {
        const IRFunction* callee = find_function(module, site.inst->callee_name);
        if (callee && callee != &caller && should_inline(*callee)) {
            if (try_inline(*site.inst, caller, *callee)) {
                return true; // Restart loop
            }
        }
    }

    return false;
}

bool InlinerPass::try_inline(Instruction& call_inst, IRFunction& caller, const IRFunction& callee) {
    ValueID start_id = get_max_value_id(caller) + 1;
    IRBuilder builder;
    builder.set_next_id(start_id);

    std::unordered_map<ValueID, ValuePtr> value_map;

    if (call_inst.operands.size() != callee.params.size())
        return false;
    for (size_t i = 0; i < callee.params.size(); ++i) {
        value_map[callee.params[i]->id] = call_inst.operands[i];
    }

    std::vector<InstructionPtr> new_instructions;
    const BasicBlock* callee_block = callee.blocks[0].get();

    ValuePtr returned_value = nullptr;

    for (const auto& inst : callee_block->instructions) {
        if (inst->opcode == Opcode::Ret) {
            if (inst->operands.size() == 1) {
                auto op = inst->operands[0];
                if (value_map.count(op->id))
                    returned_value = value_map[op->id];
                else
                    returned_value = op;
            }
            continue;
        }

        // Use make_unique for InstructionPtr (unique_ptr)
        auto new_inst = std::make_unique<Instruction>();
        new_inst->opcode = inst->opcode;
        new_inst->type = inst->type;
        new_inst->callee_name = inst->callee_name;
        new_inst->field_index = inst->field_index;
        new_inst->line = call_inst.line;
        new_inst->column = call_inst.column;

        for (const auto& op : inst->operands) {
            if (value_map.count(op->id))
                new_inst->operands.push_back(value_map[op->id]);
            else
                new_inst->operands.push_back(op);
        }

        if (inst->result) {
            auto new_res = builder.create_value(inst->result->type, inst->result->name + ".i");
            new_inst->result = new_res;
            value_map[inst->result->id] = new_res;
        }

        new_instructions.push_back(std::move(new_inst));
    }

    BasicBlock* caller_block = nullptr;
    std::vector<InstructionPtr>::iterator call_it;

    for (auto& block : caller.blocks) {
        auto it = std::find_if(block->instructions.begin(), block->instructions.end(),
                               [&](const InstructionPtr& ptr) { return ptr.get() == &call_inst; });
        if (it != block->instructions.end()) {
            caller_block = block.get();
            call_it = it;
            break;
        }
    }

    if (!caller_block)
        return false;

    // Use make_move_iterator to insert unique_ptrs
    caller_block->instructions.insert(call_it, std::make_move_iterator(new_instructions.begin()),
                                      std::make_move_iterator(new_instructions.end()));

    if (call_inst.result && returned_value) {
        ValueID old_id = call_inst.result->id;
        for (auto& block : caller.blocks) {
            for (auto& inst : block->instructions) {
                for (auto& op : inst->operands) {
                    if (op->id == old_id) {
                        op = returned_value;
                    }
                }
            }
        }
    }

    // Erase original call
    auto it = std::find_if(caller_block->instructions.begin(), caller_block->instructions.end(),
                           [&](const InstructionPtr& ptr) { return ptr.get() == &call_inst; });
    if (it != caller_block->instructions.end()) {
        caller_block->instructions.erase(it);
    }

    return true;
}

} // namespace flux::ir
