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

void expect_visibility_error(const std::string& code_a, const std::string& code_b,
                             const std::string& test_name, const std::string& expected_error) {
    try {
        flux::Lexer lexer_a(code_a);
        flux::Parser parser_a(lexer_a.tokenize());
        auto module_a = parser_a.parse_module();

        flux::Lexer lexer_b(code_b);
        flux::Parser parser_b(lexer_b.tokenize());
        auto module_b = parser_b.parse_module();

        flux::semantic::Resolver resolver;
        resolver.initialize_intrinsics();

        // Resolve Module A (this populates global scope with A:: symbols)
        resolver.resolve(module_a);

        // Resolve Module B
        resolver.resolve(module_b);

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

void run_visibility_test(const std::string& code_a, const std::string& code_b,
                         const std::string& test_name) {
    try {
        flux::Lexer lexer_a(code_a);
        flux::Parser parser_a(lexer_a.tokenize());
        auto module_a = parser_a.parse_module();

        flux::Lexer lexer_b(code_b);
        flux::Parser parser_b(lexer_b.tokenize());
        auto module_b = parser_b.parse_module();

        flux::semantic::Resolver resolver;
        resolver.initialize_intrinsics();

        resolver.resolve(module_a);
        resolver.resolve(module_b);

        std::cout << test_name << " passed.\n";
    } catch (const std::exception& e) {
        std::cerr << test_name << " failed: " << e.what() << "\n";
        assert(false);
    }
}

void test_public_function_access() {
    std::string mod_a = "module A; pub func hello() {}";
    std::string mod_b = "module B; import A; func test() { A::hello(); }";
    run_visibility_test(mod_a, mod_b, "test_public_function_access");
}

void test_private_function_access() {
    std::string mod_a = "module A; func secret() {}";
    std::string mod_b = "module B; import A; func test() { A::secret(); }";
    // Since secret is in Module A, it is registered as A::secret in the global scope
    // with visibility Private and module_name A.
    expect_visibility_error(mod_a, mod_b, "test_private_function_access", "is private");
}

void test_public_field_access() {
    std::string mod_a = "module A; pub struct S { pub x: Int32 }";
    std::string mod_b = "module B; import A; func test(s: A::S) { let v: Int32 = s.x; }";
    run_visibility_test(mod_a, mod_b, "test_public_field_access");
}

void test_private_field_access() {
    std::string mod_a = "module A; pub struct S { x: Int32 }";
    std::string mod_b = "module B; import A; func test(s: A::S) { let v: Int32 = s.x; }";
    expect_visibility_error(mod_a, mod_b, "test_private_field_access", "field 'x' is private");
}

void test_same_module_private_access() {
    std::string mod_a = R"(
        module A;
        struct S { x: Int32 }
        func test(s: S) { let v: Int32 = s.x; }
    )";
    run_visibility_test(mod_a, "", "test_same_module_private_access");
}

int main() {
    test_public_function_access();
    test_private_function_access();
    test_public_field_access();
    test_private_field_access();
    test_same_module_private_access();
    std::cout << "All visibility tests passed.\n";
    return 0;
}
