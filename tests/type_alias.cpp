#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <cassert>
#include <exception>
#include <iostream>

using namespace flux;

void test_recursive_resolution() {
    std::cout << "--- Testing recursive resolution ---" << std::endl;
    const std::string source = R"(
        type A = B;
        type B = C;
        type C = Int32;

        func foo(x: A) -> B {
            return x;
        }
    )";

    try {
        Lexer lexer(source);
        std::cout << "Tokenizing..." << std::endl;
        auto tokens = lexer.tokenize();
        std::cout << "Parsing..." << std::endl;
        Parser parser(tokens);
        auto mod = parser.parse_module();

        std::cout << "Resolving..." << std::endl;
        semantic::Resolver resolver;
        resolver.resolve_module(mod);
        std::cout << "Returned from resolve_module" << std::endl;
        std::cout << "RecursiveResolution: PASS" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "RecursiveResolution FAILED: " << e.what() << std::endl;
        exit(1);
    } catch (const std::exception& e) {
        std::cerr << "RecursiveResolution CRASHED (std::exception): " << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "RecursiveResolution CRASHED (unknown exception)" << std::endl;
        exit(1);
    }
}

void test_circular_resolution() {
    std::cout << "--- Testing circular resolution detection ---" << std::endl;
    const std::string source = R"(
        type A = B;
        type B = A;

        func foo(x: A) -> Void {
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        semantic::Resolver resolver;
        try {
            resolver.resolve_module(mod);
            std::cerr << "CircularResolution: FAILED (should have thrown)" << std::endl;
            exit(1);
        } catch (const DiagnosticError& e) {
            std::cout << "CircularResolution: PASS (caught: " << e.what() << ")" << std::endl;
            if (std::string(e.what()).find("circular") == std::string::npos) {
                std::cerr << "CircularResolution: FAILED (wrong error message)" << std::endl;
                exit(1);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "CircularResolution CRASHED (std::exception): " << e.what() << std::endl;
        exit(1);
    } catch (...) {
        std::cerr << "CircularResolution CRASHED (unknown exception)" << std::endl;
        exit(1);
    }
}

int main() {
    try {
        test_recursive_resolution();
        test_circular_resolution();
        std::cout << "\ntype_alias: ALL TESTS PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unhandled unknown exception in main" << std::endl;
        return 1;
    }
    return 0;
}
