#ifndef FLUX_AST_PRINTER_H
#define FLUX_AST_PRINTER_H

#include "ast/ast.h"
#include <string>

namespace flux::ast {
class ASTPrinter {
  public:
    void print(const Module& module);

  private:
    void indent();

    void print_module(const Module& module);

    void print_import(const Import& imp);

    void print_function(const FunctionDecl& fn);

    void print_struct(const StructDecl& s);
    void print_enum(const EnumDecl& e);
    void print_impl(const ImplBlock& i);
    void print_trait(const TraitDecl& t);

    void print_block(const Block& block);

    void print_statement(const Stmt& stmt);

    void print_expression(const Expr& expr);

    void print_pattern(const Pattern& pattern);

  private:
    int indent_level_ = 0;
};
} // namespace flux::ast

#endif // FLUX_AST_PRINTER_H
