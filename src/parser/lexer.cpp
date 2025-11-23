#include "scratchbird/parser/lexer.h"
#include "scratchbird/core/utf8_utils.h"
#include <cctype>
#include <charconv>
#include <algorithm>
#include <iostream>

namespace scratchbird
{
    namespace parser
    {

        // Keyword table (case-insensitive)
        struct KeywordEntry
        {
            const char *text;
            TokenType type;
        };

        static const KeywordEntry KEYWORDS[] = {
            // SQL keywords
            {"CREATE", TokenType::KW_CREATE},
            {"TABLE", TokenType::KW_TABLE},
            {"INDEX", TokenType::KW_INDEX},     // Phase 2 Task 2.3
            {"UNIQUE", TokenType::KW_UNIQUE},   // Phase 2 Task 2.3
            {"INSERT", TokenType::KW_INSERT},
            {"INTO", TokenType::KW_INTO},
            {"VALUES", TokenType::KW_VALUES},
            {"SELECT", TokenType::KW_SELECT},
            {"FROM", TokenType::KW_FROM},
            {"WHERE", TokenType::KW_WHERE},
            {"UPDATE", TokenType::KW_UPDATE},    // Phase 1 Task 2.1
            {"DELETE", TokenType::KW_DELETE},    // Phase 1 Task 2.2
            {"MERGE", TokenType::KW_MERGE},      // Alpha 1 - Advanced SQL
            {"MATCHED", TokenType::KW_MATCHED},  // Alpha 1 - Advanced SQL
            {"SOURCE", TokenType::KW_SOURCE},    // Alpha 1 - Advanced SQL: MERGE BY SOURCE
            {"TARGET", TokenType::KW_TARGET},    // Alpha 1 - Advanced SQL: MERGE BY TARGET
            {"RETURNING", TokenType::KW_RETURNING}, // Alpha 1 - Advanced SQL
            {"NULL", TokenType::KW_NULL},
            {"NOT", TokenType::KW_NOT},
            {"ANALYZE", TokenType::KW_ANALYZE},  // Phase 1 Task 1.1.2
            {"EXPLAIN", TokenType::KW_EXPLAIN},  // Phase 1 Task 1.5
            {"COLUMN", TokenType::KW_COLUMN},    // Phase 1 Task 1.1.2
            {"SAMPLE", TokenType::KW_SAMPLE},    // Phase 1 Task 1.1.2

            // JOIN keywords (Phase 1 Task 3.1)
            {"JOIN", TokenType::KW_JOIN},
            {"INNER", TokenType::KW_INNER},
            {"LEFT", TokenType::KW_LEFT},
            {"RIGHT", TokenType::KW_RIGHT},
            {"FULL", TokenType::KW_FULL},
            {"OUTER", TokenType::KW_OUTER},
            {"CROSS", TokenType::KW_CROSS},
            {"NATURAL", TokenType::KW_NATURAL},
            {"USING", TokenType::KW_USING},

            // Aggregation keywords (Phase 1 Task 4.1)
            {"GROUP", TokenType::KW_GROUP},
            {"BY", TokenType::KW_BY},
            {"HAVING", TokenType::KW_HAVING},
            {"ORDER", TokenType::KW_ORDER},
            {"ASC", TokenType::KW_ASC},
            {"DESC", TokenType::KW_DESC},
            {"LIMIT", TokenType::KW_LIMIT},
            {"OFFSET", TokenType::KW_OFFSET},
            {"DISTINCT", TokenType::KW_DISTINCT},
            {"ALL", TokenType::KW_ALL},

            // Advanced grouping (ROLLUP, CUBE, GROUPING SETS)
            {"ROLLUP", TokenType::KW_ROLLUP},
            {"CUBE", TokenType::KW_CUBE},
            {"GROUPING", TokenType::KW_GROUPING},
            {"SETS", TokenType::KW_SETS},

            // Set operations (UNION, INTERSECT, EXCEPT)
            {"UNION", TokenType::KW_UNION},
            {"INTERSECT", TokenType::KW_INTERSECT},

            // Aggregate functions (Phase 1 Task 4.1)
            {"COUNT", TokenType::KW_COUNT},
            {"SUM", TokenType::KW_SUM},
            {"AVG", TokenType::KW_AVG},
            {"MIN", TokenType::KW_MIN},
            {"MAX", TokenType::KW_MAX},
            {"ARRAY_AGG", TokenType::KW_ARRAY_AGG},  // Phase 2 Task 12

            // Window functions (Phase 1 Task 6)
            {"OVER", TokenType::KW_OVER},
            {"PARTITION", TokenType::KW_PARTITION},
            {"ROWS", TokenType::KW_ROWS},
            {"RANGE", TokenType::KW_RANGE},
            {"BETWEEN", TokenType::KW_BETWEEN},
            {"UNBOUNDED", TokenType::KW_UNBOUNDED},
            {"PRECEDING", TokenType::KW_PRECEDING},
            {"FOLLOWING", TokenType::KW_FOLLOWING},
            {"CURRENT", TokenType::KW_CURRENT},
            {"ROW", TokenType::KW_ROW},
            {"ROW_NUMBER", TokenType::KW_ROW_NUMBER},
            {"RANK", TokenType::KW_RANK},
            {"DENSE_RANK", TokenType::KW_DENSE_RANK},
            {"LAG", TokenType::KW_LAG},
            {"LEAD", TokenType::KW_LEAD},
            {"FIRST_VALUE", TokenType::KW_FIRST_VALUE},
            {"LAST_VALUE", TokenType::KW_LAST_VALUE},
            {"NTH_VALUE", TokenType::KW_NTH_VALUE},
            {"CUME_DIST", TokenType::KW_CUME_DIST},
            {"PERCENT_RANK", TokenType::KW_PERCENT_RANK},
            {"NULLS", TokenType::KW_NULLS},
            {"FIRST", TokenType::KW_FIRST},
            {"LAST", TokenType::KW_LAST},
            {"AND", TokenType::KW_AND},
            {"OR", TokenType::KW_OR},

            // Numeric types
            {"INT", TokenType::KW_INT},
            {"INTEGER", TokenType::KW_INTEGER},
            {"SMALLINT", TokenType::KW_SMALLINT},
            {"BIGINT", TokenType::KW_BIGINT},
            {"TINYINT", TokenType::KW_TINYINT},
            {"INT128", TokenType::KW_INT128},
            {"UINT8", TokenType::KW_UINT8},
            {"UINT16", TokenType::KW_UINT16},
            {"UINT32", TokenType::KW_UINT32},
            {"UINT64", TokenType::KW_UINT64},
            {"REAL", TokenType::KW_REAL},
            {"FLOAT", TokenType::KW_FLOAT},
            {"DOUBLE", TokenType::KW_DOUBLE},
            {"DECIMAL", TokenType::KW_DECIMAL},
            {"NUMERIC", TokenType::KW_NUMERIC},
            {"MONEY", TokenType::KW_MONEY},

            // String types
            {"CHAR", TokenType::KW_CHAR},
            {"CHARACTER", TokenType::KW_CHARACTER},
            {"VARCHAR", TokenType::KW_VARCHAR},
            {"TEXT", TokenType::KW_TEXT},

            // Binary types
            {"BINARY", TokenType::KW_BINARY},
            {"VARBINARY", TokenType::KW_VARBINARY},
            {"BLOB", TokenType::KW_BLOB},
            {"BYTEA", TokenType::KW_BYTEA},

            // Date/Time types
            {"DATE", TokenType::KW_DATE},
            {"TIME", TokenType::KW_TIME},
            {"TIMESTAMP", TokenType::KW_TIMESTAMP},
            {"INTERVAL", TokenType::KW_INTERVAL},
            {"EXTRACT", TokenType::KW_EXTRACT},

            // Boolean
            {"BOOLEAN", TokenType::KW_BOOLEAN},
            {"BOOL", TokenType::KW_BOOL},

            // Special types
            {"UUID", TokenType::KW_UUID},
            {"JSON", TokenType::KW_JSON},
            {"JSONB", TokenType::KW_JSONB},
            {"XML", TokenType::KW_XML},
            {"VECTOR", TokenType::KW_VECTOR},
            {"ARRAY", TokenType::KW_ARRAY},  // Phase 2 Task 12

            // Spatial types (Type Integration Phase 3)
            {"POINT", TokenType::KW_POINT},
            {"LINESTRING", TokenType::KW_LINESTRING},
            {"POLYGON", TokenType::KW_POLYGON},
            {"MULTIPOINT", TokenType::KW_MULTIPOINT},
            {"MULTILINESTRING", TokenType::KW_MULTILINESTRING},
            {"MULTIPOLYGON", TokenType::KW_MULTIPOLYGON},
            {"GEOMETRYCOLLECTION", TokenType::KW_GEOMETRYCOLLECTION},

            // Range types (Task 15 Phase 4)
            {"INT4RANGE", TokenType::KW_INT4RANGE},
            {"INT8RANGE", TokenType::KW_INT8RANGE},
            {"NUMRANGE", TokenType::KW_NUMRANGE},
            {"DATERANGE", TokenType::KW_DATERANGE},
            {"TSRANGE", TokenType::KW_TSRANGE},
            {"TSTZRANGE", TokenType::KW_TSTZRANGE},

            // JSON functions (Phase 1 Task 7)
            {"JSON_EXTRACT", TokenType::KW_JSON_EXTRACT},
            {"JSON_OBJECT", TokenType::KW_JSON_OBJECT},
            {"JSON_ARRAY", TokenType::KW_JSON_ARRAY},
            {"JSON_SET", TokenType::KW_JSON_SET},
            {"JSON_INSERT", TokenType::KW_JSON_INSERT},
            {"JSON_REMOVE", TokenType::KW_JSON_REMOVE},
            {"JSONB_EXTRACT_PATH", TokenType::KW_JSONB_EXTRACT_PATH},
            {"JSONB_BUILD_OBJECT", TokenType::KW_JSONB_BUILD_OBJECT},
            {"JSONB_BUILD_ARRAY", TokenType::KW_JSONB_BUILD_ARRAY},
            {"JSONB_SET", TokenType::KW_JSONB_SET},

            // Conditional functions (Phase 1 Task 8)
            {"COALESCE", TokenType::KW_COALESCE},
            {"NULLIF", TokenType::KW_NULLIF},
            {"CASE", TokenType::KW_CASE},
            {"WHEN", TokenType::KW_WHEN},
            {"THEN", TokenType::KW_THEN},
            {"ELSE", TokenType::KW_ELSE},
            {"END", TokenType::KW_END},

            // Array functions (Phase 2 Task 12)
            {"ARRAY_TO_STRING", TokenType::KW_ARRAY_TO_STRING},
            {"STRING_TO_ARRAY", TokenType::KW_STRING_TO_ARRAY},
            {"ARRAY_APPEND", TokenType::KW_ARRAY_APPEND},
            {"ARRAY_PREPEND", TokenType::KW_ARRAY_PREPEND},
            {"ARRAY_CAT", TokenType::KW_ARRAY_CAT},
            {"ARRAY_REMOVE", TokenType::KW_ARRAY_REMOVE},
            {"ARRAY_REPLACE", TokenType::KW_ARRAY_REPLACE},
            {"ARRAY_LENGTH", TokenType::KW_ARRAY_LENGTH},
            {"ARRAY_DIMS", TokenType::KW_ARRAY_DIMS},
            {"ARRAY_UPPER", TokenType::KW_ARRAY_UPPER},
            {"ARRAY_LOWER", TokenType::KW_ARRAY_LOWER},
            {"UNNEST", TokenType::KW_UNNEST},

            // Type conversion
            {"CAST", TokenType::KW_CAST},
            {"TRY_CAST", TokenType::KW_TRY_CAST},
            {"AS", TokenType::KW_AS},

            // Pattern matching
            {"LIKE", TokenType::KW_LIKE},
            {"ILIKE", TokenType::KW_ILIKE},

            // Character set and collation
            {"SET", TokenType::KW_SET},
            {"COLLATE", TokenType::KW_COLLATE},
            {"COLLATION", TokenType::KW_COLLATION},
            {"DEFAULT", TokenType::KW_DEFAULT},

            // Timezone
            {"ZONE", TokenType::KW_ZONE},
            {"WITH", TokenType::KW_WITH},
            {"RECURSIVE", TokenType::KW_RECURSIVE}, // WITH RECURSIVE for recursive CTEs
            {"WITHOUT", TokenType::KW_WITHOUT},
            {"AT", TokenType::KW_AT},

            // Transaction control (Phase 2 Task 2.6, Phase 3 Task 3.6)
            {"START", TokenType::KW_START},
            {"TRANSACTION", TokenType::KW_TRANSACTION},
            {"COMMIT", TokenType::KW_COMMIT},
            {"ROLLBACK", TokenType::KW_ROLLBACK},
            {"READ", TokenType::KW_READ},
            {"WRITE", TokenType::KW_WRITE},
            {"ONLY", TokenType::KW_ONLY},
            {"WAIT", TokenType::KW_WAIT},
            {"ISOLATION", TokenType::KW_ISOLATION},
            {"LEVEL", TokenType::KW_LEVEL},
            {"COMMITTED", TokenType::KW_COMMITTED},
            {"SNAPSHOT", TokenType::KW_SNAPSHOT},
            {"STABILITY", TokenType::KW_STABILITY},
            {"RESERVING", TokenType::KW_RESERVING},
            {"SHARED", TokenType::KW_SHARED},
            {"PROTECTED", TokenType::KW_PROTECTED},
            {"FOR", TokenType::KW_FOR},
            {"OUTSTANDING", TokenType::KW_OUTSTANDING},
            {"LOCK", TokenType::KW_LOCK},
            {"TIMEOUT", TokenType::KW_TIMEOUT},

            // Database maintenance (Phase 3 Task 3.3)
            {"SWEEP", TokenType::KW_SWEEP},
            {"DATABASE", TokenType::KW_DATABASE},

            // Tablespace management (Phase 2 Task 2.1, 2.2)
            {"TABLESPACE", TokenType::KW_TABLESPACE},
            {"LOCATION", TokenType::KW_LOCATION},
            {"AUTOEXTEND", TokenType::KW_AUTOEXTEND},
            {"AUTOEXTEND_SIZE", TokenType::KW_AUTOEXTEND_SIZE},
            {"MAXSIZE", TokenType::KW_MAXSIZE},
            {"UNLIMITED", TokenType::KW_UNLIMITED},
            {"PREALLOC", TokenType::KW_PREALLOC},
            {"ENABLE", TokenType::KW_ENABLE},      // Security Phase 3.4: ENABLE ROW LEVEL SECURITY
            {"DISABLE", TokenType::KW_DISABLE},    // Security Phase 3.4: DISABLE ROW LEVEL SECURITY
            {"FORCE", TokenType::KW_FORCE},
            {"CASCADE", TokenType::KW_CASCADE},    // ALPHA Phase 1 - DDL Modifications
            {"RESTRICT", TokenType::KW_RESTRICT},  // ALPHA Phase 1 - DDL Modifications
            {"DROP", TokenType::KW_DROP},
            {"TRUNCATE", TokenType::KW_TRUNCATE},  // ALPHA Phase 1 - TRUNCATE TABLE
            {"ASYNC", TokenType::KW_ASYNC},        // ALPHA Phase 1 - TRUNCATE TABLE ASYNC
            {"SYNC", TokenType::KW_SYNC},          // ALPHA Phase 1 - TRUNCATE TABLE SYNC
            {"SEQUENCE", TokenType::KW_SEQUENCE},  // ALPHA Phase 1 - Sequences
            {"INCREMENT", TokenType::KW_INCREMENT},// ALPHA Phase 1 - Sequences
            {"MINVALUE", TokenType::KW_MINVALUE},  // ALPHA Phase 1 - Sequences
            {"MAXVALUE", TokenType::KW_MAXVALUE},  // ALPHA Phase 1 - Sequences
            {"NO", TokenType::KW_NO},              // ALPHA Phase 1 - Sequences (NO MINVALUE/MAXVALUE/CYCLE)
            {"START", TokenType::KW_START},        // ALPHA Phase 1 - Sequences
            {"CACHE", TokenType::KW_CACHE},        // ALPHA Phase 1 - Sequences
            {"CYCLE", TokenType::KW_CYCLE},        // ALPHA Phase 1 - Sequences
            {"RESTART", TokenType::KW_RESTART},    // ALPHA Phase 1 - Sequences
            {"NEXTVAL", TokenType::KW_NEXTVAL},    // ALPHA Phase 1 - Sequences
            {"CURRVAL", TokenType::KW_CURRVAL},    // ALPHA Phase 1 - Sequences
            {"SETVAL", TokenType::KW_SETVAL},      // ALPHA Phase 1 - Sequences
            {"VIEW", TokenType::KW_VIEW},              // ALPHA Phase 1 - Views
            {"REPLACE", TokenType::KW_REPLACE},        // ALPHA Phase 1 - Views (CREATE OR REPLACE)
            {"MATERIALIZED", TokenType::KW_MATERIALIZED}, // ALPHA Phase 1 - Materialized Views
            {"REFRESH", TokenType::KW_REFRESH},        // ALPHA Phase 1 - Materialized Views
            {"CONCURRENTLY", TokenType::KW_CONCURRENTLY}, // ALPHA Phase 1 - Materialized Views
            {"CHECK", TokenType::KW_CHECK},            // ALPHA Phase 1 - Views (WITH CHECK OPTION)
            {"OPTION", TokenType::KW_OPTION},          // ALPHA Phase 1 - Views (WITH CHECK OPTION)
            {"ON", TokenType::KW_ON},
            {"OFF", TokenType::KW_OFF},
            {"ALTER", TokenType::KW_ALTER},   // Phase 2 Task 2.2
            {"RENAME", TokenType::KW_RENAME}, // Phase 2 Task 2.2
            {"TO", TokenType::KW_TO},         // Phase 2 Task 2.2
            {"ADD", TokenType::KW_ADD},       // ALPHA Phase 1 - ALTER TABLE ADD COLUMN
            {"TYPE", TokenType::KW_TYPE},     // ALPHA Phase 1 - ALTER TABLE ALTER COLUMN TYPE
            {"CONSTRAINT", TokenType::KW_CONSTRAINT}, // ALPHA Phase C - Table constraints
            {"IDENTITY", TokenType::KW_IDENTITY},     // ALPHA Phase 1 - IDENTITY columns
            {"GENERATED", TokenType::KW_GENERATED},   // ALPHA Phase 1 - GENERATED columns
            {"ALWAYS", TokenType::KW_ALWAYS},         // ALPHA Phase 1 - GENERATED ALWAYS
            {"STORED", TokenType::KW_STORED},         // ALPHA Phase 1 - GENERATED ... STORED
            {"VIRTUAL", TokenType::KW_VIRTUAL},       // ALPHA Phase 1 - GENERATED ... VIRTUAL
            {"DEFERRABLE", TokenType::KW_DEFERRABLE}, // ALPHA Phase 1 - Deferred constraints
            {"INITIALLY", TokenType::KW_INITIALLY},   // ALPHA Phase 1 - INITIALLY DEFERRED/IMMEDIATE
            {"DEFERRED", TokenType::KW_DEFERRED},     // ALPHA Phase 1 - INITIALLY DEFERRED
            {"IMMEDIATE", TokenType::KW_IMMEDIATE},   // ALPHA Phase 1 - INITIALLY IMMEDIATE
            {"BY", TokenType::KW_BY},                 // ALPHA Phase 1 - GENERATED BY DEFAULT
            {"FOREIGN", TokenType::KW_FOREIGN},       // ALPHA Phase C - Foreign keys
            {"KEY", TokenType::KW_KEY},               // ALPHA Phase C - Foreign/primary keys
            {"PRIMARY", TokenType::KW_PRIMARY},       // ALPHA Phase C - Primary keys
            {"ONLINE", TokenType::KW_ONLINE}, // Phase 4 Task 4.1.1
            {"ATTACH", TokenType::KW_ATTACH}, // Phase 6 Task 6.1
            {"DETACH", TokenType::KW_DETACH}, // Phase 6 Task 6.2

            // Subquery keywords (Phase 2 Wave 2 - Agent B)
            {"IN", TokenType::KW_IN},
            {"EXISTS", TokenType::KW_EXISTS},

            // Trigger keywords (Phase 2 Wave 2 - Agent C)
            {"TRIGGER", TokenType::KW_TRIGGER},
            {"BEFORE", TokenType::KW_BEFORE},
            {"AFTER", TokenType::KW_AFTER},
            {"EXECUTE", TokenType::KW_EXECUTE},
            {"PROCEDURE", TokenType::KW_PROCEDURE},
            {"OLD", TokenType::KW_OLD},
            {"NEW", TokenType::KW_NEW},

            // Stored procedure keywords (Phase 2 Task 10.2)
            {"FUNCTION", TokenType::KW_FUNCTION},
            {"RETURNS", TokenType::KW_RETURNS},
            {"LANGUAGE", TokenType::KW_LANGUAGE},
            {"BEGIN", TokenType::KW_BEGIN},
            // "END" already registered for CASE
            {"DECLARE", TokenType::KW_DECLARE},
            {"RETURN", TokenType::KW_RETURN},
            {"IF", TokenType::KW_IF},
            // "THEN" already registered for CASE
            // "ELSE" already registered for CASE
            {"ELSIF", TokenType::KW_ELSIF},
            {"ENDIF", TokenType::KW_ENDIF},
            {"LOOP", TokenType::KW_LOOP},
            {"WHILE", TokenType::KW_WHILE},
            {"ENDLOOP", TokenType::KW_ENDLOOP},
            {"EXIT", TokenType::KW_EXIT},
            // "WHEN" already registered for CASE
            {"RAISE", TokenType::KW_RAISE},
            {"EXCEPTION", TokenType::KW_EXCEPTION},
            {"TRY", TokenType::KW_TRY},
            {"EXCEPT", TokenType::KW_EXCEPT},

            // Security keywords (ALPHA Phase 1 - Security System Phase 2)
            {"USER", TokenType::KW_USER},
            {"ROLE", TokenType::KW_ROLE},
            // "GROUP" already defined (line 53)
            {"GRANT", TokenType::KW_GRANT},
            {"REVOKE", TokenType::KW_REVOKE},
            {"PRIVILEGES", TokenType::KW_PRIVILEGES},
            {"PASSWORD", TokenType::KW_PASSWORD},
            {"SUPERUSER", TokenType::KW_SUPERUSER},
            {"NOSUPERUSER", TokenType::KW_NOSUPERUSER},
            {"SESSION", TokenType::KW_SESSION},
            {"AUTHORIZATION", TokenType::KW_AUTHORIZATION},
            {"RESET", TokenType::KW_RESET},
            {"PUBLIC", TokenType::KW_PUBLIC},
            {"USAGE", TokenType::KW_USAGE},
            {"CONNECT", TokenType::KW_CONNECT},
            {"REFERENCES", TokenType::KW_REFERENCES},
            // Security Phase 3.4: Row-level security
            {"POLICY", TokenType::KW_POLICY},
            {"SQL", TokenType::KW_SQL},               // Security Phase 3.1: SQL SECURITY DEFINER/INVOKER
            {"SECURITY", TokenType::KW_SECURITY},
            {"DEFINER", TokenType::KW_DEFINER},       // Security Phase 3.1: SQL SECURITY DEFINER
            {"INVOKER", TokenType::KW_INVOKER},       // Security Phase 3.1: SQL SECURITY INVOKER
            // "ENABLE" already defined above
            // "DISABLE" already defined above
            // "FORCE" already defined above
            // "LEVEL" already defined above
            // "ROW" already defined above

            // SQL Engine Commands (ALPHA Phase 1 - Developer Experience)
            {"SHOW", TokenType::KW_SHOW},
            {"DESCRIBE", TokenType::KW_DESCRIBE},
            {"TABLES", TokenType::KW_TABLES},
            {"DATABASES", TokenType::KW_DATABASES},
            {"SCHEMAS", TokenType::KW_SCHEMAS},
            {"COLUMNS", TokenType::KW_COLUMNS},
            {"INDEXES", TokenType::KW_INDEXES},
        };

        // Case-insensitive string comparison
        static bool strcaseeq(std::string_view a, std::string_view b)
        {
            if (a.size() != b.size())
                return false;
            return std::equal(a.begin(), a.end(), b.begin(), [](char ca, char cb)
                              { return std::toupper(ca) == std::toupper(cb); });
        }

        // Lexer implementation
        Lexer::Lexer(std::string_view input)
            : input_(input), current_pos_(0), line_(1), column_(1), error_reporter_(nullptr),
              has_lookahead_(false)
        {
        }

        Lexer::~Lexer() = default;

        Token Lexer::nextToken()
        {
            if (has_lookahead_)
            {
                has_lookahead_ = false;
                return lookahead_token_;
            }

            skipWhitespace();

            if (current_pos_ >= input_.size())
            {
                return Token::makeEOF(currentLocation());
            }

            SourceLocation start_loc = currentLocation();
            char ch = currentChar();

            // Identifiers and keywords
            if (std::isalpha(ch) || ch == '_')
            {
                return scanIdentifier();
            }

            // Numbers
            if (std::isdigit(ch))
            {
                return scanNumber();
            }

            // String literals
            if (ch == '\'')
            {
                return scanString();
            }

            // Comments
            if (ch == '-' && peekChar() == '-')
            {
                scanComment();
                return nextToken(); // Skip comment and get next token
            }
            if (ch == '/' && peekChar() == '*')
            {
                scanComment();
                return nextToken(); // Skip comment and get next token
            }

            // Operators and punctuation
            return scanOperator();
        }

        Token Lexer::peekToken()
        {
            if (!has_lookahead_)
            {
                lookahead_token_ = nextToken();
                has_lookahead_ = true;
            }
            return lookahead_token_;
        }

        SourceLocation Lexer::currentLocation() const
        {
            return SourceLocation(line_, column_, current_pos_);
        }

        std::string_view Lexer::getTokenText(const Token &token) const
        {
            return input_.substr(token.location.offset, token.length);
        }

        char Lexer::currentChar() const
        {
            return current_pos_ < input_.size() ? input_[current_pos_] : '\0';
        }

        char Lexer::peekChar() const
        {
            size_t pos = current_pos_ + 1;
            return pos < input_.size() ? input_[pos] : '\0';
        }

        char Lexer::peekChar2() const
        {
            size_t pos = current_pos_ + 2;
            return pos < input_.size() ? input_[pos] : '\0';
        }

        void Lexer::advance()
        {
            if (current_pos_ < input_.size())
            {
                if (input_[current_pos_] == '\n')
                {
                    line_++;
                    column_ = 1;
                }
                else
                {
                    column_++;
                }
                current_pos_++;
            }
        }

        void Lexer::skipWhitespace()
        {
            while (current_pos_ < input_.size() && std::isspace(currentChar()))
            {
                advance();
            }
        }

        Token Lexer::scanIdentifier()
        {
            SourceLocation start_loc = currentLocation();
            size_t start_pos = current_pos_;

            // First character is letter or underscore
            advance();

            // Subsequent characters can be letters, digits, or underscores
            while (std::isalnum(currentChar()) || currentChar() == '_')
            {
                advance();
            }

            size_t length = current_pos_ - start_pos;
            std::string_view text = input_.substr(start_pos, length);

            // Validate UTF-8 encoding (Phase 1: Foundation Infrastructure)
            if (!scratchbird::core::UTF8Utils::isValidUTF8(text))
            {
                return makeError("Identifier contains invalid UTF-8");
            }

            // Validate identifier length (SQL standard: 128 characters, not bytes)
            if (!scratchbird::core::UTF8Utils::isValidIdentifierLength(text))
            {
                size_t char_count = scratchbird::core::UTF8Utils::countCharacters(text);
                return makeError("Identifier too long: " + std::to_string(char_count) +
                                 " characters (maximum 128)");
            }

            // Check if it's a keyword
            TokenType keyword_type = checkKeyword(text);
            if (keyword_type != TokenType::IDENTIFIER)
            {
                return Token::makeKeyword(start_loc, length, keyword_type);
            }

            // Regular identifier - intern it
            StringPool::StringId id = string_pool_.intern(text);
            return Token::makeIdentifier(start_loc, length, id);
        }

        Token Lexer::scanNumber()
        {
            SourceLocation start_loc = currentLocation();
            size_t start_pos = current_pos_;
            bool has_decimal = false;
            bool has_exponent = false;

            // Scan integer part
            while (std::isdigit(currentChar()))
            {
                advance();
            }

            // Check for decimal point (allow trailing decimal like PostgreSQL: "123.")
            // But reject ".." to avoid ambiguity with range operators
            if (currentChar() == '.' && peekChar() != '.')
            {
                has_decimal = true;
                advance(); // Skip '.'
                // Optionally consume digits after decimal point
                while (std::isdigit(currentChar()))
                {
                    advance();
                }
            }

            // Check for exponent
            if ((currentChar() == 'e' || currentChar() == 'E') &&
                (std::isdigit(peekChar()) ||
                 ((peekChar() == '+' || peekChar() == '-') && std::isdigit(peekChar2()))))
            {
                has_exponent = true;
                advance(); // Skip 'e' or 'E'
                if (currentChar() == '+' || currentChar() == '-')
                {
                    advance();
                }
                while (std::isdigit(currentChar()))
                {
                    advance();
                }
            }

            size_t length = current_pos_ - start_pos;
            std::string_view text = input_.substr(start_pos, length);

            if (has_decimal || has_exponent)
            {
                // Parse as float
                double value = 0.0;
                auto result = std::from_chars(text.data(), text.data() + text.size(), value);
                if (result.ec != std::errc())
                {
                    return makeError("Invalid floating-point number");
                }
                return Token::makeFloat(start_loc, length, value);
            }
            else
            {
                // Parse as integer
                int64_t value = 0;
                auto result = std::from_chars(text.data(), text.data() + text.size(), value);
                if (result.ec != std::errc())
                {
                    return makeError("Invalid integer");
                }
                return Token::makeInteger(start_loc, length, value);
            }
        }

        Token Lexer::scanString()
        {
            SourceLocation start_loc = currentLocation();
            size_t start_pos = current_pos_;

            advance(); // Skip opening quote

            std::string value;
            while (current_pos_ < input_.size() && currentChar() != '\'')
            {
                if (currentChar() == '\\')
                {
                    advance();
                    if (current_pos_ >= input_.size())
                    {
                        return makeError("Unterminated string literal");
                    }
                    // Simple escape sequences
                    switch (currentChar())
                    {
                        case 'n':
                            value += '\n';
                            break;
                        case 't':
                            value += '\t';
                            break;
                        case 'r':
                            value += '\r';
                            break;
                        case '\\':
                            value += '\\';
                            break;
                        case '\'':
                            value += '\'';
                            break;
                        default:
                            value += currentChar();
                            break;
                    }
                }
                else
                {
                    value += currentChar();
                }
                advance();
            }

            if (current_pos_ >= input_.size())
            {
                return makeError("Unterminated string literal");
            }

            advance(); // Skip closing quote

            size_t length = current_pos_ - start_pos;
            StringPool::StringId id = string_pool_.intern(value);
            return Token::makeString(start_loc, length, id);
        }

        Token Lexer::scanOperator()
        {
            SourceLocation start_loc = currentLocation();
            char ch = currentChar();

            switch (ch)
            {
                case '+':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::PLUS);
                case '-':
                    advance();
                    // Check for JSON operators: -> and ->>
                    if (currentChar() == '>')
                    {
                        advance();
                        if (currentChar() == '>')
                        {
                            advance();
                            return Token::makeOperator(start_loc, 3, TokenType::DOUBLE_ARROW);
                        }
                        return Token::makeOperator(start_loc, 2, TokenType::ARROW);
                    }
                    // Check for range adjacent operator: -|-
                    else if (currentChar() == '|')
                    {
                        advance();
                        if (currentChar() == '-')
                        {
                            advance();
                            return Token::makeOperator(start_loc, 3, TokenType::MINUS_PIPE_MINUS);
                        }
                        // Just -| is not valid, backtrack conceptually by error
                        return makeError("Unexpected character sequence '-|'");
                    }
                    return Token::makeOperator(start_loc, 1, TokenType::MINUS);
                case '*':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::STAR);
                case '/':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::SLASH);
                case '%':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::PERCENT);
                case '(':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::LEFT_PAREN);
                case ')':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::RIGHT_PAREN);
                case ',':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::COMMA);
                case ';':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::SEMICOLON);
                case '.':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::DOT);
                case ':':
                    // : is used in :: for type casting
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::COLON);
                case '=':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::EQUAL);

                case '<':
                    advance();
                    if (currentChar() == '=')
                    {
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::LESS_EQUAL);
                    }
                    else if (currentChar() == '>')
                    {
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::NOT_EQUAL);
                    }
                    else if (currentChar() == '@')
                    {
                        // <@ operator (array/range contained by)
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::LESS_AT);
                    }
                    else if (currentChar() == '<')
                    {
                        // << operator (range strictly left of)
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::SHIFT_LEFT);
                    }
                    return Token::makeOperator(start_loc, 1, TokenType::LESS_THAN);

                case '>':
                    advance();
                    if (currentChar() == '=')
                    {
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::GREATER_EQUAL);
                    }
                    else if (currentChar() == '>')
                    {
                        // >> operator (range strictly right of)
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::SHIFT_RIGHT);
                    }
                    return Token::makeOperator(start_loc, 1, TokenType::GREATER_THAN);

                case '#':
                    // JSON path operators: #> and #>>
                    advance();
                    if (currentChar() == '>')
                    {
                        advance();
                        if (currentChar() == '>')
                        {
                            advance();
                            return Token::makeOperator(start_loc, 3, TokenType::HASH_DOUBLE_ARROW);
                        }
                        return Token::makeOperator(start_loc, 2, TokenType::HASH_ARROW);
                    }
                    return makeError("Unexpected character '#'");

                case '&':
                    // && operator (array overlap)
                    advance();
                    if (currentChar() == '&')
                    {
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::AMPERSAND_AMPERSAND);
                    }
                    return makeError("Unexpected character '&'");

                case '@':
                    // @> operator (array contains)
                    advance();
                    if (currentChar() == '>')
                    {
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::AT_GREATER);
                    }
                    return makeError("Unexpected character '@'");

                case '~':
                    // Regex operators: ~ and ~*
                    advance();
                    if (currentChar() == '*')
                    {
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::TILDE_STAR);
                    }
                    return Token::makeOperator(start_loc, 1, TokenType::TILDE);

                case '!':
                    // Regex not match operators: !~ and !~*
                    advance();
                    if (currentChar() == '~')
                    {
                        advance();
                        if (currentChar() == '*')
                        {
                            advance();
                            return Token::makeOperator(start_loc, 3, TokenType::EXCLAIM_TILDE_STAR);
                        }
                        return Token::makeOperator(start_loc, 2, TokenType::EXCLAIM_TILDE);
                    }
                    return makeError("Unexpected character '!'");

                case '[':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::LEFT_BRACKET);

                case ']':
                    advance();
                    return Token::makeOperator(start_loc, 1, TokenType::RIGHT_BRACKET);

                default:
                    advance();
                    return makeError("Unexpected character");
            }
        }

        void Lexer::scanComment()
        {
            if (currentChar() == '-' && peekChar() == '-')
            {
                // Line comment
                advance();
                advance(); // Skip --
                while (current_pos_ < input_.size() && currentChar() != '\n')
                {
                    advance();
                }
            }
            else if (currentChar() == '/' && peekChar() == '*')
            {
                // Block comment
                advance();
                advance(); // Skip /*
                // Safe check: ensure input_.size() >= 2 before subtracting
                while (input_.size() > 1 && current_pos_ < input_.size() - 1)
                {
                    if (currentChar() == '*' && peekChar() == '/')
                    {
                        advance();
                        advance(); // Skip */
                        break;
                    }
                    advance();
                }
            }
        }

        TokenType Lexer::checkKeyword(std::string_view text) const
        {
            for (const auto &kw : KEYWORDS)
            {
                if (strcaseeq(text, kw.text))
                {
                    return kw.type;
                }
            }
            return TokenType::IDENTIFIER;
        }

        Token Lexer::makeError(const std::string &message)
        {
            SourceLocation loc = currentLocation();
            if (error_reporter_)
            {
                ErrorReporter::Error err;
                err.location = loc;
                err.message = message;
                error_reporter_->reportError(err);
            }
            return Token::makeError(loc, 1);
        }

        // SimpleErrorReporter implementation
        void SimpleErrorReporter::reportError(const Error &error)
        {
            errors_.push_back(error);
            std::cerr << "Error at " << error.location.line << ":" << error.location.column << ": "
                      << error.message << std::endl;
            if (!error.hint.empty())
            {
                std::cerr << "Hint: " << error.hint << std::endl;
            }
        }

    } // namespace parser
} // namespace scratchbird