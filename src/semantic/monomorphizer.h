#ifndef FLUX_SEMANTIC_MONOMORPHIZER_H
#define FLUX_SEMANTIC_MONOMORPHIZER_H

#include "ast/ast.h"
#include "semantic/resolver.h"
#include "semantic/type.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace flux::semantic {

class Monomorphizer {
  public:
    explicit Monomorphizer(const ::flux::semantic::Resolver& resolver);
    ::flux::ast::Module monomorphize(const ::flux::ast::Module& module);

  private:
    const ::flux::semantic::Resolver& resolver_;

    // Mangle name for specialization (e.g. foo<Int32> -> foo__Int32)
    std::string mangle_name(const std::string& name,
                            const std::vector<::flux::semantic::FluxType>& type_args);
    std::string mangle_type(const ::flux::semantic::FluxType& type);

    // Instantiate a function
    ::flux::ast::FunctionDecl
    instantiate_function(const std::string& original_name,
                         const std::vector<::flux::semantic::FluxType>& type_args);

    // Substitutions
    void substitute_in_function(
        ::flux::ast::FunctionDecl& fn,
        const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping);
    void
    substitute_in_block(::flux::ast::Block& block,
                        const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping);
    void
    substitute_in_stmt(::flux::ast::Stmt& stmt,
                       const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping);
    void
    substitute_in_expr(::flux::ast::Expr& expr,
                       const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping);
    void substitute_in_pattern(
        ::flux::ast::Pattern& pattern,
        const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping);

    // Helpers
    std::string substitute_type_name(
        const std::string& name,
        const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping);
    ::flux::semantic::FluxType
    substitute_type(const ::flux::semantic::FluxType& type,
                    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping);

    // Cache of instantiated functions (mangled names)
    std::unordered_set<std::string> instantiated_functions_;
};

} // namespace flux::semantic

#endif // FLUX_SEMANTIC_MONOMORPHIZER_H
