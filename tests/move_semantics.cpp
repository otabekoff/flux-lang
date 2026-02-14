#include "ast/ast.h"
#include "lexer/diagnostic.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>
#include <memory>

using namespace flux;
using namespace flux::ast;
using namespace flux::semantic;

void test_explicit_move() {
    Resolver resolver;
    resolver.enter_scope();

    // let a: String
    resolver.current_scope_->declare(
        {"a", SymbolKind::Variable, false, false, false, Visibility::None, "", "String"});

    // move a
    auto a_expr = std::make_unique<IdentifierExpr>("a");
    auto move_expr = std::make_unique<MoveExpr>(std::move(a_expr));

    resolver.resolve_expression(*move_expr);

    // use a -> Error
    auto use_a = std::make_unique<IdentifierExpr>("a");
    try {
        resolver.resolve_expression(*use_a);
        assert(false && "Should have thrown use-after-move error");
    } catch (const DiagnosticError& e) {
        std::string msg = e.what();
        assert(msg.find("use of moved value 'a'") != std::string::npos);
    }

    resolver.exit_scope();
    std::cout << "test_explicit_move passed\n";
}

void test_implicit_move_assignment() {
    Resolver resolver;
    resolver.enter_scope();

    // let a: String
    resolver.current_scope_->declare(
        {"a", SymbolKind::Variable, false, false, false, Visibility::None, "", "String"});

    // let b: String = a
    auto let = std::make_unique<LetStmt>("b", "String", false, false,
                                         std::make_unique<IdentifierExpr>("a"));

    // resolve_statement checks compatibility and marks 'a' as moved
    // Mock type_of("a") returning String
    // We assume default resolver state with String available?
    // Resolver uses type_from_name. We need to ensure String is known or handled.
    // Builtin types are handled in type_from_name. String is builtin.

    // We need to ensure type_of(*init) works.
    // IdentifierExpr("a") -> looks up "a" -> returns "String"

    resolver.resolve_statement(*let);

    // use a -> Error
    auto use_a = std::make_unique<IdentifierExpr>("a");
    try {
        resolver.resolve_expression(*use_a);
        assert(false && "Should have thrown use-after-move error");
    } catch (const DiagnosticError& e) {
        std::string msg = e.what();
        assert(msg.find("use of moved value 'a'") != std::string::npos);
    }

    resolver.exit_scope();
    std::cout << "test_implicit_move_assignment passed\n";
}

void test_copy_semantics() {
    Resolver resolver;
    resolver.enter_scope();

    // let i: Int32
    resolver.current_scope_->declare(
        {"i", SymbolKind::Variable, false, false, false, Visibility::None, "", "Int32"});

    // let j: Int32 = i
    auto let = std::make_unique<LetStmt>("j", "Int32", false, false,
                                         std::make_unique<IdentifierExpr>("i"));

    resolver.resolve_statement(*let);

    // use i -> OK
    auto use_i = std::make_unique<IdentifierExpr>("i");
    FluxType t = resolver.type_of(*use_i);
    assert(t.name == "Int32");

    resolver.exit_scope();
    std::cout << "test_copy_semantics passed\n";
}

void test_revival() {
    Resolver resolver;
    resolver.enter_scope();

    // let mut a: String
    resolver.current_scope_->declare(
        {"a", SymbolKind::Variable, true, false, false, Visibility::None, "", "String"});

    // move a (manually)
    auto a_expr = std::make_unique<IdentifierExpr>("a");
    auto move_expr = std::make_unique<MoveExpr>(std::move(a_expr));
    resolver.resolve_expression(*move_expr);

    // check it is moved
    try {
        auto use_a = std::make_unique<IdentifierExpr>("a");
        resolver.resolve_expression(*use_a);
        assert(false && "Should be moved");
    } catch (...) {
    }

    // a = "new value" (Mock assignment)
    // Assignment logic:
    // 1. Check target
    // 2. Revival

    auto assign =
        std::make_unique<AssignStmt>(std::make_unique<IdentifierExpr>("a"),
                                     std::make_unique<StringExpr>("val"), TokenKind::Assign);

    resolver.resolve_statement(*assign);

    // use a -> OK
    auto use_a_again = std::make_unique<IdentifierExpr>("a");
    FluxType t = resolver.type_of(*use_a_again);
    assert(t.name == "String");

    resolver.exit_scope();
    std::cout << "test_revival passed\n";
}

void test_implicit_move_call() {
    Resolver resolver;
    resolver.enter_scope(); // main scope

    // func take(s: String)
    resolver.current_scope_->declare({"take",
                                      SymbolKind::Function,
                                      false,
                                      false,
                                      false,
                                      Visibility::None,
                                      "",
                                      "Void",
                                      {"String"}});

    // let a: String
    resolver.current_scope_->declare(
        {"a", SymbolKind::Variable, false, false, false, Visibility::None, "", "String"});

    // take(a)
    std::vector<ExprPtr> args;
    args.push_back(std::make_unique<IdentifierExpr>("a"));
    auto call =
        std::make_unique<CallExpr>(std::make_unique<IdentifierExpr>("take"), std::move(args));

    resolver.resolve_expression(*call);

    // use a -> Error
    auto use_a = std::make_unique<IdentifierExpr>("a");
    try {
        resolver.resolve_expression(*use_a);
        assert(false && "Should have thrown use-after-move error");
    } catch (const DiagnosticError& e) {
        std::string msg = e.what();
        assert(msg.find("use of moved value 'a'") != std::string::npos);
    }

    resolver.exit_scope();
    std::cout << "test_implicit_move_call passed\n";
}

void test_implicit_move_struct() {
    Resolver resolver;
    resolver.enter_scope();

    // struct Wrapper { val: String }
    // Declaring struct is handled by Resolver::resolve(Module), but we are using parts.
    // Struct literal type checking relies on struct_fields_.
    // We need to inject struct info into resolver manually?
    // Resolver implementation details are private.
    // But struct_fields_ is private.
    // However, Resolver::resolve(Module) populates them.

    Module mod;
    mod.structs.push_back(StructDecl("Wrapper", {}, {{"val", "String", Visibility::Public}}));

    resolver.resolve(mod); // This clears scopes and sets up builtins

    // declare 'a'
    resolver.current_scope_->declare(
        {"a", SymbolKind::Variable, false, false, false, Visibility::None, "", "String"});

    // Wrapper { val: a }
    std::vector<FieldInit> fields;
    fields.push_back({"val", std::make_unique<IdentifierExpr>("a")});
    auto lit = std::make_unique<StructLiteralExpr>("Wrapper", std::move(fields));

    resolver.resolve_expression(*lit);

    // use a -> Error
    auto use_a = std::make_unique<IdentifierExpr>("a");
    try {
        resolver.resolve_expression(*use_a);
        assert(false && "Should have thrown use-after-move error");
    } catch (const DiagnosticError& e) {
        std::string msg = e.what();
        assert(msg.find("use of moved value 'a'") != std::string::npos);
    }

    std::cout << "test_implicit_move_struct passed\n";
}

int main() {
    try {
        test_explicit_move();
        test_implicit_move_assignment();
        test_copy_semantics();
        test_revival();
        test_implicit_move_call();
        test_implicit_move_struct();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
