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

            // Numeric types
            KW_INT,
            KW_INTEGER,
            KW_SMALLINT,
            KW_BIGINT,
            KW_TINYINT,
            KW_INT128,
            KW_UINT8,
            KW_UINT16,
            KW_UINT32,
            KW_UINT64,
            KW_REAL,
            KW_FLOAT,
            KW_DOUBLE,
            KW_DECIMAL,
            KW_NUMERIC,
            KW_MONEY,

            // String types
            KW_CHAR,
            KW_CHARACTER,
            KW_VARCHAR,
            KW_TEXT,

            // Binary types
            KW_BINARY,
            KW_VARBINARY,
            KW_BLOB,
            KW_BYTEA,

            // Date/Time types
            KW_DATE,
            KW_TIME,
            KW_TIMESTAMP,
            KW_INTERVAL,

            // Boolean
            KW_BOOLEAN,
            KW_BOOL,

            // Special types
            KW_UUID,
            KW_JSON,
            KW_JSONB,
            KW_XML,
            KW_VECTOR,

            // Type conversion
            KW_CAST,
            KW_TRY_CAST,
            KW_AS,

            // Pattern matching
            KW_LIKE,
            KW_ILIKE,

            // Character set and collation
            KW_SET,
            KW_COLLATE,
            KW_COLLATION,
            KW_DEFAULT,

            // Timezone
            KW_ZONE,
            KW_WITH,
            KW_WITHOUT,
            KW_AT,

            // Transaction control (Phase 2 Task 2.6)
            KW_START,
            KW_TRANSACTION,
            KW_COMMIT,
            KW_ROLLBACK,
            KW_READ,
            KW_WRITE,
            KW_ONLY,
            KW_WAIT,
            KW_ISOLATION,
            KW_LEVEL,
            KW_COMMITTED,
            KW_SNAPSHOT,
            KW_STABILITY,
            KW_RESERVING,
            KW_SHARED,
            KW_PROTECTED,
            KW_FOR,
            KW_OUTSTANDING,

            // Database maintenance (Phase 3 Task 3.3)
            KW_SWEEP,
            KW_DATABASE,
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

            // Default constructor - initializes to safe state
            Token() : type(TokenType::ERROR), location(), length(0)
            {
                value.int_value = 0; // Initialize union to zero
            }

            // Constructor helpers
            static Token makeEOF(const SourceLocation &loc)
            {
                Token t;
                t.type = TokenType::END_OF_FILE;
                t.location = loc;
                t.length = 0;
                t.value.int_value = 0; // Initialize union
                return t;
            }

            static Token makeError(const SourceLocation &loc, uint32_t len)
            {
                Token t;
                t.type = TokenType::ERROR;
                t.location = loc;
                t.length = len;
                t.value.int_value = 0; // Initialize union
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
                t.value.int_value = 0; // Initialize union
                return t;
            }

            static Token makeOperator(const SourceLocation &loc, uint32_t len, TokenType opType)
            {
                Token t;
                t.type = opType;
                t.location = loc;
                t.length = len;
                t.value.int_value = 0; // Initialize union
                return t;
            }
        };

        // Convert token type to string for debugging
        const char *tokenTypeToString(TokenType type);

    } // namespace parser
} // namespace scratchbird