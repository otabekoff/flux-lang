#include "ast/ast.h"
#include "lexer/diagnostic.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_async_await_valid() {
    // async fn fetch() -> Int32 { return 42; }
    // async fn main() { let x = await fetch(); }

    Module mod;

    mod.functions.emplace_back();
    auto& fetch_fn = mod.functions.back();
    fetch_fn.name = "fetch";
    fetch_fn.return_type = "Int32";
    fetch_fn.is_async = true;
    fetch_fn.has_body = true;
    fetch_fn.body.statements.push_back(
        std::make_unique<ReturnStmt>(std::make_unique<NumberExpr>("42")));

    mod.functions.emplace_back();
    auto& main_fn = mod.functions.back();
    main_fn.name = "main";
    main_fn.return_type = "Void";
    main_fn.is_async = true;
    main_fn.has_body = true;

    // await fetch()
    std::vector<ExprPtr> args;
    auto call =
        std::make_unique<CallExpr>(std::make_unique<IdentifierExpr>("fetch"), std::move(args));
    ExprPtr await_expr = std::make_unique<AwaitExpr>(std::move(call));

    main_fn.body.statements.push_back(
        std::make_unique<LetStmt>("x", "Int32", false, false, std::move(await_expr)));

    Resolver resolver;
    try {
        resolver.resolve(mod);
    } catch (const flux::DiagnosticError& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        assert(false);
    }
}

void test_await_invalid_context() {
    // fn sync_fn() { await some_async_fn(); }

    Module mod;

    mod.functions.emplace_back();
    auto& some_async_fn = mod.functions.back();
    some_async_fn.name = "some_async_fn";
    some_async_fn.return_type = "Void";
    some_async_fn.is_async = true;

    mod.functions.emplace_back();
    auto& sync_fn = mod.functions.back();
    sync_fn.name = "sync_fn";
    sync_fn.return_type = "Void";
    sync_fn.is_async = false;
    sync_fn.has_body = true;

    std::vector<ExprPtr> args;
    auto call = std::make_unique<CallExpr>(std::make_unique<IdentifierExpr>("some_async_fn"),
                                           std::move(args));
    ExprPtr await_expr = std::make_unique<AwaitExpr>(std::move(call));
    sync_fn.body.statements.push_back(std::make_unique<ExprStmt>(std::move(await_expr)));

    Resolver resolver;
    bool caught = false;
    try {
        resolver.resolve(mod);
    } catch (const flux::DiagnosticError& e) {
        std::string msg = e.what();
        if (msg.find("'await' is only allowed inside an 'async' function") != std::string::npos) {
            caught = true;
        } else {
            std::cerr << "Wrong error: " << msg << "\n";
        }
    }
    assert(caught);
}

void test_spawn_valid() {
    // async fn task() {}
    // fn main() { spawn task(); }

    Module mod;

    mod.functions.emplace_back();
    auto& task_fn = mod.functions.back();
    task_fn.name = "task";
    task_fn.return_type = "Void";
    task_fn.is_async = true;

    mod.functions.emplace_back();
    auto& main_fn = mod.functions.back();
    main_fn.name = "main";
    main_fn.return_type = "Void";
    main_fn.has_body = true;

    std::vector<ExprPtr> args;
    auto call =
        std::make_unique<CallExpr>(std::make_unique<IdentifierExpr>("task"), std::move(args));
    ExprPtr spawn_expr = std::make_unique<SpawnExpr>(std::move(call));
    main_fn.body.statements.push_back(std::make_unique<ExprStmt>(std::move(spawn_expr)));

    Resolver resolver;
    resolver.resolve(mod);
}

int main() {
    test_async_await_valid();
    test_await_invalid_context();
    test_spawn_valid();
    std::cout << "Concurrency tests passed.\n";
    return 0;
}
