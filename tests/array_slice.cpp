#include "ast/ast.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_array_type_resolution() {
    // [1, 2, 3] should resolve to [Int32;3]
    std::vector<ExprPtr> elems;
    elems.push_back(std::make_unique<NumberExpr>("1"));
    elems.push_back(std::make_unique<NumberExpr>("2"));
    elems.push_back(std::make_unique<NumberExpr>("3"));
    ArrayExpr arr(std::move(elems));
    Resolver resolver;
    FluxType t = resolver.type_of(arr);
    assert(t.kind == TypeKind::Array);
    assert(t.name == "[Int32;3]");
}

void test_array_type_error() {
    // [1, 2.0] should error (mixed types)
    std::vector<ExprPtr> elems;
    elems.push_back(std::make_unique<NumberExpr>("1"));
    elems.push_back(std::make_unique<NumberExpr>("2.0"));
    ArrayExpr arr(std::move(elems));
    Resolver resolver;
    try {
        resolver.type_of(arr);
        assert(false && "Should throw for mixed types");
    } catch (...) {
        // Expected
    }
}

void test_slice_type_resolution() {
    // [1,2,3][0:2] should resolve to [Int32]
    std::vector<ExprPtr> elems;
    elems.push_back(std::make_unique<NumberExpr>("1"));
    elems.push_back(std::make_unique<NumberExpr>("2"));
    elems.push_back(std::make_unique<NumberExpr>("3"));
    auto arr_ptr = std::make_unique<ArrayExpr>(std::move(elems));
    SliceExpr slice(std::move(arr_ptr), std::make_unique<NumberExpr>("0"),
                    std::make_unique<NumberExpr>("2"));
    Resolver resolver;
    FluxType t = resolver.type_of(slice);
    assert(t.kind == TypeKind::Slice);
    assert(t.name == "[Int32]");
}

void test_slice_type_error() {
    // Slicing non-array should error
    NumberExpr num("1");
    SliceExpr slice(std::make_unique<NumberExpr>("1"), std::make_unique<NumberExpr>("0"),
                    std::make_unique<NumberExpr>("2"));
    Resolver resolver;
    try {
        resolver.type_of(slice);
        assert(false && "Should throw for non-array slice");
    } catch (...) {
        // Expected
    }
}

int main() {
    test_array_type_resolution();
    test_array_type_error();
    test_slice_type_resolution();
    test_slice_type_error();
    std::cout << "Array/Slice tests passed.\n";
    return 0;
}
