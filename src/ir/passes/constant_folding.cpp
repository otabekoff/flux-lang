#include "ir/passes/constant_folding.h"

#include <cmath>
#include <variant>

namespace flux::ir {

bool ConstantFoldingPass::run(IRModule& module) {
    bool modified = false;
    for (auto& fn : module.functions) {
        if (fold_function(*fn))
            modified = true;
    }
    return modified;
}

bool ConstantFoldingPass::fold_function(IRFunction& fn) {
    bool modified = false;
    for (auto& bb : fn.blocks) {
        if (fold_block(*bb))
            modified = true;
    }
    return modified;
}

bool ConstantFoldingPass::fold_block(BasicBlock& bb) {
    bool modified = false;
    for (auto& inst : bb.instructions) {
        if (try_fold(*inst))
            modified = true;
    }
    return modified;
}

bool ConstantFoldingPass::try_fold(Instruction& inst) {
    if (!inst.result)
        return false;

    // ── Binary operations on two integer constants ──────
    if (inst.operands.size() == 2 && inst.operands[0]->is_constant &&
        inst.operands[1]->is_constant) {

        auto* lhs_int = std::get_if<int64_t>(&inst.operands[0]->constant_value);
        auto* rhs_int = std::get_if<int64_t>(&inst.operands[1]->constant_value);

        if (lhs_int && rhs_int) {
            int64_t result = 0;
            bool folded = true;

            switch (inst.opcode) {
            case Opcode::Add:
                result = *lhs_int + *rhs_int;
                break;
            case Opcode::Sub:
                result = *lhs_int - *rhs_int;
                break;
            case Opcode::Mul:
                result = *lhs_int * *rhs_int;
                break;
            case Opcode::Div:
                if (*rhs_int == 0) {
                    folded = false;
                    break;
                }
                result = *lhs_int / *rhs_int;
                break;
            case Opcode::Mod:
                if (*rhs_int == 0) {
                    folded = false;
                    break;
                }
                result = *lhs_int % *rhs_int;
                break;
            case Opcode::BitAnd:
                result = *lhs_int & *rhs_int;
                break;
            case Opcode::BitOr:
                result = *lhs_int | *rhs_int;
                break;
            case Opcode::BitXor:
                result = *lhs_int ^ *rhs_int;
                break;
            case Opcode::Shl:
                result = *lhs_int << *rhs_int;
                break;
            case Opcode::Shr:
                result = *lhs_int >> *rhs_int;
                break;
            default:
                folded = false;
                break;
            }

            if (folded) {
                inst.result->is_constant = true;
                inst.result->constant_value = result;
                inst.result->type = inst.operands[0]->type;
                return true;
            }
        }

        // ── Comparison operations on two integer constants ──
        if (lhs_int && rhs_int) {
            bool result = false;
            bool folded = true;

            switch (inst.opcode) {
            case Opcode::Eq:
                result = *lhs_int == *rhs_int;
                break;
            case Opcode::Ne:
                result = *lhs_int != *rhs_int;
                break;
            case Opcode::Lt:
                result = *lhs_int < *rhs_int;
                break;
            case Opcode::Le:
                result = *lhs_int <= *rhs_int;
                break;
            case Opcode::Gt:
                result = *lhs_int > *rhs_int;
                break;
            case Opcode::Ge:
                result = *lhs_int >= *rhs_int;
                break;
            default:
                folded = false;
                break;
            }

            if (folded) {
                inst.result->is_constant = true;
                inst.result->constant_value = result;
                inst.result->type = make_bool();
                return true;
            }
        }

        // ── Float constant folding ─────────────────────────
        auto* lhs_flt = std::get_if<double>(&inst.operands[0]->constant_value);
        auto* rhs_flt = std::get_if<double>(&inst.operands[1]->constant_value);

        if (lhs_flt && rhs_flt) {
            double result = 0.0;
            bool folded = true;

            switch (inst.opcode) {
            case Opcode::Add:
                result = *lhs_flt + *rhs_flt;
                break;
            case Opcode::Sub:
                result = *lhs_flt - *rhs_flt;
                break;
            case Opcode::Mul:
                result = *lhs_flt * *rhs_flt;
                break;
            case Opcode::Div:
                if (*rhs_flt == 0.0) {
                    folded = false;
                    break;
                }
                result = *lhs_flt / *rhs_flt;
                break;
            case Opcode::Mod:
                if (*rhs_flt == 0.0) {
                    folded = false;
                    break;
                }
                result = std::fmod(*lhs_flt, *rhs_flt);
                break;
            default:
                folded = false;
                break;
            }

            if (folded) {
                inst.result->is_constant = true;
                inst.result->constant_value = result;
                inst.result->type = inst.operands[0]->type;
                return true;
            }
        }

        // ── Boolean constant folding ───────────────────────
        auto* lhs_bool = std::get_if<bool>(&inst.operands[0]->constant_value);
        auto* rhs_bool = std::get_if<bool>(&inst.operands[1]->constant_value);

        if (lhs_bool && rhs_bool) {
            bool result = false;
            bool folded = true;

            switch (inst.opcode) {
            case Opcode::LogicAnd:
                result = *lhs_bool && *rhs_bool;
                break;
            case Opcode::LogicOr:
                result = *lhs_bool || *rhs_bool;
                break;
            default:
                folded = false;
                break;
            }

            if (folded) {
                inst.result->is_constant = true;
                inst.result->constant_value = result;
                inst.result->type = make_bool();
                return true;
            }
        }
    }

    // ── Unary operations on a single constant ───────────
    if (inst.operands.size() == 1 && inst.operands[0]->is_constant) {
        auto* val_int = std::get_if<int64_t>(&inst.operands[0]->constant_value);
        if (val_int && inst.opcode == Opcode::Neg) {
            inst.result->is_constant = true;
            inst.result->constant_value = -(*val_int);
            inst.result->type = inst.operands[0]->type;
            return true;
        }

        auto* val_bool = std::get_if<bool>(&inst.operands[0]->constant_value);
        if (val_bool && inst.opcode == Opcode::LogicNot) {
            inst.result->is_constant = true;
            inst.result->constant_value = !(*val_bool);
            inst.result->type = make_bool();
            return true;
        }

        if (val_int && inst.opcode == Opcode::BitNot) {
            inst.result->is_constant = true;
            inst.result->constant_value = ~(*val_int);
            inst.result->type = inst.operands[0]->type;
            return true;
        }
    }

    return false;
}

} // namespace flux::ir
