#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <iostream>
#include <string>
#include <vector>

using namespace flux;
using namespace flux::semantic;

void test_default_method_omitted() {
    std::cout << "--- Running test_default_method_omitted ---" << std::endl;
    std::string source = R"(
        trait Greet {
            func hello(self: &Self) -> String;
            func bye(self: &Self) -> String {
                return "Goodbye";
            }
        }

        struct Person { name: String }

        impl Greet for Person {
            func hello(self: &Person) -> String {
                return "Hello";
            }
            // 'bye' is omitted, should use default
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: default method successfully omitted" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: unexpected error: " << e.what() << std::endl;
    }
}

void test_default_method_overridden() {
    std::cout << "--- Running test_default_method_overridden ---" << std::endl;
    std::string source = R"(
        trait Greet {
            func hello(self: &Self) -> String;
            func bye(self: &Self) -> String {
                return "Goodbye";
            }
        }

        struct Robot { id: Int32 }

        impl Greet for Robot {
            func hello(self: &Robot) -> String {
                return "Beep";
            }
            func bye(self: &Robot) -> String {
                return "Shutting down";
            }
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: default method overridden" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: unexpected error: " << e.what() << std::endl;
    }
}

void test_missing_mandatory_method() {
    std::cout << "--- Running test_missing_mandatory_method ---" << std::endl;
    std::string source = R"(
        trait Greet {
            func hello(self: &Self) -> String; // Mandatory
            func bye(self: &Self) -> String {
                return "Goodbye";
            }
        }

        struct Alien { category: String }

        impl Greet for Alien {
            // 'hello' is missing!
            func bye(self: &Alien) -> String { return "Zog"; }
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "FAIL: expected error for missing mandatory method" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "PASSED: caught error: " << e.what() << std::endl;
    }
}

int main() {
    test_default_method_omitted();
    test_default_method_overridden();
    test_missing_mandatory_method();

    std::cout << "ALL TRAIT DEFAULT METHOD TESTS PASSED." << std::endl;
    return 0;
}
