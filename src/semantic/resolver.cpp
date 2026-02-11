#include "resolver.h"

#include <ranges>

#include "lexer/diagnostic.h"
#include "type.h"

#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace flux::semantic {
Type Resolver::type_from_name(const std::string& name) const {
    using semantic::Type;
    using semantic::TypeKind;
    Type Resolver::type_from_name(const std::string& name) const {
        using semantic::Type;
        using semantic::TypeKind;

        if (name.starts_with("Int") || name.starts_with("UInt") || name == "IntPtr" ||
            name == "UIntPtr")
            return {TypeKind::Int, name};
        if (name.starts_with("Int") || name.starts_with("UInt") || name == "IntPtr" ||
            name == "UIntPtr")
            return {TypeKind::Int, name};

        if (name.starts_with("Float"))
            return {TypeKind::Float, name};
        if (name.starts_with("Float"))
            return {TypeKind::Float, name};

        if (name == "Bool")
            return {TypeKind::Bool, name};
        if (name == "Bool")
            return {TypeKind::Bool, name};

        if (name == "String")
            return {TypeKind::String, name};
        if (name == "String")
            return {TypeKind::String, name};

        if (name == "Char")
            return {TypeKind::Char, name};
        if (name == "Char")
            return {TypeKind::Char, name};

        if (name == "Void")
            return {TypeKind::Void, name};
        if (name == "Void")
            return {TypeKind::Void, name};

        if (name == "Never")
            return {TypeKind::Never, name};
        if (name == "Never")
            return {TypeKind::Never, name};

        // Enum types
        if (enum_variants_.contains(name)) {
            return {TypeKind::Enum, name};
        }
        // Enum types
        if (enum_variants_.contains(name)) {
            return {TypeKind::Enum, name};
        }

        // Struct/class types
        if (struct_fields_.contains(name)) {
            return {TypeKind::Struct, name};
        }
        // Struct/class types
        if (struct_fields_.contains(name)) {
            return {TypeKind::Struct, name};
        }

        // Type aliases
        if (type_aliases_.contains(name)) {
            return type_from_name(type_aliases_.at(name));
        }
        // Type aliases
        if (type_aliases_.contains(name)) {
            return type_from_name(type_aliases_.at(name));
        }

        // Generic types (e.g., Box<Int32>)
        if (name.find('<') != std::string::npos) {
            std::string base = name.substr(0, name.find('<'));
            if (struct_fields_.contains(base) || enum_variants_.contains(base)) {
                return {TypeKind::Struct, name};
            }
        }
        // Generic types (e.g., Box<Int32>)
        if (name.find('<') != std::string::npos) {
            std::string base = name.substr(0, name.find('<'));
            if (struct_fields_.contains(base) || enum_variants_.contains(base)) {
                return {TypeKind::Struct, name};
            }
        }

        // Reference types
        if (name.starts_with("&")) {
            // Check for &mut T
            bool is_mut = false;
            std::string inner = name.substr(1);
            if (inner.starts_with("mut ")) {
                is_mut = true;
                inner = inner.substr(4); // skip "mut "
            }
            // Remove any leading/trailing whitespace
            while (!inner.empty() && std::isspace(inner.front()))
                inner.erase(0, 1);
            while (!inner.empty() && std::isspace(inner.back()))
                inner.pop_back();
            std::string ref_name = "&";
            if (is_mut)
                ref_name += "mut ";
            ref_name += inner;
            return Type{TypeKind::Ref, ref_name, is_mut};
        }

        // Tuple types
        if (name.starts_with("(")) {
            return {TypeKind::Tuple, name};
        }
        // Tuple types
        if (name.starts_with("(")) {
            return {TypeKind::Tuple, name};
        }

        // Option<T>
        if (name.starts_with("Option<")) {
            auto start = name.find('<') + 1;
            auto end = name.find_last_of('>');
            std::string inner = name.substr(start, end - start);
            // Trim whitespace
            inner.erase(0, inner.find_first_not_of(" \t\n\r"));
            inner.erase(inner.find_last_not_of(" \t\n\r") + 1);
            Type inner_type = type_from_name(inner);
            return Type(TypeKind::Option, name, false, {}, nullptr, {inner_type});
        }
        // Result<T,E>
        if (name.starts_with("Result<")) {
            auto start = name.find('<') + 1;
            auto end = name.find_last_of('>');
            std::string inner = name.substr(start, end - start);
            auto comma = inner.find(',');
            std::string ok = inner.substr(0, comma);
            std::string err = inner.substr(comma + 1);
            // Trim whitespace for both ok and err
            ok.erase(0, ok.find_first_not_of(" \t\n\r"));
            ok.erase(ok.find_last_not_of(" \t\n\r") + 1);
            err.erase(0, err.find_first_not_of(" \t\n\r"));
            err.erase(err.find_last_not_of(" \t\n\r") + 1);
            Type ok_type = type_from_name(ok);
            Type err_type = type_from_name(err);
            return Type(TypeKind::Result, name, false, {}, nullptr, {ok_type, err_type});
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

    Type Resolver::type_of(const ast::Expr& expr) {
        if (auto lambda = dynamic_cast<const ast::LambdaExpr*>(&expr)) {
            using semantic::Type;
            using semantic::TypeKind;
            std::vector<Type> param_types;
            for (const auto& param : lambda->params) {
                param_types.push_back(type_from_name(param.type));
            }
            Type ret_type = type_from_name(lambda->return_type);
            Type fn_type;
            fn_type.kind = TypeKind::Function;
            fn_type.name = "fn";
            fn_type.param_types = std::move(param_types);
            fn_type.return_type = std::make_unique<Type>(ret_type);
            return fn_type;
        }
        using semantic::Type;
        using semantic::TypeKind;

        if (auto arr = dynamic_cast<const ast::ArrayExpr*>(&expr)) {
            if (arr->elements.empty()) {
                throw DiagnosticError("empty array literal is not allowed", 0, 0);
            }
            Type first_type = type_of(*arr->elements[0]);
            for (size_t i = 1; i < arr->elements.size(); ++i) {
                Type t = type_of(*arr->elements[i]);
                if (t != first_type) {
                    throw DiagnosticError("array elements must have the same type", 0, 0);
                }
            }
            std::string name =
                "[" + first_type.name + ";" + std::to_string(arr->elements.size()) + "]";
            return {TypeKind::Array, name};
        }

        if (auto slice = dynamic_cast<const ast::SliceExpr*>(&expr)) {
            // The array part must be an array
            Type arr_type = type_of(*slice->array);
            if (arr_type.kind != TypeKind::Array) {
                throw DiagnosticError("slice base must be an array", 0, 0);
            }
            // Extract element type from array type name: [T;N] -> [T]
            std::string elem_type;
            auto lbrack = arr_type.name.find('[');
            auto semi = arr_type.name.find(';');
            if (lbrack != std::string::npos && semi != std::string::npos && semi > lbrack + 1) {
                elem_type = arr_type.name.substr(lbrack + 1, semi - lbrack - 1);
            } else {
                elem_type = "Unknown";
            }
            std::string name = "[" + elem_type + "]";
            return {TypeKind::Slice, name};
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
        if (auto arr = dynamic_cast<const ast::ArrayExpr*>(&expr)) {
            if (arr->elements.empty()) {
                throw DiagnosticError("empty array literal is not allowed", 0, 0);
            }
            Type first_type = type_of(*arr->elements[0]);
            for (size_t i = 1; i < arr->elements.size(); ++i) {
                Type t = type_of(*arr->elements[i]);
                if (t != first_type) {
                    throw DiagnosticError("array elements must have the same type", 0, 0);
                }
            }
            std::string name =
                "[" + first_type.name + ";" + std::to_string(arr->elements.size()) + "]";
            return {TypeKind::Array, name};
        }

        if (auto slice = dynamic_cast<const ast::SliceExpr*>(&expr)) {
            // The array part must be an array
            Type arr_type = type_of(*slice->array);
            if (arr_type.kind != TypeKind::Array) {
                throw DiagnosticError("slice base must be an array", 0, 0);
            }
            // Extract element type from array type name: [T;N] -> [T]
            std::string elem_type;
            auto lbrack = arr_type.name.find('[');
            auto semi = arr_type.name.find(';');
            if (lbrack != std::string::npos && semi != std::string::npos && semi > lbrack + 1) {
                elem_type = arr_type.name.substr(lbrack + 1, semi - lbrack - 1);
            } else {
                elem_type = "Unknown";
            }
            std::string name = "[" + elem_type + "]";
            return {TypeKind::Slice, name};
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
        if (dynamic_cast<const ast::BoolExpr*>(&expr)) {
            return {TypeKind::Bool, "Bool"};
        }

        if (dynamic_cast<const ast::StringExpr*>(&expr)) {
            return {TypeKind::String, "String"};
        }
        if (dynamic_cast<const ast::StringExpr*>(&expr)) {
            return {TypeKind::String, "String"};
        }

        if (auto tuple = dynamic_cast<const ast::TupleExpr*>(&expr)) {
            std::string name = "(";
            bool first = true;
            for (const auto& elem : tuple->elements) {
                Type t = type_of(*elem);
                if (!first)
                    name += ",";
                name += t.name;
                first = false;
            }
            name += ")";
            return {TypeKind::Tuple, name};
        }

        if (dynamic_cast<const ast::CharExpr*>(&expr)) {
            return {TypeKind::Char, "Char"};
        }
        if (auto tuple = dynamic_cast<const ast::TupleExpr*>(&expr)) {
            std::string name = "(";
            bool first = true;
            for (const auto& elem : tuple->elements) {
                Type t = type_of(*elem);
                if (!first)
                    name += ",";
                name += t.name;
                first = false;
            }
            name += ")";
            return {TypeKind::Tuple, name};
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
            if (auto id = dynamic_cast<const ast::IdentifierExpr*>(&expr)) {
                // Check if it's an enum variant used standalone
                if (is_enum_variant(id->name)) {
                    std::string enum_name = find_enum_for_variant(id->name);
                    return {TypeKind::Enum, enum_name};
                }

                const Symbol* sym = current_scope_->lookup(id->name);
                if (!sym) {
                    throw DiagnosticError("use of undeclared identifier '" + id->name + "'", 0, 0);
                }
                return type_from_name(sym->type);
            }
            const Symbol* sym = current_scope_->lookup(id->name);
            if (!sym) {
                throw DiagnosticError("use of undeclared identifier '" + id->name + "'", 0, 0);
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
                        throw DiagnosticError("no variant '" + rhs_id->name + "' in enum '" +
                                                  lhs_id->name + "'",
                                              0, 0);
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
                            throw DiagnosticError("no variant '" + rhs_id->name + "' in enum '" +
                                                      lhs_id->name + "'",
                                                  0, 0);
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
                    Type lhs = type_of(*bin->left);

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
                                if (p.first == field_name)
                                    return p.second;
                            }
                            return std::nullopt;
                        };

                        // If lhs is a struct type, try to find the field
                        if (lhs.kind == TypeKind::Struct) {
                            if (auto ftype = lookup_field(lhs.name)) {
                                return type_from_name(*ftype);
                            }
                        }

                        // If lhs is an identifier referring to a variable, try to lookup its
                        // declared type
                        if (auto lhs_id =
                                dynamic_cast<const ast::IdentifierExpr*>(bin->left.get())) {
                            if (const Symbol* sym = current_scope_->lookup(lhs_id->name)) {
                                if (auto ftype = lookup_field(sym->type)) {
                                    return type_from_name(*ftype);
                                }
                            }
                        }

                        // Could also handle more complex receivers (calls returning structs, etc.)
                        return unknown();
                    }

                    return unknown();
                }
                // Handle dot access (field access / method call receiver)
                if (bin->op == TokenKind::Dot) {
                    // Try to resolve the type of the left-hand side
                    Type lhs = type_of(*bin->left);

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
                                if (p.first == field_name)
                                    return p.second;
                            }
                            return std::nullopt;
                        };

                        // If lhs is a struct type, try to find the field
                        if (lhs.kind == TypeKind::Struct) {
                            if (auto ftype = lookup_field(lhs.name)) {
                                return type_from_name(*ftype);
                            }
                        }

                        // If lhs is an identifier referring to a variable, try to lookup its
                        // declared type
                        if (auto lhs_id =
                                dynamic_cast<const ast::IdentifierExpr*>(bin->left.get())) {
                            if (const Symbol* sym = current_scope_->lookup(lhs_id->name)) {
                                if (auto ftype = lookup_field(sym->type)) {
                                    return type_from_name(*ftype);
                                }
                            }
                        }

                        // Could also handle more complex receivers (calls returning structs, etc.)
                        return unknown();
                    }

                    return unknown();
                }

                // Handle index access
                if (bin->op == TokenKind::LBracket) {
                    type_of(*bin->left);
                    type_of(*bin->right);
                    return unknown(); // Would need element type info
                }
                // Handle index access
                if (bin->op == TokenKind::LBracket) {
                    type_of(*bin->left);
                    type_of(*bin->right);
                    return unknown(); // Would need element type info
                }

                Type lhs = type_of(*bin->left);
                Type rhs = type_of(*bin->right);
                Type lhs = type_of(*bin->left);
                Type rhs = type_of(*bin->right);

                // Range operator
                if (bin->op == TokenKind::DotDot) {
                    return {TypeKind::Unknown, "Range"};
                }
                // Range operator
                if (bin->op == TokenKind::DotDot) {
                    return {TypeKind::Unknown, "Range"};
                }

                // Arithmetic
                if (bin->op == TokenKind::Plus || bin->op == TokenKind::Minus ||
                    bin->op == TokenKind::Star || bin->op == TokenKind::Slash ||
                    bin->op == TokenKind::Percent) {
                    if (lhs.kind != rhs.kind) {
                        throw DiagnosticError("type mismatch in binary expression", 0, 0);
                    }
                    // Arithmetic
                    if (bin->op == TokenKind::Plus || bin->op == TokenKind::Minus ||
                        bin->op == TokenKind::Star || bin->op == TokenKind::Slash ||
                        bin->op == TokenKind::Percent) {
                        if (lhs.kind != rhs.kind) {
                            throw DiagnosticError("type mismatch in binary expression", 0, 0);
                        }

                        if (lhs.kind != TypeKind::Int && lhs.kind != TypeKind::Float) {
                            throw DiagnosticError("invalid operands for arithmetic operator", 0, 0);
                        }
                        if (lhs.kind != TypeKind::Int && lhs.kind != TypeKind::Float) {
                            throw DiagnosticError("invalid operands for arithmetic operator", 0, 0);
                        }

                        return lhs;
                    }
                    return lhs;
                }

                // Bitwise operators
                if (bin->op == TokenKind::Amp || bin->op == TokenKind::Pipe ||
                    bin->op == TokenKind::Caret || bin->op == TokenKind::ShiftLeft ||
                    bin->op == TokenKind::ShiftRight) {
                    if (lhs.kind != TypeKind::Int) {
                        throw DiagnosticError("invalid operands for bitwise operator", 0, 0);
                    }
                    return lhs;
                }
                // Bitwise operators
                if (bin->op == TokenKind::Amp || bin->op == TokenKind::Pipe ||
                    bin->op == TokenKind::Caret || bin->op == TokenKind::ShiftLeft ||
                    bin->op == TokenKind::ShiftRight) {
                    if (lhs.kind != TypeKind::Int) {
                        throw DiagnosticError("invalid operands for bitwise operator", 0, 0);
                    }
                    return lhs;
                }

                // Comparisons
                if (bin->op == TokenKind::EqualEqual || bin->op == TokenKind::BangEqual ||
                    bin->op == TokenKind::Less || bin->op == TokenKind::LessEqual ||
                    bin->op == TokenKind::Greater || bin->op == TokenKind::GreaterEqual) {
                    if (lhs != rhs) {
                        throw DiagnosticError("comparison between incompatible types", 0, 0);
                    }
                    // Comparisons
                    if (bin->op == TokenKind::EqualEqual || bin->op == TokenKind::BangEqual ||
                        bin->op == TokenKind::Less || bin->op == TokenKind::LessEqual ||
                        bin->op == TokenKind::Greater || bin->op == TokenKind::GreaterEqual) {
                        if (lhs != rhs) {
                            throw DiagnosticError("comparison between incompatible types", 0, 0);
                        }

                        return {TypeKind::Bool, "Bool"};
                    }
                    return {TypeKind::Bool, "Bool"};
                }

                // Logical operators
                if (bin->op == TokenKind::AmpAmp || bin->op == TokenKind::PipePipe) {
                    if (lhs.kind != TypeKind::Bool || rhs.kind != TypeKind::Bool) {
                        throw DiagnosticError("logical operators require Bool operands", 0, 0);
                    }
                    return {TypeKind::Bool, "Bool"};
                }
            }
            // Logical operators
            if (bin->op == TokenKind::AmpAmp || bin->op == TokenKind::PipePipe) {
                if (lhs.kind != TypeKind::Bool || rhs.kind != TypeKind::Bool) {
                    throw DiagnosticError("logical operators require Bool operands", 0, 0);
                }
                return {TypeKind::Bool, "Bool"};
            }
        }

        if (auto un = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
            Type operand = type_of(*un->operand);
            if (auto un = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
                Type operand = type_of(*un->operand);

                if (un->op == TokenKind::Minus) {
                    if (operand.kind != TypeKind::Int && operand.kind != TypeKind::Float) {
                        throw DiagnosticError("invalid operand for unary '-'", 0, 0);
                    }
                    return operand;
                }
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
                if (un->op == TokenKind::Bang) {
                    if (operand.kind != TypeKind::Bool) {
                        throw DiagnosticError("invalid operand for '!'", 0, 0);
                    }
                    return {TypeKind::Bool, "Bool"};
                }

                if (un->op == TokenKind::Amp) {
                    // Use AST mutability flag
                    bool is_mut = un->is_mutable;
                    std::string ref_name = "&";
                    if (is_mut)
                        ref_name += "mut ";
                    ref_name += operand.name;
                    return Type{TypeKind::Ref, ref_name, is_mut};
                }

                if (un->op == TokenKind::Tilde) {
                    if (operand.kind != TypeKind::Int) {
                        throw DiagnosticError("invalid operand for '~'", 0, 0);
                    }
                    return operand;
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
            Type target = type_from_name(cast->target_type);
            if (target.kind == TypeKind::Unknown) {
                throw DiagnosticError("unknown type in cast: '" + cast->target_type + "'", 0, 0);
            }
            return target;
        }
        if (auto cast = dynamic_cast<const ast::CastExpr*>(&expr)) {
            (void)type_of(*cast->expr);
            Type target = type_from_name(cast->target_type);
            if (target.kind == TypeKind::Unknown) {
                throw DiagnosticError("unknown type in cast: '" + cast->target_type + "'", 0, 0);
            }
            return target;
        }

        if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
            const auto* callee = dynamic_cast<const ast::IdentifierExpr*>(call->callee.get());
            if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
                const auto* callee = dynamic_cast<const ast::IdentifierExpr*>(call->callee.get());

                if (!callee) {
                    // Could be a method call (binary expr with dot/coloncolon)
                    // For now, type-check arguments and return unknown
                    for (const auto& arg : call->arguments) {
                        type_of(*arg);
                    }
                    return unknown();
                }
                if (!callee) {
                    // Could be a method call (binary expr with dot/coloncolon)
                    // For now, type-check arguments and return unknown
                    for (const auto& arg : call->arguments) {
                        type_of(*arg);
                    }
                    return unknown();
                }

                // Built-in functions
                if (callee->name == "drop" || callee->name == "panic" || callee->name == "assert") {
                    for (const auto& arg : call->arguments) {
                        type_of(*arg);
                    }
                    if (callee->name == "panic")
                        return {TypeKind::Never, "Never"};
                    return {TypeKind::Void, "Void"};
                }
                // Built-in functions
                if (callee->name == "drop" || callee->name == "panic" || callee->name == "assert") {
                    for (const auto& arg : call->arguments) {
                        type_of(*arg);
                    }
                    if (callee->name == "panic")
                        return {TypeKind::Never, "Never"};
                    return {TypeKind::Void, "Void"};
                }

                const Symbol* sym = current_scope_->lookup(callee->name);
                if (!sym) {
                    throw DiagnosticError("use of undeclared function '" + callee->name + "'", 0,
                                          0);
                }
                const Symbol* sym = current_scope_->lookup(callee->name);
                if (!sym) {
                    throw DiagnosticError("use of undeclared function '" + callee->name + "'", 0,
                                          0);
                }

                if (sym->kind != SymbolKind::Function) {
                    throw DiagnosticError("'" + callee->name + "' is not a function", 0, 0);
                }
                if (sym->kind != SymbolKind::Function) {
                    throw DiagnosticError("'" + callee->name + "' is not a function", 0, 0);
                }

                if (call->arguments.size() != sym->param_types.size()) {
                    throw DiagnosticError("expected " + std::to_string(sym->param_types.size()) +
                                              " arguments, got " +
                                              std::to_string(call->arguments.size()),
                                          0, 0);
                }
                if (call->arguments.size() != sym->param_types.size()) {
                    throw DiagnosticError("expected " + std::to_string(sym->param_types.size()) +
                                              " arguments, got " +
                                              std::to_string(call->arguments.size()),
                                          0, 0);
                }

                for (size_t i = 0; i < call->arguments.size(); ++i) {
                    Type arg_type = type_of(*call->arguments[i]);
                    Type param_type = type_from_name(sym->param_types[i]);
                    for (size_t i = 0; i < call->arguments.size(); ++i) {
                        Type arg_type = type_of(*call->arguments[i]);
                        Type param_type = type_from_name(sym->param_types[i]);

                        if (arg_type != param_type && param_type.kind != TypeKind::Unknown &&
                            arg_type.kind != TypeKind::Unknown) {
                            throw DiagnosticError("argument " + std::to_string(i + 1) +
                                                      " has type '" + arg_type.name +
                                                      "', expected '" + param_type.name + "'",
                                                  0, 0);
                        }
                    }
                    if (arg_type != param_type && param_type.kind != TypeKind::Unknown &&
                        arg_type.kind != TypeKind::Unknown) {
                        throw DiagnosticError("argument " + std::to_string(i + 1) + " has type '" +
                                                  arg_type.name + "', expected '" +
                                                  param_type.name + "'",
                                              0, 0);
                    }
                }

                for (const auto& arg : call->arguments) {
                    resolve_expression(*arg);
                }
                for (const auto& arg : call->arguments) {
                    resolve_expression(*arg);
                }

                return type_from_name(sym->type);
            }
            return type_from_name(sym->type);
        }

        if (auto mv = dynamic_cast<const ast::MoveExpr*>(&expr)) {
            return type_of(*mv->operand);
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
            if (auto sl = dynamic_cast<const ast::StructLiteralExpr*>(&expr)) {
                // Get base struct name (strip generic params)
                std::string base = sl->struct_name;
                if (auto pos = base.find('<'); pos != std::string::npos) {
                    base = base.substr(0, pos);
                }

                if (struct_fields_.contains(base) || enum_variants_.contains(base)) {
                    return {TypeKind::Struct, sl->struct_name};
                }
                if (struct_fields_.contains(base) || enum_variants_.contains(base)) {
                    return {TypeKind::Struct, sl->struct_name};
                }

                // Check scope
                if (current_scope_->lookup(base)) {
                    return {TypeKind::Struct, sl->struct_name};
                }
                // Check scope
                if (current_scope_->lookup(base)) {
                    return {TypeKind::Struct, sl->struct_name};
                }

                return {TypeKind::Struct, sl->struct_name};
            }
            return {TypeKind::Struct, sl->struct_name};
        }

        if (auto ep = dynamic_cast<const ast::ErrorPropagationExpr*>(&expr)) {
            return type_of(*ep->operand);
        }
        if (auto ep = dynamic_cast<const ast::ErrorPropagationExpr*>(&expr)) {
            return type_of(*ep->operand);
        }

        if (dynamic_cast<const ast::AwaitExpr*>(&expr)) {
            return unknown();
        }
        if (dynamic_cast<const ast::AwaitExpr*>(&expr)) {
            return unknown();
        }

        if (dynamic_cast<const ast::SpawnExpr*>(&expr)) {
            return unknown();
        }
        if (dynamic_cast<const ast::SpawnExpr*>(&expr)) {
            return unknown();
        }

        if (dynamic_cast<const ast::RangeExpr*>(&expr)) {
            return unknown();
        }
        if (dynamic_cast<const ast::RangeExpr*>(&expr)) {
            return unknown();
        }

        return unknown();
    }
    return unknown();
}

/* =======================
   Scope management
   ======================= */
/* =======================
   Scope management
   ======================= */

void Resolver::enter_scope() {
    current_scope_ = new Scope(current_scope_);
}
void Resolver::enter_scope() {
    current_scope_ = new Scope(current_scope_);
}

void Resolver::exit_scope() {
    const Scope* old = current_scope_;
    current_scope_ = current_scope_->parent();
    delete old;
}
void Resolver::exit_scope() {
    const Scope* old = current_scope_;
    current_scope_ = current_scope_->parent();
    delete old;
}

/* =======================
   Entry point
   ======================= */
/* =======================
   Entry point
   ======================= */

void Resolver::resolve(const ast::Module& module) {
    enter_scope(); // global scope
    void Resolver::resolve(const ast::Module& module) {
        enter_scope(); // global scope

        // Declare built-in functions/modules
        current_scope_->declare({"io", SymbolKind::Variable, false, true, "Module", {}});
        current_scope_->declare({"std", SymbolKind::Variable, false, true, "Module", {}});
        current_scope_->declare({"drop", SymbolKind::Function, false, true, "Void", {"Unknown"}});
        current_scope_->declare({"panic", SymbolKind::Function, false, true, "Never", {"String"}});
        current_scope_->declare({"assert", SymbolKind::Function, false, true, "Void", {"Bool"}});
        current_scope_->declare(
            {"range", SymbolKind::Function, false, true, "Range", {"Int32", "Int32"}});
        // Declare built-in functions/modules
        current_scope_->declare({"io", SymbolKind::Variable, false, true, "Module", {}});
        current_scope_->declare({"std", SymbolKind::Variable, false, true, "Module", {}});
        current_scope_->declare({"drop", SymbolKind::Function, false, true, "Void", {"Unknown"}});
        current_scope_->declare({"panic", SymbolKind::Function, false, true, "Never", {"String"}});
        current_scope_->declare({"assert", SymbolKind::Function, false, true, "Void", {"Bool"}});
        current_scope_->declare(
            {"range", SymbolKind::Function, false, true, "Range", {"Int32", "Int32"}});

        resolve_module(module);
        exit_scope();
    }
    resolve_module(module);
    exit_scope();
}

/* =======================
   Module
   ======================= */
/* =======================
   Module
   ======================= */

void Resolver::resolve_module(const ast::Module& module) {
    enter_scope();
    // Declare imports
    for (const auto& imp : module.imports) {
        std::string root_path = imp.module_path;
        const auto pos = root_path.find("::");
        if (pos != std::string::npos) {
            root_path = root_path.substr(0, pos);
        }
        current_scope_->declare({root_path, SymbolKind::Variable, false, true, "Module", {}});
    }
    void Resolver::resolve_module(const ast::Module& module) {
        enter_scope();
        // Declare imports
        for (const auto& imp : module.imports) {
            std::string root_path = imp.module_path;
            const auto pos = root_path.find("::");
            if (pos != std::string::npos) {
                root_path = root_path.substr(0, pos);
            }
            current_scope_->declare({root_path, SymbolKind::Variable, false, true, "Module", {}});
        }

        // Declare type aliases
        for (const auto& ta : module.type_aliases) {
            type_aliases_[ta.name] = ta.target_type;
            current_scope_->declare({ta.name, SymbolKind::Variable, false, true, "Type", {}});
        }
        // Declare type aliases
        for (const auto& ta : module.type_aliases) {
            type_aliases_[ta.name] = ta.target_type;
            current_scope_->declare({ta.name, SymbolKind::Variable, false, true, "Type", {}});
        }

        // Declare types (structs, enums, classes, traits)
        for (const auto& s : module.structs) {
            current_scope_->declare({s.name, SymbolKind::Variable, false, true, "Type", {}});
            // Store struct fields for type checking
            std::vector<std::pair<std::string, std::string>> fields;
            for (const auto& f : s.fields) {
                fields.push_back({f.name, f.type});
            }
            struct_fields_[s.name] = std::move(fields);
        }
        // Declare types (structs, enums, classes, traits)
        for (const auto& s : module.structs) {
            current_scope_->declare({s.name, SymbolKind::Variable, false, true, "Type", {}});
            // Store struct fields for type checking
            std::vector<std::pair<std::string, std::string>> fields;
            for (const auto& f : s.fields) {
                fields.push_back({f.name, f.type});
            }
            struct_fields_[s.name] = std::move(fields);
        }

        for (const auto& c : module.classes) {
            current_scope_->declare({c.name, SymbolKind::Variable, false, true, "Type", {}});
            std::vector<std::pair<std::string, std::string>> fields;
            for (const auto& f : c.fields) {
                fields.push_back({f.name, f.type});
            }
            struct_fields_[c.name] = std::move(fields);
        }
        for (const auto& c : module.classes) {
            current_scope_->declare({c.name, SymbolKind::Variable, false, true, "Type", {}});
            std::vector<std::pair<std::string, std::string>> fields;
            for (const auto& f : c.fields) {
                fields.push_back({f.name, f.type});
            }
            struct_fields_[c.name] = std::move(fields);
        }

        for (const auto& e : module.enums) {
            current_scope_->declare({e.name, SymbolKind::Variable, false, true, "Type", {}});
            std::vector<std::string> vars;
            vars.reserve(e.variants.size());
            for (const auto& [name, types] : e.variants) {
                vars.push_back(name);
            }
            enum_variants_[e.name] = std::move(vars);
        }
        exit_scope();
        for (const auto& e : module.enums) {
            current_scope_->declare({e.name, SymbolKind::Variable, false, true, "Type", {}});
            std::vector<std::string> vars;
            vars.reserve(e.variants.size());
            for (const auto& [name, types] : e.variants) {
                vars.push_back(name);
            }
            enum_variants_[e.name] = std::move(vars);
        }
        exit_scope();

        for (const auto& t : module.traits) {
            current_scope_->declare({t.name, SymbolKind::Variable, false, true, "Type", {}});
        }
        for (const auto& t : module.traits) {
            current_scope_->declare({t.name, SymbolKind::Variable, false, true, "Type", {}});
        }

        // Declare functions first (forward visibility)
        for (const auto& fn : module.functions) {
            std::vector<std::string> params;
            params.reserve(fn.params.size());
            for (const auto& p : fn.params)
                params.push_back(p.type);
            // Declare functions first (forward visibility)
            for (const auto& fn : module.functions) {
                std::vector<std::string> params;
                params.reserve(fn.params.size());
                for (const auto& p : fn.params)
                    params.push_back(p.type);

                Symbol sym;
                sym.name = fn.name;
                sym.kind = SymbolKind::Function;
                sym.type = fn.return_type;
                sym.param_types = std::move(params);
                Symbol sym;
                sym.name = fn.name;
                sym.kind = SymbolKind::Function;
                sym.type = fn.return_type;
                sym.param_types = std::move(params);

                if (!current_scope_->declare(sym)) {
                    throw DiagnosticError("duplicate function '" + fn.name + "'", 0, 0);
                }
            }
            if (!current_scope_->declare(sym)) {
                throw DiagnosticError("duplicate function '" + fn.name + "'", 0, 0);
            }
        }

        // Declare impl methods (forward visibility)
        for (const auto& impl : module.impls) {
            for (const auto& method : impl.methods) {
                std::string qualified_name = impl.target_name + "::" + method.name;
                std::vector<std::string> params;
                for (const auto& p : method.params) {
                    if (p.name != "self") {
                        params.push_back(p.type);
                    }
                }
                current_scope_->declare({qualified_name, SymbolKind::Function, false, true,
                                         method.return_type, std::move(params)});
            }
        }
        // Declare impl methods (forward visibility)
        for (const auto& impl : module.impls) {
            for (const auto& method : impl.methods) {
                std::string qualified_name = impl.target_name + "::" + method.name;
                std::vector<std::string> params;
                for (const auto& p : method.params) {
                    if (p.name != "self") {
                        params.push_back(p.type);
                    }
                }
                current_scope_->declare({qualified_name, SymbolKind::Function, false, true,
                                         method.return_type, std::move(params)});
            }
        }

        // Resolve function bodies
        for (const auto& fn : module.functions) {
            resolve_function(fn);
        }
        // Resolve function bodies
        for (const auto& fn : module.functions) {
            resolve_function(fn);
        }

        // Resolve impl blocks
        for (const auto& impl : module.impls) {
            for (const auto& method : impl.methods) {
                resolve_function(method);
            }
        }
    }
    // Resolve impl blocks
    for (const auto& impl : module.impls) {
        for (const auto& method : impl.methods) {
            resolve_function(method);
        }
    }
}

/* =======================
   Function
   ======================= */
/* =======================
   Function
   ======================= */

void Resolver::resolve_function(const ast::FunctionDecl& fn) {
    enter_scope(); // function scope
    void Resolver::resolve_function(const ast::FunctionDecl& fn) {
        enter_scope(); // function scope

        current_function_return_type_ = type_from_name(fn.return_type);
        in_loop_ = false;
        current_function_return_type_ = type_from_name(fn.return_type);
        in_loop_ = false;

        // Declare parameters
        for (const auto& param : fn.params) {
            if (!current_scope_->declare(
                    {param.name, SymbolKind::Variable, false, false, param.type, {}})) {
                throw DiagnosticError("duplicate parameter '" + param.name + "'", 0, 0);
            }
        }
        // Declare parameters
        for (const auto& param : fn.params) {
            if (!current_scope_->declare(
                    {param.name, SymbolKind::Variable, false, false, param.type, {}})) {
                throw DiagnosticError("duplicate parameter '" + param.name + "'", 0, 0);
            }
        }

        // Resolve body and collect return info
        bool body_returns = resolve_block(fn.body);
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
        // Enforce return correctness
        if (current_function_return_type_.kind != TypeKind::Void &&
            current_function_return_type_.kind != TypeKind::Never &&
            current_function_return_type_.kind != TypeKind::Unknown && !body_returns) {
            throw DiagnosticError("missing return in function returning '" +
                                      current_function_return_type_.name + "'",
                                  0, 0);
        }

        exit_scope();
    }
    exit_scope();
}

/* =======================
   Block
   ======================= */
/* =======================
   Block
   ======================= */

bool Resolver::resolve_block(const ast::Block& block) {
    enter_scope(); // block scope
    bool Resolver::resolve_block(const ast::Block& block) {
        enter_scope(); // block scope

        bool always_returns = false;
        bool always_returns = false;

        for (const auto& stmt : block.statements) {
            if (always_returns) {
                throw DiagnosticError("unreachable code after return", 0, 0);
            }
            for (const auto& stmt : block.statements) {
                if (always_returns) {
                    throw DiagnosticError("unreachable code after return", 0, 0);
                }

                if (resolve_statement(*stmt)) {
                    always_returns = true;
                }
            }
            if (resolve_statement(*stmt)) {
                always_returns = true;
            }
        }

        exit_scope();
        return always_returns;
    }
    exit_scope();
    return always_returns;
}

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
    // Generic container: Box<T>, Vec<T>, etc.
    if (auto open = type_name.find('<'); open != std::string::npos) {
        auto close = type_name.find('>');
        if (close != std::string::npos) {
            std::string inner = type_name.substr(open + 1, close - open - 1);
            if (inner.size() == 1 && std::isupper(inner[0]))
                return true;
        }
    }
    return false;
}
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
    // Generic container: Box<T>, Vec<T>, etc.
    if (auto open = type_name.find('<'); open != std::string::npos) {
        auto close = type_name.find('>');
        if (close != std::string::npos) {
            std::string inner = type_name.substr(open + 1, close - open - 1);
            if (inner.size() == 1 && std::isupper(inner[0]))
                return true;
        }
    }
    return false;
}

bool Resolver::resolve_statement(const ast::Stmt& stmt) {
    // return
    if (const auto* ret = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
        if (ret->expression) {
            Type returned = type_of(*ret->expression);
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
    bool Resolver::resolve_statement(const ast::Stmt& stmt) {
        // return
        if (const auto* ret = dynamic_cast<const ast::ReturnStmt*>(&stmt)) {
            if (ret->expression) {
                Type returned = type_of(*ret->expression);
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
            Type init_type = type_of(*let_stmt->initializer);
            // let / const
            if (const auto* let_stmt = dynamic_cast<const ast::LetStmt*>(&stmt)) {
                // 1. Compute initializer type
                Type init_type = type_of(*let_stmt->initializer);

                // 2. Declared type (must be explicit)
                Type declared_type = type_from_name(let_stmt->type_name);
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
                // 2. Declared type (must be explicit)
                Type declared_type = type_from_name(let_stmt->type_name);
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

                // 3. Enforce compatibility (but allow unknown types to pass)
                if (init_type != declared_type && init_type.kind != TypeKind::Unknown &&
                    declared_type.kind != TypeKind::Unknown) {
                    throw DiagnosticError("cannot initialize variable '" + let_stmt->name +
                                              "' of type '" + declared_type.name +
                                              "' with value of type '" + init_type.name + "'",
                                          0, 0);
                }
                // 3. Enforce compatibility (but allow unknown types to pass)
                if (init_type != declared_type && init_type.kind != TypeKind::Unknown &&
                    declared_type.kind != TypeKind::Unknown) {
                    throw DiagnosticError("cannot initialize variable '" + let_stmt->name +
                                              "' of type '" + declared_type.name +
                                              "' with value of type '" + init_type.name + "'",
                                          0, 0);
                }

                // 4. Declare symbol
                if (!current_scope_->declare({let_stmt->name,
                                              SymbolKind::Variable,
                                              let_stmt->is_mutable,
                                              let_stmt->is_const,
                                              let_stmt->type_name,
                                              {}})) {
                    throw DiagnosticError("duplicate variable '" + let_stmt->name + "'", 0, 0);
                }
                // 4. Declare symbol
                if (!current_scope_->declare({let_stmt->name,
                                              SymbolKind::Variable,
                                              let_stmt->is_mutable,
                                              let_stmt->is_const,
                                              let_stmt->type_name,
                                              {}})) {
                    throw DiagnosticError("duplicate variable '" + let_stmt->name + "'", 0, 0);
                }

                return false;
            }
            return false;
        }

        // assignment
        if (const auto* asg = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
            const auto* id = dynamic_cast<const ast::IdentifierExpr*>(asg->target.get());
            // assignment
            if (const auto* asg = dynamic_cast<const ast::AssignStmt*>(&stmt)) {
                const auto* id = dynamic_cast<const ast::IdentifierExpr*>(asg->target.get());

                if (id) {
                    const Symbol* sym = current_scope_->lookup(id->name);
                    if (!sym) {
                        throw DiagnosticError(
                            "assignment to undeclared variable '" + id->name + "'", 0, 0);
                    }
                    if (id) {
                        const Symbol* sym = current_scope_->lookup(id->name);
                        if (!sym) {
                            throw DiagnosticError(
                                "assignment to undeclared variable '" + id->name + "'", 0, 0);
                        }

                        if (sym->is_const) {
                            throw DiagnosticError("cannot assign to constant '" + id->name + "'", 0,
                                                  0);
                        }
                        if (sym->is_const) {
                            throw DiagnosticError("cannot assign to constant '" + id->name + "'", 0,
                                                  0);
                        }

                        if (!sym->is_mutable) {
                            throw DiagnosticError(
                                "cannot assign to immutable variable '" + id->name + "'", 0, 0);
                        }
                        if (!sym->is_mutable) {
                            throw DiagnosticError(
                                "cannot assign to immutable variable '" + id->name + "'", 0, 0);
                        }

                        const Type lhs = type_from_name(sym->type);
                        const Type lhs = type_from_name(sym->type);

                        if (asg->op == TokenKind::Assign) {
                            if (const Type rhs = type_of(*asg->value);
                                lhs != rhs && lhs.kind != TypeKind::Unknown &&
                                rhs.kind != TypeKind::Unknown) {
                                throw DiagnosticError("cannot assign value of type '" + rhs.name +
                                                          "' to variable of type '" + lhs.name +
                                                          "'",
                                                      0, 0);
                            }
                        } else {
                            // Compound assignment — type-check the RHS
                            type_of(*asg->value);
                        }
                    } else {
                        // Complex target (field access, index access, etc.) — just resolve
                        resolve_expression(*asg->target);
                    }
                    if (asg->op == TokenKind::Assign) {
                        if (const Type rhs = type_of(*asg->value); lhs != rhs &&
                                                                   lhs.kind != TypeKind::Unknown &&
                                                                   rhs.kind != TypeKind::Unknown) {
                            throw DiagnosticError("cannot assign value of type '" + rhs.name +
                                                      "' to variable of type '" + lhs.name + "'",
                                                  0, 0);
                        }
                    } else {
                        // Compound assignment — type-check the RHS
                        type_of(*asg->value);
                    }
                } else {
                    // Complex target (field access, index access, etc.) — just resolve
                    resolve_expression(*asg->target);
                }

                resolve_expression(*asg->value);
                return false;
            }
            resolve_expression(*asg->value);
            return false;
        }

        // block statement
        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
            return resolve_block(block->block);
        }
        // block statement
        if (const auto* block = dynamic_cast<const ast::BlockStmt*>(&stmt)) {
            return resolve_block(block->block);
        }

        // if statement
        if (const auto* ifs = dynamic_cast<const ast::IfStmt*>(&stmt)) {
            resolve_expression(*ifs->condition);
            // if statement
            if (const auto* ifs = dynamic_cast<const ast::IfStmt*>(&stmt)) {
                resolve_expression(*ifs->condition);

                bool then_returns = resolve_statement(*ifs->then_branch);
                bool else_returns = false;
                bool then_returns = resolve_statement(*ifs->then_branch);
                bool else_returns = false;

                if (ifs->else_branch) {
                    else_returns = resolve_statement(*ifs->else_branch);
                }
                if (ifs->else_branch) {
                    else_returns = resolve_statement(*ifs->else_branch);
                }

                return then_returns && else_returns;
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
            // for loop
            if (const auto* fl = dynamic_cast<const ast::ForStmt*>(&stmt)) {
                resolve_expression(*fl->iterable);

                enter_scope();
                enter_scope();

                // Declare loop variable
                std::string var_type = fl->var_type.empty() ? "Unknown" : fl->var_type;
                current_scope_->declare(
                    {fl->variable, SymbolKind::Variable, false, false, var_type, {}});
                // Declare loop variable
                std::string var_type = fl->var_type.empty() ? "Unknown" : fl->var_type;
                current_scope_->declare(
                    {fl->variable, SymbolKind::Variable, false, false, var_type, {}});

                bool prev_in_loop = in_loop_;
                in_loop_ = true;
                resolve_statement(*fl->body);
                in_loop_ = prev_in_loop;
                bool prev_in_loop = in_loop_;
                in_loop_ = true;
                resolve_statement(*fl->body);
                in_loop_ = prev_in_loop;

                exit_scope();
                return false;
            }
            exit_scope();
            return false;
        }

        // loop
        if (const auto* lp = dynamic_cast<const ast::LoopStmt*>(&stmt)) {
            bool prev_in_loop = in_loop_;
            in_loop_ = true;
            resolve_statement(*lp->body);
            in_loop_ = prev_in_loop;
            return false; // Conservative — could return if it never breaks
        }
        // loop
        if (const auto* lp = dynamic_cast<const ast::LoopStmt*>(&stmt)) {
            bool prev_in_loop = in_loop_;
            in_loop_ = true;
            resolve_statement(*lp->body);
            in_loop_ = prev_in_loop;
            return false; // Conservative — could return if it never breaks
        }

        // break
        if (dynamic_cast<const ast::BreakStmt*>(&stmt)) {
            if (!in_loop_) {
                throw DiagnosticError("'break' used outside of loop", 0, 0);
            }
            return false;
        }
        // break
        if (dynamic_cast<const ast::BreakStmt*>(&stmt)) {
            if (!in_loop_) {
                throw DiagnosticError("'break' used outside of loop", 0, 0);
            }
            return false;
        }

        // continue
        if (dynamic_cast<const ast::ContinueStmt*>(&stmt)) {
            if (!in_loop_) {
                throw DiagnosticError("'continue' used outside of loop", 0, 0);
            }
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
            return false;
        }
        // expression statement
        if (const auto* es = dynamic_cast<const ast::ExprStmt*>(&stmt)) {
            resolve_expression(*es->expression);
            return false;
        }

        // match statement
        if (const auto* ms = dynamic_cast<const ast::MatchStmt*>(&stmt)) {
            Type subject_type = type_of(*ms->expression);
            // match statement
            if (const auto* ms = dynamic_cast<const ast::MatchStmt*>(&stmt)) {
                Type subject_type = type_of(*ms->expression);

                if (subject_type.kind == TypeKind::Enum &&
                    !enum_variants_.contains(subject_type.name)) {
                    throw DiagnosticError("unknown enum type '" + subject_type.name + "' in match",
                                          0, 0);
                }
                if (subject_type.kind == TypeKind::Enum &&
                    !enum_variants_.contains(subject_type.name)) {
                    throw DiagnosticError("unknown enum type '" + subject_type.name + "' in match",
                                          0, 0);
                }

                bool has_wildcard = false;
                bool all_arms_return = true;
                bool has_wildcard = false;
                bool all_arms_return = true;

                std::unordered_set<std::string> seen_enum_variants;
                bool seen_true = false;
                bool seen_false = false;
                std::unordered_set<std::string> seen_enum_variants;
                bool seen_true = false;
                bool seen_false = false;

                resolve_expression(*ms->expression);
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
                        // Check if this identifier is an enum variant (bare variant name)
                        if (is_enum_variant(ip->name)) {
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
                    for (const auto& arm : ms->arms) {
                        // Track patterns
                        if (dynamic_cast<const ast::WildcardPattern*>(arm.pattern.get())) {
                            has_wildcard = true;
                        } else if (const auto* vp = dynamic_cast<const ast::VariantPattern*>(
                                       arm.pattern.get())) {
                            seen_enum_variants.insert(vp->variant_name);
                        } else if (const auto* ip = dynamic_cast<const ast::IdentifierPattern*>(
                                       arm.pattern.get())) {
                            // Check if this identifier is an enum variant (bare variant name)
                            if (is_enum_variant(ip->name)) {
                                seen_enum_variants.insert(ip->name);
                            }
                        } else if (const auto* lp = dynamic_cast<const ast::LiteralPattern*>(
                                       arm.pattern.get())) {
                            if (const auto* b =
                                    dynamic_cast<const ast::BoolExpr*>(lp->literal.get())) {
                                if (b->value)
                                    seen_true = true;
                                else
                                    seen_false = true;
                            }
                        }

                        // Resolve arm body in its own scope
                        enter_scope();
                        resolve_pattern(*arm.pattern);
                        // Resolve arm body in its own scope
                        enter_scope();
                        resolve_pattern(*arm.pattern);

                        // Resolve guard if present
                        if (arm.guard) {
                            resolve_expression(*arm.guard);
                        }
                        // Resolve guard if present
                        if (arm.guard) {
                            resolve_expression(*arm.guard);
                        }

                        bool arm_returns = resolve_statement(*arm.body);
                        exit_scope();
                        bool arm_returns = resolve_statement(*arm.body);
                        exit_scope();

                        all_arms_return = all_arms_return && arm_returns;
                    }
                    all_arms_return = all_arms_return && arm_returns;
                }

                // Exhaustiveness rules
                if (!has_wildcard) {
                    if (subject_type.kind == TypeKind::Bool) {
                        if (!(seen_true && seen_false)) {
                            throw DiagnosticError("non-exhaustive match on Bool", 0, 0);
                        }
                    } else if (subject_type.kind == TypeKind::Enum) {
                        const auto& variants = enum_variants_.at(subject_type.name);
                        // Exhaustiveness rules
                        if (!has_wildcard) {
                            if (subject_type.kind == TypeKind::Bool) {
                                if (!(seen_true && seen_false)) {
                                    throw DiagnosticError("non-exhaustive match on Bool", 0, 0);
                                }
                            } else if (subject_type.kind == TypeKind::Enum) {
                                const auto& variants = enum_variants_.at(subject_type.name);

                                size_t covered = 0;
                                for (const auto& v : variants) {
                                    std::string qualified;
                                    qualified.reserve(subject_type.name.size() + v.size() + 2);
                                    qualified.append(subject_type.name).append("::").append(v);
                                    size_t covered = 0;
                                    for (const auto& v : variants) {
                                        std::string qualified;
                                        qualified.reserve(subject_type.name.size() + v.size() + 2);
                                        qualified.append(subject_type.name).append("::").append(v);

                                        if (seen_enum_variants.contains(v) ||
                                            seen_enum_variants.contains(qualified)) {
                                            covered++;
                                        }
                                    }
                                    if (seen_enum_variants.contains(v) ||
                                        seen_enum_variants.contains(qualified)) {
                                        covered++;
                                    }
                                }

                                if (covered != variants.size()) {
                                    throw DiagnosticError("non-exhaustive match on enum '" +
                                                              subject_type.name + "'",
                                                          0, 0);
                                }
                            } else {
                                throw DiagnosticError("non-exhaustive match (add '_' wildcard arm)",
                                                      0, 0);
                            }
                        }
                        if (covered != variants.size()) {
                            throw DiagnosticError(
                                "non-exhaustive match on enum '" + subject_type.name + "'", 0, 0);
                        }
                    } else {
                        throw DiagnosticError("non-exhaustive match (add '_' wildcard arm)", 0, 0);
                    }
                }

                return all_arms_return;
            }
            return all_arms_return;
        }

        throw DiagnosticError("unsupported statement", 0, 0);
    }
    throw DiagnosticError("unsupported statement", 0, 0);
}

/* =======================
   Expressions
   ======================= */
/* =======================
   Expressions
   ======================= */

void Resolver::resolve_expression(const ast::Expr& expr) {
    if (const auto* id = dynamic_cast<const ast::IdentifierExpr*>(&expr)) {
        // Enum variants are NOT variables and do not require lookup
        if (is_enum_variant(id->name)) {
            return;
        }
        void Resolver::resolve_expression(const ast::Expr& expr) {
            if (const auto* id = dynamic_cast<const ast::IdentifierExpr*>(&expr)) {
                // Enum variants are NOT variables and do not require lookup
                if (is_enum_variant(id->name)) {
                    return;
                }

                // Built-in identifiers
                if (id->name == "self" || id->name == "Self" || id->name == "drop" ||
                    id->name == "panic" || id->name == "assert") {
                    return;
                }
                // Built-in identifiers
                if (id->name == "self" || id->name == "Self" || id->name == "drop" ||
                    id->name == "panic" || id->name == "assert") {
                    return;
                }

                if (!current_scope_->lookup(id->name)) {
                    throw DiagnosticError("use of undeclared identifier '" + id->name + "'", 0, 0);
                }
                return;
            }
            if (!current_scope_->lookup(id->name)) {
                throw DiagnosticError("use of undeclared identifier '" + id->name + "'", 0, 0);
            }
            return;
        }

        if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
            (void)type_of(expr);
            for (const auto& arg : call->arguments) {
                resolve_expression(*arg);
            }
            return;
        }
        if (const auto* call = dynamic_cast<const ast::CallExpr*>(&expr)) {
            (void)type_of(expr);
            for (const auto& arg : call->arguments) {
                resolve_expression(*arg);
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
        if (const auto* un = dynamic_cast<const ast::UnaryExpr*>(&expr)) {
            resolve_expression(*un->operand);
            return;
        }

        if (const auto* mv = dynamic_cast<const ast::MoveExpr*>(&expr)) {
            resolve_expression(*mv->operand);
            return;
        }
        if (const auto* mv = dynamic_cast<const ast::MoveExpr*>(&expr)) {
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
            }
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
            }
            return;
        }

        if (const auto* cast = dynamic_cast<const ast::CastExpr*>(&expr)) {
            resolve_expression(*cast->expr);
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
        if (const auto* ep = dynamic_cast<const ast::ErrorPropagationExpr*>(&expr)) {
            resolve_expression(*ep->operand);
            return;
        }

        if (const auto* aw = dynamic_cast<const ast::AwaitExpr*>(&expr)) {
            resolve_expression(*aw->operand);
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
        if (const auto* rng = dynamic_cast<const ast::RangeExpr*>(&expr)) {
            if (rng->start)
                resolve_expression(*rng->start);
            if (rng->end)
                resolve_expression(*rng->end);
            return;
        }

        // literals (NumberExpr, StringExpr, BoolExpr, CharExpr) are fine
    }
    // literals (NumberExpr, StringExpr, BoolExpr, CharExpr) are fine
}

void Resolver::resolve_pattern(const ast::Pattern& pattern) {
    if (const auto* id_pat = dynamic_cast<const ast::IdentifierPattern*>(&pattern)) {
        // Is this identifier an enum variant?
        if (is_enum_variant(id_pat->name)) {
            return;
        }
        void Resolver::resolve_pattern(const ast::Pattern& pattern) {
            if (const auto* id_pat = dynamic_cast<const ast::IdentifierPattern*>(&pattern)) {
                // Is this identifier an enum variant?
                if (is_enum_variant(id_pat->name)) {
                    return;
                }

                // Otherwise: normal variable binding
                if (!current_scope_->declare(
                        {id_pat->name, SymbolKind::Variable, false, true, "Unknown", {}})) {
                    throw DiagnosticError("duplicate variable '" + id_pat->name + "' in pattern", 0,
                                          0);
                }
                return;
            }
            // Otherwise: normal variable binding
            if (!current_scope_->declare(
                    {id_pat->name, SymbolKind::Variable, false, true, "Unknown", {}})) {
                throw DiagnosticError("duplicate variable '" + id_pat->name + "' in pattern", 0, 0);
            }
            return;
        }

        if (const auto* var_pat = dynamic_cast<const ast::VariantPattern*>(&pattern)) {
            // Parse qualified name
            std::string root = var_pat->variant_name;
            if (const auto pos = root.find("::"); pos != std::string::npos) {
                root = root.substr(0, pos);
            }
            if (!current_scope_->lookup(root) && !is_enum_variant(root)) {
                throw DiagnosticError(
                    "use of undeclared identifier '" + root + "' in variant pattern", 0, 0);
            }
            for (const auto& sub : var_pat->sub_patterns) {
                resolve_pattern(*sub);
            }
            return;
        }
        if (const auto* var_pat = dynamic_cast<const ast::VariantPattern*>(&pattern)) {
            // Parse qualified name
            std::string root = var_pat->variant_name;
            if (const auto pos = root.find("::"); pos != std::string::npos) {
                root = root.substr(0, pos);
            }
            if (!current_scope_->lookup(root) && !is_enum_variant(root)) {
                throw DiagnosticError(
                    "use of undeclared identifier '" + root + "' in variant pattern", 0, 0);
            }
            for (const auto& sub : var_pat->sub_patterns) {
                resolve_pattern(*sub);
            }
            return;
        }

        if (const auto* lit_pat = dynamic_cast<const ast::LiteralPattern*>(&pattern)) {
            resolve_expression(*lit_pat->literal);
            return;
        }
        if (const auto* lit_pat = dynamic_cast<const ast::LiteralPattern*>(&pattern)) {
            resolve_expression(*lit_pat->literal);
            return;
        }

        // WildcardPattern is fine
    }

    // Numeric helpers for type name analysis and promotion
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

    // WildcardPattern is fine
}

// Numeric helpers for type name analysis and promotion
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

} // namespace flux::semantic
