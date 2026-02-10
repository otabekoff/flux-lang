#ifndef FLUX_LEXER_H
#define FLUX_LEXER_H

#include <string>
#include <vector>
#include "token.h"

namespace flux {
    class Lexer {
    public:
        explicit Lexer(std::string source);

        std::vector<Token> tokenize();

    private:
        char peek() const;

        char peek_next() const;

        char advance();

        bool is_at_end() const;

        std::string source_;
        std::size_t position_ = 0;
        std::size_t line_ = 1;
        std::size_t column_ = 1;
    };
} // namespace flux

#endif // FLUX_LEXER_H
