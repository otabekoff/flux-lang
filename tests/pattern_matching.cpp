#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux;
using namespace flux::semantic;

void run_test(const std::string& code, const std::string& test_name) {
    try {
        Lexer lexer(code);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto module = parser.parse_module();
        Resolver resolver;
        resolver.resolve(module);
        std::cout << test_name << " passed.\n";
    } catch (const std::exception& e) {
        std::cerr << test_name << " failed: " << e.what() << "\n";
        assert(false);
    }
}

void test_tuple_destructuring_let() {
    std::string code = R"(
        func test() {
            let (x, y): (Int32, Float64) = (1, 2.0);
            let a: Int32 = x;
            let b: Float64 = y;
        }
    )";
    run_test(code, "test_tuple_destructuring_let");
}

void test_match_tuple_pattern() {
    std::string code = R"(
        func test() {
            let t: (Int32, Int32) = (1, 2);
            match t {
                (1, y) => { let z: Int32 = y; },
                (x, 2) => { let z: Int32 = x; },
                _ => {}
            }
        }
    )";
    run_test(code, "test_match_tuple_pattern");
}

void test_match_struct_pattern() {
    std::string code = R"(
        struct Point { x: Int32, y: Int32 }
        func test() {
            let p: Point = Point { x: 1, y: 2 };
            match p {
                Point { x: 1, y: b } => { let z: Int32 = b; },
                Point { x: a, y: 2 } => { let z: Int32 = a; },
                _ => {}
            }
        }
    )";
    run_test(code, "test_match_struct_pattern");
}

void test_nested_patterns() {
    std::string code = R"(
        struct Wrapper { val: (Int32, Bool) }
        func test() {
            let w: Wrapper = Wrapper { val: (42, true) };
            match w {
                Wrapper { val: (n, true) } => { let x: Int32 = n; },
                _ => {}
            }
        }
    )";
    run_test(code, "test_nested_patterns");
}

void test_range_patterns() {
    std::string code = R"(
        func test() {
            let n: Int32 = 5;
            match n {
                1..10 => { let x: Int32 = n; },
                11..=20 => { let x: Int32 = n; },
                _ => {}
            }

            let c: Char = 'a';
            match c {
                'a'..'z' => { let y: Char = c; },
                _ => {}
            }
        }
    )";
    run_test(code, "test_range_patterns");
}

void test_or_patterns() {
    std::string code = R"(
        func test() {
            let n: Int32 = 1;
            match n {
                1 | 2 | 3 => { let x: Int32 = n; },
                _ => {}
            }

            let t: (Int32, Int32) = (1, 2);
            match t {
                (1, x) | (x, 2) => { let y: Int32 = x; },
                _ => {}
            }
        }
    )";
    run_test(code, "test_or_patterns");
}

int main() {
    test_tuple_destructuring_let();
    test_match_tuple_pattern();
    test_match_struct_pattern();
    test_nested_patterns();
    test_range_patterns();
    test_or_patterns();
    std::cout << "All pattern matching tests passed.\n";
    return 0;
}
