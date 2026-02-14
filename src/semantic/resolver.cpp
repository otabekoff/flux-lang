#include "resolver.h"

#include <iostream>
#include <ranges>

#include "ast/ast.h"
#include "lexer/diagnostic.h"
#include "type.h"

#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace flux::semantic {

// Helper to check if a type name references a generic type parameter
static bool has_generic_param(const std::string& type_name) {
    // Single uppercase letter is a generic param (T, U, V, etc.)
    if (type_name.size() == 1 && std::isupper(type_name[0]))
        return true;
    // Reference to generic: &T, &mut T
    if (type_name.find("&") != std::string::npos) {
        // Check the non-reference part
        auto pos = type_name.find_last_of("& ");
        if (pos != std::string::npos) {
            std::string inner = type_name.substr(pos + 1);
            if (inner.size() == 1 && std::isupper(inner[0]))
                return true;
        }
    }
    // Generic with args: Box<T>
    if (type_name.find("<") != std::string::npos) {
        // This is simplified but covers most cases
        return type_name.find("<T>") != std::string::npos ||
               type_name.find(", T>") != std::string::npos ||
               type_name.find("<U>") != std::string::npos ||
               type_name.find(", U>") != std::string::npos;
    }
    return false;
}

struct ScopeGuard {
    Resolver* res;
    ScopeGuard(Resolver* r) : res(r) {
        res->enter_scope();
    }
    ~ScopeGuard() {
        res->exit_scope();
    }
};

FluxType Resolver::type_from_name(const std::string& name) {
    std::unordered_set<std::string> seen;
    return type_from_name_internal(name, seen);
}

FluxType Resolver::type_from_name_internal(const std::string& name,
                                           std::unordered_set<std::string>& seen) {
    if (name.empty()) {
        return FluxType(TypeKind::Unknown, "");
    }
    // Check substitution map for generic type parameters
    auto sub_it = substitution_map_.find(name);
    if (sub_it != substitution_map_.end()) {
        return sub_it->second;
    }

    // Option<T> and Result<T,E> types
    if (name.starts_with("Option<")) {
        size_t start = name.find('<');
        size_t end = name.rfind('>');
        if (start != std::string::npos && end != std::string::npos && end > start + 1) {
            std::string inner = name.substr(start + 1, end - start - 1);
            FluxType inner_type = type_from_name_internal(inner, seen);
            FluxType t(TypeKind::Option, name);
            t.generic_args.push_back(inner_type);
            return t;
        }
    }
    if (name.starts_with("Result<")) {
        size_t start = name.find('<');
        size_t end = name.rfind('>');
        if (start != std::string::npos && end != std::string::npos && end > start + 1) {
            std::string inner = name.substr(start + 1, end - start - 1);
            size_t comma = inner.find(',');
            if (comma != std::string::npos) {
                std::string t1 = inner.substr(0, comma);
                std::string t2 = inner.substr(comma + 1);
                // Trim whitespace
                t1.erase(0, t1.find_first_not_of(" \t\n"));
                t1.erase(t1.find_last_not_of(" \t\n") + 1);
                t2.erase(0, t2.find_first_not_of(" \t\n"));
                t2.erase(t2.find_last_not_of(" \t\n") + 1);
                FluxType type1 = type_from_name_internal(t1, seen);
                FluxType type2 = type_from_name_internal(t2, seen);
                FluxType t(TypeKind::Result, name);
                t.generic_args.push_back(type1);
                t.generic_args.push_back(type2);
                return t;
            }
        }
    }

    if (name.starts_with("Int") || name.starts_with("UInt") || name == "IntPtr" ||
        name == "UIntPtr") {
        return FluxType(TypeKind::Int, name);
    }

    if (name.starts_with("Float")) {
        return FluxType(TypeKind::Float, name);
    }

    if (name == "Bool") {
        return FluxType(TypeKind::Bool, name);
    }

    if (name == "String") {
        return FluxType(TypeKind::String, name);
    }

    if (name == "Char") {
        return FluxType(TypeKind::Char, name);
    }

    if (name == "Void") {
        return FluxType(TypeKind::Void, name);
    }

    if (name == "Never") {
        return FluxType(TypeKind::Never, name);
    }

    if (name == "Self" && !current_type_name_.empty()) {
        return type_from_name_internal(current_type_name_, seen);
    }

    // Enum types
    if (enum_variants_.contains(name)) {
        return {TypeKind::Enum, name};
    }

    // Struct/class types
    if (struct_fields_.contains(name)) {
        return {TypeKind::Struct, name};
    }

    // FluxType aliases
    if (type_aliases_.contains(name)) {
        if (seen.contains(name)) {
            throw DiagnosticError("circular type alias detected: '" + name + "'", 0, 0);
        }
        seen.insert(name);
        FluxType resolved = type_from_name_internal(type_aliases_.at(name), seen);
        seen.erase(name);
        return resolved;
    }

    // Associated types (e.g., T::Item)
    if (name.find("::") != std::string::npos) {
        size_t pos = name.rfind("::");
        std::string base_name = name.substr(0, pos);
        std::string assoc_name = name.substr(pos + 2);

        FluxType base_type = type_from_name_internal(base_name, seen);
        if (base_type.kind != TypeKind::Unknown) {
            // 1. Look for concrete impl mapping
            auto it_range = trait_impls_.find(base_type.name);
            if (it_range != trait_impls_.end()) {
                for (const auto& trait : it_range->second) {
                    auto it = impl_associated_types_.find(std::make_pair(base_type.name, trait));
                    if (it != impl_associated_types_.end()) {
                        auto assoc_it = it->second.find(assoc_name);
                        if (assoc_it != it->second.end()) {
                            return type_from_name_internal(assoc_it->second, seen);
                        }
                    }
                }
            }

            // 2. Look in generic bounds if base_type is a generic param
            std::vector<std::string> bounds;
            if (!current_function_name_.empty() &&
                function_type_params_.contains(current_function_name_)) {
                for (const auto& p : function_type_params_.at(current_function_name_)) {
                    if (p.starts_with(base_type.name + ":")) {
                        // Extract traits from "T: Trait1 + Trait2"
                        size_t colon = p.find(':');
                        std::string traits_list = p.substr(colon + 1);
                        // Simple split by '+' - could be more robust
                        size_t start = 0;
                        size_t plus = traits_list.find('+');
                        while (plus != std::string::npos) {
                            std::string t = traits_list.substr(start, plus - start);
                            t.erase(0, t.find_first_not_of(" \t"));
                            t.erase(t.find_last_not_of(" \t") + 1);
                            bounds.push_back(t);
                            start = plus + 1;
                            plus = traits_list.find('+', start);
                        }
                        std::string t = traits_list.substr(start);
                        t.erase(0, t.find_first_not_of(" \t"));
                        t.erase(t.find_last_not_of(" \t") + 1);
                        bounds.push_back(t);
                    }
                }
            }

            for (const auto& trait_bound : bounds) {
                // Remove generic args from trait name for lookup: Trait<Arg> -> Trait
                std::string trait_base = trait_bound;
                if (auto b_pos = trait_base.find('<'); b_pos != std::string::npos) {
                    trait_base = trait_base.substr(0, b_pos);
                }

                auto it = trait_associated_types_.find(trait_base);
                if (it != trait_associated_types_.end()) {
                    for (const auto& candidate : it->second) {
                        if (candidate == assoc_name) {
                            // In a generic context, we might keep it as T::Item
                            // or resolve to a placeholder. For now, return a generic type
                            // representing the associated type itself.
                            return FluxType(TypeKind::Generic, name);
                        }
                    }
                }
            }
        }
    }

    // Generic type parameters from scope
    if (current_scope_) {
        if (auto sym = current_scope_->lookup(name)) {
            if (sym->kind == SymbolKind::Variable && sym->type == "FluxType") {
                return FluxType(TypeKind::Generic, name);
            }
        }
    }

    // Generic types (e.g., Box<Int32>)
    if (name.find('<') != std::string::npos) {
        size_t open = name.find('<');
        size_t close = name.rfind('>');
        if (open != std::string::npos && close != std::string::npos && close > open + 1) {
            std::string base = name.substr(0, open);
            if (enum_variants_.contains(base) || type_type_params_.contains(base) ||
                trait_type_params_.contains(base) || function_type_params_.contains(base) ||
                type_aliases_.contains(base) || struct_fields_.contains(base)) {
                std::string args_str = name.substr(open + 1, close - open - 1);
                std::vector<FluxType> args;

                // Split args by comma, respecting nested brackets
                int depth = 0;
                size_t start = 0;
                for (size_t i = 0; i <= args_str.size(); ++i) {
                    bool is_end = (i == args_str.size());
                    if (!is_end) {
                        if (args_str[i] == '<' || args_str[i] == '(' || args_str[i] == '[')
                            depth++;
                        else if (args_str[i] == '>' || args_str[i] == ')' || args_str[i] == ']')
                            depth--;
                    }

                    if (is_end || (args_str[i] == ',' && depth == 0)) {
                        std::string arg = args_str.substr(start, i - start);
                        // trim
                        arg.erase(0, arg.find_first_not_of(" \t\n"));
                        arg.erase(arg.find_last_not_of(" \t\n") + 1);
                        if (!arg.empty()) {
                            args.push_back(type_from_name_internal(arg, seen));
                        }
                        start = i + 1;
                    }
                }

                FluxType t(TypeKind::Struct, name);
                t.generic_args = std::move(args);

                // Record type instantiation
                if (!t.generic_args.empty()) {
                    record_type_instantiation(base, t.generic_args);
                }

                return t;
            }
        }
    }

    // Reference types
    if (name.starts_with("&")) {
        return {TypeKind::Ref, name};
    }

    // Array types: [T; N] or Slice types: [T]
    if (name.starts_with("[")) {
        size_t semicolon = name.rfind(';');
        size_t end = name.rfind(']');
        if (end != std::string::npos) {
            if (semicolon != std::string::npos && end > semicolon) {
                // Array type: [T; N]
                std::string inner = name.substr(1, semicolon - 1);
                std::string size_str = name.substr(semicolon + 1, end - semicolon - 1);
                // Trim inner
                inner.erase(0, inner.find_first_not_of(" \t\n"));
                inner.erase(inner.find_last_not_of(" \t\n") + 1);

                FluxType value_type = type_from_name_internal(inner, seen);
                if (value_type.kind != TypeKind::Unknown) {
                    std::string canonical_name = "[" + value_type.name + ";" + size_str + "]";
                    return {TypeKind::Array, canonical_name};
                }
            } else if (semicolon == std::string::npos) {
                // Slice type: [T]
                std::string inner = name.substr(1, end - 1);
                // Trim inner
                inner.erase(0, inner.find_first_not_of(" \t\n"));
                inner.erase(inner.find_last_not_of(" \t\n") + 1);

                FluxType value_type = type_from_name_internal(inner, seen);
                if (value_type.kind != TypeKind::Unknown) {
                    std::string canonical_name = "[" + value_type.name + "]";
                    return {TypeKind::Slice, canonical_name};
                }
            }
        }
    }

    if (name.starts_with("(")) {
        // Check if it's a function type: contains "->" after the argument list
        // We need to find the matching ')' for the opening '('
        int depth = 0;
        size_t args_end = std::string::npos;
        for (size_t i = 0; i < name.size(); ++i) {
            if (name[i] == '(')
                depth++;
            else if (name[i] == ')') {
                depth--;
                if (depth == 0) {
                    args_end = i;
                    break;
                }
            }
        }

        if (args_end != std::string::npos) {
            // Check for "->" after the args
            size_t arrow_pos = name.find("->", args_end);
            if (arrow_pos != std::string::npos) {
                // It IS a function type: (Args) -> Ret
                std::string args_content = name.substr(1, args_end - 1);
                std::vector<FluxType> params;
                if (!args_content.empty()) {
                    int d = 0;
                    size_t start = 0;
                    for (size_t i = 0; i <= args_content.size(); ++i) {
                        bool is_end = (i == args_content.size());
                        if (!is_end) {
                            if (args_content[i] == '(' || args_content[i] == '<')
                                d++;
                            else if (args_content[i] == ')' || args_content[i] == '>')
                                d--;
                        }

                        if (is_end || (args_content[i] == ',' && d == 0)) {
                            std::string param_str = args_content.substr(start, i - start);
                            // Trim
                            size_t first = param_str.find_first_not_of(" \t\n");
                            if (first != std::string::npos) {
                                size_t last = param_str.find_last_not_of(" \t\n");
                                param_str = param_str.substr(first, last - first + 1);
                            } else {
                                param_str = "";
                            }
                            if (!param_str.empty()) {
                                params.push_back(type_from_name_internal(param_str, seen));
                            }
                            start = i + 1;
                        }
                    }
                }

                std::string ret_str = name.substr(arrow_pos + 2);
                // Trim
                size_t first = ret_str.find_first_not_of(" \t\n");
                if (first != std::string::npos) {
                    size_t last = ret_str.find_last_not_of(" \t\n");
                    ret_str = ret_str.substr(first, last - first + 1);
                }
                FluxType ret_type = type_from_name_internal(ret_str, seen);

                return FluxType(TypeKind::Function, name, false, std::move(params),
                                std::make_unique<FluxType>(std::move(ret_type)));
            } else {
                // Pure tuple type: (T1, T2, ...)
                std::string content = name.substr(1, args_end - 1);
                std::vector<FluxType> elements;
                if (!content.empty()) {
                    int d = 0;
                    size_t s = 0;
                    for (size_t i = 0; i <= content.size(); ++i) {
                        bool is_end = (i == content.size());
                        if (!is_end) {
                            if (content[i] == '(' || content[i] == '<')
                                d++;
                            else if (content[i] == ')' || content[i] == '>')
                                d--;
                        }
                        if (is_end || (content[i] == ',' && d == 0)) {
                            std::string element_str = content.substr(s, i - s);
                            // trim
                            size_t f = element_str.find_first_not_of(" \t\n");
                            if (f != std::string::npos) {
                                size_t l = element_str.find_last_not_of(" \t\n");
                                element_str = element_str.substr(f, l - f + 1);
                                elements.push_back(type_from_name_internal(element_str, seen));
                            }
                            s = i + 1;
                        }
                    }
                }
                FluxType res(TypeKind::Tuple, name);
                res.generic_args = std::move(elements);
                return res;
            }
        }
    }

    return unknown();
}

bool Resolver::is_enum_variant(const std::string& name) const {
    for (const auto& variants : enum_variants_ | std::views::values) {
        for (const auto& v : variants) {
            if (v == name) {
                return true;
            }
        }
    }
    return false;
}

std::string Resolver::find_enum_for_variant(const std::string& variant_name) const {
    for (const auto& [enum_name, variants] : enum_variants_) {
        for (const auto& v : variants) {
            if (v == variant_name) {
                return enum_name;
            }
        }
    }
    return "";
}

FluxType Resolver::type_of(const ast::Expr& expr) {
    using semantic::FluxType;
    using semantic::TypeKind;

    if (auto arr = dynamic_cast<const ast::ArrayExpr*>(&expr)) {
        if (arr->elements.empty()) {
            throw DiagnosticError("empty array literal is not allowed", 0, 0);
        }
        FluxType first_type = type_of(*arr->elements[0]);
        bool any_never = first_type.kind == TypeKind::Never;

        for (size_t i = 1; i < arr->elements.size(); ++i) {
            FluxType t = type_of(*arr->elements[i]);
            if (t.kind == TypeKind::Never) {
                any_never = true;
                continue;
            }
            if (first_type.kind == TypeKind::Never) {
                first_type = t;
            } else if (t != first_type && t.kind != TypeKind::Unknown) {
                throw DiagnosticError("array elements must have the same type", 0, 0);
            }
        }
        if (any_never)
            return never_type();
        std::string name = "[" + first_type.name + ";" + std::to_string(arr->elements.size()) + "]";
        return {TypeKind::Array, name};
    }

    if (auto slice = dynamic_cast<const ast::SliceExpr*>(&expr)) {
        FluxType arr_type = type_of(*slice->array);
        if (arr_type.kind != TypeKind::Array && arr_type.kind != TypeKind::Slice) {
            throw DiagnosticError("slice base must be an array or slice", 0, 0);
        }

        // If it's an array [T; N] or slice [T], the result is a slice [T]
        std::string elem_type_name;
        if (arr_type.kind == TypeKind::Array) {
            // [T; N] -> find T
            auto lbrack = arr_type.name.find('[');
            auto semi = arr_type.name.find(';');
            if (lbrack != std::string::npos && semi != std::string::npos && semi > lbrack + 1) {
                elem_type_name = arr_type.name.substr(lbrack + 1, semi - lbrack - 1);
            } else {
                elem_type_name = "Unknown";
            }
        } else {
            // [T] -> find T
            auto lbrack = arr_type.name.find('[');
            auto rbrack = arr_type.name.find(']');
            if (lbrack != std::string::npos && rbrack != std::string::npos && rbrack > lbrack + 1) {
                elem_type_name = arr_type.name.substr(lbrack + 1, rbrack - lbrack - 1);
            } else {
                elem_type_name = "Unknown";
            }
        }

        return {TypeKind::Slice, "[" + elem_type_name + "]"};
    }

    if (auto idx = dynamic_cast<const ast::IndexExpr*>(&expr)) {
        FluxType arr_type = type_of(*idx->array);
        if (arr_type.kind != TypeKind::Array && arr_type.kind != TypeKind::Slice) {
            throw DiagnosticError("index base must be an array or slice", 0, 0);
        }

        // Index type must be integer
        FluxType index_type = type_of(*idx->index);
        if (index_type.kind != TypeKind::Int && index_type.kind != TypeKind::Unknown) {
            throw DiagnosticError("index must be an integer", 0, 0);
        }

        // Extract element type
        std::string elem_type_name;
        if (arr_type.kind == TypeKind::Array) {
            auto lbrack = arr_type.name.find('[');
            auto semi = arr_type.name.find(';');
            if (lbrack != std::string::npos && semi != std::string::npos && semi > lbrack + 1) {
                elem_type_name = arr_type.name.substr(lbrack + 1, semi - lbrack - 1);
            } else {
                elem_type_name = "Unknown";
            }
        } else {
            auto lbrack = arr_type.name.find('[');
            auto rbrack = arr_type.name.find(']');
            if (lbrack != std::string::npos && rbrack != std::string::npos && rbrack > lbrack + 1) {
                elem_type_name = arr_type.name.substr(lbrack + 1, rbrack - lbrack - 1);
            } else {
                elem_type_name = "Unknown";
            }
        }

        return type_from_name(elem_type_name);
    }

    if (auto num = dynamic_cast<const ast::NumberExpr*>(&expr)) {
        const std::string& s = num->value;
        const bool is_float = (s.find('.') != std::string::npos) ||
                              (s.find('e') != std::string::npos) ||
                              (s.find('E') != std::string::npos);
        if (is_float)
            return {TypeKind::Float, "Float64"};
        return {TypeKind::Int, "Int32"};
    }

    if (dynamic_cast<const ast::BoolExpr*>(&expr)) {
        return {TypeKind::Bool, "Bool"};
    }

    if (dynamic_cast<const ast::StringExpr*>(&expr)) {
        return {TypeKind::String, "String"};
    }

    if (auto tuple = dynamic_cast<const ast::TupleExpr*>(&expr)) {
        std::string name = "(";
        bool first = true;
        bool any_never = false;
        std::vector<FluxType> elems;
        for (const auto& elem : tuple->elements) {
            FluxType t = type_of(*elem);
            elems.push_back(t);
            if (t.kind == TypeKind::Never)
                any_never = true;
            if (!first)
                name += ", ";
            name += t.name;
            first = false;
        }
        if (any_never)
            return never_type();
        name += ")";
        return FluxType(TypeKind::Tuple, name, false, {}, nullptr, std::move(elems));
    }

    if (auto lambda = dynamic_cast<const ast::LambdaExpr*>(&expr)) {
        std::vector<FluxType> param_types;
        std::string name = "(";
        bool first = true;
        for (const auto& param : lambda->params) {
            FluxType t = type_from_name(param.type);
            param_types.push_back(t);
            if (!first)
                name += ", ";
            name += t.name;
            first = false;
        }
        name += ")";

        FluxType return_type = type_from_name(lambda->return_type);
        name += " -> " + return_type.name;

        FluxType fn_type(TypeKind::Function, name, false, param_types,
                         std::make_unique<FluxType>(return_type));
        return fn_type;
    }

    if (dynamic_cast<const ast::CharExpr*>(&expr)) {
        return {TypeKind::Char, "Char"};
    }

    if (auto id = dynamic_cast<const ast::IdentifierExpr*>(&expr)) {
        // Check if it's an enum variant used standalone
        if (is_enum_variant(id->name)) {
            std::string enum_name = find_enum_for_variant(id->name);
            return {TypeKind::Enum, enum_name};
        }

        if (id->name == "None") {
            FluxType t(TypeKind::Option, "Option<Unknown>");
            t.generic_args.push_back(unknown());
            return t;
        }

        std::string lookup_name = id->name;
        if (auto pos = lookup_name.find('<'); pos != std::string::npos) {
            lookup_name = lookup_name.substr(0, pos);
        }

        const Symbol* sym = current_scope_->lookup(lookup_name);
        if (!sym) {
            throw DiagnosticError("use of undeclared identifier '" + id->name + "'", 0, 0);
        }

        if (sym->kind == SymbolKind::Function) {
            std::vector<FluxType> params;
            std::string name = "(";
            bool first = true;
            for (const auto& p : sym->param_types) {
                FluxType t = type_from_name(p);
                params.push_back(t);
                if (!first)
                    name += ", ";
                name += t.name;
                first = false;
            }
            name += ")";

            FluxType ret = type_from_name(sym->type);
            name += " -> " + ret.name;

            return FluxType(TypeKind::Function, name, false, std::move(params),
                            std::make_unique<FluxType>(std::move(ret)));
        }

        return type_from_name(sym->type);
    }

    if (auto bin = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        // Handle enum variant access: Color::Red
        if (bin->op == TokenKind::ColonColon) {
            auto* lhs_id = dynamic_cast<const ast::IdentifierExpr*>(bin->left.get());
            auto* rhs_id = dynamic_cast<const ast::IdentifierExpr*>(bin->right.get());
            if (lhs_id && rhs_id) {
                // Check if it's an enum type with that variant
                if (enum_variants_.contains(lhs_id->name)) {
                    const auto& variants = enum_variants_.at(lhs_id->name);
                    bool found = false;
                    for (const auto& v : variants) {
                        if (v == rhs_id->name) {
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        return {TypeKind::Enum, lhs_id->name};
                    }
                    throw DiagnosticError(
                        "no variant '" + rhs_id->name + "' in enum '" + lhs_id->name + "'", 0, 0);
                }
            }
            // For module paths (e.g., io::println), just resolve left
            if (lhs_id) {
                const Symbol* sym = current_scope_->lookup(lhs_id->name);
                if (sym) {
                    return type_from_name(sym->type);
                }
            }
            return unknown();
        }

        // Handle dot access (field access / method call receiver)
        if (bin->op == TokenKind::Dot) {
            // Try to resolve the type of the left-hand side
            FluxType lhs = type_of(*bin->left);
            if (lhs.kind == TypeKind::Never)
                return never_type();

            // Determine the field name (right side must be identifier)
            if (auto rhs_id = dynamic_cast<const ast::IdentifierExpr*>(bin->right.get())) {
                const std::string field_name = rhs_id->name;

                // Helper to lookup a field on a struct by base name
                auto lookup_field =
                    [&](const std::string& struct_name) -> std::optional<std::string> {
                    std::string base = struct_name;
                    auto pos = base.find('<');
                    if (pos != std::string::npos)
                        base = base.substr(0, pos);
                    if (!struct_fields_.contains(base))
                        return std::nullopt;
                    for (const auto& p : struct_fields_.at(base)) {
                        if (p.name == field_name) {
                            // Enforce visibility
                            if (p.visibility == ast::Visibility::Private ||
                                p.visibility == ast::Visibility::None) {
                                // Private fields are only accessible within the type's own methods
                                if (current_type_name_ != base) {
                                    throw DiagnosticError("field '" + field_name + "' is private",
                                                          0, 0);
                                }
                            }
                            return p.type;
                        }
                    }
                    return std::nullopt;
                };

                // If lhs is a struct type, try to find the field
                if (lhs.kind == TypeKind::Struct) {
                    if (auto ftype = lookup_field(lhs.name)) {
                        return type_from_name(*ftype);
                    }
                }

                // 3. Try method lookup in scope (qualified name: FluxType::Method)
                std::string base_type_name = lhs.name;
                // Strip references (&Person -> Person)
                if (base_type_name.starts_with("&mut ")) {
                    base_type_name = base_type_name.substr(5);
                } else if (base_type_name.starts_with("&")) {
                    base_type_name = base_type_name.substr(1);
                }
                // Strip generic arguments (Person<T> -> Person)
                if (auto pos = base_type_name.find('<'); pos != std::string::npos) {
                    base_type_name = base_type_name.substr(0, pos);
                }

                // Strip generic arguments from field name for method lookup
                std::string method_lookup_name = field_name;
                if (auto pos = method_lookup_name.find('<'); pos != std::string::npos) {
                    method_lookup_name = method_lookup_name.substr(0, pos);
                }

                std::string qualified_name = base_type_name + "::" + method_lookup_name;
                if (const Symbol* sym = current_scope_->lookup(qualified_name)) {
                    if (sym->kind == SymbolKind::Function) {
                        // Enforce method visibility
                        if (sym->visibility == ast::Visibility::Private) {
                            if (current_type_name_ != base_type_name) {
                                throw DiagnosticError(
                                    "method '" + method_lookup_name + "' is private", 0, 0);
                            }
                        }
                        std::vector<FluxType> params;
                        // Skip the first parameter (implicit 'self')
                        for (size_t i = 1; i < sym->param_types.size(); ++i) {
                            params.push_back(type_from_name(sym->param_types[i]));
                        }
                        FluxType ret_type = type_from_name(sym->type);

                        // Construct a descriptive signature for the bound method
                        std::string fn_signature = "(";
                        for (size_t i = 0; i < params.size(); ++i) {
                            if (i > 0)
                                fn_signature += ", ";
                            fn_signature += params[i].name;
                        }
                        fn_signature += ") -> " + ret_type.name;

                        return FluxType(TypeKind::Function, fn_signature, false, std::move(params),
                                        std::make_unique<FluxType>(std::move(ret_type)));
                    }
                }

                // 4. Try trait method lookup for generic parameters
                // If lhs is Unknown, try to get the declared type name from the symbol
                std::string type_for_bounds = base_type_name;
                if (lhs.kind == TypeKind::Unknown || lhs.kind == TypeKind::Generic) {
                    if (auto lhs_id = dynamic_cast<const ast::IdentifierExpr*>(bin->left.get())) {
                        if (const Symbol* var_sym = current_scope_->lookup(lhs_id->name)) {
                            std::string declared = var_sym->type;
                            // Strip references
                            if (declared.starts_with("&mut "))
                                declared = declared.substr(5);
                            else if (declared.starts_with("&"))
                                declared = declared.substr(1);
                            if (auto pos = declared.find('<'); pos != std::string::npos)
                                declared = declared.substr(0, pos);
                            if (!declared.empty())
                                type_for_bounds = declared;
                        }
                    }
                }
                std::vector<std::string> trait_bounds = get_bounds_for_type(type_for_bounds);
                for (const auto& trait_bound : trait_bounds) {
                    std::string tb_name = trait_bound;
                    if (auto pos = tb_name.find('<'); pos != std::string::npos) {
                        tb_name = tb_name.substr(0, pos);
                    }
                    // First try scope lookup (for impl-registered methods)
                    std::string trait_qualified = tb_name + "::" + field_name;
                    if (const Symbol* sym = current_scope_->lookup(trait_qualified)) {
                        if (sym->kind == SymbolKind::Function) {
                            std::vector<FluxType> params;
                            for (size_t i = 1; i < sym->param_types.size(); ++i) {
                                params.push_back(type_from_name(sym->param_types[i]));
                            }
                            FluxType ret_type = type_from_name(sym->type);
                            std::string fn_signature = "(";
                            for (size_t i = 0; i < params.size(); ++i) {
                                if (i > 0)
                                    fn_signature += ", ";
                                fn_signature += params[i].name;
                            }
                            fn_signature += ") -> " + ret_type.name;

                            return FluxType(TypeKind::Function, fn_signature, false,
                                            std::move(params),
                                            std::make_unique<FluxType>(std::move(ret_type)));
                        }
                    }
                    // Fallback: search trait_methods_ directly (for trait declarations
                    // whose methods haven't been registered as symbols in scope)
                    auto tm_it = trait_methods_.find(tb_name);
                    if (tm_it != trait_methods_.end()) {
                        for (const auto& sig : tm_it->second) {
                            if (sig.name == field_name) {
                                std::vector<FluxType> params;
                                for (const auto& pt : sig.param_types) {
                                    params.push_back(type_from_name(pt));
                                }
                                FluxType ret_type = type_from_name(sig.return_type);
                                std::string fn_signature = "(";
                                for (size_t i = 0; i < params.size(); ++i) {
                                    if (i > 0)
                                        fn_signature += ", ";
                                    fn_signature += params[i].name;
                                }
                                fn_signature += ") -> " + ret_type.name;

                                return FluxType(TypeKind::Function, fn_signature, false,
                                                std::move(params),
                                                std::make_unique<FluxType>(std::move(ret_type)));
                            }
                        }
                    }
                }

                throw DiagnosticError(
                    "type '" + lhs.name + "' has no field or method '" + field_name + "'", 0, 0);
            }

            throw DiagnosticError("right side of '.' must be an identifier", 0, 0);
        }

        FluxType lhs = type_of(*bin->left);
        FluxType rhs = type_of(*bin->right);

        if (lhs.kind == TypeKind::Never || rhs.kind == TypeKind::Never) {
            return never_type();
        }

        // Range operator
        if (bin->op == TokenKind::DotDot) {
            return {TypeKind::Unknown, "Range"};
        }

        // Arithmetic
        if (bin->op == TokenKind::Plus || bin->op == TokenKind::Minus ||
            bin->op == TokenKind::Star || bin->op == TokenKind::Slash ||
            bin->op == TokenKind::Percent) {
            if (lhs.kind != rhs.kind && lhs.kind != TypeKind::Unknown &&
                rhs.kind != TypeKind::Unknown) {
                throw DiagnosticError("type mismatch in binary expression", 0, 0);
            }

            if (lhs.kind != TypeKind::Int && lhs.kind != TypeKind::Float &&
                lhs.kind != TypeKind::Unknown) {
                throw DiagnosticError("invalid operands for arithmetic operator", 0, 0);
            }

            return lhs;
        }

        // Bitwise operators
        if (bin->op == TokenKind::Amp || bin->op == TokenKind::Pipe ||
            bin->op == TokenKind::Caret || bin->op == TokenKind::ShiftLeft ||
            bin->op == TokenKind::ShiftRight) {
            if (lhs.kind != TypeKind::Int && lhs.kind != TypeKind::Unknown) {
                throw DiagnosticError("invalid operands for bitwise operator", 0, 0);
            }
            return lhs;
        }

        // Comparisons
        if (bin->op == TokenKind::EqualEqual || bin->op == TokenKind::BangEqual ||
            bin->op == TokenKind::Less || bin->op == TokenKind::LessEqual ||
            bin->op == TokenKind::Greater || bin->op == TokenKind::GreaterEqual) {
            if (lhs != rhs && lhs.kind != TypeKind::Unknown && rhs.kind != TypeKind::Unknown) {
                throw DiagnosticError("comparison between incompatible types", 0, 0);
            }

            return {TypeKind::Bool, "Bool"};
        }

        // Logical operators
        if (bin->op == TokenKind::AmpAmp || bin->op == TokenKind::PipePipe) {
            if ((lhs.kind != TypeKind::Bool && lhs.kind != TypeKind::Unknown) ||
                (rhs.kind != TypeKind::Bool && rhs.kind != TypeKind::Unknown)) {
                throw DiagnosticError("logical operators require Bool operands", 0, 0);
            }
            return {TypeKind::Bool, "Bool"};
        }
    }

    if (auto un = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        FluxType operand = type_of(*un->operand);
        if (operand.kind == TypeKind::Never)
            return never_type();

        if (un->op == TokenKind::Minus) {
            if (operand.kind != TypeKind::Int && operand.kind != TypeKind::Float) {
                throw DiagnosticError("invalid operand for unary '-'", 0, 0);
            }
            return operand;
        }

        if (un->op == TokenKind::Bang) {
            if (operand.kind != TypeKind::Bool) {
                throw DiagnosticError("invalid operand for '!'", 0, 0);
            }
            return {TypeKind::Bool, "Bool"};
        }

        if (un->op == TokenKind::Amp) {
            // Handle mutable and immutable references
            if (un->is_mutable) {
                return {TypeKind::Ref, "&mut " + operand.name, true};
            } else {
                return {TypeKind::Ref, "&" + operand.name, false};
            }
        }

        if (un->op == TokenKind::Tilde) {
            if (operand.kind != TypeKind::Int) {
                throw DiagnosticError("invalid operand for '~'", 0, 0);
            }
            return operand;
        }
    }

    if (auto cast = dynamic_cast<const ast::CastExpr*>(&expr)) {
        (void)type_of(*cast->expr);
        FluxType target = type_from_name(cast->target_type);
        if (target.kind == TypeKind::Unknown) {
            return target;
        }
    } // This closing brace was missing in the original code, but the diff implies it should be
      // here.

    if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
        FluxType callee_type = unknown();

        // 1. Resolve callee type
        if (call->callee) {
            callee_type = type_of(*call->callee);
            if (callee_type.kind == TypeKind::Never)
                return never_type();
        }

        // Record method call instantiations for dot-access calls.
        // type_of for dot-access returns the method's return type, not TypeKind::Function,
        // so the recording block inside the Function check below is never reached.
        if (auto bin = dynamic_cast<const ast::BinaryExpr*>(call->callee.get())) {
            if (bin->op == TokenKind::Dot || bin->op == TokenKind::ColonColon) {

                try {
                    FluxType lhs_type = type_of(*bin->left);
                    std::string lhs_name = lhs_type.name;
                    if (lhs_name.starts_with("&mut "))
                        lhs_name = lhs_name.substr(5);
                    else if (lhs_name.starts_with("&"))
                        lhs_name = lhs_name.substr(1);

                    std::string lhs_base = lhs_name;
                    if (auto pos = lhs_base.find('<'); pos != std::string::npos)
                        lhs_base = lhs_base.substr(0, pos);

                    if (auto rhs_id = dynamic_cast<const ast::IdentifierExpr*>(bin->right.get())) {
                        std::string callee_name = rhs_id->name;
                        std::string method_name = callee_name;
                        if (auto pos = method_name.find('<'); pos != std::string::npos)
                            method_name = method_name.substr(0, pos);
                        std::string qualified = lhs_base + "::" + method_name;

                        auto tp_it = function_type_params_.find(qualified);
                        if (tp_it != function_type_params_.end()) {
                            auto bounds = parse_type_param_bounds(tp_it->second);
                            std::unordered_map<std::string, std::string> param_mapping;

                            // 1. Map struct type params from LHS receiver type
                            if (!lhs_type.generic_args.empty()) {
                                auto it = type_type_params_.find(lhs_base);
                                if (it != type_type_params_.end()) {
                                    std::vector<std::string> raw;
                                    for (const auto& p : it->second) {
                                        if (p.find(':') == std::string::npos)
                                            raw.push_back(p);
                                    }
                                    for (size_t i = 0;
                                         i < raw.size() && i < lhs_type.generic_args.size(); ++i) {
                                        param_mapping[raw[i]] = lhs_type.generic_args[i].name;
                                    }
                                }
                            } else if (!substitution_map_.empty()) {
                                // During monomorphization, 'self' has bare type (e.g. Wrapper)
                                // without generic args. Use the current substitution_map_ to
                                // fill in concrete types for struct type params.
                                auto it = type_type_params_.find(lhs_base);
                                if (it != type_type_params_.end()) {
                                    for (const auto& p : it->second) {
                                        if (p.find(':') == std::string::npos &&
                                            substitution_map_.contains(p)) {
                                            param_mapping[p] = substitution_map_[p].name;
                                        }
                                    }
                                }
                            }

                            // 2. Map method's own type params from explicit generic args
                            // Parse generic args from callee_name manually since type_from_name
                            // cannot resolve function names that aren't registered type names.
                            std::vector<FluxType> explicit_args;
                            if (auto angle = callee_name.find('<'); angle != std::string::npos) {
                                std::string args_str = callee_name.substr(angle + 1);
                                if (!args_str.empty() && args_str.back() == '>')
                                    args_str.pop_back();
                                // Split by comma respecting nested angle brackets
                                int depth = 0;
                                std::string current;
                                for (char c : args_str) {
                                    if (c == '<')
                                        depth++;
                                    else if (c == '>')
                                        depth--;
                                    else if (c == ',' && depth == 0) {
                                        if (!current.empty())
                                            explicit_args.push_back(type_from_name(current));
                                        current.clear();
                                        continue;
                                    }
                                    current += c;
                                }
                                if (!current.empty())
                                    explicit_args.push_back(type_from_name(current));
                            }
                            if (!explicit_args.empty()) {
                                std::vector<std::string> all_raw;
                                for (const auto& p : tp_it->second) {
                                    if (p.find(':') == std::string::npos)
                                        all_raw.push_back(p);
                                }
                                // Offset past struct params
                                size_t struct_count = 0;
                                auto struct_it = type_type_params_.find(lhs_base);
                                if (struct_it != type_type_params_.end()) {
                                    for (const auto& p : struct_it->second) {
                                        if (p.find(':') == std::string::npos)
                                            struct_count++;
                                    }
                                }
                                for (size_t i = 0; i < explicit_args.size() &&
                                                   (i + struct_count) < all_raw.size();
                                     ++i) {
                                    param_mapping[all_raw[i + struct_count]] =
                                        explicit_args[i].name;
                                }
                            }

                            // 3. Record the instantiation
                            std::vector<FluxType> concrete_args;
                            for (const auto& p : tp_it->second) {
                                std::string param_name = p;
                                auto colon = param_name.find(':');
                                if (colon != std::string::npos)
                                    param_name = param_name.substr(0, colon);

                                // Simple trim
                                size_t first = param_name.find_first_not_of(" \t\n");
                                if (first != std::string::npos) {
                                    size_t last = param_name.find_last_not_of(" \t\n");
                                    param_name = param_name.substr(first, last - first + 1);
                                }

                                if (param_mapping.contains(param_name)) {
                                    concrete_args.push_back(
                                        type_from_name(param_mapping[param_name]));
                                }
                            }
                            if (!concrete_args.empty()) {
                                record_function_instantiation(qualified, concrete_args);
                            }
                        }
                    }
                } catch (...) {
                }
            }
        }

        // Special handling for Option/Result constructors
        if (auto callee_id = dynamic_cast<const ast::IdentifierExpr*>(call->callee.get())) {
            if (callee_id->name == "Some" && call->arguments.size() == 1) {
                FluxType val_type = type_of(*call->arguments[0]);
                if (val_type.kind == TypeKind::Never)
                    return never_type();
                FluxType t(TypeKind::Option, "Option<" + val_type.name + ">");
                t.generic_args.push_back(val_type);
                return t;
            }
            if (callee_id->name == "Ok" && call->arguments.size() == 1) {
                FluxType val_type = type_of(*call->arguments[0]);
                if (val_type.kind == TypeKind::Never)
                    return never_type();
                FluxType t(TypeKind::Result, "Result<" + val_type.name + ", Unknown>");
                t.generic_args.push_back(val_type);
                t.generic_args.push_back(unknown());
                return t;
            }
            if (callee_id->name == "Err" && call->arguments.size() == 1) {
                FluxType err_type = type_of(*call->arguments[0]);
                if (err_type.kind == TypeKind::Never)
                    return never_type();
                FluxType t(TypeKind::Result, "Result<Unknown, " + err_type.name + ">");
                t.generic_args.push_back(unknown());
                t.generic_args.push_back(err_type);
                return t;
            }
        }

        // 2. Check if it's a function type
        if (callee_type.kind == TypeKind::Function) {
            if (call->arguments.size() != callee_type.param_types.size()) {
                throw DiagnosticError("expected " + std::to_string(callee_type.param_types.size()) +
                                          " arguments, got " +
                                          std::to_string(call->arguments.size()),
                                      0, 0);
            }

            bool any_never = false;
            for (size_t i = 0; i < call->arguments.size(); ++i) {
                FluxType arg_type = type_of(*call->arguments[i]);
                if (arg_type.kind == TypeKind::Never)
                    any_never = true;
                FluxType param_type = callee_type.param_types[i];

                if (arg_type != param_type && param_type.kind != TypeKind::Unknown &&
                    arg_type.kind != TypeKind::Unknown && arg_type.kind != TypeKind::Never) {
                    throw DiagnosticError("argument " + std::to_string(i + 1) + " has type '" +
                                              arg_type.name + "', expected '" + param_type.name +
                                              "'",
                                          0, 0);
                }
            }

            // Trait bound enforcement: check type param bounds against concrete arg types
            std::string base;
            std::string callee_full_name;
            if (auto callee_id = dynamic_cast<const ast::IdentifierExpr*>(call->callee.get())) {
                base = callee_id->name;
                callee_full_name = callee_id->name;
            } else if (auto bin = dynamic_cast<const ast::BinaryExpr*>(call->callee.get())) {
                if (bin->op == TokenKind::Dot || bin->op == TokenKind::ColonColon) {
                    FluxType lhs_type = type_of(*bin->left);
                    std::string lhs_name = lhs_type.name;
                    // Strip pointers and references
                    if (lhs_name.starts_with("&mut "))
                        lhs_name = lhs_name.substr(5);
                    else if (lhs_name.starts_with("&"))
                        lhs_name = lhs_name.substr(1);

                    // Strip generics from the LHS type for qualified lookup
                    if (auto pos = lhs_name.find('<'); pos != std::string::npos) {
                        lhs_name = lhs_name.substr(0, pos);
                    }

                    if (auto rhs_id = dynamic_cast<const ast::IdentifierExpr*>(bin->right.get())) {
                        callee_full_name = rhs_id->name;
                        base = lhs_name + "::" + rhs_id->name;
                    }
                }
            }

            if (!base.empty()) {
                if (auto pos = base.find('<'); pos != std::string::npos) {
                    base = base.substr(0, pos);
                }

                auto tp_it = function_type_params_.find(base);
                if (tp_it != function_type_params_.end()) {
                    auto bounds = parse_type_param_bounds(tp_it->second);
                    auto sym = current_scope_->lookup(base);
                    if (sym && sym->kind == SymbolKind::Function) {
                        std::unordered_map<std::string, std::string> param_mapping;

                        // Explicit generic args from callee name (e.g. foo<Int32>)
                        FluxType explicit_type = type_from_name(callee_full_name);
                        if (!explicit_type.generic_args.empty()) {
                            std::vector<std::string> raw_params;
                            for (const auto& p : tp_it->second) {
                                if (p.find(':') == std::string::npos) {
                                    raw_params.push_back(p);
                                }
                            }

                            size_t offset = 0;

                            // For methods, the first N params are from the struct/impl
                            if (base.find("::") != std::string::npos) {
                                std::string type_name = base.substr(0, base.rfind("::"));
                                if (auto pos = type_name.find('<'); pos != std::string::npos)
                                    type_name = type_name.substr(0, pos);

                                auto struct_it = type_type_params_.find(type_name);
                                if (struct_it != type_type_params_.end()) {
                                    for (const auto& p : struct_it->second) {
                                        if (p.find(':') == std::string::npos)
                                            offset++;
                                    }
                                }
                            }

                            for (size_t i = 0; i < explicit_type.generic_args.size() &&
                                               (i + offset) < raw_params.size();
                                 ++i) {
                                param_mapping[raw_params[i + offset]] =
                                    explicit_type.generic_args[i].name;
                            }
                        }

                        // Inference from lhs (for methods like p.foo())
                        if (auto bin = dynamic_cast<const ast::BinaryExpr*>(call->callee.get())) {
                            if (bin->op == TokenKind::Dot || bin->op == TokenKind::ColonColon) {
                                FluxType lhs_type = type_of(*bin->left);
                                if (!lhs_type.generic_args.empty()) {
                                    // Extract base type name to get its type params
                                    std::string lhs_base = lhs_type.name;
                                    if (auto pos = lhs_base.find('<'); pos != std::string::npos)
                                        lhs_base = lhs_base.substr(0, pos);

                                    auto it = type_type_params_.find(lhs_base);
                                    if (it != type_type_params_.end()) {
                                        std::vector<std::string> raw_params;
                                        for (const auto& p : it->second) {
                                            if (p.find(':') == std::string::npos)
                                                raw_params.push_back(p);
                                        }
                                        for (size_t i = 0; i < raw_params.size() &&
                                                           i < lhs_type.generic_args.size();
                                             ++i) {
                                            param_mapping[raw_params[i]] =
                                                lhs_type.generic_args[i].name;
                                        }
                                    }
                                }
                            }
                        }

                        // Inference from arguments
                        // Note: for methods, sym->param_types[0] is 'self', but call->arguments[0]
                        // is the first REAL arg.
                        size_t sym_offset = 0;
                        if (auto bin = dynamic_cast<const ast::BinaryExpr*>(call->callee.get())) {
                            if (bin->op == TokenKind::Dot || bin->op == TokenKind::ColonColon) {
                                sym_offset = 1;
                            }
                        }

                        for (size_t i = 0; i < call->arguments.size() &&
                                           (i + sym_offset) < sym->param_types.size();
                             ++i) {
                            for (const auto& b : bounds) {
                                if (sym->param_types[i + sym_offset] == b.param_name) {
                                    FluxType arg_type = type_of(*call->arguments[i]);
                                    if (arg_type.kind != TypeKind::Unknown &&
                                        arg_type.kind != TypeKind::Never) {
                                        param_mapping[b.param_name] = arg_type.name;
                                    }
                                }
                            }
                        }
                        for (const auto& b : bounds) {
                            auto map_it = param_mapping.find(b.param_name);
                            if (map_it != param_mapping.end()) {
                                for (const auto& trait_name : b.bounds) {
                                    if (!type_implements_trait(map_it->second, trait_name)) {
                                        throw DiagnosticError("type '" + map_it->second +
                                                                  "' does not implement trait '" +
                                                                  trait_name +
                                                                  "' required by type parameter '" +
                                                                  b.param_name + "'",
                                                              0, 0);
                                    }
                                }
                            }
                        }

                        // Record function instantiation
                        std::vector<FluxType> concrete_args;
                        for (const auto& p : tp_it->second) {
                            std::string param_name = p;
                            auto colon = param_name.find(':');
                            if (colon != std::string::npos)
                                param_name = param_name.substr(0, colon);

                            // Simple trim
                            size_t first = param_name.find_first_not_of(" \t\n");
                            if (first != std::string::npos) {
                                size_t last = param_name.find_last_not_of(" \t\n");
                                param_name = param_name.substr(first, last - first + 1);
                            }

                            if (param_mapping.contains(param_name)) {
                                concrete_args.push_back(type_from_name(param_mapping[param_name]));
                            }
                        }
                        if (!concrete_args.empty()) {
                            record_function_instantiation(base, concrete_args);
                        }
                    }
                }
            }

            if (any_never)
                return never_type();

            if (callee_type.return_type)
                return *callee_type.return_type;
            return void_type();
        }

        // Special handling for panic built-in if callee resolution is simple
        if (auto callee_id = dynamic_cast<const ast::IdentifierExpr*>(call->callee.get())) {
            if (callee_id->name == "panic") {
                return never_type();
            }
        }

        // 3. Built-ins (fallback if callee resolution failed or returned non-Function)
        if (callee_type.kind == TypeKind::Unknown) {
            for (const auto& arg : call->arguments) {
                resolve_expression(*arg);
            }
            return unknown();
        }

        throw DiagnosticError("called object is not a function", 0, 0);
    }

    if (auto mv = dynamic_cast<const ast::MoveExpr*>(&expr)) {
        return type_of(*mv->operand);
    }

    if (auto sl = dynamic_cast<const ast::StructLiteralExpr*>(&expr)) {
        // Get base struct name (strip generic params)
        std::string base = sl->struct_name;
        if (auto pos = base.find('<'); pos != std::string::npos) {
            base = base.substr(0, pos);
        }

        if (!struct_fields_.contains(base) && !enum_variants_.contains(base) &&
            !current_scope_->lookup(base)) {
            return {TypeKind::Struct, sl->struct_name};
        }

        // Check generic bounds if any
        auto tp_it = type_type_params_.find(base);
        if (tp_it != type_type_params_.end()) {
            auto bounds = parse_type_param_bounds(tp_it->second);
            FluxType concrete = type_from_name(sl->struct_name);

            // Map type param names to concrete types
            if (concrete.generic_args.size() > 0) {
                std::vector<std::string> raw_params;
                for (const auto& p : tp_it->second) {
                    if (p.find(':') == std::string::npos) {
                        raw_params.push_back(p);
                    }
                }

                std::unordered_map<std::string, std::string> mapping;
                for (size_t i = 0; i < raw_params.size() && i < concrete.generic_args.size(); ++i) {
                    mapping[raw_params[i]] = concrete.generic_args[i].name;
                }

                // Now check all bounds
                auto all_bounds = parse_type_param_bounds(tp_it->second);
                for (const auto& b : all_bounds) {
                    if (mapping.contains(b.param_name)) {
                        const std::string& arg_type = mapping.at(b.param_name);
                        for (const auto& trait : b.bounds) {
                            if (!type_implements_trait(arg_type, trait)) {
                                if (!has_generic_param(arg_type)) {
                                    throw DiagnosticError(
                                        "type '" + arg_type + "' does not implement trait '" +
                                            trait + "' required by struct '" + base + "'",
                                        0, 0);
                                }
                            }
                        }
                    }
                }

                // Record type instantiation
                if (!concrete.generic_args.empty()) {
                    record_type_instantiation(base, concrete.generic_args);
                }
            }
        }

        return type_from_name(sl->struct_name);
    }

    if (auto ep = dynamic_cast<const ast::ErrorPropagationExpr*>(&expr)) {
        return type_of(*ep->operand);
    }

    if (dynamic_cast<const ast::AwaitExpr*>(&expr)) {
        return unknown();
    }

    if (dynamic_cast<const ast::SpawnExpr*>(&expr)) {
        return unknown();
    }

    if (dynamic_cast<const ast::RangeExpr*>(&expr)) {
        return unknown();
    }

    return unknown();
}

/* =======================
   Scope management
   ======================= */

void Resolver::enter_scope() {
    auto new_scope = std::make_unique<Scope>(current_scope_);
    current_scope_ = new_scope.get();
    all_scopes_.push_back(std::move(new_scope));
}

void Resolver::exit_scope() {
    if (current_scope_) {
        current_scope_ = current_scope_->parent();
    }
}

/* =======================
   Entry point
   ======================= */

void Resolver::resolve(const ast::Module& module) {
    all_scopes_.clear();
    auto global_scope = std::make_unique<Scope>(nullptr);
    current_scope_ = global_scope.get();
    all_scopes_.push_back(std::move(global_scope));

    // Built-ins
    current_scope_->declare({"drop",
                             SymbolKind::Function,
                             false,
                             true,
                             false,
                             ast::Visibility::Public,
                             "",
                             "Void",
                             {"T"}});
    current_scope_->declare({"panic",
                             SymbolKind::Function,
                             false,
                             true,
                             false,
                             ast::Visibility::Public,
                             "",
                             "Never",
                             {"String"}});
    current_scope_->declare({"assert",
                             SymbolKind::Function,
                             false,
                             true,
                             false,
                             ast::Visibility::Public,
                             "",
                             "Void",
                             {"Bool", "String"}});
    current_scope_->declare({"Some",
                             SymbolKind::Function,
                             false,
                             true,
                             false,
                             ast::Visibility::Public,
                             "",
                             "Option<T>",
                             {"T"}});
    current_scope_->declare({"None",
                             SymbolKind::Variable,
                             false,
                             true,
                             false,
                             ast::Visibility::Public,
                             "",
                             "Option<T>",
                             {}});
    current_scope_->declare({"Ok",
                             SymbolKind::Function,
                             false,
                             true,
                             false,
                             ast::Visibility::Public,
                             "",
                             "Result<T,E>",
                             {"T"}});
    current_scope_->declare({"Err",
                             SymbolKind::Function,
                             false,
                             true,
                             false,
                             ast::Visibility::Public,
                             "",
                             "Result<T,E>",
                             {"E"}});

    enter_scope(); // Module scope - persists after resolve()
    resolve_module(module);

    // Monomorphization pass (Phase 2)
    monomorphize_recursive();
}

/* =======================
   Module
   ======================= */

void Resolver::resolve_module(const ast::Module& module) {
    current_module_name_ = module.name;
    // Note: Caller (resolve()) handles entering the initial module scope.
    // If we are resolving nested modules (imports), we might need more logic here.

    // Declare imports
    for (const auto& imp : module.imports) {
        std::string root_path = imp.module_path;
        const auto pos = root_path.find("::");
        if (pos != std::string::npos) {
            root_path = root_path.substr(0, pos);
        }
        current_scope_->declare({root_path,
                                 SymbolKind::Variable,
                                 false,
                                 true,
                                 false,
                                 ast::Visibility::Private,
                                 "",
                                 "Module",
                                 {}});
    }

    // Declare type aliases
    for (const auto& ta : module.type_aliases) {
        type_aliases_[ta.name] = ta.target_type;
        current_scope_->declare(
            {ta.name, SymbolKind::Variable, false, true, false, ta.visibility, "", "FluxType", {}});
    }

    // Validate type aliases (catch circular definitions early)
    for (const auto& ta : module.type_aliases) {
        type_from_name(ta.name);
    }

    // Declare types (structs, enums, classes, traits)
    for (const auto& s : module.structs) {
        current_scope_->declare(
            {s.name, SymbolKind::Variable, false, true, false, s.visibility, "", "FluxType", {}});
        // Store struct fields for type checking
        std::vector<FieldInfo> fields;
        for (const auto& f : s.fields) {
            fields.push_back({f.name, f.type, f.visibility});
        }
        struct_fields_[s.name] = std::move(fields);

        // Store type params for structs
        auto bounds = parse_where_clause(s.where_clause);
        std::vector<std::string> combined = s.type_params;
        for (const auto& b : bounds) {
            std::string str = b.param_name + ": ";
            for (size_t i = 0; i < b.bounds.size(); ++i) {
                if (i > 0)
                    str += " + ";
                str += b.bounds[i];
            }
            combined.push_back(std::move(str));
        }
        type_type_params_[s.name] = std::move(combined);
    }

    for (const auto& c : module.classes) {
        current_scope_->declare(
            {c.name, SymbolKind::Variable, false, true, false, c.visibility, "", "FluxType", {}});
        std::vector<FieldInfo> fields;
        for (const auto& f : c.fields) {
            fields.push_back({f.name, f.type, f.visibility});
        }
        struct_fields_[c.name] = std::move(fields);

        // Store type params for classes
        auto bounds = parse_where_clause(c.where_clause);
        std::vector<std::string> combined = c.type_params;
        for (const auto& b : bounds) {
            std::string str = b.param_name + ": ";
            for (size_t i = 0; i < b.bounds.size(); ++i) {
                if (i > 0)
                    str += " + ";
                str += b.bounds[i];
            }
            combined.push_back(std::move(str));
        }
        type_type_params_[c.name] = std::move(combined);
    }

    for (const auto& e : module.enums) {
        current_scope_->declare(
            {e.name, SymbolKind::Variable, false, true, false, e.visibility, "", "FluxType", {}});
        std::vector<std::string> vars;
        vars.reserve(e.variants.size());
        for (const auto& [name, types] : e.variants) {
            vars.push_back(name);
        }
        enum_variants_[e.name] = std::move(vars);

        // Store type params for enums
        auto bounds = parse_where_clause(e.where_clause);
        std::vector<std::string> combined = e.type_params;
        for (const auto& b : bounds) {
            std::string str = b.param_name + ": ";
            for (size_t i = 0; i < b.bounds.size(); ++i) {
                if (i > 0)
                    str += " + ";
                str += b.bounds[i];
            }
            combined.push_back(std::move(str));
        }
        type_type_params_[e.name] = std::move(combined);
    }

    for (const auto& t : module.traits) {
        current_scope_->declare(
            {t.name, SymbolKind::Variable, false, true, false, t.visibility, "", "Trait", {}});
        auto bounds = parse_where_clause(t.where_clause);
        std::vector<std::string> combined = t.type_params;
        for (const auto& b : bounds) {
            std::string str = b.param_name + ": ";
            for (size_t i = 0; i < b.bounds.size(); ++i) {
                if (i > 0)
                    str += " + ";
                str += b.bounds[i];
            }
            combined.push_back(std::move(str));
        }
        trait_type_params_[t.name] = std::move(combined);

        // Store associated types
        std::vector<std::string> assoc_names;
        for (const auto& assoc : t.associated_types) {
            assoc_names.push_back(assoc.name);
        }
        trait_associated_types_[t.name] = std::move(assoc_names);

        // Register trait method signatures
        std::vector<TraitMethodSig> sigs;
        for (const auto& m : t.methods) {
            TraitMethodSig sig;
            sig.name = m.name;
            sig.return_type = m.return_type;
            for (const auto& p : m.params) {
                if (p.name == "self") {
                    sig.self_type = p.type;
                } else {
                    sig.param_types.push_back(p.type);
                }
            }
            sig.has_default = m.has_body;
            sigs.push_back(std::move(sig));
        }
        trait_methods_[t.name] = std::move(sigs);
    }

    // Declare functions first (forward visibility)
    for (const auto& fn : module.functions) {
        // Parse bounds from where clause and append to type_params
        auto bounds = parse_where_clause(fn.where_clause);
        std::vector<std::string> combined_type_params = fn.type_params;
        for (const auto& b : bounds) {
            std::string s = b.param_name + ": ";
            for (size_t i = 0; i < b.bounds.size(); ++i) {
                if (i > 0)
                    s += " + ";
                s += b.bounds[i];
            }
            combined_type_params.push_back(std::move(s));
        }
        function_type_params_[fn.name] = combined_type_params;

        std::vector<std::string> params;
        params.reserve(fn.params.size());
        for (const auto& p : fn.params)
            params.push_back(p.type);

        Symbol sym;
        sym.name = fn.name;
        sym.kind = SymbolKind::Function;
        sym.type = fn.return_type;
        sym.param_types = std::move(params);
        sym.is_moved = false; // Added

        if (!current_scope_->declare(sym)) {
            throw DiagnosticError("duplicate function '" + fn.name + "'", 0, 0);
        }
        function_decls_[fn.name] = &fn;
    }

    // Declare impl methods (forward visibility) and register trait impls
    for (const auto& impl : module.impls) {
        if (!impl.trait_name.empty()) {
            // Check orphan rule
            bool is_local_type = false;
            if (const Symbol* sym = current_scope_->lookup(impl.target_name)) {
                if (sym->module_name == current_module_name_ || sym->module_name == "")
                    is_local_type = true;
            }
            bool is_local_trait = false;
            // Strip generics for lookup
            std::string trait_base = impl.trait_name;
            if (auto pos = trait_base.find('<'); pos != std::string::npos) {
                trait_base = trait_base.substr(0, pos);
            }
            if (const Symbol* sym = current_scope_->lookup(trait_base)) {
                if (sym->module_name == current_module_name_ || sym->module_name == "")
                    is_local_trait = true;
            }

            if (!is_local_type && !is_local_trait && impl.trait_name != "") {
                throw flux::DiagnosticError(
                    "orphan rule violation: cannot implement foreign trait '" + impl.trait_name +
                        "' for foreign type '" + impl.target_name + "'",
                    0, 0);
            }

            trait_impls_[impl.target_name].insert(impl.trait_name);

            // Store associated type mappings
            std::unordered_map<std::string, std::string> assoc_mapping;
            for (const auto& assoc : impl.associated_types) {
                // For impls, the "default_type" field actually stores the target type
                assoc_mapping[assoc.name] = assoc.default_type;
            }

            // Perform trait conformance check (associated types)
            trait_base = impl.trait_name;
            if (auto pos = trait_base.find('<'); pos != std::string::npos) {
                trait_base = trait_base.substr(0, pos);
            }

            auto trait_assoc_it = trait_associated_types_.find(trait_base);
            if (trait_assoc_it != trait_associated_types_.end()) {
                const auto& required_assocs = trait_assoc_it->second;
                for (const auto& required : required_assocs) {
                    if (!assoc_mapping.contains(required)) {
                        throw flux::DiagnosticError("impl of trait '" + impl.trait_name +
                                                        "' for type '" + impl.target_name +
                                                        "' is missing associated type '" +
                                                        required + "'",
                                                    0, 0);
                    }
                }
            }

            impl_associated_types_[std::make_pair(impl.target_name, impl.trait_name)] =
                std::move(assoc_mapping);

            // 3. Perform trait conformance check (methods)
            std::unordered_map<std::string, std::string> generic_mapping;
            auto it_params = trait_type_params_.find(trait_base);
            if (it_params != trait_type_params_.end()) {
                FluxType trait_type = type_from_name(impl.trait_name);
                if (!trait_type.generic_args.empty()) {
                    std::vector<std::string> raw_params;
                    for (const auto& p : it_params->second) {
                        if (p.find(':') == std::string::npos)
                            raw_params.push_back(p);
                    }
                    for (size_t i = 0; i < raw_params.size() && i < trait_type.generic_args.size();
                         ++i) {
                        generic_mapping[raw_params[i]] = trait_type.generic_args[i].name;
                    }

                    // Check trait bounds (including where clauses)
                    auto bounds = parse_type_param_bounds(it_params->second);
                    auto impl_bounds = parse_type_param_bounds(impl.type_params);
                    std::unordered_set<std::string> impl_generics;
                    for (const auto& b : impl_bounds)
                        impl_generics.insert(b.param_name);

                    for (const auto& b : bounds) {
                        std::string concrete_type;
                        if (b.param_name == "Self") {
                            concrete_type = impl.target_name;
                        } else if (generic_mapping.contains(b.param_name)) {
                            concrete_type = generic_mapping.at(b.param_name);
                        } else {
                            continue;
                        }

                        if (impl_generics.contains(concrete_type))
                            continue;

                        for (const auto& req : b.bounds) {
                            if (!type_implements_trait(concrete_type, req)) {
                                throw DiagnosticError(
                                    "type '" + concrete_type + "' does not implement trait '" +
                                        req + "' required by trait '" + impl.trait_name + "'",
                                    0, 0);
                            }
                        }
                    }
                }
            }

            auto trait_methods_it = trait_methods_.find(trait_base);
            if (trait_methods_it != trait_methods_.end()) {
                const auto& required_methods = trait_methods_it->second;
                for (const auto& required : required_methods) {
                    bool found = false;
                    for (const auto& provided : impl.methods) {
                        if (provided.name == required.name) {
                            if (!compare_signatures(required, provided, impl.target_name,
                                                    generic_mapping)) {
                                throw flux::DiagnosticError(
                                    "method '" + provided.name + "' in impl of '" +
                                        impl.trait_name + "' for '" + impl.target_name +
                                        "' has a signature mismatch with trait",
                                    0, 0);
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found && !required.has_default) {
                        throw flux::DiagnosticError(
                            "impl of trait '" + impl.trait_name + "' for type '" +
                                impl.target_name + "' is missing method '" + required.name + "'",
                            0, 0);
                    }
                }

                // 4. Register inherited trait methods (those with defaults not overridden)
                for (const auto& trait_sig : required_methods) {
                    bool overridden = false;
                    for (const auto& provided : impl.methods) {
                        if (provided.name == trait_sig.name) {
                            overridden = true;
                            break;
                        }
                    }

                    if (!overridden && trait_sig.has_default) {
                        std::string base_target = impl.target_name;
                        if (auto pos = base_target.find('<'); pos != std::string::npos) {
                            base_target = base_target.substr(0, pos);
                        }
                        std::string qualified_name = base_target + "::" + trait_sig.name;

                        // Substitute Self and generics in signature
                        std::string ret_type = trait_sig.return_type;
                        if (ret_type == "Self")
                            ret_type = impl.target_name;
                        for (const auto& [gen, concrete] : generic_mapping) {
                            if (ret_type == gen)
                                ret_type = concrete;
                        }

                        std::vector<std::string> params;
                        std::string self_type = trait_sig.self_type;
                        if (self_type.find("Self") != std::string::npos) {
                            auto pos = self_type.find("Self");
                            self_type.replace(pos, 4, impl.target_name);
                        }
                        params.push_back(self_type);

                        for (auto p : trait_sig.param_types) {
                            if (p == "Self")
                                p = impl.target_name;
                            for (const auto& [gen, concrete] : generic_mapping) {
                                if (p == gen)
                                    p = concrete;
                            }
                            params.push_back(p);
                        }

                        current_scope_->declare({qualified_name, SymbolKind::Function, false, true,
                                                 false, ast::Visibility::Public, "", ret_type,
                                                 std::move(params)});
                    }
                }
            }
        }

        // Declare all methods in the impl block in the scope (both trait and inherent)
        for (const auto& method : impl.methods) {
            std::string base_target = impl.target_name;
            if (auto pos = base_target.find('<'); pos != std::string::npos) {
                base_target = base_target.substr(0, pos);
            }
            std::string qualified_name = base_target + "::" + method.name;

            // Combine impl type params and method type params
            std::vector<std::string> combined_params = impl.type_params;
            combined_params.insert(combined_params.end(), method.type_params.begin(),
                                   method.type_params.end());
            function_type_params_[qualified_name] = combined_params;

            std::vector<std::string> params;
            for (const auto& p : method.params)
                params.push_back(p.type);

            Symbol sym;
            sym.name = qualified_name;
            sym.kind = SymbolKind::Function;
            sym.type = method.return_type;
            sym.param_types = std::move(params);
            sym.visibility = method.visibility;
            sym.is_moved = false; // Added

            current_scope_->declare(sym);
            function_decls_[qualified_name] = &method;
        }
    }

    // Resolve function bodies
    for (const auto& fn : module.functions) {
        resolve_function(fn);
    }

    // Resolve impl method bodies
    for (const auto& impl : module.impls) {
        std::string old_type = current_type_name_;
        current_type_name_ = impl.target_name;

        std::string base_target = impl.target_name;
        if (auto pos = base_target.find('<'); pos != std::string::npos) {
            base_target = base_target.substr(0, pos);
        }

        for (const auto& method : impl.methods) {
            resolve_function(method, base_target + "::" + method.name);
        }

        current_type_name_ = old_type;
    }
}

void Resolver::resolve_function(const ast::FunctionDecl& fn, const std::string& name) {
    std::string fn_name = name.empty() ? fn.name : name;
    std::string old_fn = current_function_name_;
    std::string old_type = current_type_name_;
    current_function_name_ = fn_name;

    // If this is a method (contains ::), set the current type name so 'Self' resolves correctly
    if (fn_name.find("::") != std::string::npos) {
        size_t pos = fn_name.rfind("::");
        current_type_name_ = fn_name.substr(0, pos);
    }

    ScopeGuard guard(this);

    current_function_return_type_ = type_from_name(fn.return_type);
    in_loop_ = false;

    // Declare parameters
    for (const auto& param : fn.params) {
        if (!current_scope_->declare({param.name,
                                      SymbolKind::Variable,
                                      false,
                                      false,
                                      false,
                                      ast::Visibility::None,
                                      "",
                                      param.type,
                                      {}})) {
            throw DiagnosticError("duplicate parameter '" + param.name + "'", 0, 0);
        }
    }

    // Resolve body and collect return info
    bool body_returns = resolve_block(fn.body);

    // Enforce return correctness
    if (current_function_return_type_.kind != TypeKind::Void &&
        current_function_return_type_.kind != TypeKind::Never &&
        current_function_return_type_.kind != TypeKind::Unknown && !body_returns) {
        throw DiagnosticError("missing return in function returning '" +
                                  current_function_return_type_.name + "'",
                              0, 0);
    }

    current_function_name_ = old_fn;
    current_type_name_ = old_type;
}

/* =======================
   Block
   ======================= */

bool Resolver::resolve_block(const ast::Block& block) {
    ScopeGuard guard(this);

    bool always_returns = false;

    for (const auto& stmt : block.statements) {
        if (always_returns) {
            throw DiagnosticError("unreachable code", 0, 0);
        }

        if (resolve_statement(*stmt)) {
            always_returns = true;
        }
    }

    return always_returns;
}

bool Resolver::resolve_statement(const ast::Stmt& stmt) {
    // return
    if (const auto* ret = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        if (ret->expression) {
            FluxType returned = type_of(*ret->expression);
            if (returned != current_function_return_type_ &&
                current_function_return_type_.kind != TypeKind::Unknown &&
                returned.kind != TypeKind::Unknown &&
                !has_generic_param(current_function_return_type_.name) &&
                !has_generic_param(returned.name)) {
                throw DiagnosticError("return type mismatch: expected '" +
                                          current_function_return_type_.name + "', got '" +
                                          returned.name + "'",
                                      0, 0);
            }
        } else if (current_function_return_type_.kind != TypeKind::Void) {
            throw DiagnosticError("returning void from non-void function", 0, 0);
        }
        return true;
    }

    // let / const
    if (const auto* let_stmt = dynamic_cast<const ast::LetStmt*>(&stmt)) {
        // 1. Compute initializer type
        resolve_expression(*let_stmt->initializer);
        FluxType init_type = type_of(*let_stmt->initializer);

        // 2. Declared type (must be explicit)
        FluxType declared_type = type_from_name(let_stmt->type_name);
        if (declared_type.kind == TypeKind::Unknown &&
            !type_aliases_.contains(let_stmt->type_name)) {
            // Allow generic/complex types to pass (e.g. Box<Int32>)
            // Only error on truly unknown simple types
            bool is_complex = let_stmt->type_name.find('<') != std::string::npos ||
                              let_stmt->type_name.find('&') != std::string::npos ||
                              let_stmt->type_name.find('(') != std::string::npos;
            if (!is_complex && !current_scope_->lookup(let_stmt->type_name)) {
                throw DiagnosticError("unknown type '" + let_stmt->type_name + "'", 0, 0);
            }
        }

        // 3. Enforce compatibility
        if (!are_types_compatible(declared_type, init_type)) {
            std::string var_name = let_stmt->name.empty() ? "(tuple)" : let_stmt->name;
            throw DiagnosticError("cannot initialize variable '" + var_name + "' of type '" +
                                      declared_type.name + "' with value of type '" +
                                      init_type.name + "'",
                                  0, 0);
        }

        // 4. Declare symbol(s)
        if (!let_stmt->tuple_names.empty()) {
            if (init_type.kind != TypeKind::Tuple) {
                throw DiagnosticError("expected tuple type for destructuring let, found '" +
                                          init_type.name + "'",
                                      0, 0);
            }
            if (let_stmt->tuple_names.size() != init_type.generic_args.size()) {
                throw DiagnosticError("destructuring pattern arity mismatch: expected " +
                                          std::to_string(init_type.generic_args.size()) +
                                          " variables, found " +
                                          std::to_string(let_stmt->tuple_names.size()),
                                      0, 0);
            }
            for (size_t i = 0; i < let_stmt->tuple_names.size(); ++i) {
                if (!current_scope_->declare({let_stmt->tuple_names[i],
                                              SymbolKind::Variable,
                                              let_stmt->is_mutable,
                                              let_stmt->is_const,
                                              false, // is_moved
                                              ast::Visibility::None,
                                              "",
                                              stringify_type(init_type.generic_args[i]),
                                              {}})) {
                    throw DiagnosticError("duplicate variable '" + let_stmt->tuple_names[i] + "'",
                                          0, 0);
                }
            }
        } else {
            if (!current_scope_->declare({let_stmt->name,
                                          SymbolKind::Variable,
                                          let_stmt->is_mutable,
                                          let_stmt->is_const,
                                          false, // is_moved
                                          ast::Visibility::None,
                                          "",
                                          let_stmt->type_name,
                                          {}})) {
                throw DiagnosticError("duplicate variable '" + let_stmt->name + "'", 0, 0);
            }
        }

        // Implicit move for non-Copy types
        if (auto id_expr = dynamic_cast<const ast::IdentifierExpr*>(let_stmt->initializer.get())) {
            Symbol* source_sym = current_scope_->lookup_mut(id_expr->name);
            if (source_sym && source_sym->kind == SymbolKind::Variable) {
                if (!is_copy_type(source_sym->type)) {
                    source_sym->is_moved = true;
                }
            }
        }

        return false;
    }

    // assignment
    if (const auto* asg = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
        const auto* id = dynamic_cast<const ast::IdentifierExpr*>(asg->target.get());

        if (id) {
            Symbol* sym = current_scope_->lookup_mut(id->name);
            if (!sym) {
                throw DiagnosticError("assignment to undeclared variable '" + id->name + "'", 0, 0);
            }

            if (sym->is_const) {
                throw DiagnosticError("cannot assign to constant '" + id->name + "'", 0, 0);
            }

            if (!sym->is_mutable) {
                throw DiagnosticError("cannot assign to immutable variable '" + id->name + "'", 0,
                                      0);
            }

            const FluxType lhs = type_from_name(sym->type);

            if (asg->op == TokenKind::Assign) {
                resolve_expression(*asg->value);
                FluxType val_type = type_of(*asg->value);
                if (!are_types_compatible(type_from_name(sym->type), val_type)) {
                    throw DiagnosticError("cannot assign type '" + val_type.name +
                                              "' to variable of type '" + sym->type + "'",
                                          0, 0);
                }

                // Revival: assigning to a moved variable makes it valid again
                sym->is_moved = false;

                // Implicit move for non-Copy types (source)
                if (auto val_id = dynamic_cast<const ast::IdentifierExpr*>(asg->value.get())) {
                    Symbol* source_sym = current_scope_->lookup_mut(val_id->name);
                    if (source_sym && source_sym->kind == SymbolKind::Variable) {
                        if (!is_copy_type(source_sym->type)) {
                            source_sym->is_moved = true;
                        }
                    }
                }
            } else {
                // Compound assignment — type-check the RHS
                type_of(*asg->value);
            }
        } else {
            // Complex target (field access, index access, etc.) — just
            // resolve
            resolve_expression(*asg->target);
        }

        resolve_expression(*asg->value);
        return false;
    }

    // block statement
    if (const auto* block = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
        return resolve_block(block->block);
    }

    // if statement
    if (const auto* ifs = dynamic_cast<const ast::IfStmt*>(&stmt)) {
        resolve_expression(*ifs->condition);

        bool then_returns = resolve_statement(*ifs->then_branch);
        bool else_returns = false;

        if (ifs->else_branch) {
            else_returns = resolve_statement(*ifs->else_branch);
        }

        return then_returns && else_returns;
    }

    // while
    if (const auto* wh = dynamic_cast<const ast::WhileStmt*>(&stmt)) {
        resolve_expression(*wh->condition);
        bool prev_in_loop = in_loop_;
        in_loop_ = true;
        resolve_statement(*wh->body);
        in_loop_ = prev_in_loop;
        return false; // conservative
    }

    // for loop
    if (const auto* fl = dynamic_cast<const ast::ForStmt*>(&stmt)) {
        resolve_expression(*fl->iterable);

        enter_scope();

        // Declare loop variable
        std::string var_type = fl->var_type.empty() ? "Unknown" : fl->var_type;
        current_scope_->declare({fl->variable,
                                 SymbolKind::Variable,
                                 false,
                                 false,
                                 false, // is_moved
                                 ast::Visibility::None,
                                 "",
                                 var_type,
                                 {}});

        bool prev_in_loop = in_loop_;
        in_loop_ = true;
        resolve_statement(*fl->body);
        in_loop_ = prev_in_loop;

        exit_scope();
        return false;
    }

    // loop
    if (const auto* lp = dynamic_cast<const ast::LoopStmt*>(&stmt)) {
        bool prev_in_loop = in_loop_;
        bool prev_break_found = break_found_;
        in_loop_ = true;
        break_found_ = false;

        resolve_statement(*lp->body);

        bool infinite = !break_found_;
        in_loop_ = prev_in_loop;
        break_found_ = prev_break_found;
        return infinite;
    }

    // break
    if (dynamic_cast<const ast::BreakStmt*>(&stmt)) {
        if (!in_loop_) {
            throw DiagnosticError("'break' used outside of loop", 0, 0);
        }
        break_found_ = true;
        return false;
    }

    // continue
    if (dynamic_cast<const ast::ContinueStmt*>(&stmt)) {
        if (!in_loop_) {
            throw DiagnosticError("'continue' used outside of loop", 0, 0);
        }
        return false;
    }

    // expression statement
    if (const auto* es = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
        resolve_expression(*es->expression);
        return type_of(*es->expression).kind == TypeKind::Never;
    }

    // match statement
    if (const auto* ms = dynamic_cast<const ast::MatchStmt*>(&stmt)) {
        FluxType subject_type = type_of(*ms->expression);

        if (subject_type.kind == TypeKind::Enum && !enum_variants_.contains(subject_type.name)) {
            throw DiagnosticError("unknown enum type '" + subject_type.name + "' in match", 0, 0);
        }

        bool has_wildcard = false;
        bool all_arms_return = true;

        std::unordered_set<std::string> seen_enum_variants;
        bool seen_true = false;
        bool seen_false = false;

        resolve_expression(*ms->expression);

        for (const auto& arm : ms->arms) {
            // Track patterns
            if (dynamic_cast<const ast::WildcardPattern*>(arm.pattern.get())) {
                has_wildcard = true;
            } else if (const auto* vp =
                           dynamic_cast<const ast::VariantPattern*>(arm.pattern.get())) {
                seen_enum_variants.insert(vp->variant_name);
            } else if (const auto* ip =
                           dynamic_cast<const ast::IdentifierPattern*>(arm.pattern.get())) {
                // Check if this identifier is an enum variant (bare variant
                // name)
                if (is_enum_variant(ip->name)) {
                    seen_enum_variants.insert(ip->name);
                }
                // Built-in Option/Result variants
                if (ip->name == "None" || ip->name == "Some" || ip->name == "Ok" ||
                    ip->name == "Err") {
                    seen_enum_variants.insert(ip->name);
                }
            } else if (const auto* lp =
                           dynamic_cast<const ast::LiteralPattern*>(arm.pattern.get())) {
                if (const auto* b = dynamic_cast<const ast::BoolExpr*>(lp->literal.get())) {
                    if (b->value)
                        seen_true = true;
                    else
                        seen_false = true;
                }
            }

            // Resolve arm body in its own scope
            enter_scope();
            resolve_pattern(*arm.pattern, subject_type);

            // Resolve guard if present
            if (arm.guard) {
                resolve_expression(*arm.guard);
            }

            bool arm_returns = resolve_statement(*arm.body);
            exit_scope();
            all_arms_return = all_arms_return && arm_returns;
        }

        // Exhaustiveness rules
        std::vector<const ast::Pattern*> patterns_to_check;
        for (const auto& arm : ms->arms) {
            if (!arm.guard) {
                patterns_to_check.push_back(arm.pattern.get());
            }
        }

        if (!is_pattern_exhaustive(subject_type, patterns_to_check)) {
            throw DiagnosticError("non-exhaustive match on '" + subject_type.name +
                                      "' (missing cases or add '_' wildcard)",
                                  0, 0);
        }
        return all_arms_return;
    }

    throw DiagnosticError("unsupported statement", 0, 0);
}

/* =======================
   Expressions
   ======================= */

void Resolver::resolve_expression(const ast::Expr& expr) {
    if (const auto* id = dynamic_cast<const ast::IdentifierExpr*>(&expr)) {
        // Enum variants are NOT variables and do not require lookup
        if (is_enum_variant(id->name)) {
            return;
        }

        // Built-in identifiers
        if (id->name == "self" || id->name == "Self" || id->name == "drop" || id->name == "panic" ||
            id->name == "assert" || id->name == "Some" || id->name == "None" || id->name == "Ok" ||
            id->name == "Err") {
            return;
        }

        Symbol* sym = current_scope_->lookup_mut(id->name);
        if (!sym) {
            throw DiagnosticError("use of undeclared identifier '" + id->name + "'", 0, 0);
        }
        if (sym->kind == SymbolKind::Variable && sym->is_moved) {
            throw DiagnosticError("use of moved value '" + id->name + "'", 0, 0);
        }
        return;
    }

    if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
        (void)type_of(expr);

        // Determine parameter types for implicit move check
        std::vector<FluxType> param_types;
        bool is_inferred_ctor = false; // For Ok, Err, Some

        if (auto callee_id = dynamic_cast<const ast::IdentifierExpr*>(call->callee.get())) {
            if (callee_id->name == "Ok" || callee_id->name == "Err" || callee_id->name == "Some") {
                is_inferred_ctor = true;
            }
        }

        try {
            // Re-fetch type to get signature (safe since type_of(expr) succeeded)
            FluxType callee_type = type_of(*call->callee);
            if (callee_type.kind == TypeKind::Function) {
                param_types = callee_type.param_types;
            }
        } catch (...) {
        }

        for (size_t i = 0; i < call->arguments.size(); ++i) {
            resolve_expression(*call->arguments[i]);

            // Implicit move for non-Copy arguments
            if (auto id_expr = dynamic_cast<const ast::IdentifierExpr*>(call->arguments[i].get())) {
                bool should_move = false;
                if (is_inferred_ctor) {
                    should_move = true;
                } else if (i < param_types.size()) {
                    // If parameter is NOT a reference, it consumes the value
                    if (!param_types[i].name.starts_with("&")) {
                        should_move = true;
                    }
                }

                if (should_move) {
                    Symbol* sym = current_scope_->lookup_mut(id_expr->name);
                    if (sym && sym->kind == SymbolKind::Variable) {
                        if (!is_copy_type(sym->type)) {
                            std::cout << "DEBUG: Marking " << id_expr->name
                                      << " as moved (CallExpr)" << std::endl;
                            sym->is_moved = true;
                        }
                    }
                }
            }
        }
        return;
    }

    if (const auto* bin = dynamic_cast<const ast::BinaryExpr*>(&expr)) {
        if (bin->op == TokenKind::ColonColon || bin->op == TokenKind::Dot) {
            resolve_expression(*bin->left);
            return;
        }
        if (bin->op == TokenKind::LBracket) {
            resolve_expression(*bin->left);
            resolve_expression(*bin->right);
            return;
        }
        resolve_expression(*bin->left);
        resolve_expression(*bin->right);
        return;
    }

    if (const auto* un = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
        resolve_expression(*un->operand);
        return;
    }

    if (const auto* mv = dynamic_cast<const ast::MoveExpr*>(&expr)) {
        if (auto id = dynamic_cast<const ast::IdentifierExpr*>(mv->operand.get())) {
            // Check if it's a variable
            Symbol* sym = current_scope_->lookup_mut(id->name);
            if (!sym) {
                throw DiagnosticError("use of undeclared identifier '" + id->name + "'", 0, 0);
            }
            if (sym->kind == SymbolKind::Variable) {
                if (sym->is_moved) {
                    throw DiagnosticError("use of moved value '" + id->name + "'", 0, 0);
                }
                sym->is_moved = true;
            }
            return;
        }
        resolve_expression(*mv->operand);
        return;
    }

    if (const auto* sl = dynamic_cast<const ast::StructLiteralExpr*>(&expr)) {
        // Check that the struct type exists
        std::string base = sl->struct_name;
        if (auto pos = base.find('<'); pos != std::string::npos) {
            base = base.substr(0, pos);
        }
        if (!current_scope_->lookup(base)) {
            throw DiagnosticError("use of undeclared struct '" + sl->struct_name + "'", 0, 0);
        }
        for (const auto& field : sl->fields) {
            resolve_expression(*field.value);

            // Implicit move for struct fields
            if (auto id_expr = dynamic_cast<const ast::IdentifierExpr*>(field.value.get())) {
                Symbol* sym = current_scope_->lookup_mut(id_expr->name);
                if (sym && sym->kind == SymbolKind::Variable) {
                    if (!is_copy_type(sym->type)) {
                        sym->is_moved = true;
                    }
                }
            }
        }
        return;
    }

    if (const auto* cast = dynamic_cast<const ast::CastExpr*>(&expr)) {
        resolve_expression(*cast->expr);
        return;
    }

    if (const auto* ep = dynamic_cast<const ast::ErrorPropagationExpr*>(&expr)) {
        resolve_expression(*ep->operand);
        return;
    }

    if (const auto* aw = dynamic_cast<const ast::AwaitExpr*>(&expr)) {
        resolve_expression(*aw->operand);
        return;
    }

    if (const auto* sp = dynamic_cast<const ast::SpawnExpr*>(&expr)) {
        resolve_expression(*sp->operand);
        return;
    }

    if (const auto* rng = dynamic_cast<const ast::RangeExpr*>(&expr)) {
        if (rng->start)
            resolve_expression(*rng->start);
        if (rng->end)
            resolve_expression(*rng->end);
        return;
    }

    // literals (NumberExpr, StringExpr, BoolExpr, CharExpr) are fine
}

void Resolver::resolve_pattern(const ast::Pattern& pattern, const FluxType& subject_type) {
    if (const auto* id_pat = dynamic_cast<const ast::IdentifierPattern*>(&pattern)) {
        // Is this identifier an enum variant?
        if (is_enum_variant(id_pat->name)) {
            return;
        }

        // Option/Result variants in pattern
        if (id_pat->name == "None" || id_pat->name == "Some" || id_pat->name == "Ok" ||
            id_pat->name == "Err") {
            return;
        }

        // Otherwise: normal variable binding
        if (!current_scope_->declare({id_pat->name,
                                      SymbolKind::Variable,
                                      false,
                                      true,
                                      false, // is_moved
                                      ast::Visibility::None,
                                      "",
                                      stringify_type(subject_type),
                                      {}})) {
            throw DiagnosticError("duplicate variable '" + id_pat->name + "' in pattern", 0, 0);
        }
        return;
    }

    if (const auto* var_pat = dynamic_cast<const ast::VariantPattern*>(&pattern)) {
        // Resolve nested patterns for enums/Option/Result
        if (subject_type.kind == TypeKind::Option) {
            if (var_pat->variant_name == "Some" && !var_pat->sub_patterns.empty()) {
                resolve_pattern(*var_pat->sub_patterns[0], subject_type.generic_args[0]);
            }
        } else if (subject_type.kind == TypeKind::Result) {
            if (var_pat->variant_name == "Ok" && !var_pat->sub_patterns.empty()) {
                resolve_pattern(*var_pat->sub_patterns[0], subject_type.generic_args[0]);
            } else if (var_pat->variant_name == "Err" && !var_pat->sub_patterns.empty()) {
                resolve_pattern(*var_pat->sub_patterns[0], subject_type.generic_args[1]);
            }
        } else if (subject_type.kind == TypeKind::Enum) {
            // For general enums, we should verify it's a valid variant
            // For now, assume variants have no members or use Unknown
            for (const auto& sub : var_pat->sub_patterns) {
                resolve_pattern(*sub, unknown());
            }
        }
        return;
    }

    if (const auto* tup_pat = dynamic_cast<const ast::TuplePattern*>(&pattern)) {
        if (subject_type.kind != TypeKind::Tuple) {
            throw DiagnosticError(
                "expected tuple type for tuple pattern, found '" + subject_type.name + "'", 0, 0);
        }
        if (tup_pat->elements.size() != subject_type.generic_args.size()) {
            throw DiagnosticError("tuple pattern arity mismatch: expected " +
                                      std::to_string(subject_type.generic_args.size()) +
                                      ", found " + std::to_string(tup_pat->elements.size()),
                                  0, 0);
        }
        for (size_t i = 0; i < tup_pat->elements.size(); ++i) {
            resolve_pattern(*tup_pat->elements[i], subject_type.generic_args[i]);
        }
        return;
    }

    if (const auto* struct_pat = dynamic_cast<const ast::StructPattern*>(&pattern)) {
        if (subject_type.kind != TypeKind::Struct) {
            throw DiagnosticError(
                "expected struct type for struct pattern, found '" + subject_type.name + "'", 0, 0);
        }
        if (!struct_fields_.contains(subject_type.name)) {
            throw DiagnosticError("unknown struct '" + subject_type.name + "' in struct pattern", 0,
                                  0);
        }
        const auto& fields = struct_fields_.at(subject_type.name);
        for (const auto& fp : struct_pat->fields) {
            bool found = false;
            for (const auto& info : fields) {
                if (fp.field_name == info.name) {
                    resolve_pattern(*fp.pattern, type_from_name(info.type));
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw DiagnosticError("struct '" + subject_type.name + "' has no field named '" +
                                          fp.field_name + "'",
                                      0, 0);
            }
        }
        return;
    }

    if (const auto* lit_pat = dynamic_cast<const ast::LiteralPattern*>(&pattern)) {
        resolve_expression(*lit_pat->literal);
        FluxType lit_type = type_of(*lit_pat->literal);
        if (!are_types_compatible(subject_type, lit_type)) {
            throw DiagnosticError("literal pattern type '" + lit_type.name +
                                      "' is incompatible with subject type '" + subject_type.name +
                                      "'",
                                  0, 0);
        }
        return;
    }

    if (const auto* or_pat = dynamic_cast<const ast::OrPattern*>(&pattern)) {
        if (or_pat->alternatives.size() < 2) {
            throw DiagnosticError("or-pattern must have at least two alternatives", 0, 0);
        }

        std::map<std::string, FluxType> expected_bindings;
        bool first = true;

        for (const auto& alt : or_pat->alternatives) {
            enter_scope();
            resolve_pattern(*alt, subject_type);

            std::map<std::string, FluxType> current_bindings;
            for (const auto& [name, sym] : current_scope_->get_symbols()) {
                current_bindings[name] = type_from_name(sym.type);
            }
            exit_scope();

            if (first) {
                expected_bindings = std::move(current_bindings);
                first = false;
            } else {
                if (current_bindings.size() != expected_bindings.size()) {
                    throw DiagnosticError(
                        "all alternatives in an or-pattern must bind the same variables", 0, 0);
                }
                for (const auto& [name, type] : expected_bindings) {
                    if (current_bindings.find(name) == current_bindings.end()) {
                        throw DiagnosticError(
                            "variable '" + name +
                                "' is not bound in all alternatives of or-pattern",
                            0, 0);
                    }
                    if (!this->are_types_compatible(type, current_bindings.at(name))) {
                        throw DiagnosticError(
                            "variable '" + name +
                                "' has inconsistent types across or-pattern alternatives",
                            0, 0);
                    }
                }
            }
        }

        for (const auto& [name, type] : expected_bindings) {
            current_scope_->declare({name,
                                     SymbolKind::Variable,
                                     false,
                                     true,
                                     false,
                                     ast::Visibility::None,
                                     "",
                                     stringify_type(type),
                                     {}});
        }
        return;
    }

    if (const auto* range_pat = dynamic_cast<const ast::RangePattern*>(&pattern)) {
        resolve_expression(*range_pat->start);
        resolve_expression(*range_pat->end);
        FluxType start_type = type_of(*range_pat->start);
        FluxType end_type = type_of(*range_pat->end);

        if (!this->are_types_compatible(start_type, end_type)) {
            throw DiagnosticError("range pattern bounds must have compatible types", 0, 0);
        }

        if (!this->are_types_compatible(subject_type, start_type)) {
            throw DiagnosticError("range pattern type mismatch: expected '" + subject_type.name +
                                      "', found '" + start_type.name + "'",
                                  0, 0);
        }

        if (start_type.kind != TypeKind::Int && start_type.kind != TypeKind::Float &&
            start_type.kind != TypeKind::Char) {
            throw DiagnosticError("range patterns are only supported for numeric and char types", 0,
                                  0);
        }

        return;
    }

    // WildcardPattern is fine
}

std::string Resolver::stringify_type(const FluxType& type) const {
    if (type.kind == TypeKind::Ref) {
        return (type.is_mut_ref ? "&mut " : "&") +
               (type.generic_args.empty() ? type.name : stringify_type(type.generic_args[0]));
    }
    if (type.kind == TypeKind::Tuple) {
        std::string res = "(";
        for (size_t i = 0; i < type.generic_args.size(); ++i) {
            if (i > 0)
                res += ", ";
            res += stringify_type(type.generic_args[i]);
        }
        res += ")";
        return res;
    }
    if (type.kind == TypeKind::Function) {
        std::string res = "(";
        for (size_t i = 0; i < type.param_types.size(); ++i) {
            if (i > 0)
                res += ", ";
            res += stringify_type(type.param_types[i]);
        }
        res += ") -> " + (type.return_type ? stringify_type(*type.return_type) : "Void");
        return res;
    }

    std::string base = type.name;
    if (!type.generic_args.empty()) {
        base += "<";
        for (size_t i = 0; i < type.generic_args.size(); ++i) {
            if (i > 0)
                base += ", ";
            base += stringify_type(type.generic_args[i]);
        }
        base += ">";
    }
    return base;
}

bool Resolver::is_pattern_exhaustive(const FluxType& type,
                                     const std::vector<const ast::Pattern*>& patterns) const {
    if (patterns.empty())
        return false;

    // 1. Check for catch-all patterns
    for (const auto* pat : patterns) {
        if (dynamic_cast<const ast::WildcardPattern*>(pat))
            return true;
        if (const auto* id_pat = dynamic_cast<const ast::IdentifierPattern*>(pat)) {
            if (!is_enum_variant(id_pat->name) && id_pat->name != "None" &&
                id_pat->name != "Some" && id_pat->name != "Ok" && id_pat->name != "Err") {
                return true;
            }
        }
    }

    // 2. Handle specific types
    if (type.kind == TypeKind::Bool) {
        bool true_covered = false;
        bool false_covered = false;
        for (const auto* pat : patterns) {
            if (const auto* lit = dynamic_cast<const ast::LiteralPattern*>(pat)) {
                if (const auto* b = dynamic_cast<const ast::BoolExpr*>(lit->literal.get())) {
                    if (b->value)
                        true_covered = true;
                    else
                        false_covered = true;
                }
            } else if (const auto* or_pat = dynamic_cast<const ast::OrPattern*>(pat)) {
                std::vector<const ast::Pattern*> alts;
                for (const auto& a : or_pat->alternatives)
                    alts.push_back(a.get());
                if (is_pattern_exhaustive(type, alts))
                    return true;
            }
        }
        return true_covered && false_covered;
    }

    if (type.kind == TypeKind::Enum || type.kind == TypeKind::Option ||
        type.kind == TypeKind::Result) {
        std::vector<std::string> variants;
        if (type.kind == TypeKind::Option) {
            variants = {"Some", "None"};
        } else if (type.kind == TypeKind::Result) {
            variants = {"Ok", "Err"};
        } else {
            if (enum_variants_.find(type.name) == enum_variants_.end())
                return false;
            variants = enum_variants_.at(type.name);
        }

        for (const auto& variant : variants) {
            std::vector<const ast::Pattern*> sub_patterns;
            FluxType member_type = unknown_type();

            if (type.kind == TypeKind::Option && variant == "Some") {
                member_type = type.generic_args[0];
            } else if (type.kind == TypeKind::Result) {
                if (variant == "Ok")
                    member_type = type.generic_args[0];
                else if (variant == "Err")
                    member_type = type.generic_args[1];
            }

            bool variant_fully_covered = false;
            for (const auto* pat : patterns) {
                if (const auto* var_pat = dynamic_cast<const ast::VariantPattern*>(pat)) {
                    if (var_pat->variant_name == variant ||
                        var_pat->variant_name == type.name + "::" + variant) {
                        if (var_pat->sub_patterns.empty()) {
                            variant_fully_covered = true;
                            break;
                        }
                        for (const auto& sp : var_pat->sub_patterns)
                            sub_patterns.push_back(sp.get());
                    }
                } else if (const auto* id_pat = dynamic_cast<const ast::IdentifierPattern*>(pat)) {
                    if (id_pat->name == variant || id_pat->name == type.name + "::" + variant) {
                        variant_fully_covered = true;
                        break;
                    }
                } else if (const auto* or_pat = dynamic_cast<const ast::OrPattern*>(pat)) {
                    // This is a bit simplified, but handle top-level Or in enums
                    for (const auto& alt : or_pat->alternatives) {
                        if (const auto* sub_var =
                                dynamic_cast<const ast::VariantPattern*>(alt.get())) {
                            if (sub_var->variant_name == variant ||
                                sub_var->variant_name == type.name + "::" + variant) {
                                if (sub_var->sub_patterns.empty()) {
                                    variant_fully_covered = true;
                                    break;
                                }
                                for (const auto& sp : sub_var->sub_patterns)
                                    sub_patterns.push_back(sp.get());
                            }
                        }
                    }
                }
            }

            if (variant_fully_covered)
                continue;

            if (sub_patterns.empty())
                return false;

            if (member_type.kind != TypeKind::Unknown) {
                if (!is_pattern_exhaustive(member_type, sub_patterns))
                    return false;
            }
        }
        return true;
    }

    // Tuples and Structs are considered non-exhaustive without a wildcard for now
    // unless we implement the full matrix algorithm.
    return false;
}

bool Resolver::is_signed_int_name(const std::string& name) const {
    return name.starts_with("Int") && name != "IntPtr";
}

bool Resolver::is_unsigned_int_name(const std::string& name) const {
    return name.starts_with("UInt") && name != "UIntPtr";
}

bool Resolver::is_float_name(const std::string& name) const {
    return name.starts_with("Float");
}

int Resolver::numeric_width(const std::string& name) const {
    if (name == "IntPtr" || name == "UIntPtr") {
        return static_cast<int>(sizeof(void*) * 8);
    }
    for (size_t i = 0; i < name.size(); ++i) {
        if (std::isdigit(name[i])) {
            try {
                return std::stoi(name.substr(i));
            } catch (...) {
                return 0;
            }
        }
    }
    return 0;
}

std::string Resolver::promote_integer_name(const std::string& a, const std::string& b) const {
    bool a_signed = is_signed_int_name(a);
    bool b_signed = is_signed_int_name(b);
    int a_width = numeric_width(a);
    int b_width = numeric_width(b);
    int width = std::max(a_width, b_width);
    if (a_signed || b_signed) {
        return "Int" + std::to_string(width);
    } else {
        return "UInt" + std::to_string(width);
    }
}

bool Resolver::are_types_compatible(const FluxType& target, const FluxType& source) const {
    if (target.kind == TypeKind::Unknown || source.kind == TypeKind::Unknown) {
        return true;
    }

    if (source.kind == TypeKind::Never) {
        return true;
    }

    if (target == source) {
        return true;
    }

    // Special handling for Option<T>
    if (target.kind == TypeKind::Option && source.kind == TypeKind::Option) {
        // Option<Unknown> (from None) is compatible with any Option<T>
        if (source.generic_args.empty() || source.generic_args[0].kind == TypeKind::Unknown) {
            return true;
        }
        // Check inner types
        if (!target.generic_args.empty() && !source.generic_args.empty()) {
            return are_types_compatible(target.generic_args[0], source.generic_args[0]);
        }
    }

    // Special handling for Result<T, E>
    if (target.kind == TypeKind::Result && source.kind == TypeKind::Result) {
        // Result<T, Unknown> (from Ok(T)) -> compatible if T matches
        // Result<Unknown, E> (from Err(E)) -> compatible if E matches
        bool t_ok = true;
        bool e_ok = true;

        // Check T
        if (!target.generic_args.empty() && !source.generic_args.empty()) {
            // If source T is unknown, it means it's an Err variant, so it
            // matches any T target
            if (source.generic_args[0].kind != TypeKind::Unknown) {
                t_ok = are_types_compatible(target.generic_args[0], source.generic_args[0]);
            }
        }

        // Check E
        if (target.generic_args.size() > 1 && source.generic_args.size() > 1) {
            // If source E is unknown, it means it's an Ok variant, so it
            // matches any E target
            if (source.generic_args[1].kind != TypeKind::Unknown) {
                e_ok = are_types_compatible(target.generic_args[1], source.generic_args[1]);
            }
        }

        return t_ok && e_ok;
    }

    return false;
}

// --- Trait bound helpers ---

std::vector<TypeParamBound>
Resolver::parse_type_param_bounds(const std::vector<std::string>& type_params) {
    std::vector<TypeParamBound> result;
    for (const auto& tp : type_params) {
        TypeParamBound bound;
        // Format: "T" or "T: Trait1 + Trait2"
        auto colon_pos = tp.find(':');
        if (colon_pos == std::string::npos) {
            bound.param_name = tp;
            // No bounds
        } else {
            bound.param_name = tp.substr(0, colon_pos);
            // Trim leading/trailing spaces from param name
            while (!bound.param_name.empty() && bound.param_name.back() == ' ')
                bound.param_name.pop_back();
            // Parse bounds after colon
            std::string bounds_str = tp.substr(colon_pos + 1);
            // Split by '+'
            size_t start = 0;
            while (start < bounds_str.size()) {
                auto plus_pos = bounds_str.find('+', start);
                std::string trait_name;
                if (plus_pos == std::string::npos) {
                    trait_name = bounds_str.substr(start);
                    start = bounds_str.size();
                } else {
                    trait_name = bounds_str.substr(start, plus_pos - start);
                    start = plus_pos + 1;
                }
                // Trim whitespace
                size_t first = trait_name.find_first_not_of(' ');
                size_t last = trait_name.find_last_not_of(' ');
                if (first != std::string::npos) {
                    bound.bounds.push_back(trait_name.substr(first, last - first + 1));
                }
            }
        }
        if (!bound.param_name.empty() && !bound.bounds.empty()) {
            result.push_back(std::move(bound));
        }
    }
    return result;
}

bool Resolver::type_implements_trait(const std::string& type_name,
                                     const std::string& trait_name) const {
    auto it = trait_impls_.find(type_name);
    if (it != trait_impls_.end()) {
        return it->second.count(trait_name) > 0;
    }
    return false;
}

std::vector<TypeParamBound> Resolver::parse_where_clause(const std::string& where_clause) {
    std::vector<TypeParamBound> result;
    if (where_clause.empty())
        return result;

    // "T: Trait + Other, U: Trait"
    size_t start = 0;
    while (start < where_clause.size()) {
        auto comma_pos = where_clause.find(',', start);
        std::string part;
        if (comma_pos == std::string::npos) {
            part = where_clause.substr(start);
            start = where_clause.size();
        } else {
            part = where_clause.substr(start, comma_pos - start);
            start = comma_pos + 1;
        }

        // Parse "T: Bounds"
        auto colon_pos = part.find(':');
        if (colon_pos != std::string::npos) {
            TypeParamBound bound;
            bound.param_name = part.substr(0, colon_pos);
            // Trim param name
            size_t p_first = bound.param_name.find_first_not_of(" \t\n");
            if (p_first != std::string::npos) {
                size_t p_last = bound.param_name.find_last_not_of(" \t\n");
                bound.param_name = bound.param_name.substr(p_first, p_last - p_first + 1);
            }

            std::string bounds_str = part.substr(colon_pos + 1);
            // Split bounds by '+'
            size_t b_start = 0;
            while (b_start < bounds_str.size()) {
                auto plus_pos = bounds_str.find('+', b_start);
                std::string trait_name;
                if (plus_pos == std::string::npos) {
                    trait_name = bounds_str.substr(b_start);
                    b_start = bounds_str.size();
                } else {
                    trait_name = bounds_str.substr(b_start, plus_pos - b_start);
                    b_start = plus_pos + 1;
                }
                // Trim trait name
                size_t t_first = trait_name.find_first_not_of(" \t\n");
                if (t_first != std::string::npos) {
                    size_t t_last = trait_name.find_last_not_of(" \t\n");
                    bound.bounds.push_back(trait_name.substr(t_first, t_last - t_first + 1));
                }
            }
            if (!bound.bounds.empty()) {
                result.push_back(std::move(bound));
            }
        }
    }
    return result;
}

void Resolver::record_function_instantiation(const std::string& name,
                                             const std::vector<FluxType>& args) {
    FunctionInstantiation inst{name, args};
    for (const auto& existing : function_instantiations_) {
        if (existing == inst)
            return;
    }
    function_instantiations_.push_back(std::move(inst));
}

void Resolver::record_type_instantiation(const std::string& name,
                                         const std::vector<FluxType>& args) {
    TypeInstantiation inst{name, args};
    for (const auto& existing : type_instantiations_) {
        if (existing == inst)
            return;
    }
    type_instantiations_.push_back(std::move(inst));
}

std::vector<std::string> Resolver::get_bounds_for_type(const std::string& type_name) {
    std::vector<std::string> bounds;
    // Strip references
    std::string base = type_name;
    if (base.starts_with("&mut "))
        base = base.substr(5);
    else if (base.starts_with("&"))
        base = base.substr(1);

    if (!current_function_name_.empty() && function_type_params_.contains(current_function_name_)) {
        for (const auto& p : function_type_params_.at(current_function_name_)) {
            if (p.starts_with(base + ":")) {
                size_t colon = p.find(':');
                std::string traits_list = p.substr(colon + 1);
                size_t start = 0;
                size_t plus = traits_list.find('+');
                while (plus != std::string::npos) {
                    std::string t = traits_list.substr(start, plus - start);
                    t.erase(0, t.find_first_not_of(" \t"));
                    t.erase(t.find_last_not_of(" \t") + 1);
                    if (!t.empty())
                        bounds.push_back(t);
                    start = plus + 1;
                    plus = traits_list.find('+', start);
                }
                std::string t = traits_list.substr(start);
                t.erase(0, t.find_first_not_of(" \t"));
                t.erase(t.find_last_not_of(" \t") + 1);
                if (!t.empty())
                    bounds.push_back(t);
            }
        }
    }
    if (bounds.empty() && !current_type_name_.empty() &&
        type_type_params_.contains(current_type_name_)) {
        for (const auto& p : type_type_params_.at(current_type_name_)) {
            if (p.starts_with(base + ":")) {
                size_t colon = p.find(':');
                std::string traits_list = p.substr(colon + 1);
                size_t start = 0;
                size_t plus = traits_list.find('+');
                while (plus != std::string::npos) {
                    std::string t = traits_list.substr(start, plus - start);
                    t.erase(0, t.find_first_not_of(" \t"));
                    t.erase(t.find_last_not_of(" \t") + 1);
                    if (!t.empty())
                        bounds.push_back(t);
                    start = plus + 1;
                    plus = traits_list.find('+', start);
                }
                std::string t = traits_list.substr(start);
                t.erase(0, t.find_first_not_of(" \t"));
                t.erase(t.find_last_not_of(" \t") + 1);
                if (!t.empty())
                    bounds.push_back(t);
            }
        }
    }
    return bounds;
}

bool Resolver::compare_signatures(
    const TraitMethodSig& trait_sig, const ast::FunctionDecl& impl_fn,
    const std::string& target_type,
    const std::unordered_map<std::string, std::string>& mapping) const {
    // 1. Compare return type (with Self substitution)
    std::string trait_ret = trait_sig.return_type;
    if (trait_ret == "Self")
        trait_ret = target_type;
    for (const auto& [gen, concrete] : mapping) {
        if (trait_ret == gen)
            trait_ret = concrete;
    }

    std::string impl_ret = impl_fn.return_type;
    if (impl_ret == "Self")
        impl_ret = target_type;

    if (trait_ret != impl_ret)
        return false;

    // 2. Compare receiver (self/&self/&mut self)
    std::string impl_self_type;
    std::vector<std::string> impl_param_types;
    for (const auto& p : impl_fn.params) {
        if (p.name == "self")
            impl_self_type = p.type;
        else
            impl_param_types.push_back(p.type);
    }

    std::string trait_self = trait_sig.self_type;
    if (trait_self.find("Self") != std::string::npos) {
        auto pos = trait_self.find("Self");
        trait_self.replace(pos, 4, target_type);
    }

    // Substitute Self in impl_self_type too
    if (impl_self_type.find("Self") != std::string::npos) {
        auto pos = impl_self_type.find("Self");
        impl_self_type.replace(pos, 4, target_type);
    }

    if (trait_self != impl_self_type)
        return false;

    // 3. Compare other parameters
    if (trait_sig.param_types.size() != impl_param_types.size())
        return false;

    for (size_t i = 0; i < trait_sig.param_types.size(); ++i) {
        std::string trait_p = trait_sig.param_types[i];
        if (trait_p == "Self")
            trait_p = target_type;
        for (const auto& [gen, concrete] : mapping) {
            if (trait_p == gen)
                trait_p = concrete;
        }

        std::string impl_p = impl_param_types[i];
        if (impl_p == "Self")
            impl_p = target_type;

        if (trait_p != impl_p)
            return false;
    }

    return true;
}

bool Resolver::is_copy_type(const std::string& type_name) {
    if (type_name == "Int8" || type_name == "Int16" || type_name == "Int32" ||
        type_name == "Int64" || type_name == "Int128" || type_name == "UInt8" ||
        type_name == "UInt16" || type_name == "UInt32" || type_name == "UInt64" ||
        type_name == "UInt128" || type_name == "IntPtr" || type_name == "UIntPtr" ||
        type_name == "Float32" || type_name == "Float64" || type_name == "Bool" ||
        type_name == "Char" || type_name == "Void" || type_name == "Never") {
        return true;
    }
    // Pointers are Copy
    if (type_name.starts_with("*"))
        return true;
    // References are Copy (they are non-owning)
    if (type_name.starts_with("&"))
        return true;
    return false;
}

void Resolver::monomorphize_recursive() {

    size_t processed = 0;
    while (processed < function_instantiations_.size()) {
        auto inst = function_instantiations_[processed++];

        const ast::FunctionDecl* fn = nullptr;
        if (function_decls_.contains(inst.name)) {
            fn = function_decls_.at(inst.name);
        }

        if (!fn)
            continue;

        // Setup substitution map for this specific instantiation
        substitution_map_.clear();
        auto it = function_type_params_.find(inst.name);
        if (it != function_type_params_.end()) {
            std::vector<std::string> raw_params;
            for (const auto& p : it->second) {
                if (p.find(':') == std::string::npos) {
                    raw_params.push_back(p);
                }
            }
            for (size_t i = 0; i < raw_params.size() && i < inst.args.size(); ++i) {
                substitution_map_[raw_params[i]] = inst.args[i];
            }
        }

        // Re-resolve function body with concrete types
        // This will trigger additional record_function_instantiation calls for transitive calls
        try {
            resolve_function(*fn, inst.name);
        } catch (const DiagnosticError&) {
            // During monomorphization, some re-resolutions may fail due to
            // generic types not being fully resolved yet. This is expected.
        }
    }
}

} // namespace flux::semantic
