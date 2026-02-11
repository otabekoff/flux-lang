#include "ast/ast.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include <cassert>
#include <iostream>
#include <vector>

using namespace flux;
using namespace flux::ast;

void test_parse_lambda() {
    // Tokens for: |x, y| -> Int32 { x }
    std::vector<Token> tokens = {
        {TokenKind::Pipe, "|", 1, 1},           {TokenKind::Identifier, "x", 1, 2},
        {TokenKind::Comma, ",", 1, 3},          {TokenKind::Identifier, "y", 1, 4},
        {TokenKind::Pipe, "|", 1, 5},           {TokenKind::Arrow, "->", 1, 6},
        {TokenKind::Identifier, "Int32", 1, 7}, {TokenKind::LBrace, "{", 1, 8},
        {TokenKind::Identifier, "x", 1, 9},     {TokenKind::RBrace, "}", 1, 10},
        {TokenKind::EndOfFile, "", 1, 11}};
    Parser parser(tokens);
    ExprPtr expr = parser.parse_expression();
    auto* lambda = dynamic_cast<LambdaExpr*>(expr.get());
    (void)lambda;
    assert(lambda && "Should parse a LambdaExpr");
    assert(lambda->params.size() == 2);
    assert(lambda->params[0].name == "x");
    assert(lambda->params[1].name == "y");
    std::cout << "Lambda parsing test passed.\n";
}

int main() {
    test_parse_lambda();
    return 0;
}
