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

void test_option_propagation() {
    std::string code = R"(
        func get_val() -> Option<Int32> {
            return Some(42);
        }
        func test() -> Option<Int32> {
            let x: Int32 = get_val()?;
            return Some(x);
        }
    )";
    run_test(code, "test_option_propagation");
}

void test_result_propagation() {
    std::string code = R"(
        func get_val() -> Result<Int32, String> {
            return Ok(42);
        }
        func test() -> Result<Int32, String> {
            let x: Int32 = get_val()?;
            return Ok(x);
        }
    )";
    run_test(code, "test_result_propagation");
}

void test_mismatched_propagation_option_in_result() {
    std::string code = R"(
        func get_opt() -> Option<Int32> { return None; }
        func test() -> Result<Int32, String> {
            let x: Int32 = get_opt()?;
            return Ok(x);
        }
    )";
    expect_error(code, "test_mismatched_propagation_option_in_result",
                 "propagate Option None in function returning Result");
}

void test_mismatched_propagation_result_in_option() {
    std::string code = R"(
        func get_res() -> Result<Int32, String> { return Ok(1); }
        func test() -> Option<Int32> {
            let x: Int32 = get_res()?;
            return Some(x);
        }
    )";
    expect_error(code, "test_mismatched_propagation_result_in_option",
                 "propagate Result error in function returning Option");
}

void test_mismatched_error_types() {
    std::string code = R"(
        func get_res() -> Result<Int32, Int32> { return Err(1); }
        func test() -> Result<Int32, String> {
            let x: Int32 = get_res()?;
            return Ok(x);
        }
    )";
    expect_error(code, "test_mismatched_error_types", "does not match function return error type");
}

void test_propagation_outside_compatible_function() {
    std::string code = R"(
        func get_val() -> Option<Int32> { return None; }
        func test() -> Void {
            let x: Int32 = get_val()?;
        }
    )";
    expect_error(code, "test_propagation_outside_compatible_function",
                 "only be used in functions returning Option or Result");
}

void test_non_option_result_propagation() {
    std::string code = R"(
        func test() -> Option<Int32> {
            let x: Int32 = 42?;
            return Some(x);
        }
    )";
    expect_error(code, "test_non_option_result_propagation",
                 "only be used on Option or Result types");
}

int main() {
    test_option_propagation();
    test_result_propagation();
    test_mismatched_propagation_option_in_result();
    test_mismatched_propagation_result_in_option();
    test_mismatched_error_types();
    test_propagation_outside_compatible_function();
    test_non_option_result_propagation();
    std::cout << "All error handling tests passed.\n";
    return 0;
}
