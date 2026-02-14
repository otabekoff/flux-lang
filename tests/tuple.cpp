#include "ast/ast.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>
#include <memory>

using namespace flux::ast;
using namespace flux::semantic;

void test_tuple_expr() {
    // (1, 2.0, true) should resolve to (Int32, Float64, Bool)
    std::vector<ExprPtr> elems;
    elems.push_back(std::make_unique<NumberExpr>("1"));
    elems.push_back(std::make_unique<NumberExpr>("2.0"));
    elems.push_back(std::make_unique<BoolExpr>(true));
    TupleExpr tuple(std::move(elems));
    Resolver resolver;
    FluxType t = resolver.type_of(tuple);
    assert(t.kind == TypeKind::Tuple);
    assert(t.name == "(Int32,Float64,Bool)");
}

int main() {
    test_tuple_expr();
    std::cout << "Tuple tests passed.\n";
    return 0;
}
