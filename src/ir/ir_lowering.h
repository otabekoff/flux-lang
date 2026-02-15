#ifndef FLUX_IR_LOWERING_H
#define FLUX_IR_LOWERING_H

#include "ast/ast.h"
#include "ir/ir.h"
#include "ir/ir_builder.h"
#include "semantic/type.h"

#include <string>
#include <unordered_map>

namespace flux::ir {

/// Translates a monomorphized Flux AST into Flux IR.
class IRLowering {
  public:
    IRLowering();

    /// Lower an entire module (main entry point).
    IRModule lower(const ast::Module& module);

  private:
    // ── Module / function / block ───────────────────────────
    void lower_function(const ast::FunctionDecl& fn);
    void lower_block(const ast::Block& block);
    void lower_statement(const ast::Stmt& stmt);
    ValuePtr lower_expression(const ast::Expr& expr);

    // ── Statement lowering ──────────────────────────────────
    void lower_let_stmt(const ast::LetStmt& stmt);
    void lower_assign_stmt(const ast::AssignStmt& stmt);
    void lower_return_stmt(const ast::ReturnStmt& stmt);
    void lower_if_stmt(const ast::IfStmt& stmt);
    void lower_while_stmt(const ast::WhileStmt& stmt);
    void lower_for_stmt(const ast::ForStmt& stmt);
    void lower_loop_stmt(const ast::LoopStmt& stmt);
    void lower_match_stmt(const ast::MatchStmt& stmt);
    void lower_break_stmt();
    void lower_continue_stmt();
    void lower_expr_stmt(const ast::ExprStmt& stmt);
    void lower_block_stmt(const ast::BlockStmt& stmt);

    // ── Expression lowering ─────────────────────────────────
    ValuePtr lower_number_expr(const ast::NumberExpr& expr);
    ValuePtr lower_string_expr(const ast::StringExpr& expr);
    ValuePtr lower_bool_expr(const ast::BoolExpr& expr);
    ValuePtr lower_char_expr(const ast::CharExpr& expr);
    ValuePtr lower_identifier_expr(const ast::IdentifierExpr& expr);
    ValuePtr lower_binary_expr(const ast::BinaryExpr& expr);
    ValuePtr lower_unary_expr(const ast::UnaryExpr& expr);
    ValuePtr lower_call_expr(const ast::CallExpr& expr);
    ValuePtr lower_member_access_expr(const ast::MemberAccessExpr& expr);
    ValuePtr lower_index_expr(const ast::IndexExpr& expr);
    ValuePtr lower_cast_expr(const ast::CastExpr& expr);
    ValuePtr lower_struct_literal_expr(const ast::StructLiteralExpr& expr);
    ValuePtr lower_tuple_expr(const ast::TupleExpr& expr);
    ValuePtr lower_array_expr(const ast::ArrayExpr& expr);
    ValuePtr lower_lambda_expr(const ast::LambdaExpr& expr);

    // ── Type conversion ─────────────────────────────────────
    std::shared_ptr<IRType> lower_type(const std::string& type_name);
    std::shared_ptr<IRType> lower_flux_type(const semantic::FluxType& type);

    // ── Variable management (local allocas) ─────────────────
    ValuePtr lookup_variable(const std::string& name);
    void declare_variable(const std::string& name, ValuePtr alloca_ptr);

    // ── Loop context  ───────────────────────────────────────
    struct LoopContext {
        BasicBlock* continue_target = nullptr;
        BasicBlock* break_target = nullptr;
    };
    void push_loop(BasicBlock* continue_bb, BasicBlock* break_bb);
    void pop_loop();

    IRBuilder builder_;

    // Variable name → alloca pointer (stack of scopes)
    std::vector<std::unordered_map<std::string, ValuePtr>> var_scopes_;
    void enter_scope();
    void exit_scope();

    // Loop stack
    std::vector<LoopContext> loop_stack_;

    // Counter for generating unique block labels
    uint32_t label_counter_ = 0;
    std::string unique_label(const std::string& prefix);
};

} // namespace flux::ir

#endif // FLUX_IR_LOWERING_H
