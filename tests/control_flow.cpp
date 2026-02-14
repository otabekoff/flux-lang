#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>
#include <string>

// A simple manual find to avoid header issues with strstr/find
bool manual_contains(const char* haystack, const char* needle) {
    if (!haystack || !needle)
        return false;
    if (needle[0] == '\0')
        return true;
    for (int i = 0; haystack[i] != '\0'; ++i) {
        bool match = true;
        for (int j = 0; needle[j] != '\0'; ++j) {
            if (haystack[i + j] == '\0' || haystack[i + j] != needle[j]) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

void run_test(const std::string& code, const std::string& test_name) {
    try {
        flux::Lexer lexer(code);
        auto tokens = lexer.tokenize();
        flux::Parser parser(tokens);
        auto module = parser.parse_module();
        flux::semantic::Resolver resolver;
        resolver.resolve(module);
        std::cout << test_name << " passed.\n";
    } catch (const std::exception& e) {
        std::cerr << test_name << " failed: " << e.what() << "\n";
        assert(false);
    }
}

void expect_error(const std::string& code, const std::string& test_name,
                  const std::string& expected_error) {
    try {
        flux::Lexer lexer(code);
        auto tokens = lexer.tokenize();
        flux::Parser parser(tokens);
        auto module = parser.parse_module();
        flux::semantic::Resolver resolver;
        resolver.resolve(module);
        std::cerr << test_name << " failed: expected error '" << expected_error
                  << "', but it passed.\n";
        assert(false);
    } catch (const flux::DiagnosticError& e) {
        if (manual_contains(e.what(), expected_error.c_str())) {
            std::cout << test_name << " passed (caught expected error: " << e.what() << ").\n";
        } else {
            std::cerr << test_name << " failed: caught error '" << e.what() << "', but expected '"
                      << expected_error << "'.\n";
            assert(false);
        }
    } catch (const std::exception& e) {
        std::cerr << test_name << " failed: caught unexpected exception: " << e.what() << "\n";
        assert(false);
    }
}

void test_uninitialized_read() {
    std::string code = R"(
        func test() {
            let x: Int32;
            let y: Int32 = x;
        }
    )";
    expect_error(code, "test_uninitialized_read", "uninitialized");
}

void test_initialized_read() {
    std::string code = R"(
        func test() {
            let x: Int32;
            x = 10;
            let y: Int32 = x;
        }
    )";
    run_test(code, "test_initialized_read");
}

void test_if_exhaustive_init() {
    std::string code = R"(
        func test(cond: Bool) {
            let x: Int32;
            if cond {
                x = 1;
            } else {
                x = 2;
            }
            let y: Int32 = x;
        }
    )";
    run_test(code, "test_if_exhaustive_init");
}

void test_if_non_exhaustive_init() {
    std::string code = R"(
        func test(cond: Bool) {
            let x: Int32;
            if cond {
                x = 1;
            }
            let y: Int32 = x;
        }
    )";
    expect_error(code, "test_if_non_exhaustive_init", "uninitialized");
}

void test_match_exhaustive_init() {
    std::string code = R"(
        enum Color { Red, Green, Blue }
        func test(c: Color) {
            let x: Int32;
            match c {
                Color::Red => { x = 1; },
                Color::Green => { x = 2; },
                Color::Blue => { x = 3; }
            }
            let y: Int32 = x;
        }
    )";
    run_test(code, "test_match_exhaustive_init");
}

void test_unreachable_code_return() {
    std::string code = R"(
        func test() {
            return;
            let x: Int32 = 1;
        }
    )";
    expect_error(code, "test_unreachable_code_return", "unreachable");
}

void test_unreachable_code_panic() {
    std::string code = R"(
        func test() {
            panic("error");
            let x: Int32 = 1;
        }
    )";
    expect_error(code, "test_unreachable_code_panic", "unreachable");
}

void test_for_loop_iterator() {
    std::string code = R"(
        func test() {
            for i in 1..10 {
                let x: Int32 = i;
            }
        }
    )";
    run_test(code, "test_for_loop_iterator");
}

void test_break_value() {
    std::string code = R"(
        func test() {
            loop {
                break 42;
            }
        }
    )";
    run_test(code, "test_break_value");
}

void test_reassign_immutable() {
    std::string code = R"(
        func test() {
            let x: Int32 = 1;
            x = 2;
        }
    )";
    expect_error(code, "test_reassign_immutable", "reassign to immutable");
}

int main() {
    test_uninitialized_read();
    test_initialized_read();
    test_if_exhaustive_init();
    test_if_non_exhaustive_init();
    test_match_exhaustive_init();
    test_unreachable_code_return();
    test_unreachable_code_panic();
    test_for_loop_iterator();
    test_break_value();
    test_reassign_immutable();
    std::cout << "All control flow tests passed.\n";
    return 0;
}
