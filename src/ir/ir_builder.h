#ifndef FLUX_IR_BUILDER_H
#define FLUX_IR_BUILDER_H

#include "ir/ir.h"

#include <string>

namespace flux::ir {

/// Fluent API for constructing Flux IR programmatically.
/// Manages SSA value numbering, basic block insertion points,
/// and CFG edge bookkeeping.
class IRBuilder {
  public:
    IRBuilder();

    // ── Module ──────────────────────────────────────────────
    IRModule& module() {
        return module_;
    }
    const IRModule& module() const {
        return module_;
    }

    void set_next_id(ValueID id) {
        next_value_id_ = id;
    }

    // ── Functions ───────────────────────────────────────────
    IRFunction* create_function(const std::string& name, std::vector<ValuePtr> params,
                                std::shared_ptr<IRType> return_type);

    // ── Blocks ──────────────────────────────────────────────
    BasicBlock* create_block(const std::string& label);
    void set_insert_point(BasicBlock* bb);
    BasicBlock* current_block() const {
        return insert_point_;
    }
    IRFunction* current_function() const {
        return current_function_;
    }

    // ── Value creation ──────────────────────────────────────
    ValuePtr create_value(std::shared_ptr<IRType> type, const std::string& name = "");

    // ── Arithmetic ──────────────────────────────────────────
    ValuePtr emit_add(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_sub(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_mul(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_div(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_mod(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_neg(ValuePtr operand);

    // ── Bitwise ─────────────────────────────────────────────
    ValuePtr emit_bit_and(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_bit_or(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_bit_xor(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_shl(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_shr(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_bit_not(ValuePtr operand);

    // ── Comparison ──────────────────────────────────────────
    ValuePtr emit_eq(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_ne(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_lt(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_le(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_gt(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_ge(ValuePtr lhs, ValuePtr rhs);

    // ── Logical ─────────────────────────────────────────────
    ValuePtr emit_logic_and(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_logic_or(ValuePtr lhs, ValuePtr rhs);
    ValuePtr emit_logic_not(ValuePtr operand);

    // ── Memory ──────────────────────────────────────────────
    ValuePtr emit_alloca(std::shared_ptr<IRType> type, const std::string& name = "");
    ValuePtr emit_load(ValuePtr ptr);
    void emit_store(ValuePtr value, ValuePtr ptr);
    ValuePtr emit_get_element_ptr(ValuePtr base, ValuePtr index);
    ValuePtr emit_get_field(ValuePtr base, uint32_t field_index,
                            std::shared_ptr<IRType> field_type);

    // ── Casts ───────────────────────────────────────────────
    ValuePtr emit_int_cast(ValuePtr value, std::shared_ptr<IRType> target);
    ValuePtr emit_float_cast(ValuePtr value, std::shared_ptr<IRType> target);
    ValuePtr emit_int_to_float(ValuePtr value, std::shared_ptr<IRType> target);
    ValuePtr emit_float_to_int(ValuePtr value, std::shared_ptr<IRType> target);
    ValuePtr emit_bitcast(ValuePtr value, std::shared_ptr<IRType> target);

    // ── Control flow ────────────────────────────────────────
    void emit_br(BasicBlock* target);
    void emit_cond_br(ValuePtr condition, BasicBlock* true_bb, BasicBlock* false_bb);
    void emit_ret(ValuePtr value = nullptr);
    void emit_unreachable();

    // ── Calls ───────────────────────────────────────────────
    ValuePtr emit_call(const std::string& callee, std::vector<ValuePtr> args,
                       std::shared_ptr<IRType> return_type);
    ValuePtr emit_call_indirect(ValuePtr callee, std::vector<ValuePtr> args,
                                std::shared_ptr<IRType> return_type);

    // ── SSA / Phi ───────────────────────────────────────────
    ValuePtr emit_phi(std::shared_ptr<IRType> type,
                      std::vector<std::pair<ValuePtr, BasicBlock*>> incoming);

    // ── Aggregates ──────────────────────────────────────────
    ValuePtr emit_insert_value(ValuePtr aggregate, ValuePtr value, uint32_t index);
    ValuePtr emit_extract_value(ValuePtr aggregate, uint32_t index,
                                std::shared_ptr<IRType> field_type);
    ValuePtr emit_struct_init(const std::string& struct_name, std::vector<ValuePtr> field_values,
                              std::shared_ptr<IRType> struct_type);

    // ── Source location ─────────────────────────────────────
    void set_source_location(uint32_t line, uint32_t column);

  private:
    // Helper to emit a binary instruction
    ValuePtr emit_binary(Opcode op, ValuePtr lhs, ValuePtr rhs,
                         std::shared_ptr<IRType> result_type = nullptr);
    // Helper to emit a unary instruction
    ValuePtr emit_unary(Opcode op, ValuePtr operand, std::shared_ptr<IRType> result_type = nullptr);
    // Insert an instruction at the current insertion point
    void insert(InstructionPtr inst);
    // Add CFG edge bookkeeping
    void add_edge(BasicBlock* from, BasicBlock* to);

    IRModule module_;
    IRFunction* current_function_ = nullptr;
    BasicBlock* insert_point_ = nullptr;
    ValueID next_value_id_ = 0;
    uint32_t current_line_ = 0;
    uint32_t current_column_ = 0;
};

} // namespace flux::ir

#endif // FLUX_IR_BUILDER_H
