#include "ir/ir_builder.h"

#include <cassert>
#include <utility>

namespace flux::ir {

IRBuilder::IRBuilder() = default;

// ── Functions ───────────────────────────────────────────────

IRFunction* IRBuilder::create_function(const std::string& name, std::vector<ValuePtr> params,
                                       std::shared_ptr<IRType> return_type) {
    auto fn = std::make_unique<IRFunction>();
    fn->name = name;
    fn->params = std::move(params);
    fn->return_type = std::move(return_type);

    for (auto& p : fn->params) {
        if (p->id == 0)
            p->id = next_value_id_++;
    }

    auto* ptr = fn.get();
    module_.functions.push_back(std::move(fn));
    current_function_ = ptr;

    // Create entry block
    auto* entry = create_block("entry");
    set_insert_point(entry);

    return ptr;
}

// ── Blocks ──────────────────────────────────────────────────

BasicBlock* IRBuilder::create_block(const std::string& label) {
    assert(current_function_ && "No active function");
    return current_function_->create_block(label);
}

void IRBuilder::set_insert_point(BasicBlock* bb) {
    insert_point_ = bb;
}

// ── Value creation ──────────────────────────────────────────

ValuePtr IRBuilder::create_value(std::shared_ptr<IRType> type, const std::string& name) {
    auto val = std::make_shared<Value>();
    val->id = next_value_id_++;
    val->type = std::move(type);
    val->name = name.empty() ? ("%" + std::to_string(val->id)) : ("%" + name);
    return val;
}

// ── Source location ─────────────────────────────────────────

void IRBuilder::set_source_location(uint32_t line, uint32_t column) {
    current_line_ = line;
    current_column_ = column;
}

// ── Helpers ─────────────────────────────────────────────────

void IRBuilder::insert(InstructionPtr inst) {
    assert(insert_point_ && "No insertion point set");
    inst->line = current_line_;
    inst->column = current_column_;
    insert_point_->instructions.push_back(std::move(inst));
}

void IRBuilder::add_edge(BasicBlock* from, BasicBlock* to) {
    from->successors.push_back(to);
    to->predecessors.push_back(from);
}

ValuePtr IRBuilder::emit_binary(Opcode op, ValuePtr lhs, ValuePtr rhs,
                                std::shared_ptr<IRType> result_type) {
    if (!result_type) {
        // For comparison ops, result is always bool
        if (op == Opcode::Eq || op == Opcode::Ne || op == Opcode::Lt || op == Opcode::Le ||
            op == Opcode::Gt || op == Opcode::Ge) {
            result_type = make_bool();
        } else {
            result_type = lhs->type; // same type as operands
        }
    }

    auto result = create_value(result_type);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = op;
    inst->result = result;
    inst->operands = {std::move(lhs), std::move(rhs)};
    inst->type = result_type;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_unary(Opcode op, ValuePtr operand, std::shared_ptr<IRType> result_type) {
    if (!result_type) {
        if (op == Opcode::LogicNot) {
            result_type = make_bool();
        } else {
            result_type = operand->type;
        }
    }

    auto result = create_value(result_type);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = op;
    inst->result = result;
    inst->operands = {std::move(operand)};
    inst->type = result_type;
    insert(std::move(inst));
    return result;
}

// ── Arithmetic ──────────────────────────────────────────────

ValuePtr IRBuilder::emit_add(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Add, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_sub(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Sub, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_mul(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Mul, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_div(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Div, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_mod(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Mod, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_neg(ValuePtr operand) {
    return emit_unary(Opcode::Neg, std::move(operand));
}

// ── Bitwise ─────────────────────────────────────────────────

ValuePtr IRBuilder::emit_bit_and(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::BitAnd, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_bit_or(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::BitOr, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_bit_xor(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::BitXor, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_shl(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Shl, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_shr(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Shr, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_bit_not(ValuePtr operand) {
    return emit_unary(Opcode::BitNot, std::move(operand));
}

// ── Comparison ──────────────────────────────────────────────

ValuePtr IRBuilder::emit_eq(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Eq, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_ne(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Ne, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_lt(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Lt, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_le(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Le, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_gt(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Gt, std::move(lhs), std::move(rhs));
}

ValuePtr IRBuilder::emit_ge(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::Ge, std::move(lhs), std::move(rhs));
}

// ── Logical ─────────────────────────────────────────────────

ValuePtr IRBuilder::emit_logic_and(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::LogicAnd, std::move(lhs), std::move(rhs), make_bool());
}

ValuePtr IRBuilder::emit_logic_or(ValuePtr lhs, ValuePtr rhs) {
    return emit_binary(Opcode::LogicOr, std::move(lhs), std::move(rhs), make_bool());
}

ValuePtr IRBuilder::emit_logic_not(ValuePtr operand) {
    return emit_unary(Opcode::LogicNot, std::move(operand), make_bool());
}

// ── Memory ──────────────────────────────────────────────────

ValuePtr IRBuilder::emit_alloca(std::shared_ptr<IRType> type, const std::string& name) {
    auto ptr_type = make_ptr(type);
    auto result = create_value(ptr_type, name);

    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Alloca;
    inst->result = result;
    inst->type = std::move(type); // the allocated type
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_load(ValuePtr ptr) {
    assert(ptr->type->kind == IRTypeKind::Ptr && "Load requires pointer operand");
    auto result_type = ptr->type->pointee;
    auto result = create_value(result_type);

    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Load;
    inst->result = result;
    inst->operands = {std::move(ptr)};
    inst->type = result_type;
    insert(std::move(inst));
    return result;
}

void IRBuilder::emit_store(ValuePtr value, ValuePtr ptr) {
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Store;
    inst->operands = {std::move(value), std::move(ptr)};
    insert(std::move(inst));
}

ValuePtr IRBuilder::emit_get_element_ptr(ValuePtr base, ValuePtr index) {
    auto result_type = base->type; // pointer to element
    if (base->type->kind == IRTypeKind::Ptr && base->type->pointee &&
        base->type->pointee->kind == IRTypeKind::Array) {
        result_type = make_ptr(base->type->pointee->element_type);
    }

    auto result = create_value(result_type);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::GetElementPtr;
    inst->result = result;
    inst->operands = {std::move(base), std::move(index)};
    inst->type = result_type;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_get_field(ValuePtr base, uint32_t field_index,
                                   std::shared_ptr<IRType> field_type) {
    auto result_type = make_ptr(field_type);
    auto result = create_value(result_type);

    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::GetField;
    inst->result = result;
    inst->operands = {std::move(base)};
    inst->field_index = field_index;
    inst->type = result_type;
    insert(std::move(inst));
    return result;
}

// ── Casts ───────────────────────────────────────────────────

ValuePtr IRBuilder::emit_int_cast(ValuePtr value, std::shared_ptr<IRType> target) {
    auto result = create_value(target);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::IntCast;
    inst->result = result;
    inst->operands = {std::move(value)};
    inst->type = target;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_float_cast(ValuePtr value, std::shared_ptr<IRType> target) {
    auto result = create_value(target);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::FloatCast;
    inst->result = result;
    inst->operands = {std::move(value)};
    inst->type = target;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_int_to_float(ValuePtr value, std::shared_ptr<IRType> target) {
    auto result = create_value(target);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::IntToFloat;
    inst->result = result;
    inst->operands = {std::move(value)};
    inst->type = target;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_float_to_int(ValuePtr value, std::shared_ptr<IRType> target) {
    auto result = create_value(target);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::FloatToInt;
    inst->result = result;
    inst->operands = {std::move(value)};
    inst->type = target;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_bitcast(ValuePtr value, std::shared_ptr<IRType> target) {
    auto result = create_value(target);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Bitcast;
    inst->result = result;
    inst->operands = {std::move(value)};
    inst->type = target;
    insert(std::move(inst));
    return result;
}

// ── Control flow ────────────────────────────────────────────

void IRBuilder::emit_br(BasicBlock* target) {
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Br;
    inst->true_block = target;
    add_edge(insert_point_, target);
    insert(std::move(inst));
}

void IRBuilder::emit_cond_br(ValuePtr condition, BasicBlock* true_bb, BasicBlock* false_bb) {
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::CondBr;
    inst->operands = {std::move(condition)};
    inst->true_block = true_bb;
    inst->false_block = false_bb;
    add_edge(insert_point_, true_bb);
    add_edge(insert_point_, false_bb);
    insert(std::move(inst));
}

void IRBuilder::emit_ret(ValuePtr value) {
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Ret;
    if (value) {
        inst->operands = {std::move(value)};
    }
    insert(std::move(inst));
}

void IRBuilder::emit_unreachable() {
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Unreachable;
    insert(std::move(inst));
}

// ── Calls ───────────────────────────────────────────────────

ValuePtr IRBuilder::emit_call(const std::string& callee, std::vector<ValuePtr> args,
                              std::shared_ptr<IRType> return_type) {
    ValuePtr result = nullptr;
    if (return_type && return_type->kind != IRTypeKind::Void) {
        result = create_value(return_type);
    }

    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Call;
    inst->result = result;
    inst->operands = std::move(args);
    inst->callee_name = callee;
    inst->type = return_type;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_call_indirect(ValuePtr callee, std::vector<ValuePtr> args,
                                       std::shared_ptr<IRType> return_type) {
    ValuePtr result = nullptr;
    if (return_type && return_type->kind != IRTypeKind::Void) {
        result = create_value(return_type);
    }

    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::CallIndirect;
    inst->result = result;
    // First operand is the callee, rest are args
    std::vector<ValuePtr> all_ops;
    all_ops.push_back(std::move(callee));
    for (auto& a : args)
        all_ops.push_back(std::move(a));
    inst->operands = std::move(all_ops);
    inst->type = return_type;
    insert(std::move(inst));
    return result;
}

// ── SSA / Phi ───────────────────────────────────────────────

ValuePtr IRBuilder::emit_phi(std::shared_ptr<IRType> type,
                             std::vector<std::pair<ValuePtr, BasicBlock*>> incoming) {
    auto result = create_value(type);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::Phi;
    inst->result = result;
    inst->phi_incoming = std::move(incoming);
    inst->type = type;
    insert(std::move(inst));
    return result;
}

// ── Aggregates ──────────────────────────────────────────────

ValuePtr IRBuilder::emit_insert_value(ValuePtr aggregate, ValuePtr value, uint32_t index) {
    auto result = create_value(aggregate->type);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::InsertValue;
    inst->result = result;
    inst->operands = {std::move(aggregate), std::move(value)};
    inst->field_index = index;
    inst->type = result->type;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_extract_value(ValuePtr aggregate, uint32_t index,
                                       std::shared_ptr<IRType> field_type) {
    auto result = create_value(field_type);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::ExtractValue;
    inst->result = result;
    inst->operands = {std::move(aggregate)};
    inst->field_index = index;
    inst->type = field_type;
    insert(std::move(inst));
    return result;
}

ValuePtr IRBuilder::emit_struct_init(const std::string& struct_name,
                                     std::vector<ValuePtr> field_values,
                                     std::shared_ptr<IRType> struct_type) {
    auto result = create_value(struct_type);
    auto inst = std::make_unique<Instruction>();
    inst->opcode = Opcode::StructInit;
    inst->result = result;
    inst->operands = std::move(field_values);
    inst->callee_name = struct_name; // reuse callee_name for struct name
    inst->type = struct_type;
    insert(std::move(inst));
    return result;
}

} // namespace flux::ir
