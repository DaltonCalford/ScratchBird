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

// ============================================================================
// SET Statement
// ============================================================================

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
    } else if (matchKeyword(TokenType::KW_CONSTRAINTS)) {
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
    } else if (matchKeyword(TokenType::KW_SEARCH_PATH)) {
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
    } else if (matchKeyword(TokenType::KW_TABLES)) {
        error("SHOW TABLES is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchKeyword(TokenType::KW_DATABASES)) {
        error("SHOW DATABASES is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchKeyword(TokenType::KW_COLUMNS)) {
        error("SHOW COLUMNS is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchKeyword(TokenType::KW_INDEXES)) {
        error("SHOW INDEXES is not supported in PostgreSQL dialect");
        synchronize();
        return;
    } else if (matchKeyword(TokenType::KW_SEARCH_PATH)) {
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

} // namespace scratchbird::parser::postgresql
