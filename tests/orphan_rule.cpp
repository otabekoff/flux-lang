#include "ast/ast.h"
#include "lexer/diagnostic.h"
#include "lexer/token.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_local_trait_local_type_pass() {
    // module test;
    // struct S {}
    // trait T {}
    // impl T for S {}

    StructDecl s_decl("S", {}, {});
    TraitDecl t_decl("T", {}, {});
    ImplBlock impl({}, "S", {});
    impl.trait_name = "T";

    Module mod;
    mod.name = "test";
    mod.structs.push_back(std::move(s_decl));
    mod.traits.push_back(std::move(t_decl));
    mod.impls.push_back(std::move(impl));

    Resolver resolver;
    resolver.resolve(mod);
    std::cout << "Local Trait, Local FluxType: PASS\n";
}

void test_local_trait_external_type_pass() {
    // module test;
    // trait T {}
    // impl T for Int32 {}

    TraitDecl t_decl("T", {}, {});
    ImplBlock impl({}, "Int32", {});
    impl.trait_name = "T";

    Module mod;
    mod.name = "test";
    mod.traits.push_back(std::move(t_decl));
    mod.impls.push_back(std::move(impl));

    Resolver resolver;
    resolver.resolve(mod);
    std::cout << "Local Trait, External FluxType: PASS\n";
}

void test_external_trait_local_type_pass() {
    // module test;
    // struct S {}
    // impl Display for S { func to_string(&self) -> String { ... } }

    StructDecl s_decl("S", {}, {});

    // Implement to_string for Display
    FunctionDecl to_string_fn;
    to_string_fn.name = "to_string";
    to_string_fn.params.push_back({"self", "&self"});
    to_string_fn.return_type = "String";
    to_string_fn.visibility = Visibility::Public;

    // Dummy body
    Block body;
    body.statements.push_back(std::make_unique<ReturnStmt>(std::make_unique<StringExpr>("S")));
    to_string_fn.body = std::move(body);
    to_string_fn.has_body = true;

    ImplBlock impl({}, "S", {});
    impl.trait_name = "Display";
    impl.methods.push_back(std::move(to_string_fn));

    Module mod;
    mod.name = "test";
    mod.structs.push_back(std::move(s_decl));
    mod.impls.push_back(std::move(impl));

    Resolver resolver;
    resolver.resolve(mod);
    std::cout << "External Trait, Local FluxType: PASS\n";
}

void test_external_trait_external_type_fail() {
    // module test;
    // impl Display for Int32 {}

    ImplBlock impl({}, "Int32", {});
    impl.trait_name = "Display";
    // Missing to_string method implementation, but orphan check should trigger first or
    // concurrently. Actually orphan check triggers before method checking in my implementation. But
    // trait method checking happens inside the same loop. The orphan check is at the top of the
    // loop.

    Module mod;
    mod.name = "test";
    mod.impls.push_back(std::move(impl));

    Resolver resolver;
    try {
        resolver.resolve(mod);
        assert(false && "Should have thrown for orphan rule violation");
    } catch (const flux::DiagnosticError& e) {
        std::string msg = e.what();
        if (msg.find("orphan rule violation") != std::string::npos) {
            std::cout << "External Trait, External FluxType: PASS (Caught expected error)\n";
        } else {
            std::cout << "Caught unexpected error: " << msg << "\n";
            assert(false && "Unexpected error message");
        }
    }
}

int main() {
    test_local_trait_local_type_pass();
    test_local_trait_external_type_pass();
    if (true) {
        // Only run if Display trait is available (Resolver updated)
        test_external_trait_local_type_pass();
        test_external_trait_external_type_fail();
    }
    return 0;
}
