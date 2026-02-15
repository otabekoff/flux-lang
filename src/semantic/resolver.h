#ifndef FLUX_RESOLVER_H
#define FLUX_RESOLVER_H

#include "ast/ast.h"
#include "scope.h"
#include "type.h"

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace flux::semantic {

struct FunctionInstantiation {
    std::string name;
    std::vector<::flux::semantic::FluxType> args;

    bool operator==(const FunctionInstantiation& other) const {
        if (name != other.name || args.size() != other.args.size())
            return false;
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] != other.args[i])
                return false;
        }
        return true;
    }
};

struct TypeInstantiation {
    std::string name;
    std::vector<::flux::semantic::FluxType> args;

    bool operator==(const TypeInstantiation& other) const {
        if (name != other.name || args.size() != other.args.size())
            return false;
        for (size_t i = 0; i < args.size(); ++i) {
            if (args[i] != other.args[i])
                return false;
        }
        return true;
    }
};

struct TypeParamBound {
    std::string param_name;
    std::vector<std::string> bounds;
};

struct Resolver {
  public:
    Resolver() = default;
    void resolve(const std::vector<ast::Module*>& modules);
    void resolve(const ast::Module& module);
    void initialize_intrinsics();

  public:
    // Scope
    void enter_scope();
    void exit_scope();

    // Structure
    void declare_module(const ast::Module& module);
    void resolve_module_bodies(const ast::Module& module);
    void resolve_module(const ast::Module& module);
    void resolve_function(const ast::FunctionDecl& fn, const std::string& name = "");
    bool resolve_block(const ast::Block& block);
    bool resolve_statement(const ast::Stmt& stmt);
    void resolve_pattern(const ast::Pattern& pattern, const FluxType& subject_type);
    void resolve_expression(const ast::Expr& expr);
    bool is_pattern_exhaustive(const FluxType& type,
                               const std::vector<const ast::Pattern*>& patterns) const;

    // Initialization state management
    std::unordered_map<Symbol*, bool> save_initialization_state();
    void restore_initialization_state(const std::unordered_map<Symbol*, bool>& state);
    void intersect_initialization_state(const std::unordered_map<Symbol*, bool>& other_state);

    // Monomorphization accessors
    const std::vector<FunctionInstantiation>& function_instantiations() const {
        return function_instantiations_;
    }
    const std::vector<TypeInstantiation>& type_instantiations() const {
        return type_instantiations_;
    }
    const std::unordered_map<std::string, const ast::FunctionDecl*>& function_decls() const {
        return function_decls_;
    }
    const std::unordered_map<std::string, std::vector<std::string>>& function_type_params() const {
        return function_type_params_;
    }

    bool is_enum_variant(const std::string& name) const;
    std::string find_enum_for_variant(const std::string& variant_name) const;

    // expressions
    ::flux::semantic::FluxType type_of(const ast::Expr& expr);
    ::flux::semantic::FluxType type_from_name(const std::string& name);
    std::string resolve_name(const std::string& name, const std::string& module_name = "") const;

  public:
    struct TraitMethodSig {
        std::string name;
        std::string self_type;
        std::vector<std::string> param_types;
        std::string return_type;
        bool has_default = false;
        ast::Visibility visibility = ast::Visibility::None;
        std::string module_name;
    };

    static std::vector<TypeParamBound>
    parse_type_param_bounds(const std::vector<std::string>& type_params);
    static std::vector<TypeParamBound> parse_where_clause(const std::string& where_clause);
    bool type_implements_trait(const std::string& type_name, const std::string& trait_name) const;

    bool
    compare_signatures(const TraitMethodSig& trait_sig, const ast::FunctionDecl& impl_fn,
                       const std::string& target_type,
                       const std::unordered_map<std::string, std::string>& generic_mapping) const;

    // Numeric helpers
    bool is_signed_int_name(const std::string& name) const;
    bool is_unsigned_int_name(const std::string& name) const;
    bool is_float_name(const std::string& name) const;
    int numeric_width(const std::string& name) const;
    std::string promote_integer_name(const std::string& a, const std::string& b) const;
    bool are_types_compatible(const ::flux::semantic::FluxType& target,
                              const ::flux::semantic::FluxType& source) const;

  public:
    static ::flux::semantic::FluxType unknown_type() {
        return {TypeKind::Unknown, "Unknown"};
    }

    ::flux::semantic::FluxType type_from_name_internal(const std::string& name,
                                                       std::unordered_set<std::string>& seen);
    void record_function_instantiation(const std::string& name,
                                       const std::vector<::flux::semantic::FluxType>& args);
    void record_type_instantiation(const std::string& name,
                                   const std::vector<::flux::semantic::FluxType>& args);
    std::vector<std::string> get_bounds_for_type(const std::string& type_name);

  public:
    std::vector<std::unique_ptr<Scope>> all_scopes_;
    Scope* current_scope_ = nullptr;
    const Scope& current_scope() const {
        return *current_scope_;
    }

    ::flux::semantic::FluxType current_function_return_type_{};
    std::string current_function_name_;
    std::string current_type_name_;
    std::string current_module_name_;

    bool in_loop_ = false;
    bool break_found_ = false;
    bool is_in_async_context_ = false;

    // For diagnostics
    struct SourceLoc {
        uint32_t line = 0;
        uint32_t column = 0;
    } last_loc_;

    std::unordered_map<std::string, std::vector<std::string>> enum_variants_;

    struct FieldInfo {
        std::string name;
        std::string type;
        ast::Visibility visibility;
    };

    std::unordered_map<std::string, std::vector<FieldInfo>> struct_fields_;
    std::unordered_map<std::string, std::vector<FieldInfo>> class_fields_;
    std::unordered_map<std::string, std::string> type_aliases_;
    // module_name -> (alias -> full_path)
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> module_aliases_;
    std::unordered_map<std::string, std::vector<TraitMethodSig>> trait_methods_;
    std::unordered_map<std::string, std::unordered_set<std::string>> trait_impls_;
    std::unordered_map<std::string, std::vector<std::string>> function_type_params_;
    std::unordered_map<std::string, std::vector<std::string>> type_type_params_;
    std::unordered_map<std::string, std::vector<std::string>> trait_type_params_;
    std::unordered_map<std::string, std::vector<std::string>> trait_associated_types_;
    std::map<std::pair<std::string, std::string>, std::unordered_map<std::string, std::string>>
        impl_associated_types_;

    std::vector<FunctionInstantiation> function_instantiations_;
    std::vector<TypeInstantiation> type_instantiations_;
    std::unordered_map<std::string, const ast::FunctionDecl*> function_decls_;
    std::unordered_map<std::string, ::flux::semantic::FluxType> substitution_map_;

    static bool is_copy_type(const std::string& type_name);
    std::string stringify_type(const ::flux::semantic::FluxType& type) const;
    void monomorphize_recursive();
};
} // namespace flux::semantic

#endif // FLUX_RESOLVER_H
