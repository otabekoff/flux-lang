#include "ir/ir_lowering.h"

#include "ast/ast.h"
#include "lexer/token.h"

#include <cassert>
#include <stdexcept>
#include <utility>

namespace flux::ir {

IRLowering::IRLowering() = default;

// ── Type conversion ─────────────────────────────────────────

std::shared_ptr<IRType> IRLowering::lower_type(const std::string& type_name) {
    if (type_name == "Int8")
        return make_i8();
    if (type_name == "Int16")
        return make_i16();
    if (type_name == "Int32")
        return make_i32();
    if (type_name == "Int64")
        return make_i64();
    if (type_name == "Int128")
        return make_i128();
    if (type_name == "UInt8")
        return make_u8();
    if (type_name == "UInt16")
        return make_u16();
    if (type_name == "UInt32")
        return make_u32();
    if (type_name == "UInt64")
        return make_u64();
    if (type_name == "UInt128")
        return make_u128();
    if (type_name == "Float32")
        return make_f32();
    if (type_name == "Float64")
        return make_f64();
    if (type_name == "Float128")
        return make_f128();
    if (type_name == "Bool")
        return make_bool();
    if (type_name == "Void")
        return make_void();
    if (type_name == "Never")
        return make_never();
    if (type_name == "String")
        return make_ptr(make_u8()); // Simplification: String = &[u8]

    // Struct / enum / other named type
    auto t = std::make_shared<IRType>();
    t->kind = IRTypeKind::Struct;
    t->name = type_name;
    return t;
}

std::shared_ptr<IRType> IRLowering::lower_flux_type(const semantic::FluxType& type) {
    return lower_type(type.name);
}

// ── Scope management ────────────────────────────────────────

void IRLowering::enter_scope() {
    var_scopes_.emplace_back();
}

void IRLowering::exit_scope() {
    if (!var_scopes_.empty())
        var_scopes_.pop_back();
}

ValuePtr IRLowering::lookup_variable(const std::string& name) {
    for (auto it = var_scopes_.rbegin(); it != var_scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end())
            return found->second;
    }
    return nullptr;
}

void IRLowering::declare_variable(const std::string& name, ValuePtr alloca_ptr) {
    if (!var_scopes_.empty()) {
        var_scopes_.back()[name] = std::move(alloca_ptr);
    }
}

// ── Loop context ────────────────────────────────────────────

void IRLowering::push_loop(BasicBlock* continue_bb, BasicBlock* break_bb) {
    loop_stack_.push_back({continue_bb, break_bb});
}

void IRLowering::pop_loop() {
    if (!loop_stack_.empty())
        loop_stack_.pop_back();
}

// ── Unique labels ───────────────────────────────────────────

std::string IRLowering::unique_label(const std::string& prefix) {
    return prefix + "." + std::to_string(label_counter_++);
}

// ============================================================
//  Module lowering
// ============================================================

IRModule IRLowering::lower(const ast::Module& module) {
    builder_.module().name = module.name;

    for (const auto& fn : module.functions) {
        lower_function(fn);
    }

    return std::move(builder_.module());
}

// ============================================================
//  Function lowering
// ============================================================

void IRLowering::lower_function(const ast::FunctionDecl& fn) {
    // Build parameter values
    std::vector<ValuePtr> params;
    for (const auto& p : fn.params) {
        auto param_val = std::make_shared<Value>();
        param_val->id = 0; // will be reassigned by builder
        param_val->type = lower_type(p.type);
        param_val->name = "%" + p.name;
        params.push_back(param_val);
    }

    auto ret_type = lower_type(fn.return_type);
    auto* ir_fn = builder_.create_function(fn.name, std::move(params), ret_type);
    ir_fn->is_async = fn.is_async;
    ir_fn->line = fn.line;
    ir_fn->column = fn.column;

    enter_scope();

    // Allocate params as local variables so they can be addressed
    for (size_t i = 0; i < fn.params.size(); ++i) {
        auto alloca = builder_.emit_alloca(ir_fn->params[i]->type, fn.params[i].name);
        builder_.emit_store(ir_fn->params[i], alloca);
        declare_variable(fn.params[i].name, alloca);
    }

    // Lower body
    if (fn.has_body) {
        lower_block(fn.body);
    }

    // Ensure function is properly terminated
    if (builder_.current_block() && !builder_.current_block()->is_terminated()) {
        if (ret_type->kind == IRTypeKind::Void) {
            builder_.emit_ret();
        } else {
            builder_.emit_unreachable();
        }
    }

    exit_scope();
}

// ============================================================
//  Block lowering
// ============================================================

void IRLowering::lower_block(const ast::Block& block) {
    enter_scope();
    for (const auto& stmt : block.statements) {
        lower_statement(*stmt);
        // If the current block is terminated, stop processing remaining statements
        if (builder_.current_block() && builder_.current_block()->is_terminated())
            break;
    }
    exit_scope();
}

// ============================================================
//  Statement dispatch
// ============================================================

void IRLowering::lower_statement(const ast::Stmt& stmt) {
    builder_.set_source_location(stmt.line, stmt.column);

    if (auto* let_s = dynamic_cast<const ast::LetStmt*>(&stmt)) {
        lower_let_stmt(*let_s);
    } else if (auto* ret_s = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        lower_return_stmt(*ret_s);
    } else if (auto* assign_s = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
        lower_assign_stmt(*assign_s);
    } else if (auto* if_s = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        lower_if_stmt(*if_s);
    } else if (auto* while_s = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        lower_while_stmt(*while_s);
    } else if (auto* for_s = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        lower_for_stmt(*for_s);
    } else if (auto* loop_s = dynamic_cast<const ast::LoopStmt*>(&stmt)) {
        lower_loop_stmt(*loop_s);
    } else if (auto* match_s = dynamic_cast<const ast::MatchStmt*>(&stmt)) {
        lower_match_stmt(*match_s);
    } else if (auto* break_s = dynamic_cast<const ast::BreakStmt*>(&stmt)) {
        (void)break_s;
        lower_break_stmt();
    } else if (auto* cont_s = dynamic_cast<const ast::ContinueStmt*>(&stmt)) {
        (void)cont_s;
        lower_continue_stmt();
    } else if (auto* expr_s = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        lower_expr_stmt(*expr_s);
    } else if (auto* block_s = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
        lower_block_stmt(*block_s);
    }
    // Other statement types (struct decl, trait decl, etc.) are type-system-only
    // and don't generate IR instructions.
}

// ============================================================
//  Statement lowering implementations
// ============================================================

void IRLowering::lower_let_stmt(const ast::LetStmt& stmt) {
    auto var_type = lower_type(stmt.type_name);

    if (!stmt.tuple_names.empty()) {
        // Tuple destructuring
        if (stmt.initializer) {
            auto init_val = lower_expression(*stmt.initializer);
            for (size_t i = 0; i < stmt.tuple_names.size(); ++i) {
                auto alloca = builder_.emit_alloca(var_type, stmt.tuple_names[i]);
                auto elem =
                    builder_.emit_extract_value(init_val, static_cast<uint32_t>(i), var_type);
                builder_.emit_store(elem, alloca);
                declare_variable(stmt.tuple_names[i], alloca);
            }
        }
    } else {
        // Single variable
        auto alloca = builder_.emit_alloca(var_type, stmt.name);
        if (stmt.initializer) {
            auto init_val = lower_expression(*stmt.initializer);
            builder_.emit_store(init_val, alloca);
        }
        declare_variable(stmt.name, alloca);
    }
}

void IRLowering::lower_assign_stmt(const ast::AssignStmt& stmt) {
    auto rhs = lower_expression(*stmt.value);

    // Get the address of the target
    if (auto* ident = dynamic_cast<const ast::IdentifierExpr*>(stmt.target.get())) {
        auto ptr = lookup_variable(ident->name);
        if (!ptr)
            return; // unresolved, skip

        if (stmt.op == TokenKind::Assign) {
            builder_.emit_store(rhs, ptr);
        } else {
            // Compound assignment: load, compute, store
            auto current = builder_.emit_load(ptr);
            ValuePtr result;
            switch (stmt.op) {
            case TokenKind::PlusAssign:
                result = builder_.emit_add(current, rhs);
                break;
            case TokenKind::MinusAssign:
                result = builder_.emit_sub(current, rhs);
                break;
            case TokenKind::StarAssign:
                result = builder_.emit_mul(current, rhs);
                break;
            case TokenKind::SlashAssign:
                result = builder_.emit_div(current, rhs);
                break;
            case TokenKind::PercentAssign:
                result = builder_.emit_mod(current, rhs);
                break;
            case TokenKind::AmpAssign:
                result = builder_.emit_bit_and(current, rhs);
                break;
            case TokenKind::PipeAssign:
                result = builder_.emit_bit_or(current, rhs);
                break;
            case TokenKind::CaretAssign:
                result = builder_.emit_bit_xor(current, rhs);
                break;
            default:
                result = rhs;
                break;
            }
            builder_.emit_store(result, ptr);
        }
    }
    // Member access and index targets will be handled with GetField/GEP in the future
}

void IRLowering::lower_return_stmt(const ast::ReturnStmt& stmt) {
    if (stmt.expression) {
        auto val = lower_expression(*stmt.expression);
        builder_.emit_ret(val);
    } else {
        builder_.emit_ret();
    }
}

void IRLowering::lower_if_stmt(const ast::IfStmt& stmt) {
    auto cond = lower_expression(*stmt.condition);

    auto* then_bb = builder_.create_block(unique_label("if.then"));
    auto* else_bb = stmt.else_branch ? builder_.create_block(unique_label("if.else")) : nullptr;
    auto* merge_bb = builder_.create_block(unique_label("if.merge"));

    builder_.emit_cond_br(cond, then_bb, else_bb ? else_bb : merge_bb);

    // Then branch
    builder_.set_insert_point(then_bb);
    lower_statement(*stmt.then_branch);
    if (!builder_.current_block()->is_terminated()) {
        builder_.emit_br(merge_bb);
    }

    // Else branch
    if (stmt.else_branch) {
        builder_.set_insert_point(else_bb);
        lower_statement(*stmt.else_branch);
        if (!builder_.current_block()->is_terminated()) {
            builder_.emit_br(merge_bb);
        }
    }

    builder_.set_insert_point(merge_bb);
}

void IRLowering::lower_while_stmt(const ast::WhileStmt& stmt) {
    auto* header_bb = builder_.create_block(unique_label("while.header"));
    auto* body_bb = builder_.create_block(unique_label("while.body"));
    auto* exit_bb = builder_.create_block(unique_label("while.exit"));

    builder_.emit_br(header_bb);

    // Header: evaluate condition
    builder_.set_insert_point(header_bb);
    auto cond = lower_expression(*stmt.condition);
    builder_.emit_cond_br(cond, body_bb, exit_bb);

    // Body
    push_loop(header_bb, exit_bb);
    builder_.set_insert_point(body_bb);
    lower_statement(*stmt.body);
    if (!builder_.current_block()->is_terminated()) {
        builder_.emit_br(header_bb);
    }
    pop_loop();

    builder_.set_insert_point(exit_bb);
}

void IRLowering::lower_for_stmt(const ast::ForStmt& stmt) {
    // For loops are desugared to while-style:
    //   let iter = iterable.iter();
    //   while (iter.has_next()) { let var = iter.next(); body; }
    // For now, we lower the for loop into a simple counted loop structure
    // that's enough for range-based iteration.

    auto* header_bb = builder_.create_block(unique_label("for.header"));
    auto* body_bb = builder_.create_block(unique_label("for.body"));
    auto* exit_bb = builder_.create_block(unique_label("for.exit"));

    // Allocate loop variable
    auto var_type = stmt.var_type.empty() ? make_i32() : lower_type(stmt.var_type);
    auto alloca = builder_.emit_alloca(var_type, stmt.variable);
    declare_variable(stmt.variable, alloca);

    // Initialize from iterable (simplified: assume iterable evaluates to a range start)
    auto iterable = lower_expression(*stmt.iterable);
    builder_.emit_store(iterable, alloca);

    builder_.emit_br(header_bb);

    // Header: check loop condition (simplified — always true for now, will be refined with range)
    builder_.set_insert_point(header_bb);
    auto current = builder_.emit_load(alloca);
    // For range-based for, we'd compare against the end value.
    // For now, emit a placeholder branch that the exit condition is managed by break.
    auto cond = make_const_bool(true);
    builder_.emit_cond_br(cond, body_bb, exit_bb);

    // Body
    push_loop(header_bb, exit_bb);
    builder_.set_insert_point(body_bb);
    lower_statement(*stmt.body);
    if (!builder_.current_block()->is_terminated()) {
        // Increment loop variable
        auto val = builder_.emit_load(alloca);
        auto one = make_const_i32(1);
        auto next = builder_.emit_add(val, one);
        builder_.emit_store(next, alloca);
        builder_.emit_br(header_bb);
    }
    pop_loop();

    builder_.set_insert_point(exit_bb);
}

void IRLowering::lower_loop_stmt(const ast::LoopStmt& stmt) {
    auto* header_bb = builder_.create_block(unique_label("loop.header"));
    auto* exit_bb = builder_.create_block(unique_label("loop.exit"));

    builder_.emit_br(header_bb);

    push_loop(header_bb, exit_bb);
    builder_.set_insert_point(header_bb);
    lower_statement(*stmt.body);
    if (!builder_.current_block()->is_terminated()) {
        builder_.emit_br(header_bb);
    }
    pop_loop();

    builder_.set_insert_point(exit_bb);
}

void IRLowering::lower_match_stmt(const ast::MatchStmt& stmt) {
    auto subject = lower_expression(*stmt.expression);
    auto* merge_bb = builder_.create_block(unique_label("match.merge"));

    // Lower each arm as a chain of conditional branches
    for (size_t i = 0; i < stmt.arms.size(); ++i) {
        const auto& arm = stmt.arms[i];
        auto* arm_bb = builder_.create_block(unique_label("match.arm." + std::to_string(i)));
        auto* next_bb = (i + 1 < stmt.arms.size())
                            ? builder_.create_block(unique_label("match.next." + std::to_string(i)))
                            : merge_bb;

        // Check if pattern matches (simplified: wildcard always matches)
        if (dynamic_cast<const ast::WildcardPattern*>(arm.pattern.get())) {
            builder_.emit_br(arm_bb);
        } else if (auto* lit_pat = dynamic_cast<const ast::LiteralPattern*>(arm.pattern.get())) {
            auto pat_val = lower_expression(*lit_pat->literal);
            auto cmp = builder_.emit_eq(subject, pat_val);
            if (arm.guard) {
                // Guard: match_cond = pattern_match && guard_expr
                auto* guard_bb =
                    builder_.create_block(unique_label("match.guard." + std::to_string(i)));
                builder_.emit_cond_br(cmp, guard_bb, next_bb);
                builder_.set_insert_point(guard_bb);
                auto guard_val = lower_expression(*arm.guard);
                builder_.emit_cond_br(guard_val, arm_bb, next_bb);
            } else {
                builder_.emit_cond_br(cmp, arm_bb, next_bb);
            }
        } else if (auto* ident_pat =
                       dynamic_cast<const ast::IdentifierPattern*>(arm.pattern.get())) {
            // Bind subject to name and always match
            auto alloca = builder_.emit_alloca(subject->type, ident_pat->name);
            builder_.emit_store(subject, alloca);
            declare_variable(ident_pat->name, alloca);
            builder_.emit_br(arm_bb);
        } else {
            // For variant/struct/tuple patterns — simplified: branch unconditionally
            builder_.emit_br(arm_bb);
        }

        // Arm body
        builder_.set_insert_point(arm_bb);
        lower_statement(*arm.body);
        if (!builder_.current_block()->is_terminated()) {
            builder_.emit_br(merge_bb);
        }

        // Move to next check
        if (i + 1 < stmt.arms.size()) {
            builder_.set_insert_point(next_bb);
        }
    }

    builder_.set_insert_point(merge_bb);
}

void IRLowering::lower_break_stmt() {
    if (!loop_stack_.empty()) {
        builder_.emit_br(loop_stack_.back().break_target);
    }
}

void IRLowering::lower_continue_stmt() {
    if (!loop_stack_.empty()) {
        builder_.emit_br(loop_stack_.back().continue_target);
    }
}

void IRLowering::lower_expr_stmt(const ast::ExprStmt& stmt) {
    if (stmt.expression) {
        lower_expression(*stmt.expression); // result discarded
    }
}

void IRLowering::lower_block_stmt(const ast::BlockStmt& stmt) {
    lower_block(stmt.block);
}

// ============================================================
//  Expression dispatch
// ============================================================

ValuePtr IRLowering::lower_expression(const ast::Expr& expr) {
    builder_.set_source_location(expr.line, expr.column);

    if (auto* num = dynamic_cast<const ast::NumberExpr*>(&expr))
        return lower_number_expr(*num);
    if (auto* str = dynamic_cast<const ast::StringExpr*>(&expr))
        return lower_string_expr(*str);
    if (auto* bl = dynamic_cast<const ast::BoolExpr*>(&expr))
        return lower_bool_expr(*bl);
    if (auto* ch = dynamic_cast<const ast::CharExpr*>(&expr))
        return lower_char_expr(*ch);
    if (auto* id = dynamic_cast<const ast::IdentifierExpr*>(&expr))
        return lower_identifier_expr(*id);
    if (auto* bin = dynamic_cast<const ast::BinaryExpr*>(&expr))
        return lower_binary_expr(*bin);
    if (auto* un = dynamic_cast<const ast::UnaryExpr*>(&expr))
        return lower_unary_expr(*un);
    if (auto* call = dynamic_cast<const ast::CallExpr*>(&expr))
        return lower_call_expr(*call);
    if (auto* mem = dynamic_cast<const ast::MemberAccessExpr*>(&expr))
        return lower_member_access_expr(*mem);
    if (auto* idx = dynamic_cast<const ast::IndexExpr*>(&expr))
        return lower_index_expr(*idx);
    if (auto* cast = dynamic_cast<const ast::CastExpr*>(&expr))
        return lower_cast_expr(*cast);
    if (auto* sl = dynamic_cast<const ast::StructLiteralExpr*>(&expr))
        return lower_struct_literal_expr(*sl);
    if (auto* tup = dynamic_cast<const ast::TupleExpr*>(&expr))
        return lower_tuple_expr(*tup);
    if (auto* arr = dynamic_cast<const ast::ArrayExpr*>(&expr))
        return lower_array_expr(*arr);
    if (auto* lam = dynamic_cast<const ast::LambdaExpr*>(&expr))
        return lower_lambda_expr(*lam);

    // Fallback: return a void/unknown value
    return builder_.create_value(make_void(), "unknown");
}

// ============================================================
//  Expression lowering implementations
// ============================================================

ValuePtr IRLowering::lower_number_expr(const ast::NumberExpr& expr) {
    // Try to determine if it's float or int
    if (expr.value.find('.') != std::string::npos) {
        return make_const_f64(std::stod(expr.value));
    }
    return make_const_i32(std::stoi(expr.value));
}

ValuePtr IRLowering::lower_string_expr(const ast::StringExpr& expr) {
    return make_const_string(expr.value);
}

ValuePtr IRLowering::lower_bool_expr(const ast::BoolExpr& expr) {
    return make_const_bool(expr.value);
}

ValuePtr IRLowering::lower_char_expr(const ast::CharExpr& expr) {
    auto val = std::make_shared<Value>();
    val->type = make_u8();
    val->is_constant = true;
    val->constant_value = static_cast<int64_t>(expr.value.empty() ? 0 : expr.value[0]);
    return val;
}

ValuePtr IRLowering::lower_identifier_expr(const ast::IdentifierExpr& expr) {
    auto ptr = lookup_variable(expr.name);
    if (ptr) {
        return builder_.emit_load(ptr);
    }
    // Could be a function reference or enum variant
    return builder_.create_value(make_i32(), expr.name);
}

ValuePtr IRLowering::lower_binary_expr(const ast::BinaryExpr& expr) {
    auto lhs = lower_expression(*expr.left);
    auto rhs = lower_expression(*expr.right);

    switch (expr.op) {
    case TokenKind::Plus:
        return builder_.emit_add(lhs, rhs);
    case TokenKind::Minus:
        return builder_.emit_sub(lhs, rhs);
    case TokenKind::Star:
        return builder_.emit_mul(lhs, rhs);
    case TokenKind::Slash:
        return builder_.emit_div(lhs, rhs);
    case TokenKind::Percent:
        return builder_.emit_mod(lhs, rhs);
    case TokenKind::EqualEqual:
        return builder_.emit_eq(lhs, rhs);
    case TokenKind::BangEqual:
        return builder_.emit_ne(lhs, rhs);
    case TokenKind::Less:
        return builder_.emit_lt(lhs, rhs);
    case TokenKind::LessEqual:
        return builder_.emit_le(lhs, rhs);
    case TokenKind::Greater:
        return builder_.emit_gt(lhs, rhs);
    case TokenKind::GreaterEqual:
        return builder_.emit_ge(lhs, rhs);
    case TokenKind::AmpAmp:
        return builder_.emit_logic_and(lhs, rhs);
    case TokenKind::PipePipe:
        return builder_.emit_logic_or(lhs, rhs);
    case TokenKind::Amp:
        return builder_.emit_bit_and(lhs, rhs);
    case TokenKind::Pipe:
        return builder_.emit_bit_or(lhs, rhs);
    case TokenKind::Caret:
        return builder_.emit_bit_xor(lhs, rhs);
    case TokenKind::ShiftLeft:
        return builder_.emit_shl(lhs, rhs);
    case TokenKind::ShiftRight:
        return builder_.emit_shr(lhs, rhs);
    default:
        return builder_.create_value(lhs->type, "binop.unknown");
    }
}

ValuePtr IRLowering::lower_unary_expr(const ast::UnaryExpr& expr) {
    auto operand = lower_expression(*expr.operand);

    switch (expr.op) {
    case TokenKind::Minus:
        return builder_.emit_neg(operand);
    case TokenKind::Bang:
        return builder_.emit_logic_not(operand);
    case TokenKind::Tilde:
        return builder_.emit_bit_not(operand);
    case TokenKind::Amp:
        return operand; // Address-of: operand is already a ptr in our model
    case TokenKind::Star:
        return builder_.emit_load(operand); // Dereference
    default:
        return operand;
    }
}

ValuePtr IRLowering::lower_call_expr(const ast::CallExpr& expr) {
    // Get callee name
    std::string callee_name = "unknown";
    if (auto* id = dynamic_cast<const ast::IdentifierExpr*>(expr.callee.get())) {
        callee_name = id->name;
    }

    // Lower arguments
    std::vector<ValuePtr> args;
    for (const auto& arg : expr.arguments) {
        args.push_back(lower_expression(*arg));
    }

    // For now, assume i32 return type (resolver already validated types)
    auto ret_type = make_i32();
    return builder_.emit_call(callee_name, std::move(args), ret_type);
}

ValuePtr IRLowering::lower_member_access_expr(const ast::MemberAccessExpr& expr) {
    auto obj = lower_expression(*expr.object);
    // Simplified member access: GetField at index 0
    // Full implementation would look up field index from struct layout
    auto field_type = make_i32(); // placeholder
    return builder_.emit_get_field(obj, 0, field_type);
}

ValuePtr IRLowering::lower_index_expr(const ast::IndexExpr& expr) {
    auto base = lower_expression(*expr.array);
    auto index = lower_expression(*expr.index);
    auto elem_ptr = builder_.emit_get_element_ptr(base, index);
    return builder_.emit_load(elem_ptr);
}

ValuePtr IRLowering::lower_cast_expr(const ast::CastExpr& expr) {
    auto value = lower_expression(*expr.expr);
    auto target = lower_type(expr.target_type);

    if (value->type->is_integer() && target->is_integer()) {
        return builder_.emit_int_cast(value, target);
    }
    if (value->type->is_float() && target->is_float()) {
        return builder_.emit_float_cast(value, target);
    }
    if (value->type->is_integer() && target->is_float()) {
        return builder_.emit_int_to_float(value, target);
    }
    if (value->type->is_float() && target->is_integer()) {
        return builder_.emit_float_to_int(value, target);
    }

    return builder_.emit_bitcast(value, target);
}

ValuePtr IRLowering::lower_struct_literal_expr(const ast::StructLiteralExpr& expr) {
    std::vector<ValuePtr> field_values;
    for (const auto& field : expr.fields) {
        field_values.push_back(lower_expression(*field.value));
    }

    auto struct_type = lower_type(expr.struct_name);
    return builder_.emit_struct_init(expr.struct_name, std::move(field_values), struct_type);
}

ValuePtr IRLowering::lower_tuple_expr(const ast::TupleExpr& expr) {
    // Build a tuple type
    auto tuple_type = std::make_shared<IRType>();
    tuple_type->kind = IRTypeKind::Tuple;
    tuple_type->name = "(";

    std::vector<ValuePtr> elems;
    for (size_t i = 0; i < expr.elements.size(); ++i) {
        auto val = lower_expression(*expr.elements[i]);
        elems.push_back(val);
        tuple_type->field_types.push_back(val->type);
        if (i > 0)
            tuple_type->name += ", ";
        tuple_type->name += val->type->name;
    }
    tuple_type->name += ")";

    // Build tuple by inserting values
    auto result = builder_.create_value(tuple_type);
    for (size_t i = 0; i < elems.size(); ++i) {
        result = builder_.emit_insert_value(result, elems[i], static_cast<uint32_t>(i));
    }
    return result;
}

ValuePtr IRLowering::lower_array_expr(const ast::ArrayExpr& expr) {
    if (expr.elements.empty()) {
        return builder_.create_value(make_ptr(make_i32()), "empty_array");
    }

    std::vector<ValuePtr> elems;
    for (const auto& e : expr.elements) {
        elems.push_back(lower_expression(*e));
    }

    auto elem_type = elems[0]->type;
    auto arr_type = make_array(elem_type, elems.size());

    auto alloca = builder_.emit_alloca(arr_type, "array");
    for (size_t i = 0; i < elems.size(); ++i) {
        auto idx = make_const_i32(static_cast<int32_t>(i));
        auto ptr = builder_.emit_get_element_ptr(alloca, idx);
        builder_.emit_store(elems[i], ptr);
    }
    return alloca;
}

ValuePtr IRLowering::lower_lambda_expr(const ast::LambdaExpr& expr) {
    // Lambdas are lowered to anonymous functions + closure environment
    // For now, create a function and return a function pointer value
    std::string lambda_name = "__lambda_" + std::to_string(label_counter_++);

    std::vector<ValuePtr> params;
    for (const auto& p : expr.params) {
        auto pv = std::make_shared<Value>();
        pv->type = lower_type(p.type);
        pv->name = "%" + p.name;
        params.push_back(pv);
    }

    auto ret_type = lower_type(expr.return_type);

    // Save current state
    auto* saved_fn = builder_.current_function();
    auto* saved_bb = builder_.current_block();
    auto saved_scopes = var_scopes_;

    // Create lambda function
    auto* lambda_fn = builder_.create_function(lambda_name, std::move(params), ret_type);
    (void)lambda_fn;

    enter_scope();
    // Lambda body is a single expression
    auto body_val = lower_expression(*expr.body);
    builder_.emit_ret(body_val);
    exit_scope();

    // Restore state
    var_scopes_ = saved_scopes;
    // Note: we can't easily restore insert point to previous function's block
    // because create_function changes current_function_. This is a known limitation;
    // the returned value acts as a function reference.

    // Return a function pointer value
    auto fn_ptr_type = std::make_shared<IRType>();
    fn_ptr_type->kind = IRTypeKind::Function;
    fn_ptr_type->name = lambda_name;
    fn_ptr_type->return_type = ret_type;
    return builder_.create_value(fn_ptr_type, lambda_name);
}

} // namespace flux::ir
