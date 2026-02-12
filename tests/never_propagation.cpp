#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"

using namespace flux;
using namespace flux::semantic;

void test_never_propagation() {
    std::cout << "--- Testing Never propagation ---" << std::endl;

    const std::string source = R"(
        func foo() -> Void {
            let x: Int32 = panic("!");
            let y: Float64 = 1.0 + panic("!");
            let z: Bool = !panic("!");
            let t: (Int32, String) = (1, panic("!"));
            let arr: [Int32; 3] = [1, 2, panic("!")];

            bar(panic("!"));
        }

        func bar(x: Int32) -> Void {}
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve(mod);
        std::cout << "NeverPropagation: PASS" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "NeverPropagation FAILED: " << e.what() << " at " << e.line() << ":"
                  << e.column() << std::endl;
        exit(1);
    }
}

void test_unreachable_code() {
    std::cout << "--- Testing unreachable code detection ---" << std::endl;

    const std::string source = R"(
        func foo() -> Void {
            panic("!");
            let x: Int32 = 5;
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve(mod);
        std::cerr << "UnreachableCode FAILED (should have thrown)" << std::endl;
        exit(1);
    } catch (const DiagnosticError& e) {
        if (std::string(e.what()).find("unreachable code") != std::string::npos) {
            std::cout << "UnreachableCode: PASS (caught: " << e.what() << ")" << std::endl;
        } else {
            std::cerr << "UnreachableCode FAILED (wrong error: " << e.what() << ")" << std::endl;
            exit(1);
        }
    }
}

int main() {
    try {
        test_never_propagation();
        test_unreachable_code();
        std::cout << "\nnever_propagation: ALL TESTS PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN CRITICAL ERROR" << std::endl;
        return 1;
    }
    return 0;
}
