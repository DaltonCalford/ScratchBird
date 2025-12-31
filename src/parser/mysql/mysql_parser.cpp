/**
 * MySQL Parser Implementation
 *
 * Recursive-descent parser for MySQL 8.0 SQL that generates SBLR bytecode
 * directly for execution by the ScratchBird engine.
 */

#include "scratchbird/parser/mysql/mysql_parser.h"
#include "scratchbird/core/catalog_manager.h"
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace scratchbird::parser::mysql {

// ============================================================================
// Helper Functions for Non-Reserved Keywords
// ============================================================================

// Check if a keyword token can be used as an identifier in expression context
// MySQL has many "non-reserved" keywords that don't need backtick quoting
static bool isNonReservedKeyword(TokenType type) {
    switch (type) {
        // Common column names that are keywords
        case TokenType::KW_STATUS:
        case TokenType::KW_DATA:
        case TokenType::KW_VALUE:
        case TokenType::KW_COMMENT:
        case TokenType::KW_PASSWORD:
        case TokenType::KW_USER:
        case TokenType::KW_ROLE:
        case TokenType::KW_EVENT:
        case TokenType::KW_ACTION:
        case TokenType::KW_PROFILE:
        case TokenType::KW_LOCAL:
        case TokenType::KW_GLOBAL:
        case TokenType::KW_SESSION:
        case TokenType::KW_ALGORITHM:
        case TokenType::KW_ENGINE:
        case TokenType::KW_LANGUAGE:
        case TokenType::KW_CONDITION:
        case TokenType::KW_YEAR:
        case TokenType::KW_DATE:
        case TokenType::KW_TIME:
        case TokenType::KW_TIMESTAMP:
        case TokenType::KW_TEXT:
        case TokenType::KW_JSON:
        case TokenType::KW_BOOL:
        case TokenType::KW_BOOLEAN:
        case TokenType::KW_FIRST:
        case TokenType::KW_LAST:
        case TokenType::KW_OPTION:
        case TokenType::KW_WORK:
        case TokenType::KW_CHAIN:
        case TokenType::KW_LEVEL:
        case TokenType::KW_OPEN:
        case TokenType::KW_CLOSE:
        case TokenType::KW_VARIABLES:
        case TokenType::KW_WARNINGS:
        case TokenType::KW_ERRORS:
            return true;
        default:
            return false;
    }
}

// Convert token type to string name for column reference
static std::string tokenToString(TokenType type) {
    switch (type) {
        case TokenType::KW_STATUS: return "status";
        case TokenType::KW_DATA: return "data";
        case TokenType::KW_VALUE: return "value";
        case TokenType::KW_COMMENT: return "comment";
        case TokenType::KW_PASSWORD: return "password";
        case TokenType::KW_USER: return "user";
        case TokenType::KW_ROLE: return "role";
        case TokenType::KW_EVENT: return "event";
        case TokenType::KW_ACTION: return "action";
        case TokenType::KW_PROFILE: return "profile";
        case TokenType::KW_LOCAL: return "local";
        case TokenType::KW_GLOBAL: return "global";
        case TokenType::KW_SESSION: return "session";
        case TokenType::KW_ALGORITHM: return "algorithm";
        case TokenType::KW_ENGINE: return "engine";
        case TokenType::KW_LANGUAGE: return "language";
        case TokenType::KW_CONDITION: return "condition";
        case TokenType::KW_YEAR: return "year";
        case TokenType::KW_DATE: return "date";
        case TokenType::KW_TIME: return "time";
        case TokenType::KW_TIMESTAMP: return "timestamp";
        case TokenType::KW_TEXT: return "text";
        case TokenType::KW_JSON: return "json";
        case TokenType::KW_BOOL: return "bool";
        case TokenType::KW_BOOLEAN: return "boolean";
        case TokenType::KW_FIRST: return "first";
        case TokenType::KW_LAST: return "last";
        case TokenType::KW_OPTION: return "option";
        case TokenType::KW_WORK: return "work";
        case TokenType::KW_CHAIN: return "chain";
        case TokenType::KW_LEVEL: return "level";
        case TokenType::KW_OPEN: return "open";
        case TokenType::KW_CLOSE: return "close";
        case TokenType::KW_VARIABLES: return "variables";
        case TokenType::KW_WARNINGS: return "warnings";
        case TokenType::KW_ERRORS: return "errors";
        default: return "";
    }
}

// ============================================================================
// Parser Construction
// ============================================================================

Parser::Parser(std::string_view input, core::Database* db, std::string_view default_schema)
    : lexer_(input)
    , db_(db)
    , default_schema_(default_schema)
{
    // Prime the parser with first token
    advance();
}

// ============================================================================
// Token Management
// ============================================================================

void Parser::advance() {
    current_token_ = lexer_.nextToken();
}

bool Parser::check(TokenType type) const {
    return current_token_.type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

static std::string buildEmulatedServerRoot(const std::string& default_schema) {
    if (default_schema.empty()) {
        return default_schema;
    }

    std::vector<std::string> parts;
    std::string current;
    for (char ch : default_schema) {
        if (ch == '/' || ch == '.') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }
    if (!current.empty()) {
        parts.push_back(current);
    }

    if (parts.size() > 4) {
        parts.resize(4);
    }

    std::string root;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            root.push_back('.');
        }
        root += parts[i];
    }
    return root;
}

bool Parser::matchKeyword(TokenType kw) {
    return match(kw);
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        Token t = current_token_;
        advance();
        return t;
    }
    error(message);
    return current_token_;
}

Token Parser::consumeKeyword(TokenType kw, const std::string& message) {
    return consume(kw, message);
}

// ============================================================================
// Error Handling
// ============================================================================

void Parser::error(const std::string& message) {
    errors_.emplace_back(message, current_token_.span.start);
}

void Parser::synchronize() {
    // Skip tokens until we find a statement boundary
    while (!check(TokenType::END_OF_FILE)) {
        if (check(TokenType::SEMICOLON)) {
            advance();
            return;
        }

        // Keywords that start new statements
        switch (current_token_.type) {
            case TokenType::KW_SELECT:
            case TokenType::KW_INSERT:
            case TokenType::KW_UPDATE:
            case TokenType::KW_DELETE:
            case TokenType::KW_CREATE:
            case TokenType::KW_ALTER:
            case TokenType::KW_DROP:
            case TokenType::KW_BEGIN:
            case TokenType::KW_COMMIT:
            case TokenType::KW_ROLLBACK:
            case TokenType::KW_SET:
            case TokenType::KW_SHOW:
            case TokenType::KW_USE:
                return;
            default:
                advance();
        }
    }
}

// ============================================================================
// Bytecode Emission
// ============================================================================

void Parser::emit(sblr::Opcode op) {
    bytecode_.push_back(static_cast<uint8_t>(op));
}

void Parser::emitByte(uint8_t byte) {
    bytecode_.push_back(byte);
}

void Parser::emitU16(uint16_t val) {
    bytecode_.push_back(val & 0xFF);
    bytecode_.push_back((val >> 8) & 0xFF);
}

void Parser::emitU32(uint32_t val) {
    bytecode_.push_back(val & 0xFF);
    bytecode_.push_back((val >> 8) & 0xFF);
    bytecode_.push_back((val >> 16) & 0xFF);
    bytecode_.push_back((val >> 24) & 0xFF);
}

void Parser::emitU64(uint64_t val) {
    for (int i = 0; i < 8; i++) {
        bytecode_.push_back((val >> (i * 8)) & 0xFF);
    }
}

void Parser::emitI64(int64_t val) {
    emitU64(static_cast<uint64_t>(val));
}

void Parser::emitF64(double val) {
    uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    emitU64(bits);
}

void Parser::emitString(std::string_view str) {
    emitU32(static_cast<uint32_t>(str.size()));
    for (char c : str) {
        bytecode_.push_back(static_cast<uint8_t>(c));
    }
}

// ============================================================================
// Main Parsing Entry Points
// ============================================================================

ParseResult Parser::parseStatement() {
    bytecode_.clear();
    errors_.clear();

    // Emit SBLR version header
    emit(sblr::Opcode::VERSION);
    emitByte(sblr::SBLR_VERSION);

    try {
        parseStatementInternal();

        // Check for extra tokens after the statement
        // A valid statement should end at EOF or SEMICOLON
        if (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
            error("Unexpected token after statement");
        }
    } catch (const std::exception& e) {
        error(e.what());
    }

    // Emit end marker
    emit(sblr::Opcode::END);

    ParseResult result;
    if (!errors_.empty()) {
        for (const auto& err : errors_) {
            result.addError(err.message, err.location);
        }
    } else {
        result.setBytecode(std::move(bytecode_));
    }
    return result;
}

std::vector<ParseResult> Parser::parseAll() {
    std::vector<ParseResult> results;

    while (!check(TokenType::END_OF_FILE)) {
        results.push_back(parseStatement());

        // Skip optional semicolon between statements
        match(TokenType::SEMICOLON);
    }

    return results;
}

// ============================================================================
// Statement Dispatch
// ============================================================================

void Parser::parseStatementInternal() {
    switch (current_token_.type) {
        case TokenType::KW_SELECT:
            parseSelectStmt();
            break;
        case TokenType::KW_INSERT:
            parseInsertStmt();
            break;
        case TokenType::KW_UPDATE:
            parseUpdateStmt();
            break;
        case TokenType::KW_DELETE:
            parseDeleteStmt();
            break;
        case TokenType::KW_REPLACE:
            parseReplaceStmt();
            break;
        case TokenType::KW_CREATE:
            parseCreateStmt();
            break;
        case TokenType::KW_RENAME:
            parseRenameStmt();
            break;
        case TokenType::KW_ALTER:
            parseAlterStmt();
            break;
        case TokenType::KW_DROP:
            parseDropStmt();
            break;
        case TokenType::KW_TRUNCATE:
            parseTruncateStmt();
            break;
        case TokenType::KW_SET:
            parseSetStmt();
            break;
        case TokenType::KW_SHOW:
            parseShowStmt();
            break;
        case TokenType::KW_DESCRIBE:
            parseDescribeStmt();
            break;
        case TokenType::KW_USE:
            parseUseStmt();
            break;
        case TokenType::KW_BEGIN:
        case TokenType::KW_START:
            parseBeginStmt();
            break;
        case TokenType::KW_COMMIT:
            parseCommitStmt();
            break;
        case TokenType::KW_ROLLBACK:
            parseRollbackStmt();
            break;
        case TokenType::KW_SAVEPOINT:
            parseSavepointStmt();
            break;
        case TokenType::KW_RELEASE:
            parseReleaseStmt();
            break;
        case TokenType::KW_LOCK:
            parseLockStmt();
            break;
        case TokenType::KW_UNLOCK:
            parseUnlockStmt();
            break;
        default:
            error("Expected statement");
            synchronize();
    }
}

// ============================================================================
// Identifier Parsing
// ============================================================================

std::string Parser::parseIdentifier() {
    if (check(TokenType::IDENTIFIER)) {
        uint32_t id = current_token_.value.string_id;
        advance();
        return std::string(lexer_.stringPool().get(id));
    }
    if (check(TokenType::BACKTICK_IDENTIFIER)) {
        uint32_t id = current_token_.value.string_id;
        advance();
        return std::string(lexer_.stringPool().get(id));
    }
    error("Expected identifier");
    return "";
}

std::string Parser::parseQualifiedName() {
    std::string name = parseIdentifier();

    while (match(TokenType::DOT)) {
        name += ".";
        name += parseIdentifier();
    }

    return name;
}

void Parser::resolveTableName(std::string& schema, std::string& table) {
    auto normalize_path = [](const std::string& path) {
        std::vector<std::string> parts;
        std::string current;
        for (char ch : path) {
            if (ch == '/' || ch == '.') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }

        std::string normalized;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                normalized.push_back('.');
            }
            normalized += parts[i];
        }
        return normalized;
    };

    std::string normalized_default = normalize_path(default_schema_);

    // If no schema specified, use default
    if (schema.empty()) {
        schema = normalized_default;
        return;
    }

    std::string normalized_schema = normalize_path(schema);
    if (normalized_schema.rfind("remote.emulated.mysql.", 0) == 0 ||
        normalized_schema == "remote.emulated.mysql")
    {
        schema = normalized_schema;
        return;
    }

    std::string server_root = buildEmulatedServerRoot(default_schema_);
    if (!server_root.empty()) {
        schema = server_root + "." + normalized_schema;
    } else {
        schema = normalized_schema;
    }
}

// ============================================================================
// SELECT Statement
// ============================================================================

void Parser::parseSelectStmt() {
    consume(TokenType::KW_SELECT, "Expected SELECT");
    emit(sblr::Opcode::SELECT);

    // Handle SELECT modifiers
    bool is_distinct = false;
    if (matchKeyword(TokenType::KW_DISTINCT) || matchKeyword(TokenType::KW_DISTINCTROW)) {
        is_distinct = true;
    } else {
        matchKeyword(TokenType::KW_ALL);  // Optional, default
    }

    // Emit DISTINCT flag
    emitByte(is_distinct ? 1 : 0);

    // Parse select list
    parseSelectList();

    // FROM clause (optional for MySQL)
    if (matchKeyword(TokenType::KW_FROM)) {
        parseFromClause();
    } else {
        // No FROM - emit null table ref
        emitU32(0);  // 0 tables
    }

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);  // No WHERE
    }

    // GROUP BY clause
    if (matchKeyword(TokenType::KW_GROUP)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after GROUP");
        parseGroupByClause();
    } else {
        emitU32(0);  // 0 group by columns
    }

    // HAVING clause
    if (matchKeyword(TokenType::KW_HAVING)) {
        parseHavingClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);  // No HAVING
    }

    // ORDER BY clause
    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
        parseOrderByClause();
    } else {
        emitU32(0);  // 0 order by columns
    }

    // LIMIT clause
    if (matchKeyword(TokenType::KW_LIMIT)) {
        parseLimitClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);  // No LIMIT
    }
}

void Parser::parseSelectList() {
    emit(sblr::Opcode::BEGIN_LIST);

    // Count position for patching
    size_t count_pos = bytecode_.size();
    emitU32(0);  // Placeholder for count

    uint32_t count = 0;

    if (match(TokenType::STAR)) {
        // SELECT *
        emit(sblr::Opcode::SELECT_STAR);
        count = 1;
    } else {
        do {
            // Parse expression
            parseExpression();

            // Optional alias
            std::string alias;
            if (matchKeyword(TokenType::KW_AS)) {
                alias = parseIdentifier();
            } else if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                alias = parseIdentifier();
            }

            // Emit alias (empty string if none)
            emitString(alias);

            count++;
        } while (match(TokenType::COMMA));
    }

    // Patch the count
    sblr::writeInt32(&bytecode_[count_pos], count);

    emit(sblr::Opcode::END_LIST);
}

void Parser::parseFromClause() {
    emit(sblr::Opcode::BEGIN_LIST);

    size_t count_pos = bytecode_.size();
    emitU32(0);  // Placeholder

    uint32_t count = 0;

    do {
        // Parse table reference
        std::string schema;
        std::string table = parseIdentifier();

        // Check for schema.table
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }

        resolveTableName(schema, table);

        emit(sblr::Opcode::TABLE_REF);
        emitString(schema + "/" + table);

        // Optional alias
        std::string alias;
        if (matchKeyword(TokenType::KW_AS)) {
            alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            // Check if this looks like an alias (not a keyword)
            if (!check(TokenType::KW_WHERE) && !check(TokenType::KW_GROUP) &&
                !check(TokenType::KW_ORDER) && !check(TokenType::KW_LIMIT) &&
                !check(TokenType::KW_JOIN) && !check(TokenType::KW_LEFT) &&
                !check(TokenType::KW_RIGHT) && !check(TokenType::KW_INNER) &&
                !check(TokenType::KW_CROSS) && !check(TokenType::KW_ON)) {
                alias = parseIdentifier();
            }
        }
        emitString(alias);

        count++;

        // Handle JOINs
        while (check(TokenType::KW_JOIN) || check(TokenType::KW_LEFT) ||
               check(TokenType::KW_RIGHT) || check(TokenType::KW_INNER) ||
               check(TokenType::KW_CROSS) || check(TokenType::KW_NATURAL) ||
               check(TokenType::KW_STRAIGHT_JOIN)) {

            // Join type
            sblr::Opcode join_op = sblr::Opcode::NESTED_LOOP_JOIN;
            uint8_t join_type = 0;  // 0=INNER, 1=LEFT, 2=RIGHT, 3=FULL, 4=CROSS

            if (matchKeyword(TokenType::KW_NATURAL)) {
                // Natural join - ignore for now
            }

            if (matchKeyword(TokenType::KW_LEFT)) {
                join_type = 1;
                matchKeyword(TokenType::KW_OUTER);
            } else if (matchKeyword(TokenType::KW_RIGHT)) {
                join_type = 2;
                matchKeyword(TokenType::KW_OUTER);
            } else if (matchKeyword(TokenType::KW_CROSS)) {
                join_type = 4;
            } else if (matchKeyword(TokenType::KW_INNER)) {
                join_type = 0;
            } else if (matchKeyword(TokenType::KW_STRAIGHT_JOIN)) {
                join_type = 0;  // Treat as inner join
            }

            consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");

            emit(sblr::Opcode::JOIN_TYPE);
            emitByte(join_type);

            // Parse joined table
            std::string j_schema;
            std::string j_table = parseIdentifier();
            if (match(TokenType::DOT)) {
                j_schema = j_table;
                j_table = parseIdentifier();
            }
            resolveTableName(j_schema, j_table);

            emit(sblr::Opcode::TABLE_REF);
            emitString(j_schema + "/" + j_table);

            // Optional alias for joined table
            std::string j_alias;
            if (matchKeyword(TokenType::KW_AS)) {
                j_alias = parseIdentifier();
            } else if (check(TokenType::IDENTIFIER)) {
                if (!check(TokenType::KW_ON) && !check(TokenType::KW_USING) &&
                    !check(TokenType::KW_WHERE)) {
                    j_alias = parseIdentifier();
                }
            }
            emitString(j_alias);

            // ON or USING clause
            if (matchKeyword(TokenType::KW_ON)) {
                emit(sblr::Opcode::JOIN_CONDITION);
                parseExpression();
            } else if (matchKeyword(TokenType::KW_USING)) {
                consume(TokenType::LEFT_PAREN, "Expected ( after USING");
                // Parse column list
                emit(sblr::Opcode::BEGIN_LIST);
                size_t using_count_pos = bytecode_.size();
                emitU32(0);
                uint32_t using_count = 0;

                do {
                    std::string col = parseIdentifier();
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(col);
                    using_count++;
                } while (match(TokenType::COMMA));

                sblr::writeInt32(&bytecode_[using_count_pos], using_count);
                emit(sblr::Opcode::END_LIST);
                consume(TokenType::RIGHT_PAREN, "Expected ) after USING columns");
            } else if (join_type != 4) {  // Not CROSS JOIN
                // Implicit join condition (for some MySQL compatibility)
                emit(sblr::Opcode::LITERAL_NULL);
            }

            count++;
        }

    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
}

void Parser::parseWhereClause() {
    emit(sblr::Opcode::WHERE_CLAUSE);
    parseExpression();
}

void Parser::parseGroupByClause() {
    emit(sblr::Opcode::GROUP_BY);
    emit(sblr::Opcode::BEGIN_LIST);

    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    do {
        parseExpression();
        count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    // Handle WITH ROLLUP
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_ROLLUP)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GROUP_ROLLUP));
        }
    }
}

void Parser::parseHavingClause() {
    emit(sblr::Opcode::HAVING);
    parseExpression();
}

void Parser::parseOrderByClause() {
    emit(sblr::Opcode::ORDER_BY);
    emit(sblr::Opcode::BEGIN_LIST);

    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    do {
        emit(sblr::Opcode::SORT_KEY);
        parseExpression();

        // Direction
        if (matchKeyword(TokenType::KW_DESC)) {
            emit(sblr::Opcode::SORT_DESC);
        } else {
            matchKeyword(TokenType::KW_ASC);  // Optional
            emit(sblr::Opcode::SORT_ASC);
        }

        // NULLS FIRST/LAST (MySQL 8.0+)
        if (matchKeyword(TokenType::KW_NULLS)) {
            if (matchKeyword(TokenType::KW_FIRST)) {
                emit(sblr::Opcode::NULLS_FIRST);
            } else if (matchKeyword(TokenType::KW_LAST)) {
                emit(sblr::Opcode::NULLS_LAST);
            }
        }

        count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
}

void Parser::parseLimitClause() {
    emit(sblr::Opcode::LIMIT);

    // Parse limit value
    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(current_token_.value.int_value);
        advance();
    } else {
        parseExpression();
    }

    // OFFSET
    if (matchKeyword(TokenType::KW_OFFSET)) {
        emit(sblr::Opcode::OFFSET);
        if (check(TokenType::INTEGER_LITERAL)) {
            emit(sblr::Opcode::LITERAL_INT64);
            emitI64(current_token_.value.int_value);
            advance();
        } else {
            parseExpression();
        }
    } else if (match(TokenType::COMMA)) {
        // MySQL-style LIMIT offset, count
        emit(sblr::Opcode::OFFSET);
        // The first number was offset, this is count
        // Need to swap - but for simplicity, treat as offset
        if (check(TokenType::INTEGER_LITERAL)) {
            emit(sblr::Opcode::LITERAL_INT64);
            emitI64(current_token_.value.int_value);
            advance();
        } else {
            parseExpression();
        }
    }
}

// ============================================================================
// Expression Parsing (Operator Precedence)
// ============================================================================

void Parser::parseExpression() {
    parseOrExpr();
}

void Parser::parseOrExpr() {
    parseXorExpr();

    while (matchKeyword(TokenType::KW_OR) || match(TokenType::OR_OP)) {
        parseXorExpr();
        emit(sblr::Opcode::EXPR_OR);
    }
}

void Parser::parseXorExpr() {
    parseAndExpr();

    while (matchKeyword(TokenType::KW_XOR)) {
        parseAndExpr();
        // XOR can be implemented as (A OR B) AND NOT (A AND B)
        // For now, emit as special opcode or implement in executor
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_XOR));
    }
}

void Parser::parseAndExpr() {
    parseNotExpr();

    while (matchKeyword(TokenType::KW_AND) || match(TokenType::AND_OP)) {
        parseNotExpr();
        emit(sblr::Opcode::EXPR_AND);
    }
}

void Parser::parseNotExpr() {
    if (matchKeyword(TokenType::KW_NOT) || match(TokenType::NOT_OP)) {
        parseNotExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_NOT));
        return;
    }
    parseComparisonExpr();
}

void Parser::parseComparisonExpr() {
    parseBitwiseOrExpr();

    // Handle comparison operators
    if (match(TokenType::EQUAL)) {
        parseBitwiseOrExpr();
        emit(sblr::Opcode::EXPR_EQ);
    } else if (match(TokenType::NOT_EQUAL)) {
        parseBitwiseOrExpr();
        emit(sblr::Opcode::EXPR_NE);
    } else if (match(TokenType::LESS_THAN)) {
        parseBitwiseOrExpr();
        emit(sblr::Opcode::EXPR_LT);
    } else if (match(TokenType::GREATER_THAN)) {
        parseBitwiseOrExpr();
        emit(sblr::Opcode::EXPR_GT);
    } else if (match(TokenType::LESS_EQUAL)) {
        parseBitwiseOrExpr();
        emit(sblr::Opcode::EXPR_LE);
    } else if (match(TokenType::GREATER_EQUAL)) {
        parseBitwiseOrExpr();
        emit(sblr::Opcode::EXPR_GE);
    } else if (match(TokenType::NULL_SAFE_EQUAL)) {
        // MySQL's <=> operator
        parseBitwiseOrExpr();
        emit(sblr::Opcode::EXPR_EQ);  // TODO: NULL-safe semantics
    } else if (matchKeyword(TokenType::KW_IS)) {
        // IS [NOT] NULL / IS [NOT] TRUE / IS [NOT] FALSE
        bool is_not = matchKeyword(TokenType::KW_NOT);

        if (matchKeyword(TokenType::KW_NULL)) {
            emit(sblr::Opcode::LITERAL_NULL);
            emit(is_not ? sblr::Opcode::EXPR_NE : sblr::Opcode::EXPR_EQ);
        } else if (matchKeyword(TokenType::KW_TRUE)) {
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(1);
            emit(is_not ? sblr::Opcode::EXPR_NE : sblr::Opcode::EXPR_EQ);
        } else if (matchKeyword(TokenType::KW_FALSE)) {
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(0);
            emit(is_not ? sblr::Opcode::EXPR_NE : sblr::Opcode::EXPR_EQ);
        }
    } else if (matchKeyword(TokenType::KW_IN)) {
        // IN (value_list) or IN (subquery)
        consume(TokenType::LEFT_PAREN, "Expected ( after IN");

        if (check(TokenType::KW_SELECT)) {
            // Subquery
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_IN));
            parseSelectStmt();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_END));
        } else {
            // Value list
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_IN_LIST));
            emit(sblr::Opcode::BEGIN_LIST);

            size_t count_pos = bytecode_.size();
            emitU32(0);

            uint32_t count = 0;
            do {
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));

            sblr::writeInt32(&bytecode_[count_pos], count);
            emit(sblr::Opcode::END_LIST);
        }

        consume(TokenType::RIGHT_PAREN, "Expected ) after IN list");
    } else if (matchKeyword(TokenType::KW_BETWEEN)) {
        // BETWEEN low AND high
        parseBitwiseOrExpr();
        consumeKeyword(TokenType::KW_AND, "Expected AND in BETWEEN");
        parseBitwiseOrExpr();
        // Emit as (expr >= low AND expr <= high)
        emit(sblr::Opcode::EXPR_GE);
        emit(sblr::Opcode::EXPR_LE);
        emit(sblr::Opcode::EXPR_AND);
    } else if (matchKeyword(TokenType::KW_LIKE)) {
        parseAdditiveExpr();
        emit(sblr::Opcode::EXPR_LIKE);

        // Optional ESCAPE
        if (matchKeyword(TokenType::KW_ESCAPE)) {
            parseAdditiveExpr();
            // TODO: Handle escape character
        }
    } else if (matchKeyword(TokenType::KW_REGEXP) || matchKeyword(TokenType::KW_RLIKE)) {
        parseAdditiveExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_REGEX_MATCH));
    }
}

void Parser::parseBitwiseOrExpr() {
    parseBitwiseXorExpr();

    while (match(TokenType::PIPE)) {
        parseBitwiseXorExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_OR));
    }
}

void Parser::parseBitwiseXorExpr() {
    parseBitwiseAndExpr();

    while (match(TokenType::CARET)) {
        parseBitwiseAndExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_XOR));
    }
}

void Parser::parseBitwiseAndExpr() {
    parseShiftExpr();

    while (match(TokenType::AMPERSAND)) {
        parseShiftExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_AND));
    }
}

void Parser::parseShiftExpr() {
    parseAdditiveExpr();

    while (true) {
        if (match(TokenType::SHIFT_LEFT)) {
            parseAdditiveExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_SHIFT_LEFT));
        } else if (match(TokenType::SHIFT_RIGHT)) {
            parseAdditiveExpr();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_SHIFT_RIGHT));
        } else {
            break;
        }
    }
}

void Parser::parseAdditiveExpr() {
    parseMultiplicativeExpr();

    while (true) {
        if (match(TokenType::PLUS)) {
            parseMultiplicativeExpr();
            emit(sblr::Opcode::EXPR_ADD);
        } else if (match(TokenType::MINUS)) {
            parseMultiplicativeExpr();
            emit(sblr::Opcode::EXPR_SUBTRACT);
        } else {
            break;
        }
    }
}

void Parser::parseMultiplicativeExpr() {
    parseUnaryExpr();

    while (true) {
        if (match(TokenType::STAR)) {
            parseUnaryExpr();
            emit(sblr::Opcode::EXPR_MULTIPLY);
        } else if (match(TokenType::SLASH)) {
            parseUnaryExpr();
            emit(sblr::Opcode::EXPR_DIVIDE);
        } else if (match(TokenType::PERCENT) || match(TokenType::DIV)) {
            parseUnaryExpr();
            emit(sblr::Opcode::EXPR_MODULO);
        } else {
            break;
        }
    }
}

void Parser::parseUnaryExpr() {
    if (match(TokenType::MINUS)) {
        parseUnaryExpr();
        // Negate: multiply by -1
        emit(sblr::Opcode::LITERAL_INT32);
        emitU32(static_cast<uint32_t>(-1));
        emit(sblr::Opcode::EXPR_MULTIPLY);
        return;
    }
    if (match(TokenType::PLUS)) {
        parseUnaryExpr();
        return;
    }
    if (match(TokenType::TILDE)) {
        parseUnaryExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_NOT));
        return;
    }
    if (match(TokenType::NOT_OP)) {
        parseUnaryExpr();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_BIT_NOT));
        return;
    }

    parsePrimaryExpr();
}

void Parser::parsePrimaryExpr() {
    // Literals
    if (check(TokenType::INTEGER_LITERAL)) {
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(current_token_.value.int_value);
        advance();
        return;
    }

    if (check(TokenType::FLOAT_LITERAL)) {
        emit(sblr::Opcode::LITERAL_DOUBLE);
        emitF64(current_token_.value.float_value);
        advance();
        return;
    }

    if (check(TokenType::STRING_LITERAL)) {
        emit(sblr::Opcode::LITERAL_STRING);
        std::string_view str = lexer_.stringPool().get(current_token_.value.string_id);
        emitString(str);
        advance();
        return;
    }

    if (check(TokenType::HEX_LITERAL)) {
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(current_token_.value.int_value);
        advance();
        return;
    }

    if (check(TokenType::BIT_LITERAL)) {
        emit(sblr::Opcode::LITERAL_INT64);
        emitI64(current_token_.value.int_value);
        advance();
        return;
    }

    // Boolean literals
    if (matchKeyword(TokenType::KW_TRUE)) {
        emit(sblr::Opcode::LITERAL_INT32);
        emitU32(1);
        return;
    }

    if (matchKeyword(TokenType::KW_FALSE)) {
        emit(sblr::Opcode::LITERAL_INT32);
        emitU32(0);
        return;
    }

    // NULL
    if (matchKeyword(TokenType::KW_NULL)) {
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }

    // User variable
    if (check(TokenType::USER_VARIABLE)) {
        std::string_view var = lexer_.stringPool().get(current_token_.value.string_id);
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_VAR_LOAD));
        emitString(var);
        advance();
        return;
    }

    // System variable
    if (check(TokenType::SYSTEM_VARIABLE)) {
        std::string_view var = lexer_.stringPool().get(current_token_.value.string_id);
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_VARIABLE));
        emitString(var);
        advance();
        return;
    }

    // Placeholder (?)
    if (match(TokenType::PLACEHOLDER)) {
        emit(sblr::Opcode::LITERAL_NULL);  // TODO: Proper placeholder handling
        return;
    }

    // Parenthesized expression or subquery
    if (match(TokenType::LEFT_PAREN)) {
        if (check(TokenType::KW_SELECT)) {
            // Scalar subquery
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_SCALAR));
            parseSelectStmt();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_END));
        } else {
            parseExpression();
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // CASE expression
    if (check(TokenType::KW_CASE)) {
        parseCaseExpr();
        return;
    }

    // CAST expression
    if (check(TokenType::KW_CAST)) {
        parseCastExpr();
        return;
    }

    // EXISTS
    if (matchKeyword(TokenType::KW_EXISTS)) {
        consume(TokenType::LEFT_PAREN, "Expected ( after EXISTS");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_EXISTS));
        parseSelectStmt();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_END));
        consume(TokenType::RIGHT_PAREN, "Expected ) after EXISTS subquery");
        return;
    }

    // Identifier or function call
    if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
        std::string name = parseIdentifier();

        // Check if function call
        if (match(TokenType::LEFT_PAREN)) {
            parseFunctionCall(name);
            return;
        }

        // Column reference (possibly qualified)
        std::string schema, table, column = name;

        if (match(TokenType::DOT)) {
            table = column;
            column = parseIdentifier();

            if (match(TokenType::DOT)) {
                schema = table;
                table = column;
                column = parseIdentifier();
            }
        }

        emit(sblr::Opcode::COLUMN_REF);
        if (!schema.empty()) {
            emitString(schema);
        } else {
            emitString("");
        }
        if (!table.empty()) {
            emitString(table);
        } else {
            emitString("");
        }
        emitString(column);
        return;
    }

    // Many MySQL keywords can be used as identifiers in expression context
    // (non-reserved keywords like STATUS, DATA, NAME, etc.)
    if (isNonReservedKeyword(current_token_.type)) {
        std::string name = tokenToString(current_token_.type);
        advance();

        // Check if function call
        if (match(TokenType::LEFT_PAREN)) {
            parseFunctionCall(name);
            return;
        }

        // Column reference (possibly qualified)
        std::string schema, table, column = name;

        if (match(TokenType::DOT)) {
            table = column;
            if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                column = parseIdentifier();
            } else if (isNonReservedKeyword(current_token_.type)) {
                column = tokenToString(current_token_.type);
                advance();
            }

            if (match(TokenType::DOT)) {
                schema = table;
                table = column;
                if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                    column = parseIdentifier();
                } else if (isNonReservedKeyword(current_token_.type)) {
                    column = tokenToString(current_token_.type);
                    advance();
                }
            }
        }

        emit(sblr::Opcode::COLUMN_REF);
        emitString(schema.empty() ? "" : schema);
        emitString(table.empty() ? "" : table);
        emitString(column);
        return;
    }

    error("Expected expression");
}

void Parser::parseFunctionCall(const std::string& name) {
    // Map function names to opcodes
    std::string upper_name = name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                   [](char c) { return std::toupper(c); });

    // Aggregate functions
    if (upper_name == "COUNT") {
        emit(sblr::Opcode::AGG_COUNT);
        if (match(TokenType::STAR)) {
            emit(sblr::Opcode::SELECT_STAR);
        } else {
            bool distinct = matchKeyword(TokenType::KW_DISTINCT);
            emitByte(distinct ? 1 : 0);
            parseExpression();
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "SUM") {
        emit(sblr::Opcode::AGG_SUM);
        bool distinct = matchKeyword(TokenType::KW_DISTINCT);
        emitByte(distinct ? 1 : 0);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "AVG") {
        emit(sblr::Opcode::AGG_AVG);
        bool distinct = matchKeyword(TokenType::KW_DISTINCT);
        emitByte(distinct ? 1 : 0);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "MIN") {
        emit(sblr::Opcode::AGG_MIN);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "MAX") {
        emit(sblr::Opcode::AGG_MAX);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // String functions
    if (upper_name == "LENGTH" || upper_name == "CHAR_LENGTH" || upper_name == "CHARACTER_LENGTH") {
        emit(sblr::Opcode::FUNC_LENGTH);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "UPPER" || upper_name == "UCASE") {
        emit(sblr::Opcode::FUNC_UPPER);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "LOWER" || upper_name == "LCASE") {
        emit(sblr::Opcode::FUNC_LOWER);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "SUBSTRING" || upper_name == "SUBSTR" || upper_name == "MID") {
        emit(sblr::Opcode::FUNC_SUBSTRING);
        parseExpression();  // string
        if (match(TokenType::COMMA) || matchKeyword(TokenType::KW_FROM)) {
            parseExpression();  // start
        }
        if (match(TokenType::COMMA) || matchKeyword(TokenType::KW_FOR)) {
            parseExpression();  // length
        } else {
            emit(sblr::Opcode::LITERAL_NULL);  // no length
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "TRIM") {
        emit(sblr::Opcode::FUNC_TRIM);
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Mathematical functions
    if (upper_name == "ABS") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_ABS));
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "ROUND") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_ROUND));
        parseExpression();
        if (match(TokenType::COMMA)) {
            parseExpression();  // decimal places
        } else {
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(0);
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "FLOOR") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_FLOOR));
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "CEIL" || upper_name == "CEILING") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_CEIL));
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "SQRT") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_SQRT));
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "POWER" || upper_name == "POW") {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_POWER));
        parseExpression();
        consume(TokenType::COMMA, "Expected ,");
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Date functions
    if (upper_name == "NOW" || upper_name == "CURRENT_TIMESTAMP") {
        emit(sblr::Opcode::FUNC_NOW);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "CURDATE" || upper_name == "CURRENT_DATE") {
        emit(sblr::Opcode::FUNC_CURRENT_DATE);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Conditional functions
    if (upper_name == "COALESCE") {
        emit(sblr::Opcode::COALESCE);
        emit(sblr::Opcode::BEGIN_LIST);

        size_t count_pos = bytecode_.size();
        emitU32(0);

        uint32_t count = 0;
        do {
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "IFNULL" || upper_name == "NVL") {
        emit(sblr::Opcode::COALESCE);
        emit(sblr::Opcode::BEGIN_LIST);
        emitU32(2);
        parseExpression();
        consume(TokenType::COMMA, "Expected ,");
        parseExpression();
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "NULLIF") {
        emit(sblr::Opcode::NULLIF);
        parseExpression();
        consume(TokenType::COMMA, "Expected ,");
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "IF") {
        emit(sblr::Opcode::CASE_WHEN);
        parseExpression();  // condition
        consume(TokenType::COMMA, "Expected ,");
        parseExpression();  // then value
        consume(TokenType::COMMA, "Expected ,");
        parseExpression();  // else value
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // JSON functions
    if (upper_name == "JSON_EXTRACT") {
        emit(sblr::Opcode::JSON_EXTRACT);
        parseExpression();
        consume(TokenType::COMMA, "Expected ,");
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    if (upper_name == "JSON_OBJECT") {
        emit(sblr::Opcode::JSON_OBJECT);
        emit(sblr::Opcode::BEGIN_LIST);

        size_t count_pos = bytecode_.size();
        emitU32(0);

        uint32_t count = 0;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));
        }

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return;
    }

    // Generic function call (unknown function)
    // Emit as literal function name + argument list
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CALL));
    emitString(upper_name);

    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            parseExpression();
            count++;
        } while (match(TokenType::COMMA));
    }

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);
    consume(TokenType::RIGHT_PAREN, "Expected )");
}

void Parser::parseCaseExpr() {
    consume(TokenType::KW_CASE, "Expected CASE");
    emit(sblr::Opcode::CASE_WHEN);

    // Simple or searched CASE
    bool is_simple = false;
    if (!check(TokenType::KW_WHEN)) {
        // Simple CASE - has case value
        is_simple = true;
        parseExpression();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);  // No case value for searched CASE
    }

    // WHEN clauses
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);

    uint32_t count = 0;
    while (matchKeyword(TokenType::KW_WHEN)) {
        parseExpression();  // WHEN condition/value
        consumeKeyword(TokenType::KW_THEN, "Expected THEN");
        parseExpression();  // THEN result
        count++;
    }

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    // ELSE clause
    if (matchKeyword(TokenType::KW_ELSE)) {
        parseExpression();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);
    }

    consumeKeyword(TokenType::KW_END, "Expected END");
}

void Parser::parseCastExpr() {
    consume(TokenType::KW_CAST, "Expected CAST");
    consume(TokenType::LEFT_PAREN, "Expected (");

    emit(sblr::Opcode::EXPR_CAST);
    parseExpression();

    consumeKeyword(TokenType::KW_AS, "Expected AS");

    // Parse target type
    MySQLDataType type = parseDataType();

    // Emit type opcode
    emit(typeToOpcode(type.kind));
    if (type.length > 0) {
        emitU32(type.length);
    }
    if (type.precision > 0) {
        emitU16(type.precision);
        emitU16(type.scale);
    }

    consume(TokenType::RIGHT_PAREN, "Expected )");
}

// ============================================================================
// INSERT Statement
// ============================================================================

void Parser::parseInsertStmt() {
    consume(TokenType::KW_INSERT, "Expected INSERT");

    // Optional modifiers
    matchKeyword(TokenType::KW_LOW_PRIORITY);
    matchKeyword(TokenType::KW_DELAYED);
    matchKeyword(TokenType::KW_HIGH_PRIORITY);
    matchKeyword(TokenType::KW_IGNORE);

    consumeKeyword(TokenType::KW_INTO, "Expected INTO");

    emit(sblr::Opcode::INSERT);

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitString(schema + "/" + table);

    // Optional column list
    emit(sblr::Opcode::BEGIN_LIST);
    size_t col_count_pos = bytecode_.size();
    emitU32(0);

    uint32_t col_count = 0;
    if (match(TokenType::LEFT_PAREN)) {
        do {
            std::string col = parseIdentifier();
            emit(sblr::Opcode::COLUMN_REF);
            emitString("");  // schema
            emitString("");  // table
            emitString(col);
            col_count++;
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    sblr::writeInt32(&bytecode_[col_count_pos], col_count);
    emit(sblr::Opcode::END_LIST);

    // VALUES or SELECT
    if (matchKeyword(TokenType::KW_VALUES) || matchKeyword(TokenType::KW_VALUE)) {
        // VALUES clause
        emit(sblr::Opcode::BEGIN_LIST);
        size_t row_count_pos = bytecode_.size();
        emitU32(0);

        uint32_t row_count = 0;
        do {
            consume(TokenType::LEFT_PAREN, "Expected (");

            emit(sblr::Opcode::BEGIN_LIST);
            size_t val_count_pos = bytecode_.size();
            emitU32(0);

            uint32_t val_count = 0;
            do {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    emit(sblr::Opcode::DEFAULT_VALUE);
                } else {
                    parseExpression();
                }
                val_count++;
            } while (match(TokenType::COMMA));

            sblr::writeInt32(&bytecode_[val_count_pos], val_count);
            emit(sblr::Opcode::END_LIST);

            consume(TokenType::RIGHT_PAREN, "Expected )");
            row_count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[row_count_pos], row_count);
        emit(sblr::Opcode::END_LIST);
    } else if (check(TokenType::KW_SELECT)) {
        // INSERT ... SELECT
        parseSelectStmt();
    } else {
        error("Expected VALUES or SELECT");
    }

    // ON DUPLICATE KEY UPDATE
    if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_DUPLICATE, "Expected DUPLICATE");
        consumeKeyword(TokenType::KW_KEY, "Expected KEY");
        consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE");

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));

        emit(sblr::Opcode::BEGIN_LIST);
        size_t update_count_pos = bytecode_.size();
        emitU32(0);

        uint32_t update_count = 0;
        do {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");

            emit(sblr::Opcode::ASSIGNMENT);
            emit(sblr::Opcode::COLUMN_REF);
            emitString("");
            emitString("");
            emitString(col);

            parseExpression();
            update_count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[update_count_pos], update_count);
        emit(sblr::Opcode::END_LIST);
    }
}

// ============================================================================
// UPDATE Statement
// ============================================================================

void Parser::parseUpdateStmt() {
    consume(TokenType::KW_UPDATE, "Expected UPDATE");

    // Optional modifiers
    matchKeyword(TokenType::KW_LOW_PRIORITY);
    matchKeyword(TokenType::KW_IGNORE);

    emit(sblr::Opcode::UPDATE);

    // Table reference(s)
    emit(sblr::Opcode::BEGIN_LIST);
    size_t table_count_pos = bytecode_.size();
    emitU32(0);

    uint32_t table_count = 0;
    do {
        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        resolveTableName(schema, table);

        emit(sblr::Opcode::TABLE_REF);
        emitString(schema + "/" + table);

        // Optional alias
        std::string alias;
        if (matchKeyword(TokenType::KW_AS)) {
            alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_SET)) {
            alias = parseIdentifier();
        }
        emitString(alias);

        table_count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[table_count_pos], table_count);
    emit(sblr::Opcode::END_LIST);

    // SET clause
    consumeKeyword(TokenType::KW_SET, "Expected SET");

    emit(sblr::Opcode::BEGIN_LIST);
    size_t assign_count_pos = bytecode_.size();
    emitU32(0);

    uint32_t assign_count = 0;
    do {
        std::string col = parseIdentifier();
        consume(TokenType::EQUAL, "Expected =");

        emit(sblr::Opcode::ASSIGNMENT);
        emit(sblr::Opcode::COLUMN_REF);
        emitString("");
        emitString("");
        emitString(col);

        parseExpression();
        assign_count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[assign_count_pos], assign_count);
    emit(sblr::Opcode::END_LIST);

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);
    }

    // ORDER BY
    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY");
        parseOrderByClause();
    }

    // LIMIT
    if (matchKeyword(TokenType::KW_LIMIT)) {
        parseLimitClause();
    }
}

// ============================================================================
// DELETE Statement
// ============================================================================

void Parser::parseDeleteStmt() {
    consume(TokenType::KW_DELETE, "Expected DELETE");

    // Optional modifiers
    matchKeyword(TokenType::KW_LOW_PRIORITY);
    matchKeyword(TokenType::KW_QUICK);
    matchKeyword(TokenType::KW_IGNORE);

    emit(sblr::Opcode::DELETE);

    consumeKeyword(TokenType::KW_FROM, "Expected FROM");

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitString(schema + "/" + table);

    // Optional alias
    std::string alias;
    if (matchKeyword(TokenType::KW_AS)) {
        alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_WHERE)) {
        alias = parseIdentifier();
    }
    emitString(alias);

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    } else {
        emit(sblr::Opcode::LITERAL_NULL);
    }

    // ORDER BY
    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY");
        parseOrderByClause();
    }

    // LIMIT
    if (matchKeyword(TokenType::KW_LIMIT)) {
        parseLimitClause();
    }
}

// ============================================================================
// REPLACE Statement (MySQL-specific)
// ============================================================================

void Parser::parseReplaceStmt() {
    consume(TokenType::KW_REPLACE, "Expected REPLACE");

    // REPLACE is similar to INSERT
    matchKeyword(TokenType::KW_LOW_PRIORITY);
    matchKeyword(TokenType::KW_DELAYED);

    consumeKeyword(TokenType::KW_INTO, "Expected INTO");

    // For now, treat as INSERT with conflict handling
    emit(sblr::Opcode::INSERT);
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitString(schema + "/" + table);

    // Column list
    emit(sblr::Opcode::BEGIN_LIST);
    size_t col_count_pos = bytecode_.size();
    emitU32(0);

    uint32_t col_count = 0;
    if (match(TokenType::LEFT_PAREN)) {
        do {
            std::string col = parseIdentifier();
            emit(sblr::Opcode::COLUMN_REF);
            emitString("");
            emitString("");
            emitString(col);
            col_count++;
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    sblr::writeInt32(&bytecode_[col_count_pos], col_count);
    emit(sblr::Opcode::END_LIST);

    // VALUES
    if (matchKeyword(TokenType::KW_VALUES) || matchKeyword(TokenType::KW_VALUE)) {
        emit(sblr::Opcode::BEGIN_LIST);
        size_t row_count_pos = bytecode_.size();
        emitU32(0);

        uint32_t row_count = 0;
        do {
            consume(TokenType::LEFT_PAREN, "Expected (");

            emit(sblr::Opcode::BEGIN_LIST);
            size_t val_count_pos = bytecode_.size();
            emitU32(0);

            uint32_t val_count = 0;
            do {
                parseExpression();
                val_count++;
            } while (match(TokenType::COMMA));

            sblr::writeInt32(&bytecode_[val_count_pos], val_count);
            emit(sblr::Opcode::END_LIST);

            consume(TokenType::RIGHT_PAREN, "Expected )");
            row_count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[row_count_pos], row_count);
        emit(sblr::Opcode::END_LIST);
    } else if (check(TokenType::KW_SELECT)) {
        parseSelectStmt();
    }
}

// ============================================================================
// DDL Statements - Stubs (to be implemented in mysql_parser_ddl.cpp)
// ============================================================================

void Parser::parseCreateStmt() {
    advance();  // Consume CREATE
    if (matchKeyword(TokenType::KW_DATABASE) || matchKeyword(TokenType::KW_SCHEMA)) {
        parseCreateDatabase();
        return;
    }
    if (check(TokenType::KW_TABLE) || check(TokenType::KW_TEMPORARY)) {
        parseCreateTable();
        return;
    }
    error("CREATE statement not yet implemented");
    synchronize();
}

void Parser::parseRenameStmt() {
    advance();  // Consume RENAME

    consumeKeyword(TokenType::KW_TABLE, "Expected TABLE after RENAME");

    auto emit_string16 = [&](std::string_view str) {
        if (str.size() > std::numeric_limits<uint16_t>::max()) {
            error("Identifier length exceeds 16-bit limit");
            emitU16(0);
            return;
        }
        emitU16(static_cast<uint16_t>(str.size()));
        for (unsigned char ch : str) {
            emitByte(static_cast<uint8_t>(ch));
        }
    };

    auto split_path = [&](const std::string& path) {
        std::vector<std::string> parts;
        std::string current;
        for (char ch : path) {
            if (ch == '/' || ch == '.') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }
        return parts;
    };

    auto build_object_path = [&](const std::string& schema_in,
                                 const std::string& object_name) {
        std::string schema = schema_in;
        std::string object = object_name;
        resolveTableName(schema, object);
        auto components = split_path(schema);
        components.push_back(object);
        return components;
    };

    auto build_schema_path = [&](const std::string& schema_in) {
        std::string schema = schema_in;
        std::string dummy = "x";
        resolveTableName(schema, dummy);
        return split_path(schema);
    };

    auto emit_object_path = [&](const std::vector<std::string>& components) {
        emitByte(static_cast<uint8_t>(core::PathType::ABSOLUTE));
        emitByte(0);  // no_search_path flag (reserved)
        if (components.size() > std::numeric_limits<uint8_t>::max()) {
            error("Object path has too many components");
            emitByte(0);
            return;
        }
        emitByte(static_cast<uint8_t>(components.size()));
        for (const auto& comp : components) {
            emit_string16(comp);
        }
    };

    auto emit_rename = [&](const std::vector<std::string>& components,
                           std::string_view new_name) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RENAME_OBJECT));
        emitByte(0);
        emitByte(static_cast<uint8_t>(core::CatalogManager::ObjectType::TABLE));
        emit_object_path(components);
        emit_string16(new_name);
    };

    auto emit_move = [&](const std::vector<std::string>& components,
                         const std::vector<std::string>& target_schema,
                         std::string_view new_name) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MOVE_OBJECT));
        emitByte(0);
        emitByte(static_cast<uint8_t>(core::CatalogManager::ObjectType::TABLE));
        emit_object_path(components);
        emit_object_path(target_schema);
        emit_string16(new_name);
    };

    std::string old_schema;
    std::string old_table = parseIdentifier();
    if (match(TokenType::DOT)) {
        old_schema = old_table;
        old_table = parseIdentifier();
    }
    auto old_components = build_object_path(old_schema, old_table);

    if (!(matchKeyword(TokenType::KW_TO) || match(TokenType::KW_AS))) {
        error("Expected TO in RENAME TABLE");
        return;
    }

    std::string new_schema;
    std::string new_table = parseIdentifier();
    if (match(TokenType::DOT)) {
        new_schema = new_table;
        new_table = parseIdentifier();
    }

    if (!new_schema.empty()) {
        auto target_schema = build_schema_path(new_schema);
        emit_move(old_components, target_schema, new_table);
    } else {
        emit_rename(old_components, new_table);
    }
}

void Parser::parseAlterStmt() {
    advance();  // Consume ALTER
    if (matchKeyword(TokenType::KW_DATABASE) || matchKeyword(TokenType::KW_SCHEMA)) {
        std::string db_name = parseIdentifier();

        auto emit_alter_database = [&](sblr::AlterDatabaseAction action,
                                       std::string_view payload) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DATABASE));
            emitByte(static_cast<uint8_t>(action));

            std::string db_path = buildEmulatedServerRoot(default_schema_);
            if (!db_path.empty()) {
                db_path.push_back('.');
            }
            db_path += db_name;
            emitString(db_path);
            emitString(payload);
        };

        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO after RENAME");
            std::string new_name = parseIdentifier();
            emit_alter_database(sblr::AlterDatabaseAction::RENAME, new_name);
            return;
        }

        // TODO: Add OWNER keyword support
        // if (matchKeyword(TokenType::KW_OWNER)) {
        //     consumeKeyword(TokenType::KW_TO, "Expected TO after OWNER");
        //     std::string owner = parseIdentifier();
        //     emit_alter_database(sblr::AlterDatabaseAction::SET_OWNER, owner);
        //     return;
        // }

        error("ALTER DATABASE supports only RENAME TO in MySQL parser");
        synchronize();
        return;
    }

    consumeKeyword(TokenType::KW_TABLE, "Expected TABLE after ALTER");

    auto emit_string16 = [&](std::string_view str) {
        if (str.size() > std::numeric_limits<uint16_t>::max()) {
            error("Identifier length exceeds 16-bit limit");
            emitU16(0);
            return;
        }
        emitU16(static_cast<uint16_t>(str.size()));
        for (unsigned char ch : str) {
            emitByte(static_cast<uint8_t>(ch));
        }
    };

    auto split_path = [&](const std::string& path) {
        std::vector<std::string> parts;
        std::string current;
        for (char ch : path) {
            if (ch == '/' || ch == '.') {
                if (!current.empty()) {
                    parts.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(ch);
            }
        }
        if (!current.empty()) {
            parts.push_back(current);
        }
        return parts;
    };

    auto build_object_path = [&](const std::string& schema_in,
                                 const std::string& object_name) {
        std::string schema = schema_in;
        std::string object = object_name;
        resolveTableName(schema, object);
        auto components = split_path(schema);
        components.push_back(object);
        return components;
    };

    auto build_schema_path = [&](const std::string& schema_in) {
        std::string schema = schema_in;
        std::string dummy = "x";
        resolveTableName(schema, dummy);
        return split_path(schema);
    };

    auto emit_object_path = [&](const std::vector<std::string>& components) {
        emitByte(static_cast<uint8_t>(core::PathType::ABSOLUTE));
        emitByte(0);  // no_search_path flag (reserved)
        if (components.size() > std::numeric_limits<uint8_t>::max()) {
            error("Object path has too many components");
            emitByte(0);
            return;
        }
        emitByte(static_cast<uint8_t>(components.size()));
        for (const auto& comp : components) {
            emit_string16(comp);
        }
    };

    auto emit_rename = [&](const std::vector<std::string>& components,
                           std::string_view new_name) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RENAME_OBJECT));
        emitByte(0);
        emitByte(static_cast<uint8_t>(core::CatalogManager::ObjectType::TABLE));
        emit_object_path(components);
        emit_string16(new_name);
    };

    auto emit_move = [&](const std::vector<std::string>& components,
                         const std::vector<std::string>& target_schema,
                         std::string_view new_name) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MOVE_OBJECT));
        emitByte(0);
        emitByte(static_cast<uint8_t>(core::CatalogManager::ObjectType::TABLE));
        emit_object_path(components);
        emit_object_path(target_schema);
        emit_string16(new_name);
    };

    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }

    auto components = build_object_path(schema, table);

    if (matchKeyword(TokenType::KW_RENAME)) {
        if (matchKeyword(TokenType::KW_TO)) {
            std::string new_schema;
            std::string new_table = parseIdentifier();
            if (match(TokenType::DOT)) {
                new_schema = new_table;
                new_table = parseIdentifier();
            }
            if (!new_schema.empty()) {
                auto target_schema = build_schema_path(new_schema);
                emit_move(components, target_schema, new_table);
            } else {
                emit_rename(components, new_table);
            }
            return;
        }
    }

    error("ALTER TABLE supports only RENAME TO [schema.table] in MySQL parser");
    synchronize();
}

void Parser::parseDropStmt() {
    advance();  // Consume DROP
    if (matchKeyword(TokenType::KW_DATABASE) || matchKeyword(TokenType::KW_SCHEMA)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::string db_name = parseIdentifier();

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_DATABASE));
        emitByte(if_exists ? 0x01 : 0x00);

        std::string db_path = buildEmulatedServerRoot(default_schema_);
        if (!db_path.empty()) {
            db_path.push_back('.');
        }
        db_path += db_name;
        emitString(db_path);
        return;
    }

    error("DROP statement not yet implemented");
    synchronize();
}

void Parser::parseTruncateStmt() {
    advance();  // Consume TRUNCATE
    error("TRUNCATE statement not yet implemented");
    synchronize();
}

void Parser::parseCreateTable() {
    matchKeyword(TokenType::KW_TEMPORARY);

    consumeKeyword(TokenType::KW_TABLE, "Expected TABLE");

    // IF NOT EXISTS
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        if (matchKeyword(TokenType::KW_EXISTS)) {
            if_not_exists = true;
        }
    }

    emit(sblr::Opcode::CREATE_TABLE);
    emitByte(if_not_exists ? 1 : 0);

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emitString(schema + "/" + table);

    // Column definitions
    consume(TokenType::LEFT_PAREN, "Expected (");

    emit(sblr::Opcode::BEGIN_LIST);
    size_t col_count_pos = bytecode_.size();
    emitU32(0);

    uint32_t col_count = 0;
    do {
        // Check for constraint definitions
        if (check(TokenType::KW_PRIMARY) || check(TokenType::KW_UNIQUE) ||
            check(TokenType::KW_FOREIGN) || check(TokenType::KW_KEY) ||
            check(TokenType::KW_INDEX) || check(TokenType::KW_FULLTEXT) ||
            check(TokenType::KW_SPATIAL) || check(TokenType::KW_CONSTRAINT) ||
            check(TokenType::KW_CHECK)) {
            // TODO: Parse table constraints
            // For now, skip to end
            while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::COMMA) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
            continue;
        }

        // Column definition
        ColumnDef col = parseColumnDef();

        emit(sblr::Opcode::COLUMN_DEF);
        emitString(col.name);

        // Emit type
        emit(typeToOpcode(col.type.kind));
        if (col.type.length > 0) {
            emitU32(col.type.length);
        }

        // Constraints
        if (!col.type.nullable) {
            emit(sblr::Opcode::NOT_NULL);
        }
        if (col.primary_key) {
            emit(sblr::Opcode::PRIMARY_KEY);
        }
        if (col.unique) {
            emit(sblr::Opcode::UNIQUE_CONSTRAINT);
        }
        if (col.auto_increment) {
            emit(sblr::Opcode::IDENTITY_COLUMN);
        }
        if (col.has_default) {
            emit(sblr::Opcode::DEFAULT_VALUE);
            if (col.default_is_null) {
                emit(sblr::Opcode::LITERAL_NULL);
            } else {
                emit(sblr::Opcode::LITERAL_STRING);
                emitString(col.default_value);
            }
        }

        col_count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[col_count_pos], col_count);
    emit(sblr::Opcode::END_LIST);

    consume(TokenType::RIGHT_PAREN, "Expected )");

    // Table options (ENGINE, CHARSET, etc.)
    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        advance();  // Skip table options for now
    }
}

ColumnDef Parser::parseColumnDef() {
    ColumnDef col;
    col.name = parseIdentifier();
    col.type = parseDataType();

    // Column constraints
    while (true) {
        if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_NULL, "Expected NULL after NOT");
            col.type.nullable = false;
        } else if (matchKeyword(TokenType::KW_NULL)) {
            col.type.nullable = true;
        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
            col.has_default = true;
            if (matchKeyword(TokenType::KW_NULL)) {
                col.default_is_null = true;
            } else if (check(TokenType::STRING_LITERAL) || check(TokenType::INTEGER_LITERAL) ||
                       check(TokenType::FLOAT_LITERAL)) {
                if (check(TokenType::STRING_LITERAL)) {
                    col.default_value = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                } else if (check(TokenType::INTEGER_LITERAL)) {
                    col.default_value = std::to_string(current_token_.value.int_value);
                } else {
                    col.default_value = std::to_string(current_token_.value.float_value);
                }
                advance();
            } else if (match(TokenType::LEFT_PAREN)) {
                // Expression default
                col.default_is_expr = true;
                // Skip expression for now
                int paren_depth = 1;
                while (paren_depth > 0 && !check(TokenType::END_OF_FILE)) {
                    if (match(TokenType::LEFT_PAREN)) paren_depth++;
                    else if (match(TokenType::RIGHT_PAREN)) paren_depth--;
                    else advance();
                }
            }
        } else if (matchKeyword(TokenType::KW_AUTO_INCREMENT)) {
            col.auto_increment = true;
        } else if (matchKeyword(TokenType::KW_PRIMARY)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY after PRIMARY");
            col.primary_key = true;
        } else if (matchKeyword(TokenType::KW_UNIQUE)) {
            matchKeyword(TokenType::KW_KEY);  // Optional KEY
            col.unique = true;
        } else if (matchKeyword(TokenType::KW_KEY)) {
            // Just KEY (creates index)
        } else if (matchKeyword(TokenType::KW_COMMENT)) {
            if (check(TokenType::STRING_LITERAL)) {
                col.comment = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            }
        } else if (matchKeyword(TokenType::KW_COLLATE)) {
            if (check(TokenType::IDENTIFIER)) {
                col.type.collation = parseIdentifier();
            }
        } else if (matchKeyword(TokenType::KW_CHARACTER)) {
            consumeKeyword(TokenType::KW_SET, "Expected SET after CHARACTER");
            if (check(TokenType::IDENTIFIER)) {
                col.type.charset = parseIdentifier();
            }
        } else if (matchKeyword(TokenType::KW_CHARSET)) {
            if (check(TokenType::IDENTIFIER)) {
                col.type.charset = parseIdentifier();
            }
        } else if (matchKeyword(TokenType::KW_REFERENCES)) {
            // Foreign key (inline)
            parseIdentifier();  // Referenced table
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    parseIdentifier();  // Referenced column
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
        } else {
            break;
        }
    }

    return col;
}

MySQLDataType Parser::parseDataType() {
    MySQLDataType type;

    // Integer types
    if (matchKeyword(TokenType::KW_TINYINT)) {
        type.kind = MySQLDataType::Kind::TINYINT;
    } else if (matchKeyword(TokenType::KW_SMALLINT)) {
        type.kind = MySQLDataType::Kind::SMALLINT;
    } else if (matchKeyword(TokenType::KW_MEDIUMINT)) {
        type.kind = MySQLDataType::Kind::MEDIUMINT;
    } else if (matchKeyword(TokenType::KW_INT) || matchKeyword(TokenType::KW_INTEGER)) {
        type.kind = MySQLDataType::Kind::INT;
    } else if (matchKeyword(TokenType::KW_BIGINT)) {
        type.kind = MySQLDataType::Kind::BIGINT;
    }
    // Floating point
    else if (matchKeyword(TokenType::KW_FLOAT)) {
        type.kind = MySQLDataType::Kind::FLOAT;
    } else if (matchKeyword(TokenType::KW_DOUBLE)) {
        matchKeyword(TokenType::KW_PRECISION);  // Optional PRECISION
        type.kind = MySQLDataType::Kind::DOUBLE;
    } else if (matchKeyword(TokenType::KW_REAL)) {
        type.kind = MySQLDataType::Kind::DOUBLE;
    } else if (matchKeyword(TokenType::KW_DECIMAL) || matchKeyword(TokenType::KW_NUMERIC)) {
        type.kind = MySQLDataType::Kind::DECIMAL;
    }
    // String types
    else if (matchKeyword(TokenType::KW_CHAR)) {
        type.kind = MySQLDataType::Kind::CHAR;
    } else if (matchKeyword(TokenType::KW_VARCHAR)) {
        type.kind = MySQLDataType::Kind::VARCHAR;
    } else if (matchKeyword(TokenType::KW_TINYTEXT)) {
        type.kind = MySQLDataType::Kind::TINYTEXT;
    } else if (matchKeyword(TokenType::KW_TEXT)) {
        type.kind = MySQLDataType::Kind::TEXT;
    } else if (matchKeyword(TokenType::KW_MEDIUMTEXT)) {
        type.kind = MySQLDataType::Kind::MEDIUMTEXT;
    } else if (matchKeyword(TokenType::KW_LONGTEXT)) {
        type.kind = MySQLDataType::Kind::LONGTEXT;
    }
    // Binary types
    else if (matchKeyword(TokenType::KW_BINARY)) {
        type.kind = MySQLDataType::Kind::BINARY;
    } else if (matchKeyword(TokenType::KW_VARBINARY)) {
        type.kind = MySQLDataType::Kind::VARBINARY;
    } else if (matchKeyword(TokenType::KW_TINYBLOB)) {
        type.kind = MySQLDataType::Kind::TINYBLOB;
    } else if (matchKeyword(TokenType::KW_BLOB)) {
        type.kind = MySQLDataType::Kind::BLOB;
    } else if (matchKeyword(TokenType::KW_MEDIUMBLOB)) {
        type.kind = MySQLDataType::Kind::MEDIUMBLOB;
    } else if (matchKeyword(TokenType::KW_LONGBLOB)) {
        type.kind = MySQLDataType::Kind::LONGBLOB;
    }
    // Date/Time types
    else if (matchKeyword(TokenType::KW_DATE)) {
        type.kind = MySQLDataType::Kind::DATE;
    } else if (matchKeyword(TokenType::KW_TIME)) {
        type.kind = MySQLDataType::Kind::TIME;
    } else if (matchKeyword(TokenType::KW_DATETIME)) {
        type.kind = MySQLDataType::Kind::DATETIME;
    } else if (matchKeyword(TokenType::KW_TIMESTAMP)) {
        type.kind = MySQLDataType::Kind::TIMESTAMP;
    } else if (matchKeyword(TokenType::KW_YEAR)) {
        type.kind = MySQLDataType::Kind::YEAR;
    }
    // Other types
    else if (matchKeyword(TokenType::KW_BIT)) {
        type.kind = MySQLDataType::Kind::BIT;
    } else if (matchKeyword(TokenType::KW_BOOL) || matchKeyword(TokenType::KW_BOOLEAN)) {
        type.kind = MySQLDataType::Kind::BOOL;
    } else if (matchKeyword(TokenType::KW_ENUM)) {
        type.kind = MySQLDataType::Kind::ENUM;
    } else if (matchKeyword(TokenType::KW_JSON)) {
        type.kind = MySQLDataType::Kind::JSON;
    } else if (matchKeyword(TokenType::KW_GEOMETRY)) {
        type.kind = MySQLDataType::Kind::GEOMETRY;
    } else {
        error("Expected data type");
        return type;
    }

    // Parse length/precision
    if (match(TokenType::LEFT_PAREN)) {
        if (check(TokenType::INTEGER_LITERAL)) {
            type.length = static_cast<int>(current_token_.value.int_value);
            type.precision = type.length;
            advance();

            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    type.scale = static_cast<int>(current_token_.value.int_value);
                    advance();
                }
            }
        } else if (type.kind == MySQLDataType::Kind::ENUM) {
            // Parse enum values
            do {
                if (check(TokenType::STRING_LITERAL)) {
                    type.enum_values.emplace_back(lexer_.stringPool().get(current_token_.value.string_id));
                    advance();
                }
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    // Type modifiers
    if (matchKeyword(TokenType::KW_UNSIGNED)) {
        type.unsigned_ = true;
    }
    if (matchKeyword(TokenType::KW_ZEROFILL)) {
        type.zerofill = true;
        type.unsigned_ = true;  // ZEROFILL implies UNSIGNED
    }

    return type;
}

sblr::Opcode Parser::typeToOpcode(MySQLDataType::Kind kind) {
    switch (kind) {
        case MySQLDataType::Kind::TINYINT:
            return sblr::Opcode::TYPE_INT8;
        case MySQLDataType::Kind::SMALLINT:
            return sblr::Opcode::TYPE_INT16;
        case MySQLDataType::Kind::MEDIUMINT:
        case MySQLDataType::Kind::INT:
            return sblr::Opcode::TYPE_INTEGER;
        case MySQLDataType::Kind::BIGINT:
            return sblr::Opcode::TYPE_BIGINT;
        case MySQLDataType::Kind::FLOAT:
            return sblr::Opcode::TYPE_FLOAT32;
        case MySQLDataType::Kind::DOUBLE:
            return sblr::Opcode::TYPE_DOUBLE;
        case MySQLDataType::Kind::DECIMAL:
            return sblr::Opcode::TYPE_DECIMAL;
        case MySQLDataType::Kind::CHAR:
            return sblr::Opcode::TYPE_CHAR;
        case MySQLDataType::Kind::VARCHAR:
        case MySQLDataType::Kind::TINYTEXT:
        case MySQLDataType::Kind::MEDIUMTEXT:
        case MySQLDataType::Kind::LONGTEXT:
            return sblr::Opcode::TYPE_VARCHAR;
        case MySQLDataType::Kind::TEXT:
            return sblr::Opcode::TYPE_TEXT;
        case MySQLDataType::Kind::BINARY:
            return sblr::Opcode::TYPE_BINARY;
        case MySQLDataType::Kind::VARBINARY:
        case MySQLDataType::Kind::TINYBLOB:
        case MySQLDataType::Kind::MEDIUMBLOB:
        case MySQLDataType::Kind::LONGBLOB:
            return sblr::Opcode::TYPE_VARBINARY;
        case MySQLDataType::Kind::BLOB:
            return sblr::Opcode::TYPE_BLOB;
        case MySQLDataType::Kind::DATE:
            return sblr::Opcode::TYPE_DATE;
        case MySQLDataType::Kind::TIME:
            return sblr::Opcode::TYPE_TIME;
        case MySQLDataType::Kind::DATETIME:
        case MySQLDataType::Kind::TIMESTAMP:
            return sblr::Opcode::TYPE_TIMESTAMP;
        case MySQLDataType::Kind::YEAR:
            return sblr::Opcode::TYPE_INT16;
        case MySQLDataType::Kind::BIT:
        case MySQLDataType::Kind::BOOL:
            return sblr::Opcode::TYPE_BOOLEAN;
        case MySQLDataType::Kind::ENUM:
        case MySQLDataType::Kind::SET:
            return sblr::Opcode::TYPE_VARCHAR;
        case MySQLDataType::Kind::JSON:
            return sblr::Opcode::TYPE_JSON;
        case MySQLDataType::Kind::GEOMETRY:
            return sblr::Opcode::EXTENDED_OPCODE;  // TODO: Proper geometry type
        default:
            return sblr::Opcode::TYPE_VARCHAR;
    }
}

void Parser::parseCreateIndex() {
    // TODO: Implement
}

void Parser::parseCreateView() {
    // TODO: Implement
}

void Parser::parseCreateDatabase() {
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }

    std::string db_name = parseIdentifier();

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DATABASE));
    emitByte(if_not_exists ? 0x01 : 0x00);

    std::string db_path = buildEmulatedServerRoot(default_schema_);
    if (!db_path.empty()) {
        db_path.push_back('.');
    }
    db_path += db_name;
    emitString(db_path);

    // Optional database options
    while (true) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            continue;
        } else if (matchKeyword(TokenType::KW_CHARACTER)) {
            consumeKeyword(TokenType::KW_SET, "Expected SET");
            parseIdentifier();
        } else if (matchKeyword(TokenType::KW_CHARSET)) {
            parseIdentifier();
        } else if (matchKeyword(TokenType::KW_COLLATE)) {
            parseIdentifier();
        // TODO: Add ENCRYPTION keyword support
        // } else if (matchKeyword(TokenType::KW_ENCRYPTION)) {
        //     match(TokenType::EQUAL);
        //     if (check(TokenType::STRING_LITERAL)) {
        //         advance();
        //     } else {
        //         parseIdentifier();
        //     }
        } else {
            break;
        }
    }
}

void Parser::parseCreateProcedure() {
    // TODO: Implement
}

void Parser::parseCreateFunction() {
    // TODO: Implement
}

void Parser::parseCreateTrigger() {
    // TODO: Implement
}

IndexDef Parser::parseIndexDef() {
    // TODO: Implement
    return IndexDef();
}

ForeignKeyDef Parser::parseForeignKeyDef() {
    // TODO: Implement
    return ForeignKeyDef();
}

// ============================================================================
// Admin Statements
// ============================================================================

void Parser::parseSetStmt() {
    consume(TokenType::KW_SET, "Expected SET");

    // Handle various SET forms
    if (matchKeyword(TokenType::KW_GLOBAL) || matchKeyword(TokenType::KW_SESSION) ||
        matchKeyword(TokenType::KW_LOCAL)) {
        // Scope is parsed for MySQL compatibility; ScratchBird treats scope as session-level.
    }

    if (matchKeyword(TokenType::KW_TRANSACTION)) {
        emit(sblr::Opcode::SET_TRANSACTION);

        constexpr uint8_t kIsoReadCommitted = 0;
        constexpr uint8_t kIsoSnapshot = 2;
        constexpr uint8_t kIsoSnapshotTableStability = 3;

        bool has_isolation = false;
        bool has_access_mode = false;
        uint8_t isolation = kIsoReadCommitted;
        uint8_t access_mode = 0;  // 0=READ WRITE, 1=READ ONLY

        while (true) {
            if (matchKeyword(TokenType::KW_ISOLATION)) {
                consumeKeyword(TokenType::KW_LEVEL, "Expected LEVEL");
                if (matchKeyword(TokenType::KW_SERIALIZABLE)) {
                    has_isolation = true;
                    isolation = kIsoSnapshotTableStability;
                } else if (matchKeyword(TokenType::KW_REPEATABLE)) {
                    consumeKeyword(TokenType::KW_READ, "Expected READ");
                    has_isolation = true;
                    isolation = kIsoSnapshot;
                } else if (matchKeyword(TokenType::KW_READ)) {
                    if (matchKeyword(TokenType::KW_COMMITTED)) {
                        has_isolation = true;
                        isolation = kIsoReadCommitted;
                    } else if (matchKeyword(TokenType::KW_UNCOMMITTED)) {
                        has_isolation = true;
                        isolation = kIsoReadCommitted;
                    }
                }
            } else if (matchKeyword(TokenType::KW_READ)) {
                if (matchKeyword(TokenType::KW_ONLY)) {
                    has_access_mode = true;
                    access_mode = 1;
                } else if (matchKeyword(TokenType::KW_WRITE)) {
                    has_access_mode = true;
                    access_mode = 0;
                }
            } else {
                break;
            }

            match(TokenType::COMMA);
        }

        uint16_t flags = 0;
        if (has_isolation) flags |= sblr::TransactionFlags::HAS_ISOLATION;
        if (has_access_mode) flags |= sblr::TransactionFlags::HAS_ACCESS_MODE;
        emitU16(flags);
        emitByte(static_cast<uint8_t>(sblr::TransactionConflictAction::DEFAULT));
        if (has_isolation) emitByte(isolation);
        if (has_access_mode) emitByte(access_mode);
        return;
    }

    // Variable name
    std::string var;
    if (check(TokenType::SYSTEM_VARIABLE)) {
        var = std::string(lexer_.stringPool().get(current_token_.value.string_id));
        advance();
    } else if (check(TokenType::USER_VARIABLE)) {
        var = "@" + std::string(lexer_.stringPool().get(current_token_.value.string_id));
        advance();
    } else {
        var = parseIdentifier();
    }

    // = or :=
    if (!match(TokenType::EQUAL) && !match(TokenType::COLON_EQUAL)) {
        error("Expected = or :=");
    }

    std::string var_upper = var;
    for (char& c : var_upper) {
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 32);
        }
    }

    if (var_upper == "AUTOCOMMIT" && check(TokenType::INTEGER_LITERAL)) {
        int64_t value = current_token_.value.int_value;
        if (value == 0 || value == 1) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_AUTOCOMMIT));
            emitByte(static_cast<uint8_t>(value));
            emitByte(static_cast<uint8_t>(sblr::TransactionConflictAction::DEFAULT));
            advance();
            return;
        }
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_VARIABLE));
    emitString(var);

    // Value
    parseExpression();
}

void Parser::parseShowStmt() {
    consume(TokenType::KW_SHOW, "Expected SHOW");

    if (matchKeyword(TokenType::KW_TABLES)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_TABLES));

        // FROM database
        if (matchKeyword(TokenType::KW_FROM) || matchKeyword(TokenType::KW_IN)) {
            std::string db = parseIdentifier();
            emitString(db);
        } else {
            emitString("");
        }

        // LIKE pattern
        if (matchKeyword(TokenType::KW_LIKE)) {
            if (check(TokenType::STRING_LITERAL)) {
                emitString(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            }
        } else {
            emitString("");
        }
    } else if (matchKeyword(TokenType::KW_DATABASES) || matchKeyword(TokenType::KW_SCHEMAS)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_DATABASES));

        if (matchKeyword(TokenType::KW_LIKE)) {
            if (check(TokenType::STRING_LITERAL)) {
                emitString(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            }
        } else {
            emitString("");
        }
    } else if (matchKeyword(TokenType::KW_COLUMNS)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_COLUMNS));

        consumeKeyword(TokenType::KW_FROM, "Expected FROM");
        std::string table = parseQualifiedName();
        emitString(table);

        if (matchKeyword(TokenType::KW_LIKE)) {
            if (check(TokenType::STRING_LITERAL)) {
                emitString(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            }
        } else {
            emitString("");
        }
    } else if (matchKeyword(TokenType::KW_INDEX) || matchKeyword(TokenType::KW_INDEXES) ||
               matchKeyword(TokenType::KW_KEY)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_INDEXES));

        consumeKeyword(TokenType::KW_FROM, "Expected FROM");
        std::string table = parseQualifiedName();
        emitString(table);
    } else if (matchKeyword(TokenType::KW_CREATE)) {
        if (matchKeyword(TokenType::KW_TABLE)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_CREATE_TABLE));

            std::string table = parseQualifiedName();
            emitString(table);
        } else if (matchKeyword(TokenType::KW_DATABASE)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_DATABASE));

            std::string db = parseIdentifier();
            emitString(db);
        }
    } else {
        error("Unknown SHOW command");
        synchronize();
    }
}

void Parser::parseDescribeStmt() {
    consume(TokenType::KW_DESCRIBE, "Expected DESCRIBE");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DESCRIBE_TABLE));

    std::string table = parseQualifiedName();
    emitString(table);

    // Optional column pattern
    if (check(TokenType::STRING_LITERAL)) {
        emitString(lexer_.stringPool().get(current_token_.value.string_id));
        advance();
    } else {
        emitString("");
    }
}

void Parser::parseUseStmt() {
    consume(TokenType::KW_USE, "Expected USE");

    // USE changes the default schema
    std::string db = parseIdentifier();

    // Update default schema to include the database
    std::string server_root = buildEmulatedServerRoot(default_schema_);
    if (server_root.empty()) {
        server_root = "remote.emulated.mysql.localhost";
    }
    default_schema_ = server_root + "." + db;

    // Emit a schema change opcode
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_VARIABLE));
    emitString("search_path");
    emit(sblr::Opcode::LITERAL_STRING);
    emitString(default_schema_);
}

// ============================================================================
// Transaction Statements
// ============================================================================

void Parser::parseBeginStmt() {
    if (matchKeyword(TokenType::KW_BEGIN)) {
        matchKeyword(TokenType::KW_WORK);
    } else if (matchKeyword(TokenType::KW_START)) {
        consumeKeyword(TokenType::KW_TRANSACTION, "Expected TRANSACTION");
    }

    emit(sblr::Opcode::START_TRANSACTION);
    bool has_access_mode = false;
    uint8_t access_mode = 0;  // 0=READ WRITE, 1=READ ONLY

    // Transaction characteristics
    while (true) {
        if (matchKeyword(TokenType::KW_READ)) {
            if (matchKeyword(TokenType::KW_ONLY)) {
                has_access_mode = true;
                access_mode = 1;
            } else if (matchKeyword(TokenType::KW_WRITE)) {
                has_access_mode = true;
                access_mode = 0;
            }
        } else if (matchKeyword(TokenType::KW_WITH)) {
            // WITH CONSISTENT SNAPSHOT - MySQL specific
            // Skip the CONSISTENT SNAPSHOT keywords
            if (check(TokenType::IDENTIFIER)) {
                advance();  // CONSISTENT
                if (check(TokenType::IDENTIFIER)) {
                    advance();  // SNAPSHOT
                }
            }
        } else {
            break;
        }

        match(TokenType::COMMA);
    }

    uint16_t flags = 0;
    if (has_access_mode) flags |= sblr::TransactionFlags::HAS_ACCESS_MODE;
    emitU16(flags);
    emitByte(static_cast<uint8_t>(sblr::TransactionConflictAction::DEFAULT));
    if (has_access_mode) {
        emitByte(access_mode);
    }
}

void Parser::parseCommitStmt() {
    consume(TokenType::KW_COMMIT, "Expected COMMIT");
    matchKeyword(TokenType::KW_WORK);

    emit(sblr::Opcode::COMMIT);
    uint8_t flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;

    // AND [NO] CHAIN
    if (matchKeyword(TokenType::KW_AND)) {
        bool no = matchKeyword(TokenType::KW_NO);
        if (matchKeyword(TokenType::KW_CHAIN)) {
            flags = no ? sblr::CommitRollbackFlags::AND_NO_CHAIN
                       : sblr::CommitRollbackFlags::AND_CHAIN;
        }
    }
    emitByte(flags);
}

void Parser::parseRollbackStmt() {
    consume(TokenType::KW_ROLLBACK, "Expected ROLLBACK");
    matchKeyword(TokenType::KW_WORK);

    // ROLLBACK TO SAVEPOINT
    if (matchKeyword(TokenType::KW_TO)) {
        matchKeyword(TokenType::KW_SAVEPOINT);
        std::string name = parseIdentifier();

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ROLLBACK_TO_SAVEPOINT));
        emitString(name);
    } else {
        emit(sblr::Opcode::ROLLBACK);
        uint8_t flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;

        // AND [NO] CHAIN
        if (matchKeyword(TokenType::KW_AND)) {
            bool no = matchKeyword(TokenType::KW_NO);
            if (matchKeyword(TokenType::KW_CHAIN)) {
                flags = no ? sblr::CommitRollbackFlags::AND_NO_CHAIN
                           : sblr::CommitRollbackFlags::AND_CHAIN;
            }
        }
        emitByte(flags);
    }
}

void Parser::parseSavepointStmt() {
    consume(TokenType::KW_SAVEPOINT, "Expected SAVEPOINT");

    std::string name = parseIdentifier();

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SAVEPOINT));
    emitString(name);
}

void Parser::parseReleaseStmt() {
    consume(TokenType::KW_RELEASE, "Expected RELEASE");
    consumeKeyword(TokenType::KW_SAVEPOINT, "Expected SAVEPOINT");

    std::string name = parseIdentifier();

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RELEASE_SAVEPOINT));
    emitString(name);
}

void Parser::parseLockStmt() {
    consume(TokenType::KW_LOCK, "Expected LOCK");
    consumeKeyword(TokenType::KW_TABLES, "Expected TABLES");

    // For now, just skip the lock statement
    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        advance();
    }

    // Emit no-op
    emit(sblr::Opcode::LITERAL_NULL);
}

void Parser::parseUnlockStmt() {
    consume(TokenType::KW_UNLOCK, "Expected UNLOCK");
    consumeKeyword(TokenType::KW_TABLES, "Expected TABLES");

    // Emit no-op
    emit(sblr::Opcode::LITERAL_NULL);
}

void Parser::parseSubquery() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_SCALAR));
    parseSelectStmt();
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_END));
}

} // namespace scratchbird::parser::mysql
