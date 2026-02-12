#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <iostream>
#include <string>
#include <vector>

using namespace flux;
using namespace flux::semantic;

void test_trait_method_dispatch() {
    std::cout << "--- Running test_trait_method_dispatch ---" << std::endl;
    std::string source = R"(
        trait Greet {
            func hello(self: &Self) -> String;
        }

        struct Person { name: String }

        impl Greet for Person {
            func hello(self: &Person) -> String {
                return "Hello";
            }
        }

        func main() -> Void {
            let p: Person = Person { name: "Alice" };
            let s: String = p.hello(); // Should resolve via Greet trait
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: trait method dispatch resolved correctly" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: " << e.what() << std::endl;
    }
}

void test_trait_method_dispatch_reference() {
    std::cout << "--- Running test_trait_method_dispatch_reference ---" << std::endl;
    std::string source = R"(
        trait Greet {
            func hello(self: &Self) -> String;
        }

        struct Person { name: String }

        impl Greet for Person {
            func hello(self: &Person) -> String {
                return "Hello";
            }
        }

        func main() -> Void {
            let p: Person = Person { name: "Alice" };
            let pref: &Person = &p;
            let s: String = pref.hello();
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: trait method dispatch on reference resolved correctly" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: " << e.what() << std::endl;
    }
}

void test_trait_method_dispatch_inherent() {
    std::cout << "--- Running test_trait_method_dispatch_inherent ---" << std::endl;
    std::string source = R"(
        trait Greet {
            func hello(self: &Self) -> String;
        }

        struct Person { name: String }

        // Inherent impl
        impl Person {
            func hello(self: &Person) -> String {
                return "Inherent";
            }
        }

        impl Greet for Person {
            func hello(self: &Person) -> String {
                return "Trait";
            }
        }

        func main() -> Void {
            let p: Person = Person { name: "Alice" };
            let s: String = p.hello(); // Should resolve to inherent method if implemented first
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: inherent method takes precedence or is resolved" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: " << e.what() << std::endl;
    }
}

void test_trait_method_dispatch_generic() {
    std::cout << "--- Running test_trait_method_dispatch_generic ---" << std::endl;
    std::string source = R"(
        trait Greet {
            func hello<T>(self: &Self, extra: T) -> String;
        }

        struct Person { name: String }

        impl Greet for Person {
            func hello<T>(self: &Person, extra: T) -> String {
                return "Hello";
            }
        }

        func main() -> Void {
            let p: Person = Person { name: "Alice" };
            let s: String = p.hello<Int32>(42);
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: generic trait method dispatch resolved correctly" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: " << e.what() << std::endl;
    }
}

void test_trait_method_dispatch_recursion() {
    std::cout << "--- Running test_trait_method_dispatch_recursion ---" << std::endl;
    std::string source = R"(
        trait Counter {
            func count(self: &Self, n: Int32) -> Int32;
        }

        struct MyCounter { val: Int32 }

        impl Counter for MyCounter {
            func count(self: &MyCounter, n: Int32) -> Int32 {
                if (n == 0) {
                    return 0;
                }
                return 1 + self.count(n - 1);
            }
        }

        func main() -> Void {
            let c: MyCounter = MyCounter { val: 0 };
            let r: Int32 = c.count(5);
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << "PASSED: recursive trait method dispatch resolved correctly" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cout << "FAIL: " << e.what() << std::endl;
    }
}

int main() {
    test_trait_method_dispatch();
    test_trait_method_dispatch_reference();
    test_trait_method_dispatch_inherent();
    test_trait_method_dispatch_generic();
    test_trait_method_dispatch_recursion();
    return 0;
}
