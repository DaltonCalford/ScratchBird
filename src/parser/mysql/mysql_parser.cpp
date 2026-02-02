/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * MySQL Parser Implementation
 *
 * Recursive-descent parser for MySQL 8.0 SQL that generates SBLR bytecode
 * directly for execution by the ScratchBird engine.
 */

#include "scratchbird/parser/mysql/mysql_parser.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/types.h"
#include "scratchbird/sblr/extract_element_catalog.h"
#include <cctype>
#include <cstring>
#include <algorithm>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <unordered_set>

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
        case TokenType::KW_ENCRYPTION:
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
        case TokenType::KW_ENCRYPTION: return "encryption";
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

static core::DataType mysqlTypeToCoreDataType(const MySQLDataType& type) {
    switch (type.kind) {
        case MySQLDataType::Kind::TINYINT:
            return type.unsigned_ ? core::DataType::UINT8 : core::DataType::INT8;
        case MySQLDataType::Kind::SMALLINT:
            return type.unsigned_ ? core::DataType::UINT16 : core::DataType::INT16;
        case MySQLDataType::Kind::MEDIUMINT:
        case MySQLDataType::Kind::INT:
            return type.unsigned_ ? core::DataType::UINT32 : core::DataType::INT32;
        case MySQLDataType::Kind::BIGINT:
            return type.unsigned_ ? core::DataType::UINT64 : core::DataType::INT64;
        case MySQLDataType::Kind::INT128:
            return type.unsigned_ ? core::DataType::UINT128 : core::DataType::INT128;
        case MySQLDataType::Kind::UINT128:
            return core::DataType::UINT128;
        case MySQLDataType::Kind::FLOAT:
            return core::DataType::FLOAT32;
        case MySQLDataType::Kind::DOUBLE:
            return core::DataType::FLOAT64;
        case MySQLDataType::Kind::DECIMAL:
            return core::DataType::DECIMAL;
        case MySQLDataType::Kind::CHAR:
            return core::DataType::CHAR;
        case MySQLDataType::Kind::VARCHAR:
        case MySQLDataType::Kind::TINYTEXT:
        case MySQLDataType::Kind::MEDIUMTEXT:
        case MySQLDataType::Kind::LONGTEXT:
            return core::DataType::VARCHAR;
        case MySQLDataType::Kind::TEXT:
            return core::DataType::TEXT;
        case MySQLDataType::Kind::BINARY:
            return core::DataType::BINARY;
        case MySQLDataType::Kind::VARBINARY:
        case MySQLDataType::Kind::TINYBLOB:
        case MySQLDataType::Kind::MEDIUMBLOB:
        case MySQLDataType::Kind::LONGBLOB:
            return core::DataType::VARBINARY;
        case MySQLDataType::Kind::BLOB:
            return core::DataType::BLOB;
        case MySQLDataType::Kind::DATE:
            return core::DataType::DATE;
        case MySQLDataType::Kind::TIME:
            return core::DataType::TIME;
        case MySQLDataType::Kind::DATETIME:
        case MySQLDataType::Kind::TIMESTAMP:
            return core::DataType::TIMESTAMP;
        case MySQLDataType::Kind::YEAR:
            return core::DataType::INT16;
        case MySQLDataType::Kind::BIT:
        case MySQLDataType::Kind::BOOL:
            return core::DataType::BOOLEAN;
        case MySQLDataType::Kind::ENUM:
        case MySQLDataType::Kind::SET:
            return core::DataType::VARCHAR;
        case MySQLDataType::Kind::JSON:
            return core::DataType::JSON;
        case MySQLDataType::Kind::GEOMETRY:
        case MySQLDataType::Kind::POINT:
        case MySQLDataType::Kind::LINESTRING:
        case MySQLDataType::Kind::POLYGON:
            return core::DataType::BLOB;
        default:
            return core::DataType::VARCHAR;
    }
}

static void resolveMySQLTypeModifiers(const MySQLDataType& type,
                                      uint32_t& precision,
                                      uint32_t& scale) {
    precision = 0;
    scale = 0;
    switch (type.kind) {
        case MySQLDataType::Kind::CHAR:
        case MySQLDataType::Kind::VARCHAR:
        case MySQLDataType::Kind::TINYTEXT:
        case MySQLDataType::Kind::MEDIUMTEXT:
        case MySQLDataType::Kind::LONGTEXT:
            precision = type.length > 0 ? static_cast<uint32_t>(type.length) : 255;
            break;
        case MySQLDataType::Kind::DECIMAL:
            precision = type.precision > 0 ? static_cast<uint32_t>(type.precision) : 18;
            scale = static_cast<uint32_t>(type.scale);
            break;
        default:
            break;
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

    if (parts.size() > 3) {
        parts.resize(3);
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

bool Parser::matchIdentifierKeyword(const char* keyword) {
    if (!check(TokenType::IDENTIFIER)) {
        return false;
    }
    std::string_view text = lexer_.stringPool().get(current_token_.value.string_id);
    size_t len = std::strlen(keyword);
    if (text.size() != len) {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
        char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
        if (a != b) {
            return false;
        }
    }
    advance();
    return true;
}

// ============================================================================
// Error Handling
// ============================================================================

void Parser::error(const std::string& message) {
    errors_.emplace_back(message, current_token_.span.start);
}

void Parser::warning(const std::string& message) {
    warnings_.push_back(message);
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
    if (!emit_enabled_) {
        return;
    }
    bytecode_.push_back(static_cast<uint8_t>(op));
}

void Parser::emitByte(uint8_t byte) {
    if (!emit_enabled_) {
        return;
    }
    bytecode_.push_back(byte);
}

void Parser::emitU16(uint16_t val) {
    if (!emit_enabled_) {
        return;
    }
    bytecode_.push_back(val & 0xFF);
    bytecode_.push_back((val >> 8) & 0xFF);
}

void Parser::emitUVarint(uint64_t val) {
    if (!emit_enabled_) {
        return;
    }
    uint8_t buffer[10];
    size_t count = sblr::writeUVarint(buffer, val);
    bytecode_.insert(bytecode_.end(), buffer, buffer + count);
}

void Parser::emitU32(uint32_t val) {
    if (!emit_enabled_) {
        return;
    }
    bytecode_.push_back(val & 0xFF);
    bytecode_.push_back((val >> 8) & 0xFF);
    bytecode_.push_back((val >> 16) & 0xFF);
    bytecode_.push_back((val >> 24) & 0xFF);
}

void Parser::emitU64(uint64_t val) {
    if (!emit_enabled_) {
        return;
    }
    for (int i = 0; i < 8; i++) {
        bytecode_.push_back((val >> (i * 8)) & 0xFF);
    }
}

void Parser::emitI64(int64_t val) {
    emitU64(static_cast<uint64_t>(val));
}

void Parser::emitF64(double val) {
    if (!emit_enabled_) {
        return;
    }
    uint64_t bits;
    std::memcpy(&bits, &val, sizeof(bits));
    emitU64(bits);
}

void Parser::emitString(std::string_view str) {
    if (!emit_enabled_) {
        return;
    }
    emitUVarint(static_cast<uint64_t>(str.size()));
    for (char c : str) {
        bytecode_.push_back(static_cast<uint8_t>(c));
    }
}

void Parser::emitDebugSpan(const SourceSpan& span) {
    if (!emit_enabled_) {
        return;
    }
    if (span.length == 0 || span.start.line == 0 || span.start.column == 0) {
        return;
    }
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DEBUG_SPAN));
    emitU32(span.start.line);
    emitU32(span.start.column);
}

// ============================================================================
// Main Parsing Entry Points
// ============================================================================

ParseResult Parser::parseStatement() {
    bytecode_.clear();
    errors_.clear();
    warnings_.clear();
    next_placeholder_index_ = 1;

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
    for (const auto& warn : warnings_) {
        result.addWarning(warn);
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
    emitDebugSpan(current_token_.span);
    if (check(TokenType::IDENTIFIER)) {
        if (matchIdentifierKeyword("PREPARE")) {
            parsePrepareStmt();
            return;
        }
        if (matchIdentifierKeyword("EXECUTE") || matchIdentifierKeyword("EXEC")) {
            parseExecuteStmt();
            return;
        }
        if (matchIdentifierKeyword("DEALLOCATE")) {
            parseDeallocateStmt();
            return;
        }
        if (matchIdentifierKeyword("KILL")) {
            parseKillStmt();
            return;
        }
        if (matchIdentifierKeyword("FLUSH")) {
            parseFlushStmt();
            return;
        }
    }
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
        case TokenType::KW_GRANT:
            parseGrantStmt();
            break;
        case TokenType::KW_REVOKE:
            parseRevokeStmt();
            break;
        case TokenType::KW_EXPLAIN:
            parseExplainStmt();
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
    int parts = 1;

    while (match(TokenType::DOT)) {
        if (parts >= 2) {
            error("MySQL qualified names must be table or db.table");
        }
        name += ".";
        name += parseIdentifier();
        parts++;
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
    if (normalized_schema.rfind("remote.emulation.mysql.", 0) == 0 ||
        normalized_schema == "remote.emulation.mysql")
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

    window_specs_.clear();
    last_expr_was_window_ = false;
    last_window_index_ = 0;

    // Handle SELECT modifiers
    bool distinct = false;
    if (matchKeyword(TokenType::KW_DISTINCT) || matchKeyword(TokenType::KW_DISTINCTROW)) {
        distinct = true;
    } else {
        matchKeyword(TokenType::KW_ALL);  // Optional, default
    }
    emitByte(distinct ? 0x01 : 0x00);

    std::vector<SelectItem> items;
    parseSelectList(items);

    bool has_from = false;
    std::string table_path;
    std::string table_alias;

    if (matchKeyword(TokenType::KW_FROM)) {
        has_from = true;

        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        auto lower_ascii = [](const std::string& value) {
            std::string out;
            out.reserve(value.size());
            for (unsigned char ch : value) {
                out.push_back(static_cast<char>(std::tolower(ch)));
            }
            return out;
        };
        bool is_dual = schema.empty() && lower_ascii(table) == "dual";

        if (matchKeyword(TokenType::KW_AS)) {
            table_alias = parseIdentifier();
        } else if ((check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) &&
                   !check(TokenType::KW_WHERE) && !check(TokenType::KW_GROUP) &&
                   !check(TokenType::KW_HAVING) && !check(TokenType::KW_ORDER) &&
                   !check(TokenType::KW_LIMIT) && !check(TokenType::KW_JOIN) &&
                   !check(TokenType::KW_LEFT) && !check(TokenType::KW_RIGHT) &&
                   !check(TokenType::KW_INNER) && !check(TokenType::KW_CROSS) &&
                   !check(TokenType::KW_ON) && !check(TokenType::KW_OFFSET)) {
            table_alias = parseIdentifier();
        }

        auto is_join_token = [](TokenType type) {
            return type == TokenType::KW_JOIN || type == TokenType::KW_LEFT ||
                   type == TokenType::KW_RIGHT || type == TokenType::KW_INNER ||
                   type == TokenType::KW_CROSS || type == TokenType::KW_NATURAL ||
                   type == TokenType::KW_STRAIGHT_JOIN;
        };
        auto is_from_terminator = [](TokenType type) {
            return type == TokenType::KW_WHERE || type == TokenType::KW_GROUP ||
                   type == TokenType::KW_HAVING || type == TokenType::KW_ORDER ||
                   type == TokenType::KW_LIMIT || type == TokenType::KW_OFFSET ||
                   type == TokenType::KW_WINDOW || type == TokenType::KW_FETCH ||
                   type == TokenType::KW_FOR || type == TokenType::KW_UNION ||
                   type == TokenType::KW_EXCEPT || type == TokenType::KW_INTERSECT;
        };

        bool has_join_tokens = check(TokenType::COMMA) || is_join_token(current_token_.type);
        if (is_dual && !has_join_tokens) {
            has_from = false;
            table_alias.clear();
        } else {
            resolveTableName(schema, table);
            table_path = schema.empty() ? table : schema + "/" + table;
        }

        if (has_join_tokens) {
            int depth = 0;
            while (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
                if (depth == 0 && is_from_terminator(current_token_.type)) {
                    break;
                }
                if (match(TokenType::LEFT_PAREN)) {
                    depth++;
                    continue;
                }
                if (match(TokenType::RIGHT_PAREN)) {
                    if (depth > 0) {
                        depth--;
                    }
                    continue;
                }
                advance();
            }
        }
    }

    if (emit_enabled_) {
        uint64_t emit_count = 0;
        for (const auto& item : items) {
            if (item.kind == SelectItem::Kind::Star ||
                item.kind == SelectItem::Kind::Column ||
                item.kind == SelectItem::Kind::Expression) {
                emit_count++;
            }
        }
        bool emit_fallback_null = false;
        if (emit_count == 0 && window_specs_.empty()) {
            emit_count = 1;
            emit_fallback_null = true;
        }

        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(emit_count);

        if (emit_fallback_null) {
            emit(sblr::Opcode::LITERAL_NULL);
            emitString("");
        } else {
            for (const auto& item : items) {
                if (item.kind == SelectItem::Kind::Star) {
                    emit(sblr::Opcode::SELECT_STAR);
                    continue;
                }
                bytecode_.insert(bytecode_.end(),
                                 item.expr_bytecode.begin(),
                                 item.expr_bytecode.end());
                emitString(item.alias);
            }
        }

        emit(sblr::Opcode::END_LIST);

        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(has_from ? 1 : 0);
        if (has_from) {
            emit(sblr::Opcode::TABLE_REF);
            emitByte(0);  // name-based reference
            emitString(table_path);
            emitString(table_alias);
        }
        emit(sblr::Opcode::END_LIST);
    }

    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    }

    if (matchKeyword(TokenType::KW_GROUP)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after GROUP");
        parseGroupByClause();
    }

    if (matchKeyword(TokenType::KW_HAVING)) {
        parseHavingClause();
    }

    if (!window_specs_.empty()) {
        emitWindowSpecs();
    }

    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
        parseOrderByClause();
    }

    if (matchKeyword(TokenType::KW_LIMIT)) {
        parseLimitClause();
    }
}

void Parser::parseSelectList(std::vector<SelectItem>& items) {
    auto decode_simple_column = [](const std::vector<uint8_t>& expr_bytes, std::string& column_out) {
        if (expr_bytes.empty()) {
            return false;
        }
        size_t pc = 0;
        if (expr_bytes[pc++] != static_cast<uint8_t>(sblr::Opcode::COLUMN_REF)) {
            return false;
        }
        uint64_t len = 0;
        size_t bytes_read = 0;
        if (!sblr::readUVarint(expr_bytes.data() + pc, expr_bytes.size() - pc, len, bytes_read)) {
            return false;
        }
        pc += bytes_read;
        if (pc + len != expr_bytes.size()) {
            return false;
        }
        column_out.assign(reinterpret_cast<const char*>(expr_bytes.data() + pc),
                          static_cast<size_t>(len));
        return true;
    };

    if (match(TokenType::STAR)) {
        SelectItem item;
        item.kind = SelectItem::Kind::Star;
        items.push_back(std::move(item));
        return;
    }

    do {
        SelectItem item;

        if (match(TokenType::STAR)) {
            item.kind = SelectItem::Kind::Star;
        } else {
            last_expr_was_window_ = false;
            item.expr_bytecode = captureExpressionBytecode();
            std::string column_name;
            if (decode_simple_column(item.expr_bytecode, column_name)) {
                item.kind = SelectItem::Kind::Column;
                item.column_name = column_name;
            } else {
                item.kind = SelectItem::Kind::Expression;
            }
        }

        bool is_window_expr = false;
        if (last_expr_was_window_ &&
            item.expr_bytecode.size() == 1 &&
            item.expr_bytecode[0] == static_cast<uint8_t>(sblr::Opcode::LITERAL_NULL)) {
            is_window_expr = true;
        }

        if (matchKeyword(TokenType::KW_AS)) {
            item.alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            item.alias = parseIdentifier();
        }

        if (is_window_expr) {
            if (last_window_index_ < window_specs_.size()) {
                auto& spec = window_specs_[last_window_index_];
                spec.output_column = item.alias.empty() ? spec.function_name : item.alias;
            }
            continue;
        }

        if (last_expr_was_window_) {
            error("Window functions cannot be combined with other expressions yet");
            if (last_window_index_ < window_specs_.size()) {
                window_specs_.erase(window_specs_.begin() + static_cast<long>(last_window_index_));
            }
            last_expr_was_window_ = false;
        }

        items.push_back(std::move(item));
    } while (match(TokenType::COMMA));
}

void Parser::parseFromClause() {
    emit(sblr::Opcode::BEGIN_LIST);

    size_t count_pos = bytecode_.size();
    emitUVarint(0);  // Placeholder

    uint32_t count = 0;
    auto patch_varint = [&](size_t pos, uint64_t value) {
        uint8_t buffer[10];
        size_t len = sblr::writeUVarint(buffer, value);
        if (len == 1) {
            bytecode_[pos] = buffer[0];
            return;
        }
        bytecode_.insert(bytecode_.begin() + pos + 1, len - 1, 0);
        std::copy(buffer, buffer + len, bytecode_.begin() + pos);
    };

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
        emitByte(0);  // name-based reference
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
            emitByte(0);  // name-based reference
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

    patch_varint(count_pos, count);
    emit(sblr::Opcode::END_LIST);
}

void Parser::parseWhereClause() {
    emit(sblr::Opcode::WHERE_CLAUSE);
    parseExpression();
}

void Parser::parseGroupByClause() {
    if (!emit_enabled_) {
        do {
            parseExpression();
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_WITH)) {
            matchKeyword(TokenType::KW_ROLLUP);
        }
        return;
    }

    std::vector<std::vector<uint8_t>> expressions;
    do {
        expressions.push_back(captureExpressionBytecode());
    } while (match(TokenType::COMMA));
    emit(sblr::Opcode::GROUP_BY);
    emitUVarint(expressions.size());
    for (const auto& expr : expressions) {
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
        }
    }

    // Handle WITH ROLLUP
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_ROLLUP)) {
        // ROLLUP parsed but not emitted in bytecode yet.
        }
    }
}

void Parser::parseHavingClause() {
    emit(sblr::Opcode::HAVING);
    parseExpression();
}

void Parser::parseOrderByClause() {
    struct SortKey {
        std::vector<uint8_t> expr;
        bool descending = false;
        bool nulls_first = false;
        bool nulls_specified = false;
    };

    std::vector<SortKey> keys;

    do {
        SortKey key;
        key.expr = captureExpressionBytecode();

        if (matchKeyword(TokenType::KW_DESC)) {
            key.descending = true;
        } else {
            matchKeyword(TokenType::KW_ASC);
        }

        if (matchKeyword(TokenType::KW_NULLS)) {
            key.nulls_specified = true;
            if (matchKeyword(TokenType::KW_FIRST)) {
                key.nulls_first = true;
            } else if (matchKeyword(TokenType::KW_LAST)) {
                key.nulls_first = false;
            }
        }

        keys.push_back(std::move(key));
    } while (match(TokenType::COMMA));

    if (keys.empty()) {
        return;
    }

    emit(sblr::Opcode::ORDER_BY);
    emitUVarint(keys.size());

    for (const auto& key : keys) {
        emit(sblr::Opcode::SORT_KEY);
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), key.expr.begin(), key.expr.end());
        }
        emit(key.descending ? sblr::Opcode::SORT_DESC : sblr::Opcode::SORT_ASC);
        if (key.nulls_specified) {
            emit(key.nulls_first ? sblr::Opcode::NULLS_FIRST : sblr::Opcode::NULLS_LAST);
        }
    }
}

void Parser::parseLimitClause() {
    int64_t limit_value = -1;
    int64_t offset_value = 0;
    bool has_offset = false;

    if (check(TokenType::INTEGER_LITERAL)) {
        int64_t first = current_token_.value.int_value;
        advance();

        if (match(TokenType::COMMA)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                limit_value = current_token_.value.int_value;
                offset_value = first;
                has_offset = true;
                advance();
            }
        } else {
            limit_value = first;
            if (matchKeyword(TokenType::KW_OFFSET)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    offset_value = current_token_.value.int_value;
                    has_offset = true;
                    advance();
                }
            }
        }
    } else {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseExpression();
        emit_enabled_ = prev_emit;
        return;
    }

    if (limit_value >= 0) {
        emit(sblr::Opcode::LIMIT);
        emitI64(limit_value);
    }
    if (has_offset) {
        emit(sblr::Opcode::OFFSET);
        emitI64(offset_value);
    }
}

std::vector<uint8_t> Parser::captureExpressionBytecode() {
    std::vector<uint8_t> saved;
    saved.swap(bytecode_);

    bool saved_emit = emit_enabled_;
    emit_enabled_ = true;

    bytecode_.clear();
    parseExpression();

    std::vector<uint8_t> expr;
    expr.swap(bytecode_);

    bytecode_.swap(saved);
    emit_enabled_ = saved_emit;
    return expr;
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
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
    } else if (matchKeyword(TokenType::KW_IS)) {
        // IS [NOT] NULL / IS [NOT] TRUE / IS [NOT] FALSE
        bool is_not = matchKeyword(TokenType::KW_NOT);

        if (matchKeyword(TokenType::KW_NULL)) {
            emit(sblr::Opcode::LITERAL_NULL);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
        } else if (matchKeyword(TokenType::KW_TRUE)) {
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(1);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
        } else if (matchKeyword(TokenType::KW_FALSE)) {
            emit(sblr::Opcode::LITERAL_INT32);
            emitU32(0);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_NULL_SAFE_EQ));
            if (is_not) {
                emit(sblr::Opcode::LITERAL_INT32);
                emitU32(0);
                emit(sblr::Opcode::EXPR_EQ);
            }
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
            uint64_t count = 0;
            do {
                parseExpression();
                count++;
            } while (match(TokenType::COMMA));
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_IN_LIST));
            emitByte(0);
            emitUVarint(count);
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
        bool has_escape = false;
        if (matchKeyword(TokenType::KW_ESCAPE)) {
            parseAdditiveExpr();
            has_escape = true;
        }

        if (has_escape) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_LIKE_ESCAPE));
        } else {
            emit(sblr::Opcode::EXPR_LIKE);
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
        if (next_placeholder_index_ > std::numeric_limits<uint16_t>::max()) {
            error("Too many placeholders in statement");
            return;
        }
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_PLACEHOLDER));
        emitU16(static_cast<uint16_t>(next_placeholder_index_++));
        emitU16(0);  // type hint unknown
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

    if (matchKeyword(TokenType::KW_EXTRACT)) {
        parseExtractExpr();
        return;
    }

    if (matchKeyword(TokenType::KW_ALTER_ELEMENT)) {
        parseAlterElementExpr();
        return;
    }

    if (matchKeyword(TokenType::KW_MATCH)) {
        consume(TokenType::LEFT_PAREN, "Expected ( after MATCH");
        parseMatchAgainstExpr();
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
        std::string upper_name = name;
        std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                       [](char c) { return static_cast<char>(std::toupper(c)); });

        if (upper_name == "MATCH" && match(TokenType::LEFT_PAREN)) {
            parseMatchAgainstExpr();
            return;
        }

        // Check if function call
        if (match(TokenType::LEFT_PAREN)) {
            parseFunctionCall(name);
            return;
        }

        // Column reference (possibly qualified)
        std::string column = name;
        if (match(TokenType::DOT)) {
            column = parseIdentifier();
            if (match(TokenType::DOT)) {
                column = parseIdentifier();
            }
        }

        emit(sblr::Opcode::COLUMN_REF);
        emitString(column);
        return;
    }

    // Many MySQL keywords can be used as identifiers in expression context
    // (non-reserved keywords like STATUS, DATA, NAME, etc.)
    if (isNonReservedKeyword(current_token_.type)) {
        std::string name = tokenToString(current_token_.type);
        advance();
        std::string upper_name = name;
        std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                       [](char c) { return static_cast<char>(std::toupper(c)); });

        if (upper_name == "MATCH" && match(TokenType::LEFT_PAREN)) {
            parseMatchAgainstExpr();
            return;
        }

        // Check if function call
        if (match(TokenType::LEFT_PAREN)) {
            parseFunctionCall(name);
            return;
        }

        // Column reference (possibly qualified)
        std::string column = name;
        if (match(TokenType::DOT)) {
            if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                column = parseIdentifier();
            } else if (isNonReservedKeyword(current_token_.type)) {
                column = tokenToString(current_token_.type);
                advance();
            }

            if (match(TokenType::DOT)) {
                if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                    column = parseIdentifier();
                } else if (isNonReservedKeyword(current_token_.type)) {
                    column = tokenToString(current_token_.type);
                    advance();
                }
            }
        }

        emit(sblr::Opcode::COLUMN_REF);
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

    if (upper_name == "VALUES") {
        if (!in_on_duplicate_update_) {
            error("VALUES() is only supported in ON DUPLICATE KEY UPDATE");
        }

        if (compat_mode_ == MySQLCompatMode::MYSQL80) {
            warning("VALUES() is deprecated in MySQL 8.0; use a column alias instead");
        }

        auto parse_values_identifier = [&]() -> std::string {
            if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                return parseIdentifier();
            }
            if (isNonReservedKeyword(current_token_.type)) {
                std::string token = tokenToString(current_token_.type);
                advance();
                return token;
            }
            error("Expected column name in VALUES()");
            return "";
        };

        std::string column = parse_values_identifier();
        if (match(TokenType::DOT)) {
            column = parse_values_identifier();
            if (match(TokenType::DOT)) {
                column = parse_values_identifier();
            }
        }
        if (match(TokenType::COMMA)) {
            error("VALUES() expects a single column name");
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after VALUES()");

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_INSERTED_COLUMN_REF));
        emitString(column);
        return;
    }

    // Aggregate functions
    if (upper_name == "COUNT") {
        bool distinct = matchKeyword(TokenType::KW_DISTINCT);
        bool count_star = false;
        std::vector<uint8_t> expr_bytes;
        if (match(TokenType::STAR)) {
            count_star = true;
        } else {
            expr_bytes = captureExpressionBytecode();
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            WindowFunctionSpec spec;
            spec.is_aggregate = true;
            spec.agg_opcode = sblr::Opcode::AGG_COUNT;
            spec.agg_count_star = count_star;
            spec.function_name = upper_name;
            if (!count_star) {
                spec.agg_expr = std::move(expr_bytes);
            }
            if (distinct) {
                warning("COUNT(DISTINCT ...) window aggregates ignore DISTINCT for now");
            }
            parseWindowSpecForFunction(spec);
            window_specs_.push_back(std::move(spec));
            last_window_index_ = window_specs_.size() - 1;
            last_expr_was_window_ = true;
            emit(sblr::Opcode::LITERAL_NULL);
            return;
        }
        emit(sblr::Opcode::AGG_COUNT);
        emitByte(1);
        if (count_star) {
            emit(sblr::Opcode::LITERAL_NULL);
        } else {
            if (distinct) {
                warning("COUNT(DISTINCT ...) is parsed but DISTINCT is ignored");
            }
            bytecode_.insert(bytecode_.end(), expr_bytes.begin(), expr_bytes.end());
        }
        return;
    }

    if (upper_name == "SUM") {
        bool distinct = matchKeyword(TokenType::KW_DISTINCT);
        std::vector<uint8_t> expr_bytes = captureExpressionBytecode();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            WindowFunctionSpec spec;
            spec.is_aggregate = true;
            spec.agg_opcode = sblr::Opcode::AGG_SUM;
            spec.function_name = upper_name;
            spec.agg_expr = std::move(expr_bytes);
            if (distinct) {
                warning("SUM(DISTINCT ...) window aggregates ignore DISTINCT for now");
            }
            parseWindowSpecForFunction(spec);
            window_specs_.push_back(std::move(spec));
            last_window_index_ = window_specs_.size() - 1;
            last_expr_was_window_ = true;
            emit(sblr::Opcode::LITERAL_NULL);
            return;
        }
        emit(sblr::Opcode::AGG_SUM);
        emitByte(1);
        if (distinct) {
            warning("SUM(DISTINCT ...) is parsed but DISTINCT is ignored");
        }
        bytecode_.insert(bytecode_.end(), expr_bytes.begin(), expr_bytes.end());
        return;
    }

    if (upper_name == "AVG") {
        bool distinct = matchKeyword(TokenType::KW_DISTINCT);
        std::vector<uint8_t> expr_bytes = captureExpressionBytecode();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            WindowFunctionSpec spec;
            spec.is_aggregate = true;
            spec.agg_opcode = sblr::Opcode::AGG_AVG;
            spec.function_name = upper_name;
            spec.agg_expr = std::move(expr_bytes);
            if (distinct) {
                warning("AVG(DISTINCT ...) window aggregates ignore DISTINCT for now");
            }
            parseWindowSpecForFunction(spec);
            window_specs_.push_back(std::move(spec));
            last_window_index_ = window_specs_.size() - 1;
            last_expr_was_window_ = true;
            emit(sblr::Opcode::LITERAL_NULL);
            return;
        }
        emit(sblr::Opcode::AGG_AVG);
        emitByte(1);
        if (distinct) {
            warning("AVG(DISTINCT ...) is parsed but DISTINCT is ignored");
        }
        bytecode_.insert(bytecode_.end(), expr_bytes.begin(), expr_bytes.end());
        return;
    }

    if (upper_name == "MIN") {
        std::vector<uint8_t> expr_bytes = captureExpressionBytecode();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            WindowFunctionSpec spec;
            spec.is_aggregate = true;
            spec.agg_opcode = sblr::Opcode::AGG_MIN;
            spec.function_name = upper_name;
            spec.agg_expr = std::move(expr_bytes);
            parseWindowSpecForFunction(spec);
            window_specs_.push_back(std::move(spec));
            last_window_index_ = window_specs_.size() - 1;
            last_expr_was_window_ = true;
            emit(sblr::Opcode::LITERAL_NULL);
            return;
        }
        emit(sblr::Opcode::AGG_MIN);
        emitByte(1);
        bytecode_.insert(bytecode_.end(), expr_bytes.begin(), expr_bytes.end());
        return;
    }

    if (upper_name == "MAX") {
        std::vector<uint8_t> expr_bytes = captureExpressionBytecode();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (matchKeyword(TokenType::KW_OVER)) {
            WindowFunctionSpec spec;
            spec.is_aggregate = true;
            spec.agg_opcode = sblr::Opcode::AGG_MAX;
            spec.function_name = upper_name;
            spec.agg_expr = std::move(expr_bytes);
            parseWindowSpecForFunction(spec);
            window_specs_.push_back(std::move(spec));
            last_window_index_ = window_specs_.size() - 1;
            last_expr_was_window_ = true;
            emit(sblr::Opcode::LITERAL_NULL);
            return;
        }
        emit(sblr::Opcode::AGG_MAX);
        emitByte(1);
        bytecode_.insert(bytecode_.end(), expr_bytes.begin(), expr_bytes.end());
        return;
    }

    // Window functions
    if (upper_name == "ROW_NUMBER") {
        consume(TokenType::RIGHT_PAREN, "Expected )");
        WindowFunctionSpec spec;
        spec.func_opcode = sblr::Opcode::WIN_ROW_NUMBER;
        spec.function_name = upper_name;
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("ROW_NUMBER requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "RANK") {
        consume(TokenType::RIGHT_PAREN, "Expected )");
        WindowFunctionSpec spec;
        spec.func_opcode = sblr::Opcode::WIN_RANK;
        spec.function_name = upper_name;
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("RANK requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "DENSE_RANK") {
        consume(TokenType::RIGHT_PAREN, "Expected )");
        WindowFunctionSpec spec;
        spec.func_opcode = sblr::Opcode::WIN_DENSE_RANK;
        spec.function_name = upper_name;
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("DENSE_RANK requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "LAG" || upper_name == "LEAD") {
        WindowFunctionSpec spec;
        spec.func_opcode = (upper_name == "LAG") ? sblr::Opcode::WIN_LAG
                                                 : sblr::Opcode::WIN_LEAD;
        spec.function_name = upper_name;
        if (!check(TokenType::RIGHT_PAREN)) {
            spec.args.push_back(captureExpressionBytecode());
            if (match(TokenType::COMMA)) {
                spec.args.push_back(captureExpressionBytecode());
                if (match(TokenType::COMMA)) {
                    spec.args.push_back(captureExpressionBytecode());
                }
            }
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("LAG/LEAD requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "FIRST_VALUE") {
        WindowFunctionSpec spec;
        spec.func_opcode = sblr::Opcode::WIN_FIRST_VALUE;
        spec.function_name = upper_name;
        if (!check(TokenType::RIGHT_PAREN)) {
            spec.args.push_back(captureExpressionBytecode());
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("FIRST_VALUE requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "LAST_VALUE") {
        WindowFunctionSpec spec;
        spec.func_opcode = sblr::Opcode::WIN_LAST_VALUE;
        spec.function_name = upper_name;
        if (!check(TokenType::RIGHT_PAREN)) {
            spec.args.push_back(captureExpressionBytecode());
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("LAST_VALUE requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "NTH_VALUE") {
        WindowFunctionSpec spec;
        spec.func_opcode = sblr::Opcode::WIN_NTH_VALUE;
        spec.function_name = upper_name;
        if (!check(TokenType::RIGHT_PAREN)) {
            spec.args.push_back(captureExpressionBytecode());
            consume(TokenType::COMMA, "Expected ,");
            spec.args.push_back(captureExpressionBytecode());
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("NTH_VALUE requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "CUME_DIST") {
        consume(TokenType::RIGHT_PAREN, "Expected )");
        WindowFunctionSpec spec;
        spec.is_extended = true;
        spec.ext_opcode = sblr::ExtendedOpcode::EXT_WIN_CUME_DIST;
        spec.function_name = upper_name;
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("CUME_DIST requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
        return;
    }
    if (upper_name == "PERCENT_RANK") {
        consume(TokenType::RIGHT_PAREN, "Expected )");
        WindowFunctionSpec spec;
        spec.is_extended = true;
        spec.ext_opcode = sblr::ExtendedOpcode::EXT_WIN_PERCENT_RANK;
        spec.function_name = upper_name;
        if (!matchKeyword(TokenType::KW_OVER)) {
            error("PERCENT_RANK requires OVER clause");
            return;
        }
        parseWindowSpecForFunction(spec);
        window_specs_.push_back(std::move(spec));
        last_window_index_ = window_specs_.size() - 1;
        last_expr_was_window_ = true;
        emit(sblr::Opcode::LITERAL_NULL);
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

std::string Parser::parseWindowColumnName() {
    auto parse_identifier = [&]() -> std::string {
        if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            return parseIdentifier();
        }
        if (isNonReservedKeyword(current_token_.type)) {
            std::string name = tokenToString(current_token_.type);
            advance();
            return name;
        }
        error("Expected column name in window clause");
        return "";
    };

    std::string column = parse_identifier();
    if (match(TokenType::DOT)) {
        column = parse_identifier();
        if (match(TokenType::DOT)) {
            column = parse_identifier();
        }
    }
    return column;
}

bool Parser::parseWindowFrameBound(sblr::Opcode& bound_out) {
    if (matchKeyword(TokenType::KW_UNBOUNDED)) {
        if (matchKeyword(TokenType::KW_PRECEDING)) {
            bound_out = sblr::Opcode::FRAME_UNBOUNDED_PRECEDING;
            return true;
        }
        if (matchKeyword(TokenType::KW_FOLLOWING)) {
            bound_out = sblr::Opcode::FRAME_UNBOUNDED_FOLLOWING;
            return true;
        }
        error("Expected PRECEDING or FOLLOWING after UNBOUNDED");
        return false;
    }
    if (matchKeyword(TokenType::KW_CURRENT)) {
        consumeKeyword(TokenType::KW_ROW, "Expected ROW after CURRENT");
        bound_out = sblr::Opcode::FRAME_CURRENT_ROW;
        return true;
    }

    parseExpression();
    if (matchKeyword(TokenType::KW_PRECEDING) || matchKeyword(TokenType::KW_FOLLOWING)) {
        error("Window frame offsets are not supported yet");
    } else {
        error("Expected PRECEDING or FOLLOWING in frame bound");
    }
    return false;
}

void Parser::parseWindowSpecForFunction(WindowFunctionSpec& spec) {
    if (!match(TokenType::LEFT_PAREN)) {
        error("Named windows are not supported in MySQL parser");
        return;
    }

    if (matchKeyword(TokenType::KW_PARTITION)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after PARTITION");
        do {
            spec.partition_columns.push_back(parseWindowColumnName());
        } while (match(TokenType::COMMA));
    }

    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
        do {
            spec.order_columns.push_back(parseWindowColumnName());
            if (matchKeyword(TokenType::KW_DESC)) {
                // DESC parsed but not emitted in window bytecode yet.
            } else {
                matchKeyword(TokenType::KW_ASC);
            }
            if (matchKeyword(TokenType::KW_NULLS)) {
                if (matchKeyword(TokenType::KW_FIRST)) {
                } else if (matchKeyword(TokenType::KW_LAST)) {
                }
            }
        } while (match(TokenType::COMMA));
    }

    TokenType frame_type = TokenType::END_OF_FILE;
    if (matchKeyword(TokenType::KW_ROWS)) {
        frame_type = TokenType::KW_ROWS;
    } else if (matchKeyword(TokenType::KW_RANGE)) {
        frame_type = TokenType::KW_RANGE;
    } else if (matchKeyword(TokenType::KW_GROUPS)) {
        frame_type = TokenType::KW_GROUPS;
    }

    if (frame_type != TokenType::END_OF_FILE) {
        spec.has_frame = true;
        if (frame_type == TokenType::KW_RANGE) {
            spec.frame_mode = sblr::Opcode::FRAME_RANGE;
        } else if (frame_type == TokenType::KW_GROUPS) {
            spec.frame_mode = sblr::Opcode::FRAME_GROUPS;
        } else {
            spec.frame_mode = sblr::Opcode::FRAME_ROWS;
        }

        if (matchKeyword(TokenType::KW_BETWEEN)) {
            if (!parseWindowFrameBound(spec.frame_start)) {
                return;
            }
            consumeKeyword(TokenType::KW_AND, "Expected AND in frame clause");
            if (!parseWindowFrameBound(spec.frame_end)) {
                return;
            }
        } else {
            if (!parseWindowFrameBound(spec.frame_start)) {
                return;
            }
            spec.frame_end = sblr::Opcode::FRAME_CURRENT_ROW;
        }
    }

    consume(TokenType::RIGHT_PAREN, "Expected )");
}

void Parser::emitWindowSpecs() {
    emit(sblr::Opcode::WINDOW);
    emitU32(static_cast<uint32_t>(window_specs_.size()));

    for (auto& spec : window_specs_) {
        if (spec.is_aggregate) {
            emit(spec.agg_opcode);
        } else if (spec.is_extended) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(spec.ext_opcode));
        } else {
            emit(spec.func_opcode);
        }

        if (spec.is_aggregate) {
            if (spec.agg_count_star) {
                emitU32(0);
            } else {
                emitU32(1);
                bytecode_.insert(bytecode_.end(), spec.agg_expr.begin(), spec.agg_expr.end());
            }
        } else {
            emitU32(static_cast<uint32_t>(spec.args.size()));
            for (const auto& arg : spec.args) {
                bytecode_.insert(bytecode_.end(), arg.begin(), arg.end());
            }
        }

        emit(sblr::Opcode::WINDOW_SPEC);

        emitU32(static_cast<uint32_t>(spec.partition_columns.size()));
        if (!spec.partition_columns.empty()) {
            emit(sblr::Opcode::PARTITION_BY);
            for (const auto& column : spec.partition_columns) {
                emit(sblr::Opcode::COLUMN_REF);
                emitString("");
                emitString(column);
            }
        }

        emitU32(static_cast<uint32_t>(spec.order_columns.size()));
        if (!spec.order_columns.empty()) {
            emit(sblr::Opcode::WINDOW_ORDER_BY);
            for (const auto& column : spec.order_columns) {
                emit(sblr::Opcode::COLUMN_REF);
                emitString("");
                emitString(column);
            }
        }

        emitU32(spec.has_frame ? 1u : 0u);
        if (spec.has_frame) {
            emit(sblr::Opcode::FRAME_CLAUSE);
            emit(spec.frame_mode);
            emit(spec.frame_start);
            emit(spec.frame_end);
        }

        if (spec.output_column.empty()) {
            spec.output_column = spec.function_name;
        }
        emitString(spec.output_column);
    }
}

void Parser::parseWindowClause() {
    emit(sblr::Opcode::WINDOW);

    if (match(TokenType::LEFT_PAREN)) {
        emit(sblr::Opcode::WINDOW_SPEC);

        if (matchKeyword(TokenType::KW_PARTITION)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY after PARTITION");
            emit(sblr::Opcode::PARTITION_BY);
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

        if (matchKeyword(TokenType::KW_ORDER)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY after ORDER");
            emit(sblr::Opcode::WINDOW_ORDER_BY);
            emit(sblr::Opcode::BEGIN_LIST);
            size_t count_pos = bytecode_.size();
            emitU32(0);
            uint32_t count = 0;
            do {
                emit(sblr::Opcode::SORT_KEY);
                parseExpression();
                if (matchKeyword(TokenType::KW_DESC)) {
                    emit(sblr::Opcode::SORT_DESC);
                } else {
                    matchKeyword(TokenType::KW_ASC);
                    emit(sblr::Opcode::SORT_ASC);
                }
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

        if (matchKeyword(TokenType::KW_ROWS) || matchKeyword(TokenType::KW_RANGE) ||
            matchKeyword(TokenType::KW_GROUPS)) {
            emit(sblr::Opcode::FRAME_CLAUSE);
            if (matchKeyword(TokenType::KW_BETWEEN)) {
                parseFrameBound();
                consumeKeyword(TokenType::KW_AND, "Expected AND in frame clause");
                parseFrameBound();
            } else {
                parseFrameBound();
            }
        }

        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else {
        std::string window_name = parseIdentifier();
        emitString(window_name);
    }
}

void Parser::parseFrameBound() {
    if (matchKeyword(TokenType::KW_UNBOUNDED)) {
        if (matchKeyword(TokenType::KW_PRECEDING)) {
            emit(sblr::Opcode::FRAME_UNBOUNDED_PRECEDING);
        } else if (matchKeyword(TokenType::KW_FOLLOWING)) {
            emit(sblr::Opcode::FRAME_UNBOUNDED_FOLLOWING);
        }
    } else if (matchKeyword(TokenType::KW_CURRENT)) {
        consumeKeyword(TokenType::KW_ROW, "Expected ROW after CURRENT");
        emit(sblr::Opcode::FRAME_CURRENT_ROW);
    } else {
        parseExpression();
        if (matchKeyword(TokenType::KW_PRECEDING)) {
            emit(sblr::Opcode::FRAME_PRECEDING);
        } else if (matchKeyword(TokenType::KW_FOLLOWING)) {
            emit(sblr::Opcode::FRAME_FOLLOWING);
        }
    }
}

void Parser::parseMatchAgainstExpr() {
    std::vector<std::string> columns;
    do {
        std::string schema;
        std::string column = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = column;
            column = parseIdentifier();
            if (match(TokenType::DOT)) {
                column = parseIdentifier();
            }
        }
        columns.push_back(column);
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected ) after MATCH column list");

    if (!matchIdentifierKeyword("AGAINST")) {
        error("Expected AGAINST after MATCH");
        synchronize();
        return;
    }
    consume(TokenType::LEFT_PAREN, "Expected ( after AGAINST");

    // Build tsvector from MATCH columns
    if (columns.empty()) {
        error("MATCH requires at least one column");
    } else if (columns.size() == 1) {
        emit(sblr::Opcode::COLUMN_REF);
        emitString(columns[0]);
    } else {
        emit(sblr::Opcode::LITERAL_STRING);
        emitString(" ");
        for (const auto& col : columns) {
            emit(sblr::Opcode::COLUMN_REF);
            emitString(col);
        }
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNC_CONCAT_WS));
        emitByte(static_cast<uint8_t>(columns.size() + 1));
    }
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_TO_TSVECTOR));
    emitByte(1);

    bool against_is_literal = check(TokenType::STRING_LITERAL);
    std::string against_literal;
    if (against_is_literal) {
        std::string_view str = lexer_.stringPool().get(current_token_.value.string_id);
        against_literal.assign(str.data(), str.size());
    }
    auto build_literal_expr = [&](const std::string& value) {
        std::vector<uint8_t> saved;
        saved.swap(bytecode_);
        bool prev_emit = emit_enabled_;
        emit_enabled_ = true;
        bytecode_.clear();
        emit(sblr::Opcode::LITERAL_STRING);
        emitString(value);
        std::vector<uint8_t> expr;
        expr.swap(bytecode_);
        bytecode_.swap(saved);
        emit_enabled_ = prev_emit;
        return expr;
    };

    bool allow_modifiers = false;
    if (against_is_literal) {
        Token next = lexer_.peek();
        allow_modifiers = (next.type == TokenType::KW_IN || next.type == TokenType::KW_WITH);
    }

    std::vector<uint8_t> against_expr;
    if (against_is_literal && allow_modifiers) {
        against_expr = build_literal_expr(against_literal);
        advance();
    } else {
        against_expr = captureExpressionBytecode();
    }

    bool boolean_mode = false;
    if (matchKeyword(TokenType::KW_IN)) {
        if (matchKeyword(TokenType::KW_BOOLEAN) || matchIdentifierKeyword("BOOLEAN")) {
            matchIdentifierKeyword("MODE");
            boolean_mode = true;
        } else if (matchKeyword(TokenType::KW_NATURAL) || matchIdentifierKeyword("NATURAL")) {
            if (matchKeyword(TokenType::KW_LANGUAGE) || matchIdentifierKeyword("LANGUAGE")) {
            }
            matchIdentifierKeyword("MODE");
        }
    }
    bool query_expansion = false;
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchIdentifierKeyword("QUERY")) {
            matchIdentifierKeyword("EXPANSION");
            query_expansion = true;
        }
    }

    if (query_expansion && against_is_literal) {
        auto expand_query = [](const std::string& input) {
            std::string out;
            std::string token;
            for (char ch : input) {
                if (std::isspace(static_cast<unsigned char>(ch))) {
                    if (!token.empty()) {
                        if (!out.empty()) {
                            out += " | ";
                        }
                        out += token;
                        token.clear();
                    }
                    continue;
                }
                token.push_back(ch);
            }
            if (!token.empty()) {
                if (!out.empty()) {
                    out += " | ";
                }
                out += token;
            }
            return out.empty() ? input : out;
        };

        against_expr = build_literal_expr(expand_query(against_literal));
        boolean_mode = true;
    } else if (query_expansion) {
        warning("WITH QUERY EXPANSION only supported for string literal AGAINST");
    }

    bytecode_.insert(bytecode_.end(), against_expr.begin(), against_expr.end());

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(
        boolean_mode ? sblr::ExtendedOpcode::EXT_TO_TSQUERY
                     : sblr::ExtendedOpcode::EXT_PLAINTO_TSQUERY));
    emitByte(1);

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_TS_RANK));
    emitByte(2);

    consume(TokenType::RIGHT_PAREN, "Expected ) after AGAINST clause");
}

void Parser::parseCastExpr() {
    consume(TokenType::KW_CAST, "Expected CAST");
    consume(TokenType::LEFT_PAREN, "Expected (");

    parseExpression();

    consumeKeyword(TokenType::KW_AS, "Expected AS");

    // Parse target type
    MySQLDataType type = parseDataType();
    core::CastFormat cast_format = core::CastFormat::DEFAULT;
    // CAST ... USING <format> (see docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md)
    if (matchKeyword(TokenType::KW_USING)) {
        std::string fmt = parseIdentifier();
        std::transform(fmt.begin(), fmt.end(), fmt.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (fmt == "hex" || fmt == "hexadecimal") {
            cast_format = core::CastFormat::HEX;
        } else if (fmt == "base64") {
            cast_format = core::CastFormat::BASE64;
        } else if (fmt == "escape") {
            cast_format = core::CastFormat::ESCAPE;
        } else {
            error("Unknown CAST USING format: " + fmt);
        }
    }

    // Emit type opcode
    emit(sblr::Opcode::EXPR_CAST);
    emitByte(0);  // try_cast = false
    emitTypeDefinition(type);
    emitByte(static_cast<uint8_t>(cast_format));

    consume(TokenType::RIGHT_PAREN, "Expected )");
}

void Parser::parseExtractExpr() {
    consume(TokenType::LEFT_PAREN, "Expected ( after EXTRACT");
    uint8_t arg_count = 0;
    sblr::ExtractField field = parseElementSelector(arg_count);
    consumeKeyword(TokenType::KW_FROM, "Expected FROM in EXTRACT expression");
    parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ) after EXTRACT");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_EXTRACT));
    emitByte(static_cast<uint8_t>(field));
    emitByte(arg_count);
}

void Parser::parseAlterElementExpr() {
    consume(TokenType::LEFT_PAREN, "Expected ( after ALTER_ELEMENT");
    uint8_t arg_count = 0;
    sblr::ExtractField field = parseElementSelector(arg_count);
    consumeKeyword(TokenType::KW_IN, "Expected IN in ALTER_ELEMENT expression");
    parseExpression();
    consumeKeyword(TokenType::KW_TO, "Expected TO in ALTER_ELEMENT expression");
    parseExpression();
    consume(TokenType::RIGHT_PAREN, "Expected ) after ALTER_ELEMENT");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_ELEMENT));
    emitByte(static_cast<uint8_t>(field));
    emitByte(arg_count);
}

sblr::ExtractField Parser::parseElementSelector(uint8_t& arg_count) {
    arg_count = 0;

    if (check(TokenType::STRING_LITERAL)) {
        std::string_view path = lexer_.stringPool().get(current_token_.value.string_id);
        emit(sblr::Opcode::LITERAL_STRING);
        emitString(path);
        advance();
        arg_count = 1;
        return sblr::ExtractField::PATH;
    }

    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::PLUS) ||
        check(TokenType::MINUS) || check(TokenType::LEFT_PAREN)) {
        parseExpression();
        arg_count = 1;
        return sblr::ExtractField::ELEMENT;
    }

    std::string name = parseIdentifier();
    bool has_args = false;

    if (match(TokenType::LEFT_PAREN)) {
        has_args = true;
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                parseExpression();
                arg_count++;
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected ) after element selector arguments");
    }

    auto resolved = sblr::resolveExtractFieldName(name);
    if (!resolved) {
        if (has_args) {
            error("Unknown EXTRACT element: " + name);
            return sblr::ExtractField::VALUE;
        }
        emit(sblr::Opcode::LITERAL_STRING);
        emitString(name);
        arg_count = 1;
        return sblr::ExtractField::FIELD;
    }

    return *resolved;
}

// ============================================================================
// INSERT Statement
// ============================================================================

void Parser::parseInsertStmt() {
    consume(TokenType::KW_INSERT, "Expected INSERT");

    // Optional modifiers
    bool saw_priority_modifier = false;
    if (matchKeyword(TokenType::KW_LOW_PRIORITY)) {
        saw_priority_modifier = true;
    }
    if (matchKeyword(TokenType::KW_DELAYED)) {
        saw_priority_modifier = true;
    }
    if (matchKeyword(TokenType::KW_HIGH_PRIORITY)) {
        saw_priority_modifier = true;
    }
    bool ignore_duplicates = matchKeyword(TokenType::KW_IGNORE);
    if (saw_priority_modifier) {
        warning("INSERT modifiers LOW_PRIORITY/HIGH_PRIORITY/DELAYED are ignored in ScratchBird");
    }

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

    std::string table_path = schema.empty() ? table : schema + "/" + table;
    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);  // name-based reference
    emitString(table_path);
    emitString("");

    std::vector<std::string> columns;
    bool has_column_list = false;
    if (match(TokenType::LEFT_PAREN)) {
        has_column_list = true;
        do {
            columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    struct InsertValue {
        bool is_default = false;
        std::vector<uint8_t> expr;
    };
    struct OnDuplicateAssignment {
        std::string column;
        std::vector<uint8_t> expr;
    };
    std::vector<std::vector<InsertValue>> rows;
    bool default_values_only = false;
    bool has_select = false;
    std::vector<uint8_t> select_bytecode;
    std::vector<OnDuplicateAssignment> on_duplicate_assignments;

    if (matchKeyword(TokenType::KW_DEFAULT)) {
        matchKeyword(TokenType::KW_VALUES);
        default_values_only = true;
    } else if (matchKeyword(TokenType::KW_VALUES) || matchKeyword(TokenType::KW_VALUE)) {
        do {
            consume(TokenType::LEFT_PAREN, "Expected (");
            std::vector<InsertValue> row;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    InsertValue val;
                    if (matchKeyword(TokenType::KW_DEFAULT)) {
                        val.is_default = true;
                    } else {
                        val.expr = captureExpressionBytecode();
                    }
                    row.push_back(std::move(val));
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
            rows.push_back(std::move(row));
        } while (match(TokenType::COMMA));
    } else if (check(TokenType::KW_SELECT)) {
        auto capture_select = [&]() {
            std::vector<uint8_t> saved;
            saved.swap(bytecode_);
            bool saved_emit = emit_enabled_;
            emit_enabled_ = true;
            bytecode_.clear();
            parseSelectStmt();
            std::vector<uint8_t> stmt;
            stmt.swap(bytecode_);
            bytecode_.swap(saved);
            emit_enabled_ = saved_emit;
            return stmt;
        };
        has_select = true;
        select_bytecode = capture_select();
    }

    if (!has_column_list) {
        auto split_components = [](const std::string& path) {
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

        if (db_ && !rows.empty()) {
            core::ObjectPath path;
            path.components = split_components(schema);
            path.components.push_back(table);
            path.type = schema.empty() ? core::PathType::UNQUALIFIED : core::PathType::ABSOLUTE;

            core::CatalogManager::ResolveOptions opts;
            opts.required_privilege =
                static_cast<uint32_t>(core::CatalogManager::Privilege::INSERT);
            core::CatalogManager::ObjectType resolved_type;
            core::ID table_id;
            core::ErrorContext ctx;
            if (db_->catalog_manager()->resolveObjectPath(path,
                                                         core::CatalogManager::ObjectType::TABLE,
                                                         opts, table_id, resolved_type, &ctx) == core::Status::OK) {
                std::vector<core::CatalogManager::ColumnInfo> cols;
                if (db_->catalog_manager()->getColumns(table_id, cols, &ctx,
                                                       opts.required_privilege) == core::Status::OK) {
                    for (const auto& col : cols) {
                        columns.push_back(col.column_name);
                    }
                }
            }
        }
    }

    std::vector<std::string> emit_columns;
    std::vector<std::vector<std::vector<uint8_t>>> emit_rows;

    bool has_default = false;
    for (const auto& row : rows) {
        for (const auto& val : row) {
            if (val.is_default) {
                has_default = true;
                break;
            }
        }
    }

    if (rows.size() > 1 && has_default) {
        error("DEFAULT values in multi-row INSERT are not supported yet");
    }

    if (!rows.empty()) {
        if (has_default && columns.empty()) {
            error("DEFAULT values require a resolved column list");
        }

        if (has_default) {
            const auto& row = rows.front();
            std::vector<std::vector<uint8_t>> row_exprs;
            for (size_t i = 0; i < row.size() && i < columns.size(); ++i) {
                if (row[i].is_default) {
                    continue;
                }
                emit_columns.push_back(columns[i]);
                row_exprs.push_back(row[i].expr);
            }
            emit_rows.push_back(std::move(row_exprs));
        } else {
            emit_columns = columns;
            for (const auto& row : rows) {
                if (!columns.empty() && row.size() != columns.size()) {
                    error("Column count doesn't match value count");
                }
                std::vector<std::vector<uint8_t>> row_exprs;
                row_exprs.reserve(row.size());
                for (const auto& val : row) {
                    row_exprs.push_back(val.expr);
                }
                emit_rows.push_back(std::move(row_exprs));
            }
        }
    }

    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(emit_columns.size());
    for (const auto& col : emit_columns) {
        emit(sblr::Opcode::COLUMN_REF);
        emitString(col);
    }
    emit(sblr::Opcode::END_LIST);

    if (has_select) {
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), select_bytecode.begin(), select_bytecode.end());
        }
    } else {
        emit(sblr::Opcode::BEGIN_LIST);
        if (default_values_only) {
            emitUVarint(0);
        } else {
            emitUVarint(emit_rows.size());
            for (const auto& row : emit_rows) {
                emit(sblr::Opcode::BEGIN_LIST);
                emitUVarint(row.size());
                for (const auto& expr : row) {
                    if (emit_enabled_) {
                        bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
                    }
                }
                emit(sblr::Opcode::END_LIST);
            }
        }
        emit(sblr::Opcode::END_LIST);
    }

    if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_DUPLICATE, "Expected DUPLICATE");
        consumeKeyword(TokenType::KW_KEY, "Expected KEY");
        consumeKeyword(TokenType::KW_UPDATE, "Expected UPDATE");

        bool saved_on_duplicate = in_on_duplicate_update_;
        in_on_duplicate_update_ = true;
        do {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            on_duplicate_assignments.push_back({col, captureExpressionBytecode()});
        } while (match(TokenType::COMMA));
        in_on_duplicate_update_ = saved_on_duplicate;
    }

    if (ignore_duplicates && !on_duplicate_assignments.empty()) {
        warning("INSERT IGNORE is ignored when ON DUPLICATE KEY UPDATE is present");
    }

    if (ignore_duplicates && on_duplicate_assignments.empty()) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT));
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_NOTHING));
    }

    if (!on_duplicate_assignments.empty()) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT));
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_CONSTRAINT));
        emitByte(0);  // no explicit constraint target (any conflict)
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));
        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(on_duplicate_assignments.size());
        for (const auto& assign : on_duplicate_assignments) {
            emit(sblr::Opcode::ASSIGNMENT);
            emit(sblr::Opcode::COLUMN_REF);
            emitString(assign.column);
            if (emit_enabled_) {
                bytecode_.insert(bytecode_.end(), assign.expr.begin(), assign.expr.end());
            }
        }
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

    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    std::string table_path = schema.empty() ? table : schema + "/" + table;
    std::string table_alias;

    if (matchKeyword(TokenType::KW_AS)) {
        table_alias = parseIdentifier();
    } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_SET)) {
        table_alias = parseIdentifier();
    }

    if (match(TokenType::COMMA)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        do {
            parseIdentifier();
            if (match(TokenType::DOT)) {
                parseIdentifier();
            }
            if (matchKeyword(TokenType::KW_AS)) {
                parseIdentifier();
            } else if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_SET)) {
                parseIdentifier();
            }
        } while (match(TokenType::COMMA));
        emit_enabled_ = prev_emit;
    }

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);
    emitString(table_path);
    emitString(table_alias);

    // SET clause
    consumeKeyword(TokenType::KW_SET, "Expected SET");

    struct Assignment {
        std::string column;
        std::vector<uint8_t> expr;
    };
    std::vector<Assignment> assignments;

    do {
        if (match(TokenType::LEFT_PAREN)) {
            std::vector<std::string> cols;
            do {
                cols.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
            consume(TokenType::EQUAL, "Expected =");
            consume(TokenType::LEFT_PAREN, "Expected (");

            size_t idx = 0;
            do {
                if (idx >= cols.size()) {
                    error("UPDATE assignment count doesn't match column list");
                }
                assignments.push_back({cols[idx++], captureExpressionBytecode()});
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            std::string col = parseIdentifier();
            consume(TokenType::EQUAL, "Expected =");
            assignments.push_back({col, captureExpressionBytecode()});
        }
    } while (match(TokenType::COMMA));

    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(assignments.size());
    for (const auto& assign : assignments) {
        emit(sblr::Opcode::ASSIGNMENT);
        emit(sblr::Opcode::COLUMN_REF);
        emitString(assign.column);
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), assign.expr.begin(), assign.expr.end());
        }
    }
    emit(sblr::Opcode::END_LIST);

    // WHERE clause
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseWhereClause();
    }

    // ORDER BY
    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY");
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseOrderByClause();
        emit_enabled_ = prev_emit;
    }

    // LIMIT
    if (matchKeyword(TokenType::KW_LIMIT)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseLimitClause();
        emit_enabled_ = prev_emit;
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

    struct TableRef {
        std::string path;
        std::string alias;
    };

    struct JoinDef {
        TableRef table;
        uint8_t join_type = 0;
        bool has_condition = false;
        std::vector<uint8_t> condition_bytes;
    };

    auto parse_table_name = [&]() -> std::string {
        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        resolveTableName(schema, table);
        return schema.empty() ? table : schema + "/" + table;
    };

    auto parse_table_ref = [&]() -> TableRef {
        TableRef ref;
        ref.path = parse_table_name();

        if (matchKeyword(TokenType::KW_AS)) {
            ref.alias = parseIdentifier();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            if (!check(TokenType::KW_WHERE) && !check(TokenType::KW_JOIN) &&
                !check(TokenType::KW_LEFT) && !check(TokenType::KW_RIGHT) &&
                !check(TokenType::KW_INNER) && !check(TokenType::KW_CROSS) &&
                !check(TokenType::KW_USING) && !check(TokenType::KW_ORDER) &&
                !check(TokenType::KW_LIMIT))
            {
                ref.alias = parseIdentifier();
            }
        }
        return ref;
    };

    auto emit_table_ref = [&](const TableRef& ref) {
        emit(sblr::Opcode::TABLE_REF);
        emitByte(0);
        emitString(ref.path);
        emitString(ref.alias);
    };

    std::vector<TableRef> targets;
    bool explicit_targets = !(check(TokenType::KW_FROM) || check(TokenType::KW_USING));

    if (explicit_targets) {
        do {
            TableRef ref;
            ref.path = parse_table_name();
            targets.push_back(std::move(ref));
        } while (match(TokenType::COMMA));
    }

    bool using_clause = false;
    if (matchKeyword(TokenType::KW_FROM)) {
        using_clause = false;
    } else if (matchKeyword(TokenType::KW_USING)) {
        using_clause = true;
    } else {
        error("Expected FROM or USING");
        synchronize();
        return;
    }

    TableRef base = parse_table_ref();
    std::vector<JoinDef> join_defs;

    if (targets.empty()) {
        std::vector<TableRef> from_targets;
        from_targets.push_back({base.path, ""});
        bool using_reparse = false;
        if (match(TokenType::COMMA)) {
            do {
                TableRef ref;
                ref.path = parse_table_name();
                from_targets.push_back(std::move(ref));
            } while (match(TokenType::COMMA));
            if (!matchKeyword(TokenType::KW_USING)) {
                error("DELETE FROM multiple tables requires USING");
                synchronize();
                return;
            }
            using_clause = true;
            using_reparse = true;
        } else if (matchKeyword(TokenType::KW_USING)) {
            using_clause = true;
            using_reparse = true;
        }

        if (using_clause) {
            targets = from_targets;
            if (using_reparse) {
                base = parse_table_ref();
            }
        } else {
            targets = from_targets;
        }
    }

    if (!explicit_targets && !targets.empty() && !using_clause) {
        bool base_in_targets = false;
        for (const auto& target : targets) {
            if (target.path == base.path) {
                base_in_targets = true;
                break;
            }
        }
        if (!base_in_targets) {
            error("DELETE target list must include the base table");
            synchronize();
            return;
        }
    }

    auto capture_join_condition_from_using = [&](const std::vector<std::string>& cols) {
        std::vector<uint8_t> saved_bytes;
        saved_bytes.swap(bytecode_);
        bool prev_emit = emit_enabled_;
        emit_enabled_ = true;
        bytecode_.clear();
        for (size_t i = 0; i < cols.size(); ++i) {
            emit(sblr::Opcode::COLUMN_REF);
            emitString(cols[i]);
            emit(sblr::Opcode::COLUMN_REF);
            emitString(cols[i]);
            emit(sblr::Opcode::EXPR_EQ);
            if (i > 0) {
                emit(sblr::Opcode::EXPR_AND);
            }
        }
        std::vector<uint8_t> expr_bytes;
        expr_bytes.swap(bytecode_);
        bytecode_.swap(saved_bytes);
        emit_enabled_ = prev_emit;
        return expr_bytes;
    };

    auto capture_expr = [&]() {
        std::vector<uint8_t> saved;
        saved.swap(bytecode_);
        bool prev_emit = emit_enabled_;
        emit_enabled_ = true;
        bytecode_.clear();
        parseExpression();
        std::vector<uint8_t> expr;
        expr.swap(bytecode_);
        bytecode_.swap(saved);
        emit_enabled_ = prev_emit;
        return expr;
    };

    while (true) {
        uint8_t join_type = 0;  // 0=INNER, 1=LEFT, 2=RIGHT, 3=FULL, 4=CROSS
        bool saw_join = false;

        if (match(TokenType::COMMA)) {
            join_type = 4;
            saw_join = true;
        } else if (matchKeyword(TokenType::KW_LEFT)) {
            join_type = 1;
            saw_join = true;
            matchKeyword(TokenType::KW_OUTER);
            consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
        } else if (matchKeyword(TokenType::KW_RIGHT)) {
            join_type = 2;
            saw_join = true;
            matchKeyword(TokenType::KW_OUTER);
            consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
        } else if (matchKeyword(TokenType::KW_CROSS)) {
            join_type = 4;
            saw_join = true;
            consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
        } else if (matchKeyword(TokenType::KW_INNER)) {
            join_type = 0;
            saw_join = true;
            consumeKeyword(TokenType::KW_JOIN, "Expected JOIN");
        } else if (matchKeyword(TokenType::KW_JOIN)) {
            join_type = 0;
            saw_join = true;
        } else if (matchKeyword(TokenType::KW_STRAIGHT_JOIN)) {
            join_type = 0;
            saw_join = true;
        }

        if (!saw_join) {
            break;
        }

        TableRef join_table = parse_table_ref();
        JoinDef def;
        def.table = join_table;
        def.join_type = join_type;

        if (matchKeyword(TokenType::KW_ON)) {
            def.has_condition = true;
            def.condition_bytes = capture_expr();
        } else if (matchKeyword(TokenType::KW_USING)) {
            consume(TokenType::LEFT_PAREN, "Expected ( after USING");
            std::vector<std::string> cols;
            do {
                cols.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ) after USING columns");
            def.has_condition = true;
            def.condition_bytes = capture_join_condition_from_using(cols);
        }

        join_defs.push_back(std::move(def));
    }

    std::vector<uint8_t> where_expr;
    bool has_where = false;
    if (matchKeyword(TokenType::KW_WHERE)) {
        has_where = true;
        where_expr = capture_expr();
    }

    if (matchKeyword(TokenType::KW_ORDER)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY");
        bool prev = emit_enabled_;
        emit_enabled_ = false;
        parseOrderByClause();
        emit_enabled_ = prev;
    }

    if (matchKeyword(TokenType::KW_LIMIT)) {
        bool prev = emit_enabled_;
        emit_enabled_ = false;
        parseLimitClause();
        emit_enabled_ = prev;
    }

    auto build_join_list = [&](const TableRef& target,
                               std::vector<JoinDef>& out_joins,
                               std::vector<std::vector<uint8_t>>& extra_conditions) {
        out_joins.clear();
        extra_conditions.clear();

        bool target_is_base = (target.path == base.path);
        if (!target_is_base && !join_defs.empty()) {
            JoinDef base_def;
            base_def.table = base;
            base_def.join_type = 4;
            out_joins.push_back(std::move(base_def));
        }

        for (const auto& def : join_defs) {
            if (def.table.path == target.path) {
                if (def.has_condition) {
                    extra_conditions.push_back(def.condition_bytes);
                }
                continue;
            }
            out_joins.push_back(def);
        }
    };

    auto build_where = [&](const std::vector<uint8_t>& base_where,
                           const std::vector<std::vector<uint8_t>>& extra) {
        std::vector<uint8_t> combined;
        bool has_any = false;
        if (!base_where.empty()) {
            combined = base_where;
            has_any = true;
        }
        for (const auto& cond : extra) {
            if (!has_any) {
                combined = cond;
                has_any = true;
            } else {
                combined.insert(combined.end(), cond.begin(), cond.end());
                combined.push_back(static_cast<uint8_t>(sblr::Opcode::EXPR_AND));
            }
        }
        return combined;
    };

    for (const auto& target : targets) {
        emit(sblr::Opcode::DELETE);
        emit_table_ref(target);

        std::vector<JoinDef> join_list;
        std::vector<std::vector<uint8_t>> extra_conditions;
        build_join_list(target, join_list, extra_conditions);

        if (!join_list.empty()) {
            emit(sblr::Opcode::BEGIN_LIST);
            emitUVarint(0);
            emit(sblr::Opcode::END_LIST);

            for (const auto& def : join_list) {
                emit(sblr::Opcode::JOIN_TYPE);
                emitByte(def.join_type);
                emit_table_ref(def.table);
                if (def.has_condition) {
                    emit(sblr::Opcode::JOIN_CONDITION);
                    bytecode_.insert(bytecode_.end(),
                                     def.condition_bytes.begin(),
                                     def.condition_bytes.end());
                } else if (def.join_type != 4) {
                    emit(sblr::Opcode::LITERAL_NULL);
                }
            }
        }

        std::vector<uint8_t> combined_where =
            build_where(where_expr, extra_conditions);
        if (!combined_where.empty()) {
            emit(sblr::Opcode::WHERE_CLAUSE);
            bytecode_.insert(bytecode_.end(),
                             combined_where.begin(),
                             combined_where.end());
        }
    }
}

// ============================================================================
// REPLACE Statement (MySQL-specific)
// ============================================================================

void Parser::parseReplaceStmt() {
    consume(TokenType::KW_REPLACE, "Expected REPLACE");

    matchKeyword(TokenType::KW_LOW_PRIORITY);
    matchKeyword(TokenType::KW_DELAYED);

    consumeKeyword(TokenType::KW_INTO, "Expected INTO");

    emit(sblr::Opcode::INSERT);

    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    std::string table_path = schema.empty() ? table : schema + "/" + table;
    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);  // name-based reference
    emitString(table_path);
    emitString("");

    std::vector<std::string> columns;
    bool has_column_list = false;
    if (match(TokenType::LEFT_PAREN)) {
        has_column_list = true;
        do {
            columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    struct InsertValue {
        bool is_default = false;
        std::vector<uint8_t> expr;
    };
    std::vector<std::vector<InsertValue>> rows;
    bool default_values_only = false;
    bool has_select = false;
    std::vector<uint8_t> select_bytecode;

    if (matchKeyword(TokenType::KW_DEFAULT)) {
        matchKeyword(TokenType::KW_VALUES);
        default_values_only = true;
    } else if (matchKeyword(TokenType::KW_VALUES) || matchKeyword(TokenType::KW_VALUE)) {
        do {
            consume(TokenType::LEFT_PAREN, "Expected (");
            std::vector<InsertValue> row;
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    InsertValue val;
                    if (matchKeyword(TokenType::KW_DEFAULT)) {
                        val.is_default = true;
                    } else {
                        val.expr = captureExpressionBytecode();
                    }
                    row.push_back(std::move(val));
                } while (match(TokenType::COMMA));
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
            rows.push_back(std::move(row));
        } while (match(TokenType::COMMA));
    } else if (check(TokenType::KW_SELECT)) {
        auto capture_select = [&]() {
            std::vector<uint8_t> saved;
            saved.swap(bytecode_);
            bool saved_emit = emit_enabled_;
            emit_enabled_ = true;
            bytecode_.clear();
            parseSelectStmt();
            std::vector<uint8_t> stmt;
            stmt.swap(bytecode_);
            bytecode_.swap(saved);
            emit_enabled_ = saved_emit;
            return stmt;
        };
        has_select = true;
        select_bytecode = capture_select();
    }

    if (!has_column_list) {
        auto split_components = [](const std::string& path) {
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

        if (db_ && !rows.empty()) {
            core::ObjectPath path;
            path.components = split_components(schema);
            path.components.push_back(table);
            path.type = schema.empty() ? core::PathType::UNQUALIFIED : core::PathType::ABSOLUTE;

            core::CatalogManager::ResolveOptions opts;
            opts.required_privilege =
                static_cast<uint32_t>(core::CatalogManager::Privilege::INSERT);
            core::CatalogManager::ObjectType resolved_type;
            core::ID table_id;
            core::ErrorContext ctx;
            if (db_->catalog_manager()->resolveObjectPath(path,
                                                         core::CatalogManager::ObjectType::TABLE,
                                                         opts, table_id, resolved_type, &ctx) == core::Status::OK) {
                std::vector<core::CatalogManager::ColumnInfo> cols;
                if (db_->catalog_manager()->getColumns(table_id, cols, &ctx,
                                                       opts.required_privilege) == core::Status::OK) {
                    for (const auto& col : cols) {
                        columns.push_back(col.column_name);
                    }
                }
            }
        }
    }

    std::vector<std::string> emit_columns;
    std::vector<std::vector<std::vector<uint8_t>>> emit_rows;

    bool has_default = false;
    for (const auto& row : rows) {
        for (const auto& val : row) {
            if (val.is_default) {
                has_default = true;
                break;
            }
        }
    }

    if (rows.size() > 1 && has_default) {
        error("DEFAULT values in multi-row REPLACE are not supported yet");
    }

    if (!rows.empty()) {
        if (has_default && columns.empty()) {
            error("DEFAULT values require a resolved column list");
        }

        if (has_default) {
            const auto& row = rows.front();
            std::vector<std::vector<uint8_t>> row_exprs;
            for (size_t i = 0; i < row.size() && i < columns.size(); ++i) {
                if (row[i].is_default) {
                    continue;
                }
                emit_columns.push_back(columns[i]);
                row_exprs.push_back(row[i].expr);
            }
            emit_rows.push_back(std::move(row_exprs));
        } else {
            emit_columns = columns;
            for (const auto& row : rows) {
                if (!columns.empty() && row.size() != columns.size()) {
                    error("Column count doesn't match value count");
                }
                std::vector<std::vector<uint8_t>> row_exprs;
                row_exprs.reserve(row.size());
                for (const auto& val : row) {
                    row_exprs.push_back(val.expr);
                }
                emit_rows.push_back(std::move(row_exprs));
            }
        }
    }

    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(emit_columns.size());
    for (const auto& col : emit_columns) {
        emit(sblr::Opcode::COLUMN_REF);
        emitString(col);
    }
    emit(sblr::Opcode::END_LIST);

    if (has_select) {
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(), select_bytecode.begin(), select_bytecode.end());
        }
    } else {
        emit(sblr::Opcode::BEGIN_LIST);
        if (default_values_only) {
            emitUVarint(0);
        } else {
            emitUVarint(emit_rows.size());
            for (const auto& row : emit_rows) {
                emit(sblr::Opcode::BEGIN_LIST);
                emitUVarint(row.size());
                for (const auto& expr : row) {
                    if (emit_enabled_) {
                        bytecode_.insert(bytecode_.end(), expr.begin(), expr.end());
                    }
                }
                emit(sblr::Opcode::END_LIST);
            }
        }
        emit(sblr::Opcode::END_LIST);
    }

    std::vector<std::string> update_columns;
    if (db_) {
        auto split_components = [](const std::string& path) {
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

        core::ObjectPath path;
        path.components = split_components(schema);
        path.components.push_back(table);
        path.type = schema.empty() ? core::PathType::UNQUALIFIED : core::PathType::ABSOLUTE;

        core::CatalogManager::ResolveOptions opts;
        opts.required_privilege =
            static_cast<uint32_t>(core::CatalogManager::Privilege::UPDATE);
        core::CatalogManager::ObjectType resolved_type;
        core::ID table_id;
        core::ErrorContext ctx;
        if (db_->catalog_manager()->resolveObjectPath(path,
                                                     core::CatalogManager::ObjectType::TABLE,
                                                     opts, table_id, resolved_type, &ctx) == core::Status::OK) {
            std::vector<core::CatalogManager::ColumnInfo> cols;
            if (db_->catalog_manager()->getColumns(table_id, cols, &ctx,
                                                   opts.required_privilege) == core::Status::OK) {
                update_columns.reserve(cols.size());
                for (const auto& col : cols) {
                    update_columns.push_back(col.column_name);
                }
            }
        }
    }

    if (update_columns.empty()) {
        update_columns = columns;
    }
    if (update_columns.empty()) {
        update_columns = emit_columns;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT));
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ON_CONFLICT_DO_UPDATE));
    emit(sblr::Opcode::BEGIN_LIST);
    emitUVarint(update_columns.size());
    for (const auto& col : update_columns) {
        emit(sblr::Opcode::ASSIGNMENT);
        emit(sblr::Opcode::COLUMN_REF);
        emitString(col);
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_INSERTED_COLUMN_REF));
        emitString(col);
    }
    emit(sblr::Opcode::END_LIST);
}

// ============================================================================
// DDL Statements - Stubs (to be implemented in mysql_parser_ddl.cpp)
// ============================================================================

void Parser::parseCreateStmt() {
    advance();  // Consume CREATE
    if (matchIdentifierKeyword("DOMAIN")) {
        error("MySQL does not support CREATE DOMAIN. Use base types or the ScratchBird native dialect.");
        synchronize();
        return;
    }
    if (matchKeyword(TokenType::KW_DATABASE) || matchKeyword(TokenType::KW_SCHEMA)) {
        parseCreateDatabase();
        return;
    }
    if (matchIdentifierKeyword("TABLESPACE")) {
        parseCreateTablespace();
        return;
    }
    if (matchKeyword(TokenType::KW_USER)) {
        parseCreateUser();
        return;
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        parseCreateRole();
        return;
    }
    if (matchKeyword(TokenType::KW_OR)) {
        consumeKeyword(TokenType::KW_REPLACE, "Expected REPLACE after OR");
        if (check(TokenType::KW_VIEW)) {
            pending_or_replace_ = true;
            parseCreateView();
            return;
        }
        error("CREATE OR REPLACE supports VIEW in MySQL parser");
        pending_or_replace_ = false;
        synchronize();
        return;
    }
    if (check(TokenType::KW_VIEW)) {
        parseCreateView();
        return;
    }
    if (check(TokenType::KW_INDEX) || check(TokenType::KW_KEY) ||
        check(TokenType::KW_UNIQUE) || check(TokenType::KW_FULLTEXT) ||
        check(TokenType::KW_SPATIAL)) {
        parseCreateIndex();
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
    if (matchIdentifierKeyword("DOMAIN")) {
        error("MySQL does not support ALTER DOMAIN. Use base types or the ScratchBird native dialect.");
        synchronize();
        return;
    }
    if (matchKeyword(TokenType::KW_DATABASE) || matchKeyword(TokenType::KW_SCHEMA)) {
        std::string db_name = parseIdentifier();
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO after RENAME");
            parseIdentifier();
            error("MySQL does not support ALTER DATABASE RENAME");
            synchronize();
            return;
        }
        std::vector<std::pair<std::string, std::string>> options;
        auto parse_option_value = [&]() -> std::string {
            if (check(TokenType::STRING_LITERAL)) {
                auto view = lexer_.stringPool().get(current_token_.value.string_id);
                std::string value(view.data(), view.size());
                advance();
                return value;
            }
            return parseIdentifier();
        };

        while (true) {
            if (matchKeyword(TokenType::KW_DEFAULT)) {
                continue;
            } else if (matchKeyword(TokenType::KW_CHARACTER)) {
                consumeKeyword(TokenType::KW_SET, "Expected SET");
                match(TokenType::EQUAL);
                std::string value = parse_option_value();
                options.emplace_back("character_set", value);
            } else if (matchKeyword(TokenType::KW_CHARSET)) {
                match(TokenType::EQUAL);
                std::string value = parse_option_value();
                options.emplace_back("character_set", value);
            } else if (matchKeyword(TokenType::KW_COLLATE)) {
                match(TokenType::EQUAL);
                std::string value = parse_option_value();
                options.emplace_back("collation", value);
            } else {
                break;
            }
        }

        if (options.empty()) {
            error("ALTER DATABASE requires CHARACTER SET or COLLATE");
            synchronize();
            return;
        }

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DATABASE));
        emitByte(static_cast<uint8_t>(sblr::AlterDatabaseAction::SET_OPTIONS));

        std::string db_path = buildEmulatedServerRoot(default_schema_);
        if (!db_path.empty()) {
            db_path += ".databases.";
        }
        db_path += db_name;
        emitString(db_path);

        emitU32(static_cast<uint32_t>(options.size()));
        for (const auto& opt : options) {
            emitString(opt.first);
            emitString(opt.second);
        }
        return;
    }
    if (matchIdentifierKeyword("TABLESPACE")) {
        parseAlterTablespace();
        return;
    }
    if (matchKeyword(TokenType::KW_USER)) {
        parseAlterUser();
        return;
    }
    if (matchKeyword(TokenType::KW_ROLE) || matchIdentifierKeyword("ROLE")) {
        parseAlterRole();
        return;
    }
    if (matchKeyword(TokenType::KW_VIEW)) {
        parseAlterView();
        return;
    }
    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        parseAlterProcedure();
        return;
    }
    if (matchKeyword(TokenType::KW_FUNCTION)) {
        parseAlterFunction();
        return;
    }
    if (matchKeyword(TokenType::KW_TRIGGER)) {
        error("MySQL does not support ALTER TRIGGER; use DROP TRIGGER and CREATE TRIGGER");
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
    std::string resolved_schema = schema;
    std::string resolved_table = table;
    resolveTableName(resolved_schema, resolved_table);
    std::string table_path = resolved_schema.empty() ? resolved_table : resolved_schema + "/" + resolved_table;

    auto reject_column_extras = [&](const ColumnDef& col, const char* context,
                                    bool allow_not_null, bool allow_charset_collate) -> bool {
        if (col.has_default || col.default_is_expr || col.default_is_null) {
            error(std::string(context) + " does not support DEFAULT yet");
            return false;
        }
        if (col.auto_increment) {
            error(std::string(context) + " does not support AUTO_INCREMENT");
            return false;
        }
        if (col.primary_key || col.unique) {
            error(std::string(context) + " does not support PRIMARY/UNIQUE");
            return false;
        }
        if (col.is_generated) {
            error(std::string(context) + " does not support GENERATED columns");
            return false;
        }
        if (!col.comment.empty()) {
            error(std::string(context) + " does not support COMMENT");
            return false;
        }
        if (!allow_charset_collate && (!col.type.charset.empty() || !col.type.collation.empty())) {
            error(std::string(context) + " does not support CHARSET/COLLATE yet");
            return false;
        }
        if (!allow_not_null && !col.type.nullable) {
            error(std::string(context) + " does not support NOT NULL");
            return false;
        }
        return true;
    };

    auto emit_add_column = [&](const ColumnDef& col) {
        emit(sblr::Opcode::ALTER_TABLE);
        emitString(table_path);
        emitByte(0);  // ADD_COLUMN
        emitString(col.name);
        core::DataType dtype = mysqlTypeToCoreDataType(col.type);
        emitU16(static_cast<uint16_t>(dtype));
        uint32_t precision = 0;
        uint32_t scale = 0;
        resolveMySQLTypeModifiers(col.type, precision, scale);
        emitU32(precision);
        emitU32(scale);
        emitByte(col.type.nullable ? 1 : 0);
        if (!col.type.charset.empty()) {
            emit(sblr::Opcode::COLUMN_CHARSET);
            emitString(col.type.charset);
        }
        if (!col.type.collation.empty()) {
            emit(sblr::Opcode::COLUMN_COLLATE);
            emitString(col.type.collation);
        }
    };

    auto emit_alter_column_type = [&](const std::string& col_name, const MySQLDataType& type) {
        emit(sblr::Opcode::ALTER_TABLE);
        emitString(table_path);
        emitByte(2);  // ALTER_COLUMN_TYPE
        emitString(col_name);
        core::DataType dtype = mysqlTypeToCoreDataType(type);
        emitU16(static_cast<uint16_t>(dtype));
        uint32_t precision = 0;
        uint32_t scale = 0;
        resolveMySQLTypeModifiers(type, precision, scale);
        emitU32(precision);
        emitU32(scale);
    };

    auto parse_index_algorithm = [&](uint8_t& index_type) {
        if (!matchKeyword(TokenType::KW_USING)) {
            return;
        }
        std::string algorithm = parseIdentifier();
        std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        if (algorithm == "BTREE") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
        } else if (algorithm == "HASH") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
        } else if (algorithm == "RTREE") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
        } else {
            error("MySQL index type must be BTREE, HASH, or RTREE");
        }
    };

    auto parse_index_columns = [&]() {
        std::vector<std::string> columns;
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            columns.push_back(parseIdentifier());
            if (match(TokenType::LEFT_PAREN)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    advance();
                }
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
            matchKeyword(TokenType::KW_ASC);
            matchKeyword(TokenType::KW_DESC);
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return columns;
    };

    auto emit_alter_add_index = [&](bool is_unique, bool is_fulltext, bool is_spatial) {
        std::string index_name;
        if (check(TokenType::IDENTIFIER)) {
            index_name = parseIdentifier();
        }

        uint8_t index_type = 0xFF;
        parse_index_algorithm(index_type);

        auto columns = parse_index_columns();

        parse_index_algorithm(index_type);

        if (is_fulltext) {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::FULLTEXT);
        } else if (is_spatial) {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
        }

        if (index_name.empty() && !columns.empty()) {
            index_name = columns.front();
        }

        emit(sblr::Opcode::CREATE_INDEX);
        emitString(index_name);
        emitString(table_path);
        emitByte(is_unique ? 1 : 0);
        emitU32(static_cast<uint32_t>(columns.size()));
        for (const auto& col : columns) {
            emitString(col);
        }
        emitU32(0);
        emitString("");
        emitByte(index_type);
        emitByte(0);
        emitByte(0);
    };

    auto parse_column_list = [&]() {
        std::vector<std::string> columns;
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return columns;
    };

    auto emit_add_constraint = [&](uint8_t constraint_type,
                                   const std::string& constraint_name,
                                   const std::vector<std::string>& columns,
                                   const std::string& parent_table,
                                   const std::vector<std::string>& parent_columns,
                                   const std::string& on_delete,
                                   const std::string& on_update,
                                   const std::vector<uint8_t>& check_expr) {
        emit(sblr::Opcode::ALTER_TABLE);
        emitString(table_path);
        emitByte(3); // ADD_CONSTRAINT
        emitByte(constraint_type);
        emitString(constraint_name);
        if (constraint_type == 0 || constraint_type == 1) {
            emitUVarint(columns.size());
            for (const auto& col : columns) {
                emitString(col);
            }
            return;
        }
        if (constraint_type == 2) {
            emitUVarint(columns.size());
            for (const auto& col : columns) {
                emitString(col);
            }
            emitString(parent_table);
            emitUVarint(parent_columns.size());
            for (const auto& col : parent_columns) {
                emitString(col);
            }
            emitString(on_delete);
            emitString(on_update);
            emitByte(0); // deferrable flags
            return;
        }
        if (constraint_type == 3) {
            emitU32(static_cast<uint32_t>(check_expr.size()));
            bytecode_.insert(bytecode_.end(), check_expr.begin(), check_expr.end());
        }
    };

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
        if (matchKeyword(TokenType::KW_COLUMN)) {
            std::string old_name = parseIdentifier();
            if (matchKeyword(TokenType::KW_TO)) {
                std::string new_name = parseIdentifier();
                emit(sblr::Opcode::ALTER_TABLE);
                emitString(table_path);
                emitByte(5);  // RENAME_COLUMN
                emitString(old_name);
                emitString(new_name);
                return;
            }
            error("Expected TO after RENAME COLUMN");
            synchronize();
            return;
        }
        error("Expected TO or COLUMN after RENAME");
        synchronize();
        return;
    }

    if (matchKeyword(TokenType::KW_ADD)) {
        bool is_unique = false;
        bool is_fulltext = false;
        bool is_spatial = false;
        std::string constraint_name;

        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            constraint_name = parseIdentifier();
        }

        if (matchKeyword(TokenType::KW_PRIMARY)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY after PRIMARY");
            auto columns = parse_column_list();
            emit_add_constraint(0, constraint_name, columns, "", {}, "NO ACTION", "NO ACTION", {});
            return;
        }
        if (matchKeyword(TokenType::KW_UNIQUE)) {
            matchKeyword(TokenType::KW_KEY);
            matchKeyword(TokenType::KW_INDEX);
            if (constraint_name.empty() &&
                (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)))
            {
                constraint_name = parseIdentifier();
            }
            auto columns = parse_column_list();
            emit_add_constraint(1, constraint_name, columns, "", {}, "NO ACTION", "NO ACTION", {});
            return;
        }
        if (matchKeyword(TokenType::KW_FOREIGN)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY after FOREIGN");
            if (constraint_name.empty() &&
                (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) &&
                !check(TokenType::LEFT_PAREN))
            {
                constraint_name = parseIdentifier();
            }
            auto columns = parse_column_list();
            consumeKeyword(TokenType::KW_REFERENCES, "Expected REFERENCES");
            std::string parent_schema;
            std::string parent_table = parseIdentifier();
            if (match(TokenType::DOT)) {
                parent_schema = parent_table;
                parent_table = parseIdentifier();
            }
            resolveTableName(parent_schema, parent_table);
            std::string parent_path = parent_schema.empty()
                ? parent_table
                : parent_schema + "/" + parent_table;

            std::vector<std::string> parent_columns;
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    parent_columns.push_back(parseIdentifier());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }

            std::string on_delete = "NO ACTION";
            std::string on_update = "NO ACTION";
            while (matchKeyword(TokenType::KW_ON)) {
                if (matchKeyword(TokenType::KW_DELETE)) {
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        on_delete = "CASCADE";
                    } else if (matchKeyword(TokenType::KW_SET)) {
                        if (matchKeyword(TokenType::KW_NULL)) {
                            on_delete = "SET NULL";
                        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                            on_delete = "SET DEFAULT";
                        }
                    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                        on_delete = "RESTRICT";
                    } else {
                        matchKeyword(TokenType::KW_NO);
                        consumeKeyword(TokenType::KW_ACTION, "Expected ACTION");
                        on_delete = "NO ACTION";
                    }
                } else if (matchKeyword(TokenType::KW_UPDATE)) {
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        on_update = "CASCADE";
                    } else if (matchKeyword(TokenType::KW_SET)) {
                        if (matchKeyword(TokenType::KW_NULL)) {
                            on_update = "SET NULL";
                        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                            on_update = "SET DEFAULT";
                        }
                    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                        on_update = "RESTRICT";
                    } else {
                        matchKeyword(TokenType::KW_NO);
                        consumeKeyword(TokenType::KW_ACTION, "Expected ACTION");
                        on_update = "NO ACTION";
                    }
                } else {
                    break;
                }
            }

            emit_add_constraint(2, constraint_name, columns, parent_path,
                                parent_columns, on_delete, on_update, {});
            return;
        }
        if (matchKeyword(TokenType::KW_CHECK)) {
            std::vector<uint8_t> expr;
            if (match(TokenType::LEFT_PAREN)) {
                expr = captureExpressionBytecode();
                consume(TokenType::RIGHT_PAREN, "Expected )");
            } else {
                expr = captureExpressionBytecode();
            }
            emit_add_constraint(3, constraint_name, {}, "", {}, "NO ACTION", "NO ACTION", expr);
            return;
        }

        if (matchKeyword(TokenType::KW_UNIQUE)) {
            is_unique = true;
        } else if (matchKeyword(TokenType::KW_FULLTEXT)) {
            is_fulltext = true;
        } else if (matchKeyword(TokenType::KW_SPATIAL)) {
            is_spatial = true;
        }

        if (matchKeyword(TokenType::KW_INDEX) || matchKeyword(TokenType::KW_KEY)) {
            emit_alter_add_index(is_unique, is_fulltext, is_spatial);
            return;
        }
        if (is_unique || is_fulltext || is_spatial) {
            error("Expected INDEX or KEY after ADD");
            synchronize();
            return;
        }
        matchKeyword(TokenType::KW_COLUMN);
        ColumnDef col = parseColumnDef();
        if (!reject_column_extras(col, "ALTER TABLE ADD COLUMN", true, true)) {
            synchronize();
            return;
        }
        emit_add_column(col);
        return;
    }

    if (matchKeyword(TokenType::KW_DROP)) {
        if (matchKeyword(TokenType::KW_PRIMARY)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY after PRIMARY");
            std::string base_name = table_path;
            size_t sep = base_name.find_last_of("/.");
            std::string table_name = (sep == std::string::npos)
                ? base_name
                : base_name.substr(sep + 1);
            emit(sblr::Opcode::ALTER_TABLE);
            emitString(table_path);
            emitByte(4); // DROP_CONSTRAINT
            emitString(table_name + "_pkey");
            emitByte(0);
            return;
        }
        if (matchKeyword(TokenType::KW_FOREIGN)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY after FOREIGN");
            std::string constraint_name = parseIdentifier();
            emit(sblr::Opcode::ALTER_TABLE);
            emitString(table_path);
            emitByte(4); // DROP_CONSTRAINT
            emitString(constraint_name);
            emitByte(0);
            return;
        }
        if (matchKeyword(TokenType::KW_CHECK)) {
            std::string constraint_name = parseIdentifier();
            emit(sblr::Opcode::ALTER_TABLE);
            emitString(table_path);
            emitByte(4); // DROP_CONSTRAINT
            emitString(constraint_name);
            emitByte(0);
            return;
        }
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            std::string constraint_name = parseIdentifier();
            emit(sblr::Opcode::ALTER_TABLE);
            emitString(table_path);
            emitByte(4); // DROP_CONSTRAINT
            emitString(constraint_name);
            emitByte(0);
            return;
        }
        if (matchKeyword(TokenType::KW_INDEX) || matchKeyword(TokenType::KW_KEY)) {
            bool if_exists = false;
            if (matchKeyword(TokenType::KW_IF)) {
                consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
                if_exists = true;
            }
            std::string index_name = parseIdentifier();
            emit(sblr::Opcode::DROP_INDEX);
            emitString(index_name);
            emitByte(if_exists ? 1 : 0);
            return;
        }
        matchKeyword(TokenType::KW_COLUMN);
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        std::string col_name = parseIdentifier();
        emit(sblr::Opcode::ALTER_TABLE);
        emitString(table_path);
        emitByte(1);  // DROP_COLUMN
        emitString(col_name);
        emitByte(if_exists ? 1 : 0);
        emitByte(0);  // cascade
        return;
    }

    if (matchKeyword(TokenType::KW_MODIFY)) {
        matchKeyword(TokenType::KW_COLUMN);
        ColumnDef col = parseColumnDef();
        if (!reject_column_extras(col, "ALTER TABLE MODIFY COLUMN", false, true)) {
            synchronize();
            return;
        }
        emit_alter_column_type(col.name, col.type);
        return;
    }

    if (matchKeyword(TokenType::KW_CHANGE)) {
        matchKeyword(TokenType::KW_COLUMN);
        std::string old_name = parseIdentifier();
        ColumnDef col = parseColumnDef();
        if (old_name != col.name) {
            error("ALTER TABLE CHANGE COLUMN rename is not supported yet; use RENAME COLUMN");
            synchronize();
            return;
        }
        if (!reject_column_extras(col, "ALTER TABLE CHANGE COLUMN", false, true)) {
            synchronize();
            return;
        }
        emit_alter_column_type(old_name, col.type);
        return;
    }

    if (matchKeyword(TokenType::KW_ALTER)) {
        if (matchKeyword(TokenType::KW_COLUMN)) {
            std::string col_name = parseIdentifier();
            if (matchKeyword(TokenType::KW_SET)) {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    (void)col_name;
                    error("ALTER TABLE ALTER COLUMN SET DEFAULT is not supported yet");
                    synchronize();
                    return;
                }
            } else if (matchKeyword(TokenType::KW_DROP)) {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    (void)col_name;
                    error("ALTER TABLE ALTER COLUMN DROP DEFAULT is not supported yet");
                    synchronize();
                    return;
                }
            }
            error("ALTER TABLE ALTER COLUMN is not supported yet");
            synchronize();
            return;
        }
    }

    error("ALTER TABLE supports RENAME, ADD/DROP COLUMN, MODIFY/CHANGE COLUMN, ADD/DROP CONSTRAINT, and ADD/DROP INDEX");
    synchronize();
}

void Parser::parseDropStmt() {
    advance();  // Consume DROP
    if (matchIdentifierKeyword("DOMAIN")) {
        error("MySQL does not support DROP DOMAIN. Use base types or the ScratchBird native dialect.");
        synchronize();
        return;
    }
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
            db_path += ".databases.";
        }
        db_path += db_name;
        emitString(db_path);
        return;
    }
    if (matchIdentifierKeyword("TABLESPACE")) {
        parseDropTablespace();
        return;
    }
    if (matchIdentifierKeyword("PREPARE")) {
        parseDeallocateStmt();
        return;
    }
    if (matchKeyword(TokenType::KW_USER)) {
        parseDropUser();
        return;
    }
    if (matchKeyword(TokenType::KW_ROLE)) {
        parseDropRole();
        return;
    }

    if (matchKeyword(TokenType::KW_TEMPORARY)) {
        consumeKeyword(TokenType::KW_TABLE, "Expected TABLE after DROP TEMPORARY");
    }

    if (matchKeyword(TokenType::KW_TABLE)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        resolveTableName(schema, table);

        emit(sblr::Opcode::DROP_TABLE);
        emitString(schema + "/" + table);
        emitByte(if_exists ? 0x01 : 0x00);
        return;
    }

    if (matchKeyword(TokenType::KW_VIEW)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::string schema;
        std::string view = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = view;
            view = parseIdentifier();
        }
        resolveTableName(schema, view);

        emit(sblr::Opcode::DROP_VIEW);
        emitString(schema + "/" + view);
        emitByte(if_exists ? 0x01 : 0x00);
        return;
    }

    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::string schema;
        std::string proc_name = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = proc_name;
            proc_name = parseIdentifier();
        }
        resolveTableName(schema, proc_name);

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_PROCEDURE_STMT));
        emitByte(if_exists ? 0x01 : 0x00);
        emitString(schema.empty() ? proc_name : schema + "/" + proc_name);
        return;
    }

    if (matchKeyword(TokenType::KW_FUNCTION)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::string schema;
        std::string func_name = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = func_name;
            func_name = parseIdentifier();
        }
        resolveTableName(schema, func_name);

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_FUNCTION_STMT));
        emitByte(if_exists ? 0x01 : 0x00);
        emitString(schema.empty() ? func_name : schema + "/" + func_name);
        return;
    }

    if (matchKeyword(TokenType::KW_TRIGGER)) {
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        }

        std::string trigger_name = parseIdentifier();
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_TRIGGER));
        emitString(trigger_name);
        emitString(std::string());
        return;
    }

    if (matchKeyword(TokenType::KW_INDEX) || matchKeyword(TokenType::KW_KEY)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::string index_name = parseIdentifier();
        if (matchKeyword(TokenType::KW_ON)) {
            std::string schema;
            std::string table = parseIdentifier();
            if (match(TokenType::DOT)) {
                schema = table;
                table = parseIdentifier();
            }
            resolveTableName(schema, table);
        }

        emit(sblr::Opcode::DROP_INDEX);
        emitString(index_name);
        emitByte(if_exists ? 1 : 0);
        return;
    }

    error("DROP statement not yet implemented");
    synchronize();
}

void Parser::parseTruncateStmt() {
    advance();  // Consume TRUNCATE
    matchKeyword(TokenType::KW_TABLE);

    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TRUNCATE_TABLE);
    emitString(schema + "/" + table);
    emitByte(0);
}

void Parser::parseCreateTable() {
    bool is_temp = matchKeyword(TokenType::KW_TEMPORARY);

    consumeKeyword(TokenType::KW_TABLE, "Expected TABLE");

    // IF NOT EXISTS
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        if (matchKeyword(TokenType::KW_EXISTS)) {
            if_not_exists = true;
        }
    }
    (void)if_not_exists;

    emit(sblr::Opcode::CREATE_TABLE);
    uint8_t flags = is_temp ? 0x01 : 0x00;
    emitByte(flags);

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);
    std::string table_path = schema.empty() ? table : schema + "/" + table;

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);
    emitString(table_path);
    emitString("");

    // Column definitions
    consume(TokenType::LEFT_PAREN, "Expected (");

    emit(sblr::Opcode::BEGIN_LIST);
    size_t col_count_pos = bytecode_.size();
    emitUVarint(0);

    uint32_t col_count = 0;
    auto patch_varint = [&](size_t pos, uint64_t value) {
        uint8_t buffer[10];
        size_t len = sblr::writeUVarint(buffer, value);
        if (len == 1) {
            bytecode_[pos] = buffer[0];
            return;
        }
        bytecode_.insert(bytecode_.begin() + pos + 1, len - 1, 0);
        std::copy(buffer, buffer + len, bytecode_.begin() + pos);
    };
    std::vector<ForeignKeyDef> pending_fks;
    std::vector<IndexDef> pending_indexes;
    do {
        bool emitted_entry = false;
        if (check(TokenType::KW_PRIMARY) || check(TokenType::KW_UNIQUE) ||
            check(TokenType::KW_FOREIGN) || check(TokenType::KW_KEY) ||
            check(TokenType::KW_INDEX) || check(TokenType::KW_FULLTEXT) ||
            check(TokenType::KW_SPATIAL) || check(TokenType::KW_CONSTRAINT) ||
            check(TokenType::KW_CHECK)) {
            std::string constraint_name;
            if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                constraint_name = parseIdentifier();
            }

            if (matchKeyword(TokenType::KW_PRIMARY)) {
                consumeKeyword(TokenType::KW_KEY, "Expected KEY after PRIMARY");
                IndexDef idx = parseIndexDef();
                idx.type = IndexDef::Type::PRIMARY;
                if (!constraint_name.empty() && idx.name.empty()) {
                    idx.name = constraint_name;
                }
                pending_indexes.push_back(std::move(idx));
            } else if (matchKeyword(TokenType::KW_UNIQUE)) {
                matchKeyword(TokenType::KW_KEY);
                matchKeyword(TokenType::KW_INDEX);
                IndexDef idx = parseIndexDef();
                idx.type = IndexDef::Type::UNIQUE;
                if (!constraint_name.empty() && idx.name.empty()) {
                    idx.name = constraint_name;
                }
                pending_indexes.push_back(std::move(idx));
            } else if (matchKeyword(TokenType::KW_FOREIGN)) {
                consumeKeyword(TokenType::KW_KEY, "Expected KEY after FOREIGN");
                ForeignKeyDef fk = parseForeignKeyDef();
                if (!constraint_name.empty() && fk.name.empty()) {
                    fk.name = constraint_name;
                }
                pending_fks.push_back(std::move(fk));
            } else if (matchKeyword(TokenType::KW_CHECK)) {
                consume(TokenType::LEFT_PAREN, "Expected (");
                bool prev_emit = emit_enabled_;
                emit_enabled_ = false;
                parseExpression();
                emit_enabled_ = prev_emit;
                consume(TokenType::RIGHT_PAREN, "Expected )");
            } else if (check(TokenType::KW_FULLTEXT) || check(TokenType::KW_SPATIAL)) {
                bool is_spatial = matchKeyword(TokenType::KW_SPATIAL);
                bool is_fulltext = false;
                if (!is_spatial) {
                    is_fulltext = matchKeyword(TokenType::KW_FULLTEXT);
                }
                matchKeyword(TokenType::KW_KEY);
                matchKeyword(TokenType::KW_INDEX);
                IndexDef idx = parseIndexDef();
                idx.type = is_spatial ? IndexDef::Type::SPATIAL : IndexDef::Type::FULLTEXT;
                if (!constraint_name.empty() && idx.name.empty()) {
                    idx.name = constraint_name;
                }
                pending_indexes.push_back(std::move(idx));
            } else if (matchKeyword(TokenType::KW_KEY) || matchKeyword(TokenType::KW_INDEX)) {
                IndexDef idx = parseIndexDef();
                idx.type = IndexDef::Type::NORMAL;
                if (!constraint_name.empty() && idx.name.empty()) {
                    idx.name = constraint_name;
                }
                pending_indexes.push_back(std::move(idx));
            }
        } else {
            ColumnDef col = parseColumnDef();
            if (col.primary_key) {
                IndexDef idx;
                idx.type = IndexDef::Type::PRIMARY;
                idx.columns.push_back(col.name);
                pending_indexes.push_back(std::move(idx));
            } else if (col.unique) {
                IndexDef idx;
                idx.type = IndexDef::Type::UNIQUE;
                idx.columns.push_back(col.name);
                pending_indexes.push_back(std::move(idx));
            }

            emit(sblr::Opcode::COLUMN_DEF);
            emit(sblr::Opcode::COLUMN_REF);
            emitString("");
            emitString(col.name);

            emitTypeDefinition(col.type);
            if (!col.type.charset.empty()) {
                emit(sblr::Opcode::COLUMN_CHARSET);
                emitString(col.type.charset);
            }
            if (!col.type.collation.empty()) {
                emit(sblr::Opcode::COLUMN_COLLATE);
                emitString(col.type.collation);
            }

            if (!col.type.nullable) {
                emit(sblr::Opcode::NOT_NULL);
            }
            if (col.auto_increment) {
                emit(sblr::Opcode::IDENTITY_COLUMN);
                emitByte(0);
            }
            if (col.has_default) {
                emit(sblr::Opcode::DEFAULT_VALUE);
                size_t len_pos = bytecode_.size();
                emitU32(0);
                size_t expr_start = bytecode_.size();
                if (col.default_is_expr && !col.default_expr_bytecode.empty()) {
                    if (emit_enabled_) {
                        bytecode_.insert(bytecode_.end(),
                                         col.default_expr_bytecode.begin(),
                                         col.default_expr_bytecode.end());
                    }
                } else {
                    switch (col.default_literal_type) {
                        case ColumnDef::DefaultLiteralType::NULL_VALUE:
                            emit(sblr::Opcode::LITERAL_NULL);
                            break;
                        case ColumnDef::DefaultLiteralType::STRING:
                            emit(sblr::Opcode::LITERAL_STRING);
                            emitString(col.default_value);
                            break;
                        case ColumnDef::DefaultLiteralType::INT:
                            emit(sblr::Opcode::LITERAL_INT64);
                            emitI64(col.default_int_value);
                            break;
                        case ColumnDef::DefaultLiteralType::FLOAT:
                            emit(sblr::Opcode::LITERAL_DOUBLE);
                            emitF64(col.default_float_value);
                            break;
                        case ColumnDef::DefaultLiteralType::NONE:
                            emit(sblr::Opcode::LITERAL_NULL);
                            break;
                    }
                }
                uint32_t expr_len = static_cast<uint32_t>(bytecode_.size() - expr_start);
                sblr::writeInt32(&bytecode_[len_pos], expr_len);
            }

            if (col.is_generated && !col.generated_expr_bytecode.empty()) {
                emit(sblr::Opcode::GENERATED_COLUMN);
                emitByte(col.generated_stored ? 1 : 2);
                emitU32(static_cast<uint32_t>(col.generated_expr_bytecode.size()));
                if (emit_enabled_) {
                    bytecode_.insert(bytecode_.end(),
                                     col.generated_expr_bytecode.begin(),
                                     col.generated_expr_bytecode.end());
                }
            }

            emitted_entry = true;
        }

        if (emitted_entry) {
            col_count++;
        }
    } while (match(TokenType::COMMA));

    patch_varint(col_count_pos, col_count);
    emit(sblr::Opcode::END_LIST);

    consume(TokenType::RIGHT_PAREN, "Expected )");

    // Table options (ENGINE, CHARSET, etc.)
    std::vector<std::pair<std::string, std::string>> table_options;
    bool has_partition_clause = false;
    std::string partition_strategy;
    std::vector<std::string> partition_columns;
    auto to_upper = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return value;
    };
    auto parse_table_option_value = [&]() -> std::optional<std::string> {
        match(TokenType::EQUAL);
        if (check(TokenType::STRING_LITERAL) || check(TokenType::INTEGER_LITERAL) ||
            check(TokenType::FLOAT_LITERAL)) {
            std::string value;
            if (check(TokenType::STRING_LITERAL)) {
                auto view = lexer_.stringPool().get(current_token_.value.string_id);
                value.assign(view.data(), view.size());
            } else if (check(TokenType::INTEGER_LITERAL)) {
                value = std::to_string(current_token_.value.int_value);
            } else {
                std::ostringstream out;
                out << current_token_.value.float_value;
                value = out.str();
            }
            advance();
            return value;
        }
        if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            return parseIdentifier();
        }
        if (isNonReservedKeyword(current_token_.type)) {
            std::string name = tokenToString(current_token_.type);
            advance();
            return name;
        }
        if (check(TokenType::KW_DEFAULT)) {
            advance();
            return std::string("DEFAULT");
        }
        return std::nullopt;
    };
    auto require_table_option_value = [&](std::string_view option) -> std::optional<std::string> {
        auto value = parse_table_option_value();
        if (!value) {
            error("Expected value for table option " + std::string(option));
            synchronize();
            return std::nullopt;
        }
        return value;
    };
    auto parse_option_identifier = [&]() -> std::string {
        if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            return parseIdentifier();
        }
        if (isNonReservedKeyword(current_token_.type)) {
            std::string name = tokenToString(current_token_.type);
            advance();
            return name;
        }
        return "";
    };
    const std::unordered_set<std::string> allowed_options = {
        "AUTOEXTEND_SIZE",
        "AVG_ROW_LENGTH",
        "CHECKSUM",
        "COMPRESSION",
        "CONNECTION",
        "DATA",
        "DELAY_KEY_WRITE",
        "ENCRYPTION",
        "ENGINE_ATTRIBUTE",
        "INDEX",
        "INSERT_METHOD",
        "KEY_BLOCK_SIZE",
        "MAX_ROWS",
        "MIN_ROWS",
        "PACK_KEYS",
        "PASSWORD",
        "ROW_FORMAT",
        "SECONDARY_ENGINE_ATTRIBUTE",
        "STATS_AUTO_RECALC",
        "STATS_PERSISTENT",
        "STATS_SAMPLE_PAGES",
        "TABLESPACE",
        "UNION"
    };
    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        if (match(TokenType::COMMA)) {
            continue;
        }
        if (matchKeyword(TokenType::KW_WITH)) {
            error("MySQL does not support WITH table options");
            synchronize();
            return;
        }
        if (matchKeyword(TokenType::KW_PARTITION)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY after PARTITION");
            if (matchKeyword(TokenType::KW_RANGE)) {
                partition_strategy = "RANGE";
            } else if (matchIdentifierKeyword("LIST")) {
                partition_strategy = "LIST";
            } else if (matchKeyword(TokenType::KW_HASH)) {
                partition_strategy = "HASH";
            } else if (matchKeyword(TokenType::KW_KEY)) {
                partition_strategy = "KEY";
            } else {
                error("Expected RANGE, LIST, HASH, or KEY after PARTITION BY");
                synchronize();
                return;
            }

            consume(TokenType::LEFT_PAREN, "Expected ( after PARTITION BY");
            do {
                std::string col = parse_option_identifier();
                if (col.empty()) {
                    error("Expected partition column name");
                    synchronize();
                    return;
                }
                partition_columns.push_back(col);
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ) after partition columns");

            if (matchKeyword(TokenType::KW_PARTITIONS)) {
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected partition count after PARTITIONS");
                    synchronize();
                    return;
                }
                advance();
            }

            if (match(TokenType::LEFT_PAREN)) {
                int depth = 1;
                while (depth > 0 && !check(TokenType::END_OF_FILE)) {
                    if (match(TokenType::LEFT_PAREN)) {
                        depth++;
                    } else if (match(TokenType::RIGHT_PAREN)) {
                        depth--;
                    } else {
                        advance();
                    }
                }
            }

            has_partition_clause = true;
            continue;
        }
        if (matchKeyword(TokenType::KW_ENGINE)) {
            auto value = require_table_option_value("ENGINE");
            if (!value) {
                return;
            }
            table_options.emplace_back("ENGINE", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_AUTO_INCREMENT)) {
            auto value = require_table_option_value("AUTO_INCREMENT");
            if (!value) {
                return;
            }
            table_options.emplace_back("AUTO_INCREMENT", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_CHARSET)) {
            auto value = require_table_option_value("CHARSET");
            if (!value) {
                return;
            }
            table_options.emplace_back("CHARSET", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_CHARACTER)) {
            if (!(matchKeyword(TokenType::KW_SET) || matchIdentifierKeyword("SET"))) {
                error("Expected SET after CHARACTER");
                synchronize();
                return;
            }
            auto value = require_table_option_value("CHARACTER SET");
            if (!value) {
                return;
            }
            table_options.emplace_back("CHARSET", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_COLLATE)) {
            auto value = require_table_option_value("COLLATE");
            if (!value) {
                return;
            }
            table_options.emplace_back("COLLATE", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_COMMENT)) {
            auto value = require_table_option_value("COMMENT");
            if (!value) {
                return;
            }
            table_options.emplace_back("COMMENT", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            if (matchKeyword(TokenType::KW_CHARSET)) {
                auto value = require_table_option_value("DEFAULT CHARSET");
                if (!value) {
                    return;
                }
                table_options.emplace_back("DEFAULT_CHARSET", *value);
                continue;
            }
            if (matchKeyword(TokenType::KW_CHARACTER)) {
                matchKeyword(TokenType::KW_SET) || matchIdentifierKeyword("SET");
                auto value = require_table_option_value("DEFAULT CHARACTER SET");
                if (!value) {
                    return;
                }
                table_options.emplace_back("DEFAULT_CHARSET", *value);
                continue;
            }
            if (matchKeyword(TokenType::KW_COLLATE)) {
                auto value = require_table_option_value("DEFAULT COLLATE");
                if (!value) {
                    return;
                }
                table_options.emplace_back("DEFAULT_COLLATE", *value);
                continue;
            }
            error("Unsupported DEFAULT table option");
            synchronize();
            return;
        }
        if (matchKeyword(TokenType::KW_DATA)) {
            if (!matchIdentifierKeyword("DIRECTORY")) {
                error("Expected DIRECTORY after DATA");
                synchronize();
                return;
            }
            auto value = require_table_option_value("DATA DIRECTORY");
            if (!value) {
                return;
            }
            table_options.emplace_back("DATA_DIRECTORY", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_INDEX)) {
            if (!matchIdentifierKeyword("DIRECTORY")) {
                error("Expected DIRECTORY after INDEX");
                synchronize();
                return;
            }
            auto value = require_table_option_value("INDEX DIRECTORY");
            if (!value) {
                return;
            }
            table_options.emplace_back("INDEX_DIRECTORY", *value);
            continue;
        }
        if (matchKeyword(TokenType::KW_UNION)) {
            match(TokenType::EQUAL);
            std::string union_value;
            if (match(TokenType::LEFT_PAREN)) {
                std::vector<std::string> names;
                auto parse_union_name = [&]() -> std::string {
                    std::string name = parseIdentifier();
                    if (match(TokenType::DOT)) {
                        name += "." + parseIdentifier();
                    }
                    return name;
                };
                if (!check(TokenType::RIGHT_PAREN)) {
                    names.push_back(parse_union_name());
                    while (match(TokenType::COMMA)) {
                        names.push_back(parse_union_name());
                    }
                }
                consume(TokenType::RIGHT_PAREN, "Expected ) after UNION");
                for (size_t i = 0; i < names.size(); ++i) {
                    if (i) union_value += ",";
                    union_value += names[i];
                }
            } else {
                auto value = require_table_option_value("UNION");
                if (!value) {
                    return;
                }
                union_value = *value;
            }
            if (!union_value.empty()) {
                table_options.emplace_back("UNION", union_value);
            }
            continue;
        }

        std::string option_name = parse_option_identifier();
        if (option_name.empty()) {
            error("Unexpected token in table options");
            synchronize();
            return;
        }
        std::string upper = to_upper(option_name);
        if (upper == "INHERITS") {
            error("MySQL does not support INHERITS");
            synchronize();
            return;
        }
        if (allowed_options.find(upper) == allowed_options.end()) {
            error("Unsupported MySQL table option: " + option_name);
            synchronize();
            return;
        }
        auto value = require_table_option_value(option_name);
        if (!value) {
            return;
        }
        table_options.emplace_back(upper, *value);
    }

    for (const auto& fk : pending_fks) {
        if (fk.columns.size() > 255 || fk.ref_columns.size() > 255) {
            error("Foreign key has too many columns");
            continue;
        }
        emit(sblr::Opcode::TABLE_FK);
        emitByte(static_cast<uint8_t>(fk.columns.size()));
        for (const auto& col : fk.columns) {
            emitString(col);
        }
        emitString(fk.ref_table);
        emitByte(static_cast<uint8_t>(fk.ref_columns.size()));
        for (const auto& col : fk.ref_columns) {
            emitString(col);
        }
        emitString(fk.on_delete.empty() ? "NO ACTION" : fk.on_delete);
        emitString(fk.on_update.empty() ? "NO ACTION" : fk.on_update);
        emitString(fk.name);
        emitByte(0);  // Not deferrable
    }

    emitString("");
    if (!table_options.empty()) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_TABLE_OPTIONS));
        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(table_options.size());
        for (const auto& opt : table_options) {
            emitString(opt.first);
            emitString(opt.second);
        }
        emit(sblr::Opcode::END_LIST);
    }
    if (has_partition_clause) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_TABLE_PARTITIONING));
        emitString(partition_strategy);
        emit(sblr::Opcode::BEGIN_LIST);
        emitUVarint(partition_columns.size());
        for (const auto& col : partition_columns) {
            emitString(col);
        }
        emit(sblr::Opcode::END_LIST);
    }

    auto resolve_index_type = [&](const IndexDef& idx) -> uint8_t {
        if (idx.type == IndexDef::Type::FULLTEXT) {
            return static_cast<uint8_t>(core::CatalogManager::IndexType::FULLTEXT);
        }
        if (idx.type == IndexDef::Type::SPATIAL) {
            return static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
        }
        if (!idx.algorithm.empty()) {
            std::string algorithm = to_upper(idx.algorithm);
            if (algorithm == "BTREE") {
                return static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
            }
            if (algorithm == "HASH") {
                return static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
            }
            if (algorithm == "RTREE") {
                return static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
            }
            error("MySQL index type must be BTREE, HASH, or RTREE");
        }
        return static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
    };

    auto make_index_name = [&](const IndexDef& idx) -> std::string {
        if (!idx.name.empty()) {
            return idx.name;
        }
        if (idx.type == IndexDef::Type::PRIMARY) {
            return "PRIMARY";
        }
        std::string name = table;
        for (const auto& col : idx.columns) {
            name += "_" + col;
        }
        switch (idx.type) {
            case IndexDef::Type::UNIQUE:
                name += "_uniq";
                break;
            case IndexDef::Type::FULLTEXT:
                name += "_ft";
                break;
            case IndexDef::Type::SPATIAL:
                name += "_spatial";
                break;
            default:
                name += "_idx";
                break;
        }
        return name;
    };

    for (const auto& idx : pending_indexes) {
        if (idx.columns.empty()) {
            error("Index definition missing columns");
            continue;
        }

        bool is_unique = (idx.type == IndexDef::Type::PRIMARY || idx.type == IndexDef::Type::UNIQUE);
        uint8_t index_type = resolve_index_type(idx);
        std::string index_name = make_index_name(idx);

        emit(sblr::Opcode::CREATE_INDEX);
        emitString(index_name);
        emitString(table_path);
        emitByte(is_unique ? 1 : 0);
        emitU32(static_cast<uint32_t>(idx.columns.size()));
        for (const auto& col : idx.columns) {
            emitString(col);
        }
        emitU32(0);
        emitString("");
        emitByte(index_type);
        emitByte(0);
        emitByte(0);
    }
}

ColumnDef Parser::parseColumnDef() {
    ColumnDef col;
    col.name = parseIdentifier();
    col.type = parseDataType();

    auto skip_parenthesized = [&]() {
        int depth = 1;
        while (depth > 0 && !check(TokenType::END_OF_FILE)) {
            if (match(TokenType::LEFT_PAREN)) {
                depth++;
            } else if (match(TokenType::RIGHT_PAREN)) {
                depth--;
            } else {
                advance();
            }
        }
    };

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
                col.default_literal_type = ColumnDef::DefaultLiteralType::NULL_VALUE;
            } else if (check(TokenType::STRING_LITERAL) || check(TokenType::INTEGER_LITERAL) ||
                       check(TokenType::FLOAT_LITERAL)) {
                if (check(TokenType::STRING_LITERAL)) {
                    col.default_value = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                    col.default_literal_type = ColumnDef::DefaultLiteralType::STRING;
                } else if (check(TokenType::INTEGER_LITERAL)) {
                    col.default_value = std::to_string(current_token_.value.int_value);
                    col.default_int_value = current_token_.value.int_value;
                    col.default_literal_type = ColumnDef::DefaultLiteralType::INT;
                } else {
                    col.default_value = std::to_string(current_token_.value.float_value);
                    col.default_float_value = current_token_.value.float_value;
                    col.default_literal_type = ColumnDef::DefaultLiteralType::FLOAT;
                }
                advance();
            } else if (matchKeyword(TokenType::KW_TRUE)) {
                col.default_int_value = 1;
                col.default_literal_type = ColumnDef::DefaultLiteralType::INT;
            } else if (matchKeyword(TokenType::KW_FALSE)) {
                col.default_int_value = 0;
                col.default_literal_type = ColumnDef::DefaultLiteralType::INT;
            } else if (match(TokenType::LEFT_PAREN)) {
                col.default_is_expr = true;
                col.default_expr_bytecode = captureExpressionBytecode();
                consume(TokenType::RIGHT_PAREN, "Expected )");
            } else {
                col.default_is_expr = true;
                col.default_expr_bytecode = captureExpressionBytecode();
            }
        } else if (check(TokenType::KW_GENERATED) || check(TokenType::KW_AS)) {
            bool saw_generated = matchKeyword(TokenType::KW_GENERATED);
            if (!saw_generated) {
                matchKeyword(TokenType::KW_AS);
            }
            col.is_generated = true;
            if (saw_generated) {
                matchKeyword(TokenType::KW_ALWAYS);
                consumeKeyword(TokenType::KW_AS, "Expected AS");
            }
            consume(TokenType::LEFT_PAREN, "Expected (");
            col.generated_expr_bytecode = captureExpressionBytecode();
            consume(TokenType::RIGHT_PAREN, "Expected )");
            if (matchKeyword(TokenType::KW_STORED)) {
                col.generated_stored = true;
            } else if (matchKeyword(TokenType::KW_VIRTUAL)) {
                col.generated_stored = false;
            }
        } else if (matchKeyword(TokenType::KW_AUTO_INCREMENT)) {
            col.auto_increment = true;
        } else if (matchKeyword(TokenType::KW_PRIMARY)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY after PRIMARY");
            col.primary_key = true;
            col.type.nullable = false;
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
    } else if (matchKeyword(TokenType::KW_INT128)) {
        type.kind = MySQLDataType::Kind::INT128;
    } else if (matchKeyword(TokenType::KW_UINT128)) {
        type.kind = MySQLDataType::Kind::UINT128;
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
    } else if (matchKeyword(TokenType::KW_POINT)) {
        type.kind = MySQLDataType::Kind::POINT;
    } else if (matchKeyword(TokenType::KW_LINESTRING)) {
        type.kind = MySQLDataType::Kind::LINESTRING;
    } else if (matchKeyword(TokenType::KW_POLYGON)) {
        type.kind = MySQLDataType::Kind::POLYGON;
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
        case MySQLDataType::Kind::POINT:
        case MySQLDataType::Kind::LINESTRING:
        case MySQLDataType::Kind::POLYGON:
            return sblr::Opcode::TYPE_BLOB;
        default:
            return sblr::Opcode::TYPE_VARCHAR;
    }
}

void Parser::emitTypeDefinition(const MySQLDataType& type) {
    // See docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md for SBLR type encoding.
    if (type.kind == MySQLDataType::Kind::INT128 ||
        type.kind == MySQLDataType::Kind::UINT128) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(
            type.kind == MySQLDataType::Kind::INT128
                ? sblr::ExtendedOpcode::EXT_TYPE_INT128
                : sblr::ExtendedOpcode::EXT_TYPE_UINT128));
        return;
    }

    emit(typeToOpcode(type.kind));
    switch (type.kind) {
        case MySQLDataType::Kind::CHAR:
        case MySQLDataType::Kind::VARCHAR:
        case MySQLDataType::Kind::TINYTEXT:
        case MySQLDataType::Kind::MEDIUMTEXT:
        case MySQLDataType::Kind::LONGTEXT:
            emitU32(type.length > 0 ? static_cast<uint32_t>(type.length) : 255);
            break;
        case MySQLDataType::Kind::DECIMAL:
            emitU32(type.precision > 0 ? static_cast<uint32_t>(type.precision) : 18);
            emitU32(static_cast<uint32_t>(type.scale));
            break;
        default:
            break;
    }
}

void Parser::parseCreateIndex() {
    bool is_unique = false;
    bool is_fulltext = false;
    bool is_spatial = false;

    if (matchKeyword(TokenType::KW_UNIQUE)) {
        is_unique = true;
    } else if (matchKeyword(TokenType::KW_FULLTEXT)) {
        is_fulltext = true;
    } else if (matchKeyword(TokenType::KW_SPATIAL)) {
        is_spatial = true;
    }

    if (!(matchKeyword(TokenType::KW_INDEX) || matchKeyword(TokenType::KW_KEY))) {
        error("Expected INDEX after CREATE");
        synchronize();
        return;
    }

    std::string index_name = parseIdentifier();

    uint8_t index_type = 0xFF;
    if (matchKeyword(TokenType::KW_USING)) {
        std::string algorithm = parseIdentifier();
        std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        if (algorithm == "BTREE") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
        } else if (algorithm == "HASH") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
        } else if (algorithm == "RTREE") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
        } else {
            error("MySQL index type must be BTREE, HASH, or RTREE");
        }
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");

    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);
    std::string table_path = schema + "/" + table;

    std::vector<std::string> columns;
    consume(TokenType::LEFT_PAREN, "Expected (");
    do {
        columns.push_back(parseIdentifier());
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                advance();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        matchKeyword(TokenType::KW_ASC);
        matchKeyword(TokenType::KW_DESC);
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected )");

    if (matchKeyword(TokenType::KW_USING)) {
        std::string algorithm = parseIdentifier();
        std::transform(algorithm.begin(), algorithm.end(), algorithm.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        if (algorithm == "BTREE") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
        } else if (algorithm == "HASH") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
        } else if (algorithm == "RTREE") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
        } else {
            error("MySQL index type must be BTREE, HASH, or RTREE");
        }
    }

    if (is_fulltext) {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::FULLTEXT);
    } else if (is_spatial) {
        index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::RTREE);
    }

    emit(sblr::Opcode::CREATE_INDEX);
    emitString(index_name);
    emitString(table_path);
    emitByte(is_unique ? 1 : 0);
    emitU32(static_cast<uint32_t>(columns.size()));
    for (const auto& col : columns) {
        emitString(col);
    }
    emitU32(0);
    emitString("");
    emitByte(index_type);
    emitByte(0);
    emitByte(0);
}

void Parser::parseCreateView() {
    consumeKeyword(TokenType::KW_VIEW, "Expected VIEW");

    emit(sblr::Opcode::CREATE_VIEW);

    bool or_replace = pending_or_replace_;
    pending_or_replace_ = false;

    std::string schema;
    std::string view_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = view_name;
        view_name = parseIdentifier();
    }
    resolveTableName(schema, view_name);
    std::string view_path = schema.empty() ? view_name : schema + "/" + view_name;

    std::vector<std::string> column_names;
    if (match(TokenType::LEFT_PAREN)) {
        do {
            column_names.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    consumeKeyword(TokenType::KW_AS, "Expected AS");
    size_t def_start = current_token_.span.start.offset;
    bool prev_emit = emit_enabled_;
    emit_enabled_ = false;
    parseSelectStmt();
    emit_enabled_ = prev_emit;
    size_t def_end = current_token_.span.start.offset;

    auto trim_sql = [](std::string_view text) -> std::string {
        size_t start = 0;
        while (start < text.size() &&
               std::isspace(static_cast<unsigned char>(text[start]))) {
            start++;
        }
        size_t end = text.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(text[end - 1]))) {
            end--;
        }
        return std::string(text.substr(start, end - start));
    };

    std::string definition;
    if (def_end >= def_start) {
        definition = trim_sql(lexer_.input().substr(def_start, def_end - def_start));
    }

    bool check_option = false;
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_CHECK)) {
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        } else if (matchKeyword(TokenType::KW_LOCAL)) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        } else if (matchIdentifierKeyword("CASCADED")) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        }
    }

    uint8_t flags = 0;
    if (or_replace) {
        flags |= 0x01;
    }
    if (column_names.size() > 0) {
        flags |= 0x04;
    }
    if (check_option) {
        flags |= 0x02;
    }

    emitString(view_path);
    emitByte(flags);
    if (!column_names.empty()) {
        if (column_names.size() > 255) {
            error("Too many column names in CREATE VIEW");
        } else {
            emitByte(static_cast<uint8_t>(column_names.size()));
            for (const auto& col : column_names) {
                emitString(col);
            }
        }
    }
    emitString(definition);
}

void Parser::parseAlterView() {
    bool or_replace = true;

    emit(sblr::Opcode::CREATE_VIEW);

    std::string schema;
    std::string view_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = view_name;
        view_name = parseIdentifier();
    }
    resolveTableName(schema, view_name);
    std::string view_path = schema.empty() ? view_name : schema + "/" + view_name;

    std::vector<std::string> column_names;
    if (match(TokenType::LEFT_PAREN)) {
        do {
            column_names.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    consumeKeyword(TokenType::KW_AS, "Expected AS");
    size_t def_start = current_token_.span.start.offset;
    bool prev_emit = emit_enabled_;
    emit_enabled_ = false;
    parseSelectStmt();
    emit_enabled_ = prev_emit;
    size_t def_end = current_token_.span.start.offset;

    auto trim_sql = [](std::string_view text) -> std::string {
        size_t start = 0;
        while (start < text.size() &&
               std::isspace(static_cast<unsigned char>(text[start]))) {
            start++;
        }
        size_t end = text.size();
        while (end > start &&
               std::isspace(static_cast<unsigned char>(text[end - 1]))) {
            end--;
        }
        return std::string(text.substr(start, end - start));
    };

    std::string definition;
    if (def_end >= def_start) {
        definition = trim_sql(lexer_.input().substr(def_start, def_end - def_start));
    }

    bool check_option = false;
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_CHECK)) {
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        } else if (matchKeyword(TokenType::KW_LOCAL)) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        } else if (matchIdentifierKeyword("CASCADED")) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        }
    }

    uint8_t flags = 0;
    if (or_replace) {
        flags |= 0x01;
    }
    if (column_names.size() > 0) {
        flags |= 0x04;
    }
    if (check_option) {
        flags |= 0x02;
    }

    emitString(view_path);
    emitByte(flags);
    if (!column_names.empty()) {
        if (column_names.size() > 255) {
            error("Too many column names in ALTER VIEW");
        } else {
            emitByte(static_cast<uint8_t>(column_names.size()));
            for (const auto& col : column_names) {
                emitString(col);
            }
        }
    }
    emitString(definition);
}

void Parser::parseAlterProcedure() {
    std::string schema;
    std::string proc_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = proc_name;
        proc_name = parseIdentifier();
    }
    resolveTableName(schema, proc_name);
    std::string proc_path = schema.empty() ? proc_name : schema + "/" + proc_name;

    bool has_option = false;
    bool has_sql_security = false;
    bool has_comment = false;
    uint8_t sql_security = 1;
    std::string comment_text;

    auto skip_comment_value = [&]() {
        if (check(TokenType::STRING_LITERAL)) {
            std::string_view str = lexer_.stringPool().get(current_token_.value.string_id);
            comment_text.assign(str.data(), str.size());
            advance();
        } else {
            comment_text = parseIdentifier();
        }
        has_comment = true;
    };

    while (true) {
        if (matchKeyword(TokenType::KW_SQL)) {
            consumeKeyword(TokenType::KW_SECURITY, "Expected SECURITY after SQL");
            has_option = true;
            has_sql_security = true;
            if (matchKeyword(TokenType::KW_DEFINER)) {
                sql_security = 0;
            } else if (matchKeyword(TokenType::KW_INVOKER)) {
                sql_security = 1;
            } else {
                error("Expected DEFINER or INVOKER after SQL SECURITY");
                synchronize();
                return;
            }
        } else if (matchKeyword(TokenType::KW_COMMENT)) {
            has_option = true;
            skip_comment_value();
        } else if (matchKeyword(TokenType::KW_LANGUAGE)) {
            has_option = true;
            matchKeyword(TokenType::KW_SQL);
        } else if (matchKeyword(TokenType::KW_CONTAINS) ||
                   matchKeyword(TokenType::KW_READS) ||
                   matchKeyword(TokenType::KW_MODIFIES) ||
                   matchKeyword(TokenType::KW_NO)) {
            has_option = true;
            matchKeyword(TokenType::KW_SQL);
            matchKeyword(TokenType::KW_DATA);
        } else {
            break;
        }
    }

    if (!has_option) {
        error("ALTER PROCEDURE requires at least one characteristic");
        synchronize();
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_PROCEDURE_STMT));
    uint8_t flags = 0;
    if (has_sql_security) flags |= 0x01;
    if (has_comment) flags |= 0x02;
    emitByte(flags);
    emitString(proc_path);
    if (has_sql_security) {
        emitByte(sql_security);
    }
    if (has_comment) {
        emitString(comment_text);
    }
}

void Parser::parseAlterFunction() {
    std::string schema;
    std::string func_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = func_name;
        func_name = parseIdentifier();
    }
    resolveTableName(schema, func_name);
    std::string func_path = schema.empty() ? func_name : schema + "/" + func_name;

    bool has_option = false;
    bool has_sql_security = false;
    bool has_deterministic = false;
    bool has_comment = false;
    uint8_t sql_security = 1;
    uint8_t deterministic = 0;
    std::string comment_text;

    auto skip_comment_value = [&]() {
        if (check(TokenType::STRING_LITERAL)) {
            std::string_view str = lexer_.stringPool().get(current_token_.value.string_id);
            comment_text.assign(str.data(), str.size());
            advance();
        } else {
            comment_text = parseIdentifier();
        }
        has_comment = true;
    };

    while (true) {
        if (matchKeyword(TokenType::KW_SQL)) {
            consumeKeyword(TokenType::KW_SECURITY, "Expected SECURITY after SQL");
            has_option = true;
            has_sql_security = true;
            if (matchKeyword(TokenType::KW_DEFINER)) {
                sql_security = 0;
            } else if (matchKeyword(TokenType::KW_INVOKER)) {
                sql_security = 1;
            } else {
                error("Expected DEFINER or INVOKER after SQL SECURITY");
                synchronize();
                return;
            }
        } else if (matchKeyword(TokenType::KW_DETERMINISTIC)) {
            has_option = true;
            has_deterministic = true;
            deterministic = 1;
        } else if (matchKeyword(TokenType::KW_NOT)) {
            if (matchKeyword(TokenType::KW_DETERMINISTIC)) {
                has_option = true;
                has_deterministic = true;
                deterministic = 0;
            } else {
                error("Expected DETERMINISTIC after NOT");
                synchronize();
                return;
            }
        } else if (matchKeyword(TokenType::KW_COMMENT)) {
            has_option = true;
            skip_comment_value();
        } else if (matchKeyword(TokenType::KW_LANGUAGE)) {
            has_option = true;
            matchKeyword(TokenType::KW_SQL);
        } else if (matchKeyword(TokenType::KW_CONTAINS) ||
                   matchKeyword(TokenType::KW_READS) ||
                   matchKeyword(TokenType::KW_MODIFIES) ||
                   matchKeyword(TokenType::KW_NO)) {
            has_option = true;
            matchKeyword(TokenType::KW_SQL);
            matchKeyword(TokenType::KW_DATA);
        } else {
            break;
        }
    }

    if (!has_option) {
        error("ALTER FUNCTION requires at least one characteristic");
        synchronize();
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_FUNCTION_STMT));
    uint8_t flags = 0;
    if (has_sql_security) flags |= 0x01;
    if (has_deterministic) flags |= 0x02;
    if (has_comment) flags |= 0x04;
    emitByte(flags);
    emitString(func_path);
    if (has_sql_security) {
        emitByte(sql_security);
    }
    if (has_deterministic) {
        emitByte(deterministic);
    }
    if (has_comment) {
        emitString(comment_text);
    }
}

void Parser::parseKillStmt() {
    uint8_t kill_type = 0;  // 0 = CONNECTION, 1 = QUERY
    if (matchIdentifierKeyword("QUERY")) {
        kill_type = 1;
    } else if (matchIdentifierKeyword("CONNECTION")) {
        kill_type = 0;
    }

    if (!check(TokenType::INTEGER_LITERAL)) {
        error("KILL requires a numeric thread id");
        synchronize();
        return;
    }
    int64_t thread_id = current_token_.value.int_value;
    advance();
    if (thread_id < 0 || thread_id > std::numeric_limits<uint32_t>::max()) {
        error("KILL thread id is out of range");
        synchronize();
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MYSQL_KILL));
    emitByte(kill_type);
    emitU32(static_cast<uint32_t>(thread_id));
}

void Parser::parseFlushStmt() {
    enum class FlushAction : uint8_t {
        TABLES = 0,
        PRIVILEGES = 1,
        LOGS = 2,
        STATUS = 3,
        HOSTS = 4
    };

    FlushAction action;
    bool with_read_lock = false;
    std::vector<std::string> tables;

    if (matchKeyword(TokenType::KW_TABLES) || matchKeyword(TokenType::KW_TABLE)) {
        action = FlushAction::TABLES;
        while (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            std::string schema;
            std::string table = parseIdentifier();
            if (match(TokenType::DOT)) {
                schema = table;
                table = parseIdentifier();
            }
            resolveTableName(schema, table);
            tables.push_back(schema.empty() ? table : schema + "/" + table);
            if (!match(TokenType::COMMA)) {
                break;
            }
        }

        if (matchKeyword(TokenType::KW_WITH) || matchIdentifierKeyword("WITH")) {
            if (matchKeyword(TokenType::KW_READ)) {
                matchKeyword(TokenType::KW_LOCAL);
                if (!matchKeyword(TokenType::KW_LOCK) && !matchIdentifierKeyword("LOCK")) {
                    error("Expected LOCK after READ in FLUSH TABLES");
                }
                with_read_lock = true;
            } else {
                error("Expected READ after WITH in FLUSH TABLES");
            }
        }
    } else if (matchIdentifierKeyword("PRIVILEGES")) {
        action = FlushAction::PRIVILEGES;
    } else if (matchIdentifierKeyword("LOGS")) {
        action = FlushAction::LOGS;
    } else if (matchIdentifierKeyword("STATUS")) {
        action = FlushAction::STATUS;
    } else if (matchIdentifierKeyword("HOSTS")) {
        action = FlushAction::HOSTS;
    } else {
        error("Unsupported FLUSH option");
        synchronize();
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MYSQL_FLUSH));
    emitByte(static_cast<uint8_t>(action));
    emitByte(with_read_lock ? 0x01 : 0x00);
    emitU32(static_cast<uint32_t>(tables.size()));
    for (const auto& table : tables) {
        emitString(table);
    }
}

void Parser::parseCreateDatabase() {
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }

    std::string db_name = parseIdentifier();
    std::vector<std::pair<std::string, std::string>> options;

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DATABASE));
    emitByte(if_not_exists ? 0x01 : 0x00);

    std::string db_path = buildEmulatedServerRoot(default_schema_);
    if (!db_path.empty()) {
        db_path += ".databases.";
    }
    db_path += db_name;
    emitString(db_path);
    emitString(db_name);

    // Optional database options
    while (true) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            continue;
        } else if (matchKeyword(TokenType::KW_CHARACTER)) {
            consumeKeyword(TokenType::KW_SET, "Expected SET");
            std::string value = parseIdentifier();
            options.emplace_back("character_set", value);
        } else if (matchKeyword(TokenType::KW_CHARSET)) {
            std::string value = parseIdentifier();
            options.emplace_back("character_set", value);
        } else if (matchKeyword(TokenType::KW_COLLATE)) {
            std::string value = parseIdentifier();
            options.emplace_back("collation", value);
        } else if (matchKeyword(TokenType::KW_ENCRYPTION)) {
            match(TokenType::EQUAL);
            std::string value;
            if (check(TokenType::STRING_LITERAL)) {
                std::string_view str = lexer_.stringPool().get(current_token_.value.string_id);
                value.assign(str.data(), str.size());
                advance();
            } else {
                value = parseIdentifier();
            }
            options.emplace_back("encryption", value);
        } else {
            break;
        }
    }

    emitU32(static_cast<uint32_t>(options.size()));
    for (const auto& opt : options) {
        emitString(opt.first);
        emitString(opt.second);
    }
    emitU32(0);  // alias count
}

void Parser::parseCreateTablespace() {
    std::string tablespace_name = parseIdentifier();

    auto parse_path_value = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto view = lexer_.stringPool().get(current_token_.value.string_id);
            std::string value(view.data(), view.size());
            advance();
            return value;
        }
        if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            return parseIdentifier();
        }
        if (isNonReservedKeyword(current_token_.type)) {
            std::string value = tokenToString(current_token_.type);
            advance();
            return value;
        }
        return "";
    };

    std::string location;
    if (matchKeyword(TokenType::KW_ADD) || matchIdentifierKeyword("ADD")) {
        if (!matchIdentifierKeyword("DATAFILE") && !matchIdentifierKeyword("FILE")) {
            error("Expected DATAFILE after ADD in CREATE TABLESPACE");
            synchronize();
            return;
        }
        location = parse_path_value();
    } else if (matchIdentifierKeyword("DATAFILE") || matchIdentifierKeyword("LOCATION")) {
        location = parse_path_value();
    } else if (check(TokenType::STRING_LITERAL)) {
        location = parse_path_value();
    }

    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        if (match(TokenType::LEFT_PAREN)) {
            int depth = 1;
            while (depth > 0 && !check(TokenType::END_OF_FILE)) {
                if (match(TokenType::LEFT_PAREN)) {
                    depth++;
                } else if (match(TokenType::RIGHT_PAREN)) {
                    depth--;
                } else {
                    advance();
                }
            }
            continue;
        }
        if (matchKeyword(TokenType::KW_ENGINE) || matchIdentifierKeyword("FILE_BLOCK_SIZE") ||
            matchIdentifierKeyword("EXTENT_SIZE") || matchIdentifierKeyword("INITIAL_SIZE") ||
            matchIdentifierKeyword("AUTOEXTEND_SIZE") || matchIdentifierKeyword("MAX_SIZE") ||
            matchIdentifierKeyword("NODEGROUP") || matchIdentifierKeyword("USE_LOGFILE_GROUP")) {
            match(TokenType::EQUAL);
            if (check(TokenType::INTEGER_LITERAL) || check(TokenType::STRING_LITERAL) ||
                check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER) ||
                isNonReservedKeyword(current_token_.type)) {
                parse_path_value();
                continue;
            }
        }
        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    if (location.empty()) {
        error("CREATE TABLESPACE requires a DATAFILE or LOCATION path");
        synchronize();
        return;
    }

    emit(sblr::Opcode::CREATE_TABLESPACE);
    emitString(tablespace_name);
    emitString(location);
    emitByte(0);   // autoextend disabled
    emitU32(0);    // autoextend size
    emitU32(0);    // max size
    emitU32(0);    // prealloc
}

void Parser::parseAlterTablespace() {
    std::string tablespace_name = parseIdentifier();

    if (matchKeyword(TokenType::KW_RENAME) || matchIdentifierKeyword("RENAME")) {
        consumeKeyword(TokenType::KW_TO, "Expected TO after RENAME");
        std::string new_name = parseIdentifier();
        emit(sblr::Opcode::ALTER_TABLESPACE);
        emitString(tablespace_name);
        emitU32(1);
        emitByte(3);  // RENAME_TO
        emitString(new_name);
        return;
    }

    if (matchKeyword(TokenType::KW_ADD) || matchIdentifierKeyword("ADD")) {
        if (!matchIdentifierKeyword("DATAFILE") && !matchIdentifierKeyword("FILE")) {
            error("Expected DATAFILE after ADD in ALTER TABLESPACE");
            synchronize();
            return;
        }
        std::string file_path;
        if (check(TokenType::STRING_LITERAL)) {
            auto view = lexer_.stringPool().get(current_token_.value.string_id);
            file_path.assign(view.data(), view.size());
            advance();
        } else {
            file_path = parseIdentifier();
        }
        emit(sblr::Opcode::ATTACH_TABLESPACE);
        emitString(file_path);
        emitString(tablespace_name);
        emitByte(1);  // validate
        emitByte(0);  // allow mismatch
        return;
    }

    if (matchKeyword(TokenType::KW_DROP) || matchIdentifierKeyword("DROP")) {
        if (matchIdentifierKeyword("DATAFILE") || matchIdentifierKeyword("FILE")) {
            if (check(TokenType::STRING_LITERAL)) {
                advance();
            } else if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                parseIdentifier();
            }
        }
        emit(sblr::Opcode::DETACH_TABLESPACE);
        emitString(tablespace_name);
        emitByte(0);  // force
        return;
    }

    bool is_reset = false;
    if (matchKeyword(TokenType::KW_RESET) || matchIdentifierKeyword("RESET")) {
        is_reset = true;
    } else if (matchKeyword(TokenType::KW_SET) || matchIdentifierKeyword("SET")) {
        is_reset = false;
    } else {
        error("Expected RENAME, ADD/DROP DATAFILE, or SET/RESET for ALTER TABLESPACE");
        synchronize();
        return;
    }

    std::vector<uint8_t> actions;
    std::vector<uint8_t> bool_values;
    std::vector<uint32_t> size_values;

    auto read_option_name = [&]() -> std::string {
        if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            return parseIdentifier();
        }
        if (isNonReservedKeyword(current_token_.type)) {
            std::string name = tokenToString(current_token_.type);
            advance();
            return name;
        }
        return "";
    };

    do {
        std::string opt = read_option_name();
        if (opt.empty()) {
            error("Expected tablespace option name");
            synchronize();
            return;
        }
        std::transform(opt.begin(), opt.end(), opt.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        match(TokenType::EQUAL);

        if (opt == "AUTOEXTEND") {
            actions.push_back(0);
            if (is_reset) {
                bool_values.push_back(0);
            } else if (matchKeyword(TokenType::KW_ON)) {
                bool_values.push_back(1);
            } else if (matchKeyword(TokenType::KW_OFF)) {
                bool_values.push_back(0);
            } else if (check(TokenType::INTEGER_LITERAL)) {
                bool_values.push_back(current_token_.value.int_value != 0 ? 1 : 0);
                advance();
            } else {
                error("Expected ON/OFF for AUTOEXTEND");
                synchronize();
                return;
            }
        } else if (opt == "AUTOEXTEND_SIZE") {
            actions.push_back(1);
            if (is_reset) {
                size_values.push_back(0);
            } else if (check(TokenType::INTEGER_LITERAL)) {
                size_values.push_back(static_cast<uint32_t>(current_token_.value.int_value));
                advance();
            } else {
                error("Expected integer for AUTOEXTEND_SIZE");
                synchronize();
                return;
            }
        } else if (opt == "MAX_SIZE" || opt == "MAXSIZE") {
            actions.push_back(2);
            if (is_reset) {
                size_values.push_back(0);
            } else if (matchKeyword(TokenType::KW_UNLIMITED)) {
                size_values.push_back(0);
            } else if (check(TokenType::INTEGER_LITERAL)) {
                size_values.push_back(static_cast<uint32_t>(current_token_.value.int_value));
                advance();
            } else {
                error("Expected integer or UNLIMITED for MAX_SIZE");
                synchronize();
                return;
            }
        } else {
            error("Unsupported ALTER TABLESPACE option: " + opt);
            synchronize();
            return;
        }
    } while (match(TokenType::COMMA));

    if (actions.empty()) {
        error("ALTER TABLESPACE requires at least one option");
        synchronize();
        return;
    }

    emit(sblr::Opcode::ALTER_TABLESPACE);
    emitString(tablespace_name);
    emitU32(static_cast<uint32_t>(actions.size()));

    size_t bool_idx = 0;
    size_t size_idx = 0;
    for (uint8_t action : actions) {
        emitByte(action);
        if (action == 0) {
            emitByte(bool_values[bool_idx++]);
        } else if (action == 1 || action == 2) {
            emitU32(size_values[size_idx++]);
        }
    }
}

void Parser::parseDropTablespace() {
    bool if_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_exists = true;
    }
    std::string tablespace_name = parseIdentifier();
    (void)if_exists;

    bool force = false;
    if (matchIdentifierKeyword("ENGINE")) {
        parseIdentifier();
    }
    if (matchKeyword(TokenType::KW_FORCE) || matchIdentifierKeyword("FORCE")) {
        force = true;
    }

    emit(sblr::Opcode::DROP_TABLESPACE);
    emitString(tablespace_name);
    emitByte(force ? 1 : 0);
}

void Parser::parseCreateProcedure() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_PROCEDURE_STMT));

    std::string schema;
    std::string proc_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = proc_name;
        proc_name = parseIdentifier();
    }
    resolveTableName(schema, proc_name);
    std::string proc_path = schema.empty() ? proc_name : schema + "/" + proc_name;

    uint8_t flags = 0;
    emitByte(flags);
    emitString(proc_path);

    std::vector<MySQLDataType> param_types;
    std::vector<std::string> param_names;
    std::vector<uint8_t> param_modes;

    consume(TokenType::LEFT_PAREN, "Expected (");
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            uint8_t mode = 0;
            if (matchKeyword(TokenType::KW_IN)) {
                mode = 0;
            } else if (matchKeyword(TokenType::KW_OUT)) {
                mode = 1;
            } else if (matchKeyword(TokenType::KW_INOUT)) {
                mode = 2;
            }

            param_modes.push_back(mode);
            param_names.push_back(parseIdentifier());
            param_types.push_back(parseDataType());

            if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                bool prev_emit = emit_enabled_;
                emit_enabled_ = false;
                parseExpression();
                emit_enabled_ = prev_emit;
            }
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_PAREN, "Expected )");

    emitByte(static_cast<uint8_t>(param_names.size()));
    for (size_t i = 0; i < param_names.size(); ++i) {
        emitByte(param_modes[i]);
        emitString(param_names[i]);
        emitByte(static_cast<uint8_t>(typeToOpcode(param_types[i].kind)));
        emitU32(static_cast<uint32_t>(param_types[i].precision));
        emitU32(static_cast<uint32_t>(param_types[i].scale));
    }

    auto capture_body = [&]() -> std::string {
        if (check(TokenType::SEMICOLON) || check(TokenType::END_OF_FILE)) {
            return "";
        }
        size_t start = current_token_.span.start.offset;
        Token last = current_token_;
        while (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
            last = current_token_;
            advance();
        }
        size_t end = last.span.start.offset + last.span.length;
        auto input = lexer_.input();
        if (end > input.size()) {
            end = input.size();
        }
        if (end <= start) {
            return "";
        }
        return std::string(input.substr(start, end - start));
    };

    std::string body = capture_body();
    emitString(body);
}

void Parser::parseCreateFunction() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_FUNCTION_STMT));

    std::string schema;
    std::string func_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = func_name;
        func_name = parseIdentifier();
    }
    resolveTableName(schema, func_name);
    std::string func_path = schema.empty() ? func_name : schema + "/" + func_name;

    uint8_t flags = 0;
    emitByte(flags);
    emitString(func_path);

    std::vector<MySQLDataType> param_types;
    std::vector<std::string> param_names;
    std::vector<uint8_t> param_modes;

    consume(TokenType::LEFT_PAREN, "Expected (");
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            uint8_t mode = 0;
            if (matchKeyword(TokenType::KW_IN)) {
                mode = 0;
            } else if (matchKeyword(TokenType::KW_OUT)) {
                mode = 1;
            } else if (matchKeyword(TokenType::KW_INOUT)) {
                mode = 2;
            }

            param_modes.push_back(mode);
            param_names.push_back(parseIdentifier());
            param_types.push_back(parseDataType());

            if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                bool prev_emit = emit_enabled_;
                emit_enabled_ = false;
                parseExpression();
                emit_enabled_ = prev_emit;
            }
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_PAREN, "Expected )");

    emitByte(static_cast<uint8_t>(param_names.size()));
    for (size_t i = 0; i < param_names.size(); ++i) {
        emitByte(param_modes[i]);
        emitString(param_names[i]);
        emitByte(static_cast<uint8_t>(typeToOpcode(param_types[i].kind)));
        emitU32(static_cast<uint32_t>(param_types[i].precision));
        emitU32(static_cast<uint32_t>(param_types[i].scale));
    }

    consumeKeyword(TokenType::KW_RETURNS, "Expected RETURNS");
    MySQLDataType return_type = parseDataType();

    emitByte(static_cast<uint8_t>(typeToOpcode(return_type.kind)));
    emitU32(static_cast<uint32_t>(return_type.precision));
    emitU32(static_cast<uint32_t>(return_type.scale));

    auto capture_body = [&]() -> std::string {
        if (check(TokenType::SEMICOLON) || check(TokenType::END_OF_FILE)) {
            return "";
        }
        size_t start = current_token_.span.start.offset;
        Token last = current_token_;
        while (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
            last = current_token_;
            advance();
        }
        size_t end = last.span.start.offset + last.span.length;
        auto input = lexer_.input();
        if (end > input.size()) {
            end = input.size();
        }
        if (end <= start) {
            return "";
        }
        return std::string(input.substr(start, end - start));
    };

    std::string body = capture_body();
    emitString(body);
}

void Parser::parseCreateTrigger() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_TRIGGER));

    std::string trigger_name = parseIdentifier();

    uint8_t timing = 0;
    if (matchKeyword(TokenType::KW_BEFORE)) {
        timing = static_cast<uint8_t>(core::CatalogManager::TriggerTiming::BEFORE);
    } else if (matchKeyword(TokenType::KW_AFTER)) {
        timing = static_cast<uint8_t>(core::CatalogManager::TriggerTiming::AFTER);
    } else {
        error("Expected BEFORE or AFTER in CREATE TRIGGER");
        synchronize();
        return;
    }

    uint8_t event_mask = 0;
    if (matchKeyword(TokenType::KW_INSERT)) {
        event_mask = static_cast<uint8_t>(1u << static_cast<uint8_t>(
            core::CatalogManager::TriggerEvent::INSERT));
    } else if (matchKeyword(TokenType::KW_UPDATE)) {
        event_mask = static_cast<uint8_t>(1u << static_cast<uint8_t>(
            core::CatalogManager::TriggerEvent::UPDATE));
    } else if (matchKeyword(TokenType::KW_DELETE)) {
        event_mask = static_cast<uint8_t>(1u << static_cast<uint8_t>(
            core::CatalogManager::TriggerEvent::DELETE));
    } else {
        error("Expected INSERT, UPDATE, or DELETE in CREATE TRIGGER");
        synchronize();
        return;
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);
    std::string table_path = schema.empty() ? table : schema + "/" + table;

    if (matchKeyword(TokenType::KW_FOR)) {
        matchKeyword(TokenType::KW_EACH);
        matchKeyword(TokenType::KW_ROW);
    }

    auto capture_body = [&]() -> std::string {
        if (check(TokenType::SEMICOLON) || check(TokenType::END_OF_FILE)) {
            return "";
        }
        size_t start = current_token_.span.start.offset;
        Token last = current_token_;
        while (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
            last = current_token_;
            advance();
        }
        size_t end = last.span.start.offset + last.span.length;
        auto input = lexer_.input();
        if (end > input.size()) {
            end = input.size();
        }
        if (end <= start) {
            return "";
        }
        return std::string(input.substr(start, end - start));
    };

    std::string body = capture_body();

    std::string proc_name = "__trigger_" + trigger_name;
    std::string proc_path = schema.empty() ? proc_name : schema + "/" + proc_name;

    // Create an implicit procedure to hold the trigger body.
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_PROCEDURE_STMT));
    emitByte(0);
    emitString(proc_path);
    emitByte(0);  // no parameters
    emitString(body);

    emitString(trigger_name);
    emitString(table_path);
    emitByte(timing);
    emitByte(event_mask);
    emitByte(static_cast<uint8_t>(core::CatalogManager::TriggerGranularity::FOR_EACH_ROW));
    emitString(proc_path);
}

IndexDef Parser::parseIndexDef() {
    IndexDef idx;
    auto to_upper = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
        return value;
    };
    auto parse_algorithm_name = [&]() -> std::string {
        if (check(TokenType::KW_BTREE)) {
            advance();
            return "BTREE";
        }
        if (check(TokenType::KW_HASH)) {
            advance();
            return "HASH";
        }
        if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
            return parseIdentifier();
        }
        error("Expected index type");
        return "";
    };
    auto validate_algorithm = [&](const std::string& algorithm) {
        if (algorithm.empty()) {
            return;
        }
        std::string upper = to_upper(algorithm);
        if (upper != "BTREE" && upper != "HASH" && upper != "RTREE") {
            error("MySQL index type must be BTREE, HASH, or RTREE");
        }
    };

    if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
        idx.name = parseIdentifier();
    }

    if (matchKeyword(TokenType::KW_USING)) {
        idx.algorithm = parse_algorithm_name();
        validate_algorithm(idx.algorithm);
    }

    consume(TokenType::LEFT_PAREN, "Expected (");
    do {
        idx.columns.push_back(parseIdentifier());
        int length = 0;
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                length = static_cast<int>(current_token_.value.int_value);
                advance();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        idx.column_lengths.push_back(length);

        if (matchKeyword(TokenType::KW_ASC) || matchKeyword(TokenType::KW_DESC)) {
            // Ignore sort order for now.
        }
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected )");

    while (!check(TokenType::COMMA) && !check(TokenType::RIGHT_PAREN) &&
           !check(TokenType::END_OF_FILE)) {
        if (matchKeyword(TokenType::KW_USING)) {
            idx.algorithm = parse_algorithm_name();
            validate_algorithm(idx.algorithm);
        } else if (matchKeyword(TokenType::KW_COMMENT)) {
            if (check(TokenType::STRING_LITERAL)) {
                idx.comment = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            } else if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
                idx.comment = parseIdentifier();
            }
        } else if (matchKeyword(TokenType::KW_WITH)) {
            if (matchIdentifierKeyword("PARSER")) {
                parseIdentifier();
            }
        } else {
            advance();
        }
    }

    return idx;
}

ForeignKeyDef Parser::parseForeignKeyDef() {
    ForeignKeyDef fk;

    if (check(TokenType::IDENTIFIER) || check(TokenType::BACKTICK_IDENTIFIER)) {
        fk.name = parseIdentifier();
    }

    consume(TokenType::LEFT_PAREN, "Expected (");
    do {
        fk.columns.push_back(parseIdentifier());
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected )");

    consumeKeyword(TokenType::KW_REFERENCES, "Expected REFERENCES");
    std::string ref_schema;
    std::string ref_table = parseIdentifier();
    if (match(TokenType::DOT)) {
        ref_schema = ref_table;
        ref_table = parseIdentifier();
    }
    resolveTableName(ref_schema, ref_table);
    fk.ref_table = ref_schema + "/" + ref_table;

    if (match(TokenType::LEFT_PAREN)) {
        do {
            fk.ref_columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    while (matchKeyword(TokenType::KW_ON)) {
        std::string* action = nullptr;
        if (matchKeyword(TokenType::KW_DELETE)) {
            action = &fk.on_delete;
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            action = &fk.on_update;
        }

        if (action) {
            if (matchKeyword(TokenType::KW_CASCADE)) {
                *action = "CASCADE";
            } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                *action = "RESTRICT";
            } else if (matchKeyword(TokenType::KW_SET)) {
                if (matchKeyword(TokenType::KW_NULL)) {
                    *action = "SET NULL";
                } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                    *action = "SET DEFAULT";
                }
            } else if (matchKeyword(TokenType::KW_NO)) {
                consumeKeyword(TokenType::KW_ACTION, "Expected ACTION");
                *action = "NO ACTION";
            }
        }
    }

    return fk;
}

// ============================================================================
// Admin Statements
// ============================================================================

void Parser::parseSetStmt() {
    consume(TokenType::KW_SET, "Expected SET");

    // Handle various SET forms
    if (matchKeyword(TokenType::KW_ROLE) || matchIdentifierKeyword("ROLE")) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_ROLE));

        if (matchIdentifierKeyword("NONE") || matchIdentifierKeyword("DEFAULT") ||
            matchIdentifierKeyword("ALL")) {
            emitByte(0x01);
            return;
        }

        std::string role_name = parseIdentifier();
        emitByte(0x00);
        emitString(role_name);
        return;
    }

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
        bool has_autocommit = false;
        bool has_conflict_error_code = false;
        uint8_t isolation = kIsoReadCommitted;
        uint8_t access_mode = 0;  // 0=READ WRITE, 1=READ ONLY
        sblr::AutocommitMode autocommit_mode = sblr::AutocommitMode::UNCHANGED;
        auto conflict_action = sblr::TransactionConflictAction::DEFAULT;
        int32_t conflict_error_code = 0;

        while (true) {
            if (matchKeyword(TokenType::KW_ON)) {
                error("MySQL does not support ON CONFLICT in SET TRANSACTION");
                synchronize();
                return;
            } else if (matchIdentifierKeyword("AUTOCOMMIT")) {
                error("MySQL does not support AUTOCOMMIT in SET TRANSACTION");
                synchronize();
                return;
            } else if (matchKeyword(TokenType::KW_ISOLATION)) {
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
        if (has_autocommit) flags |= sblr::TransactionFlags::HAS_AUTOCOMMIT;
        if (has_conflict_error_code) flags |= sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE;
        emitU16(flags);
        emitByte(static_cast<uint8_t>(conflict_action));
        if (has_conflict_error_code) {
            emitU32(static_cast<uint32_t>(conflict_error_code));
        }
        if (has_autocommit) {
            emitByte(static_cast<uint8_t>(autocommit_mode));
        }
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

    if (var_upper == "AUTOCOMMIT") {
        auto identifierEquals = [&](const char* keyword) -> bool {
            if (!check(TokenType::IDENTIFIER)) {
                return false;
            }
            std::string_view text = lexer_.stringPool().get(current_token_.value.string_id);
            size_t len = std::strlen(keyword);
            if (text.size() != len) {
                return false;
            }
            for (size_t i = 0; i < len; ++i) {
                char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
                char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
                if (a != b) {
                    return false;
                }
            }
            return true;
        };

        if (check(TokenType::INTEGER_LITERAL) || check(TokenType::KW_ON) ||
            identifierEquals("ON") || identifierEquals("OFF")) {
            uint8_t mode = 0;
            if (check(TokenType::INTEGER_LITERAL)) {
                int64_t value = current_token_.value.int_value;
                advance();
                if (value == 0) {
                    mode = 0;
                } else if (value == 1) {
                    mode = 1;
                } else {
                    error("AUTOCOMMIT expects 0/1 or ON/OFF");
                }
            } else if (matchKeyword(TokenType::KW_ON) || matchIdentifierKeyword("ON")) {
                mode = 1;
            } else if (matchIdentifierKeyword("OFF")) {
                mode = 0;
            } else {
                error("Expected AUTOCOMMIT mode (ON/OFF/1/0)");
            }

            auto conflict_action = sblr::TransactionConflictAction::DEFAULT;
            int32_t conflict_error_code = 0;
            bool has_conflict_error_code = false;

            if (matchKeyword(TokenType::KW_ON)) {
                error("MySQL does not support ON CONFLICT for SET AUTOCOMMIT");
                synchronize();
                return;
            }

            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_AUTOCOMMIT));
            emitByte(mode);
            emitByte(static_cast<uint8_t>(conflict_action));
            if (conflict_action == sblr::TransactionConflictAction::ERROR) {
                int32_t code = has_conflict_error_code ? conflict_error_code : 0;
                emitU32(static_cast<uint32_t>(code));
            }
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
    } else if (matchKeyword(TokenType::KW_STATUS) || matchIdentifierKeyword("STATUS")) {
        if (matchKeyword(TokenType::KW_GLOBAL) || matchKeyword(TokenType::KW_SESSION)) {
            // Scope ignored for now.
        }
        if (matchKeyword(TokenType::KW_LIKE)) {
            if (check(TokenType::STRING_LITERAL)) {
                advance();
            }
            warning("SHOW STATUS LIKE is not filtered; returning metrics snapshot");
        } else if (matchKeyword(TokenType::KW_WHERE)) {
            parseExpression();
            warning("SHOW STATUS WHERE is not filtered; returning metrics snapshot");
        }
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_METRICS));
    } else if (matchKeyword(TokenType::KW_VARIABLES) || matchIdentifierKeyword("VARIABLES")) {
        if (matchKeyword(TokenType::KW_GLOBAL) || matchKeyword(TokenType::KW_SESSION)) {
            // Scope ignored for now.
        }

        auto contains_wildcard = [](const std::string& text) {
            for (char ch : text) {
                if (ch == '%' || ch == '_') {
                    return true;
                }
            }
            return false;
        };

        std::string pattern;
        if (matchKeyword(TokenType::KW_LIKE)) {
            if (check(TokenType::STRING_LITERAL)) {
                pattern = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            }
        } else if (matchKeyword(TokenType::KW_WHERE)) {
            parseExpression();
            warning("SHOW VARIABLES WHERE is not filtered; returning full variable list");
        }

        if (!pattern.empty() && !contains_wildcard(pattern)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_VARIABLE));
            emitString(pattern);
        } else {
            if (!pattern.empty()) {
                warning("SHOW VARIABLES LIKE pattern not filtered; returning full variable list");
            }
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_ALL));
        }
    } else if (matchIdentifierKeyword("PROCESSLIST")) {
        warning("SHOW PROCESSLIST is mapped to SHOW SYSTEM output");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_SYSTEM));
    } else if (matchKeyword(TokenType::KW_WARNINGS) || matchIdentifierKeyword("WARNINGS")) {
        warning("SHOW WARNINGS is mapped to SHOW SYSTEM output");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_SYSTEM));
    } else if (matchKeyword(TokenType::KW_ERRORS) || matchIdentifierKeyword("ERRORS")) {
        warning("SHOW ERRORS is mapped to SHOW SYSTEM output");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_SYSTEM));
    } else if (matchIdentifierKeyword("GRANTS")) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_GRANTS));

        if (matchKeyword(TokenType::KW_FOR) || matchIdentifierKeyword("FOR")) {
            if (check(TokenType::STRING_LITERAL)) {
                emitString(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            } else {
                emitString(parseIdentifier());
            }
        } else {
            emitString("");
        }
    } else {
        error("Unknown SHOW command");
        synchronize();
    }
}

// ============================================================================
// DCL Statements
// ============================================================================

void Parser::parseCreateUser() {
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT after IF");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    auto parse_user_name = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            std::string value(lexer_.stringPool().get(current_token_.value.string_id));
            advance();
            return value;
        }
        return parseIdentifier();
    };

    std::string username = parse_user_name();
    bool has_password = false;
    std::string password;

    if (matchKeyword(TokenType::KW_IDENTIFIED)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after IDENTIFIED");
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after IDENTIFIED BY");
        } else {
            password = std::string(lexer_.stringPool().get(current_token_.value.string_id));
            advance();
            has_password = true;
        }
    } else if (matchKeyword(TokenType::KW_WITH) ||
               matchIdentifierKeyword("WITH")) {
        if (matchKeyword(TokenType::KW_PASSWORD) || matchIdentifierKeyword("PASSWORD")) {
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal after PASSWORD");
            } else {
                password = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
                has_password = true;
            }
        }
    }

    if (if_not_exists) {
        warning("CREATE USER IF NOT EXISTS is not enforced by executor");
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_USER));
    emitString(username);

    uint8_t flags = 0;
    if (has_password) {
        flags |= 0x01;
    }
    emitByte(flags);
    if (has_password) {
        emitString(password);
    }
}

void Parser::parseCreateRole() {
    std::string role_name = parseIdentifier();

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_ROLE));
    emitString(role_name);
}

void Parser::parseAlterUser() {
    auto parse_user_name = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            std::string value(lexer_.stringPool().get(current_token_.value.string_id));
            advance();
            return value;
        }
        return parseIdentifier();
    };

    std::string username = parse_user_name();
    bool has_password = false;
    std::string password;

    if (matchKeyword(TokenType::KW_IDENTIFIED)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after IDENTIFIED");
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after IDENTIFIED BY");
        } else {
            password = std::string(lexer_.stringPool().get(current_token_.value.string_id));
            advance();
            has_password = true;
        }
    } else if (matchKeyword(TokenType::KW_PASSWORD) || matchIdentifierKeyword("PASSWORD")) {
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after PASSWORD");
        } else {
            password = std::string(lexer_.stringPool().get(current_token_.value.string_id));
            advance();
            has_password = true;
        }
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_USER));
    emitString(username);

    uint8_t flags = 0;
    if (has_password) {
        flags |= 0x01;
    }
    emitByte(flags);
    if (has_password) {
        emitString(password);
    }
}

void Parser::parseAlterRole() {
    bool if_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    auto parse_role_name = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            std::string value(lexer_.stringPool().get(current_token_.value.string_id));
            advance();
            return value;
        }
        return parseIdentifier();
    };

    std::string role_name = parse_role_name();
    std::string new_name;

    if (matchKeyword(TokenType::KW_RENAME) || matchIdentifierKeyword("RENAME")) {
        consumeKeyword(TokenType::KW_TO, "Expected TO after RENAME");
        new_name = parse_role_name();
    } else {
        error("ALTER ROLE supports RENAME TO only");
        synchronize();
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_ROLE));
    emitString(role_name);

    uint8_t flags = 0;
    if (if_exists) {
        flags |= 0x01;
    }
    emitByte(flags);
    emitString(new_name);
}

void Parser::parseDropUser() {
    bool if_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    auto parse_user_name = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            std::string value(lexer_.stringPool().get(current_token_.value.string_id));
            advance();
            return value;
        }
        return parseIdentifier();
    };

    std::string username = parse_user_name();
    bool cascade = false;
    if (matchIdentifierKeyword("CASCADE")) {
        cascade = true;
    } else if (matchIdentifierKeyword("RESTRICT")) {
        cascade = false;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_USER));
    emitString(username);

    uint8_t flags = 0;
    if (if_exists) {
        flags |= 0x01;
    }
    if (cascade) {
        flags |= 0x02;
    }
    emitByte(flags);
}

void Parser::parseDropRole() {
    bool if_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    std::string role_name = parseIdentifier();
    bool cascade = false;
    if (matchIdentifierKeyword("CASCADE")) {
        cascade = true;
    } else if (matchIdentifierKeyword("RESTRICT")) {
        cascade = false;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_ROLE));
    emitString(role_name);

    uint8_t flags = 0;
    if (if_exists) {
        flags |= 0x01;
    }
    if (cascade) {
        flags |= 0x02;
    }
    emitByte(flags);
}

void Parser::parseGrantStmt() {
    consume(TokenType::KW_GRANT, "Expected GRANT");

    if (matchKeyword(TokenType::KW_ROLE) || matchIdentifierKeyword("ROLE")) {
        std::string role_name = parseIdentifier();
        consumeKeyword(TokenType::KW_TO, "Expected TO after GRANT ROLE");

        std::string grantee_name = parseIdentifier();
        bool with_admin_option = false;

        if (matchKeyword(TokenType::KW_WITH) || matchIdentifierKeyword("WITH")) {
            if (matchKeyword(TokenType::KW_ADMIN) || matchIdentifierKeyword("ADMIN")) {
                consumeKeyword(TokenType::KW_OPTION, "Expected OPTION after ADMIN");
                with_admin_option = true;
            }
        }

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GRANT_ROLE));
        emitString(role_name);
        emitByte(static_cast<uint8_t>(core::CatalogManager::GranteeType::USER));
        emitString(grantee_name);
        emitByte(with_admin_option ? 0x01 : 0x00);
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GRANT_PRIVILEGE));

    uint32_t privileges = 0;
    bool has_column_list = false;
    std::vector<std::string> column_names;

    auto add_privilege = [&](core::CatalogManager::Privilege priv) {
        privileges |= static_cast<uint32_t>(priv);
    };

    if (matchKeyword(TokenType::KW_ALL)) {
        matchKeyword(TokenType::KW_PRIVILEGES);
        privileges = static_cast<uint32_t>(core::CatalogManager::Privilege::ALL);
    } else {
        do {
            if (matchKeyword(TokenType::KW_SELECT)) {
                add_privilege(core::CatalogManager::Privilege::SELECT);
            } else if (matchKeyword(TokenType::KW_INSERT)) {
                add_privilege(core::CatalogManager::Privilege::INSERT);
            } else if (matchKeyword(TokenType::KW_UPDATE)) {
                add_privilege(core::CatalogManager::Privilege::UPDATE);
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                add_privilege(core::CatalogManager::Privilege::DELETE);
            } else if (matchIdentifierKeyword("EXECUTE")) {
                add_privilege(core::CatalogManager::Privilege::EXECUTE);
            } else if (matchKeyword(TokenType::KW_REFERENCES)) {
                add_privilege(core::CatalogManager::Privilege::REFERENCES);
            } else if (matchKeyword(TokenType::KW_TRIGGER)) {
                add_privilege(core::CatalogManager::Privilege::TRIGGER);
            } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
                add_privilege(core::CatalogManager::Privilege::TRUNCATE);
            } else if (matchKeyword(TokenType::KW_USAGE)) {
                add_privilege(core::CatalogManager::Privilege::USAGE);
            } else if (matchKeyword(TokenType::KW_CREATE)) {
                add_privilege(core::CatalogManager::Privilege::CREATE);
            } else if (matchIdentifierKeyword("CONNECT")) {
                add_privilege(core::CatalogManager::Privilege::CONNECT);
            } else if (matchKeyword(TokenType::KW_TEMPORARY)) {
                add_privilege(core::CatalogManager::Privilege::TEMPORARY);
            } else if (matchIdentifierKeyword("COPY")) {
                add_privilege(core::CatalogManager::Privilege::COPY_FILE);
            } else {
                error("Unsupported GRANT privilege");
                break;
            }

            if (match(TokenType::LEFT_PAREN)) {
                has_column_list = true;
                do {
                    column_names.push_back(parseIdentifier());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
        } while (match(TokenType::COMMA));
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");

    core::CatalogManager::PermissionObjectType object_type =
        core::CatalogManager::PermissionObjectType::TABLE;
    if (matchKeyword(TokenType::KW_TABLE)) {
        object_type = core::CatalogManager::PermissionObjectType::TABLE;
    } else if (matchIdentifierKeyword("SEQUENCE")) {
        object_type = core::CatalogManager::PermissionObjectType::SEQUENCE;
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        object_type = core::CatalogManager::PermissionObjectType::FUNCTION;
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        object_type = core::CatalogManager::PermissionObjectType::PROCEDURE;
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        object_type = core::CatalogManager::PermissionObjectType::SCHEMA;
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        object_type = core::CatalogManager::PermissionObjectType::VIEW;
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        object_type = core::CatalogManager::PermissionObjectType::DATABASE;
    } else if (matchKeyword(TokenType::KW_ALL)) {
        error("GRANT ON ALL ... is not supported in current bytecode");
    }

    std::string object_name = parseQualifiedName();
    if (match(TokenType::COMMA)) {
        error("GRANT supports a single object in current bytecode");
        parseQualifiedName();
    }

    consumeKeyword(TokenType::KW_TO, "Expected TO");

    core::CatalogManager::GranteeType grantee_type = core::CatalogManager::GranteeType::USER;
    std::string grantee_name;
    if (matchKeyword(TokenType::KW_PUBLIC)) {
        grantee_type = core::CatalogManager::GranteeType::PUBLIC;
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        grantee_type = core::CatalogManager::GranteeType::ROLE;
        grantee_name = parseIdentifier();
    } else if (matchIdentifierKeyword("GROUP")) {
        grantee_type = core::CatalogManager::GranteeType::GROUP;
        grantee_name = parseIdentifier();
    } else if (check(TokenType::STRING_LITERAL)) {
        grantee_name = std::string(lexer_.stringPool().get(current_token_.value.string_id));
        advance();
    } else {
        grantee_name = parseIdentifier();
    }
    if (match(TokenType::COMMA)) {
        error("GRANT supports a single grantee in current bytecode");
        parseIdentifier();
    }

    bool with_grant_option = false;
    if (matchKeyword(TokenType::KW_WITH) || matchIdentifierKeyword("WITH")) {
        consumeKeyword(TokenType::KW_GRANT, "Expected GRANT");
        consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
        with_grant_option = true;
    }

    uint8_t flags = 0;
    if (with_grant_option) flags |= 0x01;
    if (has_column_list) flags |= 0x02;

    emitU32(privileges);
    emitByte(static_cast<uint8_t>(object_type));
    emitString(object_name);
    emitByte(static_cast<uint8_t>(grantee_type));
    emitString(grantee_name);
    emitByte(flags);
    if (has_column_list) {
        emitU32(static_cast<uint32_t>(column_names.size()));
        for (const auto& name : column_names) {
            emitString(name);
        }
    }
}

void Parser::parseRevokeStmt() {
    consume(TokenType::KW_REVOKE, "Expected REVOKE");

    if (matchKeyword(TokenType::KW_ROLE) || matchIdentifierKeyword("ROLE")) {
        std::string role_name = parseIdentifier();
        consumeKeyword(TokenType::KW_FROM, "Expected FROM after REVOKE ROLE");

        std::string grantee_name = parseIdentifier();
        bool cascade = false;
        if (matchIdentifierKeyword("CASCADE")) {
            cascade = true;
        } else if (matchIdentifierKeyword("RESTRICT")) {
            cascade = false;
        }

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_REVOKE_ROLE));
        emitString(role_name);
        emitByte(static_cast<uint8_t>(core::CatalogManager::GranteeType::USER));
        emitString(grantee_name);
        emitByte(cascade ? 0x01 : 0x00);
        return;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_REVOKE_PRIVILEGE));

    if (matchKeyword(TokenType::KW_GRANT)) {
        consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
        consumeKeyword(TokenType::KW_FOR, "Expected FOR");
    }

    uint32_t privileges = 0;
    bool has_column_list = false;
    std::vector<std::string> column_names;

    auto add_privilege = [&](core::CatalogManager::Privilege priv) {
        privileges |= static_cast<uint32_t>(priv);
    };

    if (matchKeyword(TokenType::KW_ALL)) {
        matchKeyword(TokenType::KW_PRIVILEGES);
        privileges = static_cast<uint32_t>(core::CatalogManager::Privilege::ALL);
    } else {
        do {
            if (matchKeyword(TokenType::KW_SELECT)) add_privilege(core::CatalogManager::Privilege::SELECT);
            else if (matchKeyword(TokenType::KW_INSERT)) add_privilege(core::CatalogManager::Privilege::INSERT);
            else if (matchKeyword(TokenType::KW_UPDATE)) add_privilege(core::CatalogManager::Privilege::UPDATE);
            else if (matchKeyword(TokenType::KW_DELETE)) add_privilege(core::CatalogManager::Privilege::DELETE);
            else if (matchIdentifierKeyword("EXECUTE")) add_privilege(core::CatalogManager::Privilege::EXECUTE);
            else if (matchKeyword(TokenType::KW_REFERENCES)) add_privilege(core::CatalogManager::Privilege::REFERENCES);
            else if (matchKeyword(TokenType::KW_TRIGGER)) add_privilege(core::CatalogManager::Privilege::TRIGGER);
            else if (matchKeyword(TokenType::KW_TRUNCATE)) add_privilege(core::CatalogManager::Privilege::TRUNCATE);
            else if (matchKeyword(TokenType::KW_USAGE)) add_privilege(core::CatalogManager::Privilege::USAGE);
            else if (matchKeyword(TokenType::KW_CREATE)) add_privilege(core::CatalogManager::Privilege::CREATE);
            else if (matchIdentifierKeyword("CONNECT")) add_privilege(core::CatalogManager::Privilege::CONNECT);
            else if (matchKeyword(TokenType::KW_TEMPORARY)) add_privilege(core::CatalogManager::Privilege::TEMPORARY);
            else if (matchIdentifierKeyword("COPY")) add_privilege(core::CatalogManager::Privilege::COPY_FILE);
            else {
                error("Unsupported REVOKE privilege");
                break;
            }

            if (match(TokenType::LEFT_PAREN)) {
                has_column_list = true;
                do {
                    column_names.push_back(parseIdentifier());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
        } while (match(TokenType::COMMA));
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");

    core::CatalogManager::PermissionObjectType object_type =
        core::CatalogManager::PermissionObjectType::TABLE;
    if (matchKeyword(TokenType::KW_TABLE)) {
        object_type = core::CatalogManager::PermissionObjectType::TABLE;
    } else if (matchIdentifierKeyword("SEQUENCE")) {
        object_type = core::CatalogManager::PermissionObjectType::SEQUENCE;
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        object_type = core::CatalogManager::PermissionObjectType::FUNCTION;
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        object_type = core::CatalogManager::PermissionObjectType::PROCEDURE;
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        object_type = core::CatalogManager::PermissionObjectType::SCHEMA;
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        object_type = core::CatalogManager::PermissionObjectType::VIEW;
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        object_type = core::CatalogManager::PermissionObjectType::DATABASE;
    } else if (matchKeyword(TokenType::KW_ALL)) {
        error("REVOKE ON ALL ... is not supported in current bytecode");
    }

    std::string object_name = parseQualifiedName();
    if (match(TokenType::COMMA)) {
        error("REVOKE supports a single object in current bytecode");
        parseQualifiedName();
    }

    consumeKeyword(TokenType::KW_FROM, "Expected FROM");

    core::CatalogManager::GranteeType grantee_type = core::CatalogManager::GranteeType::USER;
    std::string grantee_name;
    if (matchKeyword(TokenType::KW_PUBLIC)) {
        grantee_type = core::CatalogManager::GranteeType::PUBLIC;
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        grantee_type = core::CatalogManager::GranteeType::ROLE;
        grantee_name = parseIdentifier();
    } else if (matchIdentifierKeyword("GROUP")) {
        grantee_type = core::CatalogManager::GranteeType::GROUP;
        grantee_name = parseIdentifier();
    } else if (check(TokenType::STRING_LITERAL)) {
        grantee_name = std::string(lexer_.stringPool().get(current_token_.value.string_id));
        advance();
    } else {
        grantee_name = parseIdentifier();
    }
    if (match(TokenType::COMMA)) {
        error("REVOKE supports a single grantee in current bytecode");
        parseIdentifier();
    }

    bool cascade = false;
    if (matchIdentifierKeyword("CASCADE")) {
        cascade = true;
    } else if (matchIdentifierKeyword("RESTRICT")) {
        cascade = false;
    }

    uint8_t flags = 0;
    if (cascade) flags |= 0x01;
    if (has_column_list) flags |= 0x02;

    emitU32(privileges);
    emitByte(static_cast<uint8_t>(object_type));
    emitString(object_name);
    emitByte(static_cast<uint8_t>(grantee_type));
    emitString(grantee_name);
    emitByte(flags);
    if (has_column_list) {
        emitU32(static_cast<uint32_t>(column_names.size()));
        for (const auto& name : column_names) {
            emitString(name);
        }
    }
}

void Parser::parseExplainStmt() {
    consume(TokenType::KW_EXPLAIN, "Expected EXPLAIN");

    emit(sblr::Opcode::EXPLAIN_PLAN);

    uint8_t options = 0;
    if (matchKeyword(TokenType::KW_ANALYZE)) {
        options |= 1;
    }
    if (matchIdentifierKeyword("VERBOSE")) {
        options |= 2;
    }

    if (match(TokenType::LEFT_PAREN)) {
        while (!check(TokenType::RIGHT_PAREN)) {
            if (matchKeyword(TokenType::KW_ANALYZE)) {
                options |= 1;
            } else if (matchIdentifierKeyword("VERBOSE")) {
                options |= 2;
            } else if (matchIdentifierKeyword("COSTS")) {
                options |= 4;
            } else if (matchIdentifierKeyword("BUFFERS")) {
                options |= 8;
            } else if (matchIdentifierKeyword("TIMING")) {
                options |= 16;
            } else if (matchIdentifierKeyword("FORMAT")) {
                parseIdentifier();
            } else {
                parseIdentifier();
            }

            if (matchKeyword(TokenType::KW_TRUE) || matchKeyword(TokenType::KW_ON)) {
                // Option enabled (already set)
            } else if (matchKeyword(TokenType::KW_FALSE) || matchIdentifierKeyword("OFF")) {
                // Option disabled - ignored for now
            }

            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    emitByte(options);
    parseStatementInternal();
}

// ============================================================================
// Dynamic SQL (PREPARE/EXECUTE/DEALLOCATE)
// ============================================================================

void Parser::parsePrepareStmt() {
    std::string statement_name = parseIdentifier();
    consumeKeyword(TokenType::KW_FROM, "Expected FROM after PREPARE name");

    auto emit_expression_payload = [&]() {
        std::vector<uint8_t> expr_bytes;
        std::vector<uint8_t> saved;
        saved.swap(bytecode_);
        parseExpression();
        expr_bytes.swap(bytecode_);
        bytecode_.swap(saved);

        emitU32(static_cast<uint32_t>(expr_bytes.size()));
        for (uint8_t byte : expr_bytes) {
            emitByte(byte);
        }
    };

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_PREPARE_STMT));
    emitString(statement_name);
    emit_expression_payload();
}

void Parser::parseExecuteStmt() {
    std::string statement_name = parseIdentifier();

    std::vector<std::vector<uint8_t>> parameters;
    if (matchIdentifierKeyword("USING")) {
        do {
            std::vector<uint8_t> expr_bytes;
            std::vector<uint8_t> saved;
            saved.swap(bytecode_);
            parseExpression();
            expr_bytes.swap(bytecode_);
            bytecode_.swap(saved);
            parameters.push_back(std::move(expr_bytes));
        } while (match(TokenType::COMMA));
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_EXECUTE_PREPARED));
    emitString(statement_name);
    emitU32(static_cast<uint32_t>(parameters.size()));
    for (const auto& expr_bytes : parameters) {
        emitU32(static_cast<uint32_t>(expr_bytes.size()));
        for (uint8_t byte : expr_bytes) {
            emitByte(byte);
        }
    }
}

void Parser::parseDeallocateStmt() {
    if (matchIdentifierKeyword("PREPARE")) {
        // optional keyword
    }

    std::string statement_name;
    if (matchIdentifierKeyword("ALL")) {
        statement_name.clear();
    } else {
        statement_name = parseIdentifier();
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DEALLOCATE_PREPARED));
    emitString(statement_name);
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
        server_root = "remote.emulation.mysql.localhost";
    }
    default_schema_ = server_root + ".databases." + db;

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
    bool has_autocommit = false;
    bool has_conflict_error_code = false;
    uint8_t access_mode = 0;  // 0=READ WRITE, 1=READ ONLY
    sblr::AutocommitMode autocommit_mode = sblr::AutocommitMode::UNCHANGED;
    auto conflict_action = sblr::TransactionConflictAction::DEFAULT;
    int32_t conflict_error_code = 0;

    // Transaction characteristics
    while (true) {
        if (matchKeyword(TokenType::KW_ON)) {
            error("MySQL does not support ON CONFLICT in START TRANSACTION");
            synchronize();
            return;
        } else if (matchIdentifierKeyword("AUTOCOMMIT")) {
            error("MySQL does not support AUTOCOMMIT in START TRANSACTION");
            synchronize();
            return;
        } else if (matchKeyword(TokenType::KW_READ)) {
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
    if (has_autocommit) flags |= sblr::TransactionFlags::HAS_AUTOCOMMIT;
    if (has_conflict_error_code) flags |= sblr::TransactionFlags::HAS_CONFLICT_ERROR_CODE;
    emitU16(flags);
    emitByte(static_cast<uint8_t>(conflict_action));
    if (has_conflict_error_code) {
        emitU32(static_cast<uint32_t>(conflict_error_code));
    }
    if (has_autocommit) {
        emitByte(static_cast<uint8_t>(autocommit_mode));
    }
    if (has_access_mode) emitByte(access_mode);
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

    enum class LockType : uint8_t {
        READ = 0,
        WRITE = 1
    };

    struct LockTarget {
        std::string table_path;
        LockType type;
    };

    std::vector<LockTarget> targets;

    while (true) {
        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        resolveTableName(schema, table);

        if (matchKeyword(TokenType::KW_AS)) {
            parseIdentifier();
        } else if (check(TokenType::IDENTIFIER)) {
            std::string_view text = lexer_.stringPool().get(current_token_.value.string_id);
            std::string upper(text);
            std::transform(upper.begin(), upper.end(), upper.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
            if (upper != "READ" && upper != "WRITE" && upper != "LOCAL" && upper != "LOW_PRIORITY") {
                parseIdentifier();  // alias
            }
        }

        LockType lock_type = LockType::READ;
        if (matchKeyword(TokenType::KW_READ)) {
            matchKeyword(TokenType::KW_LOCAL);
            lock_type = LockType::READ;
        } else if (matchKeyword(TokenType::KW_LOW_PRIORITY)) {
            consumeKeyword(TokenType::KW_WRITE, "Expected WRITE after LOW_PRIORITY");
            lock_type = LockType::WRITE;
        } else if (matchKeyword(TokenType::KW_WRITE)) {
            lock_type = LockType::WRITE;
        } else {
            error("LOCK TABLES requires READ or WRITE");
            synchronize();
            return;
        }

        targets.push_back({schema.empty() ? table : schema + "/" + table, lock_type});

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MYSQL_LOCK_TABLES));
    emitU32(static_cast<uint32_t>(targets.size()));
    for (const auto& target : targets) {
        emitString(target.table_path);
        emitByte(static_cast<uint8_t>(target.type));
    }
}

void Parser::parseUnlockStmt() {
    consume(TokenType::KW_UNLOCK, "Expected UNLOCK");
    consumeKeyword(TokenType::KW_TABLES, "Expected TABLES");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MYSQL_UNLOCK_TABLES));
}

void Parser::parseSubquery() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_SCALAR));
    parseSelectStmt();
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SUBQUERY_END));
}

} // namespace scratchbird::parser::mysql
