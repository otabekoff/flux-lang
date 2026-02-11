#ifndef FLUX_RESOLVER_H
#define FLUX_RESOLVER_H

#include "ast/ast.h"
#include "scope.h"
#include "type.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace flux::semantic {
class Resolver {
  public:
    void resolve(const ast::Module& module);

  public:
    // Scope
    void enter_scope();

    void exit_scope();

    // Structure
    void resolve_module(const ast::Module& module);

    void resolve_function(const ast::FunctionDecl& fn);

    bool resolve_block(const ast::Block& block);

    bool resolve_statement(const ast::Stmt& stmt);

    bool is_enum_variant(const std::string& name) const;

    std::string find_enum_for_variant(const std::string& variant_name) const;

    // expressions
    void resolve_expression(const ast::Expr& expr);

    Type type_of(const ast::Expr& expr);

    Type type_from_name(const std::string& name) const;

  public:
    // Numeric helpers (public for testing and reuse)
    bool is_signed_int_name(const std::string& name) const;
    bool is_unsigned_int_name(const std::string& name) const;
    bool is_float_name(const std::string& name) const;
    int numeric_width(const std::string& name) const;
    std::string promote_integer_name(const std::string& a, const std::string& b) const;

    bool are_types_compatible(const Type& target, const Type& source) const;

  private:
    static Type unknown() {
        return {TypeKind::Unknown, "Unknown"};
    }

    // patterns
    void resolve_pattern(const ast::Pattern& pattern);

  private:
    Scope* current_scope_ = nullptr;

    // Needed for return analysis
    Type current_function_return_type_{};

    // Loop context for break/continue
    bool in_loop_ = false;
    bool break_found_ = false;

    // Match exhaustiveness: enum name -> list of variant names
    std::unordered_map<std::string, std::vector<std::string>> enum_variants_;

    // Struct/class fields: struct name -> [(field name, field type)]
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>>
        struct_fields_;

    // Type aliases: alias name -> underlying type name
    std::unordered_map<std::string, std::string> type_aliases_;
};
} // namespace flux::semantic

#endif // FLUX_RESOLVER_H
