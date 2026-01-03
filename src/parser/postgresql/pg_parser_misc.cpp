/**
 * PostgreSQL Parser - Miscellaneous Statement Parsing
 *
 * Handles SET, SHOW, transaction control, GRANT/REVOKE, ANALYZE, EXPLAIN, COPY.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
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

        if (matchKeyword(TokenType::KW_ALL)) {
            emitByte(0);  // All constraints
        } else {
            // List of constraint names
            emit(sblr::Opcode::BEGIN_LIST);
            size_t count_pos = bytecode_.size();
            emitU32(0);
            uint32_t count = 0;
            do {
                std::string name = parseIdentifier();
                emitString(name);
                count++;
            } while (match(TokenType::COMMA));
            sblr::writeInt32(&bytecode_[count_pos], count);
            emit(sblr::Opcode::END_LIST);
        }

        if (matchKeyword(TokenType::KW_DEFERRED)) {
            emitByte(1);
        } else if (matchKeyword(TokenType::KW_IMMEDIATE)) {
            emitByte(2);
        }
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        // SET ROLE
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_ROLE));

        if (matchKeyword(TokenType::KW_NONE)) {
            emitString("");
        } else {
            std::string role_name = parseIdentifier();
            emitString(role_name);
        }
    } else if (matchKeyword(TokenType::KW_SESSION)) {
        if (matchKeyword(TokenType::KW_AUTHORIZATION)) {
            // SET SESSION AUTHORIZATION
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_SET_SESSION_AUTH));

            if (matchKeyword(TokenType::KW_DEFAULT)) {
                emitString("");
            } else {
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
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GRANT));

    // Privileges
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);
    uint32_t count = 0;

    if (matchKeyword(TokenType::KW_ALL)) {
        matchKeyword(TokenType::KW_PRIVILEGES);
        emitString("ALL");
        count = 1;
    } else {
        do {
            std::string priv;
            if (matchKeyword(TokenType::KW_SELECT)) {
                priv = "SELECT";
            } else if (matchKeyword(TokenType::KW_INSERT)) {
                priv = "INSERT";
            } else if (matchKeyword(TokenType::KW_UPDATE)) {
                priv = "UPDATE";
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                priv = "DELETE";
            } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
                priv = "TRUNCATE";
            } else if (matchKeyword(TokenType::KW_REFERENCES)) {
                priv = "REFERENCES";
            } else if (matchKeyword(TokenType::KW_TRIGGER)) {
                priv = "TRIGGER";
            } else if (matchKeyword(TokenType::KW_EXECUTE)) {
                priv = "EXECUTE";
            } else if (matchKeyword(TokenType::KW_USAGE)) {
                priv = "USAGE";
            } else if (matchKeyword(TokenType::KW_CREATE)) {
                priv = "CREATE";
            } else if (matchKeyword(TokenType::KW_CONNECT)) {
                priv = "CONNECT";
            } else if (matchKeyword(TokenType::KW_TEMPORARY)) {
                priv = "TEMPORARY";
            }
            emitString(priv);

            // Column list for column-level privileges
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    parseIdentifier();
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }

            count++;
        } while (match(TokenType::COMMA));
    }

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    consumeKeyword(TokenType::KW_ON, "Expected ON");

    // Object type and name
    if (matchKeyword(TokenType::KW_TABLE)) {
        emitByte(1);
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
        emitByte(2);
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        emitByte(3);
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        emitByte(4);
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        emitByte(5);
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        emitByte(6);
    } else if (matchKeyword(TokenType::KW_ALL)) {
        // GRANT ON ALL TABLES/SEQUENCES/FUNCTIONS IN SCHEMA
        if (matchKeyword(TokenType::KW_TABLES)) {
            emitByte(11);
        } else if (matchKeyword(TokenType::KW_SEQUENCES)) {
            emitByte(12);
        } else if (matchKeyword(TokenType::KW_FUNCTIONS)) {
            emitByte(13);
        }
        consumeKeyword(TokenType::KW_IN, "Expected IN");
        consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
    } else {
        emitByte(1);  // Default to TABLE
    }

    // Object names
    emit(sblr::Opcode::BEGIN_LIST);
    size_t obj_count_pos = bytecode_.size();
    emitU32(0);
    uint32_t obj_count = 0;
    do {
        std::string obj_name = parseQualifiedName();
        emitString(obj_name);
        obj_count++;
    } while (match(TokenType::COMMA));
    sblr::writeInt32(&bytecode_[obj_count_pos], obj_count);
    emit(sblr::Opcode::END_LIST);

    consumeKeyword(TokenType::KW_TO, "Expected TO");

    // Grantees
    emit(sblr::Opcode::BEGIN_LIST);
    size_t grantee_count_pos = bytecode_.size();
    emitU32(0);
    uint32_t grantee_count = 0;
    do {
        if (matchKeyword(TokenType::KW_PUBLIC)) {
            emitString("PUBLIC");
        } else if (matchKeyword(TokenType::KW_GROUP)) {
            std::string group_name = parseIdentifier();
            emitString("GROUP:" + group_name);
        } else {
            std::string grantee = parseIdentifier();
            emitString(grantee);
        }
        grantee_count++;
    } while (match(TokenType::COMMA));
    sblr::writeInt32(&bytecode_[grantee_count_pos], grantee_count);
    emit(sblr::Opcode::END_LIST);

    // WITH GRANT OPTION
    if (matchKeyword(TokenType::KW_WITH)) {
        consumeKeyword(TokenType::KW_GRANT, "Expected GRANT");
        consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_GRANT_OPTION));
    }
}

void Parser::parseRevokeStmt() {
    consume(TokenType::KW_REVOKE, "Expected REVOKE");

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_REVOKE));

    // GRANT OPTION FOR
    bool revoke_grant_option = false;
    if (matchKeyword(TokenType::KW_GRANT)) {
        consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
        consumeKeyword(TokenType::KW_FOR, "Expected FOR");
        revoke_grant_option = true;
    }
    emitByte(revoke_grant_option ? 1 : 0);

    // Privileges (same as GRANT)
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);
    uint32_t count = 0;

    if (matchKeyword(TokenType::KW_ALL)) {
        matchKeyword(TokenType::KW_PRIVILEGES);
        emitString("ALL");
        count = 1;
    } else {
        do {
            std::string priv;
            if (matchKeyword(TokenType::KW_SELECT)) priv = "SELECT";
            else if (matchKeyword(TokenType::KW_INSERT)) priv = "INSERT";
            else if (matchKeyword(TokenType::KW_UPDATE)) priv = "UPDATE";
            else if (matchKeyword(TokenType::KW_DELETE)) priv = "DELETE";
            else if (matchKeyword(TokenType::KW_TRUNCATE)) priv = "TRUNCATE";
            else if (matchKeyword(TokenType::KW_REFERENCES)) priv = "REFERENCES";
            else if (matchKeyword(TokenType::KW_TRIGGER)) priv = "TRIGGER";
            else if (matchKeyword(TokenType::KW_EXECUTE)) priv = "EXECUTE";
            else if (matchKeyword(TokenType::KW_USAGE)) priv = "USAGE";
            else if (matchKeyword(TokenType::KW_CREATE)) priv = "CREATE";
            emitString(priv);
            count++;
        } while (match(TokenType::COMMA));
    }

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    consumeKeyword(TokenType::KW_ON, "Expected ON");

    // Object type (similar to GRANT)
    if (matchKeyword(TokenType::KW_TABLE)) emitByte(1);
    else if (matchKeyword(TokenType::KW_SEQUENCE)) emitByte(2);
    else if (matchKeyword(TokenType::KW_FUNCTION)) emitByte(3);
    else if (matchKeyword(TokenType::KW_DATABASE)) emitByte(5);
    else if (matchKeyword(TokenType::KW_SCHEMA)) emitByte(6);
    else emitByte(1);

    // Object names
    emit(sblr::Opcode::BEGIN_LIST);
    size_t obj_count_pos = bytecode_.size();
    emitU32(0);
    uint32_t obj_count = 0;
    do {
        emitString(parseQualifiedName());
        obj_count++;
    } while (match(TokenType::COMMA));
    sblr::writeInt32(&bytecode_[obj_count_pos], obj_count);
    emit(sblr::Opcode::END_LIST);

    consumeKeyword(TokenType::KW_FROM, "Expected FROM");

    // Grantees
    emit(sblr::Opcode::BEGIN_LIST);
    size_t grantee_count_pos = bytecode_.size();
    emitU32(0);
    uint32_t grantee_count = 0;
    do {
        if (matchKeyword(TokenType::KW_PUBLIC)) {
            emitString("PUBLIC");
        } else {
            emitString(parseIdentifier());
        }
        grantee_count++;
    } while (match(TokenType::COMMA));
    sblr::writeInt32(&bytecode_[grantee_count_pos], grantee_count);
    emit(sblr::Opcode::END_LIST);

    // CASCADE/RESTRICT
    if (matchKeyword(TokenType::KW_CASCADE)) {
        emitByte(1);
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
        emitByte(2);
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
    std::string table_name = parseQualifiedName();

    // Optional column list
    if (match(TokenType::LEFT_PAREN)) {
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;
        do {
            emit(sblr::Opcode::COLUMN_REF);
            emitString(parseIdentifier());
            count++;
        } while (match(TokenType::COMMA));
        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    emit(sblr::Opcode::TABLE_REF);
    emitString(table_name);

    if (matchKeyword(TokenType::KW_FROM)) {
        emitByte(1);  // COPY FROM

        // STDIN or filename
        if (matchKeyword(TokenType::KW_STDIN)) {
            emitString("STDIN");
        } else if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            emitString(lexer_.stringPool().get(id));
            advance();
        }
    } else if (matchKeyword(TokenType::KW_TO)) {
        emitByte(2);  // COPY TO

        // STDOUT or filename
        if (matchKeyword(TokenType::KW_STDOUT)) {
            emitString("STDOUT");
        } else if (check(TokenType::STRING_LITERAL)) {
            uint32_t id = current_token_.value.string_id;
            emitString(lexer_.stringPool().get(id));
            advance();
        }
    }

    // Options
    if (matchKeyword(TokenType::KW_WITH)) {
        match(TokenType::LEFT_PAREN);

        while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            if (matchKeyword(TokenType::KW_FORMAT)) {
                parseIdentifier();  // csv, text, binary
            } else if (matchKeyword(TokenType::KW_DELIMITER)) {
                if (check(TokenType::STRING_LITERAL)) {
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_NULL)) {
                if (check(TokenType::STRING_LITERAL)) {
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_HEADER)) {
                matchKeyword(TokenType::KW_TRUE) || matchKeyword(TokenType::KW_FALSE);
            } else if (matchKeyword(TokenType::KW_QUOTE)) {
                if (check(TokenType::STRING_LITERAL)) {
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_ESCAPE)) {
                if (check(TokenType::STRING_LITERAL)) {
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_ENCODING)) {
                if (check(TokenType::STRING_LITERAL)) {
                    advance();
                }
            } else {
                break;
            }
            match(TokenType::COMMA);
        }

        match(TokenType::RIGHT_PAREN);
    }

    // WHERE clause (for COPY FROM)
    if (matchKeyword(TokenType::KW_WHERE)) {
        parseExpression();
    }
}

} // namespace scratchbird::parser::postgresql
