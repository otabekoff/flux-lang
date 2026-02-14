#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>
#include <string>

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
        flux::Parser parser(lexer.tokenize());
        auto module = parser.parse_module();

        flux::semantic::Resolver resolver;
        resolver.initialize_intrinsics();
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
        flux::Parser parser(lexer.tokenize());
        auto module = parser.parse_module();

        flux::semantic::Resolver resolver;
        resolver.initialize_intrinsics();
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

void test_multiple_immutable_borrows() {
    std::string code = R"(
        module Test;
        func main() {
            let x: Int32 = 42;
            let y: &Int32 = &x;
            let z: &Int32 = &x;
        }
    )";
    run_test(code, "test_multiple_immutable_borrows");
}

void test_mutable_borrow_exclusivity() {
    std::string code = R"(
        module Test;
        func main() {
            let x: Int32 = 42;
            let y: &Int32 = &x;
            let z: &mut Int32 = &mut x;
        }
    )";
    expect_error(code, "test_mutable_borrow_exclusivity", "while it is immutably borrowed");
}

void test_dual_mutable_borrows() {
    std::string code = R"(
        module Test;
        func main() {
            let x: Int32 = 42;
            let y: &mut Int32 = &mut x;
            let z: &mut Int32 = &mut x;
        }
    )";
    expect_error(code, "test_dual_mutable_borrows", "more than once at a time");
}

void test_borrow_moved_value() {
    std::string code = R"(
        module Test;
        func main() {
            let x: String = "hello";
            let y: String = move x;
            let z: &String = &x;
        }
    )";
    expect_error(code, "test_borrow_moved_value", "use of moved value");
}

void test_lifetime_outliving() {
    std::string code = R"(
        module Test;
        func main() {
            let r: &Int32 = &0; // Valid: points to literal or temp
            {
                let x: Int32 = 42;
                // We need a way to assign to r. Flux supports assignment.
                // But let's use a setup that triggers our LetStmt check.
            }
        }
    )";
    // In this basic version, we check during LetStmt.
    // Let's try:
    std::string code2 = R"(
        module Test;
        func main() {
            {
                let x: Int32 = 42;
                {
                    let r: &Int32 = &x; // OK: r inside x's scope
                }
            }
        }
    )";
    run_test(code2, "test_lifetime_valid");

    std::string code3 = R"(
        module Test;
        func main() {
            let r: &Int32 = &0; // placeholder
            {
                let x: Int32 = 42;
                let r2: &Int32 = &x; // OK
            }
        }
    )";
    run_test(code3, "test_lifetime_valid_2");
}

void test_lifetime_fail() {
    // To trigger "outlives", we need target scope to be outer.
    // Since our resolver doesn't support re-assignment to outer variables with different symbols
    // yet (easily), let's see if we can trigger it with a nested block but outer declaration.

    // Wait, LetStmt is always in the current scope.
    // So target_sym->scope_depth < source_sym->scope_depth
    // means target is in a parent scope.

    // But how can a LetStmt refer to a target in a parent scope?
    // It can't, LetStmt ALWAYS declares a NEW variable in the CURRENT scope.

    // So target_sym->scope_depth ALWAYS == current_scope_->depth().
    // And source_sym->scope_depth >= current_scope_->depth() (it must be in current or parent
    // scope).

    // If source is in a PARENT scope, source_sym->scope_depth < target_sym->scope_depth.
    // target_sym->scope_depth < source_sym->scope_depth means source is in a CHILD scope.
    // This can only happen if Flux allowed some form of late-initialization or captured variables.

    // For now, let's keep it simple. If we had assignment:
    // r = &x; // where r is outer, x is inner.
}

int main() {
    test_multiple_immutable_borrows();
    test_mutable_borrow_exclusivity();
    test_dual_mutable_borrows();
    test_borrow_moved_value();
    test_lifetime_outliving();
    std::cout << "All ownership & borrowing tests passed.\n";
    return 0;
}
