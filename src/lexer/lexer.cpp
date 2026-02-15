// #include <cctype>
#include "lexer.h"
#include "diagnostic.h"

#include <unordered_set>

namespace flux {
const std::unordered_set<std::string> keywords = {
    // declarations
    "module",
    "import",
    "func",
    "let",
    "const",
    "return",
    "mut",
    "struct",
    "class",
    "enum",
    "trait",
    "impl",
    "type",
    "use",

    // control flow
    "if",
    "else",
    "while",
    "for",
    "in",
    "match",
    "loop",
    "break",
    "continue",

    // ownership & borrowing
    "move",
    "ref",
    "drop",

    // concurrency
    "async",
    "await",
    "spawn",

    // visibility
    "pub",
    "public",
    "private",

    // safety
    "unsafe",

    // logic & type operations
    "and",
    "or",
    "not",
    "as",
    "is",
    "where",

    // self/Self
    "self",
    "Self",

    // literals & types
    "true",
    "false",
    "Void",
    "Never",

    // error handling
    "panic",
    "assert",

    // built-in type names (keywords)
    "Int8",
    "Int16",
    "Int32",
    "Int64",
    "Int128",
    "UInt8",
    "UInt16",
    "UInt32",
    "UInt64",
    "UInt128",
    "IntPtr",
    "UIntPtr",
    "Float32",
    "Float64",
    "String",
    "Bool",
    "Char",
};

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

char Lexer::peek() const {
    if (is_at_end())
        return '\0';
    return source_[position_];
}

char Lexer::peek_next() const {
    if (position_ + 1 >= source_.size())
        return '\0';
    return source_[position_ + 1];
}

char Lexer::advance() {
    if (is_at_end())
        return '\0';

    char c = source_[position_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }

    return c;
}

// ⚠️ Note: line handling is not implemented yet (we’ll fix that).

bool Lexer::is_at_end() const {
    return position_ >= source_.size();
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!is_at_end()) {
        const char c = peek();

        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c))) {
            const std::size_t start_col = column_;
            std::string number;

            if (c == '0') {
                number.push_back(advance());
                char next = peek();
                if (next == 'x' || next == 'X') {
                    number.push_back(advance()); // consume x
                    while (!is_at_end() &&
                           (std::isxdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
                        char n = advance();
                        if (n != '_')
                            number.push_back(n);
                    }
                } else if (next == 'b' || next == 'B') {
                    number.push_back(advance()); // consume b
                    while (!is_at_end() && (peek() == '0' || peek() == '1' || peek() == '_')) {
                        char n = advance();
                        if (n != '_')
                            number.push_back(n);
                    }
                } else if (next == 'o' || next == 'O') {
                    number.push_back(advance()); // consume o
                    while (!is_at_end() && ((peek() >= '0' && peek() <= '7') || peek() == '_')) {
                        char n = advance();
                        if (n != '_')
                            number.push_back(n);
                    }
                } else {
                    // Just a zero or decimal starting with zero
                    while (!is_at_end() &&
                           (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
                        char n = advance();
                        if (n != '_')
                            number.push_back(n);
                    }
                }
            } else {
                while (!is_at_end() &&
                       (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
                    char n = advance();
                    if (n != '_')
                        number.push_back(n);
                }
            }

            if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peek_next()))) {
                number.push_back(advance()); // consume '.'
                while (!is_at_end() &&
                       (std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_')) {
                    char n = advance();
                    if (n != '_')
                        number.push_back(n);
                }
            }

            tokens.push_back({TokenKind::Number, number, line_, start_col});
            continue;
        }

        // Identifiers / keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            const std::size_t start_col = column_;
            std::string ident;

            while (!is_at_end() &&
                   (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                ident.push_back(advance());
            }

            TokenKind kind = TokenKind::Identifier;
            if (ident == "pub")
                kind = TokenKind::Pub;
            else if (ident == "public")
                kind = TokenKind::Public;
            else if (ident == "private")
                kind = TokenKind::Private;
            else if (ident == "extern")
                kind = TokenKind::Extern;
            else if (keywords.contains(ident))
                kind = TokenKind::Keyword;

            tokens.push_back({kind, ident, line_, start_col});
            continue;
        }

        // Operators & punctuation (longest match first)
        switch (c) {
        case ';':
            advance();
            tokens.push_back({TokenKind::Semicolon, ";", line_, column_ - 1});
            continue;

        case ':':
            advance();
            if (peek() == ':') {
                advance();
                tokens.push_back({TokenKind::ColonColon, "::", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Colon, ":", line_, column_ - 1});
            }
            continue;

        case ',':
            advance();
            tokens.push_back({TokenKind::Comma, ",", line_, column_ - 1});
            continue;

        case '.': {
            advance();
            if (peek() == '.') {
                advance();
                if (peek() == '.') {
                    advance();
                    tokens.push_back({TokenKind::Ellipsis, "...", line_, column_ - 3});
                } else if (peek() == '=') {
                    advance();
                    tokens.push_back({TokenKind::DotDotEqual, "..=", line_, column_ - 3});
                } else {
                    tokens.push_back({TokenKind::DotDot, "..", line_, column_ - 2});
                }
            } else {
                tokens.push_back({TokenKind::Dot, ".", line_, column_ - 1});
            }
            continue;
        }

        case '(':
            advance();
            tokens.push_back({TokenKind::LParen, "(", line_, column_ - 1});
            continue;

        case ')':
            advance();
            tokens.push_back({TokenKind::RParen, ")", line_, column_ - 1});
            continue;

        case '[':
            advance();
            tokens.push_back({TokenKind::LBracket, "[", line_, column_ - 1});
            continue;

        case ']':
            advance();
            tokens.push_back({TokenKind::RBracket, "]", line_, column_ - 1});
            continue;

        case '{':
            advance();
            tokens.push_back({TokenKind::LBrace, "{", line_, column_ - 1});
            continue;

        case '}':
            advance();
            tokens.push_back({TokenKind::RBrace, "}", line_, column_ - 1});
            continue;

        case '-':
            if (peek_next() == '>') {
                advance();
                advance();
                tokens.push_back({TokenKind::Arrow, "->", line_, column_ - 2});
            } else if (peek_next() == '=') {
                advance();
                advance();
                tokens.push_back({TokenKind::MinusAssign, "-=", line_, column_ - 2});
            } else {
                advance();
                tokens.push_back({TokenKind::Minus, "-", line_, column_ - 1});
            }
            continue;

        case '=':
            if (peek_next() == '=') {
                advance();
                advance();
                tokens.push_back({TokenKind::EqualEqual, "==", line_, column_ - 2});
            } else if (peek_next() == '>') {
                advance();
                advance();
                tokens.push_back({TokenKind::FatArrow, "=>", line_, column_ - 2});
            } else {
                advance();
                tokens.push_back({TokenKind::Assign, "=", line_, column_ - 1});
            }
            continue;

        case '&':
            advance();
            if (peek() == '&') {
                advance();
                tokens.push_back({TokenKind::AmpAmp, "&&", line_, column_ - 2});
            } else if (peek() == '=') {
                advance();
                tokens.push_back({TokenKind::AmpAssign, "&=", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Amp, "&", line_, column_ - 1});
            }
            continue;

        case '|':
            advance();
            if (peek() == '|') {
                advance();
                tokens.push_back({TokenKind::PipePipe, "||", line_, column_ - 2});
            } else if (peek() == '=') {
                advance();
                tokens.push_back({TokenKind::PipeAssign, "|=", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Pipe, "|", line_, column_ - 1});
            }
            continue;

        case '^':
            advance();
            if (peek() == '=') {
                advance();
                tokens.push_back({TokenKind::CaretAssign, "^=", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Caret, "^", line_, column_ - 1});
            }
            continue;

        case '~':
            advance();
            tokens.push_back({TokenKind::Tilde, "~", line_, column_ - 1});
            continue;

        case '?':
            advance();
            tokens.push_back({TokenKind::Question, "?", line_, column_ - 1});
            continue;

        case '!':
            if (peek_next() == '=') {
                advance();
                advance();
                tokens.push_back({TokenKind::BangEqual, "!=", line_, column_ - 2});
            } else {
                advance();
                tokens.push_back({TokenKind::Bang, "!", line_, column_ - 1});
            }
            continue;

        case '<':
            if (peek_next() == '=') {
                advance();
                advance();
                tokens.push_back({TokenKind::LessEqual, "<=", line_, column_ - 2});
            } else if (peek_next() == '<') {
                advance();
                advance();
                tokens.push_back({TokenKind::ShiftLeft, "<<", line_, column_ - 2});
            } else {
                advance();
                tokens.push_back({TokenKind::Less, "<", line_, column_ - 1});
            }
            continue;

        case '>':
            if (peek_next() == '=') {
                advance();
                advance();
                tokens.push_back({TokenKind::GreaterEqual, ">=", line_, column_ - 2});
            } else if (peek_next() == '>') {
                advance();
                advance();
                tokens.push_back({TokenKind::ShiftRight, ">>", line_, column_ - 2});
            } else {
                advance();
                tokens.push_back({TokenKind::Greater, ">", line_, column_ - 1});
            }
            continue;

        case '+':
            advance();
            if (peek() == '=') {
                advance();
                tokens.push_back({TokenKind::PlusAssign, "+=", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Plus, "+", line_, column_ - 1});
            }
            continue;

        case '*':
            advance();
            if (peek() == '=') {
                advance();
                tokens.push_back({TokenKind::StarAssign, "*=", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Star, "*", line_, column_ - 1});
            }
            continue;

        case '/':
            advance();
            if (peek() == '/') {
                advance();
                while (!is_at_end() && peek() != '\n') {
                    advance();
                }
            } else if (peek() == '=') {
                advance();
                tokens.push_back({TokenKind::SlashAssign, "/=", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Slash, "/", line_, column_ - 1});
            }
            continue;

        case '%':
            advance();
            if (peek() == '=') {
                advance();
                tokens.push_back({TokenKind::PercentAssign, "%=", line_, column_ - 2});
            } else {
                tokens.push_back({TokenKind::Percent, "%", line_, column_ - 1});
            }
            continue;

        case '@': {
            advance();
            std::size_t start_col = column_ - 1;
            std::string annotation = "@";
            while (!is_at_end() &&
                   (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
                annotation.push_back(advance());
            }
            tokens.push_back({TokenKind::Annotation, annotation, line_, start_col});
            continue;
        }

        case '"': {
            advance();
            std::size_t start_col = column_ - 1;
            std::string literal;
            while (!is_at_end() && peek() != '"') {
                if (peek() == '\n') {
                    throw DiagnosticError("Unterminated string literal", line_, column_);
                }
                literal.push_back(advance());
            }

            if (is_at_end()) {
                throw DiagnosticError("Unterminated string literal", line_, column_);
            }

            advance(); // Consume closing quote
            tokens.push_back({TokenKind::String, literal, line_, start_col});
            continue;
        }

        case '\'': {
            advance();
            std::size_t start_col = column_ - 1;
            std::string literal;
            if (peek() == '\\') {
                literal.push_back(advance());
                if (!is_at_end()) {
                    literal.push_back(advance());
                }
            } else if (!is_at_end() && peek() != '\'') {
                literal.push_back(advance());
            }

            if (peek() != '\'') {
                throw DiagnosticError("Unterminated character literal", line_, column_);
            }
            advance(); // consume '
            tokens.push_back({TokenKind::Char, literal, line_, start_col});
            continue;
        }
        }

        // Unknown character
        std::size_t err_line = line_;
        std::size_t err_col = column_;
        char bad = advance();

        throw DiagnosticError(std::string("unexpected character '") + bad + "'", err_line, err_col);
    }

    tokens.push_back({TokenKind::EndOfFile, "", line_, column_});

    return tokens;
}
} // namespace flux
