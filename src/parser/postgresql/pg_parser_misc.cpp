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
 * PostgreSQL Parser - Miscellaneous Statement Parsing
 *
 * Handles SET, SHOW, transaction control, GRANT/REVOKE, ANALYZE, EXPLAIN, COPY.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/core/catalog_manager.h"
#include <algorithm>
#include <cctype>
#include <limits>

namespace scratchbird::parser::postgresql {

static parser::v3::SchemaPath buildPathFromQualified(parser::v3::StringPool& pool,
                                                     const std::string& name) {
    std::vector<parser::v3::StringPool::StringId> comps;
    std::string cur;
    for (char ch : name) {
        if (ch == '.') {
            if (!cur.empty()) {
                comps.push_back(pool.intern(cur));
                cur.clear();
            }
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) comps.push_back(pool.intern(cur));
    return parser::v3::SchemaPath(parser::v3::PathType::UNQUALIFIED, std::move(comps));
}

// ============================================================================
// SET Statement
// ============================================================================

parser::v3::Statement* Parser::parseSetStmtV3() {
    if (!matchKeyword(TokenType::KW_SET)) {
        return nullptr;
    }

    auto* stmt = arena()->create<parser::v3::SetStmt>();

    if (matchKeyword(TokenType::KW_LOCAL)) {
        stmt->scope = parser::v3::SetStmt::Scope::LOCAL;
    } else if (matchKeyword(TokenType::KW_SESSION)) {
        stmt->scope = parser::v3::SetStmt::Scope::SESSION;
    }

    if (matchKeyword(TokenType::KW_TRANSACTION)) {
        stmt->set_type = parser::v3::SetStmt::SetType::TRANSACTION;
        while (true) {
            if (matchKeyword(TokenType::KW_ON)) {
                error("PostgreSQL does not support ON CONFLICT in SET TRANSACTION");
            } else if (matchIdentifierKeyword("AUTOCOMMIT")) {
                error("PostgreSQL does not support AUTOCOMMIT in SET TRANSACTION");
            } else if (matchKeyword(TokenType::KW_ISOLATION)) {
                consumeKeyword(TokenType::KW_LEVEL, "Expected LEVEL");
                stmt->has_isolation_level = true;
                if (matchKeyword(TokenType::KW_SERIALIZABLE)) {
                    stmt->isolation_level = parser::v3::IsolationLevel::SERIALIZABLE;
                } else if (matchKeyword(TokenType::KW_REPEATABLE)) {
                    consumeKeyword(TokenType::KW_READ, "Expected READ");
                    stmt->isolation_level = parser::v3::IsolationLevel::REPEATABLE_READ;
                } else if (matchKeyword(TokenType::KW_READ)) {
                    if (matchKeyword(TokenType::KW_COMMITTED)) {
                        stmt->isolation_level = parser::v3::IsolationLevel::READ_COMMITTED;
                    } else if (matchKeyword(TokenType::KW_UNCOMMITTED)) {
                        stmt->isolation_level = parser::v3::IsolationLevel::READ_UNCOMMITTED;
                    }
                }
            } else if (matchKeyword(TokenType::KW_READ)) {
                stmt->has_access_mode = true;
                if (matchKeyword(TokenType::KW_ONLY)) {
                    stmt->access_mode = parser::v3::TransactionAccess::READ_ONLY;
                } else if (matchKeyword(TokenType::KW_WRITE)) {
                    stmt->access_mode = parser::v3::TransactionAccess::READ_WRITE;
                } else {
                    error("Expected ONLY or WRITE after READ");
                }
            } else if (matchKeyword(TokenType::KW_DEFERRABLE)) {
                stmt->deferrable = true;
            } else if (matchKeyword(TokenType::KW_NOT)) {
                consumeKeyword(TokenType::KW_DEFERRABLE, "Expected DEFERRABLE");
                stmt->not_deferrable = true;
            } else {
                break;
            }
            match(TokenType::COMMA);
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_ROLE)) {
        stmt->set_type = parser::v3::SetStmt::SetType::ROLE;
        if (matchKeyword(TokenType::KW_NONE) || matchKeyword(TokenType::KW_DEFAULT)) {
            stmt->is_default = true;
        } else {
            stmt->name = parseIdentifierId();
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SESSION)) {
        if (matchKeyword(TokenType::KW_AUTHORIZATION)) {
            stmt->set_type = parser::v3::SetStmt::SetType::SESSION_AUTHORIZATION;
            if (matchKeyword(TokenType::KW_DEFAULT) || matchKeyword(TokenType::KW_RESET)) {
                stmt->is_default = true;
            } else {
                stmt->name = parseIdentifierId();
            }
            return stmt;
        }
    }

    if (matchKeyword(TokenType::KW_TIME)) {
        consumeKeyword(TokenType::KW_ZONE, "Expected ZONE");
        stmt->set_type = parser::v3::SetStmt::SetType::TIME_ZONE;
        if (matchKeyword(TokenType::KW_LOCAL) || matchKeyword(TokenType::KW_DEFAULT)) {
            stmt->is_default = true;
            return stmt;
        }
        stmt->value = parseExpression();
        return stmt;
    }

    if (matchIdentifierKeyword("SQL_DIALECT")) {
        stmt->set_type = parser::v3::SetStmt::SetType::SQL_DIALECT;
        if (check(TokenType::INTEGER_LITERAL)) {
            stmt->sql_dialect = static_cast<uint8_t>(current_token_.value.int_value);
            advance();
        } else {
            error("Expected integer for SQL DIALECT");
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_CONSTRAINTS) || matchIdentifierKeyword("CONSTRAINTS")) {
        stmt->set_type = parser::v3::SetStmt::SetType::VARIABLE;
        stmt->name = string_pool_.intern("constraints_mode");
        if (!matchKeyword(TokenType::KW_ALL)) {
            // Parse and discard explicit constraint names for now.
            do {
                parseIdentifierId();
            } while (match(TokenType::COMMA));
        }
        auto* value = arena()->create<parser::v3::LiteralExpr>();
        value->literal_type = parser::v3::LiteralType::STRING;
        if (matchKeyword(TokenType::KW_DEFERRED)) {
            value->string_value = string_pool_.intern("deferred");
        } else {
            consumeKeyword(TokenType::KW_IMMEDIATE, "Expected DEFERRED or IMMEDIATE");
            value->string_value = string_pool_.intern("immediate");
        }
        stmt->value = value;
        return stmt;
    }

    stmt->set_type = parser::v3::SetStmt::SetType::VARIABLE;
    std::string name = parseIdentifier();
    if (match(TokenType::DOT)) {
        name += ".";
        name += parseIdentifier();
    }
    stmt->name = string_pool_.intern(name);
    if (match(TokenType::EQUAL) || matchKeyword(TokenType::KW_TO)) {
        // optional
    }
    if (matchKeyword(TokenType::KW_DEFAULT)) {
        stmt->is_default = true;
    } else {
        stmt->value = parseExpression();
    }
    std::string upper_name = name;
    std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    if (upper_name == "AUTOCOMMIT") {
        error("PostgreSQL does not support SET AUTOCOMMIT");
    }
    return stmt;
}

void Parser::parseSetStmt() {
    consume(TokenType::KW_SET, "Expected SET");

    // SET LOCAL / SET SESSION
    bool is_local = matchKeyword(TokenType::KW_LOCAL);
    bool is_session = matchKeyword(TokenType::KW_SESSION);

    if (matchKeyword(TokenType::KW_TRANSACTION)) {
        // SET TRANSACTION
        emit(sblr::Opcode::SET_TRANSACTION);

        constexpr uint8_t kIsoReadCommitted = 0;
        constexpr uint8_t kIsoSnapshot = 2;
        constexpr uint8_t kIsoSnapshotTableStability = 3;

        bool has_isolation = false;
        bool has_access_mode = false;
        bool has_deferrable = false;
        bool has_autocommit = false;
        bool has_conflict_error_code = false;
        uint8_t isolation = kIsoReadCommitted;
        uint8_t access_mode = 0;  // 0=READ WRITE, 1=READ ONLY
        uint8_t deferrable = 0;
        sblr::AutocommitMode autocommit_mode = sblr::AutocommitMode::UNCHANGED;
        auto conflict_action = sblr::TransactionConflictAction::DEFAULT;
        int32_t conflict_error_code = 0;

        // Transaction characteristics
        while (true) {
            if (matchKeyword(TokenType::KW_ON)) {
                error("PostgreSQL does not support ON CONFLICT in SET TRANSACTION");
                synchronize();
                return;
            } else if (matchIdentifierKeyword("AUTOCOMMIT")) {
                error("PostgreSQL does not support AUTOCOMMIT in SET TRANSACTION");
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
            } else if (matchKeyword(TokenType::KW_DEFERRABLE)) {
                has_deferrable = true;
                deferrable = 1;
            } else if (matchKeyword(TokenType::KW_NOT)) {
                consumeKeyword(TokenType::KW_DEFERRABLE, "Expected DEFERRABLE");
                has_deferrable = true;
                deferrable = 0;
            } else {
                break;
            }
            match(TokenType::COMMA);
        }

        uint16_t flags = 0;
        if (has_isolation) flags |= sblr::TransactionFlags::HAS_ISOLATION;
        if (has_access_mode) flags |= sblr::TransactionFlags::HAS_ACCESS_MODE;
        if (has_deferrable) flags |= sblr::TransactionFlags::HAS_DEFERRABLE;
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
        if (has_deferrable) emitByte(deferrable);
    } else if (matchIdentifierKeyword("CONSTRAINTS")) {
        // SET CONSTRAINTS
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_CONSTRAINTS));

        bool all_constraints = false;
        std::vector<std::string> names;
        if (matchKeyword(TokenType::KW_ALL)) {
            all_constraints = true;
        } else {
            do {
                names.push_back(parseIdentifier());
            } while (match(TokenType::COMMA));
        }

        bool deferred = false;
        if (matchKeyword(TokenType::KW_DEFERRED)) {
            deferred = true;
        } else if (matchKeyword(TokenType::KW_IMMEDIATE)) {
            deferred = false;
        }

        uint8_t flags = 0;
        if (all_constraints) {
            flags |= 0x01;
        }
        if (deferred) {
            flags |= 0x02;
        }
        emitByte(flags);

        if (!all_constraints) {
            if (names.size() > 255) {
                error("Too many constraint names in SET CONSTRAINTS");
            }
            emitByte(static_cast<uint8_t>(names.size()));
            for (const auto& name : names) {
                emitString(name);
            }
        }
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        // SET ROLE
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_ROLE));

        uint8_t flags = 0;
        if (matchKeyword(TokenType::KW_NONE) || matchKeyword(TokenType::KW_DEFAULT)) {
            flags |= 0x01;
            emitByte(flags);
        } else {
            emitByte(flags);
            std::string role_name = parseIdentifier();
            emitString(role_name);
        }
    } else if (matchKeyword(TokenType::KW_SESSION)) {
        if (matchKeyword(TokenType::KW_AUTHORIZATION)) {
            // SET SESSION AUTHORIZATION
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_SESSION_AUTH));

            uint8_t flags = 0;
            if (matchKeyword(TokenType::KW_DEFAULT) || matchKeyword(TokenType::KW_RESET)) {
                flags |= 0x01;
                emitByte(flags);
            } else {
                emitByte(flags);
                std::string user_name = parseIdentifier();
                emitString(user_name);
            }
        }
    } else if (matchKeyword(TokenType::KW_TIME)) {
        consumeKeyword(TokenType::KW_ZONE, "Expected ZONE");
        // SET TIME ZONE
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_VARIABLE));
        emitString("timezone");

        if (matchKeyword(TokenType::KW_LOCAL)) {
            emitString("LOCAL");
        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
            emitString("DEFAULT");
        } else if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            emitString(lexer_.stringPool().get(id));
            advance();
        } else {
            parseExpression();
        }
    } else if (matchIdentifierKeyword("SEARCH_PATH")) {
        // SET search_path
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_VARIABLE));
        emitString("search_path");

        if (matchKeyword(TokenType::KW_TO) || match(TokenType::EQUAL)) {
            emit(sblr::Opcode::BEGIN_LIST);
            size_t count_pos = bytecode_.size();
            emitU32(0);
            uint32_t count = 0;
            do {
                std::string schema = parseIdentifier();
                emitString(schema);
                count++;
            } while (match(TokenType::COMMA));
            sblr::writeInt32(&bytecode_[count_pos], count);
            emit(sblr::Opcode::END_LIST);
        }
    } else {
        // Generic SET variable = value
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_VARIABLE));

        std::string var_name = parseIdentifier();
        // Handle dotted names like client_encoding
        while (match(TokenType::DOT)) {
            var_name += ".";
            var_name += parseIdentifier();
        }
        std::string var_upper = var_name;
        std::transform(var_upper.begin(), var_upper.end(), var_upper.begin(), ::toupper);
        if (var_upper == "AUTOCOMMIT") {
            error("PostgreSQL does not support SET AUTOCOMMIT");
            synchronize();
            return;
        }
        emitString(var_name);

        if (matchKeyword(TokenType::KW_TO) || match(TokenType::EQUAL)) {
            if (matchKeyword(TokenType::KW_DEFAULT)) {
                emit(sblr::Opcode::LITERAL_NULL);
            } else {
                parseExpression();
            }
        }
    }
}

// ============================================================================
// SHOW Statement
// ============================================================================

parser::v3::Statement* Parser::parseShowStmtV3() {
    if (!matchKeyword(TokenType::KW_SHOW)) {
        return nullptr;
    }

    auto* stmt = arena()->create<parser::v3::ShowStmt>();

    if (matchKeyword(TokenType::KW_ALL)) {
        stmt->show_type = parser::v3::ShowStmt::ShowType::ALL;
        return stmt;
    }
    if (matchKeyword(TokenType::KW_TRANSACTION)) {
        consumeKeyword(TokenType::KW_ISOLATION, "Expected ISOLATION");
        consumeKeyword(TokenType::KW_LEVEL, "Expected LEVEL");
        stmt->show_type = parser::v3::ShowStmt::ShowType::TRANSACTION_ISOLATION_LEVEL;
        return stmt;
    }
    if (matchIdentifierKeyword("SEARCH_PATH")) {
        stmt->show_type = parser::v3::ShowStmt::ShowType::VARIABLE;
        stmt->name = string_pool_.intern("search_path");
        return stmt;
    }
    if (matchKeyword(TokenType::KW_VERSION)) {
        stmt->show_type = parser::v3::ShowStmt::ShowType::VERSION;
        return stmt;
    }
    if (matchKeyword(TokenType::KW_TABLES) || matchKeyword(TokenType::KW_TABLE) ||
        matchIdentifierKeyword("TABLES")) {
        error("SHOW TABLES is not supported in PostgreSQL dialect");
        return stmt;
    }
    if (matchKeyword(TokenType::KW_DATABASES) || matchKeyword(TokenType::KW_DATABASE) ||
        matchIdentifierKeyword("DATABASES")) {
        error("SHOW DATABASES is not supported in PostgreSQL dialect");
        return stmt;
    }
    if (matchKeyword(TokenType::KW_COLUMNS) || matchIdentifierKeyword("COLUMNS")) {
        error("SHOW COLUMNS is not supported in PostgreSQL dialect");
        return stmt;
    }
    if (matchKeyword(TokenType::KW_INDEXES) || matchIdentifierKeyword("INDEXES")) {
        error("SHOW INDEXES is not supported in PostgreSQL dialect");
        return stmt;
    }

    stmt->show_type = parser::v3::ShowStmt::ShowType::VARIABLE;
    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        stmt->name = parseIdentifierId();
    }
    return stmt;
}

void Parser::parseShowStmt() {
    consume(TokenType::KW_SHOW, "Expected SHOW");

    if (matchKeyword(TokenType::KW_ALL)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_ALL));
    } else if (matchKeyword(TokenType::KW_TRANSACTION)) {
        consumeKeyword(TokenType::KW_ISOLATION, "Expected ISOLATION");
        consumeKeyword(TokenType::KW_LEVEL, "Expected LEVEL");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_TRANSACTION_LEVEL));
    } else if (matchKeyword(TokenType::KW_TABLES) || matchIdentifierKeyword("TABLES")) {
        error("SHOW TABLES is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchKeyword(TokenType::KW_DATABASES) || matchIdentifierKeyword("DATABASES")) {
        error("SHOW DATABASES is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchKeyword(TokenType::KW_COLUMNS) || matchIdentifierKeyword("COLUMNS")) {
        error("SHOW COLUMNS is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchKeyword(TokenType::KW_INDEXES) || matchIdentifierKeyword("INDEXES")) {
        error("SHOW INDEXES is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchIdentifierKeyword("SEARCH_PATH")) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_SEARCH_PATH));
    } else {
        // Generic SHOW variable
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SHOW_VARIABLE));
        std::string var_name = parseIdentifier();
        emitString(var_name);
    }
}

// ============================================================================
// Transaction Control
// ============================================================================

parser::v3::Statement* Parser::parseBeginStmtV3() {
    if (!(matchKeyword(TokenType::KW_BEGIN) || matchKeyword(TokenType::KW_START))) {
        return nullptr;
    }
    auto* stmt = arena()->create<parser::v3::StartTransactionStmt>();
    matchKeyword(TokenType::KW_WORK) || matchKeyword(TokenType::KW_TRANSACTION);

    while (true) {
        if (matchKeyword(TokenType::KW_ISOLATION)) {
            consumeKeyword(TokenType::KW_LEVEL, "Expected LEVEL");
            stmt->has_isolation_level = true;
            if (matchKeyword(TokenType::KW_SERIALIZABLE)) {
                stmt->isolation_level = parser::v3::IsolationLevel::SERIALIZABLE;
            } else if (matchKeyword(TokenType::KW_REPEATABLE)) {
                consumeKeyword(TokenType::KW_READ, "Expected READ");
                stmt->isolation_level = parser::v3::IsolationLevel::REPEATABLE_READ;
            } else if (matchKeyword(TokenType::KW_READ)) {
                if (matchKeyword(TokenType::KW_COMMITTED)) {
                    stmt->isolation_level = parser::v3::IsolationLevel::READ_COMMITTED;
                } else if (matchKeyword(TokenType::KW_UNCOMMITTED)) {
                    stmt->isolation_level = parser::v3::IsolationLevel::READ_UNCOMMITTED;
                }
            }
        } else if (matchKeyword(TokenType::KW_READ)) {
            stmt->has_access_mode = true;
            if (matchKeyword(TokenType::KW_ONLY)) {
                stmt->access_mode = parser::v3::TransactionAccess::READ_ONLY;
            } else if (matchKeyword(TokenType::KW_WRITE)) {
                stmt->access_mode = parser::v3::TransactionAccess::READ_WRITE;
            }
        } else if (matchKeyword(TokenType::KW_DEFERRABLE)) {
            stmt->deferrable = true;
        } else if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_DEFERRABLE, "Expected DEFERRABLE");
            stmt->not_deferrable = true;
        } else {
            break;
        }
        match(TokenType::COMMA);
    }

    return stmt;
}

parser::v3::Statement* Parser::parsePrepareStmtV3() {
    if (!matchKeyword(TokenType::KW_PREPARE)) {
        return nullptr;
    }
    auto* stmt = arena()->create<parser::v3::PrepareTransactionStmt>();
    if (matchKeyword(TokenType::KW_TRANSACTION)) {
        // ok
    }
    if (check(TokenType::STRING_LITERAL)) {
        stmt->gid = internFromLexer(current_token_.value.string_id);
        advance();
    } else {
        stmt->gid = parseIdentifierId();
    }
    return stmt;
}

parser::v3::Statement* Parser::parseCommitStmtV3() {
    if (!matchKeyword(TokenType::KW_COMMIT)) {
        return nullptr;
    }
    auto* stmt = arena()->create<parser::v3::CommitStmt>();
    if (matchIdentifierKeyword("PREPARED")) {
        stmt->is_prepared = true;
        if (check(TokenType::STRING_LITERAL)) {
            stmt->prepared_gid = internFromLexer(current_token_.value.string_id);
            advance();
        } else {
            stmt->prepared_gid = parseIdentifierId();
        }
        return stmt;
    }
    matchKeyword(TokenType::KW_WORK);
    if (matchKeyword(TokenType::KW_AND)) {
        if (matchKeyword(TokenType::KW_CHAIN)) {
            stmt->and_chain = true;
        } else if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_CHAIN, "Expected CHAIN");
            stmt->and_no_chain = true;
        }
    }
    return stmt;
}

parser::v3::Statement* Parser::parseRollbackStmtV3() {
    if (!matchKeyword(TokenType::KW_ROLLBACK)) {
        return nullptr;
    }
    auto* stmt = arena()->create<parser::v3::RollbackStmt>();
    if (matchIdentifierKeyword("PREPARED")) {
        stmt->is_prepared = true;
        if (check(TokenType::STRING_LITERAL)) {
            stmt->prepared_gid = internFromLexer(current_token_.value.string_id);
            advance();
        } else {
            stmt->prepared_gid = parseIdentifierId();
        }
        return stmt;
    }
    matchKeyword(TokenType::KW_WORK);
    if (matchKeyword(TokenType::KW_TO)) {
        if (matchKeyword(TokenType::KW_SAVEPOINT)) {
            stmt->to_savepoint = true;
            stmt->savepoint_name = parseIdentifierId();
        }
    } else if (matchKeyword(TokenType::KW_AND)) {
        if (matchKeyword(TokenType::KW_CHAIN)) {
            stmt->and_chain = true;
        } else if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_CHAIN, "Expected CHAIN");
            stmt->and_no_chain = true;
        }
    }
    return stmt;
}

parser::v3::Statement* Parser::parseSavepointStmtV3() {
    if (!matchKeyword(TokenType::KW_SAVEPOINT)) {
        return nullptr;
    }
    auto* stmt = arena()->create<parser::v3::SavepointStmt>();
    stmt->name = parseIdentifierId();
    return stmt;
}

parser::v3::Statement* Parser::parseReleaseStmtV3() {
    if (!matchKeyword(TokenType::KW_RELEASE)) {
        return nullptr;
    }
    auto* stmt = arena()->create<parser::v3::ReleaseSavepointStmt>();
    matchKeyword(TokenType::KW_SAVEPOINT);
    stmt->name = parseIdentifierId();
    return stmt;
}

void Parser::parseBeginStmt() {
    consume(TokenType::KW_BEGIN, "Expected BEGIN");

    emit(sblr::Opcode::START_TRANSACTION);

    // Optional WORK/TRANSACTION
    matchKeyword(TokenType::KW_WORK) || matchKeyword(TokenType::KW_TRANSACTION);

    constexpr uint8_t kIsoReadCommitted = 0;
    constexpr uint8_t kIsoSnapshot = 2;
    constexpr uint8_t kIsoSnapshotTableStability = 3;

    bool has_isolation = false;
    bool has_access_mode = false;
    bool has_deferrable = false;
    bool has_autocommit = false;
    bool has_conflict_error_code = false;
    uint8_t isolation = kIsoReadCommitted;
    uint8_t access_mode = 0;  // 0=READ WRITE, 1=READ ONLY
    uint8_t deferrable = 0;
    sblr::AutocommitMode autocommit_mode = sblr::AutocommitMode::UNCHANGED;
    auto conflict_action = sblr::TransactionConflictAction::DEFAULT;
    int32_t conflict_error_code = 0;

    // Transaction characteristics
    while (true) {
        if (matchKeyword(TokenType::KW_ON)) {
            error("PostgreSQL does not support ON CONFLICT in BEGIN");
            synchronize();
            return;
        } else if (matchIdentifierKeyword("AUTOCOMMIT")) {
            error("PostgreSQL does not support AUTOCOMMIT in BEGIN");
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
        } else if (matchKeyword(TokenType::KW_DEFERRABLE)) {
            has_deferrable = true;
            deferrable = 1;
        } else if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_DEFERRABLE, "Expected DEFERRABLE");
            has_deferrable = true;
            deferrable = 0;
        } else {
            break;
        }
        match(TokenType::COMMA);
    }

    uint16_t flags = 0;
    if (has_isolation) flags |= sblr::TransactionFlags::HAS_ISOLATION;
    if (has_access_mode) flags |= sblr::TransactionFlags::HAS_ACCESS_MODE;
    if (has_deferrable) flags |= sblr::TransactionFlags::HAS_DEFERRABLE;
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
    if (has_deferrable) emitByte(deferrable);
}

void Parser::parsePrepareStmt() {
    consume(TokenType::KW_PREPARE, "Expected PREPARE");
    consumeKeyword(TokenType::KW_TRANSACTION, "Expected TRANSACTION");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_PREPARE_TRANSACTION));

    if (check(TokenType::STRING_LITERAL)) {
        uint32_t id = current_token_.value.string_id;
        emitString(lexer_.stringPool().get(id));
        advance();
    } else {
        error("Expected string literal after PREPARE TRANSACTION");
        emitString("");
    }
}

void Parser::parseCommitStmt() {
    consume(TokenType::KW_COMMIT, "Expected COMMIT");
    matchKeyword(TokenType::KW_WORK) || matchKeyword(TokenType::KW_TRANSACTION);

    if (matchIdentifierKeyword("PREPARED")) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_COMMIT_PREPARED));

        if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            emitString(lexer_.stringPool().get(id));
            advance();
        } else {
            error("Expected string literal after COMMIT PREPARED");
            emitString("");
        }
        return;
    }

    emit(sblr::Opcode::COMMIT);
    uint8_t flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;

    // AND [NO] CHAIN
    if (matchKeyword(TokenType::KW_AND)) {
        if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_CHAIN, "Expected CHAIN");
            flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;
        } else {
            consumeKeyword(TokenType::KW_CHAIN, "Expected CHAIN");
            flags = sblr::CommitRollbackFlags::AND_CHAIN;
        }
    }
    emitByte(flags);
}

void Parser::parseRollbackStmt() {
    consume(TokenType::KW_ROLLBACK, "Expected ROLLBACK");
    matchKeyword(TokenType::KW_WORK) || matchKeyword(TokenType::KW_TRANSACTION);

    if (matchIdentifierKeyword("PREPARED")) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ROLLBACK_PREPARED));

        if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            emitString(lexer_.stringPool().get(id));
            advance();
        } else {
            error("Expected string literal after ROLLBACK PREPARED");
            emitString("");
        }
        return;
    }

    // ROLLBACK TO SAVEPOINT
    if (matchKeyword(TokenType::KW_TO)) {
        matchKeyword(TokenType::KW_SAVEPOINT);
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ROLLBACK_TO_SAVEPOINT));
        std::string savepoint_name = parseIdentifier();
        emitString(savepoint_name);
    } else {
        emit(sblr::Opcode::ROLLBACK);
        uint8_t flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;

        // AND [NO] CHAIN
        if (matchKeyword(TokenType::KW_AND)) {
            if (matchKeyword(TokenType::KW_NO)) {
                consumeKeyword(TokenType::KW_CHAIN, "Expected CHAIN");
                flags = sblr::CommitRollbackFlags::AND_NO_CHAIN;
            } else {
                consumeKeyword(TokenType::KW_CHAIN, "Expected CHAIN");
                flags = sblr::CommitRollbackFlags::AND_CHAIN;
            }
        }
        emitByte(flags);
    }
}

void Parser::parseSavepointStmt() {
    consume(TokenType::KW_SAVEPOINT, "Expected SAVEPOINT");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SAVEPOINT));

    std::string savepoint_name = parseIdentifier();
    emitString(savepoint_name);
}

void Parser::parseReleaseStmt() {
    consume(TokenType::KW_RELEASE, "Expected RELEASE");
    matchKeyword(TokenType::KW_SAVEPOINT);

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RELEASE_SAVEPOINT));

    std::string savepoint_name = parseIdentifier();
    emitString(savepoint_name);
}

// ============================================================================
// GRANT and REVOKE
// ============================================================================

void Parser::parseGrantStmt() {
    consume(TokenType::KW_GRANT, "Expected GRANT");

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
            } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
                add_privilege(core::CatalogManager::Privilege::TRUNCATE);
            } else if (matchKeyword(TokenType::KW_REFERENCES)) {
                add_privilege(core::CatalogManager::Privilege::REFERENCES);
            } else if (matchKeyword(TokenType::KW_TRIGGER)) {
                add_privilege(core::CatalogManager::Privilege::TRIGGER);
            } else if (matchKeyword(TokenType::KW_EXECUTE)) {
                add_privilege(core::CatalogManager::Privilege::EXECUTE);
            } else if (matchKeyword(TokenType::KW_USAGE)) {
                add_privilege(core::CatalogManager::Privilege::USAGE);
            } else if (matchKeyword(TokenType::KW_CREATE)) {
                add_privilege(core::CatalogManager::Privilege::CREATE);
            } else if (matchKeyword(TokenType::KW_CONNECT)) {
                add_privilege(core::CatalogManager::Privilege::CONNECT);
            } else if (matchKeyword(TokenType::KW_TEMPORARY)) {
                add_privilege(core::CatalogManager::Privilege::TEMPORARY);
            } else if (matchKeyword(TokenType::KW_COPY)) {
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
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
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
    } else if (matchKeyword(TokenType::KW_GROUP)) {
        grantee_type = core::CatalogManager::GranteeType::GROUP;
        grantee_name = parseIdentifier();
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        grantee_type = core::CatalogManager::GranteeType::ROLE;
        grantee_name = parseIdentifier();
    } else {
        grantee_name = parseIdentifier();
    }
    if (match(TokenType::COMMA)) {
        error("GRANT supports a single grantee in current bytecode");
        parseIdentifier();
    }

    // WITH GRANT OPTION
    bool with_grant_option = false;
    if (matchKeyword(TokenType::KW_WITH)) {
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

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_REVOKE_PRIVILEGE));

    // GRANT OPTION FOR
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
            else if (matchKeyword(TokenType::KW_TRUNCATE)) add_privilege(core::CatalogManager::Privilege::TRUNCATE);
            else if (matchKeyword(TokenType::KW_REFERENCES)) add_privilege(core::CatalogManager::Privilege::REFERENCES);
            else if (matchKeyword(TokenType::KW_TRIGGER)) add_privilege(core::CatalogManager::Privilege::TRIGGER);
            else if (matchKeyword(TokenType::KW_EXECUTE)) add_privilege(core::CatalogManager::Privilege::EXECUTE);
            else if (matchKeyword(TokenType::KW_USAGE)) add_privilege(core::CatalogManager::Privilege::USAGE);
            else if (matchKeyword(TokenType::KW_CREATE)) add_privilege(core::CatalogManager::Privilege::CREATE);
            else if (matchKeyword(TokenType::KW_CONNECT)) add_privilege(core::CatalogManager::Privilege::CONNECT);
            else if (matchKeyword(TokenType::KW_TEMPORARY)) add_privilege(core::CatalogManager::Privilege::TEMPORARY);
            else if (matchKeyword(TokenType::KW_COPY)) add_privilege(core::CatalogManager::Privilege::COPY_FILE);
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
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
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
    } else if (matchKeyword(TokenType::KW_GROUP)) {
        grantee_type = core::CatalogManager::GranteeType::GROUP;
        grantee_name = parseIdentifier();
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        grantee_type = core::CatalogManager::GranteeType::ROLE;
        grantee_name = parseIdentifier();
    } else {
        grantee_name = parseIdentifier();
    }
    if (match(TokenType::COMMA)) {
        error("REVOKE supports a single grantee in current bytecode");
        parseIdentifier();
    }

    // CASCADE/RESTRICT
    bool cascade = false;
    if (matchKeyword(TokenType::KW_CASCADE)) {
        cascade = true;
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
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

// ============================================================================
// ANALYZE Statement
// ============================================================================

void Parser::parseAnalyzeStmt() {
    consume(TokenType::KW_ANALYZE, "Expected ANALYZE");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ANALYZE));

    // VERBOSE
    bool verbose = matchKeyword(TokenType::KW_VERBOSE);
    emitByte(verbose ? 1 : 0);

    // Optional table list
    if (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;

        do {
            std::string table_name = parseQualifiedName();
            emitString(table_name);

            // Optional column list
            if (match(TokenType::LEFT_PAREN)) {
                emit(sblr::Opcode::BEGIN_LIST);
                size_t col_count_pos = bytecode_.size();
                emitU32(0);
                uint32_t col_count = 0;
                do {
                    emitString(parseIdentifier());
                    col_count++;
                } while (match(TokenType::COMMA));
                sblr::writeInt32(&bytecode_[col_count_pos], col_count);
                emit(sblr::Opcode::END_LIST);
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }

            count++;
        } while (match(TokenType::COMMA));

        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
    }
}

// ============================================================================
// EXPLAIN Statement
// ============================================================================

void Parser::parseExplainStmt() {
    consume(TokenType::KW_EXPLAIN, "Expected EXPLAIN");

    emit(sblr::Opcode::EXPLAIN_PLAN);

    // Options
    uint8_t options = 0;
    if (matchKeyword(TokenType::KW_ANALYZE)) {
        options |= 1;
    }
    if (matchKeyword(TokenType::KW_VERBOSE)) {
        options |= 2;
    }

    // Parenthesized options
    if (match(TokenType::LEFT_PAREN)) {
        while (!check(TokenType::RIGHT_PAREN)) {
            if (matchKeyword(TokenType::KW_ANALYZE)) {
                options |= 1;
            } else if (matchKeyword(TokenType::KW_VERBOSE)) {
                options |= 2;
            } else if (matchKeyword(TokenType::KW_COSTS)) {
                options |= 4;
            } else if (matchKeyword(TokenType::KW_BUFFERS)) {
                options |= 8;
            } else if (matchKeyword(TokenType::KW_TIMING)) {
                options |= 16;
            } else if (matchKeyword(TokenType::KW_FORMAT)) {
                // TEXT, XML, JSON, YAML
                parseIdentifier();
            } else {
                parseIdentifier();  // Skip unknown option
            }

            // Optional boolean value
            if (matchKeyword(TokenType::KW_TRUE) || matchKeyword(TokenType::KW_ON)) {
                // Option enabled (already set above)
            } else if (matchKeyword(TokenType::KW_FALSE) || matchKeyword(TokenType::KW_OFF)) {
                // Option disabled - would need to unset bits
            }

            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    emitByte(options);

    // The statement to explain
    parseStatementInternal();
}

// ============================================================================
// COPY Statement
// ============================================================================

void Parser::parseCopyStmt() {
    consume(TokenType::KW_COPY, "Expected COPY");

    // COPY table [(columns)] FROM/TO
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);
    std::string table_path = schema.empty() ? table : schema + "/" + table;

    // Optional column list
    std::vector<std::string> column_names;
    if (match(TokenType::LEFT_PAREN)) {
        do {
            column_names.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    uint8_t direction = 0;
    std::string target;
    if (matchKeyword(TokenType::KW_FROM)) {
        direction = 1;
        // STDIN or filename
        if (matchKeyword(TokenType::KW_STDIN)) {
            target = "STDIN";
        } else if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            target = std::string(lexer_.stringPool().get(id));
            advance();
        } else {
            error("Expected STDIN or string literal for COPY FROM");
        }
    } else if (matchKeyword(TokenType::KW_TO)) {
        direction = 2;
        // STDOUT or filename
        if (matchKeyword(TokenType::KW_STDOUT)) {
            target = "STDOUT";
        } else if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            target = std::string(lexer_.stringPool().get(id));
            advance();
        } else {
            error("Expected STDOUT or string literal for COPY TO");
        }
    } else {
        error("Expected COPY FROM or COPY TO");
    }

    struct CopyOptions {
        enum class Format : uint8_t {
            TEXT = 1,
            CSV = 2,
            BINARY = 3
        };

        Format format = Format::TEXT;
        bool delimiter_set = false;
        char delimiter = '\t';
        bool null_set = false;
        std::string null_string = "\\N";
        bool header = false;
        bool header_set = false;
        char quote = '"';
        char escape = '\\';
        std::string encoding;
        bool encoding_set = false;
    };

    auto to_upper = [](const std::string& input) {
        std::string out;
        out.reserve(input.size());
        for (char c : input) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return out;
    };

    CopyOptions options;

    // Options
    if (matchKeyword(TokenType::KW_WITH)) {
        match(TokenType::LEFT_PAREN);

        while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            if (matchKeyword(TokenType::KW_FORMAT)) {
                std::string fmt;
                if (matchKeyword(TokenType::KW_CSV)) {
                    fmt = "CSV";
                } else if (matchKeyword(TokenType::KW_TEXT)) {
                    fmt = "TEXT";
                } else if (matchIdentifierKeyword("BINARY")) {
                    fmt = "BINARY";
                } else if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    fmt = std::string(lexer_.stringPool().get(id));
                    advance();
                } else {
                    fmt = parseIdentifier();
                }
                auto fmt_upper = to_upper(fmt);
                if (fmt_upper == "CSV") {
                    options.format = CopyOptions::Format::CSV;
                } else if (fmt_upper == "TEXT") {
                    options.format = CopyOptions::Format::TEXT;
                } else if (fmt_upper == "BINARY") {
                    options.format = CopyOptions::Format::BINARY;
                } else {
                    error("Unsupported COPY FORMAT value");
                }
            } else if (matchKeyword(TokenType::KW_DELIMITER)) {
                matchKeyword(TokenType::KW_AS);
                if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    std::string value = std::string(lexer_.stringPool().get(id));
                    if (value.size() != 1) {
                        error("COPY DELIMITER must be a single character");
                    } else {
                        options.delimiter = value[0];
                        options.delimiter_set = true;
                    }
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_NULL)) {
                matchKeyword(TokenType::KW_AS);
                if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    options.null_string = std::string(lexer_.stringPool().get(id));
                    options.null_set = true;
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_HEADER)) {
                options.header = true;
                options.header_set = true;
                if (matchKeyword(TokenType::KW_FALSE)) {
                    options.header = false;
                } else {
                    matchKeyword(TokenType::KW_TRUE);
                }
            } else if (matchKeyword(TokenType::KW_QUOTE)) {
                if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    std::string value = std::string(lexer_.stringPool().get(id));
                    if (value.size() != 1) {
                        error("COPY QUOTE must be a single character");
                    } else {
                        options.quote = value[0];
                    }
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_ESCAPE)) {
                if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    std::string value = std::string(lexer_.stringPool().get(id));
                    if (value.size() != 1) {
                        error("COPY ESCAPE must be a single character");
                    } else {
                        options.escape = value[0];
                    }
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_ENCODING)) {
                if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    options.encoding = std::string(lexer_.stringPool().get(id));
                    options.encoding_set = true;
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_CSV)) {
                options.format = CopyOptions::Format::CSV;
            } else if (matchKeyword(TokenType::KW_TEXT)) {
                options.format = CopyOptions::Format::TEXT;
            } else if (matchIdentifierKeyword("BINARY")) {
                options.format = CopyOptions::Format::BINARY;
            } else {
                break;
            }
            match(TokenType::COMMA);
        }

        match(TokenType::RIGHT_PAREN);
    }

    if (options.format == CopyOptions::Format::CSV) {
        if (!options.delimiter_set) {
            options.delimiter = ',';
        }
        if (!options.null_set) {
            options.null_string.clear();
        }
    }

    // WHERE clause (for COPY FROM)
    if (matchKeyword(TokenType::KW_WHERE)) {
        bool prev_emit = emit_enabled_;
        emit_enabled_ = false;
        parseExpression();
        emit_enabled_ = prev_emit;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_COPY));
    emitString(table_path);
    emitByte(direction);
    emitString(target);
    emitU32(static_cast<uint32_t>(column_names.size()));
    for (const auto& name : column_names) {
        emitString(name);
    }
    emitByte(static_cast<uint8_t>(options.format));
    emitString(std::string(1, options.delimiter));
    emitString(options.null_string);
    emitByte(options.header ? 1 : 0);
    emitString(std::string(1, options.quote));
    emitString(std::string(1, options.escape));
    emitString(options.encoding);
}

// ============================================================================
// V3 AST equivalents for GRANT/REVOKE/ANALYZE/EXPLAIN/COPY
// ============================================================================

parser::v3::Statement* Parser::parseGrantStmtV3() {
    if (!matchKeyword(TokenType::KW_GRANT)) {
        return nullptr;
    }

    auto* stmt = arena()->create<parser::v3::GrantStmt>();

    if (matchKeyword(TokenType::KW_ALL)) {
        matchKeyword(TokenType::KW_PRIVILEGES);
        stmt->privileges.push_back(parser::v3::PrivilegeType::ALL);
    } else {
        while (true) {
            if (matchKeyword(TokenType::KW_SELECT)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::SELECT);
            } else if (matchKeyword(TokenType::KW_INSERT)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::INSERT);
            } else if (matchKeyword(TokenType::KW_UPDATE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::UPDATE);
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::DELETE);
            } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::TRUNCATE);
            } else if (matchKeyword(TokenType::KW_REFERENCES)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::REFERENCES);
            } else if (matchKeyword(TokenType::KW_TRIGGER)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::TRIGGER);
            } else if (matchKeyword(TokenType::KW_EXECUTE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::EXECUTE);
            } else if (matchKeyword(TokenType::KW_USAGE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::USAGE);
            } else if (matchKeyword(TokenType::KW_CREATE) || matchKeyword(TokenType::KW_CONNECT) ||
                       matchKeyword(TokenType::KW_TEMPORARY)) {
                error("Unsupported GRANT privilege in V3 AST");
                break;
            } else if (matchKeyword(TokenType::KW_COPY)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::COPY);
            } else {
                error("Unsupported GRANT privilege");
                break;
            }

            if (match(TokenType::LEFT_PAREN)) {
                // Column-level privileges are not modeled in AST v3 yet.
                do {
                    parseIdentifierId();
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected ) after column list");
                error("Column-level GRANT privileges are not supported in V3 AST");
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");
    if (matchKeyword(TokenType::KW_ALL)) {
        if (matchKeyword(TokenType::KW_TABLES)) {
            consumeKeyword(TokenType::KW_IN, "Expected IN");
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            stmt->object_type = parser::v3::PrivilegeObjectType::ALL_TABLES_IN_SCHEMA;
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } else if (matchKeyword(TokenType::KW_SEQUENCES)) {
            consumeKeyword(TokenType::KW_IN, "Expected IN");
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            stmt->object_type = parser::v3::PrivilegeObjectType::ALL_SEQUENCES_IN_SCHEMA;
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } else if (matchKeyword(TokenType::KW_FUNCTIONS)) {
            consumeKeyword(TokenType::KW_IN, "Expected IN");
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            stmt->object_type = parser::v3::PrivilegeObjectType::ALL_FUNCTIONS_IN_SCHEMA;
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        }
    } else if (matchKeyword(TokenType::KW_TABLE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::TABLE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::VIEW;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::SEQUENCE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::FUNCTION;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::PROCEDURE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::SCHEMA;
        stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::DATABASE;
        stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER) ||
               isNonReservedKeyword(current_token_.type)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::TABLE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else {
        // Default to table object semantics to keep grammar aligned with
        // PostgreSQL's common "GRANT ... ON relname" form.
        stmt->object_type = parser::v3::PrivilegeObjectType::TABLE;
        stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
    }

    consumeKeyword(TokenType::KW_TO, "Expected TO");
    if (matchKeyword(TokenType::KW_PUBLIC)) {
        stmt->is_public = true;
    } else {
        do {
            stmt->grantees.push_back(parseIdentifierId());
        } while (match(TokenType::COMMA));
    }

    if (matchKeyword(TokenType::KW_WITH)) {
        consumeKeyword(TokenType::KW_GRANT, "Expected GRANT after WITH");
        consumeKeyword(TokenType::KW_OPTION, "Expected OPTION after WITH GRANT");
        stmt->with_grant_option = true;
    }

    return stmt;
}

parser::v3::Statement* Parser::parseRevokeStmtV3() {
    if (!matchKeyword(TokenType::KW_REVOKE)) {
        return nullptr;
    }

    auto* stmt = arena()->create<parser::v3::RevokeStmt>();

    if (matchKeyword(TokenType::KW_GRANT)) {
        consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
        consumeKeyword(TokenType::KW_FOR, "Expected FOR");
        stmt->grant_option_for = true;
    }

    if (matchKeyword(TokenType::KW_ALL)) {
        matchKeyword(TokenType::KW_PRIVILEGES);
        stmt->privileges.push_back(parser::v3::PrivilegeType::ALL);
    } else {
        while (true) {
            if (matchKeyword(TokenType::KW_SELECT)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::SELECT);
            } else if (matchKeyword(TokenType::KW_INSERT)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::INSERT);
            } else if (matchKeyword(TokenType::KW_UPDATE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::UPDATE);
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::DELETE);
            } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::TRUNCATE);
            } else if (matchKeyword(TokenType::KW_REFERENCES)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::REFERENCES);
            } else if (matchKeyword(TokenType::KW_TRIGGER)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::TRIGGER);
            } else if (matchKeyword(TokenType::KW_EXECUTE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::EXECUTE);
            } else if (matchKeyword(TokenType::KW_USAGE)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::USAGE);
            } else if (matchKeyword(TokenType::KW_CREATE) || matchKeyword(TokenType::KW_CONNECT) ||
                       matchKeyword(TokenType::KW_TEMPORARY)) {
                error("Unsupported REVOKE privilege in V3 AST");
                break;
            } else if (matchKeyword(TokenType::KW_COPY)) {
                stmt->privileges.push_back(parser::v3::PrivilegeType::COPY);
            } else {
                error("Unsupported REVOKE privilege");
                break;
            }

            if (match(TokenType::LEFT_PAREN)) {
                do {
                    parseIdentifierId();
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected ) after column list");
                error("Column-level REVOKE privileges are not supported in V3 AST");
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");
    if (matchKeyword(TokenType::KW_ALL)) {
        if (matchKeyword(TokenType::KW_TABLES)) {
            consumeKeyword(TokenType::KW_IN, "Expected IN");
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            stmt->object_type = parser::v3::PrivilegeObjectType::ALL_TABLES_IN_SCHEMA;
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } else if (matchKeyword(TokenType::KW_SEQUENCES)) {
            consumeKeyword(TokenType::KW_IN, "Expected IN");
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            stmt->object_type = parser::v3::PrivilegeObjectType::ALL_SEQUENCES_IN_SCHEMA;
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } else if (matchKeyword(TokenType::KW_FUNCTIONS)) {
            consumeKeyword(TokenType::KW_IN, "Expected IN");
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            stmt->object_type = parser::v3::PrivilegeObjectType::ALL_FUNCTIONS_IN_SCHEMA;
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        }
    } else if (matchKeyword(TokenType::KW_TABLE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::TABLE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::VIEW;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::SEQUENCE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::FUNCTION;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::PROCEDURE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::SCHEMA;
        stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::DATABASE;
        stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
    } else if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER) ||
               isNonReservedKeyword(current_token_.type)) {
        stmt->object_type = parser::v3::PrivilegeObjectType::TABLE;
        do {
            stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
        } while (match(TokenType::COMMA));
    } else {
        stmt->object_type = parser::v3::PrivilegeObjectType::TABLE;
        stmt->objects.push_back(buildPathFromQualified(string_pool_, parseQualifiedName()));
    }

    consumeKeyword(TokenType::KW_FROM, "Expected FROM");
    if (matchKeyword(TokenType::KW_PUBLIC)) {
        stmt->is_public = true;
    } else {
        do {
            stmt->grantees.push_back(parseIdentifierId());
        } while (match(TokenType::COMMA));
    }

    if (matchKeyword(TokenType::KW_CASCADE)) {
        stmt->cascade = true;
    }

    return stmt;
}

parser::v3::Statement* Parser::parseAnalyzeStmtV3() {
    if (!matchKeyword(TokenType::KW_ANALYZE)) {
        return nullptr;
    }

    auto* stmt = arena()->create<parser::v3::AnalyzeStmt>();
    if (matchKeyword(TokenType::KW_VERBOSE)) {
        stmt->verbose = true;
    }

    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER) ||
        isNonReservedKeyword(current_token_.type)) {
        stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                stmt->column_name = parseIdentifierId();
                stmt->has_column = true;
                while (match(TokenType::COMMA)) {
                    // Additional columns are parsed for compatibility.
                    parseIdentifierId();
                }
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after column list");
        }
    }

    return stmt;
}

parser::v3::Statement* Parser::parseExplainStmtV3() {
    if (!matchKeyword(TokenType::KW_EXPLAIN)) {
        return nullptr;
    }

    auto* stmt = arena()->create<parser::v3::ExplainStmt>();

    auto parse_option = [&](parser::v3::ExplainStmt* target) {
        if (matchKeyword(TokenType::KW_ANALYZE)) {
            target->analyze = true;
            return true;
        }
        if (matchKeyword(TokenType::KW_VERBOSE)) {
            target->verbose = true;
            return true;
        }
        if (matchKeyword(TokenType::KW_COSTS)) {
            target->costs = true;
            return true;
        }
        if (matchKeyword(TokenType::KW_BUFFERS)) {
            target->buffers = true;
            return true;
        }
        if (matchKeyword(TokenType::KW_TIMING)) {
            target->timing = true;
            return true;
        }
        if (matchKeyword(TokenType::KW_FORMAT)) {
            if (matchKeyword(TokenType::KW_JSON)) {
                target->format_json = true;
            } else if (matchKeyword(TokenType::KW_XML)) {
                target->format_xml = true;
            } else if (matchKeyword(TokenType::KW_TEXT)) {
                // default text
            } else if (check(TokenType::IDENTIFIER)) {
                std::string fmt = parseIdentifier();
                if (fmt == "YAML" || fmt == "yaml") {
                    target->format_yaml = true;
                }
            }
            return true;
        }
        return false;
    };

    if (match(TokenType::LEFT_PAREN)) {
        do {
            parse_option(stmt);
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ) after EXPLAIN options");
    } else {
        while (parse_option(stmt)) {
            // parse repeated options
        }
    }

    stmt->query = parseStatementInternal();
    if (!stmt->query) {
        error("Expected statement after EXPLAIN");
    }
    return stmt;
}

parser::v3::Statement* Parser::parseCopyStmtV3() {
    if (!matchKeyword(TokenType::KW_COPY)) {
        return nullptr;
    }

    auto* stmt = arena()->create<parser::v3::CopyStmt>();

    if (match(TokenType::LEFT_PAREN)) {
        stmt->query = parseSelectStmt();
        consume(TokenType::RIGHT_PAREN, "Expected ) after COPY query");
    } else {
        stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
        if (match(TokenType::LEFT_PAREN)) {
            do {
                stmt->columns.push_back(parseIdentifierId());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ) after column list");
        }
    }

    if (matchKeyword(TokenType::KW_FROM)) {
        stmt->direction = parser::v3::CopyStmt::Direction::FROM;
    } else {
        consumeKeyword(TokenType::KW_TO, "Expected TO or FROM");
        stmt->direction = parser::v3::CopyStmt::Direction::TO;
    }

    if (matchKeyword(TokenType::KW_STDIN)) {
        stmt->target_is_stdin = true;
    } else if (matchKeyword(TokenType::KW_STDOUT)) {
        stmt->target_is_stdout = true;
    } else {
        if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            advance();
            stmt->target = internFromLexer(id);
        } else {
            stmt->target = parseIdentifierId();
        }
    }

    if (matchKeyword(TokenType::KW_WITH)) {
        if (match(TokenType::LEFT_PAREN)) {
            auto parse_string_or_ident = [&]() -> parser::v3::StringPool::StringId {
                if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    advance();
                    return internFromLexer(id);
                }
                return parseIdentifierId();
            };
            auto parse_single_char_option = [&](const std::string& option_name)
                -> parser::v3::StringPool::StringId {
                auto value = parse_string_or_ident();
                if (value == parser::v3::StringPool::INVALID_ID) {
                    return value;
                }
                std::string text = std::string(string_pool_.get(value));
                if (text.size() != 1) {
                    error("COPY " + option_name + " must be a single character");
                }
                return value;
            };
            while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
                std::string upper;
                if (matchKeyword(TokenType::KW_FORMAT)) {
                    upper = "FORMAT";
                } else if (matchKeyword(TokenType::KW_DELIMITER)) {
                    upper = "DELIMITER";
                } else if (matchKeyword(TokenType::KW_NULL)) {
                    upper = "NULL";
                } else if (matchKeyword(TokenType::KW_HEADER)) {
                    upper = "HEADER";
                } else if (matchKeyword(TokenType::KW_QUOTE)) {
                    upper = "QUOTE";
                } else if (matchKeyword(TokenType::KW_ESCAPE)) {
                    upper = "ESCAPE";
                } else if (matchKeyword(TokenType::KW_ENCODING)) {
                    upper = "ENCODING";
                } else {
                    std::string option = parseIdentifier();
                    upper.reserve(option.size());
                    for (char c : option) {
                        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                    }
                }

                bool has_value = match(TokenType::EQUAL);
                if (upper == "FORMAT") {
                    if (matchKeyword(TokenType::KW_TEXT)) {
                        stmt->options.format = parser::v3::CopyOptions::Format::TEXT;
                    } else if (matchKeyword(TokenType::KW_CSV) || matchIdentifierKeyword("CSV")) {
                        stmt->options.format = parser::v3::CopyOptions::Format::CSV;
                    } else if (matchIdentifierKeyword("BINARY")) {
                        stmt->options.format = parser::v3::CopyOptions::Format::BINARY;
                    } else if (check(TokenType::IDENTIFIER)) {
                        std::string fmt = parseIdentifier();
                        if (fmt == "csv" || fmt == "CSV") {
                            stmt->options.format = parser::v3::CopyOptions::Format::CSV;
                        } else if (fmt == "binary" || fmt == "BINARY") {
                            stmt->options.format = parser::v3::CopyOptions::Format::BINARY;
                        }
                    } else if (has_value) {
                        error("Unsupported COPY FORMAT value");
                    }
                    stmt->options.format_set = true;
                } else if (upper == "DELIMITER") {
                    if (!has_value) {
                        matchKeyword(TokenType::KW_AS);
                    }
                    stmt->options.delimiter = parse_single_char_option("DELIMITER");
                    stmt->options.delimiter_set = true;
                } else if (upper == "NULL") {
                    if (!has_value) {
                        matchKeyword(TokenType::KW_AS);
                    }
                    if (check(TokenType::STRING_LITERAL)) {
                        uint32_t id = current_token_.value.string_id;
                        advance();
                        stmt->options.null_string = internFromLexer(id);
                    } else {
                        stmt->options.null_string = parseIdentifierId();
                    }
                    stmt->options.null_set = true;
                } else if (upper == "HEADER") {
                    if (!has_value) {
                        stmt->options.header = true;
                    } else if (matchKeyword(TokenType::KW_TRUE)) {
                        stmt->options.header = true;
                    } else if (matchKeyword(TokenType::KW_FALSE)) {
                        stmt->options.header = false;
                    } else {
                        error("Expected TRUE/FALSE for HEADER");
                    }
                    stmt->options.header_set = true;
                } else if (upper == "QUOTE") {
                    if (!has_value) {
                        matchKeyword(TokenType::KW_AS);
                    }
                    stmt->options.quote = parse_single_char_option("QUOTE");
                    stmt->options.quote_set = true;
                } else if (upper == "ESCAPE") {
                    if (!has_value) {
                        matchKeyword(TokenType::KW_AS);
                    }
                    stmt->options.escape = parse_single_char_option("ESCAPE");
                    stmt->options.escape_set = true;
                } else if (upper == "ENCODING") {
                    if (!has_value) {
                        matchKeyword(TokenType::KW_AS);
                    }
                    stmt->options.encoding = parse_string_or_ident();
                    stmt->options.encoding_set = true;
                } else {
                    error("Unsupported COPY option");
                }

                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            consume(TokenType::RIGHT_PAREN, "Expected ) after COPY options");
        }
    }

    auto id_to_string = [&](parser::v3::StringPool::StringId id) -> std::string {
        if (id == parser::v3::StringPool::INVALID_ID) {
            return "";
        }
        return std::string(string_pool_.get(id));
    };
    auto path_to_string = [&](const parser::v3::SchemaPath& path) -> std::string {
        std::string out;
        for (size_t i = 0; i < path.components.size(); ++i) {
            if (i != 0) {
                out.push_back('.');
            }
            out += string_pool_.get(path.components[i]);
        }
        return out;
    };

    std::string table_path = path_to_string(stmt->table_path);
    uint8_t direction = (stmt->direction == parser::v3::CopyStmt::Direction::FROM) ? 1u : 2u;
    std::string target;
    if (stmt->target_is_stdin) {
        target = "STDIN";
    } else if (stmt->target_is_stdout) {
        target = "STDOUT";
    } else {
        target = id_to_string(stmt->target);
    }

    uint8_t format = 1;  // TEXT
    if (stmt->options.format == parser::v3::CopyOptions::Format::CSV) {
        format = 2;
    } else if (stmt->options.format == parser::v3::CopyOptions::Format::BINARY) {
        format = 3;
    }

    std::string delimiter = stmt->options.delimiter_set ? id_to_string(stmt->options.delimiter) : "\t";
    std::string null_string = stmt->options.null_set ? id_to_string(stmt->options.null_string) : "\\N";
    if (format == 2) {
        if (!stmt->options.delimiter_set) {
            delimiter = ",";
        }
        if (!stmt->options.null_set) {
            null_string.clear();
        }
    }
    std::string quote = stmt->options.quote_set ? id_to_string(stmt->options.quote) : "\"";
    std::string escape = stmt->options.escape_set ? id_to_string(stmt->options.escape) : "\\";
    std::string encoding = stmt->options.encoding_set ? id_to_string(stmt->options.encoding) : "";

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_COPY));
    emitString(table_path);
    emitByte(direction);
    emitString(target);
    emitU32(static_cast<uint32_t>(stmt->columns.size()));
    for (auto col : stmt->columns) {
        emitString(id_to_string(col));
    }
    emitByte(format);
    emitString(delimiter);
    emitString(null_string);
    emitByte(stmt->options.header ? 1 : 0);
    emitString(quote);
    emitString(escape);
    emitString(encoding);

    return stmt;
}

} // namespace scratchbird::parser::postgresql
