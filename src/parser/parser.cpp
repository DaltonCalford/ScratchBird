#include "scratchbird/parser/parser.h"
#include <sstream>

namespace scratchbird
{
    namespace parser
    {

        Parser::Parser(Lexer &lexer, ASTArena &arena) : lexer_(lexer), arena_(arena)
        {
            // Initialize: previous is EOF, current is first real token
            previous_token_ = Token::makeEOF(SourceLocation());
            current_token_ = lexer_.nextToken();
        }

        Parser::~Parser() = default;

        void Parser::advance()
        {
            previous_token_ = current_token_;
            if (!isAtEnd())
            {
                current_token_ = lexer_.nextToken();
            }
        }

        bool Parser::match(TokenType type)
        {
            if (check(type))
            {
                advance();
                return true;
            }
            return false;
        }

        bool Parser::consume(TokenType type, const std::string &message)
        {
            if (check(type))
            {
                advance();
                return true;
            }

            error(message);
            return false;
        }

        void Parser::error(const std::string &message)
        {
            Error err;
            err.location = current().location;
            err.message = message;
            reportError(err);
        }

        void Parser::reportError(const Error &error)
        {
            errors_.push_back(error);
        }

        void Parser::synchronize()
        {
            // Check if current token is already a recovery point before advancing
            // This prevents skipping over statement keywords when they are the error token
            while (!isAtEnd())
            {
                // Check if we've reached a statement boundary (semicolon)
                if (current().type == TokenType::SEMICOLON)
                {
                    advance(); // Consume the semicolon
                    return;
                }

                // Check if current token is a statement-starting keyword
                switch (current().type)
                {
                    case TokenType::KW_CREATE:
                    case TokenType::KW_INSERT:
                    case TokenType::KW_SELECT:
                    case TokenType::KW_ANALYZE:  // Phase 1 Task 1.1.2
                    case TokenType::KW_START:
                    case TokenType::KW_SET:
                    case TokenType::KW_COMMIT:
                    case TokenType::KW_ROLLBACK:
                    case TokenType::KW_SWEEP:
                        // Found a recovery point - don't consume it, let normal parsing continue
                        return;
                    default:
                        break;
                }

                // Not at a recovery point, consume this token and continue
                advance();
            }
        }

        SourceSpan Parser::makeSpan(const SourceLocation &start) const
        {
            return SourceSpan(start, previous_token_.location);
        }

        SourceSpan Parser::makeSpan(const SourceLocation &start, const SourceLocation &end) const
        {
            return SourceSpan(start, end);
        }

        ParseResult Parser::parseStatement()
        {
            ParseResult result;
            errors_.clear();

            try
            {
                Statement *stmt = nullptr;

                if (match(TokenType::KW_CREATE))
                {
                    if (check(TokenType::KW_TABLESPACE))
                    {
                        stmt = parseCreateTablespace();
                    }
                    else if (check(TokenType::KW_TABLE))
                    {
                        stmt = parseCreateTable();
                    }
                    else if (check(TokenType::KW_INDEX) || check(TokenType::KW_UNIQUE))
                    {
                        stmt = parseCreateIndex();
                    }
                    else
                    {
                        error("Expected TABLE, INDEX, UNIQUE INDEX, or TABLESPACE after CREATE");
                        synchronize();
                    }
                }
                else if (match(TokenType::KW_INSERT))
                {
                    stmt = parseInsert();
                }
                else if (match(TokenType::KW_SELECT))
                {
                    stmt = parseSelect();
                }
                else if (match(TokenType::KW_ANALYZE))  // Phase 1 Task 1.1.2
                {
                    stmt = parseAnalyze();
                }
                else if (match(TokenType::KW_START))
                {
                    stmt = parseStartTransaction();
                }
                else if (match(TokenType::KW_SET))
                {
                    stmt = parseSetTransaction();
                }
                else if (match(TokenType::KW_COMMIT))
                {
                    stmt = parseCommit();
                }
                else if (match(TokenType::KW_ROLLBACK))
                {
                    stmt = parseRollback();
                }
                else if (match(TokenType::KW_SWEEP))
                {
                    stmt = parseSweep();
                }
                else if (match(TokenType::KW_ALTER))
                {
                    if (check(TokenType::KW_TABLESPACE))
                    {
                        stmt = parseAlterTablespace();
                    }
                    else if (check(TokenType::KW_TABLE))
                    {
                        stmt = parseAlterTable(); // Phase 4 Task 4.1.1
                    }
                    else
                    {
                        error("Expected TABLESPACE or TABLE after ALTER");
                        synchronize();
                    }
                }
                else if (match(TokenType::KW_DROP))
                {
                    if (check(TokenType::KW_TABLESPACE))
                    {
                        stmt = parseDropTablespace();
                    }
                    else
                    {
                        error("Expected TABLESPACE after DROP");
                        synchronize();
                    }
                }
                else if (match(TokenType::KW_ATTACH))
                {
                    if (check(TokenType::KW_TABLESPACE))
                    {
                        stmt = parseAttachTablespace();
                    }
                    else
                    {
                        error("Expected TABLESPACE after ATTACH");
                        synchronize();
                    }
                }
                else if (match(TokenType::KW_DETACH))
                {
                    if (check(TokenType::KW_TABLESPACE))
                    {
                        stmt = parseDetachTablespace();
                    }
                    else
                    {
                        error("Expected TABLESPACE after DETACH");
                        synchronize();
                    }
                }
                else if (isAtEnd())
                {
                    error("Expected SQL statement, but got end of file");
                }
                else
                {
                    error("Unexpected token at start of statement: " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                }

                if (stmt && !isAtEnd() && !match(TokenType::SEMICOLON))
                {
                    error("Expected semicolon or end of input");
                }

                result.setStatement(stmt);
            }
            catch (...)
            {
                error("Internal parser error");
            }

            // Copy errors to result
            for (const auto &err : errors_)
            {
                result.addError(err);
            }

            return result;
        }

        Statement *Parser::parseCreateTable()
        {
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_TABLE, "Expected TABLE after CREATE"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after CREATE TABLE, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return nullptr;
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after table name"))
            {
                synchronize();
                return nullptr;
            }

            std::vector<ColumnDef *> columns;

            // Parse column definitions
            do
            {
                auto *col = parseColumnDef();
                if (col)
                {
                    columns.push_back(col);
                }
                else
                {
                    // Error already reported
                    synchronize();
                    return nullptr;
                }
            } while (match(TokenType::COMMA));

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after column definitions"))
            {
                synchronize();
                return nullptr;
            }

            // Parse optional TABLESPACE clause (Phase 2 Task 2.3)
            StringPool::StringId tablespace_name = 0;
            if (match(TokenType::KW_TABLESPACE))
            {
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected tablespace name after TABLESPACE, but got " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                    return nullptr;
                }
                tablespace_name = current().value.string_id;
                advance();
            }

            auto span = makeSpan(start_loc);
            return arena_.make<CreateTableStmt>(span, table_name, std::move(columns), 0, 0, tablespace_name);
        }

        Statement *Parser::parseCreateIndex()
        {
            // CREATE [UNIQUE] INDEX index_name ON table_name (column_list) [TABLESPACE tablespace_name]
            auto start_loc = previous().location;

            // Check for UNIQUE
            bool is_unique = false;
            if (match(TokenType::KW_UNIQUE))
            {
                is_unique = true;
            }

            // Expect INDEX keyword
            if (!consume(TokenType::KW_INDEX, "Expected INDEX"))
            {
                synchronize();
                return nullptr;
            }

            // Expect index name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected index name after INDEX, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return nullptr;
            }
            StringPool::StringId index_name = current().value.string_id;
            advance();

            // Expect ON keyword
            if (!consume(TokenType::KW_ON, "Expected ON after index name"))
            {
                synchronize();
                return nullptr;
            }

            // Expect table name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after ON, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return nullptr;
            }
            StringPool::StringId table_name = current().value.string_id;
            advance();

            // Expect opening parenthesis
            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after table name"))
            {
                synchronize();
                return nullptr;
            }

            // Parse column list
            std::vector<StringPool::StringId> columns;
            do
            {
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected column name, but got " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                    return nullptr;
                }
                columns.push_back(current().value.string_id);
                advance();
            } while (match(TokenType::COMMA));

            // Expect closing parenthesis
            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after column list"))
            {
                synchronize();
                return nullptr;
            }

            // Parse optional TABLESPACE clause (Phase 2 Task 2.3)
            StringPool::StringId tablespace_name = 0;
            if (match(TokenType::KW_TABLESPACE))
            {
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected tablespace name after TABLESPACE, but got " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                    return nullptr;
                }
                tablespace_name = current().value.string_id;
                advance();
            }

            auto span = makeSpan(start_loc);
            return arena_.make<CreateIndexStmt>(span, index_name, table_name, std::move(columns),
                                                is_unique, tablespace_name);
        }

        ColumnDef *Parser::parseColumnDef()
        {
            auto start_loc = current().location;

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected column name, but got " +
                      std::string(tokenTypeToString(current().type)));
                return nullptr;
            }

            StringPool::StringId col_name = current().value.string_id;
            advance();

            TypeName type = parseTypeName();
            if (hasErrors())
            {
                return nullptr;
            }

            bool nullable = true;
            if (match(TokenType::KW_NOT))
            {
                if (!consume(TokenType::KW_NULL, "Expected NULL after NOT"))
                {
                    return nullptr;
                }
                nullable = false;
            }
            else if (match(TokenType::KW_NULL))
            {
                nullable = true;
            }

            auto span = makeSpan(start_loc);
            return arena_.make<ColumnDef>(span, col_name, type, nullable);
        }

        TypeName Parser::parseTypeName()
        {
            DataType type = DataType::UNKNOWN;
            uint32_t precision = 0;
            uint32_t scale = 0;

            // Numeric types
            if (match(TokenType::KW_TINYINT))
            {
                type = DataType::INT8;
            }
            else if (match(TokenType::KW_SMALLINT))
            {
                type = DataType::INT16;
            }
            else if (match(TokenType::KW_INT) || match(TokenType::KW_INTEGER))
            {
                type = DataType::INT32;
            }
            else if (match(TokenType::KW_BIGINT))
            {
                type = DataType::INT64;
            }
            else if (match(TokenType::KW_INT128))
            {
                type = DataType::INT128;
            }
            else if (match(TokenType::KW_UINT8))
            {
                type = DataType::UINT8;
            }
            else if (match(TokenType::KW_UINT16))
            {
                type = DataType::UINT16;
            }
            else if (match(TokenType::KW_UINT32))
            {
                type = DataType::UINT32;
            }
            else if (match(TokenType::KW_UINT64))
            {
                type = DataType::UINT64;
            }
            else if (match(TokenType::KW_MONEY))
            {
                type = DataType::MONEY;
            }
            else if (match(TokenType::KW_REAL) || match(TokenType::KW_FLOAT))
            {
                type = DataType::FLOAT32;
            }
            else if (match(TokenType::KW_DOUBLE))
            {
                type = DataType::FLOAT64;
            }
            else if (match(TokenType::KW_DECIMAL) || match(TokenType::KW_NUMERIC))
            {
                type = DataType::DECIMAL;
                // Parse precision and scale: DECIMAL(p, s)
                if (match(TokenType::LEFT_PAREN))
                {
                    if (check(TokenType::INTEGER_LITERAL))
                    {
                        precision = static_cast<uint32_t>(current().value.int_value);
                        advance();

                        if (match(TokenType::COMMA))
                        {
                            if (check(TokenType::INTEGER_LITERAL))
                            {
                                scale = static_cast<uint32_t>(current().value.int_value);
                                advance();
                            }
                        }
                    }
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after DECIMAL precision/scale");
                }
            }
            // String types
            else if (match(TokenType::KW_CHAR) || match(TokenType::KW_CHARACTER))
            {
                type = DataType::CHAR;
                if (match(TokenType::LEFT_PAREN))
                {
                    if (check(TokenType::INTEGER_LITERAL))
                    {
                        precision = static_cast<uint32_t>(current().value.int_value);
                        advance();
                    }
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after CHAR length");
                }
            }
            else if (match(TokenType::KW_VARCHAR))
            {
                type = DataType::VARCHAR;
                if (match(TokenType::LEFT_PAREN))
                {
                    if (check(TokenType::INTEGER_LITERAL))
                    {
                        precision = static_cast<uint32_t>(current().value.int_value);
                        if (precision == 0 || precision > 65535)
                        {
                            error("VARCHAR precision must be between 1 and 65535");
                        }
                        advance();
                    }
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after VARCHAR length");
                }
            }
            else if (match(TokenType::KW_TEXT))
            {
                type = DataType::TEXT;
            }
            // Binary types
            else if (match(TokenType::KW_BINARY))
            {
                type = DataType::BINARY;
                if (match(TokenType::LEFT_PAREN))
                {
                    if (check(TokenType::INTEGER_LITERAL))
                    {
                        precision = static_cast<uint32_t>(current().value.int_value);
                        advance();
                    }
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after BINARY length");
                }
            }
            else if (match(TokenType::KW_VARBINARY))
            {
                type = DataType::VARBINARY;
                if (match(TokenType::LEFT_PAREN))
                {
                    if (check(TokenType::INTEGER_LITERAL))
                    {
                        precision = static_cast<uint32_t>(current().value.int_value);
                        advance();
                    }
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after VARBINARY length");
                }
            }
            else if (match(TokenType::KW_BLOB))
            {
                type = DataType::BLOB;
            }
            else if (match(TokenType::KW_BYTEA))
            {
                type = DataType::BYTEA;
            }
            // Date/Time types
            else if (match(TokenType::KW_DATE))
            {
                type = DataType::DATE;
            }
            else if (match(TokenType::KW_TIME))
            {
                type = DataType::TIME;
            }
            else if (match(TokenType::KW_TIMESTAMP))
            {
                type = DataType::TIMESTAMP;
            }
            else if (match(TokenType::KW_INTERVAL))
            {
                type = DataType::INTERVAL;
            }
            // Boolean
            else if (match(TokenType::KW_BOOLEAN) || match(TokenType::KW_BOOL))
            {
                type = DataType::BOOLEAN;
            }
            // Special types
            else if (match(TokenType::KW_UUID))
            {
                type = DataType::UUID;
            }
            else if (match(TokenType::KW_JSON))
            {
                type = DataType::JSON;
            }
            else if (match(TokenType::KW_JSONB))
            {
                type = DataType::JSONB;
            }
            else if (match(TokenType::KW_XML))
            {
                type = DataType::XML;
            }
            else if (match(TokenType::KW_VECTOR))
            {
                type = DataType::VECTOR;
                // Parse dimensions: VECTOR(n)
                if (match(TokenType::LEFT_PAREN))
                {
                    if (check(TokenType::INTEGER_LITERAL))
                    {
                        precision = static_cast<uint32_t>(current().value.int_value);
                        if (precision == 0 || precision > 65535)
                        {
                            error("VECTOR dimensions must be between 1 and 65535");
                        }
                        advance();
                    }
                    consume(TokenType::RIGHT_PAREN, "Expected ')' after VECTOR dimensions");
                }
            }
            else
            {
                error("Expected data type, but got " +
                      std::string(tokenTypeToString(current().type)));
                type = DataType::INT32; // Default
            }

            return TypeName(type, precision, scale);
        }

        Statement *Parser::parseInsert()
        {
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_INTO, "Expected INTO after INSERT"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after INSERT INTO, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return nullptr;
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            std::vector<StringPool::StringId> columns;

            // Parse column list
            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after table name"))
            {
                synchronize();
                return nullptr;
            }

            do
            {
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected column name, but got " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                    return nullptr;
                }
                columns.push_back(current().value.string_id);
                advance();
            } while (match(TokenType::COMMA));

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after column list"))
            {
                synchronize();
                return nullptr;
            }

            if (!consume(TokenType::KW_VALUES, "Expected VALUES"))
            {
                synchronize();
                return nullptr;
            }

            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after VALUES"))
            {
                synchronize();
                return nullptr;
            }

            std::vector<Expression *> values;

            // Parse value list
            do
            {
                auto *expr = parseExpression();
                if (!expr)
                {
                    synchronize();
                    return nullptr;
                }
                values.push_back(expr);
            } while (match(TokenType::COMMA));

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after value list"))
            {
                synchronize();
                return nullptr;
            }

            if (columns.size() != values.size())
            {
                error("Column count doesn't match value count");
                return nullptr;
            }

            auto span = makeSpan(start_loc);
            return arena_.make<InsertStmt>(span, table_name, std::move(columns), std::move(values));
        }

        Statement *Parser::parseSelect()
        {
            auto start_loc = previous().location;

            std::vector<SelectItem> select_list;

            // Parse select list
            if (check(TokenType::STAR))
            {
                advance();
                select_list.push_back(SelectItem());
            }
            else
            {
                do
                {
                    auto *expr = parseExpression();
                    if (!expr)
                    {
                        synchronize();
                        return nullptr;
                    }

                    StringPool::StringId alias = 0;
                    // TODO: Parse AS alias if needed

                    select_list.push_back(SelectItem(expr, alias));
                } while (match(TokenType::COMMA));
            }

            if (!consume(TokenType::KW_FROM, "Expected FROM"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after FROM, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return nullptr;
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            Expression *where_clause = nullptr;
            if (match(TokenType::KW_WHERE))
            {
                where_clause = parseExpression();
                if (!where_clause)
                {
                    synchronize();
                    return nullptr;
                }
            }

            auto span = makeSpan(start_loc);
            return arena_.make<SelectStmt>(span, std::move(select_list), table_name, where_clause);
        }

        Statement *Parser::parseAnalyze()
        {
            // ANALYZE table_name [COLUMN column_name] [SAMPLE sample_rate]
            // Phase 1 Task 1.1.2: Statistics collection
            auto start_loc = previous().location;

            // Parse table name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after ANALYZE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            // Optional: COLUMN column_name
            StringPool::StringId column_name = 0;
            if (match(TokenType::KW_COLUMN))
            {
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected column name after COLUMN");
                    synchronize();
                    return nullptr;
                }

                column_name = current().value.string_id;
                advance();
            }

            // Optional: SAMPLE sample_rate
            float sample_rate = 0.0f;
            if (match(TokenType::KW_SAMPLE))
            {
                if (!check(TokenType::FLOAT_LITERAL) && !check(TokenType::INTEGER_LITERAL))
                {
                    error("Expected numeric sample rate after SAMPLE");
                    synchronize();
                    return nullptr;
                }

                if (current().type == TokenType::FLOAT_LITERAL)
                {
                    sample_rate = current().value.float_value;
                }
                else
                {
                    sample_rate = static_cast<float>(current().value.int_value);
                }

                advance();

                // Validate sample rate (0.0-1.0)
                if (sample_rate < 0.0f || sample_rate > 1.0f)
                {
                    error("Sample rate must be between 0.0 and 1.0");
                    synchronize();
                    return nullptr;
                }
            }

            auto span = makeSpan(start_loc);
            return arena_.make<AnalyzeStmt>(span, table_name, column_name, sample_rate);
        }

        Statement *Parser::parseStartTransaction()
        {
            // START TRANSACTION [READ WRITE | READ ONLY] [WAIT | NO WAIT]
            // [ISOLATION LEVEL {READ COMMITTED | SNAPSHOT | SNAPSHOT TABLE STABILITY}]
            // [LOCK TIMEOUT seconds]
            // [RESERVING table1 FOR {SHARED|PROTECTED} {READ|WRITE}, ...]
            // [WITH COMMIT OUTSTANDING]
            auto start_loc = previous().location;

            // Consume TRANSACTION keyword
            if (!consume(TokenType::KW_TRANSACTION, "Expected TRANSACTION after START"))
            {
                synchronize();
                return nullptr;
            }

            // Default values
            TransactionMode mode = TransactionMode::READ_WRITE;
            IsolationLevel isolation = IsolationLevel::READ_COMMITTED;
            bool wait = true;
            bool commit_outstanding = false;
            uint32_t lock_timeout = 0;
            std::vector<TableReservation> table_reservations;

            // Parse optional transaction mode
            if (match(TokenType::KW_READ))
            {
                if (match(TokenType::KW_WRITE))
                {
                    mode = TransactionMode::READ_WRITE;
                }
                else if (match(TokenType::KW_ONLY))
                {
                    mode = TransactionMode::READ_ONLY;
                }
                else if (match(TokenType::KW_COMMITTED))
                {
                    // READ COMMITTED - this is the isolation level, rewind
                    // We need to handle "READ COMMITTED" as isolation level
                    isolation = IsolationLevel::READ_COMMITTED;
                }
                else
                {
                    error("Expected WRITE, ONLY, or COMMITTED after READ");
                }
            }

            // Parse NO WAIT
            if (match(TokenType::KW_NOT))
            {
                if (!consume(TokenType::KW_WAIT, "Expected WAIT after NO"))
                {
                    synchronize();
                    return nullptr;
                }
                wait = false;
            }

            // Parse ISOLATION LEVEL
            if (match(TokenType::KW_ISOLATION))
            {
                if (!consume(TokenType::KW_LEVEL, "Expected LEVEL after ISOLATION"))
                {
                    synchronize();
                    return nullptr;
                }

                if (match(TokenType::KW_READ))
                {
                    if (!consume(TokenType::KW_COMMITTED, "Expected COMMITTED after READ"))
                    {
                        synchronize();
                        return nullptr;
                    }
                    isolation = IsolationLevel::READ_COMMITTED;
                }
                else if (match(TokenType::KW_SNAPSHOT))
                {
                    if (match(TokenType::KW_TABLE))
                    {
                        if (!consume(TokenType::KW_STABILITY, "Expected STABILITY after TABLE"))
                        {
                            synchronize();
                            return nullptr;
                        }
                        isolation = IsolationLevel::SNAPSHOT_TABLE_STABILITY;
                    }
                    else
                    {
                        isolation = IsolationLevel::SNAPSHOT;
                    }
                }
                else
                {
                    error("Expected isolation level (READ COMMITTED, SNAPSHOT, or SNAPSHOT TABLE "
                          "STABILITY)");
                    synchronize();
                    return nullptr;
                }
            }

            // Parse LOCK TIMEOUT (Phase 3 Task 3.6)
            if (match(TokenType::KW_LOCK))
            {
                if (!consume(TokenType::KW_TIMEOUT, "Expected TIMEOUT after LOCK"))
                {
                    synchronize();
                    return nullptr;
                }

                if (!check(TokenType::INTEGER_LITERAL))
                {
                    error("Expected timeout value (integer) after LOCK TIMEOUT");
                    synchronize();
                    return nullptr;
                }

                lock_timeout = static_cast<uint32_t>(current().value.int_value);
                advance();
            }

            // Parse RESERVING clause (Phase 3 Task 3.6)
            if (match(TokenType::KW_RESERVING))
            {
                do
                {
                    // Parse table name
                    if (!check(TokenType::IDENTIFIER))
                    {
                        error("Expected table name in RESERVING clause");
                        synchronize();
                        return nullptr;
                    }

                    StringPool::StringId table_name = current().value.string_id;
                    advance();

                    // Parse FOR keyword
                    if (!consume(TokenType::KW_FOR,
                                 "Expected FOR after table name in RESERVING clause"))
                    {
                        synchronize();
                        return nullptr;
                    }

                    // Parse lock mode (SHARED or PROTECTED)
                    TableLockMode lock_mode;
                    if (match(TokenType::KW_SHARED))
                    {
                        lock_mode = TableLockMode::SHARED;
                    }
                    else if (match(TokenType::KW_PROTECTED))
                    {
                        lock_mode = TableLockMode::PROTECTED;
                    }
                    else
                    {
                        error("Expected SHARED or PROTECTED in RESERVING clause");
                        synchronize();
                        return nullptr;
                    }

                    // Parse access mode (READ or WRITE)
                    bool for_write = false;
                    if (match(TokenType::KW_READ))
                    {
                        for_write = false;
                    }
                    else if (match(TokenType::KW_WRITE))
                    {
                        for_write = true;
                    }
                    else
                    {
                        error("Expected READ or WRITE in RESERVING clause");
                        synchronize();
                        return nullptr;
                    }

                    // Add this table reservation
                    table_reservations.emplace_back(table_name, lock_mode, for_write);

                } while (match(TokenType::COMMA));
            }

            // Parse WITH COMMIT OUTSTANDING
            if (match(TokenType::KW_WITH))
            {
                if (!consume(TokenType::KW_COMMIT, "Expected COMMIT after WITH"))
                {
                    synchronize();
                    return nullptr;
                }
                if (!consume(TokenType::KW_OUTSTANDING, "Expected OUTSTANDING after COMMIT"))
                {
                    synchronize();
                    return nullptr;
                }
                commit_outstanding = true;
            }

            auto span = makeSpan(start_loc);
            return arena_.make<StartTransactionStmt>(span, mode, isolation, wait,
                                                     commit_outstanding, lock_timeout,
                                                     std::move(table_reservations));
        }

        Statement *Parser::parseSetTransaction()
        {
            // SET TRANSACTION [READ WRITE | READ ONLY] [WAIT | NO WAIT]
            // [ISOLATION LEVEL {READ COMMITTED | SNAPSHOT | SNAPSHOT TABLE STABILITY}]
            // [LOCK TIMEOUT seconds]
            // [RESERVING table1 FOR {SHARED|PROTECTED} {READ|WRITE}, ...]
            auto start_loc = previous().location;

            // Consume TRANSACTION keyword
            if (!consume(TokenType::KW_TRANSACTION, "Expected TRANSACTION after SET"))
            {
                synchronize();
                return nullptr;
            }

            // Default values
            TransactionMode mode = TransactionMode::READ_WRITE;
            IsolationLevel isolation = IsolationLevel::READ_COMMITTED;
            bool wait = true;
            uint32_t lock_timeout = 0;
            std::vector<TableReservation> table_reservations;

            // Parse optional transaction mode
            if (match(TokenType::KW_READ))
            {
                if (match(TokenType::KW_WRITE))
                {
                    mode = TransactionMode::READ_WRITE;
                }
                else if (match(TokenType::KW_ONLY))
                {
                    mode = TransactionMode::READ_ONLY;
                }
                else if (match(TokenType::KW_COMMITTED))
                {
                    // READ COMMITTED - this is the isolation level, rewind
                    // We need to handle "READ COMMITTED" as isolation level
                    isolation = IsolationLevel::READ_COMMITTED;
                }
                else
                {
                    error("Expected WRITE, ONLY, or COMMITTED after READ");
                }
            }

            // Parse NO WAIT
            if (match(TokenType::KW_NOT))
            {
                if (!consume(TokenType::KW_WAIT, "Expected WAIT after NO"))
                {
                    synchronize();
                    return nullptr;
                }
                wait = false;
            }

            // Parse ISOLATION LEVEL
            if (match(TokenType::KW_ISOLATION))
            {
                if (!consume(TokenType::KW_LEVEL, "Expected LEVEL after ISOLATION"))
                {
                    synchronize();
                    return nullptr;
                }

                if (match(TokenType::KW_READ))
                {
                    if (!consume(TokenType::KW_COMMITTED, "Expected COMMITTED after READ"))
                    {
                        synchronize();
                        return nullptr;
                    }
                    isolation = IsolationLevel::READ_COMMITTED;
                }
                else if (match(TokenType::KW_SNAPSHOT))
                {
                    if (match(TokenType::KW_TABLE))
                    {
                        if (!consume(TokenType::KW_STABILITY, "Expected STABILITY after TABLE"))
                        {
                            synchronize();
                            return nullptr;
                        }
                        isolation = IsolationLevel::SNAPSHOT_TABLE_STABILITY;
                    }
                    else
                    {
                        isolation = IsolationLevel::SNAPSHOT;
                    }
                }
                else
                {
                    error("Expected isolation level (READ COMMITTED, SNAPSHOT, or SNAPSHOT TABLE "
                          "STABILITY)");
                    synchronize();
                    return nullptr;
                }
            }

            // Parse LOCK TIMEOUT (Phase 3 Task 3.6)
            if (match(TokenType::KW_LOCK))
            {
                if (!consume(TokenType::KW_TIMEOUT, "Expected TIMEOUT after LOCK"))
                {
                    synchronize();
                    return nullptr;
                }

                if (!check(TokenType::INTEGER_LITERAL))
                {
                    error("Expected timeout value (integer) after LOCK TIMEOUT");
                    synchronize();
                    return nullptr;
                }

                lock_timeout = static_cast<uint32_t>(current().value.int_value);
                advance();
            }

            // Parse RESERVING clause (Phase 3 Task 3.6)
            if (match(TokenType::KW_RESERVING))
            {
                do
                {
                    // Parse table name
                    if (!check(TokenType::IDENTIFIER))
                    {
                        error("Expected table name in RESERVING clause");
                        synchronize();
                        return nullptr;
                    }

                    StringPool::StringId table_name = current().value.string_id;
                    advance();

                    // Parse FOR keyword
                    if (!consume(TokenType::KW_FOR,
                                 "Expected FOR after table name in RESERVING clause"))
                    {
                        synchronize();
                        return nullptr;
                    }

                    // Parse lock mode (SHARED or PROTECTED)
                    TableLockMode lock_mode;
                    if (match(TokenType::KW_SHARED))
                    {
                        lock_mode = TableLockMode::SHARED;
                    }
                    else if (match(TokenType::KW_PROTECTED))
                    {
                        lock_mode = TableLockMode::PROTECTED;
                    }
                    else
                    {
                        error("Expected SHARED or PROTECTED in RESERVING clause");
                        synchronize();
                        return nullptr;
                    }

                    // Parse access mode (READ or WRITE)
                    bool for_write = false;
                    if (match(TokenType::KW_READ))
                    {
                        for_write = false;
                    }
                    else if (match(TokenType::KW_WRITE))
                    {
                        for_write = true;
                    }
                    else
                    {
                        error("Expected READ or WRITE in RESERVING clause");
                        synchronize();
                        return nullptr;
                    }

                    // Add this table reservation
                    table_reservations.emplace_back(table_name, lock_mode, for_write);

                } while (match(TokenType::COMMA));
            }

            auto span = makeSpan(start_loc);
            return arena_.make<SetTransactionStmt>(span, mode, isolation, wait, lock_timeout,
                                                   std::move(table_reservations));
        }

        Statement *Parser::parseCommit()
        {
            // COMMIT
            auto start_loc = previous().location;

            auto span = makeSpan(start_loc);
            return arena_.make<CommitStmt>(span);
        }

        Statement *Parser::parseRollback()
        {
            // ROLLBACK
            auto start_loc = previous().location;

            auto span = makeSpan(start_loc);
            return arena_.make<RollbackStmt>(span);
        }

        Statement *Parser::parseSweep()
        {
            // SWEEP DATABASE
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_DATABASE, "Expected DATABASE after SWEEP"))
            {
                synchronize();
                return nullptr;
            }

            auto span = makeSpan(start_loc);
            return arena_.make<SweepStmt>(span);
        }

        Statement *Parser::parseCreateTablespace()
        {
            // CREATE TABLESPACE name LOCATION 'path' [AUTOEXTEND ON|OFF] [AUTOEXTEND_SIZE N] [MAXSIZE N|UNLIMITED] [PREALLOC N]
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_TABLESPACE, "Expected TABLESPACE after CREATE"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected tablespace name after CREATE TABLESPACE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId tablespace_name = current().value.string_id;
            advance();

            if (!consume(TokenType::KW_LOCATION, "Expected LOCATION after tablespace name"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::STRING_LITERAL))
            {
                error("Expected string literal for LOCATION path");
                synchronize();
                return nullptr;
            }

            StringPool::StringId location = current().value.string_id;
            advance();

            // Parse optional parameters with defaults
            bool autoextend_enabled = true;
            uint32_t autoextend_size_mb = 100;
            uint32_t max_size_mb = 0;  // 0 = UNLIMITED
            uint32_t prealloc_pages = 0;

            // Parse optional AUTOEXTEND clause
            if (match(TokenType::KW_AUTOEXTEND))
            {
                if (match(TokenType::KW_ON))
                {
                    autoextend_enabled = true;
                }
                else if (match(TokenType::KW_OFF))
                {
                    autoextend_enabled = false;
                }
                else
                {
                    error("Expected ON or OFF after AUTOEXTEND");
                    synchronize();
                    return nullptr;
                }
            }

            // Parse optional AUTOEXTEND_SIZE clause
            if (match(TokenType::KW_AUTOEXTEND_SIZE))
            {
                if (!check(TokenType::INTEGER_LITERAL))
                {
                    error("Expected integer value for AUTOEXTEND_SIZE");
                    synchronize();
                    return nullptr;
                }
                autoextend_size_mb = static_cast<uint32_t>(current().value.int_value);
                advance();
            }

            // Parse optional MAXSIZE clause
            if (match(TokenType::KW_MAXSIZE))
            {
                if (match(TokenType::KW_UNLIMITED))
                {
                    max_size_mb = 0;  // 0 = UNLIMITED
                }
                else if (check(TokenType::INTEGER_LITERAL))
                {
                    max_size_mb = static_cast<uint32_t>(current().value.int_value);
                    advance();
                }
                else
                {
                    error("Expected UNLIMITED or integer value for MAXSIZE");
                    synchronize();
                    return nullptr;
                }
            }

            // Parse optional PREALLOC clause
            if (match(TokenType::KW_PREALLOC))
            {
                if (!check(TokenType::INTEGER_LITERAL))
                {
                    error("Expected integer value for PREALLOC");
                    synchronize();
                    return nullptr;
                }
                prealloc_pages = static_cast<uint32_t>(current().value.int_value);
                advance();
            }

            auto span = makeSpan(start_loc);
            return arena_.make<CreateTablespaceStmt>(span, tablespace_name, location,
                                                      autoextend_enabled, autoextend_size_mb,
                                                      max_size_mb, prealloc_pages);
        }

        Statement *Parser::parseDropTablespace()
        {
            // DROP TABLESPACE name [FORCE]
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_TABLESPACE, "Expected TABLESPACE after DROP"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected tablespace name after DROP TABLESPACE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId tablespace_name = current().value.string_id;
            advance();

            // Parse optional FORCE clause
            bool force = false;
            if (match(TokenType::KW_FORCE))
            {
                force = true;
            }

            auto span = makeSpan(start_loc);
            return arena_.make<DropTablespaceStmt>(span, tablespace_name, force);
        }

        Statement *Parser::parseAttachTablespace()
        {
            // ATTACH TABLESPACE 'file_path' [AS 'name']
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_TABLESPACE, "Expected TABLESPACE after ATTACH"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::STRING_LITERAL))
            {
                error("Expected file path (string literal) after ATTACH TABLESPACE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId file_path = current().value.string_id;
            advance();

            // Parse optional AS clause
            StringPool::StringId tablespace_name = 0;
            if (match(TokenType::KW_AS))
            {
                if (!check(TokenType::STRING_LITERAL) && !check(TokenType::IDENTIFIER))
                {
                    error("Expected tablespace name after AS");
                    synchronize();
                    return nullptr;
                }
                tablespace_name = current().value.string_id;
                advance();
            }

            auto span = makeSpan(start_loc);
            return arena_.make<AttachTablespaceStmt>(span, file_path, tablespace_name);
        }

        Statement *Parser::parseDetachTablespace()
        {
            // DETACH TABLESPACE name [FORCE]
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_TABLESPACE, "Expected TABLESPACE after DETACH"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected tablespace name after DETACH TABLESPACE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId tablespace_name = current().value.string_id;
            advance();

            // Parse optional FORCE clause
            bool force = false;
            if (match(TokenType::KW_FORCE))
            {
                force = true;
            }

            auto span = makeSpan(start_loc);
            return arena_.make<DetachTablespaceStmt>(span, tablespace_name, force);
        }

        Statement *Parser::parseAlterTablespace()
        {
            // ALTER TABLESPACE name { AUTOEXTEND ON|OFF | AUTOEXTEND_SIZE N | MAXSIZE N|UNLIMITED | RENAME TO new_name }
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_TABLESPACE, "Expected TABLESPACE after ALTER"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected tablespace name after ALTER TABLESPACE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId tablespace_name = current().value.string_id;
            advance();

            // Create ALTER statement
            auto *stmt = arena_.make<AlterTablespaceStmt>(makeSpan(start_loc), tablespace_name);

            // Parse alterations (at least one required)
            bool parsed_alteration = false;

            while (!isAtEnd() && !check(TokenType::SEMICOLON))
            {
                TablespaceAlteration alteration(TablespaceAlterationType::SET_AUTOEXTEND);

                if (match(TokenType::KW_AUTOEXTEND))
                {
                    // AUTOEXTEND ON|OFF
                    if (match(TokenType::KW_ON))
                    {
                        alteration.type = TablespaceAlterationType::SET_AUTOEXTEND;
                        alteration.autoextend_enabled = true;
                        stmt->addAlteration(alteration);
                        parsed_alteration = true;
                    }
                    else if (match(TokenType::KW_OFF))
                    {
                        alteration.type = TablespaceAlterationType::SET_AUTOEXTEND;
                        alteration.autoextend_enabled = false;
                        stmt->addAlteration(alteration);
                        parsed_alteration = true;
                    }
                    else
                    {
                        error("Expected ON or OFF after AUTOEXTEND");
                        synchronize();
                        return nullptr;
                    }
                }
                else if (match(TokenType::KW_AUTOEXTEND_SIZE))
                {
                    // AUTOEXTEND_SIZE N
                    if (!check(TokenType::INTEGER_LITERAL))
                    {
                        error("Expected integer value for AUTOEXTEND_SIZE");
                        synchronize();
                        return nullptr;
                    }
                    alteration.type = TablespaceAlterationType::SET_AUTOEXTEND_SIZE;
                    alteration.size_value = static_cast<uint32_t>(current().value.int_value);
                    advance();
                    stmt->addAlteration(alteration);
                    parsed_alteration = true;
                }
                else if (match(TokenType::KW_MAXSIZE))
                {
                    // MAXSIZE N | UNLIMITED
                    if (match(TokenType::KW_UNLIMITED))
                    {
                        alteration.type = TablespaceAlterationType::SET_MAXSIZE;
                        alteration.size_value = 0; // 0 = UNLIMITED
                        stmt->addAlteration(alteration);
                        parsed_alteration = true;
                    }
                    else if (check(TokenType::INTEGER_LITERAL))
                    {
                        alteration.type = TablespaceAlterationType::SET_MAXSIZE;
                        alteration.size_value = static_cast<uint32_t>(current().value.int_value);
                        advance();
                        stmt->addAlteration(alteration);
                        parsed_alteration = true;
                    }
                    else
                    {
                        error("Expected UNLIMITED or integer value for MAXSIZE");
                        synchronize();
                        return nullptr;
                    }
                }
                else if (match(TokenType::KW_RENAME))
                {
                    // RENAME TO new_name
                    if (!consume(TokenType::KW_TO, "Expected TO after RENAME"))
                    {
                        synchronize();
                        return nullptr;
                    }

                    if (!check(TokenType::IDENTIFIER))
                    {
                        error("Expected new tablespace name after RENAME TO");
                        synchronize();
                        return nullptr;
                    }

                    alteration.type = TablespaceAlterationType::RENAME_TO;
                    alteration.new_name = current().value.string_id;
                    advance();
                    stmt->addAlteration(alteration);
                    parsed_alteration = true;
                }
                else
                {
                    // No more alterations to parse
                    break;
                }
            }

            if (!parsed_alteration)
            {
                error("Expected at least one alteration after ALTER TABLESPACE");
                synchronize();
                return nullptr;
            }

            return stmt;
        }

        Statement *Parser::parseAlterTable()
        {
            // ALTER TABLE name SET TABLESPACE tablespace_name [ONLINE]
            auto start_loc = previous().location;

            if (!consume(TokenType::KW_TABLE, "Expected TABLE after ALTER"))
            {
                synchronize();
                return nullptr;
            }

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after ALTER TABLE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            // Expect SET
            if (!consume(TokenType::KW_SET, "Expected SET after table name"))
            {
                synchronize();
                return nullptr;
            }

            // Expect TABLESPACE
            if (!consume(TokenType::KW_TABLESPACE, "Expected TABLESPACE after SET"))
            {
                synchronize();
                return nullptr;
            }

            // Get tablespace name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected tablespace name after TABLESPACE");
                synchronize();
                return nullptr;
            }

            StringPool::StringId tablespace_name = current().value.string_id;
            advance();

            // Check for optional ONLINE clause
            bool online = false;
            if (match(TokenType::KW_ONLINE))
            {
                online = true;
            }

            // Create ALTER TABLE SET TABLESPACE statement
            auto *stmt = arena_.make<AlterTableSetTablespaceStmt>(makeSpan(start_loc), table_name,
                                                                   tablespace_name, online);

            return stmt;
        }

        Expression *Parser::parseExpression()
        {
            return parseComparison();
        }

        Expression *Parser::parseComparison()
        {
            auto *expr = parseTerm();
            if (!expr)
                return nullptr;

            while (true)
            {
                BinaryOp op;
                if (match(TokenType::EQUAL))
                {
                    op = BinaryOp::EQ;
                }
                else if (match(TokenType::NOT_EQUAL))
                {
                    op = BinaryOp::NE;
                }
                else if (match(TokenType::LESS_THAN))
                {
                    op = BinaryOp::LT;
                }
                else if (match(TokenType::GREATER_THAN))
                {
                    op = BinaryOp::GT;
                }
                else if (match(TokenType::LESS_EQUAL))
                {
                    op = BinaryOp::LE;
                }
                else if (match(TokenType::GREATER_EQUAL))
                {
                    op = BinaryOp::GE;
                }
                else if (match(TokenType::KW_LIKE))
                {
                    op = BinaryOp::LIKE;
                }
                else if (match(TokenType::KW_ILIKE))
                {
                    op = BinaryOp::ILIKE;
                }
                else
                {
                    break;
                }

                auto *right = parseTerm();
                if (!right)
                    return nullptr;

                auto span = makeSpan(expr->span().start, right->span().end);
                expr = arena_.make<BinaryOpExpr>(span, op, expr, right);
            }

            return expr;
        }

        Expression *Parser::parseTerm()
        {
            auto *expr = parseFactor();
            if (!expr)
                return nullptr;

            while (true)
            {
                BinaryOp op;
                if (match(TokenType::PLUS))
                {
                    op = BinaryOp::ADD;
                }
                else if (match(TokenType::MINUS))
                {
                    op = BinaryOp::SUBTRACT;
                }
                else
                {
                    break;
                }

                auto *right = parseFactor();
                if (!right)
                    return nullptr;

                auto span = makeSpan(expr->span().start, right->span().end);
                expr = arena_.make<BinaryOpExpr>(span, op, expr, right);
            }

            return expr;
        }

        Expression *Parser::parseFactor()
        {
            auto *expr = parsePrimary();
            if (!expr)
                return nullptr;

            while (true)
            {
                BinaryOp op;
                if (match(TokenType::STAR))
                {
                    op = BinaryOp::MULTIPLY;
                }
                else if (match(TokenType::SLASH))
                {
                    op = BinaryOp::DIVIDE;
                }
                else if (match(TokenType::PERCENT))
                {
                    op = BinaryOp::MODULO;
                }
                else
                {
                    break;
                }

                auto *right = parsePrimary();
                if (!right)
                    return nullptr;

                auto span = makeSpan(expr->span().start, right->span().end);
                expr = arena_.make<BinaryOpExpr>(span, op, expr, right);
            }

            return expr;
        }

        Expression *Parser::parsePrimary()
        {
            auto start_loc = current().location;

            if (match(TokenType::INTEGER_LITERAL))
            {
                auto span = makeSpan(start_loc, previous().location);
                auto *lit = arena_.make<LiteralExpr>(span, LiteralExpr::INTEGER);
                lit->setIntValue(previous().value.int_value);
                return lit;
            }

            if (match(TokenType::FLOAT_LITERAL))
            {
                auto span = makeSpan(start_loc, previous().location);
                auto *lit = arena_.make<LiteralExpr>(span, LiteralExpr::FLOAT);
                lit->setFloatValue(previous().value.float_value);
                return lit;
            }

            if (match(TokenType::STRING_LITERAL))
            {
                auto span = makeSpan(start_loc, previous().location);
                auto *lit = arena_.make<LiteralExpr>(span, LiteralExpr::STRING);
                lit->setStringValue(previous().value.string_id);
                return lit;
            }

            if (match(TokenType::KW_NULL))
            {
                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<LiteralExpr>(span, LiteralExpr::NULL_LITERAL);
            }

            // CAST(expr AS type) or TRY_CAST(expr AS type)
            if (match(TokenType::KW_CAST) || match(TokenType::KW_TRY_CAST))
            {
                bool is_try_cast = previous().type == TokenType::KW_TRY_CAST;
                const char *keyword_name = is_try_cast ? "TRY_CAST" : "CAST";

                if (!consume(TokenType::LEFT_PAREN,
                             std::string("Expected '(' after ") + keyword_name))
                    return nullptr;

                auto *expr = parseExpression();
                if (!expr)
                    return nullptr;

                if (!consume(TokenType::KW_AS,
                             std::string("Expected AS in ") + keyword_name + " expression"))
                    return nullptr;

                auto target_type = parseTypeName();

                auto end_loc = current().location;
                if (!consume(TokenType::RIGHT_PAREN,
                             std::string("Expected ')' after ") + keyword_name))
                    return nullptr;

                auto span = makeSpan(start_loc, end_loc);
                return arena_.make<CastExpr>(span, expr, target_type, is_try_cast);
            }

            if (check(TokenType::IDENTIFIER))
            {
                auto name = current().value.string_id;
                advance();

                // Check if this is a function call
                if (match(TokenType::LEFT_PAREN))
                {
                    // Parse function arguments
                    std::vector<Expression *> args;

                    // Handle empty argument list
                    if (!check(TokenType::RIGHT_PAREN))
                    {
                        do
                        {
                            auto *arg = parseExpression();
                            if (!arg)
                                return nullptr;
                            args.push_back(arg);
                        } while (match(TokenType::COMMA));
                    }

                    auto end_loc = current().location;
                    if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after function arguments"))
                        return nullptr;

                    auto span = makeSpan(start_loc, end_loc);
                    return arena_.make<FunctionCallExpr>(span, name, std::move(args));
                }
                else
                {
                    // Just an identifier
                    auto span = makeSpan(start_loc, previous().location);
                    return arena_.make<IdentifierExpr>(span, name);
                }
            }

            if (match(TokenType::LEFT_PAREN))
            {
                auto *expr = parseExpression();
                if (!expr)
                    return nullptr;

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after expression"))
                {
                    return nullptr;
                }
                return expr;
            }

            error("Expected expression, but got " + std::string(tokenTypeToString(current().type)));
            return nullptr;
        }

        // Convenience function
        std::unique_ptr<ParseResult> parseSQL(const std::string &sql)
        {
            Lexer lexer(sql);
            ASTArena arena;
            Parser parser(lexer, arena);

            auto result = std::make_unique<ParseResult>(parser.parseStatement());
            return result;
        }

    } // namespace parser
} // namespace scratchbird