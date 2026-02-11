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

void test_parse_type_param_bounds() {
    std::cout << "--- Testing parse_type_param_bounds ---" << std::endl;

    // No bounds
    {
        auto bounds = Resolver::parse_type_param_bounds({"T"});
        assert(bounds.empty() && "Unbounded param should produce no bound entries");
    }

    // Single bound
    {
        auto bounds = Resolver::parse_type_param_bounds({"T: Display"});
        assert(bounds.size() == 1);
        assert(bounds[0].param_name == "T");
        assert(bounds[0].bounds.size() == 1);
        assert(bounds[0].bounds[0] == "Display");
    }

    // Multiple bounds
    {
        auto bounds = Resolver::parse_type_param_bounds({"T: Display + Clone"});
        assert(bounds.size() == 1);
        assert(bounds[0].param_name == "T");
        assert(bounds[0].bounds.size() == 2);
        assert(bounds[0].bounds[0] == "Display");
        assert(bounds[0].bounds[1] == "Clone");
    }

    // Multiple type params
    {
        auto bounds = Resolver::parse_type_param_bounds({"T: Display", "U: Clone"});
        assert(bounds.size() == 2);
        assert(bounds[0].param_name == "T");
        assert(bounds[0].bounds[0] == "Display");
        assert(bounds[1].param_name == "U");
        assert(bounds[1].bounds[0] == "Clone");
    }

    std::cout << "ParseTypeParamBounds: PASS" << std::endl;
}

void test_trait_bound_pass() {
    std::cout << "--- Testing trait bound enforcement (pass) ---" << std::endl;

    const std::string source = R"(
        trait Display {
            func to_string(self) -> String;
        }

        struct Point {
            x: Int32,
            y: Int32
        }

        impl Display for Point {
            func to_string(self) -> String {
                return "Point";
            }
        }

        func print_it<T: Display>(x: T) -> Void {}

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
        std::cout << "TraitBoundPass: PASS" << std::endl;
    } catch (const DiagnosticError& e) {
        std::cerr << "TraitBoundPass FAILED: " << e.what() << " at " << e.line() << ":"
                  << e.column() << std::endl;
        exit(1);
    }
}

void test_trait_bound_fail() {
    std::cout << "--- Testing trait bound enforcement (fail) ---" << std::endl;

    const std::string source = R"(
        trait Display {
            func to_string(self) -> String;
        }

        struct Point {
            x: Int32,
            y: Int32
        }

        func print_it<T: Display>(x: T) -> Void {}

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
        std::cerr << "TraitBoundFail FAILED (should have thrown)" << std::endl;
        exit(1);
    } catch (const DiagnosticError& e) {
        if (std::string(e.what()).find("does not implement trait") != std::string::npos) {
            std::cout << "TraitBoundFail: PASS (caught: " << e.what() << ")" << std::endl;
        } else {
            std::cerr << "TraitBoundFail FAILED (wrong error: " << e.what() << ")" << std::endl;
            exit(1);
        }
    }
}

void test_multi_bound_enforcement() {
    std::cout << "--- Testing multi-bound enforcement ---" << std::endl;

    const std::string source = R"(
        trait Display {
            func to_string(self) -> String;
        }

        trait Clone {
            func clone(self) -> Self;
        }

        struct Point {
            x: Int32,
            y: Int32
        }

        impl Display for Point {
            func to_string(self) -> String {
                return "Point";
            }
        }

        func needs_both<T: Display + Clone>(x: T) -> Void {}

        func main() -> Void {
            let p: Point = Point { x: 1, y: 2 };
            needs_both(p);
        }
    )";

    try {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto mod = parser.parse_module();

        Resolver resolver;
        resolver.resolve_module(mod);
        std::cerr << "MultiBoundEnforcement FAILED (should have thrown)" << std::endl;
        exit(1);
    } catch (const DiagnosticError& e) {
        if (std::string(e.what()).find("does not implement trait 'Clone'") != std::string::npos) {
            std::cout << "MultiBoundEnforcement: PASS (caught: " << e.what() << ")" << std::endl;
        } else {
            std::cerr << "MultiBoundEnforcement FAILED (wrong error: " << e.what() << ")"
                      << std::endl;
            exit(1);
        }
    }
}

int main() {
    try {
        test_parse_type_param_bounds();
        test_trait_bound_pass();
        test_trait_bound_fail();
        test_multi_bound_enforcement();
        std::cout << "\ntrait_bound: ALL TESTS PASSED" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN CRITICAL ERROR" << std::endl;
        return 1;
    }
    return 0;
}
