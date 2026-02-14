#include "monomorphizer.h"
#include "ast/ast.h"
#include "resolver.h"
#include "type.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace flux::semantic {

Monomorphizer::Monomorphizer(const ::flux::semantic::Resolver& resolver) : resolver_(resolver) {}

::flux::ast::Module Monomorphizer::monomorphize(const ::flux::ast::Module& module) {
    auto cloned_module = module.clone();
    ::flux::ast::Module new_module =
        std::move(*static_cast<::flux::ast::Module*>(cloned_module.get()));

    new_module.functions.clear();

    for (const auto& fn : module.functions) {
        if (fn.type_params.empty()) {
            auto cloned_fn = fn.clone();
            new_module.functions.push_back(
                std::move(*static_cast<::flux::ast::FunctionDecl*>(cloned_fn.get())));
        }
    }

    for (const auto& inst : resolver_.function_instantiations()) {
        std::string mangled = mangle_name(inst.name, inst.args);
        if (instantiated_functions_.find(mangled) != instantiated_functions_.end()) {
            continue;
        }

        try {
            auto specialized = instantiate_function(inst.name, inst.args);
            new_module.functions.push_back(std::move(specialized));
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to instantiate " << inst.name << ": " << e.what() << "\n";
        }
    }

    std::unordered_map<std::string, ::flux::semantic::FluxType> empty_map;
    for (auto& fn : new_module.functions) {
        substitute_in_function(fn, empty_map);
    }

    return new_module;
}

std::string Monomorphizer::mangle_name(const std::string& name,
                                       const std::vector<::flux::semantic::FluxType>& type_args) {
    if (type_args.empty())
        return name;

    std::stringstream ss;
    ss << name << "__";
    for (size_t i = 0; i < type_args.size(); ++i) {
        ss << mangle_type(type_args[i]);
        if (i < type_args.size() - 1)
            ss << "_";
    }
    return ss.str();
}

std::string Monomorphizer::mangle_type(const ::flux::semantic::FluxType& type) {
    std::string n = type.name;
    if (n == "Int32")
        return "i32";
    if (n == "Float64")
        return "f64";
    if (n == "Bool")
        return "bool";
    if (n == "String")
        return "str";

    std::string mangled = n;
    for (char& ch : mangled) {
        if (ch == '<')
            ch = 'L';
        else if (ch == '>')
            ch = 'R';
        else if (ch == ',')
            ch = 'S';
        else if (ch == ' ')
            ch = '_';
    }

    std::string final_mangled;
    for (char ch : mangled) {
        if (ch == '&')
            final_mangled += "Ref";
        else
            final_mangled += ch;
    }
    mangled = final_mangled;

    for (const auto& arg : type.generic_args) {
        mangled += "_" + mangle_type(arg);
    }

    return mangled;
}

::flux::ast::FunctionDecl
Monomorphizer::instantiate_function(const std::string& original_name,
                                    const std::vector<::flux::semantic::FluxType>& type_args) {
    const auto& decls = resolver_.function_decls();
    if (decls.find(original_name) == decls.end()) {
        throw std::runtime_error("Function declaration not found: " + original_name);
    }

    const ::flux::ast::FunctionDecl* original_decl = decls.at(original_name);

    auto cloned = original_decl->clone();
    ::flux::ast::FunctionDecl specialized =
        std::move(*static_cast<::flux::ast::FunctionDecl*>(cloned.get()));

    specialized.name = mangle_name(original_name, type_args);
    specialized.type_params.clear();

    std::unordered_map<std::string, ::flux::semantic::FluxType> mapping;
    auto tp_it = resolver_.function_type_params().find(original_name);
    if (tp_it != resolver_.function_type_params().end()) {
        const auto& params = tp_it->second;
        std::vector<std::string> pure_params;
        for (const auto& p : params) {
            size_t colon = p.find(':');
            if (colon != std::string::npos) {
                pure_params.push_back(p.substr(0, colon));
            } else {
                pure_params.push_back(p);
            }
        }

        for (size_t i = 0; i < pure_params.size() && i < type_args.size(); ++i) {
            mapping[pure_params[i]] = type_args[i];
        }
    }

    substitute_in_function(specialized, mapping);
    instantiated_functions_.insert(specialized.name);

    return specialized;
}

void Monomorphizer::substitute_in_function(
    ::flux::ast::FunctionDecl& fn,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    fn.return_type = substitute_type_name(fn.return_type, mapping);
    for (auto& param : fn.params) {
        param.type = substitute_type_name(param.type, mapping);
    }
    substitute_in_block(fn.body, mapping);
}

void Monomorphizer::substitute_in_block(
    ::flux::ast::Block& block,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    for (const auto& stmt : block.statements) {
        if (stmt)
            substitute_in_stmt(*stmt, mapping);
    }
}

void Monomorphizer::substitute_in_stmt(
    ::flux::ast::Stmt& stmt,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    if (auto* rs = dynamic_cast<::flux::ast::ReturnStmt*>(&stmt)) {
        if (rs->expression)
            substitute_in_expr(*rs->expression, mapping);
    } else if (auto* ls = dynamic_cast<::flux::ast::LetStmt*>(&stmt)) {
        ls->type_name = substitute_type_name(ls->type_name, mapping);
        if (ls->initializer)
            substitute_in_expr(*ls->initializer, mapping);
    } else if (auto* as = dynamic_cast<::flux::ast::AssignStmt*>(&stmt)) {
        if (as->target)
            substitute_in_expr(*as->target, mapping);
        if (as->value)
            substitute_in_expr(*as->value, mapping);
    } else if (auto* bs = dynamic_cast<::flux::ast::BlockStmt*>(&stmt)) {
        substitute_in_block(bs->block, mapping);
    } else if (auto* is = dynamic_cast<::flux::ast::IfStmt*>(&stmt)) {
        if (is->condition)
            substitute_in_expr(*is->condition, mapping);
        if (is->then_branch)
            substitute_in_stmt(*is->then_branch, mapping);
        if (is->else_branch)
            substitute_in_stmt(*is->else_branch, mapping);
    } else if (auto* ws = dynamic_cast<::flux::ast::WhileStmt*>(&stmt)) {
        if (ws->condition)
            substitute_in_expr(*ws->condition, mapping);
        if (ws->body)
            substitute_in_stmt(*ws->body, mapping);
    } else if (auto* es = dynamic_cast<::flux::ast::ExprStmt*>(&stmt)) {
        if (es->expression)
            substitute_in_expr(*es->expression, mapping);
    }
}

void Monomorphizer::substitute_in_expr(
    ::flux::ast::Expr& expr,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    if (auto* ident_node = dynamic_cast<::flux::ast::IdentifierExpr*>(&expr)) {
        if (mapping.find(ident_node->name) != mapping.end()) {
            ident_node->name = mapping.at(ident_node->name).name;
        }
    } else if (auto* call = dynamic_cast<::flux::ast::CallExpr*>(&expr)) {
        if (call->callee)
            substitute_in_expr(*call->callee, mapping);
        for (auto& arg : call->arguments) {
            if (arg)
                substitute_in_expr(*arg, mapping);
        }

        if (call->callee) {
            if (auto* callee_node =
                    dynamic_cast<::flux::ast::IdentifierExpr*>(call->callee.get())) {
                if (callee_node->name.find('<') != std::string::npos) {
                    size_t open = callee_node->name.find('<');
                    std::string base = callee_node->name.substr(0, open);
                    std::string args_str =
                        callee_node->name.substr(open + 1, callee_node->name.size() - open - 2);

                    std::string substituted_args = args_str;
                    for (const auto& [gen, concrete] : mapping) {
                        size_t pos = 0;
                        while ((pos = substituted_args.find(gen, pos)) != std::string::npos) {
                            bool boundary_l =
                                (pos == 0 || !std::isalnum(substituted_args[pos - 1]));
                            bool boundary_r = (pos + gen.length() == substituted_args.length() ||
                                               !std::isalnum(substituted_args[pos + gen.length()]));
                            if (boundary_l && boundary_r) {
                                substituted_args.replace(pos, gen.length(), concrete.name);
                                pos += concrete.name.length();
                            } else {
                                pos += gen.length();
                            }
                        }
                    }

                    std::string mangled = base + "__" + substituted_args;
                    for (char& ch : mangled) {
                        if (ch == ',' || ch == ' ' || ch == '<' || ch == '>')
                            ch = '_';
                    }

                    while (mangled.find("___") != std::string::npos)
                        mangled.replace(mangled.find("___"), 3, "__");

                    callee_node->name = mangled;
                }
            }
        }
    } else if (auto* bin = dynamic_cast<::flux::ast::BinaryExpr*>(&expr)) {
        if (bin->left)
            substitute_in_expr(*bin->left, mapping);
        if (bin->right)
            substitute_in_expr(*bin->right, mapping);
    } else if (auto* un = dynamic_cast<::flux::ast::UnaryExpr*>(&expr)) {
        if (un->operand)
            substitute_in_expr(*un->operand, mapping);
    } else if (auto* sl = dynamic_cast<::flux::ast::StructLiteralExpr*>(&expr)) {
        sl->struct_name = substitute_type_name(sl->struct_name, mapping);
        for (auto& field : sl->fields) {
            if (field.value)
                substitute_in_expr(*field.value, mapping);
        }
    } else if (auto* ma = dynamic_cast<::flux::ast::MemberAccessExpr*>(&expr)) {
        if (ma->object)
            substitute_in_expr(*ma->object, mapping);
    }
}

void Monomorphizer::substitute_in_pattern(
    ::flux::ast::Pattern& pattern,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    if (auto* vp = dynamic_cast<::flux::ast::VariantPattern*>(&pattern)) {
        vp->variant_name = substitute_type_name(vp->variant_name, mapping);
        for (auto& sub : vp->sub_patterns) {
            if (sub)
                substitute_in_pattern(*sub, mapping);
        }
    }
}

std::string Monomorphizer::substitute_type_name(
    const std::string& name,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    if (mapping.find(name) != mapping.end()) {
        return mapping.at(name).name;
    }
    return name;
}

::flux::semantic::FluxType Monomorphizer::substitute_type(
    const ::flux::semantic::FluxType& type,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    if (type.kind == ::flux::semantic::TypeKind::Generic &&
        mapping.find(type.name) != mapping.end()) {
        return mapping.at(type.name);
    }
    ::flux::semantic::FluxType result = type;
    for (auto& arg : result.generic_args) {
        arg = substitute_type(arg, mapping);
    }
    return result;
}

} // namespace flux::semantic
