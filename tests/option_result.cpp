#include "../src/semantic/resolver.h"
#include "../src/semantic/type.h"
#include <cassert>
#include <iostream>

using namespace flux::semantic;

void test_option_type() {
    Resolver resolver;
    FluxType t = resolver.type_from_name("Option<Int>");
    assert(t.kind == TypeKind::Option);
    assert(t.generic_args.size() == 1);
    assert(t.generic_args[0].kind == TypeKind::Int);
    std::cout << "Option<Int> test passed\n";
}

void test_result_type() {
    Resolver resolver;
    FluxType t = resolver.type_from_name("Result<Int, String>");
    assert(t.kind == TypeKind::Result);
    assert(t.generic_args.size() == 2);
    assert(t.generic_args[0].kind == TypeKind::Int);
    assert(t.generic_args[1].kind == TypeKind::String);
    std::cout << "Result<Int, String> test passed\n";
}

int main() {
    test_option_type();
    test_result_type();
    std::cout << "All Option/Result type tests passed!\n";
    return 0;
}
