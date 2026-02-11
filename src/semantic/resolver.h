#ifndef FLUX_RESOLVER_H
#define FLUX_RESOLVER_H

#include "ast/ast.h"
#include "scope.h"
#include "type.h"

#include <string>
#include <unordered_map>
#include <unordered_set>
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

  public:
    // Trait method signature for registry
    struct TraitMethodSig {
        std::string name;
        std::vector<std::string> param_types; // excluding self
        std::string return_type;
    };

    // Parsed type parameter bound: "T: Display + Clone" → {param_name="T",
    // bounds={"Display","Clone"}}
    struct TypeParamBound {
        std::string param_name;
        std::vector<std::string> bounds;
    };

    // Parse type_params strings into structured bounds
    static std::vector<TypeParamBound>
    parse_type_param_bounds(const std::vector<std::string>& type_params);

    // Check if a type implements a trait
    bool type_implements_trait(const std::string& type_name, const std::string& trait_name) const;

  private:
    static Type unknown() {
        return {TypeKind::Unknown, "Unknown"};
    }

    // patterns
    void resolve_pattern(const ast::Pattern& pattern);

    // internal recursive version with cycle detection
    Type type_from_name_internal(const std::string& name,
                                 std::unordered_set<std::string>& seen) const;

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

    // Trait registry: trait name -> list of required method signatures
    std::unordered_map<std::string, std::vector<TraitMethodSig>> trait_methods_;

    // Impl registry: type name -> set of trait names it implements
    std::unordered_map<std::string, std::unordered_set<std::string>> trait_impls_;

    // Function type params: function name -> type_params (for bound enforcement at call sites)
    std::unordered_map<std::string, std::vector<std::string>> function_type_params_;
};
} // namespace flux::semantic

#endif // FLUX_RESOLVER_H
