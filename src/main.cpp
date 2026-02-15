#include <fstream>
#include <iostream>
#include <sstream>

#include "lexer/diagnostic.h"
#include "lexer/lexer.h"

#include "ast/ast.h"
#include "parser/parser.h"

// Debug
#include "ast/ast_printer.h"

#include "semantic/monomorphizer.h"

// IR
#include "ir/ir_lowering.h"
#include "ir/ir_pass.h"
#include "ir/ir_printer.h"
#include "ir/passes/constant_folding.h"
#include "ir/passes/dead_code_elimination.h"
#include "ir/passes/inliner.h"
#include "ir/passes/ir_verifier.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "flux: no input file\n";
        return 1;
    }

    const char* const path = argv[1];
    bool emit_ir = false;

    // Parse flags
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--emit-ir")
            emit_ir = true;
    }

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

        std::cout << "Parsed module: " << module.name << '\n';

        flux::semantic::Resolver resolver;
        resolver.resolve(module);

        std::cout << "Semantic analysis OK\n";

        // Monomorphization
        std::cout << "Starting monomorphization...\n";
        flux::semantic::Monomorphizer monomorphizer(resolver);
        flux::ast::Module monomorphized_module = monomorphizer.monomorphize(module);

        std::cout << "Monomorphization OK. Specialized functions generated: "
                  << (monomorphized_module.functions.size() - module.functions.size()) << "\n";

        // IR Lowering
        std::cout << "Lowering to IR...\n";
        flux::ir::IRLowering lowering;
        auto ir_module = lowering.lower(monomorphized_module);

        std::cout << "IR lowering OK. Functions: " << ir_module.functions.size() << "\n";

        // IR Optimization Passes
        std::cout << "Running IR passes...\n";
        std::vector<std::unique_ptr<flux::ir::IRPass>> passes;

        // Validation (Pre-opt)
        passes.push_back(std::make_unique<flux::ir::IRVerifierPass>());

        // Optimizations
        passes.push_back(std::make_unique<flux::ir::InlinerPass>());
        passes.push_back(std::make_unique<flux::ir::ConstantFoldingPass>());
        passes.push_back(std::make_unique<flux::ir::DeadCodeEliminationPass>());

        // Validation (Post-opt)
        passes.push_back(std::make_unique<flux::ir::IRVerifierPass>());

        int modified = flux::ir::run_passes(ir_module, passes);
        std::cout << "IR passes complete. Passes that modified IR: " << modified << "\n";

        // Emit IR if requested
        if (emit_ir) {
            flux::ir::IRPrinter printer;
            printer.print(ir_module, std::cout);
        }

    } catch (const flux::DiagnosticError& e) {
        std::cerr << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Internal error: " << e.what() << '\n';
        return 1;
    }

    return 0;
}
