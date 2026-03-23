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
#include <array>

#if __has_include(<openssl/sha.h>)
#include <openssl/sha.h>
#define SCRATCHBIRD_PG_PARSER_HAS_OPENSSL_SHA1 1
#endif

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
        case TokenType::KW_PUBLIC:
        case TokenType::KW_OPTIONS:
        case TokenType::KW_COMMENT:
        case TokenType::KW_LANGUAGE:
        case TokenType::KW_PLPGSQL:
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
        case TokenType::KW_LOCATION:
        case TokenType::KW_EXPLAIN:
        case TokenType::KW_ANY:
        case TokenType::KW_SOME:
        case TokenType::KW_OID:
        case TokenType::KW_TEXT:
        case TokenType::KW_INT2:
        case TokenType::KW_INT4:
        case TokenType::KW_INT8:
        case TokenType::KW_SMALLINT:
        case TokenType::KW_INTEGER:
        case TokenType::KW_BIGINT:
        case TokenType::KW_REGCLASS:
        case TokenType::KW_REGTYPE:
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
        case TokenType::KW_PLPGSQL: return "plpgsql";
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
        case TokenType::KW_LOCATION: return "location";
        case TokenType::KW_EXPLAIN: return "explain";
        case TokenType::KW_ANY: return "any";
        case TokenType::KW_SOME: return "some";
        case TokenType::KW_OID: return "oid";
        case TokenType::KW_TEXT: return "text";
        case TokenType::KW_INT2: return "int2";
        case TokenType::KW_INT4: return "int4";
        case TokenType::KW_INT8: return "int8";
        case TokenType::KW_SMALLINT: return "smallint";
        case TokenType::KW_INTEGER: return "integer";
        case TokenType::KW_BIGINT: return "bigint";
        case TokenType::KW_REGCLASS: return "regclass";
        case TokenType::KW_REGTYPE: return "regtype";
        default: return "";
    }
}

std::string toUpperAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return value;
}

int hexNibble(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return 10 + (ch - 'a');
    }
    if (ch >= 'A' && ch <= 'F') {
        return 10 + (ch - 'A');
    }
    return -1;
}

bool parseUuidLiteral(std::string_view literal, core::ID& out) {
    if (literal.size() != 36) {
        return false;
    }

    static constexpr std::array<size_t, 4> kDashPos{8, 13, 18, 23};
    for (size_t dash : kDashPos) {
        if (literal[dash] != '-') {
            return false;
        }
    }

    size_t out_idx = 0;
    for (size_t i = 0; i < literal.size();) {
        if (literal[i] == '-') {
            ++i;
            continue;
        }
        if (i + 1 >= literal.size() || out_idx >= out.bytes.size()) {
            return false;
        }
        const int hi = hexNibble(literal[i]);
        const int lo = hexNibble(literal[i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out.bytes[out_idx++] = static_cast<uint8_t>((hi << 4) | lo);
        i += 2;
    }
    return out_idx == out.bytes.size();
}

const core::ID& postgresqlDomainIdentityNamespaceUuid() {
    static const core::ID kNamespace = [] {
        core::ID id{};
        const bool ok = parseUuidLiteral("1cc0f343-1406-4d4c-a38d-a94535b536eb", id);
        if (!ok) {
            return core::ID{};
        }
        return id;
    }();
    return kNamespace;
}

std::string canonicalPostgresqlDomainIdentityKey(const std::string& domain_name) {
    return "domain|global|" + toUpperAscii(domain_name);
}

core::ID deterministicPostgresqlDomainId(const std::string& domain_name) {
    const core::ID& ns_uuid = postgresqlDomainIdentityNamespaceUuid();
    const std::string key = canonicalPostgresqlDomainIdentityKey(domain_name);
    core::ID out{};

#ifdef SCRATCHBIRD_PG_PARSER_HAS_OPENSSL_SHA1
    unsigned char hash[SHA_DIGEST_LENGTH];
    std::string namespaced;
    namespaced.reserve(ns_uuid.bytes.size() + key.size());
    for (uint8_t byte : ns_uuid.bytes) {
        namespaced.push_back(static_cast<char>(byte));
    }
    namespaced.append(key);
    SHA1(reinterpret_cast<const unsigned char*>(namespaced.data()),
         namespaced.size(),
         hash);

    for (size_t i = 0; i < out.bytes.size(); ++i) {
        out.bytes[i] = hash[i];
    }
#else
    auto fnv1a64 = [](std::string_view input, uint64_t seed) -> uint64_t {
        uint64_t hash = 1469598103934665603ULL ^ seed;
        for (unsigned char ch : input) {
            hash ^= static_cast<uint64_t>(ch);
            hash *= 1099511628211ULL;
        }
        return hash;
    };

    std::string namespaced;
    namespaced.reserve(ns_uuid.bytes.size() + key.size());
    for (uint8_t byte : ns_uuid.bytes) {
        namespaced.push_back(static_cast<char>(byte));
    }
    namespaced.append(key);

    uint64_t hi = fnv1a64(namespaced, 0x9ae16a3b2f90404fULL);
    uint64_t lo = fnv1a64(namespaced, 0xc3a5c85c97cb3127ULL);
    for (size_t i = 0; i < 8; ++i) {
        out.bytes[i] = static_cast<uint8_t>((hi >> ((7 - i) * 8)) & 0xFF);
        out.bytes[8 + i] = static_cast<uint8_t>((lo >> ((7 - i) * 8)) & 0xFF);
    }
#endif

    out.bytes[6] = static_cast<uint8_t>((out.bytes[6] & 0x0F) | 0x50);
    out.bytes[8] = static_cast<uint8_t>((out.bytes[8] & 0x3F) | 0x80);
    return out;
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
    arena_ = std::make_unique<parser::v3::ASTArena>();
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

void Parser::emitUUID(const core::ID& uuid) {
    if (!emit_enabled_) {
        return;
    }
    for (uint8_t byte : uuid.bytes) {
        bytecode_.push_back(byte);
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
    statement_ = nullptr;
    string_pool_.clear();
    arena_ = std::make_unique<parser::v3::ASTArena>();

    // Emit parser-side bytecode header for parser diagnostics paths.
    emit(sblr::Opcode::VERSION);
    emitByte(sblr::SBLR_VERSION);

    try {
        statement_ = parseStatementInternal();

        if (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
            error("Unexpected trailing tokens after statement");
            while (!check(TokenType::END_OF_FILE) && !check(TokenType::SEMICOLON)) {
                advance();
            }
        }
    } catch (const std::exception& e) {
        error(e.what());
    }

    emit(sblr::Opcode::END);

    ParseResult result;
    if (!errors_.empty()) {
        for (const auto& err : errors_) {
            result.addError(err.message, err.location);
        }
    } else {
        result.setStatement(statement_);
        result.setArena(std::move(arena_));
        result.setBytecode(std::move(bytecode_));
        result.stringPool() = string_pool_;
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

parser::v3::Statement* Parser::parseStatementInternal() {
    parser::v3::WithClause* with = nullptr;
    // Handle WITH clause (CTE) for SELECT/INSERT/UPDATE/DELETE
    if (check(TokenType::KW_WITH)) {
        with = parseWithClause();
        // After WITH, must be SELECT, INSERT, UPDATE, or DELETE
    }

    emitDebugSpan(current_token_.span);

    if (check(TokenType::ERROR)) {
        std::string_view input = lexer_.input();
        size_t start = current_token_.span.start.offset;
        bool is_psql_meta = false;
        size_t meta_start = start;
        if (start < input.size() && input[start] == '\\') {
            is_psql_meta = true;
            meta_start = start;
        } else if (start > 0 && input[start - 1] == '\\') {
            is_psql_meta = true;
            meta_start = start - 1;
        }

        if (is_psql_meta) {
            uint32_t meta_line = current_token_.span.start.line;
            Token last = current_token_;
            while (!check(TokenType::END_OF_FILE) &&
                   current_token_.span.start.line == meta_line) {
                last = current_token_;
                advance();
            }

            size_t end = last.span.start.offset + last.span.length;
            if (end > input.size()) {
                end = input.size();
            }
            std::string payload;
            if (end > meta_start) {
                payload = std::string(input.substr(meta_start, end - meta_start));
            }

            auto* stmt = arena()->create<parser::v3::AlterSystemStmt>();
            stmt->name = string_pool_.intern("postgresql.compat.psql_meta");
            auto* lit = arena()->create<parser::v3::LiteralExpr>();
            lit->literal_type = parser::v3::LiteralType::STRING;
            lit->string_value = string_pool_.intern(payload);
            stmt->value = lit;
            return stmt;
        }
    }

    if (check(TokenType::IDENTIFIER)) {
        auto capture_remaining_clause = [&]() -> std::string {
            if (check(TokenType::SEMICOLON) || check(TokenType::END_OF_FILE)) {
                return {};
            }

            std::string_view input = lexer_.input();
            size_t start = current_token_.span.start.offset;
            size_t end = start;
            Token last = current_token_;
            while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
                last = current_token_;
                advance();
            }
            end = last.span.start.offset + last.span.length;
            if (end > input.size()) {
                end = input.size();
            }
            if (end <= start) {
                return {};
            }
            return std::string(input.substr(start, end - start));
        };

        auto consume_remaining_clause = [&]() {
            while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
                advance();
            }
        };

        auto make_alter_system_stmt =
            [&](const std::string& key, const std::string& payload) -> parser::v3::AlterSystemStmt* {
                auto* stmt = arena()->create<parser::v3::AlterSystemStmt>();
                stmt->name = string_pool_.intern(key);
                auto* lit = arena()->create<parser::v3::LiteralExpr>();
                lit->literal_type = parser::v3::LiteralType::STRING;
                lit->string_value = string_pool_.intern(payload);
                stmt->value = lit;
                return stmt;
            };

        if (matchIdentifierKeyword("CHECKPOINT")) {
            if (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
                error("CHECKPOINT does not accept trailing clauses");
                consume_remaining_clause();
                return nullptr;
            }
            return arena()->create<parser::v3::SweepDatabaseStmt>();
        }

        if (matchIdentifierKeyword("LOAD")) {
            bool if_not_exists = false;
            if (matchKeyword(TokenType::KW_IF)) {
                consumeKeyword(TokenType::KW_NOT, "Expected NOT after IF");
                consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
                if_not_exists = true;
            }

            // Accept both LOAD EXTENSION <name> and LOAD <name>.
            matchIdentifierKeyword("EXTENSION");

            std::string extension_name;
            if (check(TokenType::STRING_LITERAL)) {
                extension_name = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            } else {
                extension_name = parseIdentifier();
            }
            std::string payload = capture_remaining_clause();
            if (if_not_exists) {
                if (!payload.empty()) {
                    payload.insert(0, ";");
                }
                payload.insert(0, "IF_NOT_EXISTS=1");
            }
            return make_alter_system_stmt("platform.extension.load." + extension_name, payload);
        }

        if (matchIdentifierKeyword("CLUSTER")) {
            std::string payload = capture_remaining_clause();
            return make_alter_system_stmt("maintenance.cluster", payload);
        }

        if (matchIdentifierKeyword("REFRESH")) {
            std::string payload = capture_remaining_clause();
            return make_alter_system_stmt("postgresql.compat.refresh", payload);
        }

        if (matchIdentifierKeyword("REINDEX")) {
            std::string payload = capture_remaining_clause();
            return make_alter_system_stmt("postgresql.compat.reindex", payload);
        }

        if (matchIdentifierKeyword("DISCARD")) {
            std::string payload = capture_remaining_clause();
            return make_alter_system_stmt("postgresql.compat.discard", payload);
        }

        if (matchIdentifierKeyword("PREPARE")) {
            std::string payload = capture_remaining_clause();
            return make_alter_system_stmt("postgresql.compat.prepare", payload);
        }

        if (matchIdentifierKeyword("LISTEN")) {
            std::string payload = capture_remaining_clause();
            return make_alter_system_stmt("postgresql.compat.listen", payload);
        }

        if (matchIdentifierKeyword("WAIT")) {
            if (!matchKeyword(TokenType::KW_FOR)) {
                error("Expected FOR after WAIT");
                consume_remaining_clause();
                return nullptr;
            }
            if (!matchIdentifierKeyword("LSN")) {
                error("Expected LSN after WAIT FOR");
                consume_remaining_clause();
                return nullptr;
            }

            std::string lsn_value;
            if (check(TokenType::STRING_LITERAL)) {
                lsn_value = std::string(lexer_.stringPool().get(current_token_.value.string_id));
                advance();
            } else if (check(TokenType::IDENTIFIER) ||
                       check(TokenType::QUOTED_IDENTIFIER) ||
                       isNonReservedKeyword(current_token_.type)) {
                lsn_value = parseIdentifier();
            } else {
                error("Expected LSN value after WAIT FOR LSN");
                consume_remaining_clause();
                return nullptr;
            }

            std::string payload = "LSN=" + lsn_value;
            std::string trailing = capture_remaining_clause();
            if (!trailing.empty()) {
                payload += ";" + trailing;
            }
            return make_alter_system_stmt("admin.wait_for_lsn", payload);
        }
    }

    switch (current_token_.type) {
        case TokenType::KW_SELECT:
            {
                auto* stmt = parseSelectStmt();
                if (with) stmt->with = with;
                return stmt;
            }
        case TokenType::KW_INSERT:
            {
                auto* stmt = parseInsertStmt();
                if (with) stmt->with = with;
                return stmt;
            }
        case TokenType::KW_UPDATE:
            {
                auto* stmt = parseUpdateStmt();
                if (with) stmt->with = with;
                return stmt;
            }
        case TokenType::KW_DELETE:
            {
                auto* stmt = parseDeleteStmt();
                if (with) stmt->with = with;
                return stmt;
            }
        case TokenType::KW_MERGE:
            return parseMergeStmt();
        case TokenType::KW_CALL:
            return parseCallStmtV3();
        case TokenType::KW_DO:
            return parseDoStmtV3();
        case TokenType::KW_DECLARE:
            {
                consume(TokenType::KW_DECLARE, "Expected DECLARE");
                auto* stmt = arena()->create<parser::v3::AlterSystemStmt>();
                stmt->name = string_pool_.intern("postgresql.compat.declare");
                auto* lit = arena()->create<parser::v3::LiteralExpr>();
                lit->literal_type = parser::v3::LiteralType::STRING;

                std::string payload;
                if (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
                    std::string_view input = lexer_.input();
                    size_t start = current_token_.span.start.offset;
                    size_t end = start;
                    Token last = current_token_;
                    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
                        last = current_token_;
                        advance();
                    }
                    end = last.span.start.offset + last.span.length;
                    if (end > input.size()) {
                        end = input.size();
                    }
                    if (end > start) {
                        payload = std::string(input.substr(start, end - start));
                    }
                }

                lit->string_value = string_pool_.intern(payload);
                stmt->value = lit;
                return stmt;
            }
        case TokenType::KW_CREATE: {
            consume(TokenType::KW_CREATE, "Expected CREATE");
            parser::v3::Statement* stmt = parseCreateStmtV3();
            if (stmt) {
                return stmt;
            }
            error("Unsupported CREATE statement for V3 PostgreSQL parser");
            return nullptr;
        }
        case TokenType::KW_ALTER: {
            consume(TokenType::KW_ALTER, "Expected ALTER");
            parser::v3::Statement* stmt = parseAlterStmtV3();
            if (stmt) {
                return stmt;
            }
            error("Unsupported ALTER statement for V3 PostgreSQL parser");
            return nullptr;
        }
        case TokenType::KW_DROP: {
            consume(TokenType::KW_DROP, "Expected DROP");
            parser::v3::Statement* stmt = parseDropStmtV3();
            if (stmt) {
                return stmt;
            }
            error("Unsupported DROP statement for V3 PostgreSQL parser");
            return nullptr;
        }
        case TokenType::KW_TRUNCATE: {
            consume(TokenType::KW_TRUNCATE, "Expected TRUNCATE");
            parser::v3::Statement* stmt = parseTruncateStmtV3();
            if (stmt) {
                return stmt;
            }
            error("Unsupported TRUNCATE statement for V3 PostgreSQL parser");
            return nullptr;
        }
        case TokenType::KW_SET:
            {
                parser::v3::Statement* stmt = parseSetStmtV3();
                if (stmt) return stmt;
                error("Unsupported SET statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_RESET:
            {
                parser::v3::Statement* stmt = parseResetStmtV3();
                if (stmt) return stmt;
                error("Unsupported RESET statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_SHOW:
            {
                parser::v3::Statement* stmt = parseShowStmtV3();
                if (stmt) return stmt;
                error("Unsupported SHOW statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_BEGIN:
            {
                parser::v3::Statement* stmt = parseBeginStmtV3();
                if (stmt) return stmt;
                error("Unsupported BEGIN statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_START:
            {
                parser::v3::Statement* stmt = parseBeginStmtV3();
                if (stmt) return stmt;
                error("Unsupported START statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_PREPARE:
            {
                parser::v3::Statement* stmt = parsePrepareStmtV3();
                if (stmt) return stmt;
                error("Unsupported PREPARE statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_EXECUTE:
            return parseExecutePreparedStmtV3();
        case TokenType::KW_DEALLOCATE:
            return parseDeallocateStmtV3();
        case TokenType::KW_COMMIT:
            {
                parser::v3::Statement* stmt = parseCommitStmtV3();
                if (stmt) return stmt;
                error("Unsupported COMMIT statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_ROLLBACK:
            {
                parser::v3::Statement* stmt = parseRollbackStmtV3();
                if (stmt) return stmt;
                error("Unsupported ROLLBACK statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_SAVEPOINT:
            {
                parser::v3::Statement* stmt = parseSavepointStmtV3();
                if (stmt) return stmt;
                error("Unsupported SAVEPOINT statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_RELEASE:
            {
                parser::v3::Statement* stmt = parseReleaseStmtV3();
                if (stmt) return stmt;
                error("Unsupported RELEASE statement for V3 PostgreSQL parser");
                return nullptr;
            }
        case TokenType::KW_CLOSE:
            return parseCloseCursorStmtV3();
        case TokenType::KW_FETCH:
        case TokenType::KW_FETCH_KW:
            return parseFetchCursorStmtV3(false);
        case TokenType::KW_MOVE:
            return parseFetchCursorStmtV3(true);
        case TokenType::KW_GRANT:
            return parseGrantStmtV3();
        case TokenType::KW_REVOKE:
            return parseRevokeStmtV3();
        case TokenType::KW_ANALYZE:
            return parseAnalyzeStmtV3();
        case TokenType::KW_REINDEX:
            return parseReindexStmtV3();
        case TokenType::KW_VACUUM:
            return parseVacuumStmtV3();
        case TokenType::KW_EXPLAIN:
            return parseExplainStmtV3();
        case TokenType::KW_COPY:
            return parseCopyStmtV3();
        case TokenType::KW_LISTEN:
            return parseListenStmtV3();
        case TokenType::KW_NOTIFY:
            return parseNotifyStmtV3();
        case TokenType::KW_UNLISTEN:
            return parseUnlistenStmtV3();
        case TokenType::KW_LOCK:
            return parseLockTableStmtV3();
        default:
            error("Expected statement");
            synchronize();
            return nullptr;
    }
}

// ============================================================================
// Identifier Parsing
// ============================================================================

parser::v3::StringPool::StringId Parser::internFromLexer(uint32_t lexer_id) {
    std::string_view text = lexer_.stringPool().get(lexer_id);
    return string_pool_.intern(text);
}

parser::v3::StringPool::StringId Parser::parseIdentifierId() {
    if (check(TokenType::IDENTIFIER)) {
        uint32_t id = current_token_.value.string_id;
        advance();
        return internFromLexer(id);
    }
    if (check(TokenType::QUOTED_IDENTIFIER)) {
        uint32_t id = current_token_.value.string_id;
        advance();
        return internFromLexer(id);
    }
    // Allow non-reserved keywords as identifiers
    if (isNonReservedKeyword(current_token_.type)) {
        std::string name = tokenToString(current_token_.type);
        advance();
        return string_pool_.intern(name);
    }
    error("Expected identifier");
    return parser::v3::StringPool::INVALID_ID;
}

std::string Parser::parseIdentifier() {
    auto id = parseIdentifierId();
    if (id == parser::v3::StringPool::INVALID_ID) {
        return "";
    }
    return std::string(string_pool_.get(id));
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

parser::v3::SchemaPath Parser::parseResolvedTablePath() {
    return buildResolvedTablePath(parseQualifiedName());
}

parser::v3::SchemaPath Parser::buildResolvedTablePath(const std::string& qualified_name) {
    auto append_components =
        [&](const std::string& value,
            std::vector<parser::v3::StringPool::StringId>& components) {
            std::string current;
            for (char ch : value) {
                if (ch == '.' || ch == '/') {
                    if (!current.empty()) {
                        components.push_back(string_pool_.intern(current));
                        current.clear();
                    }
                } else {
                    current.push_back(ch);
                }
            }
            if (!current.empty()) {
                components.push_back(string_pool_.intern(current));
            }
        };

    std::string schema;
    std::string table = qualified_name;
    const auto split = qualified_name.find_last_of('.');
    if (split != std::string::npos) {
        schema = qualified_name.substr(0, split);
        table = qualified_name.substr(split + 1);
    }

    resolveTableName(schema, table);

    std::vector<parser::v3::StringPool::StringId> components;
    append_components(schema, components);
    append_components(table, components);

    parser::v3::PathType path_type = components.size() > 1
                                         ? parser::v3::PathType::ABSOLUTE
                                         : parser::v3::PathType::UNQUALIFIED;
    return parser::v3::SchemaPath(path_type, std::move(components));
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
    auto has_hierarchy = [](const std::string& value) {
        return value.find('.') != std::string::npos ||
               value.find('/') != std::string::npos;
    };
    auto has_emulation_prefix = [](const std::string& value) {
        auto has_prefix = [&](const std::string& prefix) {
            return value == prefix || value.rfind(prefix + ".", 0) == 0;
        };
        return has_prefix("emulated.postgresql") ||
               has_prefix("emulation.postgresql") ||
               has_prefix("remote.emulated.postgresql") ||
               has_prefix("remote.emulation.postgresql");
    };
    auto equals_ci = [](const std::string& lhs, const std::string& rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (size_t i = 0; i < lhs.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
                std::tolower(static_cast<unsigned char>(rhs[i]))) {
                return false;
            }
        }
        return true;
    };
    auto last_component = [](const std::string& value) {
        size_t split = value.find_last_of("./");
        if (split == std::string::npos || split + 1 >= value.size()) {
            return value;
        }
        return value.substr(split + 1);
    };
    auto is_virtual_system_schema = [&](const std::string& value) {
        const std::string leaf = last_component(value);
        return equals_ci(leaf, "pg_catalog") || equals_ci(leaf, "information_schema");
    };
    const bool default_is_database_alias =
        !normalized_default.empty() &&
        ((!has_hierarchy(normalized_default) && !has_emulation_prefix(normalized_default)) ||
         (has_emulation_prefix(normalized_default) &&
          normalized_default.find(".databases.") != std::string::npos));

    // If no schema specified, use default
    if (schema.empty()) {
        if (default_is_database_alias) {
            // In manager-bound PostgreSQL emulation, an unqualified object name should
            // remain unqualified so execution resolves against the live current/search path.
            // Treating the bound database alias as a schema qualifier here causes DDL/DML
            // path divergence (e.g. CREATE TABLE vs INSERT into the same name).
            schema.clear();
        } else {
            schema = normalized_default;
        }
        return;
    }

    std::string normalized_schema = normalize_path(schema);
    if (has_emulation_prefix(normalized_schema))
    {
        schema = normalized_schema;
        return;
    }

    if (default_is_database_alias)
    {
        // In manager-bound emulation, explicit schema qualifiers are still
        // relative to the emulated database root.
        if (is_virtual_system_schema(normalized_schema))
        {
            schema = normalized_schema;
            return;
        }
        if (!normalized_default.empty() &&
            normalized_schema != normalized_default &&
            normalized_schema.rfind(normalized_default + ".", 0) != 0)
        {
            schema = normalized_default + "." + normalized_schema;
        }
        else
        {
            schema = normalized_schema;
        }
        return;
    }

    if (!normalized_default.empty()) {
        schema = normalized_default + "." + normalized_schema;
    } else {
        schema = normalized_schema;
    }
}

bool Parser::resolveDomainId(const std::string& type_name, core::ID& domain_id_out) {
    if (type_name.empty()) {
        error("Domain name cannot be empty");
        return false;
    }

    std::string canonical_name = type_name;
    std::replace(canonical_name.begin(), canonical_name.end(), '/', '.');
    domain_id_out = deterministicPostgresqlDomainId(canonical_name);
    return true;
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
            return sblr::Opcode::TYPE_JSON;
        case PgDataType::Kind::ARRAY:
            return sblr::Opcode::TYPE_ARRAY;
        default:
            return sblr::Opcode::TYPE_VARCHAR;  // Default fallback
    }
}

void Parser::emitTypeDefinition(const PgDataType& type) {
    // See docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md for SBLR type encoding.
    // C2: JSONPATH support - emit as extended type
    if (type.kind == PgDataType::Kind::JSONPATH) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_TYPE_JSONPATH));
        return;
    }
    auto emit_extended = [&](sblr::ExtendedOpcode opcode) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(opcode));
    };

    auto emit_extended_for_kind = [&](PgDataType::Kind kind,
                                      bool with_time_zone) -> bool {
        switch (kind) {
            case PgDataType::Kind::TIMETZ:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TIME_TZ);
                return true;
            case PgDataType::Kind::TIMESTAMPTZ:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TIMESTAMP_TZ);
                return true;
            case PgDataType::Kind::TIME:
                if (with_time_zone) {
                    emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TIME_TZ);
                    return true;
                }
                return false;
            case PgDataType::Kind::TIMESTAMP:
                if (with_time_zone) {
                    emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TIMESTAMP_TZ);
                    return true;
                }
                return false;
            case PgDataType::Kind::JSONB:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_JSONB);
                return true;
            case PgDataType::Kind::MONEY:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_MONEY);
                return true;
            case PgDataType::Kind::INTERVAL:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_INTERVAL);
                return true;
            case PgDataType::Kind::INET:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_INET);
                return true;
            case PgDataType::Kind::CIDR:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_CIDR);
                return true;
            case PgDataType::Kind::MACADDR:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_MACADDR);
                return true;
            case PgDataType::Kind::MACADDR8:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_MACADDR8);
                return true;
            case PgDataType::Kind::TSVECTOR:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TSVECTOR);
                return true;
            case PgDataType::Kind::TSQUERY:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TSQUERY);
                return true;
            case PgDataType::Kind::INT4RANGE:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_INT4RANGE);
                return true;
            case PgDataType::Kind::INT8RANGE:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_INT8RANGE);
                return true;
            case PgDataType::Kind::NUMRANGE:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_NUMRANGE);
                return true;
            case PgDataType::Kind::DATERANGE:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_DATERANGE);
                return true;
            case PgDataType::Kind::TSRANGE:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TSRANGE);
                return true;
            case PgDataType::Kind::TSTZRANGE:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_TSTZRANGE);
                return true;
            case PgDataType::Kind::XML:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_XML);
                return true;
            case PgDataType::Kind::POINT:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_POINT);
                return true;
            case PgDataType::Kind::POLYGON:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_POLYGON);
                return true;
            case PgDataType::Kind::COMPOSITE:
                emit_extended(sblr::ExtendedOpcode::EXT_TYPE_COMPOSITE);
                return true;
            default:
                return false;
        }
    };

    if (type.kind == PgDataType::Kind::DOMAIN) {
        core::ID domain_id;
        if (!resolveDomainId(type.type_name, domain_id)) {
            return;
        }
        emit(sblr::Opcode::TYPE_DOMAIN);
        emitUUID(domain_id);
        emitByte(0);  // is_array = false (array domains not supported yet)
        return;
    }

    if (type.kind == PgDataType::Kind::ARRAY) {
        if (type.element_kind == PgDataType::Kind::DOMAIN) {
            core::ID domain_id;
            if (!resolveDomainId(type.element_type, domain_id)) {
                return;
            }
            emit(sblr::Opcode::TYPE_DOMAIN);
            emitUUID(domain_id);
            emitByte(1);
            emitU32(type.array_size > 0 ? static_cast<uint32_t>(type.array_size) : 0);
            return;
        }

        emit(sblr::Opcode::TYPE_ARRAY);
        PgDataType element = type;
        element.kind = type.element_kind;
        if (element.kind == PgDataType::Kind::INT128 ||
            element.kind == PgDataType::Kind::UINT128) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(
                element.kind == PgDataType::Kind::INT128
                    ? sblr::ExtendedOpcode::EXT_TYPE_INT128
                    : sblr::ExtendedOpcode::EXT_TYPE_UINT128));
        } else if (!emit_extended_for_kind(element.kind, element.with_time_zone)) {
            emit(typeToOpcode(element.kind));
            switch (element.kind) {
                case PgDataType::Kind::CHAR:
                case PgDataType::Kind::VARCHAR:
                case PgDataType::Kind::BIT:
                case PgDataType::Kind::VARBIT:
                    emitU32(element.length > 0 ? static_cast<uint32_t>(element.length) : 255);
                    break;
                case PgDataType::Kind::DECIMAL:
                case PgDataType::Kind::NUMERIC:
                case PgDataType::Kind::MONEY:
                    emitU32(element.precision > 0 ? static_cast<uint32_t>(element.precision) : 18);
                    emitU32(static_cast<uint32_t>(element.scale));
                    break;
                default:
                    break;
            }
        }
        emitU32(type.array_size > 0 ? static_cast<uint32_t>(type.array_size) : 0);
        return;
    }

    if (type.kind == PgDataType::Kind::INT128 ||
        type.kind == PgDataType::Kind::UINT128) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(
            type.kind == PgDataType::Kind::INT128
                ? sblr::ExtendedOpcode::EXT_TYPE_INT128
                : sblr::ExtendedOpcode::EXT_TYPE_UINT128));
        return;
    }

    if (emit_extended_for_kind(type.kind, type.with_time_zone)) {
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
