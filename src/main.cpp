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

// Codegen
#include "codegen/codegen.h"

// Driver
#include "driver/module_loader.h"
#include <filesystem>
#include <map>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "flux: no input file\n";
        return 1;
    }

    const char* const path = argv[1];
    bool emit_ir = false;
    bool emit_llvm = false;

    // Parse flags
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--emit-ir")
            emit_ir = true;
        else if (arg == "--emit-llvm")
            emit_llvm = true;
    }

    std::string entry_path = path;

    try {
        flux::ModuleLoader loader;
        // Search in current directory and std/
        loader.add_search_path(std::filesystem::current_path() / "std");

        std::cout << "Loading modules...\n";
        auto* main_module = loader.load(entry_path);

        std::vector<flux::ast::Module*> modules;
        for (auto& [name, mod] :
             const_cast<std::map<std::string, flux::ast::Module>&>(loader.modules())) {
            modules.push_back(&mod);
        }

        std::cout << "Loaded " << modules.size() << " modules.\n";

        flux::semantic::Resolver resolver;
        resolver.resolve(modules);

        std::cout << "Semantic analysis OK\n";

        // Monomorphization
        std::cout << "Starting monomorphization...\n";
        flux::semantic::Monomorphizer monomorphizer(resolver);
        flux::ast::Module monomorphized_module = monomorphizer.monomorphize(*main_module);

        std::cout << "Monomorphization OK. Specialized functions generated: "
                  << (monomorphized_module.functions.size() - main_module->functions.size())
                  << "\n";

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

        // Codegen
        if (emit_llvm) {
            std::cout << "Generating LLVM IR...\n";
            flux::codegen::CodeGenerator generator;
            generator.compile(ir_module);
            std::cout << generator.to_string() << std::endl;
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
