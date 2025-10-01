#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <unordered_map>

namespace scratchbird
{
    namespace parser
    {

        // Token types for SQL lexer
        enum class TokenType : uint8_t
        {
            // Special tokens
            END_OF_FILE = 0,
            ERROR,

            // Literals
            INTEGER_LITERAL,
            FLOAT_LITERAL,
            STRING_LITERAL,

            // Identifiers and keywords
            IDENTIFIER,
            KEYWORD,

            // Operators and punctuation
            PLUS,    // +
            MINUS,   // -
            STAR,    // *
            SLASH,   // /
            PERCENT, // %

            EQUAL,         // =
            NOT_EQUAL,     // <>
            LESS_THAN,     // <
            GREATER_THAN,  // >
            LESS_EQUAL,    // <=
            GREATER_EQUAL, // >=

            LEFT_PAREN,  // (
            RIGHT_PAREN, // )
            COMMA,       // ,
            SEMICOLON,   // ;
            DOT,         // .

            // SQL Keywords (minimal set for Alpha)
            // These are detected post-lexing based on identifier content
            KW_CREATE,
            KW_TABLE,
            KW_INSERT,
            KW_INTO,
            KW_VALUES,
            KW_SELECT,
            KW_FROM,
            KW_WHERE,
            KW_NULL,
            KW_NOT,
            KW_INTEGER,
            KW_BIGINT,
            KW_DOUBLE,
            KW_VARCHAR,
        };

        // Location in source file
        struct SourceLocation
        {
            uint32_t line;
            uint32_t column;
            uint32_t offset; // Byte offset in source

            SourceLocation() : line(1), column(1), offset(0) {}
            SourceLocation(uint32_t l, uint32_t c, uint32_t o) : line(l), column(c), offset(o) {}
        };

        // String pool for interning
        class StringPool
        {
        public:
            using StringId = uint32_t;

            StringId intern(std::string_view str);
            std::string_view get(StringId id) const;
            void clear();

        private:
            std::vector<std::string> strings_;
            std::unordered_map<std::string_view, StringId> lookup_;
        };

        // Token structure
        struct Token
        {
            TokenType type;
            SourceLocation location;
            uint32_t length; // Length of token text

            union
            {
                int64_t int_value;              // For INTEGER_LITERAL
                double float_value;             // For FLOAT_LITERAL
                StringPool::StringId string_id; // For IDENTIFIER, STRING_LITERAL
                uint8_t keyword_code;           // For specific keywords
            } value;

            // Constructor helpers
            static Token makeEOF(const SourceLocation &loc)
            {
                Token t;
                t.type = TokenType::END_OF_FILE;
                t.location = loc;
                t.length = 0;
                return t;
            }

            static Token makeError(const SourceLocation &loc, uint32_t len)
            {
                Token t;
                t.type = TokenType::ERROR;
                t.location = loc;
                t.length = len;
                return t;
            }

            static Token makeInteger(const SourceLocation &loc, uint32_t len, int64_t val)
            {
                Token t;
                t.type = TokenType::INTEGER_LITERAL;
                t.location = loc;
                t.length = len;
                t.value.int_value = val;
                return t;
            }

            static Token makeFloat(const SourceLocation &loc, uint32_t len, double val)
            {
                Token t;
                t.type = TokenType::FLOAT_LITERAL;
                t.location = loc;
                t.length = len;
                t.value.float_value = val;
                return t;
            }

            static Token makeString(const SourceLocation &loc, uint32_t len,
                                    StringPool::StringId id)
            {
                Token t;
                t.type = TokenType::STRING_LITERAL;
                t.location = loc;
                t.length = len;
                t.value.string_id = id;
                return t;
            }

            static Token makeIdentifier(const SourceLocation &loc, uint32_t len,
                                        StringPool::StringId id)
            {
                Token t;
                t.type = TokenType::IDENTIFIER;
                t.location = loc;
                t.length = len;
                t.value.string_id = id;
                return t;
            }

            static Token makeKeyword(const SourceLocation &loc, uint32_t len, TokenType kwType)
            {
                Token t;
                t.type = kwType;
                t.location = loc;
                t.length = len;
                return t;
            }

            static Token makeOperator(const SourceLocation &loc, uint32_t len, TokenType opType)
            {
                Token t;
                t.type = opType;
                t.location = loc;
                t.length = len;
                return t;
            }
        };

        // Convert token type to string for debugging
        const char *tokenTypeToString(TokenType type);

    } // namespace parser
} // namespace scratchbird