#ifndef FLUX_PARSER_H
#define FLUX_PARSER_H

#include "ast/ast.h"
#include "lexer/token.h"
#include <vector>

namespace flux {
class Parser {
  public:
    explicit Parser(std::vector<Token> tokens);
    ast::ExprPtr parse_expression(int min_prec = 0);
    ast::Module parse_module();

  private:
    /* =======================
       Core token navigation
       ======================= */
    const Token& peek(std::size_t offset = 0) const;

    const Token& advance();

    bool is_at_end() const;

    bool match(TokenKind kind);

    bool match_keyword(const std::string& keyword);

    const Token& expect(TokenKind kind, const char* message);

    /* =======================
       Expressions (Pratt)
       ======================= */

    ast::ExprPtr parse_primary();

    /* =======================
       Grammar constructs
       ======================= */
    ast::Import parse_import();
    std::string parse_module_path();

    ast::FunctionDecl parse_function(ast::Visibility visibility = ast::Visibility::None,
                                     bool is_async = false, bool is_external = false);

    ast::StructDecl parse_struct_declaration(ast::Visibility visibility = ast::Visibility::None);
    ast::ClassDecl parse_class_declaration(ast::Visibility visibility = ast::Visibility::None);
    ast::EnumDecl parse_enum_declaration(ast::Visibility visibility = ast::Visibility::None);
    ast::ImplBlock parse_impl_block();
    ast::TraitDecl parse_trait_declaration(ast::Visibility visibility = ast::Visibility::None);
    ast::TypeAlias parse_type_alias(ast::Visibility visibility = ast::Visibility::None);

    std::vector<std::string> parse_type_params();
    std::string parse_type();
    std::string parse_where_clause();
    ast::Block parse_block();

    ast::StmtPtr parse_statement();

    ast::StmtPtr parse_let_statement();
    ast::StmtPtr parse_if_statement();
    ast::StmtPtr parse_while_statement();
    ast::StmtPtr parse_for_statement();
    ast::StmtPtr parse_loop_statement();
    ast::StmtPtr parse_match_statement();

    ast::PatternPtr parse_pattern();
    ast::PatternPtr parse_pattern_atom();
    ast::PatternPtr parse_range_pattern();

    /* =======================
       Visibility helpers
       ======================= */
    bool check_visibility();
    ast::Visibility parse_visibility();

  private:
    std::vector<Token> tokens_;
    std::size_t current_ = 0;
};
} // namespace flux

#endif // FLUX_PARSER_H
