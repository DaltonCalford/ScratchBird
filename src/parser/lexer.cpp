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
            {"CREATE", TokenType::KW_CREATE},   {"TABLE", TokenType::KW_TABLE},
            {"INSERT", TokenType::KW_INSERT},   {"INTO", TokenType::KW_INTO},
            {"VALUES", TokenType::KW_VALUES},   {"SELECT", TokenType::KW_SELECT},
            {"FROM", TokenType::KW_FROM},       {"WHERE", TokenType::KW_WHERE},
            {"NULL", TokenType::KW_NULL},       {"NOT", TokenType::KW_NOT},

            // Numeric types
            {"INT", TokenType::KW_INT},         {"INTEGER", TokenType::KW_INTEGER},
            {"SMALLINT", TokenType::KW_SMALLINT}, {"BIGINT", TokenType::KW_BIGINT},
            {"TINYINT", TokenType::KW_TINYINT}, {"INT128", TokenType::KW_INT128},
            {"UINT8", TokenType::KW_UINT8},     {"UINT16", TokenType::KW_UINT16},
            {"UINT32", TokenType::KW_UINT32},   {"UINT64", TokenType::KW_UINT64},
            {"REAL", TokenType::KW_REAL},       {"FLOAT", TokenType::KW_FLOAT},
            {"DOUBLE", TokenType::KW_DOUBLE},
            {"DECIMAL", TokenType::KW_DECIMAL}, {"NUMERIC", TokenType::KW_NUMERIC},
            {"MONEY", TokenType::KW_MONEY},

            // String types
            {"CHAR", TokenType::KW_CHAR},       {"CHARACTER", TokenType::KW_CHARACTER},
            {"VARCHAR", TokenType::KW_VARCHAR}, {"TEXT", TokenType::KW_TEXT},

            // Binary types
            {"BINARY", TokenType::KW_BINARY},   {"VARBINARY", TokenType::KW_VARBINARY},
            {"BLOB", TokenType::KW_BLOB},       {"BYTEA", TokenType::KW_BYTEA},

            // Date/Time types
            {"DATE", TokenType::KW_DATE},       {"TIME", TokenType::KW_TIME},
            {"TIMESTAMP", TokenType::KW_TIMESTAMP}, {"INTERVAL", TokenType::KW_INTERVAL},

            // Boolean
            {"BOOLEAN", TokenType::KW_BOOLEAN}, {"BOOL", TokenType::KW_BOOL},

            // Special types
            {"UUID", TokenType::KW_UUID},       {"JSON", TokenType::KW_JSON},
            {"JSONB", TokenType::KW_JSONB},     {"XML", TokenType::KW_XML},
            {"VECTOR", TokenType::KW_VECTOR},

            // Type conversion
            {"CAST", TokenType::KW_CAST},       {"TRY_CAST", TokenType::KW_TRY_CAST},
            {"AS", TokenType::KW_AS},

            // Pattern matching
            {"LIKE", TokenType::KW_LIKE},       {"ILIKE", TokenType::KW_ILIKE},

            // Character set and collation
            {"SET", TokenType::KW_SET},         {"COLLATE", TokenType::KW_COLLATE},
            {"COLLATION", TokenType::KW_COLLATION}, {"DEFAULT", TokenType::KW_DEFAULT},

            // Timezone
            {"ZONE", TokenType::KW_ZONE},       {"WITH", TokenType::KW_WITH},
            {"WITHOUT", TokenType::KW_WITHOUT}, {"AT", TokenType::KW_AT},

            // Transaction control (Phase 2 Task 2.6)
            {"START", TokenType::KW_START},     {"TRANSACTION", TokenType::KW_TRANSACTION},
            {"COMMIT", TokenType::KW_COMMIT},   {"ROLLBACK", TokenType::KW_ROLLBACK},
            {"READ", TokenType::KW_READ},       {"WRITE", TokenType::KW_WRITE},
            {"ONLY", TokenType::KW_ONLY},       {"WAIT", TokenType::KW_WAIT},
            {"ISOLATION", TokenType::KW_ISOLATION}, {"LEVEL", TokenType::KW_LEVEL},
            {"COMMITTED", TokenType::KW_COMMITTED}, {"SNAPSHOT", TokenType::KW_SNAPSHOT},
            {"STABILITY", TokenType::KW_STABILITY}, {"RESERVING", TokenType::KW_RESERVING},
            {"SHARED", TokenType::KW_SHARED},   {"PROTECTED", TokenType::KW_PROTECTED},
            {"FOR", TokenType::KW_FOR},         {"OUTSTANDING", TokenType::KW_OUTSTANDING},

            // Database maintenance (Phase 3 Task 3.3)
            {"SWEEP", TokenType::KW_SWEEP},     {"DATABASE", TokenType::KW_DATABASE},
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
                return makeError("Identifier too long: " + std::to_string(char_count) + " characters (maximum 128)");
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
                    return Token::makeOperator(start_loc, 1, TokenType::LESS_THAN);

                case '>':
                    advance();
                    if (currentChar() == '=')
                    {
                        advance();
                        return Token::makeOperator(start_loc, 2, TokenType::GREATER_EQUAL);
                    }
                    return Token::makeOperator(start_loc, 1, TokenType::GREATER_THAN);

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