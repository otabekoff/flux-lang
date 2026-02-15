#include "monomorphizer.h"
#include "ast/ast.h"
#include "resolver.h"
#include "type.h"
#include <algorithm>
#include <iostream>
#include <sstream>

namespace flux::semantic {

Monomorphizer::Monomorphizer(const ::flux::semantic::Resolver& resolver) : resolver_(resolver) {}

::flux::ast::Module Monomorphizer::monomorphize(const ::flux::ast::Module& main_module) {
    ::flux::ast::Module assembly;
    assembly.name = main_module.name;

    // 1. Collect all non-generic functions from all modules
    for (const auto& [name, decl_ptr] : resolver_.function_decls()) {
        if (decl_ptr->type_params.empty()) {
            auto cloned_fn = decl_ptr->clone();
            ::flux::ast::FunctionDecl fn =
                std::move(*static_cast<::flux::ast::FunctionDecl*>(cloned_fn.get()));
            // Ensure non-namespaced functions in main module keep their names,
            // but others use their qualified names.
            if (name.find("::") != std::string::npos) {
                fn.name = name;
            }
            assembly.functions.push_back(std::move(fn));
        }
    }

    // 2. Instantiate all required specializations
    for (const auto& inst : resolver_.function_instantiations()) {
        std::string mangled = mangle_name(inst.name, inst.args);
        if (instantiated_functions_.find(mangled) != instantiated_functions_.end()) {
            continue;
        }

        try {
            auto specialized = instantiate_function(inst.name, inst.args);
            assembly.functions.push_back(std::move(specialized));
        } catch (const std::exception& e) {
            std::cerr << "Warning: Failed to instantiate " << inst.name << ": " << e.what() << "\n";
        }
    }

    std::unordered_map<std::string, ::flux::semantic::FluxType> empty_map;
    for (auto& fn : assembly.functions) {
        // Derive the module context from the function's qualified name
        std::string fn_module = assembly.name;
        if (auto pos = fn.name.rfind("::"); pos != std::string::npos) {
            fn_module = fn.name.substr(0, pos);
        }
        substitute_in_function(fn, empty_map, fn_module);
    }

    return assembly;
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
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping,
    const std::string& module_name) {
    fn.return_type = substitute_type_name(fn.return_type, mapping);
    for (auto& param : fn.params) {
        param.type = substitute_type_name(param.type, mapping);
    }
    substitute_in_block(fn.body, mapping, module_name);
}

void Monomorphizer::substitute_in_block(
    ::flux::ast::Block& block,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping,
    const std::string& module_name) {
    for (auto& stmt : block.statements) {
        if (stmt)
            substitute_in_stmt(stmt, mapping, module_name);
    }
}

void Monomorphizer::substitute_in_stmt(
    ::flux::ast::StmtPtr& stmt,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping,
    const std::string& module_name) {
    if (auto* rs = dynamic_cast<::flux::ast::ReturnStmt*>(stmt.get())) {
        if (rs->expression)
            substitute_in_expr(rs->expression, mapping, module_name);
    } else if (auto* ls = dynamic_cast<::flux::ast::LetStmt*>(stmt.get())) {
        ls->type_name = substitute_type_name(ls->type_name, mapping);
        if (ls->initializer)
            substitute_in_expr(ls->initializer, mapping, module_name);
    } else if (auto* as = dynamic_cast<::flux::ast::AssignStmt*>(stmt.get())) {
        if (as->target)
            substitute_in_expr(as->target, mapping, module_name);
        if (as->value)
            substitute_in_expr(as->value, mapping, module_name);
    } else if (auto* bs = dynamic_cast<::flux::ast::BlockStmt*>(stmt.get())) {
        substitute_in_block(bs->block, mapping, module_name);
    } else if (auto* is = dynamic_cast<::flux::ast::IfStmt*>(stmt.get())) {
        if (is->condition)
            substitute_in_expr(is->condition, mapping, module_name);
        if (is->then_branch)
            substitute_in_stmt(is->then_branch, mapping, module_name);
        if (is->else_branch)
            substitute_in_stmt(is->else_branch, mapping, module_name);
    } else if (auto* ws = dynamic_cast<::flux::ast::WhileStmt*>(stmt.get())) {
        if (ws->condition)
            substitute_in_expr(ws->condition, mapping, module_name);
        if (ws->body)
            substitute_in_stmt(ws->body, mapping, module_name);
    } else if (auto* es = dynamic_cast<::flux::ast::ExprStmt*>(stmt.get())) {
        if (es->expression)
            substitute_in_expr(es->expression, mapping, module_name);
    }
}

void Monomorphizer::substitute_in_expr(
    ::flux::ast::ExprPtr& expr,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping,
    const std::string& module_name) {
    if (auto* ident_node = dynamic_cast<::flux::ast::IdentifierExpr*>(expr.get())) {
        if (mapping.find(ident_node->name) != mapping.end()) {
            ident_node->name = mapping.at(ident_node->name).name;
        }
    } else if (auto* call = dynamic_cast<::flux::ast::CallExpr*>(expr.get())) {
        if (call->callee)
            substitute_in_expr(call->callee, mapping, module_name);
        for (auto& arg : call->arguments) {
            if (arg)
                substitute_in_expr(arg, mapping, module_name);
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

                // Qualify bare function names within namespaced modules
                // e.g., inside std::io::println, "puts" -> "std::io::puts"
                if (callee_node->name.find("::") == std::string::npos && !module_name.empty() &&
                    module_name.find("::") != std::string::npos) {
                    std::string qualified = module_name + "::" + callee_node->name;
                    const auto& decls = resolver_.function_decls();
                    if (decls.find(qualified) != decls.end()) {
                        callee_node->name = qualified;
                    }
                }
            }
        }
    } else if (auto* bin = dynamic_cast<::flux::ast::BinaryExpr*>(expr.get())) {
        if (bin->op == ::flux::TokenKind::ColonColon) {
            // Hierarchical name resolution: io::println -> std::io::println
            std::vector<std::string> parts;
            ::flux::ast::Expr* current = expr.get();
            bool valid_chain = true;
            while (auto* b = dynamic_cast<::flux::ast::BinaryExpr*>(current)) {
                if (b->op != ::flux::TokenKind::ColonColon) {
                    valid_chain = false;
                    break;
                }
                if (auto* rhs_id = dynamic_cast<::flux::ast::IdentifierExpr*>(b->right.get())) {
                    parts.insert(parts.begin(), rhs_id->name);
                } else {
                    valid_chain = false;
                    break;
                }
                current = b->left.get();
            }

            if (valid_chain) {
                if (auto* root_id = dynamic_cast<::flux::ast::IdentifierExpr*>(current)) {
                    parts.insert(parts.begin(), root_id->name);
                    std::string full_name;
                    for (size_t i = 0; i < parts.size(); ++i) {
                        if (i > 0)
                            full_name += "::";
                        full_name += parts[i];
                    }

                    // Resolve the name
                    std::string resolved = resolver_.resolve_name(full_name, module_name);

                    // Replace BinaryExpr with IdentifierExpr
                    auto new_id = std::make_unique<::flux::ast::IdentifierExpr>(resolved);
                    new_id->line = expr->line;
                    new_id->column = expr->column;
                    expr = std::move(new_id);
                    return; // Done with this branch
                }
            }
        }

        if (bin->left)
            substitute_in_expr(bin->left, mapping, module_name);
        if (bin->right)
            substitute_in_expr(bin->right, mapping, module_name);
    } else if (auto* un = dynamic_cast<::flux::ast::UnaryExpr*>(expr.get())) {
        if (un->operand)
            substitute_in_expr(un->operand, mapping, module_name);
    } else if (auto* sl = dynamic_cast<::flux::ast::StructLiteralExpr*>(expr.get())) {
        sl->struct_name = substitute_type_name(sl->struct_name, mapping);
        for (auto& field : sl->fields) {
            if (field.value)
                substitute_in_expr(field.value, mapping, module_name);
        }
    } else if (auto* ma = dynamic_cast<::flux::ast::MemberAccessExpr*>(expr.get())) {
        if (ma->object)
            substitute_in_expr(ma->object, mapping, module_name);
    }
}

void Monomorphizer::substitute_in_pattern(
    ::flux::ast::PatternPtr& pattern,
    const std::unordered_map<std::string, ::flux::semantic::FluxType>& mapping) {
    if (auto* vp = dynamic_cast<::flux::ast::VariantPattern*>(pattern.get())) {
        vp->variant_name = substitute_type_name(vp->variant_name, mapping);
        for (auto& sub : vp->sub_patterns) {
            if (sub)
                substitute_in_pattern(sub, mapping);
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
