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

void test_where_clause_function() {
    std::cout << "--- Testing function where clause ---" << std::endl;

    const std::string source = R"(
        trait Display {
            func to_string(self) -> String;
        }

        struct Point { x: Int32, y: Int32 }

        impl Display for Point {
            func to_string(self) -> String { return "Point"; }
        }

        func print_it<T>(x: T) -> Void where T: Display {
            // body
        }

        func main() -> Void {
            let p: Point = Point { x: 1, y: 2 };
            print_it(p);
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve_module(mod);
        std::cout << "WhereClauseFunction: PASS" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "WhereClauseFunction FAILED: " << e.what() << " at " << e.line() << ":"
                  << e.column() << std::endl;
        exit(1);
    }
}

void test_where_clause_impl() {
    std::cout << "--- Testing impl where clause ---" << std::endl;

    const std::string source = R"(
        trait Clone {
            func clone(self) -> Self;
        }

        struct Box<T> { val: T }

        impl<T> Box<T> where T: Clone {
            func clone(self) -> Box<T> {
                return Box { val: self.val.clone() };
            }
        }

        func main() -> Void {}
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();
        // Just verify parsing for now, semantic check is harder without full monomorphization
        std::cout << "WhereClauseImpl: PASS (parsed)" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "WhereClauseImpl FAILED: " << e.what() << " at " << e.line() << ":"
                  << e.column() << std::endl;
        exit(1);
    }
}

void test_where_clause_fail() {
    std::cout << "--- Testing function where clause failure ---" << std::endl;

    const std::string source = R"(
        trait Display {
            func to_string(self) -> String;
        }

        struct Point { x: Int32, y: Int32 }

        func print_it<T>(x: T) -> Void where T: Display {}

        func main() -> Void {
            let p: Point = Point { x: 1, y: 2 };
            print_it(p); // Point does not implement Display
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve_module(mod);
        std::cerr << "WhereClauseFail FAILED (should have thrown)" << std::endl;
        exit(1);
    } catch (const DiagnosticError& e) {
        if (std::string(e.what()).find("does not implement trait") != std::string::npos) {
            std::cout << "WhereClauseFail: PASS (caught mismatch)" << std::endl;
        } else {
            std::cerr << "WhereClauseFail FAILED (wrong error: " << e.what() << ")" << std::endl;
            exit(1);
        }
    }
}

int main() {
    try {
        test_where_clause_function();
        test_where_clause_impl();
        test_where_clause_fail();
        std::cout << "\nwhere_clause: ALL TESTS PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
