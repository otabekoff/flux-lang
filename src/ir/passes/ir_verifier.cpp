#include "ir/passes/ir_verifier.h"
#include "ir/ir.h"
#include <iostream>
#include <sstream>

namespace flux::ir {

bool IRVerifierPass::run(IRModule& module) {
    errors_.clear();

    for (const auto& fn : module.functions) {
        verify_function(*fn);
    }

    if (!errors_.empty()) {
        std::cerr << "IR Verifier Errors:\n";
        for (const auto& err : errors_) {
            std::cerr << "  " << err << "\n";
        }
        throw std::runtime_error("IR Verification Failed");
    }
    return false; // IR is valid (not modified)
}

void IRVerifierPass::verify_function(const IRFunction& fn) {
    if (fn.blocks.empty())
        return; // Empty function is technically allowed (external?)

    for (const auto& block : fn.blocks) {
        verify_block(*block, fn);
    }
}

void IRVerifierPass::verify_block(const BasicBlock& bb, const IRFunction& fn) {
    if (bb.instructions.empty()) {
        errors_.push_back("Function '" + fn.name + "': Block '" + bb.label +
                          "' is empty and unterminated.");
        return;
    }

    if (!bb.is_terminated()) {
        errors_.push_back("Function '" + fn.name + "': Block '" + bb.label +
                          "' is not terminated (missing ret/br).");
    }

    for (const auto& inst : bb.instructions) {
        verify_instruction(*inst, bb, fn);
    }
}

void IRVerifierPass::verify_instruction(const Instruction& inst, const BasicBlock& bb,
                                        const IRFunction& fn) {
    // Basic operand checks
    switch (inst.opcode) {
    case Opcode::Add:
    case Opcode::Sub:
    case Opcode::Mul:
    case Opcode::Div:
    case Opcode::Mod:
        if (inst.operands.size() != 2) {
            errors_.push_back("Function '" + fn.name + "': Arithmetic op requires 2 operands.");
        } else {
            auto lhs = inst.operands[0]->type;
            auto rhs = inst.operands[1]->type;
            if (lhs->kind != rhs->kind) {
                errors_.push_back("Function '" + fn.name + "': Arithmetic op type mismatch (" +
                                  lhs->name + " vs " + rhs->name + ").");
            }
        }
        break;

    case Opcode::Br:
        if (!inst.true_block) {
            errors_.push_back("Function '" + fn.name + "': Br instruction missing target block.");
        }
        break;

    case Opcode::CondBr:
        if (inst.operands.size() != 1) {
            errors_.push_back("Function '" + fn.name + "': CondBr requires condition operand.");
        } else if (inst.operands[0]->type->kind != IRTypeKind::Bool) {
            errors_.push_back("Function '" + fn.name + "': CondBr condition must be Bool.");
        }
        if (!inst.true_block || !inst.false_block) {
            errors_.push_back("Function '" + fn.name + "': CondBr missing target blocks.");
        }
        break;

    case Opcode::Ret:
        // Check return type matches function signature (if we had easy access to it here, which we
        // do via fn.return_type)
        if (inst.operands.size() > 1) {
            errors_.push_back("Function '" + fn.name + "': Ret can only have 0 or 1 operand.");
        }
        if (fn.return_type->kind == IRTypeKind::Void) {
            if (!inst.operands.empty()) {
                errors_.push_back("Function '" + fn.name + "': Void function returns a value.");
            }
        } else {
            if (inst.operands.empty()) {
                // Allow empty ret for void? No, we just checked void.
                // Actually IR might allow implicit void return? No, explicit Ret instruction.
                errors_.push_back("Function '" + fn.name + "': Non-void function returns nothing.");
            } else {
                // strict type check?
                // For now just basic check
            }
        }
        break;

    default:
        break;
    }
}

} // namespace flux::ir
