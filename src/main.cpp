#include <iostream>
#include <fstream>
#include <sstream>

#include "lexer/lexer.h"
#include "lexer/diagnostic.h"

#include "parser/parser.h"
#include "ast/ast.h"

// Debug
#include "ast/ast_printer.h"

#include "semantic/resolver.h"


int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "flux: no input file\n";
        return 1;
    }

    const char * const path = argv[1];

    std::ifstream file(path);
    if (!file) {
        std::cerr << "flux: could not open file: " << path << '\n';
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    try {
        flux::Lexer lexer(buffer.str());
        const auto tokens = lexer.tokenize();

        flux::Parser parser(tokens);
        const flux::ast::Module module = parser.parse_module();

        flux::ast::ASTPrinter printer;
        printer.print(module);

        std::cout << "Parsed module: " << module.name << '\n';

        flux::semantic::Resolver resolver;
        resolver.resolve(module);

        std::cout << "Semantic analysis OK\n";
    } catch (const flux::DiagnosticError &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    // TODO:
    // 1. read source
    // 2. lex
    // 3. parse
    // 4. analyze
    // 5. generate code

    return 0;
}
