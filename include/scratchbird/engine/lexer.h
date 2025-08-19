#ifndef SCRATCHBIRD_ENGINE_LEXER_H
#define SCRATCHBIRD_ENGINE_LEXER_H

#include "scratchbird/engine/source_span.h"

#include <string>
#include <string_view>
#include <vector>

namespace scratchbird::engine
{

    enum class TokenKind {
        Identifier,
        QuotedIdentifier,
        Integer,
        Decimal,
        String,
        Date,
        Time,
        Timestamp,
        Uuid,
        Symbol,
        Keyword,
        End
    };

    struct Token {
        TokenKind kind;
        std::string text;
        SourceSpan span{};
        std::string charset; // non-empty for N'..' or _CHARSET'..'
    };

    class Lexer
    {
      public:
        explicit Lexer(std::string_view input);
        std::vector<Token> lex();

      private:
        std::string_view s;
        size_t i;
        void skip_ws_and_comments();
        bool is_ident_start(char c);
        bool is_ident_cont(char c);
        Token lex_ident_or_kw();
        Token lex_quoted_ident();
        Token lex_number();
        Token lex_string();
        Token lex_dollar_string();
        Token lex_symbol();
    };

    // Lexer diagnostics (thread-local)
    void lexer_clear_warnings();
    const std::vector<std::string>& lexer_warnings();

} // namespace scratchbird::engine

#endif // SCRATCHBIRD_ENGINE_LEXER_H
