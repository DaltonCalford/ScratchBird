#include "scratchbird/parser/token.h"
#include <cassert>

namespace scratchbird
{
    namespace parser
    {

        // String pool implementation
        StringPool::StringId StringPool::intern(std::string_view str)
        {
            // Check if already interned
            auto it = lookup_.find(str);
            if (it != lookup_.end())
            {
                return it->second;
            }

            // Add new string
            StringId id = static_cast<StringId>(strings_.size());
            strings_.emplace_back(str);

            // Update lookup with string_view pointing to the stored string
            lookup_[strings_.back()] = id;

            return id;
        }

        std::string_view StringPool::get(StringId id) const
        {
            assert(id < strings_.size());
            return strings_[id];
        }

        void StringPool::clear()
        {
            strings_.clear();
            lookup_.clear();
        }

        // Token type to string conversion
        const char *tokenTypeToString(TokenType type)
        {
            switch (type)
            {
                case TokenType::END_OF_FILE:
                    return "EOF";
                case TokenType::ERROR:
                    return "ERROR";

                case TokenType::INTEGER_LITERAL:
                    return "INTEGER";
                case TokenType::FLOAT_LITERAL:
                    return "FLOAT";
                case TokenType::STRING_LITERAL:
                    return "STRING";

                case TokenType::IDENTIFIER:
                    return "IDENTIFIER";
                case TokenType::KEYWORD:
                    return "KEYWORD";

                case TokenType::PLUS:
                    return "+";
                case TokenType::MINUS:
                    return "-";
                case TokenType::STAR:
                    return "*";
                case TokenType::SLASH:
                    return "/";
                case TokenType::PERCENT:
                    return "%";

                case TokenType::EQUAL:
                    return "=";
                case TokenType::NOT_EQUAL:
                    return "<>";
                case TokenType::LESS_THAN:
                    return "<";
                case TokenType::GREATER_THAN:
                    return ">";
                case TokenType::LESS_EQUAL:
                    return "<=";
                case TokenType::GREATER_EQUAL:
                    return ">=";

                case TokenType::LEFT_PAREN:
                    return "(";
                case TokenType::RIGHT_PAREN:
                    return ")";
                case TokenType::COMMA:
                    return ",";
                case TokenType::SEMICOLON:
                    return ";";
                case TokenType::DOT:
                    return ".";

                case TokenType::KW_CREATE:
                    return "CREATE";
                case TokenType::KW_TABLE:
                    return "TABLE";
                case TokenType::KW_INSERT:
                    return "INSERT";
                case TokenType::KW_INTO:
                    return "INTO";
                case TokenType::KW_VALUES:
                    return "VALUES";
                case TokenType::KW_SELECT:
                    return "SELECT";
                case TokenType::KW_FROM:
                    return "FROM";
                case TokenType::KW_WHERE:
                    return "WHERE";
                case TokenType::KW_NULL:
                    return "NULL";
                case TokenType::KW_NOT:
                    return "NOT";
                case TokenType::KW_INTEGER:
                    return "INTEGER";
                case TokenType::KW_BIGINT:
                    return "BIGINT";
                case TokenType::KW_DOUBLE:
                    return "DOUBLE";
                case TokenType::KW_VARCHAR:
                    return "VARCHAR";

                default:
                    return "UNKNOWN";
            }
        }

    } // namespace parser
} // namespace scratchbird