#include "parser.h"
#include "lexer/diagnostic.h"

namespace flux {
/* =======================
   Operator precedence
   ======================= */

static int precedence(TokenKind kind) {
    switch (kind) {
    case TokenKind::PipePipe:
        return 5;

    case TokenKind::AmpAmp:
        return 6;

    case TokenKind::Pipe:
        return 7;

    case TokenKind::Caret:
        return 8;

    case TokenKind::Amp:
        return 9;

    case TokenKind::EqualEqual:
    case TokenKind::BangEqual:
        return 10;

    case TokenKind::Less:
    case TokenKind::LessEqual:
    case TokenKind::Greater:
    case TokenKind::GreaterEqual:
        return 20;

    case TokenKind::ShiftLeft:
    case TokenKind::ShiftRight:
        return 25;

    case TokenKind::Plus:
    case TokenKind::Minus:
        return 30;

    case TokenKind::Star:
    case TokenKind::Slash:
    case TokenKind::Percent:
        return 40;

    case TokenKind::DotDot:
        return 3;

    default:
        return -1;
    }
}

/* =======================
   Core helpers
   ======================= */

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

const Token& Parser::peek(std::size_t offset) const {
    if (current_ + offset >= tokens_.size())
        return tokens_.back();
    return tokens_[current_ + offset];
}

const Token& Parser::advance() {
    if (!is_at_end())
        current_++;
    return tokens_[current_ - 1];
}

bool Parser::is_at_end() const {
    return peek().kind == TokenKind::EndOfFile;
}

bool Parser::match(TokenKind kind) {
    if (peek().kind == kind) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match_keyword(const std::string& keyword) {
    if (peek().kind == TokenKind::Keyword && peek().lexeme == keyword) {
        advance();
        return true;
    }
    return false;
}

const Token& Parser::expect(TokenKind kind, const char* message) {
    if (peek().kind == kind)
        return advance();

    throw DiagnosticError(std::string(message), peek().line, peek().column);
}

bool Parser::check_visibility() {
    return peek().kind == TokenKind::Keyword &&
           (peek().lexeme == "pub" || peek().lexeme == "public" || peek().lexeme == "private");
}

/* =======================
   Expressions (Pratt)
   ======================= */

ast::ExprPtr Parser::parse_primary() {

    const Token& tok = peek();
    ast::ExprPtr expr;
    // Lambda/closure: |params| -> RetType { body }
    if (tok.kind == TokenKind::Pipe) {
        advance(); // consume '|'
        std::vector<ast::LambdaExpr::Param> params;
        // Parse parameter list
        if (peek().kind != TokenKind::Pipe) {
            do {
                const Token& param_tok = expect(TokenKind::Identifier, "expected parameter name");
                std::string param_type;
                if (match(TokenKind::Colon)) {
                    param_type = parse_type();
                } else {
                    param_type = "Unknown";
                }
                params.emplace_back(param_tok.lexeme, param_type);
            } while (match(TokenKind::Comma));
        }
        expect(TokenKind::Pipe, "expected '|' after lambda parameters");
        // Optional return type
        std::string ret_type;
        if (peek().kind == TokenKind::Arrow) {
            advance(); // consume '->'
            ret_type = parse_type();
        } else {
            ret_type = "Unknown";
        }
        // Body
        expect(TokenKind::LBrace, "expected '{' to start lambda body");
        ast::ExprPtr body = parse_expression(); // For now, parse a single expression as body
        expect(TokenKind::RBrace, "expected '}' after lambda body");
        return std::make_unique<ast::LambdaExpr>(std::move(params), std::move(ret_type),
                                                 std::move(body));
    }

    // Unary operators
    if (tok.kind == TokenKind::Minus || tok.kind == TokenKind::Bang || tok.kind == TokenKind::Amp ||
        tok.kind == TokenKind::Tilde) {
        const TokenKind op = tok.kind;
        advance();

        // Handle &mut
        if (op == TokenKind::Amp && peek().kind == TokenKind::Keyword && peek().lexeme == "mut") {
            advance(); // consume 'mut'
            ast::ExprPtr operand = this->parse_expression(50);
            return std::make_unique<ast::UnaryExpr>(op, std::move(operand), true);
        }

        ast::ExprPtr operand = this->parse_expression(50); // higher than any binary
        return std::make_unique<ast::UnaryExpr>(op, std::move(operand), false);
    }

    // 'not' keyword as unary
    if (tok.kind == TokenKind::Keyword && tok.lexeme == "not") {
        advance();
        ast::ExprPtr operand = this->parse_expression(50);
        return std::make_unique<ast::UnaryExpr>(TokenKind::Bang, std::move(operand));
    }

    // Grouping or tuple: ( expression ) or (expr1, expr2, ...)
    if (tok.kind == TokenKind::LParen) {
        advance(); // consume '('
        std::vector<ast::ExprPtr> elements;
        if (peek().kind != TokenKind::RParen) {
            elements.push_back(this->parse_expression());
            while (match(TokenKind::Comma)) {
                elements.push_back(this->parse_expression());
            }
        }
        expect(TokenKind::RParen, "expected ')' after tuple or grouping");
        if (elements.size() == 1) {
            expr = std::move(elements[0]);
        } else {
            expr = std::make_unique<ast::TupleExpr>(std::move(elements));
        }
    }
    // Array literal: [expr1, expr2, ...]
    else if (tok.kind == TokenKind::LBracket) {
        advance(); // consume '['
        std::vector<ast::ExprPtr> elements;
        if (peek().kind != TokenKind::RBracket) {
            elements.push_back(parse_expression());
            while (match(TokenKind::Comma)) {
                elements.push_back(parse_expression());
            }
        }
        expect(TokenKind::RBracket, "expected ']' after array literal");
        expr = std::make_unique<ast::ArrayExpr>(std::move(elements));
    }
    // Literals
    else if (tok.kind == TokenKind::Number) {
        advance();
        expr = std::make_unique<ast::NumberExpr>(tok.lexeme);
    } else if (tok.kind == TokenKind::String) {
        advance();
        expr = std::make_unique<ast::StringExpr>(tok.lexeme);
    } else if (tok.kind == TokenKind::Char) {
        advance();
        expr = std::make_unique<ast::CharExpr>(tok.lexeme);
    }
    // Move
    else if (tok.kind == TokenKind::Keyword && tok.lexeme == "move") {
        advance();
        return std::make_unique<ast::MoveExpr>(this->parse_expression(50));
    }
    // await
    else if (tok.kind == TokenKind::Keyword && tok.lexeme == "await") {
        advance();
        return std::make_unique<ast::AwaitExpr>(this->parse_expression(50));
    }
    // spawn
    else if (tok.kind == TokenKind::Keyword && tok.lexeme == "spawn") {
        advance();
        return std::make_unique<ast::SpawnExpr>(this->parse_expression(50));
    }
    // drop
    else if (tok.kind == TokenKind::Keyword && tok.lexeme == "drop") {
        advance();
        expect(TokenKind::LParen, "expected '(' after 'drop'");
        auto arg = parse_expression();
        expect(TokenKind::RParen, "expected ')' after drop argument");
        auto callee = std::make_unique<ast::IdentifierExpr>("drop");
        std::vector<ast::ExprPtr> args;
        args.push_back(std::move(arg));
        expr = std::make_unique<ast::CallExpr>(std::move(callee), std::move(args));
    }
    // panic
    else if (tok.kind == TokenKind::Keyword && tok.lexeme == "panic") {
        advance();
        expect(TokenKind::LParen, "expected '(' after 'panic'");
        auto arg = parse_expression();
        expect(TokenKind::RParen, "expected ')' after panic argument");
        auto callee = std::make_unique<ast::IdentifierExpr>("panic");
        std::vector<ast::ExprPtr> args;
        args.push_back(std::move(arg));
        expr = std::make_unique<ast::CallExpr>(std::move(callee), std::move(args));
    }
    // assert
    else if (tok.kind == TokenKind::Keyword && tok.lexeme == "assert") {
        advance();
        expect(TokenKind::LParen, "expected '(' after 'assert'");
        auto arg = parse_expression();
        expect(TokenKind::RParen, "expected ')' after assert argument");
        auto callee = std::make_unique<ast::IdentifierExpr>("assert");
        std::vector<ast::ExprPtr> args;
        args.push_back(std::move(arg));
        expr = std::make_unique<ast::CallExpr>(std::move(callee), std::move(args));
    }
    // Keywords (true/false/self/Self)
    else if (tok.kind == TokenKind::Keyword) {
        if (tok.lexeme == "true") {
            advance();
            expr = std::make_unique<ast::BoolExpr>(true);
        } else if (tok.lexeme == "false") {
            advance();
            expr = std::make_unique<ast::BoolExpr>(false);
        } else if (tok.lexeme == "self") {
            advance();
            expr = std::make_unique<ast::IdentifierExpr>("self");
        } else if (tok.lexeme == "Self") {
            advance();
            expr = std::make_unique<ast::IdentifierExpr>("Self");
        } else {
            throw DiagnosticError("expected expression", tok.line, tok.column);
        }
    }
    // Identifiers
    else if (tok.kind == TokenKind::Identifier) {
        advance();
        expr = std::make_unique<ast::IdentifierExpr>(tok.lexeme);
    } else {
        throw DiagnosticError("expected expression", tok.line, tok.column);
    }

    // Suffixes: ::, ., (, {, ?, as, [, slice
    while (true) {
        if (match(TokenKind::ColonColon)) {
            const std::string member =
                expect(TokenKind::Identifier, "expected member name after '::'").lexeme;
            expr = std::make_unique<ast::BinaryExpr>(TokenKind::ColonColon, std::move(expr),
                                                     std::make_unique<ast::IdentifierExpr>(member));
        } else if (match(TokenKind::Dot)) {
            const std::string member =
                expect(TokenKind::Identifier, "expected member name after '.'").lexeme;
            expr = std::make_unique<ast::BinaryExpr>(TokenKind::Dot, std::move(expr),
                                                     std::make_unique<ast::IdentifierExpr>(member));
        } else if (match(TokenKind::LParen)) {
            std::vector<ast::ExprPtr> args;
            if (peek().kind != TokenKind::RParen) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RParen, "expected ')' after arguments");
            expr = std::make_unique<ast::CallExpr>(std::move(expr), std::move(args));
        } else if (match(TokenKind::Question)) {
            // Error propagation: expr?
            expr = std::make_unique<ast::ErrorPropagationExpr>(std::move(expr));
        } else if (peek().kind == TokenKind::Less) {
            // Ambiguity: generic or less-than?
            if (auto* id = dynamic_cast<ast::IdentifierExpr*>(expr.get())) {
                std::string type = id->name;
                std::size_t saved = current_;
                advance(); // <
                type += "<";
                bool is_generic = true;
                try {
                    do {
                        type += parse_type();
                        if (match(TokenKind::Comma))
                            type += ", ";
                    } while (peek().kind != TokenKind::Greater && !is_at_end());
                    if (peek().kind == TokenKind::Greater) {
                        type += expect(TokenKind::Greater, "expected '>'").lexeme;
                    } else {
                        is_generic = false;
                    }
                } catch (...) {
                    is_generic = false;
                }

                if (is_generic) {
                    expr = std::make_unique<ast::IdentifierExpr>(type);
                } else {
                    current_ = saved;
                    break;
                }
            } else {
                break;
            }
        } else if (peek().kind == TokenKind::LBrace &&
                   (peek(1).kind == TokenKind::RBrace ||
                    (peek(1).kind == TokenKind::Identifier && peek(2).kind == TokenKind::Colon))) {
            // Struct literal: { } or { ident :
            advance(); // consume '{'
            std::vector<ast::FieldInit> fields;
            while (!match(TokenKind::RBrace)) {
                const std::string field_name =
                    expect(TokenKind::Identifier, "expected field name").lexeme;
                expect(TokenKind::Colon, "expected ':' after field name");
                ast::ExprPtr value = parse_expression();
                fields.push_back({field_name, std::move(value)});
                match(TokenKind::Comma);
            }

            std::string struct_name;
            if (auto* id = dynamic_cast<ast::IdentifierExpr*>(expr.get())) {
                struct_name = id->name;
            } else {
                struct_name = "<qualified-name>";
            }

            expr = std::make_unique<ast::StructLiteralExpr>(struct_name, std::move(fields));
        } else if (peek().kind == TokenKind::Keyword && peek().lexeme == "as") {
            advance(); // consume 'as'
            std::string target_type = parse_type();
            expr = std::make_unique<ast::CastExpr>(std::move(expr), target_type);
        } else if (peek().kind == TokenKind::LBracket) {
            // Slice: expr[start:end]
            advance(); // consume '['
            ast::ExprPtr start = nullptr;
            ast::ExprPtr end = nullptr;
            if (peek().kind != TokenKind::Colon && peek().kind != TokenKind::RBracket) {
                start = parse_expression();
            }
            if (match(TokenKind::Colon)) {
                if (peek().kind != TokenKind::RBracket) {
                    end = parse_expression();
                }
            }
            expect(TokenKind::RBracket, "expected ']' after slice");
            expr =
                std::make_unique<ast::SliceExpr>(std::move(expr), std::move(start), std::move(end));
        } else {
            break;
        }
    }

    return expr;
}

ast::ExprPtr Parser::parse_expression(int min_prec) {
    ast::ExprPtr left = parse_primary();

    while (true) {
        TokenKind op = peek().kind;
        int prec = precedence(op);

        // Handle 'and' / 'or' keyword operators
        if (peek().kind == TokenKind::Keyword) {
            if (peek().lexeme == "and") {
                op = TokenKind::AmpAmp;
                prec = precedence(TokenKind::AmpAmp);
            } else if (peek().lexeme == "or") {
                op = TokenKind::PipePipe;
                prec = precedence(TokenKind::PipePipe);
            }
        }

        if (prec < min_prec)
            break;

        advance(); // consume operator

        ast::ExprPtr right = parse_expression(prec + 1);

        left = std::make_unique<ast::BinaryExpr>(op, std::move(left), std::move(right));
    }

    return left;
}

/* =======================
   Top-level
   ======================= */

ast::Module Parser::parse_module() {
    ast::Module module;

    if (peek().kind == TokenKind::Keyword && peek().lexeme == "module") {
        advance();
        module.name = expect(TokenKind::Identifier, "expected module name").lexeme;
        expect(TokenKind::Semicolon, "expected ';' after module declaration");
    }

    while (peek().kind == TokenKind::Keyword && peek().lexeme == "import") {
        module.imports.push_back(parse_import());
    }

    while (!is_at_end()) {
        // Skip annotations
        while (peek().kind == TokenKind::Annotation) {
            advance();
        }

        bool is_public = false;
        if (check_visibility()) {
            if (peek().lexeme == "pub" || peek().lexeme == "public") {
                is_public = true;
            }
            advance();
        }

        if (peek().kind == TokenKind::Keyword) {
            if (peek().lexeme == "async") {
                advance();
                if (peek().kind == TokenKind::Keyword && peek().lexeme == "func") {
                    module.functions.push_back(parse_function(is_public, true));
                    continue;
                }
                throw DiagnosticError("expected 'func' after 'async'", peek().line, peek().column);
            }
            if (peek().lexeme == "func") {
                module.functions.push_back(parse_function(is_public));
                continue;
            }
            if (peek().lexeme == "struct") {
                module.structs.push_back(parse_struct_declaration(is_public));
                continue;
            }
            if (peek().lexeme == "class") {
                module.classes.push_back(parse_class_declaration(is_public));
                continue;
            }
            if (peek().lexeme == "enum") {
                module.enums.push_back(parse_enum_declaration(is_public));
                continue;
            }
            if (peek().lexeme == "impl") {
                module.impls.push_back(parse_impl_block());
                continue;
            }
            if (peek().lexeme == "trait") {
                module.traits.push_back(parse_trait_declaration(is_public));
                continue;
            }
            if (peek().lexeme == "type") {
                module.type_aliases.push_back(parse_type_alias(is_public));
                continue;
            }
        }
        throw DiagnosticError("expected top-level declaration (func, struct, "
                              "class, enum, impl, trait, type)",
                              peek().line, peek().column);
    }

    return module;
}

/* =======================
   Import
   ======================= */

ast::Import Parser::parse_import() {
    expect(TokenKind::Keyword, "expected 'import'");

    std::string path = expect(TokenKind::Identifier, "expected module name").lexeme;
    while (match(TokenKind::ColonColon)) {
        path += "::";
        path += expect(TokenKind::Identifier, "expected name after '::'").lexeme;
    }

    expect(TokenKind::Semicolon, "expected ';' after import");

    return ast::Import{std::move(path)};
}

/* =======================
   Function
   ======================= */

ast::FunctionDecl Parser::parse_function(bool is_public, bool is_async) {
    expect(TokenKind::Keyword, "expected 'func'");

    ast::FunctionDecl fn;
    fn.is_public = is_public;
    fn.is_async = is_async;
    fn.name = expect(TokenKind::Identifier, "expected function name").lexeme;
    fn.type_params = parse_type_params();

    expect(TokenKind::LParen, "expected '('");
    if (peek().kind != TokenKind::RParen) {
        do {
            std::string param_name;
            if (peek().kind == TokenKind::Keyword && peek().lexeme == "self") {
                param_name = "self";
                advance();
                // self doesn't need a type annotation
                if (match(TokenKind::Colon)) {
                    fn.params.push_back({param_name, parse_type()});
                } else {
                    fn.params.push_back({param_name, "Self"});
                }
            } else {
                param_name = expect(TokenKind::Identifier, "expected parameter name").lexeme;
                expect(TokenKind::Colon, "expected ':' after parameter name");
                fn.params.push_back({param_name, parse_type()});
            }
        } while (match(TokenKind::Comma));
    }
    expect(TokenKind::RParen, "expected ')'");

    expect(TokenKind::Arrow, "expected '->'");
    fn.return_type = parse_type();

    fn.where_clause = parse_where_clause();

    // Trait method signatures can end with ';' instead of a body
    if (match(TokenKind::Semicolon)) {
        // No body — trait method signature
        fn.body = ast::Block{};
    } else {
        fn.body = parse_block();
    }
    return fn;
}

/* =======================
   Block
   ======================= */

ast::Block Parser::parse_block() {
    ast::Block block;
    expect(TokenKind::LBrace, "expected '{'");

    while (!match(TokenKind::RBrace)) {
        block.statements.push_back(parse_statement());
    }

    return block;
}

/* =======================
   Statements
   ======================= */

ast::StmtPtr Parser::parse_statement() {
    if (peek().kind == TokenKind::Keyword) {
        if (peek().lexeme == "let" || peek().lexeme == "const") {
            return parse_let_statement();
        }
        if (peek().lexeme == "return") {
            advance();
            ast::ExprPtr expr;
            if (peek().kind != TokenKind::Semicolon) {
                expr = parse_expression();
            }
            expect(TokenKind::Semicolon, "expected ';' after return");
            return std::make_unique<ast::ReturnStmt>(std::move(expr));
        }
        if (peek().lexeme == "if") {
            return parse_if_statement();
        }
        if (peek().lexeme == "while") {
            return parse_while_statement();
        }
        if (peek().lexeme == "for") {
            return parse_for_statement();
        }
        if (peek().lexeme == "loop") {
            return parse_loop_statement();
        }
        if (peek().lexeme == "match") {
            return parse_match_statement();
        }
        if (peek().lexeme == "break") {
            advance();
            expect(TokenKind::Semicolon, "expected ';' after 'break'");
            return std::make_unique<ast::BreakStmt>();
        }
        if (peek().lexeme == "continue") {
            advance();
            expect(TokenKind::Semicolon, "expected ';' after 'continue'");
            return std::make_unique<ast::ContinueStmt>();
        }
    }

    // Skip annotations within blocks
    while (peek().kind == TokenKind::Annotation) {
        advance();
    }

    if (peek().kind == TokenKind::LBrace) {
        auto block_stmt = std::make_unique<ast::BlockStmt>();
        block_stmt->block = parse_block();
        return block_stmt;
    }

    // Try assignment: identifier followed by = or compound assignment
    if (peek().kind == TokenKind::Identifier) {
        std::size_t lookahead = 1;
        while (peek(lookahead).kind == TokenKind::Dot ||
               peek(lookahead).kind == TokenKind::ColonColon) {
            lookahead += 2; // skip . or :: and identifier
        }

        TokenKind assign_op = peek(lookahead).kind;
        if (assign_op == TokenKind::Assign || assign_op == TokenKind::PlusAssign ||
            assign_op == TokenKind::MinusAssign || assign_op == TokenKind::StarAssign ||
            assign_op == TokenKind::SlashAssign || assign_op == TokenKind::PercentAssign ||
            assign_op == TokenKind::AmpAssign || assign_op == TokenKind::PipeAssign ||
            assign_op == TokenKind::CaretAssign) {
            ast::ExprPtr target = parse_primary();
            TokenKind op = peek().kind;
            advance(); // consume assignment operator
            ast::ExprPtr value = parse_expression();
            expect(TokenKind::Semicolon, "expected ';' after assignment");
            return std::make_unique<ast::AssignStmt>(std::move(target), std::move(value), op);
        }
    }

    // Expression statement
    ast::ExprPtr expr = parse_expression();

    // Check for assignment after expression (for complex LHS targets)
    if (peek().kind == TokenKind::Assign || peek().kind == TokenKind::PlusAssign ||
        peek().kind == TokenKind::MinusAssign || peek().kind == TokenKind::StarAssign ||
        peek().kind == TokenKind::SlashAssign || peek().kind == TokenKind::PercentAssign) {
        TokenKind op = peek().kind;
        advance();
        ast::ExprPtr value = parse_expression();
        expect(TokenKind::Semicolon, "expected ';' after assignment");
        return std::make_unique<ast::AssignStmt>(std::move(expr), std::move(value), op);
    }

    expect(TokenKind::Semicolon, "expected ';' after expression");
    return std::make_unique<ast::ExprStmt>(std::move(expr));
}

ast::StmtPtr Parser::parse_let_statement() {
    bool is_const = false;
    bool is_mutable = false;

    if (peek().lexeme == "const") {
        advance();
        is_const = true;
    } else {
        expect(TokenKind::Keyword, "expected 'let'");
        if (peek().kind == TokenKind::Keyword && peek().lexeme == "mut") {
            advance();
            is_mutable = true;
        }
    }

    std::vector<std::string> tuple_names;
    std::string name;
    if (match(TokenKind::LParen)) {
        // Tuple destructuring: let (x, y): (T, U) = ...;
        do {
            name = expect(TokenKind::Identifier, "expected tuple variable name").lexeme;
            tuple_names.push_back(name);
        } while (match(TokenKind::Comma));
        expect(TokenKind::RParen, "expected ')' after tuple destructuring");
    } else {
        name = expect(TokenKind::Identifier, "expected variable name").lexeme;
    }
    std::string type_name;
    expect(TokenKind::Colon, "expected ':'");
    type_name = parse_type();
    expect(TokenKind::Assign, "expected '='");
    ast::ExprPtr initializer = parse_expression();
    expect(TokenKind::Semicolon, "expected ';'");
    if (!tuple_names.empty()) {
        return std::make_unique<ast::LetStmt>(std::move(tuple_names), std::move(type_name),
                                              is_mutable, is_const, std::move(initializer));
    } else {
        return std::make_unique<ast::LetStmt>(std::move(name), std::move(type_name), is_mutable,
                                              is_const, std::move(initializer));
    }
}

ast::StmtPtr Parser::parse_if_statement() {
    expect(TokenKind::Keyword, "expected 'if'");
    ast::ExprPtr condition = parse_expression();
    ast::StmtPtr then_branch = parse_statement();
    ast::StmtPtr else_branch = nullptr;

    if (peek().kind == TokenKind::Keyword && peek().lexeme == "else") {
        advance();
        else_branch = parse_statement();
    }

    return std::make_unique<ast::IfStmt>(std::move(condition), std::move(then_branch),
                                         std::move(else_branch));
}

ast::StmtPtr Parser::parse_while_statement() {
    expect(TokenKind::Keyword, "expected 'while'");
    ast::ExprPtr condition = parse_expression();
    ast::StmtPtr body = parse_statement();

    return std::make_unique<ast::WhileStmt>(std::move(condition), std::move(body));
}

ast::StmtPtr Parser::parse_for_statement() {
    expect(TokenKind::Keyword, "expected 'for'");

    std::string var_name = expect(TokenKind::Identifier, "expected loop variable name").lexeme;

    std::string var_type;
    if (match(TokenKind::Colon)) {
        var_type = parse_type();
    }

    if (!(peek().kind == TokenKind::Keyword && peek().lexeme == "in")) {
        throw DiagnosticError("expected 'in' after for loop variable", peek().line, peek().column);
    }
    advance(); // consume 'in'

    ast::ExprPtr iterable = parse_expression();
    ast::StmtPtr body = parse_statement();

    return std::make_unique<ast::ForStmt>(std::move(var_name), std::move(var_type),
                                          std::move(iterable), std::move(body));
}

ast::StmtPtr Parser::parse_loop_statement() {
    expect(TokenKind::Keyword, "expected 'loop'");
    ast::StmtPtr body = parse_statement();

    return std::make_unique<ast::LoopStmt>(std::move(body));
}

ast::StmtPtr Parser::parse_match_statement() {
    expect(TokenKind::Keyword, "expected 'match'");
    ast::ExprPtr expression = parse_expression();
    expect(TokenKind::LBrace, "expected '{' after match expression");

    std::vector<ast::MatchArm> arms;
    while (!match(TokenKind::RBrace)) {
        ast::PatternPtr pattern = parse_pattern();

        // Optional match guard: `if <condition>`
        ast::ExprPtr guard;
        if (peek().kind == TokenKind::Keyword && peek().lexeme == "if") {
            advance(); // consume 'if'
            guard = parse_expression();
        }

        expect(TokenKind::FatArrow, "expected '=>' after pattern");

        ast::StmtPtr body;
        if (peek().kind == TokenKind::LBrace) {
            body = std::make_unique<ast::BlockStmt>();
            dynamic_cast<ast::BlockStmt*>(body.get())->block = parse_block();
        } else {
            body = std::make_unique<ast::ExprStmt>(parse_expression());
        }

        arms.push_back({std::move(pattern), std::move(guard), std::move(body)});
        match(TokenKind::Comma); // Optional comma after arm
    }

    return std::make_unique<ast::MatchStmt>(std::move(expression), std::move(arms));
}

ast::PatternPtr Parser::parse_pattern() {
    const Token& tok = peek();

    if (tok.kind == TokenKind::Number || tok.kind == TokenKind::String ||
        (tok.kind == TokenKind::Keyword && (tok.lexeme == "true" || tok.lexeme == "false"))) {
        return std::make_unique<ast::LiteralPattern>(parse_primary());
    }

    // Negative number pattern
    if (tok.kind == TokenKind::Minus && peek(1).kind == TokenKind::Number) {
        advance(); // consume '-'
        auto num = parse_primary();
        auto neg = std::make_unique<ast::UnaryExpr>(TokenKind::Minus, std::move(num));
        return std::make_unique<ast::LiteralPattern>(std::move(neg));
    }

    if (tok.kind == TokenKind::Identifier) {
        if (tok.lexeme == "_") {
            advance();
            return std::make_unique<ast::WildcardPattern>();
        }

        std::string name = advance().lexeme;
        if (match(TokenKind::ColonColon)) {
            name += "::" + expect(TokenKind::Identifier, "expected variant name").lexeme;
            std::vector<ast::PatternPtr> sub_patterns;
            if (match(TokenKind::LParen)) {
                if (peek().kind != TokenKind::RParen) {
                    do {
                        sub_patterns.push_back(parse_pattern());
                    } while (match(TokenKind::Comma));
                }
                expect(TokenKind::RParen, "expected ')' after variant patterns");
            }
            return std::make_unique<ast::VariantPattern>(name, std::move(sub_patterns));
        }

        if (peek().kind == TokenKind::LParen) {
            advance(); // (
            std::vector<ast::PatternPtr> sub_patterns;
            if (peek().kind != TokenKind::RParen) {
                do {
                    sub_patterns.push_back(parse_pattern());
                } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RParen, "expected ')' after variant patterns");
            return std::make_unique<ast::VariantPattern>(name, std::move(sub_patterns));
        }

        return std::make_unique<ast::IdentifierPattern>(name);
    }

    throw DiagnosticError("expected pattern", tok.line, tok.column);
}

ast::StructDecl Parser::parse_struct_declaration(bool is_public) {
    expect(TokenKind::Keyword, "expected 'struct'");
    const std::string name = expect(TokenKind::Identifier, "expected struct name").lexeme;
    std::vector<std::string> type_params = parse_type_params();
    expect(TokenKind::LBrace, "expected '{'");

    std::vector<ast::Field> fields;
    while (!match(TokenKind::RBrace)) {
        std::string visibility;
        if (check_visibility()) {
            visibility = peek().lexeme;
            advance();
        }

        const std::string field_name = expect(TokenKind::Identifier, "expected field name").lexeme;
        expect(TokenKind::Colon, "expected ':'");
        std::string field_type = parse_type();
        fields.push_back({field_name, std::move(field_type), std::move(visibility)});
        match(TokenKind::Comma);
    }

    auto decl = ast::StructDecl{name, std::move(type_params), std::move(fields)};
    decl.is_public = is_public;
    return decl;
}

ast::ClassDecl Parser::parse_class_declaration(bool is_public) {
    expect(TokenKind::Keyword, "expected 'class'");
    const std::string name = expect(TokenKind::Identifier, "expected class name").lexeme;
    std::vector<std::string> type_params = parse_type_params();
    expect(TokenKind::LBrace, "expected '{'");

    std::vector<ast::Field> fields;
    while (!match(TokenKind::RBrace)) {
        std::string visibility;
        if (check_visibility()) {
            visibility = peek().lexeme;
            advance();
        }

        const std::string field_name = expect(TokenKind::Identifier, "expected field name").lexeme;
        expect(TokenKind::Colon, "expected ':'");
        std::string field_type = parse_type();
        fields.push_back({field_name, std::move(field_type), std::move(visibility)});
        match(TokenKind::Comma);
    }

    auto decl = ast::ClassDecl{name, std::move(type_params), std::move(fields)};
    decl.is_public = is_public;
    return decl;
}

ast::EnumDecl Parser::parse_enum_declaration(bool is_public) {
    expect(TokenKind::Keyword, "expected 'enum'");
    const std::string name = expect(TokenKind::Identifier, "expected enum name").lexeme;
    std::vector<std::string> type_params = parse_type_params();
    expect(TokenKind::LBrace, "expected '{'");

    std::vector<ast::Variant> variants;
    while (!match(TokenKind::RBrace)) {
        const std::string variant_name =
            expect(TokenKind::Identifier, "expected variant name").lexeme;
        std::vector<std::string> types;
        if (match(TokenKind::LParen)) {
            if (peek().kind != TokenKind::RParen) {
                do {
                    types.push_back(parse_type());
                } while (match(TokenKind::Comma));
            }
            expect(TokenKind::RParen, "expected ')'");
        }
        variants.push_back({variant_name, std::move(types)});
        match(TokenKind::Comma);
    }

    auto decl = ast::EnumDecl{name, std::move(type_params), std::move(variants)};
    decl.is_public = is_public;
    return decl;
}

ast::ImplBlock Parser::parse_impl_block() {
    expect(TokenKind::Keyword, "expected 'impl'");
    std::vector<std::string> type_params = parse_type_params();
    const std::string name = parse_type();

    std::string trait_name;
    if (peek().kind == TokenKind::Keyword && peek().lexeme == "for") {
        trait_name = name;
        advance();
    }

    std::string target_name;
    if (!trait_name.empty()) {
        target_name = parse_type();
    } else {
        target_name = name;
    }

    std::string where_clause = parse_where_clause();

    expect(TokenKind::LBrace, "expected '{'");

    std::vector<ast::FunctionDecl> methods;
    while (!match(TokenKind::RBrace)) {
        bool is_pub = false;
        if (check_visibility()) {
            if (peek().lexeme == "pub" || peek().lexeme == "public")
                is_pub = true;
            advance();
        }

        bool is_async_method = false;
        if (peek().kind == TokenKind::Keyword && peek().lexeme == "async") {
            advance();
            is_async_method = true;
        }

        methods.push_back(parse_function(is_pub, is_async_method));
    }

    auto impl = ast::ImplBlock{std::move(type_params), target_name, std::move(methods)};
    impl.trait_name = trait_name;
    impl.where_clause = std::move(where_clause);
    return impl;
}

ast::TraitDecl Parser::parse_trait_declaration(bool is_public) {
    expect(TokenKind::Keyword, "expected 'trait'");
    const std::string name = expect(TokenKind::Identifier, "expected trait name").lexeme;
    std::vector<std::string> type_params = parse_type_params();

    std::string where_clause = parse_where_clause();

    expect(TokenKind::LBrace, "expected '{'");

    std::vector<ast::FunctionDecl> methods;
    while (!match(TokenKind::RBrace)) {
        methods.push_back(parse_function());
    }

    auto decl = ast::TraitDecl{name, std::move(type_params), std::move(methods)};
    decl.is_public = is_public;
    return decl;
}

ast::TypeAlias Parser::parse_type_alias(bool is_public) {
    expect(TokenKind::Keyword, "expected 'type'");
    const std::string name = expect(TokenKind::Identifier, "expected type alias name").lexeme;
    expect(TokenKind::Assign, "expected '='");
    std::string target = parse_type();
    expect(TokenKind::Semicolon, "expected ';' after type alias");

    auto alias = ast::TypeAlias{name, std::move(target)};
    alias.is_public = is_public;
    return alias;
}

std::vector<std::string> Parser::parse_type_params() {
    std::vector<std::string> params;
    if (match(TokenKind::Less)) {
        do {
            std::string param =
                expect(TokenKind::Identifier, "expected type parameter name").lexeme;
            // Optional trait bound: T: Ord + Display
            if (match(TokenKind::Colon)) {
                param += ": ";
                param += expect(TokenKind::Identifier, "expected trait name").lexeme;
                while (match(TokenKind::Plus)) {
                    param += " + ";
                    param += expect(TokenKind::Identifier, "expected trait name").lexeme;
                }
            }
            params.push_back(std::move(param));
        } while (match(TokenKind::Comma));
        expect(TokenKind::Greater, "expected '>' after type parameters");
    }
    return params;
}

std::string Parser::parse_where_clause() {
    if (!(peek().kind == TokenKind::Keyword && peek().lexeme == "where"))
        return "";

    advance(); // consume 'where'
    std::string clause;

    while (peek().kind != TokenKind::LBrace && peek().kind != TokenKind::Semicolon &&
           !is_at_end()) {
        if (!clause.empty())
            clause += " ";
        clause += peek().lexeme;
        advance();
    }
    return clause;
}

std::string Parser::parse_type() {
    std::string type;
    if (match(TokenKind::Amp)) {
        type = "&";
        if (peek().kind == TokenKind::Keyword && peek().lexeme == "mut") {
            advance();
            type += "mut ";
        }
        type += parse_type();
        return type;
    }

    // Array type: [T; N]
    if (peek().kind == TokenKind::LBracket) {
        advance();
        std::string element_type = parse_type();
        expect(TokenKind::Semicolon, "expected ';' in array type");
        Token size_tok = expect(TokenKind::Number, "expected array size");
        expect(TokenKind::RBracket, "expected ']' after array size");
        return "[" + element_type + ";" + size_tok.lexeme + "]";
    }

    // Tuple type: (T1, T2, ...)
    if (peek().kind == TokenKind::LParen) {
        advance();
        type = "(";
        if (peek().kind != TokenKind::RParen) {
            do {
                type += parse_type();
                if (peek().kind == TokenKind::Comma) {
                    type += ", ";
                    advance();
                }
            } while (peek().kind != TokenKind::RParen && !is_at_end());
        }
        expect(TokenKind::RParen, "expected ')'");
        type += ")";

        if (match(TokenKind::Arrow)) {
            type += " -> ";
            type += parse_type();
        }

        return type;
    }

    const Token& tok = peek();
    if (tok.kind == TokenKind::Keyword || tok.kind == TokenKind::Identifier) {
        type = advance().lexeme;
        while (match(TokenKind::ColonColon)) {
            type += "::" + expect(TokenKind::Identifier, "expected name after '::'").lexeme;
        }

        if (match(TokenKind::Less)) {
            type += "<";
            do {
                type += parse_type();
                if (peek().kind == TokenKind::Comma) {
                    type += ", ";
                    advance();
                }
            } while (peek().kind != TokenKind::Greater && !is_at_end());
            type += expect(TokenKind::Greater, "expected '>'").lexeme;
        }

        return type;
    }

    throw DiagnosticError("expected type name", tok.line, tok.column);
}
} // namespace flux
