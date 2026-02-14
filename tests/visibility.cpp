#include "ast/ast.h"
#include "lexer/diagnostic.h"
#include "lexer/token.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_public_field_access() {
    // struct Data { pub x: Int32 }
    StructDecl data_decl("Data", {}, {{"x", "Int32", Visibility::Public}});
    data_decl.visibility = Visibility::Public;

    Module mod;
    mod.structs.push_back(std::move(data_decl));

    // (Data { x: 1 }).x
    std::vector<FieldInit> field_inits;
    field_inits.push_back({"x", std::make_unique<NumberExpr>("1")});
    auto lit = std::make_unique<StructLiteralExpr>("Data", std::move(field_inits));
    auto access = std::make_unique<BinaryExpr>(flux::TokenKind::Dot, std::move(lit),
                                               std::make_unique<IdentifierExpr>("x"));

    Resolver resolver;
    resolver.resolve(mod);
    FluxType t = resolver.type_of(*access);
    assert(t.kind == TypeKind::Int);
}

void test_private_field_access_fail() {
    // struct Data { x: Int32 } (private by default)
    StructDecl data_decl("Data", {}, {{"x", "Int32", Visibility::None}});
    data_decl.visibility = Visibility::Public;

    Module mod;
    mod.structs.push_back(std::move(data_decl));

    // (Data { x: 1 }).x
    std::vector<FieldInit> field_inits;
    field_inits.push_back({"x", std::make_unique<NumberExpr>("1")});
    auto lit = std::make_unique<StructLiteralExpr>("Data", std::move(field_inits));
    auto access = std::make_unique<BinaryExpr>(flux::TokenKind::Dot, std::move(lit),
                                               std::make_unique<IdentifierExpr>("x"));

    Resolver resolver;
    resolver.resolve(mod);

    try {
        resolver.type_of(*access);
        assert(false && "Should have thrown for private field access");
    } catch (const flux::DiagnosticError& e) {
        std::cout << "Caught expected error: " << e.what() << std::endl;
    }
}

void test_private_field_access_pass_in_method() {
    // struct Data { x: Int32 }
    // impl Data { func get_x(self) -> Int32 { return self.x; } }

    StructDecl data_decl("Data", {}, {{"x", "Int32", Visibility::None}});
    data_decl.visibility = Visibility::Public;

    // self.x
    auto self_expr = std::make_unique<IdentifierExpr>("self");
    auto access = std::make_unique<BinaryExpr>(flux::TokenKind::Dot, std::move(self_expr),
                                               std::make_unique<IdentifierExpr>("x"));
    auto ret_stmt = std::make_unique<ReturnStmt>(std::move(access));

    std::vector<std::unique_ptr<Stmt>> body_stmts;
    body_stmts.push_back(std::move(ret_stmt));
    Block body;
    body.statements = std::move(body_stmts);

    FunctionDecl get_x_fn;
    get_x_fn.name = "get_x";
    get_x_fn.params.push_back({"self", "Data"});
    get_x_fn.return_type = "Int32";
    get_x_fn.body = std::move(body);
    get_x_fn.has_body = true;
    get_x_fn.visibility = Visibility::Public;

    std::vector<FunctionDecl> methods;
    methods.push_back(std::move(get_x_fn));

    ImplBlock impl({}, "Data", std::move(methods));

    Module mod;
    mod.structs.push_back(std::move(data_decl));
    mod.impls.push_back(std::move(impl));

    Resolver resolver;
    resolver.resolve(mod);
    std::cout << "Private field access in method passed.\n";
}

void test_private_method_access_fail() {
    // struct Data {}
    // impl Data { private func secret(self) {} }

    StructDecl data_decl("Data", {}, {});
    data_decl.visibility = Visibility::Public;

    FunctionDecl secret_fn;
    secret_fn.name = "secret";
    secret_fn.params.push_back({"self", "Data"});
    secret_fn.visibility = Visibility::Private;
    secret_fn.body = Block({});
    secret_fn.has_body = true;

    std::vector<FunctionDecl> methods;
    methods.push_back(std::move(secret_fn));
    ImplBlock impl({}, "Data", std::move(methods));

    Module mod;
    mod.structs.push_back(std::move(data_decl));
    mod.impls.push_back(std::move(impl));

    // (Data {}).secret()
    ExprPtr lit = std::make_unique<StructLiteralExpr>("Data", std::vector<FieldInit>{});
    ExprPtr dot = std::make_unique<BinaryExpr>(flux::TokenKind::Dot, std::move(lit),
                                               std::make_unique<IdentifierExpr>("secret"));

    std::vector<ExprPtr> call_args;
    auto call = std::make_unique<CallExpr>(std::move(dot), std::move(call_args));

    Resolver resolver;
    resolver.resolve(mod);

    try {
        resolver.type_of(*call);
        assert(false && "Should have thrown for private method access");
    } catch (const flux::DiagnosticError& e) {
        std::cout << "Caught expected error: " << e.what() << std::endl;
    }
}

int main() {
    test_public_field_access();
    test_private_field_access_fail();
    test_private_field_access_pass_in_method();
    test_private_method_access_fail();
    std::cout << "All visibility tests passed.\n";
    return 0;
}
