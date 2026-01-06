/**
 * PostgreSQL Parser Implementation - Core
 *
 * Recursive-descent parser for PostgreSQL 16 SQL that generates SBLR bytecode
 * directly for execution by the ScratchBird engine.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include <cctype>
#include <cstring>
#include <algorithm>
#include <stdexcept>

namespace scratchbird::parser::postgresql {

// ============================================================================
// Helper Functions for Non-Reserved Keywords
// ============================================================================

// Check if a keyword token can be used as an identifier in expression context
// PostgreSQL has ~96 reserved keywords but many can be used as column names
bool Parser::isNonReservedKeyword(TokenType type) const {
    switch (type) {
        // Common column names that are keywords in some contexts
        case TokenType::KW_NAME:
        case TokenType::KW_VALUE:
        case TokenType::KW_TYPE:
        case TokenType::KW_DATA:
        case TokenType::KW_TIME:
        case TokenType::KW_TIMESTAMP:
        case TokenType::KW_DATE:
        case TokenType::KW_YEAR:
        case TokenType::KW_MONTH:
        case TokenType::KW_DAY:
        case TokenType::KW_HOUR:
        case TokenType::KW_MINUTE:
        case TokenType::KW_SECOND:
        case TokenType::KW_ZONE:
        case TokenType::KW_OPTIONS:
        case TokenType::KW_COMMENT:
        case TokenType::KW_LANGUAGE:
        case TokenType::KW_VERSION:
        case TokenType::KW_ROLE:
        case TokenType::KW_ACTION:
        case TokenType::KW_LEVEL:
        case TokenType::KW_MODE:
        case TokenType::KW_FIRST:
        case TokenType::KW_LAST:
        case TokenType::KW_NEXT:
        case TokenType::KW_PRIOR:
        case TokenType::KW_ABSOLUTE:
        case TokenType::KW_RELATIVE:
            return true;
        default:
            return false;
    }
}

// Convert token type to string name for column reference
static std::string tokenToString(TokenType type) {
    switch (type) {
        case TokenType::KW_NAME: return "name";
        case TokenType::KW_VALUE: return "value";
        case TokenType::KW_TYPE: return "type";
        case TokenType::KW_DATA: return "data";
        case TokenType::KW_TIME: return "time";
        case TokenType::KW_TIMESTAMP: return "timestamp";
        case TokenType::KW_DATE: return "date";
        case TokenType::KW_YEAR: return "year";
        case TokenType::KW_MONTH: return "month";
        case TokenType::KW_DAY: return "day";
        case TokenType::KW_HOUR: return "hour";
        case TokenType::KW_MINUTE: return "minute";
        case TokenType::KW_SECOND: return "second";
        case TokenType::KW_ZONE: return "zone";
        case TokenType::KW_OPTIONS: return "options";
        case TokenType::KW_COMMENT: return "comment";
        case TokenType::KW_LANGUAGE: return "language";
        case TokenType::KW_VERSION: return "version";
        case TokenType::KW_ROLE: return "role";
        case TokenType::KW_ACTION: return "action";
        case TokenType::KW_LEVEL: return "level";
        case TokenType::KW_MODE: return "mode";
        case TokenType::KW_FIRST: return "first";
        case TokenType::KW_LAST: return "last";
        case TokenType::KW_NEXT: return "next";
        case TokenType::KW_PRIOR: return "prior";
        case TokenType::KW_ABSOLUTE: return "absolute";
        case TokenType::KW_RELATIVE: return "relative";
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
            case TokenType::KW_PREPARE:
            case TokenType::KW_COMMIT:
            case TokenType::KW_ROLLBACK:
            case TokenType::KW_SET:
            case TokenType::KW_SHOW:
            case TokenType::KW_GRANT:
            case TokenType::KW_REVOKE:
            case TokenType::KW_TRUNCATE:
            case TokenType::KW_ANALYZE:
            case TokenType::KW_EXPLAIN:
            case TokenType::KW_COPY:
            case TokenType::KW_WITH:
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
    // Handle WITH clause (CTE) for SELECT/INSERT/UPDATE/DELETE
    if (check(TokenType::KW_WITH)) {
        parseWithClause();
        // After WITH, must be SELECT, INSERT, UPDATE, or DELETE
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
        case TokenType::KW_MERGE:
            parseMergeStmt();
            break;
        case TokenType::KW_CREATE:
            parseCreateStmt();
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
        case TokenType::KW_BEGIN:
            parseBeginStmt();
            break;
        case TokenType::KW_PREPARE:
            parsePrepareStmt();
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
        case TokenType::KW_GRANT:
            parseGrantStmt();
            break;
        case TokenType::KW_REVOKE:
            parseRevokeStmt();
            break;
        case TokenType::KW_ANALYZE:
            parseAnalyzeStmt();
            break;
        case TokenType::KW_EXPLAIN:
            parseExplainStmt();
            break;
        case TokenType::KW_COPY:
            parseCopyStmt();
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
    if (check(TokenType::QUOTED_IDENTIFIER)) {
        uint32_t id = current_token_.value.string_id;
        advance();
        return std::string(lexer_.stringPool().get(id));
    }
    // Allow non-reserved keywords as identifiers
    if (isNonReservedKeyword(current_token_.type)) {
        std::string name = tokenToString(current_token_.type);
        advance();
        return name;
    }
    error("Expected identifier");
    return "";
}

std::string Parser::parseQualifiedName() {
    std::string name = parseIdentifier();
    int parts = 1;

    while (match(TokenType::DOT)) {
        if (parts >= 2) {
            error("PostgreSQL qualified names must be schema.object");
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
    if (normalized_schema.rfind("remote.emulated.postgresql.", 0) == 0 ||
        normalized_schema == "remote.emulated.postgresql")
    {
        schema = normalized_schema;
        return;
    }

    if (!normalized_default.empty()) {
        schema = normalized_default + "." + normalized_schema;
    } else {
        schema = normalized_schema;
    }
}

// ============================================================================
// Type Conversion Helper
// ============================================================================

sblr::Opcode Parser::typeToOpcode(PgDataType::Kind kind) {
    switch (kind) {
        case PgDataType::Kind::SMALLINT:
        case PgDataType::Kind::INTEGER:
        case PgDataType::Kind::SERIAL:
        case PgDataType::Kind::SMALLSERIAL:
            return sblr::Opcode::TYPE_INTEGER;
        case PgDataType::Kind::BIGINT:
        case PgDataType::Kind::BIGSERIAL:
            return sblr::Opcode::TYPE_BIGINT;
        case PgDataType::Kind::REAL:
        case PgDataType::Kind::DOUBLE_PRECISION:
            return sblr::Opcode::TYPE_DOUBLE;
        case PgDataType::Kind::DECIMAL:
        case PgDataType::Kind::NUMERIC:
        case PgDataType::Kind::MONEY:
            return sblr::Opcode::TYPE_DECIMAL;
        case PgDataType::Kind::CHAR:
            return sblr::Opcode::TYPE_CHAR;
        case PgDataType::Kind::VARCHAR:
            return sblr::Opcode::TYPE_VARCHAR;
        case PgDataType::Kind::TEXT:
            return sblr::Opcode::TYPE_TEXT;
        case PgDataType::Kind::BYTEA:
            return sblr::Opcode::TYPE_BYTEA;
        case PgDataType::Kind::DATE:
            return sblr::Opcode::TYPE_DATE;
        case PgDataType::Kind::TIME:
        case PgDataType::Kind::TIMETZ:
            return sblr::Opcode::TYPE_TIME;
        case PgDataType::Kind::TIMESTAMP:
        case PgDataType::Kind::TIMESTAMPTZ:
            return sblr::Opcode::TYPE_TIMESTAMP;
        case PgDataType::Kind::BOOLEAN:
            return sblr::Opcode::TYPE_BOOLEAN;
        case PgDataType::Kind::UUID:
            return sblr::Opcode::TYPE_UUID;
        case PgDataType::Kind::JSON:
        case PgDataType::Kind::JSONB:
            return sblr::Opcode::TYPE_JSON;
        default:
            return sblr::Opcode::TYPE_VARCHAR;  // Default fallback
    }
}

void Parser::emitTypeDefinition(const PgDataType& type) {
    // See docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md for SBLR type encoding.
    if (type.kind == PgDataType::Kind::INT128 ||
        type.kind == PgDataType::Kind::UINT128) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(
            type.kind == PgDataType::Kind::INT128
                ? sblr::ExtendedOpcode::EXT_TYPE_INT128
                : sblr::ExtendedOpcode::EXT_TYPE_UINT128));
        return;
    }

    emit(typeToOpcode(type.kind));
    switch (type.kind) {
        case PgDataType::Kind::CHAR:
        case PgDataType::Kind::VARCHAR:
        case PgDataType::Kind::BIT:
        case PgDataType::Kind::VARBIT:
            emitU32(type.length > 0 ? static_cast<uint32_t>(type.length) : 255);
            break;
        case PgDataType::Kind::DECIMAL:
        case PgDataType::Kind::NUMERIC:
        case PgDataType::Kind::MONEY:
            emitU32(type.precision > 0 ? static_cast<uint32_t>(type.precision) : 18);
            emitU32(static_cast<uint32_t>(type.scale));
            break;
        default:
            break;
    }
}

} // namespace scratchbird::parser::postgresql
