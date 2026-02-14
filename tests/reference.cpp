#include "ast/ast.h"
#include "lexer/token.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_immutable_reference() {
    // &1 should resolve to &Int32 (immutable)
    std::cout << "test_immutable_reference: start\n" << std::flush;
    NumberExpr num("1");
    std::cout << "after NumberExpr\n" << std::flush;
    UnaryExpr ref(flux::TokenKind::Amp, std::make_unique<NumberExpr>("1"));
    std::cout << "after UnaryExpr\n" << std::flush;
    Resolver resolver;
    std::cout << "after Resolver\n" << std::flush;
    FluxType t = resolver.type_of(ref);
    std::cout << "after type_of, kind=" << static_cast<int>(t.kind)
              << ", is_mut_ref=" << t.is_mut_ref << ", name=" << t.name << "\n"
              << std::flush;
    assert(t.kind == TypeKind::Ref);
    std::cout << "after assert kind==Ref\n" << std::flush;
    assert(!t.is_mut_ref);
    std::cout << "after assert !is_mut_ref\n" << std::flush;
    assert(t.name == "&Int32");
    std::cout << "after assert name==&Int32\n" << std::flush;
}

void test_mutable_reference() {
    // &mut 1 should resolve to &mut Int32 (mutable)
    std::cout << "test_mutable_reference: start\n" << std::flush;
    // &mut 1 should resolve to &mut Int32 (mutable)
    std::cout << "test_mutable_reference: start\n" << std::flush;
    // Use the is_mutable flag in UnaryExpr to represent &mut 1
    UnaryExpr ref(flux::TokenKind::Amp, std::make_unique<NumberExpr>("1"), true);
    std::cout << "after UnaryExpr\n" << std::flush;
    Resolver resolver;
    std::cout << "after Resolver\n" << std::flush;
    FluxType t = resolver.type_of(ref);
    std::cout << "after type_of, kind=" << static_cast<int>(t.kind)
              << ", is_mut_ref=" << t.is_mut_ref << ", name=" << t.name << "\n"
              << std::flush;
    assert(t.kind == TypeKind::Ref);
    std::cout << "after assert kind==Ref\n" << std::flush;
    assert(t.is_mut_ref);
    std::cout << "after assert is_mut_ref\n" << std::flush;
    assert(t.name == "&mut Int32");
    std::cout << "after assert name==&mut Int32\n" << std::flush;
}

int main() {
    test_immutable_reference();
    test_mutable_reference();
    std::cout << "Reference type tests passed.\n" << std::endl;
    return 0;
}
