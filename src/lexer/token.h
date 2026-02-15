#ifndef FLUX_TOKEN_H
#define FLUX_TOKEN_H

#include <cstddef>
#include <string>

namespace flux {
enum class TokenKind {
    Identifier,
    Number,
    String,
    Char,
    Keyword,
    Annotation, // @test, @doc, etc.
    Pub,
    Public,
    Private,
    Extern,

    // punctuation
    Semicolon,   // ;
    Colon,       // :
    Comma,       // ,
    Dot,         // .
    DotDot,      // ..
    DotDotEqual, // ..=
    Ellipsis,    // ...
    LParen,      // (
    RParen,      // )
    LBrace,      // {
    RBrace,      // }
    LBracket,    // [
    RBracket,    // ]
    Arrow,       // ->
    FatArrow,    // =>
    ColonColon,  // ::

    // operators
    Bang,         // !
    Assign,       // =
    Plus,         // +
    Minus,        // -
    Star,         // *
    Slash,        // /
    Percent,      // %
    EqualEqual,   // ==
    BangEqual,    // !=
    Less,         // <
    LessEqual,    // <=
    Greater,      // >
    GreaterEqual, // >=
    AmpAmp,       // &&
    PipePipe,     // ||
    Amp,          // &
    Pipe,         // |
    Caret,        // ^
    Tilde,        // ~
    Question,     // ?
    ShiftLeft,    // <<
    ShiftRight,   // >>

    // compound assignment
    PlusAssign,    // +=
    MinusAssign,   // -=
    StarAssign,    // *=
    SlashAssign,   // /=
    PercentAssign, // %=
    AmpAssign,     // &=
    PipeAssign,    // |=
    CaretAssign,   // ^=

    EndOfFile,
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

inline const char* to_string(TokenKind kind) {
    switch (kind) {
    case TokenKind::Identifier:
        return "Identifier";
    case TokenKind::Number:
        return "Number";
    case TokenKind::String:
        return "String";
    case TokenKind::Char:
        return "Char";
    case TokenKind::Keyword:
        return "Keyword";
    case TokenKind::Annotation:
        return "Annotation";
    case TokenKind::Pub:
        return "Pub";
    case TokenKind::Public:
        return "Public";
    case TokenKind::Private:
        return "Private";
    case TokenKind::Extern:
        return "Extern";

    case TokenKind::Semicolon:
        return "Semicolon";
    case TokenKind::Colon:
        return "Colon";
    case TokenKind::Comma:
        return "Comma";
    case TokenKind::Dot:
        return "Dot";
    case TokenKind::DotDot:
        return "DotDot";
    case TokenKind::DotDotEqual:
        return "DotDotEqual";
    case TokenKind::Ellipsis:
        return "Ellipsis";
    case TokenKind::LParen:
        return "LParen";
    case TokenKind::RParen:
        return "RParen";
    case TokenKind::LBrace:
        return "LBrace";
    case TokenKind::RBrace:
        return "RBrace";
    case TokenKind::LBracket:
        return "LBracket";
    case TokenKind::RBracket:
        return "RBracket";
    case TokenKind::Arrow:
        return "Arrow";
    case TokenKind::FatArrow:
        return "FatArrow";
    case TokenKind::ColonColon:
        return "ColonColon";

    case TokenKind::Bang:
        return "Bang";
    case TokenKind::Assign:
        return "Assign";
    case TokenKind::Plus:
        return "Plus";
    case TokenKind::Minus:
        return "Minus";
    case TokenKind::Star:
        return "Star";
    case TokenKind::Slash:
        return "Slash";
    case TokenKind::Percent:
        return "Percent";
    case TokenKind::EqualEqual:
        return "EqualEqual";
    case TokenKind::BangEqual:
        return "BangEqual";
    case TokenKind::Less:
        return "Less";
    case TokenKind::LessEqual:
        return "LessEqual";
    case TokenKind::Greater:
        return "Greater";
    case TokenKind::GreaterEqual:
        return "GreaterEqual";
    case TokenKind::AmpAmp:
        return "AmpAmp";
    case TokenKind::PipePipe:
        return "PipePipe";
    case TokenKind::Amp:
        return "Amp";
    case TokenKind::Pipe:
        return "Pipe";
    case TokenKind::Caret:
        return "Caret";
    case TokenKind::Tilde:
        return "Tilde";
    case TokenKind::Question:
        return "Question";
    case TokenKind::ShiftLeft:
        return "ShiftLeft";
    case TokenKind::ShiftRight:
        return "ShiftRight";

    case TokenKind::PlusAssign:
        return "PlusAssign";
    case TokenKind::MinusAssign:
        return "MinusAssign";
    case TokenKind::StarAssign:
        return "StarAssign";
    case TokenKind::SlashAssign:
        return "SlashAssign";
    case TokenKind::PercentAssign:
        return "PercentAssign";
    case TokenKind::AmpAssign:
        return "AmpAssign";
    case TokenKind::PipeAssign:
        return "PipeAssign";
    case TokenKind::CaretAssign:
        return "CaretAssign";

    case TokenKind::EndOfFile:
        return "EndOfFile";
    default:
        return "Unknown";
    }
}
} // namespace flux

#endif // FLUX_TOKEN_H
