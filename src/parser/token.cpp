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
            // Bounds check instead of assert (works in release builds)
            if (id >= strings_.size())
            {
                // Return empty string_view for invalid IDs instead of crashing
                static const std::string empty_str;
                return std::string_view(empty_str);
            }
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

                // JSON operators (Phase 1 Task 7)
                case TokenType::ARROW:
                    return "->";
                case TokenType::DOUBLE_ARROW:
                    return "->>";
                case TokenType::HASH_ARROW:
                    return "#>";
                case TokenType::HASH_DOUBLE_ARROW:
                    return "#>>";

                case TokenType::KW_CREATE:
                    return "CREATE";
                case TokenType::KW_TABLE:
                    return "TABLE";
                case TokenType::KW_INDEX:
                    return "INDEX";
                case TokenType::KW_UNIQUE:
                    return "UNIQUE";
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

                // Transaction control (Phase 2 Task 2.6)
                case TokenType::KW_START:
                    return "START";
                case TokenType::KW_TRANSACTION:
                    return "TRANSACTION";
                case TokenType::KW_COMMIT:
                    return "COMMIT";
                case TokenType::KW_ROLLBACK:
                    return "ROLLBACK";
                case TokenType::KW_READ:
                    return "READ";
                case TokenType::KW_WRITE:
                    return "WRITE";
                case TokenType::KW_ONLY:
                    return "ONLY";
                case TokenType::KW_WAIT:
                    return "WAIT";
                case TokenType::KW_ISOLATION:
                    return "ISOLATION";
                case TokenType::KW_LEVEL:
                    return "LEVEL";
                case TokenType::KW_COMMITTED:
                    return "COMMITTED";
                case TokenType::KW_SNAPSHOT:
                    return "SNAPSHOT";
                case TokenType::KW_STABILITY:
                    return "STABILITY";
                case TokenType::KW_RESERVING:
                    return "RESERVING";
                case TokenType::KW_SHARED:
                    return "SHARED";
                case TokenType::KW_PROTECTED:
                    return "PROTECTED";
                case TokenType::KW_FOR:
                    return "FOR";
                case TokenType::KW_OUTSTANDING:
                    return "OUTSTANDING";
                case TokenType::KW_LOCK:
                    return "LOCK";
                case TokenType::KW_TIMEOUT:
                    return "TIMEOUT";

                // Tablespace management (Phase 2 Task 2.1)
                case TokenType::KW_TABLESPACE:
                    return "TABLESPACE";
                case TokenType::KW_LOCATION:
                    return "LOCATION";
                case TokenType::KW_AUTOEXTEND:
                    return "AUTOEXTEND";
                case TokenType::KW_AUTOEXTEND_SIZE:
                    return "AUTOEXTEND_SIZE";
                case TokenType::KW_MAXSIZE:
                    return "MAXSIZE";
                case TokenType::KW_UNLIMITED:
                    return "UNLIMITED";
                case TokenType::KW_PREALLOC:
                    return "PREALLOC";
                case TokenType::KW_FORCE:
                    return "FORCE";
                case TokenType::KW_DROP:
                    return "DROP";
                case TokenType::KW_ON:
                    return "ON";
                case TokenType::KW_OFF:
                    return "OFF";
                case TokenType::KW_ALTER:
                    return "ALTER";
                case TokenType::KW_RENAME:
                    return "RENAME";
                case TokenType::KW_TO:
                    return "TO";

                // JSON functions (Phase 1 Task 7)
                case TokenType::KW_JSON_EXTRACT:
                    return "JSON_EXTRACT";
                case TokenType::KW_JSON_OBJECT:
                    return "JSON_OBJECT";
                case TokenType::KW_JSON_ARRAY:
                    return "JSON_ARRAY";
                case TokenType::KW_JSON_SET:
                    return "JSON_SET";
                case TokenType::KW_JSON_INSERT:
                    return "JSON_INSERT";
                case TokenType::KW_JSON_REMOVE:
                    return "JSON_REMOVE";
                case TokenType::KW_JSONB_EXTRACT_PATH:
                    return "JSONB_EXTRACT_PATH";
                case TokenType::KW_JSONB_BUILD_OBJECT:
                    return "JSONB_BUILD_OBJECT";
                case TokenType::KW_JSONB_BUILD_ARRAY:
                    return "JSONB_BUILD_ARRAY";
                case TokenType::KW_JSONB_SET:
                    return "JSONB_SET";

                default:
                    return "UNKNOWN";
            }
        }

    } // namespace parser
} // namespace scratchbird