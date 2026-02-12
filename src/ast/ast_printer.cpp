#include "ast_printer.h"
#include <iostream>

namespace flux::ast {
/* =======================
   Helpers
   ======================= */

void ASTPrinter::indent() {
    for (int i = 0; i < indent_level_; ++i)
        std::cout << "  ";
}

/* =======================
   Entry point
   ======================= */

void ASTPrinter::print(const Module& module) {
    print_module(module);
}

/* =======================
   Module
   ======================= */

void ASTPrinter::print_module(const Module& module) {
    std::cout << "Module " << module.name << '\n';

    indent_level_++;
    for (const auto& imp : module.imports) {
        print_import(imp);
    }
    for (const auto& ta : module.type_aliases) {
        indent();
        std::cout << "TypeAlias " << ta.name << " = " << ta.target_type << '\n';
    }
    for (const auto& s : module.structs) {
        print_struct(s);
    }
    for (const auto& c : module.classes) {
        indent();
        std::cout << "Class " << c.name << '\n';
        indent_level_++;
        for (const auto& field : c.fields) {
            indent();
            std::cout << "Field " << field.name << " : " << field.type << '\n';
        }
        indent_level_--;
    }
    for (const auto& e : module.enums) {
        print_enum(e);
    }
    for (const auto& t : module.traits) {
        print_trait(t);
    }
    for (const auto& fn : module.functions) {
        print_function(fn);
    }
    for (const auto& i : module.impls) {
        print_impl(i);
    }
    indent_level_--;
}

/* =======================
   Import
   ======================= */

void ASTPrinter::print_import(const Import& imp) {
    indent();
    std::cout << "Import " << imp.module_path << '\n';
}

/* =======================
   Function
   ======================= */

void ASTPrinter::print_function(const FunctionDecl& fn) {
    indent();
    std::cout << "Function " << fn.name;
    if (!fn.type_params.empty()) {
        std::cout << "<";
        for (size_t i = 0; i < fn.type_params.size(); ++i) {
            std::cout << fn.type_params[i] << (i == fn.type_params.size() - 1 ? "" : ", ");
        }
        std::cout << ">";
    }
    std::cout << " -> " << fn.return_type << '\n';

    indent_level_++;
    print_block(fn.body);
    indent_level_--;
}

void ASTPrinter::print_struct(const StructDecl& s) {
    indent();
    std::cout << "Struct " << s.name;
    if (!s.type_params.empty()) {
        std::cout << "<";
        for (size_t i = 0; i < s.type_params.size(); ++i) {
            std::cout << s.type_params[i] << (i == s.type_params.size() - 1 ? "" : ", ");
        }
        std::cout << ">";
    }
    std::cout << '\n';
    indent_level_++;
    for (const auto& field : s.fields) {
        indent();
        std::cout << "Field " << field.name << " : " << field.type << '\n';
    }
    indent_level_--;
}

void ASTPrinter::print_enum(const EnumDecl& e) {
    indent();
    std::cout << "Enum " << e.name;
    if (!e.type_params.empty()) {
        std::cout << "<";
        for (size_t i = 0; i < e.type_params.size(); ++i) {
            std::cout << e.type_params[i] << (i == e.type_params.size() - 1 ? "" : ", ");
        }
        std::cout << ">";
    }
    std::cout << '\n';
    indent_level_++;
    for (const auto& v : e.variants) {
        indent();
        std::cout << "Variant " << v.name;
        if (!v.types.empty()) {
            std::cout << "(";
            for (size_t i = 0; i < v.types.size(); ++i) {
                std::cout << v.types[i] << (i == v.types.size() - 1 ? "" : ", ");
            }
            std::cout << ")";
        }
        std::cout << '\n';
    }
    indent_level_--;
}

void ASTPrinter::print_impl(const ImplBlock& impl) {
    indent();
    std::cout << "Impl";
    if (!impl.type_params.empty()) {
        std::cout << "<";
        for (size_t i = 0; i < impl.type_params.size(); ++i) {
            std::cout << impl.type_params[i] << (i == impl.type_params.size() - 1 ? "" : ", ");
        }
        std::cout << ">";
    }
    std::cout << " " << impl.target_name << '\n';
    indent_level_++;
    for (const auto& assoc : impl.associated_types) {
        indent();
        std::cout << "AssociatedType " << assoc.name << " = " << assoc.default_type << '\n';
    }
    for (const auto& m : impl.methods) {
        print_function(m);
    }
    indent_level_--;
}

void ASTPrinter::print_trait(const TraitDecl& t) {
    indent();
    std::cout << "Trait " << t.name;
    if (!t.type_params.empty()) {
        std::cout << "<";
        for (size_t i = 0; i < t.type_params.size(); ++i) {
            std::cout << t.type_params[i] << (i == t.type_params.size() - 1 ? "" : ", ");
        }
        std::cout << ">";
    }
    std::cout << '\n';
    indent_level_++;
    for (const auto& assoc : t.associated_types) {
        indent();
        std::cout << "AssociatedType " << assoc.name;
        if (!assoc.default_type.empty()) {
            std::cout << " = " << assoc.default_type;
        }
        std::cout << '\n';
    }
    for (const auto& m : t.methods) {
        print_function(m);
    }
    indent_level_--;
}

/* =======================
   Block
   ======================= */

void ASTPrinter::print_block(const Block& block) {
    indent();
    std::cout << "Block\n";

    indent_level_++;
    for (const auto& stmt : block.statements) {
        print_statement(*stmt);
    }
    indent_level_--;
}

/* =======================
   Statements
   ======================= */

void ASTPrinter::print_statement(const Stmt& stmt) {
    if (auto let = dynamic_cast<const LetStmt*>(&stmt)) {
        indent();
        if (let->is_const)
            std::cout << "Const ";
        else {
            std::cout << "Let ";
            if (let->is_mutable)
                std::cout << "mut ";
        }
        std::cout << let->name << " : " << let->type_name << '\n';

        indent_level_++;
        print_expression(*let->initializer);
        indent_level_--;
        return;
    }

    if (auto ret = dynamic_cast<const ReturnStmt*>(&stmt)) {
        indent();
        std::cout << "Return\n";
        if (ret->expression) {
            indent_level_++;
            print_expression(*ret->expression);
            indent_level_--;
        }
        return;
    }

    if (auto es = dynamic_cast<const ExprStmt*>(&stmt)) {
        indent();
        std::cout << "ExprStmt\n";
        indent_level_++;
        print_expression(*es->expression);
        indent_level_--;
        return;
    }

    if (auto block = dynamic_cast<const BlockStmt*>(&stmt)) {
        print_block(block->block);
        return;
    }

    if (auto ifs = dynamic_cast<const IfStmt*>(&stmt)) {
        indent();
        std::cout << "If\n";
        indent_level_++;
        print_expression(*ifs->condition);
        print_statement(*ifs->then_branch);
        if (ifs->else_branch) {
            print_statement(*ifs->else_branch);
        }
        indent_level_--;
        return;
    }

    if (auto wh = dynamic_cast<const WhileStmt*>(&stmt)) {
        indent();
        std::cout << "While\n";
        indent_level_++;
        print_expression(*wh->condition);
        print_statement(*wh->body);
        indent_level_--;
        return;
    }

    if (auto ms = dynamic_cast<const MatchStmt*>(&stmt)) {
        indent();
        std::cout << "Match\n";
        indent_level_++;
        print_expression(*ms->expression);
        for (const auto& arm : ms->arms) {
            indent();
            std::cout << "Arm\n";
            indent_level_++;
            print_pattern(*arm.pattern);
            print_statement(*arm.body);
            indent_level_--;
        }
        indent_level_--;
        return;
    }

    if (auto asg = dynamic_cast<const AssignStmt*>(&stmt)) {
        indent();
        std::cout << "Assign\n";
        indent_level_++;
        print_expression(*asg->target);
        print_expression(*asg->value);
        indent_level_--;
        return;
    }

    if (auto fl = dynamic_cast<const ForStmt*>(&stmt)) {
        indent();
        std::cout << "For " << fl->variable;
        if (!fl->var_type.empty())
            std::cout << " : " << fl->var_type;
        std::cout << " in\n";
        indent_level_++;
        print_expression(*fl->iterable);
        print_statement(*fl->body);
        indent_level_--;
        return;
    }

    if (auto lp = dynamic_cast<const LoopStmt*>(&stmt)) {
        indent();
        std::cout << "Loop\n";
        indent_level_++;
        print_statement(*lp->body);
        indent_level_--;
        return;
    }

    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        indent();
        std::cout << "Break\n";
        return;
    }

    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        indent();
        std::cout << "Continue\n";
        return;
    }

    indent();
    std::cout << "<unknown statement>\n";
}

/* =======================
   Expressions
   ======================= */

void ASTPrinter::print_expression(const Expr& expr) {
    if (auto num = dynamic_cast<const NumberExpr*>(&expr)) {
        indent();
        std::cout << "Number(" << num->value << ")\n";
        return;
    }

    if (auto id = dynamic_cast<const IdentifierExpr*>(&expr)) {
        indent();
        std::cout << "Identifier(" << id->name << ")\n";
        return;
    }

    if (auto str = dynamic_cast<const StringExpr*>(&expr)) {
        indent();
        std::cout << "String(\"" << str->value << "\")\n";
        return;
    }

    if (auto ch = dynamic_cast<const CharExpr*>(&expr)) {
        indent();
        std::cout << "Char('" << ch->value << "')\n";
        return;
    }

    if (auto b = dynamic_cast<const BoolExpr*>(&expr)) {
        indent();
        std::cout << "Bool(" << (b->value ? "true" : "false") << ")\n";
        return;
    }

    if (auto call = dynamic_cast<const CallExpr*>(&expr)) {
        indent();
        std::cout << "Call\n";
        indent_level_++;
        print_expression(*call->callee);
        for (const auto& arg : call->arguments) {
            print_expression(*arg);
        }
        indent_level_--;
        return;
    }

    if (auto un = dynamic_cast<const UnaryExpr*>(&expr)) {
        indent();
        std::cout << "Unary(" << to_string(un->op) << ")\n";

        indent_level_++;
        print_expression(*un->operand);
        indent_level_--;
        return;
    }

    if (auto mv = dynamic_cast<const MoveExpr*>(&expr)) {
        indent();
        std::cout << "Move\n";
        indent_level_++;
        print_expression(*mv->operand);
        indent_level_--;
        return;
    }

    if (auto cast = dynamic_cast<const CastExpr*>(&expr)) {
        indent();
        std::cout << "Cast(" << cast->target_type << ")\n";
        indent_level_++;
        print_expression(*cast->expr);
        indent_level_--;
        return;
    }

    if (auto bin = dynamic_cast<const BinaryExpr*>(&expr)) {
        indent();
        std::cout << "Binary(" << to_string(bin->op) << ")\n";

        indent_level_++;
        print_expression(*bin->left);
        print_expression(*bin->right);
        indent_level_--;
        return;
    }

    if (auto sl = dynamic_cast<const StructLiteralExpr*>(&expr)) {
        indent();
        std::cout << "StructLiteral(" << sl->struct_name << ")\n";
        indent_level_++;
        for (const auto& field : sl->fields) {
            indent();
            std::cout << "FieldInit " << field.name << '\n';
            indent_level_++;
            print_expression(*field.value);
            indent_level_--;
        }
        indent_level_--;
        return;
    }

    if (auto ep = dynamic_cast<const ErrorPropagationExpr*>(&expr)) {
        indent();
        std::cout << "ErrorPropagation(?)\n";
        indent_level_++;
        print_expression(*ep->operand);
        indent_level_--;
        return;
    }

    if (auto aw = dynamic_cast<const AwaitExpr*>(&expr)) {
        indent();
        std::cout << "Await\n";
        indent_level_++;
        print_expression(*aw->operand);
        indent_level_--;
        return;
    }

    if (auto sp = dynamic_cast<const SpawnExpr*>(&expr)) {
        indent();
        std::cout << "Spawn\n";
        indent_level_++;
        print_expression(*sp->operand);
        indent_level_--;
        return;
    }

    if (auto rng = dynamic_cast<const RangeExpr*>(&expr)) {
        indent();
        std::cout << "Range\n";
        indent_level_++;
        if (rng->start)
            print_expression(*rng->start);
        if (rng->end)
            print_expression(*rng->end);
        indent_level_--;
        return;
    }

    indent();
    std::cout << "<unknown expression>\n";
}

void ASTPrinter::print_pattern(const Pattern& pattern) {
    if (auto lp = dynamic_cast<const LiteralPattern*>(&pattern)) {
        indent();
        std::cout << "LiteralPattern\n";
        indent_level_++;
        print_expression(*lp->literal);
        indent_level_--;
        return;
    }

    if (auto ip = dynamic_cast<const IdentifierPattern*>(&pattern)) {
        indent();
        std::cout << "IdentifierPattern(" << ip->name << ")\n";
        return;
    }

    if (dynamic_cast<const WildcardPattern*>(&pattern)) {
        indent();
        std::cout << "WildcardPattern\n";
        return;
    }

    if (auto vp = dynamic_cast<const VariantPattern*>(&pattern)) {
        indent();
        std::cout << "VariantPattern(" << vp->variant_name << ")\n";
        indent_level_++;
        for (const auto& sub : vp->sub_patterns) {
            print_pattern(*sub);
        }
        indent_level_--;
        return;
    }

    indent();
    std::cout << "<unknown pattern>\n";
}
} // namespace flux::ast
