#include "ast/ast.h"
#include "lexer/diagnostic.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_compound_assignment() {
    // let mut x: Int32 = 10;
    // x += 5;

    Module mod;
    FunctionDecl main_fn;
    main_fn.name = "main";
    main_fn.return_type = "Void";
    main_fn.has_body = true;

    main_fn.body.statements.push_back(
        std::make_unique<LetStmt>("x", "Int32", true, false, std::make_unique<NumberExpr>("10")));

    auto assign = std::make_unique<AssignStmt>(std::make_unique<IdentifierExpr>("x"),
                                               std::make_unique<NumberExpr>("5"),
                                               flux::TokenKind::PlusAssign);
    main_fn.body.statements.push_back(std::move(assign));
    mod.functions.push_back(std::move(main_fn));

    Resolver resolver;
    resolver.resolve(mod);
}

void test_compound_assignment_invalid_type() {
    // let mut s: String = "hi";
    // s += 5; // Should fail

    Module mod;
    mod.functions.emplace_back();
    auto& main_fn = mod.functions.back();
    main_fn.name = "main";
    main_fn.return_type = "Void";
    main_fn.has_body = true;

    main_fn.body.statements.push_back(
        std::make_unique<LetStmt>("s", "String", true, false, std::make_unique<StringExpr>("hi")));

    auto assign = std::make_unique<AssignStmt>(std::make_unique<IdentifierExpr>("s"),
                                               std::make_unique<NumberExpr>("5"),
                                               flux::TokenKind::PlusAssign);
    main_fn.body.statements.push_back(std::move(assign));

    Resolver resolver;
    bool caught = false;
    try {
        resolver.resolve(mod);
    } catch (const flux::DiagnosticError& e) {
        if (std::string(e.what()).find("compound assignment only allowed for numeric types") !=
            std::string::npos) {
            caught = true;
        }
    }
    assert(caught);
}

void test_error_location() {
    // let x: Int32 = "hi"; // Should show location 10:5

    Module mod;
    mod.functions.emplace_back();
    auto& main_fn = mod.functions.back();
    main_fn.name = "main";
    main_fn.return_type = "Void";
    main_fn.has_body = true;

    auto let =
        std::make_unique<LetStmt>("x", "Int32", false, false, std::make_unique<StringExpr>("hi"));
    let->line = 10;
    let->column = 5;

    main_fn.body.statements.push_back(std::move(let));

    Resolver resolver;
    try {
        resolver.resolve(mod);
        assert(false);
    } catch (const flux::DiagnosticError& e) {
        if (e.line() != 10 || e.column() != 5) {
            std::cerr << "Expected error at 10:5, but got " << e.line() << ":" << e.column()
                      << "\n";
            assert(false);
        }
    }
}

int main() {
    test_compound_assignment();
    test_compound_assignment_invalid_type();
    test_error_location();
    std::cout << "Hardening tests passed.\n";
    return 0;
}
