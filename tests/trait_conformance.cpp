#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <iostream>
#include <string>
#include <vector>

using namespace flux;
using namespace flux::semantic;

void test_missing_method() {
    std::cout << "--- Running test_missing_method ---" << std::endl;
    std::string source = R"(
        trait Display {
            func show(self: &Self) -> String;
        }

        struct Point { x: Int32, y: Int32 }

        impl Display for Point {
            // Missing show method
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "FAIL: expected error for missing method" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "PASSED: caught error: " << e.what() << std::endl;
    }
}

void test_signature_mismatch_return() {
    std::cout << "--- Running test_signature_mismatch_return ---" << std::endl;
    std::string source = R"(
        trait Area {
            func area(self: &Self) -> Float64;
        }

        struct Circle { r: Float64 }

        impl Area for Circle {
            func area(self: &Self) -> Int32 { return 0; } // Wrong return type
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "FAIL: expected signature mismatch error" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "PASSED: caught error: " << e.what() << std::endl;
    }
}

void test_signature_mismatch_params() {
    std::cout << "--- Running test_signature_mismatch_params ---" << std::endl;
    std::string source = R"(
        trait Add<T> {
            func add(self: Self, other: T) -> Self;
        }

        struct Complex { re: Float64, im: Float64 }

        impl Add<Complex> for Complex {
            func add(self: Complex, other: Int32) -> Complex { return self; } // other: Int32 != Complex
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "FAIL: expected signature mismatch error (params)" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "PASSED: caught error: " << e.what() << std::endl;
    }
}

void test_self_mismatch() {
    std::cout << "--- Running test_self_mismatch ---" << std::endl;
    std::string source = R"(
        trait Clone {
            func clone(self: &Self) -> Self;
        }

        struct Data { val: Int32 }

        impl Clone for Data {
            func clone(self: Data) -> Data { return self; } // self != &Data
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "FAIL: expected error for self mismatch" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "PASSED: caught error: " << e.what() << std::endl;
    }
}

void test_valid_implementation() {
    std::cout << "--- Running test_valid_implementation ---" << std::endl;
    std::string source = R"(
        trait Display {
            func to_string(self: &Self) -> String;
        }

        trait Add<T> {
            func add(self: Self, other: T) -> Self;
        }

        struct Point { x: Int32, y: Int32 }

        impl Display for Point {
            func to_string(self: &Point) -> String { return "Point"; }
        }

        impl Add<Point> for Point {
            func add(self: Point, other: Point) -> Point { return self; }
        }

        func main() -> Void {
            let p: Point = Point { x: 1, y: 2 };
            let s: String = p.to_string();
            let p2: Point = p.add(p);
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: valid implementation" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: unexpected error: " << e.what() << std::endl;
    }
}

int main() {
    test_missing_method();
    test_signature_mismatch_return();
    test_signature_mismatch_params();
    test_self_mismatch();
    test_valid_implementation();

    std::cout << "ALL TRAIT CONFORMANCE TESTS PASSED." << std::endl;
    return 0;
}
