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
                    case TokenType::KW_EXPLAIN:  // Phase 1 Task 1.5
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
                    // Trigger support will be added by Agent C
                    // else if (check(TokenType::KW_TRIGGER))
                    // {
                    //     stmt = parseCreateTrigger();
                    // }
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
                else if (match(TokenType::KW_WITH))  // Phase 2 Wave 2: WITH clause (CTEs)
                {
                    // WITH clause starts a SELECT statement
                    stmt = parseSelect();  // parseSelect will handle the WITH clause
                }
                else if (match(TokenType::KW_SELECT))
                {
                    stmt = parseSelect();
                }
                else if (match(TokenType::KW_UPDATE))  // Phase 1 Task 2.1
                {
                    stmt = parseUpdate();
                }
                else if (match(TokenType::KW_DELETE))  // Phase 1 Task 2.2
                {
                    stmt = parseDelete();
                }
                else if (match(TokenType::KW_ANALYZE))  // Phase 1 Task 1.1.2
                {
                    stmt = parseAnalyze();
                }
                else if (match(TokenType::KW_EXPLAIN))  // Phase 1 Task 1.5
                {
                    stmt = parseExplain();
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
                    // Trigger support will be added by Agent C
                    // else if (check(TokenType::KW_TRIGGER))
                    // {
                    //     stmt = parseDropTrigger();
                    // }
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
            // CREATE [UNIQUE] INDEX index_name ON table_name [USING index_type] (column_list) [WHERE condition] [TABLESPACE tablespace_name]
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

            // Parse optional USING clause (LSM Integration Phase 2 Task 2.1)
            // Syntax: CREATE INDEX ... ON table USING {BTREE|HASH|LSM|...} (columns)
            StringPool::StringId index_type = 0;
            if (match(TokenType::KW_USING))
            {
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected index type after USING (e.g., BTREE, HASH, LSM), but got " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                    return nullptr;
                }
                index_type = current().value.string_id;
                advance();
            }

            // Expect opening parenthesis
            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after table name"))
            {
                synchronize();
                return nullptr;
            }

            // Parse column/expression list - Task 17
            std::vector<CreateIndexStmt::IndexColumn> index_columns;
            do
            {
                // Check if this is an expression (starts with LEFT_PAREN for explicit grouping)
                // or a function call
                if (check(TokenType::LEFT_PAREN))
                {
                    // Expression index: ((expression))
                    advance(); // consume first (
                    Expression *expr = parseExpression();
                    if (!expr)
                    {
                        error("Failed to parse expression");
                        synchronize();
                        return nullptr;
                    }
                    if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after expression"))
                    {
                        synchronize();
                        return nullptr;
                    }
                    index_columns.push_back(CreateIndexStmt::IndexColumn(expr));
                }
                else if (check(TokenType::IDENTIFIER))
                {
                    // Simple column name (not an expression)
                    // Note: For expression indexes with function calls like LOWER(email),
                    // use explicit parentheses: CREATE INDEX idx ON table ((LOWER(email)))
                    StringPool::StringId col_name = current().value.string_id;
                    advance();
                    index_columns.push_back(CreateIndexStmt::IndexColumn(col_name));
                }
                else
                {
                    error("Expected column name or expression, but got " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                    return nullptr;
                }
            } while (match(TokenType::COMMA));

            // Expect closing parenthesis
            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after column list"))
            {
                synchronize();
                return nullptr;
            }

            // Parse optional WHERE clause (Task 17: Partial/Filtered indexes)
            Expression *where_clause = nullptr;
            if (match(TokenType::KW_WHERE))
            {
                where_clause = parseExpression();
                if (!where_clause)
                {
                    error("Failed to parse WHERE clause");
                    synchronize();
                    return nullptr;
                }
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
            return arena_.make<CreateIndexStmt>(span, index_name, table_name,
                                                std::move(index_columns), where_clause, is_unique,
                                                tablespace_name, index_type);
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

        // Phase 2 Wave 2 - Agent C: Parse CREATE TRIGGER
        Statement *Parser::parseCreateTrigger()
        {
            // CREATE TRIGGER trigger_name BEFORE|AFTER INSERT|UPDATE|DELETE ON table_name
            // FOR EACH ROW EXECUTE PROCEDURE procedure_name()
            
            auto start_loc = previous().location;
            
            // Consume TRIGGER keyword
            if (!consume(TokenType::KW_TRIGGER, "Expected TRIGGER keyword"))
            {
                return nullptr;
            }
            
            // Get trigger name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected trigger name after CREATE TRIGGER");
                return nullptr;
            }
            StringPool::StringId trigger_name = current().value.string_id;
            advance();
            
            // Parse timing: BEFORE or AFTER
            TriggerTiming timing;
            if (match(TokenType::KW_BEFORE))
            {
                timing = TriggerTiming::BEFORE;
            }
            else if (match(TokenType::KW_AFTER))
            {
                timing = TriggerTiming::AFTER;
            }
            else
            {
                error("Expected BEFORE or AFTER after trigger name");
                return nullptr;
            }
            
            // Parse event: INSERT, UPDATE, or DELETE
            TriggerEvent event;
            if (match(TokenType::KW_INSERT))
            {
                event = TriggerEvent::INSERT;
            }
            else if (match(TokenType::KW_UPDATE))
            {
                event = TriggerEvent::UPDATE;
            }
            else if (match(TokenType::KW_DELETE))
            {
                event = TriggerEvent::DELETE;
            }
            else
            {
                error("Expected INSERT, UPDATE, or DELETE after trigger timing");
                return nullptr;
            }
            
            // Parse ON table_name
            if (!consume(TokenType::KW_ON, "Expected ON after trigger event"))
            {
                return nullptr;
            }
            
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after ON");
                return nullptr;
            }
            StringPool::StringId table_name = current().value.string_id;
            advance();

            // Parse FOR ROW (simplified - skip EACH)
            if (!consume(TokenType::KW_FOR, "Expected FOR ROW"))
            {
                return nullptr;
            }
            if (!consume(TokenType::KW_ROW, "Expected FOR ROW"))
            {
                return nullptr;
            }
            
            TriggerGranularity granularity = TriggerGranularity::FOR_EACH_ROW;
            
            // Parse EXECUTE PROCEDURE procedure_name()
            if (!consume(TokenType::KW_EXECUTE, "Expected EXECUTE PROCEDURE"))
            {
                return nullptr;
            }
            if (!consume(TokenType::KW_PROCEDURE, "Expected EXECUTE PROCEDURE"))
            {
                return nullptr;
            }
            
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected procedure name after EXECUTE PROCEDURE");
                return nullptr;
            }
            StringPool::StringId procedure_name = current().value.string_id;
            advance();
            
            // Optional parentheses: procedure_name()
            if (match(TokenType::LEFT_PAREN))
            {
                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after procedure name"))
                {
                    return nullptr;
                }
            }
            
            // Optional semicolon
            match(TokenType::SEMICOLON);
            
            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<CreateTriggerStmt>(span, trigger_name, table_name,
                                                   timing, event, granularity, procedure_name);
        }
        
        // Phase 2 Wave 2 - Agent C: Parse DROP TRIGGER
        Statement *Parser::parseDropTrigger()
        {
            // DROP TRIGGER [IF EXISTS] trigger_name
            
            auto start_loc = previous().location;
            
            // Consume TRIGGER keyword
            if (!consume(TokenType::KW_TRIGGER, "Expected TRIGGER keyword"))
            {
                return nullptr;
            }
            
            // Check for IF EXISTS (future support)
            bool if_exists = false;
            // For now, we'll skip IF EXISTS support
            
            // Get trigger name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected trigger name after DROP TRIGGER");
                return nullptr;
            }
            StringPool::StringId trigger_name = current().value.string_id;
            advance();
            
            // Optional semicolon
            match(TokenType::SEMICOLON);
            
            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<DropTriggerStmt>(span, trigger_name, if_exists);
        }

        // ===== PSQL - Stored Procedures and Functions (Phase 2 Task 10.2) =====

        Statement *Parser::parseCreateFunction()
        {
            // CREATE [OR REPLACE] FUNCTION name ( params ) RETURNS type AS block
            auto start_loc = previous().location;

            // Check for OR REPLACE (not yet in parseStatement switch, so skip for now)
            bool or_replace = false;

            // Consume FUNCTION keyword
            if (!consume(TokenType::KW_FUNCTION, "Expected FUNCTION keyword"))
                return nullptr;

            // Get function name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected function name");
                return nullptr;
            }
            StringPool::StringId func_name = current().value.string_id;
            advance();

            // Parse parameter list
            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after function name"))
                return nullptr;

            std::vector<Parameter*> params;
            if (!check(TokenType::RIGHT_PAREN))
            {
                params = parseParameterList();
            }

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters"))
                return nullptr;

            // Parse RETURNS clause
            if (!consume(TokenType::KW_RETURNS, "Expected RETURNS clause"))
                return nullptr;

            TypeName return_type = parseTypeName();
            TypeName *return_type_ptr = arena_.make<TypeName>(return_type);

            // Parse AS (IS keyword not yet in lexer)
            if (!check(TokenType::KW_AS))
            {
                error("Expected AS before function body");
                return nullptr;
            }
            advance();

            // Parse PSQL block
            BlockStmt *body = parsePSQLBlock();
            if (!body)
                return nullptr;

            auto span = SourceSpan(start_loc, previous().location);
            // Constructor: (span, name, params, return_type, or_replace, body)
            return arena_.make<CreateFunctionStmt>(span, func_name, params, return_type_ptr, or_replace, body);
        }

        Statement *Parser::parseCreateProcedure()
        {
            // CREATE [OR REPLACE] PROCEDURE name ( params ) AS block
            auto start_loc = previous().location;

            bool or_replace = false;

            // Consume PROCEDURE keyword
            if (!consume(TokenType::KW_PROCEDURE, "Expected PROCEDURE keyword"))
                return nullptr;

            // Get procedure name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected procedure name");
                return nullptr;
            }
            StringPool::StringId proc_name = current().value.string_id;
            advance();

            // Parse parameter list
            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after procedure name"))
                return nullptr;

            std::vector<Parameter*> params;
            if (!check(TokenType::RIGHT_PAREN))
            {
                params = parseParameterList();
            }

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after parameters"))
                return nullptr;

            // Parse AS (IS keyword not yet in lexer)
            if (!check(TokenType::KW_AS))
            {
                error("Expected AS before procedure body");
                return nullptr;
            }
            advance();

            // Parse PSQL block
            BlockStmt *body = parsePSQLBlock();
            if (!body)
                return nullptr;

            auto span = SourceSpan(start_loc, previous().location);
            // Constructor: (span, name, params, or_replace, body)
            return arena_.make<CreateProcedureStmt>(span, proc_name, params, or_replace, body);
        }

        std::vector<Parameter*> Parser::parseParameterList()
        {
            std::vector<Parameter*> params;

            do
            {
                Parameter *param = parseParameter();
                params.push_back(param);
            } while (match(TokenType::COMMA));

            return params;
        }

        Parameter* Parser::parseParameter()
        {
            // Check for IN/OUT/INOUT - not yet in lexer, so default to IN
            // TODO: Add IN/OUT/INOUT token support
            ParameterMode mode = ParameterMode::IN;

            // Parameter name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected parameter name");
                return nullptr;
            }
            StringPool::StringId param_name = current().value.string_id;
            advance();

            // Parameter type
            TypeName type = parseTypeName();
            TypeName *type_ptr = arena_.make<TypeName>(type);

            // Optional DEFAULT value
            Expression *default_value = nullptr;
            if (check(TokenType::KW_DEFAULT))
            {
                advance();
                default_value = parseExpression();
            }

            // Allocate Parameter in arena with constructor
            Parameter *param = arena_.make<Parameter>(param_name, type_ptr, mode, default_value);

            return param;
        }

        BlockStmt *Parser::parsePSQLBlock()
        {
            // [DECLARE declarations] BEGIN statements [EXCEPTION handlers] END
            auto start_loc = current().location;

            std::vector<VarDeclarationStmt*> declarations;
            std::vector<Statement*> statements;
            std::vector<ExceptionHandler*> handlers;

            // Optional DECLARE section
            if (check(TokenType::KW_DECLARE))
            {
                advance();
                declarations = parseDeclareSection();
            }

            // BEGIN keyword
            if (!consume(TokenType::KW_BEGIN, "Expected BEGIN to start block"))
                return nullptr;

            // Parse statements until END, EXCEPTION, or error
            while (!check(TokenType::KW_END) && !check(TokenType::KW_EXCEPTION) && !isAtEnd())
            {
                Statement *stmt = nullptr;

                // Dispatch to appropriate statement parser
                if (check(TokenType::KW_IF))
                {
                    advance();
                    stmt = parseIfStatement();
                }
                else if (check(TokenType::KW_LOOP))
                {
                    advance();
                    stmt = parseLoopStatement();
                }
                else if (check(TokenType::KW_WHILE))
                {
                    advance();
                    stmt = parseWhileStatement();
                }
                else if (check(TokenType::KW_EXIT))
                {
                    advance();
                    stmt = parseExitStatement();
                }
                else if (check(TokenType::KW_RETURN))
                {
                    advance();
                    stmt = parseReturnStatement();
                }
                else if (check(TokenType::KW_RAISE))
                {
                    advance();
                    stmt = parseRaiseStatement();
                }
                else if (check(TokenType::IDENTIFIER))
                {
                    stmt = parseAssignmentOrCall();
                }
                else if (check(TokenType::KW_SELECT))
                {
                    advance();
                    stmt = parseSelect();
                }
                else if (check(TokenType::KW_INSERT))
                {
                    advance();
                    stmt = parseInsert();
                }
                else if (check(TokenType::KW_UPDATE))
                {
                    advance();
                    stmt = parseUpdate();
                }
                else if (check(TokenType::KW_DELETE))
                {
                    advance();
                    stmt = parseDelete();
                }
                else
                {
                    error("Unexpected token in PSQL block: " + std::string(tokenTypeToString(current().type)));
                    advance();
                    continue;
                }

                if (stmt)
                    statements.push_back(stmt);

                // Optional semicolon
                match(TokenType::SEMICOLON);
            }

            // Optional EXCEPTION section
            if (check(TokenType::KW_EXCEPTION))
            {
                advance();
                handlers = parseExceptionHandlers();
            }

            // END keyword
            if (!consume(TokenType::KW_END, "Expected END to close block"))
                return nullptr;

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<BlockStmt>(span, declarations, statements, handlers);
        }

        std::vector<VarDeclarationStmt*> Parser::parseDeclareSection()
        {
            std::vector<VarDeclarationStmt*> declarations;

            // Parse declarations until BEGIN
            while (!check(TokenType::KW_BEGIN) && !isAtEnd())
            {
                VarDeclarationStmt *decl = parseVariableDeclaration();
                if (decl)
                    declarations.push_back(decl);

                // Consume semicolon
                if (!consume(TokenType::SEMICOLON, "Expected ';' after variable declaration"))
                    break;
            }

            return declarations;
        }

        VarDeclarationStmt *Parser::parseVariableDeclaration()
        {
            // var_name type [DEFAULT expr]
            auto start_loc = current().location;

            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected variable name");
                return nullptr;
            }
            StringPool::StringId var_name = current().value.string_id;
            advance();

            TypeName var_type = parseTypeName();
            TypeName *var_type_ptr = arena_.make<TypeName>(var_type);

            Expression *default_value = nullptr;
            if (check(TokenType::KW_DEFAULT))
            {
                advance();
                default_value = parseExpression();
            }

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<VarDeclarationStmt>(span, var_name, var_type_ptr, false, default_value);
        }

        Statement *Parser::parseIfStatement()
        {
            // IF condition THEN statements [ELSIF condition THEN statements]* [ELSE statements] END IF
            auto start_loc = previous().location;

            // Parse main condition
            Expression *condition = parseExpression();
            if (!condition)
                return nullptr;

            if (!consume(TokenType::KW_THEN, "Expected THEN after IF condition"))
                return nullptr;

            // Parse then-branch statements (stub for now)
            std::vector<Statement*> then_stmts;

            // Parse ELSIF clauses (stub)
            std::vector<ElsIfClause*> elsif_clauses;

            // Parse optional ELSE clause (stub)
            std::vector<Statement*> else_stmts;

            // END IF
            if (!consume(TokenType::KW_END, "Expected END to close IF statement"))
                return nullptr;
            if (!consume(TokenType::KW_IF, "Expected IF after END"))
                return nullptr;

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<IfStmt>(span, condition, then_stmts, elsif_clauses, else_stmts);
        }

        Statement *Parser::parseLoopStatement()
        {
            // LOOP statements END LOOP
            auto start_loc = previous().location;

            std::vector<Statement*> body;

            if (!consume(TokenType::KW_END, "Expected END to close LOOP"))
                return nullptr;
            if (!consume(TokenType::KW_LOOP, "Expected LOOP after END"))
                return nullptr;

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<LoopStmt>(span, 0, body);  // label=0, then body
        }

        Statement *Parser::parseWhileStatement()
        {
            // WHILE condition DO statements END WHILE - but we don't have DO keyword
            // Simplify to: WHILE condition statements END WHILE
            auto start_loc = previous().location;

            Expression *condition = parseExpression();
            if (!condition)
                return nullptr;

            std::vector<Statement*> body;

            if (!consume(TokenType::KW_END, "Expected END to close WHILE"))
                return nullptr;
            if (!consume(TokenType::KW_WHILE, "Expected WHILE after END"))
                return nullptr;

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<WhileStmt>(span, 0, condition, body);  // label=0, condition, body
        }

        Statement *Parser::parseExitStatement()
        {
            // EXIT [WHEN condition]
            auto start_loc = previous().location;

            Expression *condition = nullptr;
            if (check(TokenType::KW_WHEN))
            {
                advance();
                condition = parseExpression();
            }

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<ExitStmt>(span, 0, condition);  // label=0, condition
        }

        Statement *Parser::parseReturnStatement()
        {
            // RETURN [expression]
            auto start_loc = previous().location;

            Expression *return_value = nullptr;
            if (!check(TokenType::SEMICOLON) && !check(TokenType::KW_END))
            {
                return_value = parseExpression();
            }

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<ReturnStmt>(span, return_value);
        }

        Statement *Parser::parseRaiseStatement()
        {
            // RAISE [EXCEPTION|NOTICE|WARNING] message
            auto start_loc = previous().location;

            RaiseStmt::Level level = RaiseStmt::Level::EXCEPTION;

            // Check for level keyword
            if (check(TokenType::KW_EXCEPTION))
            {
                advance();
                level = RaiseStmt::Level::EXCEPTION;
            }
            // TODO: Add NOTICE, WARNING tokens when needed

            // Parse message expression
            Expression *message = parseExpression();
            if (!message)
            {
                error("Expected message expression after RAISE");
                return nullptr;
            }

            std::vector<Expression*> args;

            auto span = SourceSpan(start_loc, previous().location);
            return arena_.make<RaiseStmt>(span, level, message, args);
        }

        Statement *Parser::parseAssignmentOrCall()
        {
            // identifier := expression  OR  identifier(args)
            // For now, just return nullptr as we need := operator in lexer
            error("Assignment statements require := operator (not yet implemented in lexer)");
            return nullptr;
        }

        std::vector<ExceptionHandler*> Parser::parseExceptionHandlers()
        {
            std::vector<ExceptionHandler*> handlers;

            // WHEN exception_name THEN statements
            while (check(TokenType::KW_WHEN))
            {
                advance();

                StringPool::StringId exception_name = 0;  // Default to OTHERS

                if (check(TokenType::IDENTIFIER))
                {
                    exception_name = current().value.string_id;
                    advance();
                }

                if (!consume(TokenType::KW_THEN, "Expected THEN after WHEN exception"))
                    break;

                // Parse handler statements (stub for now)
                std::vector<Statement*> stmts;

                // Allocate in arena
                ExceptionHandler *handler = arena_.make<ExceptionHandler>(exception_name, stmts);
                handlers.push_back(handler);
            }

            return handlers;
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

            // Parse optional WITH clause (Phase 2 Wave 2: CTE support)
            WithClause *with_clause = parseWithClause();

            // If WITH clause was parsed, we already consumed SELECT, use current location
            // Otherwise, use previous location (SELECT keyword location)
            if (with_clause == nullptr)
            {
                // No WITH clause, start_loc is correct (SELECT keyword)
            }

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

            // Parse FROM clause with potential JOINs (Phase 1 Task 3.1)
            FromClause from_clause = parseFromClause();

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

            // Create SELECT statement
            SelectStmt *stmt;
            if (with_clause)
            {
                // Use constructor with WITH clause
                stmt = arena_.make<SelectStmt>(span, with_clause, std::move(select_list), std::move(from_clause), where_clause);
            }
            else if (!from_clause.joins.empty())
            {
                stmt = arena_.make<SelectStmt>(span, std::move(select_list), std::move(from_clause), where_clause);
            }
            else
            {
                // Legacy path - single table
                stmt = arena_.make<SelectStmt>(span, std::move(select_list), from_clause.base_table.table_name, where_clause);
            }

            // Parse GROUP BY clause (Phase 1 Task 4.1)
            if (match(TokenType::KW_GROUP))
            {
                if (!consume(TokenType::KW_BY, "Expected BY after GROUP"))
                {
                    synchronize();
                    return nullptr;
                }

                GroupByClause group_by = parseGroupByClause();
                stmt->setGroupByClause(std::move(group_by));
            }

            // Parse ORDER BY clause (Phase 1 Task 5.1)
            if (match(TokenType::KW_ORDER))
            {
                if (!consume(TokenType::KW_BY, "Expected BY after ORDER"))
                {
                    synchronize();
                    return nullptr;
                }

                std::vector<OrderByItem> order_by = parseOrderByClause();
                stmt->setOrderByClause(std::move(order_by));
            }

            // Parse LIMIT clause (Phase 1 Task 5.2)
            if (match(TokenType::KW_LIMIT))
            {
                parseLimitClause(stmt);
            }

            return stmt;
        }

        // Parse table reference with optional alias (Phase 1 Task 3.1)
        TableRef Parser::parseTableRef()
        {
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return TableRef(0);
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            // Check for optional alias (AS keyword is optional in SQL)
            StringPool::StringId alias = 0;
            if (match(TokenType::KW_AS) || check(TokenType::IDENTIFIER))
            {
                if (check(TokenType::IDENTIFIER))
                {
                    alias = current().value.string_id;
                    advance();
                }
            }

            return TableRef(table_name, alias);
        }

        // Parse FROM clause with optional JOINs (Phase 1 Task 3.1)
        FromClause Parser::parseFromClause()
        {
            // Parse base table
            TableRef base_table = parseTableRef();
            FromClause from_clause(base_table);

            // Parse optional JOIN clauses
            while (true)
            {
                // Check for JOIN keywords
                bool is_natural = match(TokenType::KW_NATURAL);

                JoinType join_type = JoinType::INNER;

                // Parse join type
                if (match(TokenType::KW_CROSS))
                {
                    join_type = JoinType::CROSS;
                    if (!consume(TokenType::KW_JOIN, "Expected JOIN after CROSS"))
                    {
                        synchronize();
                        break;
                    }
                }
                else if (match(TokenType::KW_INNER))
                {
                    join_type = JoinType::INNER;
                    if (!consume(TokenType::KW_JOIN, "Expected JOIN after INNER"))
                    {
                        synchronize();
                        break;
                    }
                }
                else if (match(TokenType::KW_LEFT))
                {
                    join_type = JoinType::LEFT;
                    match(TokenType::KW_OUTER);  // OUTER is optional
                    if (!consume(TokenType::KW_JOIN, "Expected JOIN after LEFT [OUTER]"))
                    {
                        synchronize();
                        break;
                    }
                }
                else if (match(TokenType::KW_RIGHT))
                {
                    join_type = JoinType::RIGHT;
                    match(TokenType::KW_OUTER);  // OUTER is optional
                    if (!consume(TokenType::KW_JOIN, "Expected JOIN after RIGHT [OUTER]"))
                    {
                        synchronize();
                        break;
                    }
                }
                else if (match(TokenType::KW_FULL))
                {
                    join_type = JoinType::FULL;
                    match(TokenType::KW_OUTER);  // OUTER is optional
                    if (!consume(TokenType::KW_JOIN, "Expected JOIN after FULL [OUTER]"))
                    {
                        synchronize();
                        break;
                    }
                }
                else if (match(TokenType::KW_JOIN))
                {
                    // Plain JOIN defaults to INNER JOIN
                    join_type = JoinType::INNER;
                }
                else
                {
                    // No more joins
                    break;
                }

                // Parse right table
                TableRef right_table = parseTableRef();

                // Parse join condition
                JoinConditionType condition_type;
                Expression *on_condition = nullptr;
                std::vector<StringPool::StringId> using_columns;

                if (join_type == JoinType::CROSS)
                {
                    // CROSS JOIN has no condition
                    condition_type = JoinConditionType::CROSS;
                }
                else if (is_natural)
                {
                    // NATURAL JOIN
                    condition_type = JoinConditionType::NATURAL;
                }
                else if (match(TokenType::KW_ON))
                {
                    // ON condition
                    condition_type = JoinConditionType::ON;
                    on_condition = parseExpression();
                    if (!on_condition)
                    {
                        synchronize();
                        break;
                    }
                }
                else if (match(TokenType::KW_USING))
                {
                    // USING (column_list)
                    condition_type = JoinConditionType::USING;
                    if (!consume(TokenType::LEFT_PAREN, "Expected '(' after USING"))
                    {
                        synchronize();
                        break;
                    }

                    do
                    {
                        if (!check(TokenType::IDENTIFIER))
                        {
                            error("Expected column name in USING clause");
                            synchronize();
                            break;
                        }
                        using_columns.push_back(current().value.string_id);
                        advance();
                    } while (match(TokenType::COMMA));

                    if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after USING column list"))
                    {
                        synchronize();
                        break;
                    }
                }
                else
                {
                    error("Expected ON or USING after JOIN");
                    synchronize();
                    break;
                }

                // Create and add the join clause
                JoinClause join_clause(join_type, is_natural, right_table, condition_type, on_condition);
                join_clause.using_columns = std::move(using_columns);
                from_clause.joins.push_back(std::move(join_clause));
            }

            return from_clause;
        }

        // Parse WITH clause (CTEs) - Phase 2 Wave 2
        WithClause *Parser::parseWithClause()
        {
            // Check if WITH keyword is present
            if (!check(TokenType::KW_WITH))
            {
                return nullptr; // No WITH clause
            }

            auto start_loc = current().location;
            advance(); // consume WITH

            std::vector<CTEDefinition> ctes;

            // Parse CTEs: cte_name [(col1, col2, ...)] AS (SELECT ...)
            do
            {
                // Parse CTE name
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected CTE name after WITH");
                    synchronize();
                    return nullptr;
                }

                StringPool::StringId cte_name = current().value.string_id;
                advance();

                // Optional column aliases: (col1, col2, ...)
                std::vector<StringPool::StringId> column_aliases;
                if (match(TokenType::LEFT_PAREN))
                {
                    if (!check(TokenType::RIGHT_PAREN))
                    {
                        do
                        {
                            if (!check(TokenType::IDENTIFIER))
                            {
                                error("Expected column name in CTE column list");
                                synchronize();
                                return nullptr;
                            }
                            column_aliases.push_back(current().value.string_id);
                            advance();
                        } while (match(TokenType::COMMA));
                    }

                    if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after CTE column list"))
                    {
                        synchronize();
                        return nullptr;
                    }
                }

                // Expect AS
                if (!consume(TokenType::KW_AS, "Expected AS after CTE name"))
                {
                    synchronize();
                    return nullptr;
                }

                // Parse CTE query: (SELECT ...)
                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after AS"))
                {
                    synchronize();
                    return nullptr;
                }

                // Parse SELECT statement
                if (!consume(TokenType::KW_SELECT, "Expected SELECT in CTE definition"))
                {
                    synchronize();
                    return nullptr;
                }

                SelectStmt *cte_query = dynamic_cast<SelectStmt *>(parseSelect());
                if (!cte_query)
                {
                    error("Expected SELECT statement in CTE definition");
                    synchronize();
                    return nullptr;
                }

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after CTE query"))
                {
                    synchronize();
                    return nullptr;
                }

                ctes.emplace_back(cte_name, cte_query, std::move(column_aliases));

            } while (match(TokenType::COMMA));

            // After all CTEs are parsed, we expect SELECT
            if (!consume(TokenType::KW_SELECT, "Expected SELECT after WITH clause"))
            {
                synchronize();
                return nullptr;
            }

            return arena_.make<WithClause>(std::move(ctes));
        }

        Statement *Parser::parseUpdate()
        {
            // UPDATE table_name SET column1 = value1, column2 = value2, ... [WHERE condition]
            // Phase 1 Task 2.1: UPDATE statement
            auto start_loc = previous().location;

            // Parse table name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after UPDATE, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return nullptr;
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            // Expect SET keyword
            if (!consume(TokenType::KW_SET, "Expected SET after table name in UPDATE"))
            {
                synchronize();
                return nullptr;
            }

            // Parse assignments: column = value, column = value, ...
            std::vector<Assignment> assignments;
            do
            {
                if (!check(TokenType::IDENTIFIER))
                {
                    error("Expected column name in SET clause, but got " +
                          std::string(tokenTypeToString(current().type)));
                    synchronize();
                    return nullptr;
                }

                StringPool::StringId column_name = current().value.string_id;
                advance();

                if (!consume(TokenType::EQUAL, "Expected '=' after column name in SET clause"))
                {
                    synchronize();
                    return nullptr;
                }

                Expression *value = parseExpression();
                if (!value)
                {
                    synchronize();
                    return nullptr;
                }

                assignments.emplace_back(column_name, value);

            } while (match(TokenType::COMMA));

            // Optional WHERE clause
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
            return arena_.make<UpdateStmt>(span, table_name, std::move(assignments), where_clause);
        }

        Statement *Parser::parseDelete()
        {
            // DELETE FROM table_name [WHERE condition]
            // Phase 1 Task 2.2: DELETE statement
            auto start_loc = previous().location;

            // Expect FROM keyword
            if (!consume(TokenType::KW_FROM, "Expected FROM after DELETE"))
            {
                synchronize();
                return nullptr;
            }

            // Parse table name
            if (!check(TokenType::IDENTIFIER))
            {
                error("Expected table name after DELETE FROM, but got " +
                      std::string(tokenTypeToString(current().type)));
                synchronize();
                return nullptr;
            }

            StringPool::StringId table_name = current().value.string_id;
            advance();

            // Optional WHERE clause
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
            return arena_.make<DeleteStmt>(span, table_name, where_clause);
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

        Statement *Parser::parseExplain()
        {
            // EXPLAIN SELECT ...
            // Phase 1 Task 1.5: EXPLAIN command
            auto start_loc = previous().location;

            // Parse the statement to explain
            // For Phase 1.5, only SELECT is supported
            auto query_result = parseStatement();

            if (!query_result.success() || !query_result.statement())
            {
                error("Expected statement after EXPLAIN");
                synchronize();
                return nullptr;
            }

            Statement *query_stmt = query_result.statement();

            // Verify it's a SELECT statement (Phase 1.5 limitation)
            if (query_stmt->kind() != ASTKind::SELECT)
            {
                error("EXPLAIN currently only supports SELECT statements");
                synchronize();
                return nullptr;
            }

            auto span = makeSpan(start_loc);
            return arena_.make<ExplainStmt>(span, query_stmt);
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
                // Array operators (Phase 2 Task 12)
                else if (match(TokenType::AMPERSAND_AMPERSAND))
                {
                    op = BinaryOp::ARRAY_OVERLAP;
                }
                else if (match(TokenType::AT_GREATER))
                {
                    op = BinaryOp::ARRAY_CONTAINS;
                }
                else if (match(TokenType::LESS_AT))
                {
                    op = BinaryOp::ARRAY_CONTAINED_BY;
                }
                // Regex operators (Phase 2 Task 13)
                else if (match(TokenType::TILDE))
                {
                    op = BinaryOp::REGEX_MATCH;
                }
                else if (match(TokenType::TILDE_STAR))
                {
                    op = BinaryOp::REGEX_MATCH_CI;
                }
                else if (match(TokenType::EXCLAIM_TILDE))
                {
                    op = BinaryOp::REGEX_NOT_MATCH;
                }
                else if (match(TokenType::EXCLAIM_TILDE_STAR))
                {
                    op = BinaryOp::REGEX_NOT_MATCH_CI;
                }
                // IN and NOT IN operators (Phase 2 Wave 2 - Agent B)
                else if (check(TokenType::KW_NOT) || check(TokenType::KW_IN))
                {
                    bool is_not = false;
                    if (match(TokenType::KW_NOT))
                    {
                        is_not = true;
                        if (!consume(TokenType::KW_IN, "Expected IN after NOT"))
                            return nullptr;
                    }
                    else
                    {
                        advance();  // consume IN
                    }

                    if (!consume(TokenType::LEFT_PAREN, "Expected '(' after IN"))
                        return nullptr;

                    // Check if it's a subquery
                    if (match(TokenType::KW_SELECT))
                    {
                        SelectStmt *subquery = dynamic_cast<SelectStmt *>(parseSelect());
                        if (!subquery)
                            return nullptr;

                        if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after IN subquery"))
                            return nullptr;

                        SubqueryType subquery_type = is_not ? SubqueryType::NOT_IN : SubqueryType::IN;
                        auto *subquery_expr = arena_.make<SubqueryExpr>(
                            makeSpan(subquery->span().start, previous().location),
                            subquery,
                            subquery_type
                        );

                        BinaryOp bin_op = is_not ? BinaryOp::NOT_IN : BinaryOp::IN;
                        auto span = makeSpan(expr->span().start, previous().location);
                        expr = arena_.make<BinaryOpExpr>(span, bin_op, expr, subquery_expr);
                        continue;
                    }
                    else
                    {
                        // IN with value list: IN (1, 2, 3)
                        // For now, we'll parse this as a series of OR comparisons
                        // This is a simplified implementation
                        error("IN with value list not yet implemented - use subquery");
                        return nullptr;
                    }
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

            // Handle JSON operators as postfix operators (Phase 1 Task 7)
            while (match(TokenType::ARROW) || match(TokenType::DOUBLE_ARROW) ||
                   match(TokenType::HASH_ARROW) || match(TokenType::HASH_DOUBLE_ARROW))
            {
                TokenType op_type = previous().type;
                JSONFunc json_op;

                switch (op_type)
                {
                case TokenType::ARROW:
                    json_op = JSONFunc::ARROW;
                    break;
                case TokenType::DOUBLE_ARROW:
                    json_op = JSONFunc::DOUBLE_ARROW;
                    break;
                case TokenType::HASH_ARROW:
                    json_op = JSONFunc::HASH_ARROW;
                    break;
                case TokenType::HASH_DOUBLE_ARROW:
                    json_op = JSONFunc::HASH_DOUBLE_ARROW;
                    break;
                default:
                    error("Unknown JSON operator");
                    return nullptr;
                }

                // Parse right-hand side (field name or path)
                auto *right = parsePrimary();
                if (!right)
                    return nullptr;

                auto span = makeSpan(expr->span().start, right->span().end);
                std::vector<Expression*> args = {expr, right};
                expr = arena_.make<JSONFuncExpr>(span, json_op, args);
            }

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

            // Aggregate functions (Phase 1 Task 4.1, Phase 2 Task 12)
            if (match(TokenType::KW_COUNT) || match(TokenType::KW_SUM) ||
                match(TokenType::KW_AVG) || match(TokenType::KW_MIN) || match(TokenType::KW_MAX) ||
                match(TokenType::KW_ARRAY_AGG))
            {
                TokenType agg_type = previous().type;
                AggregateFunc agg_func;

                switch (agg_type)
                {
                case TokenType::KW_COUNT:
                    agg_func = AggregateFunc::COUNT;
                    break;
                case TokenType::KW_SUM:
                    agg_func = AggregateFunc::SUM;
                    break;
                case TokenType::KW_AVG:
                    agg_func = AggregateFunc::AVG;
                    break;
                case TokenType::KW_MIN:
                    agg_func = AggregateFunc::MIN;
                    break;
                case TokenType::KW_MAX:
                    agg_func = AggregateFunc::MAX;
                    break;
                case TokenType::KW_ARRAY_AGG:
                    agg_func = AggregateFunc::ARRAY_AGG;
                    break;
                default:
                    error("Unknown aggregate function");
                    return nullptr;
                }

                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after aggregate function"))
                    return nullptr;

                // Check for DISTINCT
                bool distinct = false;
                if (match(TokenType::KW_DISTINCT))
                {
                    distinct = true;
                }

                // Parse argument (or * for COUNT(*))
                Expression *arg = nullptr;
                if (match(TokenType::STAR))
                {
                    // COUNT(*) - arg remains nullptr
                    if (agg_func != AggregateFunc::COUNT)
                    {
                        error("* is only valid with COUNT");
                        return nullptr;
                    }
                }
                else
                {
                    arg = parseExpression();
                    if (!arg)
                        return nullptr;
                }

                auto end_loc = current().location;
                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after aggregate function"))
                    return nullptr;

                auto span = makeSpan(start_loc, end_loc);
                return arena_.make<AggregateExpr>(span, agg_func, arg, distinct);
            }

            // Window functions (Phase 1 Task 6)
            if (match(TokenType::KW_ROW_NUMBER) || match(TokenType::KW_RANK) ||
                match(TokenType::KW_DENSE_RANK) || match(TokenType::KW_LAG) ||
                match(TokenType::KW_LEAD) || match(TokenType::KW_FIRST_VALUE) ||
                match(TokenType::KW_LAST_VALUE) || match(TokenType::KW_NTH_VALUE))
            {
                TokenType win_type = previous().type;
                WindowFunc win_func;

                switch (win_type)
                {
                case TokenType::KW_ROW_NUMBER:
                    win_func = WindowFunc::ROW_NUMBER;
                    break;
                case TokenType::KW_RANK:
                    win_func = WindowFunc::RANK;
                    break;
                case TokenType::KW_DENSE_RANK:
                    win_func = WindowFunc::DENSE_RANK;
                    break;
                case TokenType::KW_LAG:
                    win_func = WindowFunc::LAG;
                    break;
                case TokenType::KW_LEAD:
                    win_func = WindowFunc::LEAD;
                    break;
                case TokenType::KW_FIRST_VALUE:
                    win_func = WindowFunc::FIRST_VALUE;
                    break;
                case TokenType::KW_LAST_VALUE:
                    win_func = WindowFunc::LAST_VALUE;
                    break;
                case TokenType::KW_NTH_VALUE:
                    win_func = WindowFunc::NTH_VALUE;
                    break;
                default:
                    error("Unknown window function");
                    return nullptr;
                }

                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after window function"))
                    return nullptr;

                // Parse arguments
                std::vector<Expression*> args;
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

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after window function arguments"))
                    return nullptr;

                // Parse OVER clause (required for window functions)
                if (!consume(TokenType::KW_OVER, "Expected OVER clause after window function"))
                    return nullptr;

                auto *window_spec = parseWindowSpec();
                if (!window_spec)
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<WindowFuncExpr>(span, win_func, args, window_spec);
            }

            // JSON functions (Phase 1 Task 7)
            if (match(TokenType::KW_JSON_EXTRACT) || match(TokenType::KW_JSON_OBJECT) ||
                match(TokenType::KW_JSON_ARRAY) || match(TokenType::KW_JSON_SET) ||
                match(TokenType::KW_JSON_INSERT) || match(TokenType::KW_JSON_REMOVE) ||
                match(TokenType::KW_JSONB_EXTRACT_PATH) || match(TokenType::KW_JSONB_BUILD_OBJECT) ||
                match(TokenType::KW_JSONB_BUILD_ARRAY) || match(TokenType::KW_JSONB_SET))
            {
                TokenType json_type = previous().type;
                JSONFunc json_func;

                switch (json_type)
                {
                case TokenType::KW_JSON_EXTRACT:
                    json_func = JSONFunc::JSON_EXTRACT;
                    break;
                case TokenType::KW_JSON_OBJECT:
                    json_func = JSONFunc::JSON_OBJECT;
                    break;
                case TokenType::KW_JSON_ARRAY:
                    json_func = JSONFunc::JSON_ARRAY;
                    break;
                case TokenType::KW_JSON_SET:
                    json_func = JSONFunc::JSON_SET;
                    break;
                case TokenType::KW_JSON_INSERT:
                    json_func = JSONFunc::JSON_INSERT;
                    break;
                case TokenType::KW_JSON_REMOVE:
                    json_func = JSONFunc::JSON_REMOVE;
                    break;
                case TokenType::KW_JSONB_EXTRACT_PATH:
                    json_func = JSONFunc::JSONB_EXTRACT_PATH;
                    break;
                case TokenType::KW_JSONB_BUILD_OBJECT:
                    json_func = JSONFunc::JSONB_BUILD_OBJECT;
                    break;
                case TokenType::KW_JSONB_BUILD_ARRAY:
                    json_func = JSONFunc::JSONB_BUILD_ARRAY;
                    break;
                case TokenType::KW_JSONB_SET:
                    json_func = JSONFunc::JSONB_SET;
                    break;
                default:
                    error("Unknown JSON function");
                    return nullptr;
                }

                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after JSON function"))
                    return nullptr;

                // Parse arguments
                std::vector<Expression*> args;
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

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after JSON function arguments"))
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<JSONFuncExpr>(span, json_func, args);
            }

            // COALESCE function (Phase 1 Task 8)
            if (match(TokenType::KW_COALESCE))
            {
                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after COALESCE"))
                    return nullptr;

                std::vector<Expression*> args;
                do
                {
                    auto *arg = parseExpression();
                    if (!arg)
                        return nullptr;
                    args.push_back(arg);
                } while (match(TokenType::COMMA));

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after COALESCE arguments"))
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<CoalesceExpr>(span, args);
            }

            // NULLIF function (Phase 1 Task 8)
            if (match(TokenType::KW_NULLIF))
            {
                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after NULLIF"))
                    return nullptr;

                auto *expr1 = parseExpression();
                if (!expr1)
                    return nullptr;

                if (!consume(TokenType::COMMA, "Expected ',' between NULLIF arguments"))
                    return nullptr;

                auto *expr2 = parseExpression();
                if (!expr2)
                    return nullptr;

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after NULLIF arguments"))
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<NullIfExpr>(span, expr1, expr2);
            }

            // CASE expression (Phase 1 Task 8)
            if (match(TokenType::KW_CASE))
            {
                Expression* case_operand = nullptr;
                std::vector<CaseExpr::WhenClause> when_clauses;
                Expression* else_result = nullptr;

                // Check if this is simple CASE (has operand before WHEN)
                if (!check(TokenType::KW_WHEN))
                {
                    case_operand = parseExpression();
                    if (!case_operand)
                        return nullptr;
                }

                // Parse WHEN clauses
                while (match(TokenType::KW_WHEN))
                {
                    auto *condition = parseExpression();
                    if (!condition)
                        return nullptr;

                    if (!consume(TokenType::KW_THEN, "Expected THEN after WHEN condition"))
                        return nullptr;

                    auto *result = parseExpression();
                    if (!result)
                        return nullptr;

                    when_clauses.push_back({condition, result});
                }

                if (when_clauses.empty())
                {
                    error("CASE expression must have at least one WHEN clause");
                    return nullptr;
                }

                // Optional ELSE clause
                if (match(TokenType::KW_ELSE))
                {
                    else_result = parseExpression();
                    if (!else_result)
                        return nullptr;
                }

                if (!consume(TokenType::KW_END, "Expected END to close CASE expression"))
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                if (case_operand)
                {
                    // Simple CASE
                    return arena_.make<CaseExpr>(span, case_operand, when_clauses, else_result);
                }
                else
                {
                    // Searched CASE
                    return arena_.make<CaseExpr>(span, when_clauses, else_result);
                }
            }

            // ARRAY literal: ARRAY[elem1, elem2, ...] (Phase 2 Task 12)
            if (match(TokenType::KW_ARRAY))
            {
                if (!consume(TokenType::LEFT_BRACKET, "Expected '[' after ARRAY"))
                    return nullptr;

                std::vector<Expression*> elements;

                // Handle empty array
                if (!check(TokenType::RIGHT_BRACKET))
                {
                    do
                    {
                        auto *elem = parseExpression();
                        if (!elem)
                            return nullptr;
                        elements.push_back(elem);
                    } while (match(TokenType::COMMA));
                }

                if (!consume(TokenType::RIGHT_BRACKET, "Expected ']' after array elements"))
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<ArrayLiteral>(span, elements);
            }

            // Array functions (Phase 2 Task 12)
            if (match(TokenType::KW_ARRAY_TO_STRING) || match(TokenType::KW_STRING_TO_ARRAY) ||
                match(TokenType::KW_ARRAY_APPEND) || match(TokenType::KW_ARRAY_PREPEND) ||
                match(TokenType::KW_ARRAY_CAT) || match(TokenType::KW_ARRAY_REMOVE) ||
                match(TokenType::KW_ARRAY_REPLACE) || match(TokenType::KW_ARRAY_LENGTH) ||
                match(TokenType::KW_ARRAY_DIMS) || match(TokenType::KW_ARRAY_UPPER) ||
                match(TokenType::KW_ARRAY_LOWER) || match(TokenType::KW_UNNEST))
            {
                // Store the function name for later use
                StringPool::StringId func_name = stringPool().intern(
                    tokenTypeToString(previous().type)
                );

                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after array function"))
                    return nullptr;

                std::vector<Expression*> args;

                // Parse function arguments
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

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after array function arguments"))
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<FunctionCallExpr>(span, func_name, args);
            }

            if (check(TokenType::IDENTIFIER))
            {
                auto name = current().value.string_id;
                advance();

                // Check for qualified name (table.column) - Phase 1 Task 3.1
                if (match(TokenType::DOT))
                {
                    // This is a qualified identifier
                    StringPool::StringId qualifier = name;

                    if (!check(TokenType::IDENTIFIER))
                    {
                        error("Expected column name after '.'");
                        return nullptr;
                    }

                    StringPool::StringId column_name = current().value.string_id;
                    advance();

                    auto span = makeSpan(start_loc, previous().location);
                    return arena_.make<IdentifierExpr>(span, qualifier, column_name);
                }
                // Check if this is a function call
                else if (match(TokenType::LEFT_PAREN))
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

            // EXISTS subquery (Phase 2 Wave 2 - Agent B)
            if (match(TokenType::KW_EXISTS))
            {
                if (!consume(TokenType::LEFT_PAREN, "Expected '(' after EXISTS"))
                    return nullptr;

                if (!consume(TokenType::KW_SELECT, "Expected SELECT after EXISTS ("))
                    return nullptr;

                SelectStmt *subquery = dynamic_cast<SelectStmt *>(parseSelect());
                if (!subquery)
                    return nullptr;

                if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after EXISTS subquery"))
                    return nullptr;

                auto span = makeSpan(start_loc, previous().location);
                return arena_.make<SubqueryExpr>(span, subquery, SubqueryType::EXISTS);
            }

            if (match(TokenType::LEFT_PAREN))
            {
                // Check if this is a scalar subquery (SELECT ...)
                if (match(TokenType::KW_SELECT))
                {
                    SelectStmt *subquery = dynamic_cast<SelectStmt *>(parseSelect());
                    if (!subquery)
                        return nullptr;

                    if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after subquery"))
                        return nullptr;

                    auto span = makeSpan(start_loc, previous().location);
                    return arena_.make<SubqueryExpr>(span, subquery, SubqueryType::SCALAR);
                }

                // Otherwise it's a grouped expression
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

        // ===== Aggregation Parsing (Phase 1 Task 4.1) =====

        // Parse GROUP BY clause with optional HAVING
        GroupByClause Parser::parseGroupByClause()
        {
            GroupByClause clause;

            // Parse grouping expressions
            do
            {
                Expression *expr = parseExpression();
                if (!expr)
                {
                    error("Expected expression in GROUP BY");
                    synchronize();
                    return clause;
                }
                clause.grouping_exprs.push_back(expr);
            } while (match(TokenType::COMMA));

            // Parse optional HAVING clause
            if (match(TokenType::KW_HAVING))
            {
                clause.having_clause = parseExpression();
                if (!clause.having_clause)
                {
                    error("Expected expression after HAVING");
                    synchronize();
                }
            }

            return clause;
        }

        // Parse ORDER BY clause
        std::vector<OrderByItem> Parser::parseOrderByClause()
        {
            std::vector<OrderByItem> items;

            do
            {
                Expression *expr = parseExpression();
                if (!expr)
                {
                    error("Expected expression in ORDER BY");
                    synchronize();
                    return items;
                }

                // Parse optional ASC/DESC
                SortOrder order = SortOrder::ASC;  // Default
                if (match(TokenType::KW_ASC))
                {
                    order = SortOrder::ASC;
                }
                else if (match(TokenType::KW_DESC))
                {
                    order = SortOrder::DESC;
                }

                // Parse optional NULLS FIRST/LAST (not implementing for now, use default)
                NullsOrder nulls_order = NullsOrder::DEFAULT;

                items.push_back(OrderByItem(expr, order, nulls_order));
            } while (match(TokenType::COMMA));

            return items;
        }

        // Parse LIMIT/OFFSET clause
        void Parser::parseLimitClause(SelectStmt *stmt)
        {
            // LIMIT count
            if (!check(TokenType::INTEGER_LITERAL))
            {
                error("Expected integer literal after LIMIT");
                synchronize();
                return;
            }

            int64_t limit_count = current().value.int_value;
            advance();

            stmt->setLimitCount(limit_count);

            // Optional OFFSET
            if (match(TokenType::KW_OFFSET))
            {
                if (!check(TokenType::INTEGER_LITERAL))
                {
                    error("Expected integer literal after OFFSET");
                    synchronize();
                    return;
                }

                int64_t offset_count = current().value.int_value;
                advance();

                stmt->setOffsetCount(offset_count);
            }
        }

        // Phase 1 Task 6: Window function parsing
        WindowSpec *Parser::parseWindowSpec()
        {
            auto start_loc = current().location;

            if (!consume(TokenType::LEFT_PAREN, "Expected '(' after OVER"))
                return nullptr;

            auto *spec = arena_.make<WindowSpec>(makeSpan(start_loc));

            // Parse PARTITION BY clause (optional)
            if (match(TokenType::KW_PARTITION))
            {
                if (!consume(TokenType::KW_BY, "Expected BY after PARTITION"))
                    return nullptr;

                do
                {
                    auto *expr = parseExpression();
                    if (!expr)
                        return nullptr;
                    spec->addPartitionBy(expr);
                } while (match(TokenType::COMMA));
            }

            // Parse ORDER BY clause (optional)
            if (match(TokenType::KW_ORDER))
            {
                if (!consume(TokenType::KW_BY, "Expected BY after ORDER"))
                    return nullptr;

                do
                {
                    auto *expr = parseExpression();
                    if (!expr)
                        return nullptr;

                    // Parse ASC/DESC (optional, default ASC)
                    bool ascending = true;
                    if (match(TokenType::KW_DESC))
                    {
                        ascending = false;
                    }
                    else
                    {
                        match(TokenType::KW_ASC); // Optional ASC keyword
                    }

                    // Parse NULLS FIRST/LAST (optional)
                    bool nulls_first = !ascending; // Default: NULLS LAST for ASC, NULLS FIRST for DESC
                    if (match(TokenType::KW_NULLS))
                    {
                        if (match(TokenType::KW_FIRST))
                        {
                            nulls_first = true;
                        }
                        else if (match(TokenType::KW_LAST))
                        {
                            nulls_first = false;
                        }
                        else
                        {
                            error("Expected FIRST or LAST after NULLS");
                            return nullptr;
                        }
                    }

                    spec->addOrderBy(expr, ascending, nulls_first);
                } while (match(TokenType::COMMA));
            }

            // Parse frame clause (optional)
            if (check(TokenType::KW_ROWS) || check(TokenType::KW_RANGE))
            {
                parseFrameClause(spec);
            }

            if (!consume(TokenType::RIGHT_PAREN, "Expected ')' after window specification"))
                return nullptr;

            return spec;
        }

        void Parser::parseFrameClause(WindowSpec *spec)
        {
            // Parse frame mode (ROWS or RANGE)
            FrameMode mode;
            if (match(TokenType::KW_ROWS))
            {
                mode = FrameMode::ROWS;
            }
            else if (match(TokenType::KW_RANGE))
            {
                mode = FrameMode::RANGE;
            }
            else
            {
                error("Expected ROWS or RANGE for frame clause");
                return;
            }

            // Parse BETWEEN or single boundary
            if (match(TokenType::KW_BETWEEN))
            {
                // Parse start boundary
                FrameBoundary start = parseFrameBoundary();

                if (!consume(TokenType::KW_AND, "Expected AND in frame clause"))
                    return;

                // Parse end boundary
                FrameBoundary end = parseFrameBoundary();

                spec->setFrame(mode, start, end);
            }
            else
            {
                // Single boundary - this is the start, end is CURRENT ROW
                FrameBoundary start = parseFrameBoundary();
                FrameBoundary end(FrameBoundaryType::CURRENT_ROW);
                spec->setFrame(mode, start, end);
            }
        }

        FrameBoundary Parser::parseFrameBoundary()
        {
            if (match(TokenType::KW_UNBOUNDED))
            {
                if (match(TokenType::KW_PRECEDING))
                {
                    return FrameBoundary(FrameBoundaryType::UNBOUNDED_PRECEDING);
                }
                else if (match(TokenType::KW_FOLLOWING))
                {
                    return FrameBoundary(FrameBoundaryType::UNBOUNDED_FOLLOWING);
                }
                else
                {
                    error("Expected PRECEDING or FOLLOWING after UNBOUNDED");
                    return FrameBoundary(FrameBoundaryType::CURRENT_ROW);
                }
            }

            if (match(TokenType::KW_CURRENT))
            {
                if (!consume(TokenType::KW_ROW, "Expected ROW after CURRENT"))
                    return FrameBoundary(FrameBoundaryType::CURRENT_ROW);

                return FrameBoundary(FrameBoundaryType::CURRENT_ROW);
            }

            // Parse offset expression
            auto *offset_expr = parseExpression();
            if (!offset_expr)
                return FrameBoundary(FrameBoundaryType::CURRENT_ROW);

            if (match(TokenType::KW_PRECEDING))
            {
                return FrameBoundary(FrameBoundaryType::PRECEDING, offset_expr);
            }
            else if (match(TokenType::KW_FOLLOWING))
            {
                return FrameBoundary(FrameBoundaryType::FOLLOWING, offset_expr);
            }
            else
            {
                error("Expected PRECEDING or FOLLOWING after offset expression");
                return FrameBoundary(FrameBoundaryType::CURRENT_ROW);
            }
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