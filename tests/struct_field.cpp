#include "ast/ast.h"
#include "lexer/token.h"
#include "semantic/resolver.h"
#include <cassert>
#include <iostream>

using namespace flux::ast;
using namespace flux::semantic;

void test_struct_field_access() {
    // Define struct: struct Point { x: Int32, y: Int32 }

    StructDecl point_decl("Point", {},
                          {{"x", "Int32", Visibility::Public}, {"y", "Int32", Visibility::Public}});

    // Create module and add struct

    Module mod;
    mod.structs.push_back(point_decl);

    // Create a struct literal: Point { x: 1, y: 2 }

    std::vector<FieldInit> field_inits;
    field_inits.push_back({"x", std::make_unique<NumberExpr>("1")});
    field_inits.push_back({"y", std::make_unique<NumberExpr>("2")});
    auto point_lit_ptr = std::make_unique<StructLiteralExpr>("Point", std::move(field_inits));

    // Field access: (Point { x: 1, y: 2 }).x
    auto field_access = std::make_unique<BinaryExpr>(flux::TokenKind::Dot, std::move(point_lit_ptr),
                                                     std::make_unique<IdentifierExpr>("x"));

    Resolver resolver;
    resolver.resolve(mod);
    FluxType t = resolver.type_of(*field_access);
    assert(t.kind == TypeKind::Int);
    assert(t.name == "Int32");
}

int main() {
    test_struct_field_access();
    std::cout << "Struct field access test passed.\n";
    return 0;
}
