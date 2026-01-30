/**
 * PostgreSQL Parser - DDL Statement Parsing
 *
 * Handles CREATE, ALTER, DROP, TRUNCATE and other DDL statements.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/query_compiler_v2.h"
#include <algorithm>
#include <cctype>
#include <limits>

namespace scratchbird::parser::postgresql {

namespace {
uint8_t encodeDataType(const PgDataType& dt) {
    using core::DataType;
    switch (dt.kind) {
        case PgDataType::Kind::SMALLINT:
        case PgDataType::Kind::INTEGER:
        case PgDataType::Kind::SERIAL:
        case PgDataType::Kind::SMALLSERIAL:
            return static_cast<uint8_t>(DataType::INT32);
        case PgDataType::Kind::BIGINT:
        case PgDataType::Kind::BIGSERIAL:
            return static_cast<uint8_t>(DataType::INT64);
        case PgDataType::Kind::REAL:
            return static_cast<uint8_t>(DataType::FLOAT32);
        case PgDataType::Kind::DOUBLE_PRECISION:
            return static_cast<uint8_t>(DataType::FLOAT64);
        case PgDataType::Kind::DECIMAL:
        case PgDataType::Kind::NUMERIC:
        case PgDataType::Kind::MONEY:
            return static_cast<uint8_t>(DataType::DECIMAL);
        case PgDataType::Kind::CHAR:
            return static_cast<uint8_t>(DataType::CHAR);
        case PgDataType::Kind::VARCHAR:
            return static_cast<uint8_t>(DataType::VARCHAR);
        case PgDataType::Kind::TEXT:
            return static_cast<uint8_t>(DataType::TEXT);
        case PgDataType::Kind::BYTEA:
            return static_cast<uint8_t>(DataType::BLOB);
        case PgDataType::Kind::DATE:
            return static_cast<uint8_t>(DataType::DATE);
        case PgDataType::Kind::TIME:
        case PgDataType::Kind::TIMETZ:
            return static_cast<uint8_t>(DataType::TIME);
        case PgDataType::Kind::TIMESTAMP:
        case PgDataType::Kind::TIMESTAMPTZ:
            return static_cast<uint8_t>(DataType::TIMESTAMP);
        case PgDataType::Kind::BOOLEAN:
            return static_cast<uint8_t>(DataType::BOOLEAN);
        case PgDataType::Kind::UUID:
            return static_cast<uint8_t>(DataType::UUID);
        default:
            return static_cast<uint8_t>(DataType::VARCHAR);
    }
}

std::string buildEmulatedServerRoot(std::string_view default_schema) {
    if (default_schema.empty()) {
        return std::string(default_schema);
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

std::pair<std::string, std::string> splitQualifiedName(const std::string& qualified) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : qualified) {
        if (ch == '.') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);

    std::string schema;
    std::string object = parts.back();
    if (parts.size() > 1) {
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            if (!schema.empty()) {
                schema.push_back('.');
            }
            schema += parts[i];
        }
    }
    return std::make_pair(schema, object);
}

std::vector<std::string> splitPath(const std::string& path) {
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
}
} // namespace

// ============================================================================
// CREATE Statement Dispatch
// ============================================================================

void Parser::parseCreateStmt() {
    consume(TokenType::KW_CREATE, "Expected CREATE");

    // Handle OR REPLACE
    bool or_replace = false;
    if (matchKeyword(TokenType::KW_OR)) {
        consumeKeyword(TokenType::KW_REPLACE, "Expected REPLACE");
        or_replace = true;
    }
    pending_or_replace_ = or_replace;

    // Handle TEMP/TEMPORARY
    bool is_temp = matchKeyword(TokenType::KW_TEMP) || matchKeyword(TokenType::KW_TEMPORARY);

    // Handle UNLOGGED
    bool is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);

    pending_create_temp_ = is_temp;
    pending_create_unlogged_ = is_unlogged;

    // What to create?
    if (matchKeyword(TokenType::KW_TABLE)) {
        parseCreateTable();
    } else if (matchKeyword(TokenType::KW_INDEX)) {
        pending_index_unique_ = false;
        parseCreateIndex();
    } else if (matchKeyword(TokenType::KW_UNIQUE)) {
        consumeKeyword(TokenType::KW_INDEX, "Expected INDEX");
        pending_index_unique_ = true;
        parseCreateIndex();
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        parseCreateView();
    } else if (matchKeyword(TokenType::KW_MATERIALIZED)) {
        consumeKeyword(TokenType::KW_VIEW, "Expected VIEW");
        parseCreateMaterializedView();
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
        parseCreateSequence();
    } else if (matchKeyword(TokenType::KW_DATABASE)) {
        parseCreateDatabase();
    } else if (matchKeyword(TokenType::KW_SCHEMA)) {
        parseCreateSchema();
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        parseCreateFunction();
    } else if (matchKeyword(TokenType::KW_PROCEDURE)) {
        parseCreateProcedure();
    } else if (matchKeyword(TokenType::KW_TRIGGER)) {
        parseCreateTrigger();
    } else if (matchKeyword(TokenType::KW_TYPE)) {
        parseCreateType();
    } else if (matchKeyword(TokenType::KW_DOMAIN)) {
        parseCreateDomain();
    } else if (matchKeyword(TokenType::KW_POLICY)) {
        parseCreatePolicy();
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_ROLE));
        std::string role_name = parseIdentifier();
        emitString(role_name);
    } else if (matchKeyword(TokenType::KW_USER)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_USER));
        std::string user_name = parseIdentifier();
        emitString(user_name);
        bool has_password = false;
        bool is_superuser = false;
        std::string password;

        if (matchKeyword(TokenType::KW_WITH)) {
            // Optional WITH keyword before options
        }
        while (true) {
            if (matchKeyword(TokenType::KW_PASSWORD)) {
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for PASSWORD");
                    break;
                }
                uint32_t id = current_token_.value.string_id;
                password = std::string(lexer_.stringPool().get(id));
                has_password = true;
                advance();
            } else if (matchKeyword(TokenType::KW_SUPERUSER)) {
                is_superuser = true;
            } else if (matchKeyword(TokenType::KW_NOSUPERUSER)) {
                is_superuser = false;
            } else if (matchKeyword(TokenType::KW_WITH)) {
                continue;
            } else {
                break;
            }
        }

        uint8_t flags = 0;
        if (has_password) flags |= 0x01;
        if (is_superuser) flags |= 0x02;
        emitByte(flags);
        if (has_password) {
            emitString(password);
        }
    } else {
        error("Expected TABLE, INDEX, VIEW, SEQUENCE, FUNCTION, etc. after CREATE");
    }

    pending_or_replace_ = false;
    pending_create_temp_ = false;
    pending_create_unlogged_ = false;
}

// ============================================================================
// CREATE TABLE
// ============================================================================

void Parser::parseCreateTable() {
    emit(sblr::Opcode::CREATE_TABLE);

    bool is_temp = pending_create_temp_;
    bool is_unlogged = pending_create_unlogged_;
    pending_create_temp_ = false;
    pending_create_unlogged_ = false;

    uint8_t on_commit_flag = 0;
    size_t flags_pos = bytecode_.size();
    emitByte(0);

    // IF NOT EXISTS
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    (void)if_not_exists;

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);

    emit(sblr::Opcode::TABLE_REF);
    emitByte(0);
    emitString(schema.empty() ? table : schema + "/" + table);
    emitString("");

    consume(TokenType::LEFT_PAREN, "Expected (");

    // Parse column definitions and constraints
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitUVarint(0);
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
    std::vector<ForeignKeyDef> pending_fks;
    std::string tablespace_name;
    bool has_partition_clause = false;
    std::string partition_strategy;
    std::vector<std::string> partition_columns;

    do {
        bool emitted_entry = false;
        // Check for table-level constraint
        if (check(TokenType::KW_PRIMARY) || check(TokenType::KW_UNIQUE) ||
            check(TokenType::KW_FOREIGN) || check(TokenType::KW_CHECK) ||
            check(TokenType::KW_CONSTRAINT) || check(TokenType::KW_EXCLUDE)) {

            // Table constraint
            std::string constraint_name;
            if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                constraint_name = parseIdentifier();
            }

            if (matchKeyword(TokenType::KW_PRIMARY)) {
                consumeKeyword(TokenType::KW_KEY, "Expected KEY");
                consume(TokenType::LEFT_PAREN, "Expected (");
                std::vector<std::string> key_columns;
                do {
                    key_columns.push_back(parseIdentifier());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
                emit(sblr::Opcode::PRIMARY_KEY);
                emit(sblr::Opcode::BEGIN_LIST);
                emitUVarint(static_cast<uint64_t>(key_columns.size()));
                for (const auto& col : key_columns) {
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString("");
                    emitString(col);
                }
                emit(sblr::Opcode::END_LIST);
                emitString(constraint_name);
                emitted_entry = true;
            } else if (matchKeyword(TokenType::KW_UNIQUE)) {
                consume(TokenType::LEFT_PAREN, "Expected (");
                std::vector<std::string> key_columns;
                do {
                    key_columns.push_back(parseIdentifier());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
                emit(sblr::Opcode::UNIQUE_CONSTRAINT);
                emit(sblr::Opcode::BEGIN_LIST);
                emitUVarint(static_cast<uint64_t>(key_columns.size()));
                for (const auto& col : key_columns) {
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString("");
                    emitString(col);
                }
                emit(sblr::Opcode::END_LIST);
                emitString(constraint_name);
                emitted_entry = true;
            } else if (matchKeyword(TokenType::KW_FOREIGN)) {
                consumeKeyword(TokenType::KW_KEY, "Expected KEY");
                ForeignKeyDef fk = parseForeignKeyDef();
                fk.name = constraint_name;
                pending_fks.push_back(std::move(fk));
            } else if (matchKeyword(TokenType::KW_CHECK)) {
                consume(TokenType::LEFT_PAREN, "Expected (");
                bool prev_emit = emit_enabled_;
                emit_enabled_ = false;
                parseExpression();
                emit_enabled_ = prev_emit;
                consume(TokenType::RIGHT_PAREN, "Expected )");
                error("Table-level CHECK constraints are not supported yet");
            }
        } else {
            // Column definition
            ColumnDef col = parseColumnDef();

            emit(sblr::Opcode::COLUMN_DEF);
            emit(sblr::Opcode::COLUMN_REF);
            emitString("");
            emitString(col.name);
            emitTypeDefinition(col.type);
            if (!col.collation.empty()) {
                emit(sblr::Opcode::COLUMN_COLLATE);
                emitString(col.collation);
            }

            // Constraints
            if (col.not_null || col.primary_key) {
                emit(sblr::Opcode::NOT_NULL);
            }
            if (col.primary_key) {
                emit(sblr::Opcode::PRIMARY_KEY);
            }
            if (col.unique) {
                emit(sblr::Opcode::UNIQUE_CONSTRAINT);
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
            if (col.is_identity) {
                emit(sblr::Opcode::IDENTITY_COLUMN);
                emitByte(col.identity_always ? 1 : 0);
            }
            if (col.is_generated) {
                if (!col.generated_expr_bytecode.empty()) {
                    emit(sblr::Opcode::GENERATED_COLUMN);
                    emitByte(col.generated_stored ? 1 : 2);
                    emitU32(static_cast<uint32_t>(col.generated_expr_bytecode.size()));
                    if (emit_enabled_) {
                        bytecode_.insert(bytecode_.end(),
                                         col.generated_expr_bytecode.begin(),
                                         col.generated_expr_bytecode.end());
                    }
                }
            }
            emitted_entry = true;
        }
        if (emitted_entry) {
            count++;
        }
    } while (match(TokenType::COMMA));

    patch_varint(count_pos, count);
    emit(sblr::Opcode::END_LIST);

    consume(TokenType::RIGHT_PAREN, "Expected )");

    // Table options
    if (matchKeyword(TokenType::KW_WITH)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        // Parse storage options
        while (!check(TokenType::RIGHT_PAREN)) {
            parseIdentifier();  // option name
            if (match(TokenType::EQUAL)) {
                bool prev_emit = emit_enabled_;
                emit_enabled_ = false;
                parseExpression();
                emit_enabled_ = prev_emit;
            }
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    if (matchKeyword(TokenType::KW_ON)) {
        consumeKeyword(TokenType::KW_COMMIT, "Expected COMMIT");
        if (!is_temp) {
            error("ON COMMIT can only be used on temporary tables");
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            on_commit_flag = 0x01;
            matchKeyword(TokenType::KW_ROWS);
        } else if (matchKeyword(TokenType::KW_PRESERVE)) {
            on_commit_flag = 0x02;
            matchKeyword(TokenType::KW_ROWS);
        } else if (matchKeyword(TokenType::KW_DROP)) {
            on_commit_flag = 0x03;
        } else {
            error("Expected DELETE, PRESERVE, or DROP after ON COMMIT");
        }
    }

    // TABLESPACE
    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        tablespace_name = parseIdentifier();
        error("TABLESPACE clauses are not supported in emulated parsers");
        tablespace_name.clear();
    }

    if (matchKeyword(TokenType::KW_PARTITION)) {
        consumeKeyword(TokenType::KW_BY, "Expected BY after PARTITION");
        if (matchKeyword(TokenType::KW_RANGE)) {
            partition_strategy = "RANGE";
        } else if (matchKeyword(TokenType::KW_LIST)) {
            partition_strategy = "LIST";
        } else if (matchKeyword(TokenType::KW_HASH)) {
            partition_strategy = "HASH";
        } else {
            error("Expected RANGE, LIST, or HASH after PARTITION BY");
        }

        consume(TokenType::LEFT_PAREN, "Expected ( after PARTITION BY");
        do {
            partition_columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected ) after partition columns");
        has_partition_clause = true;
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
        uint8_t deferrable_flags = 0;
        if (fk.deferrable) {
            deferrable_flags |= 0x01;
        }
        if (fk.initially_deferred) {
            deferrable_flags |= 0x02;
        }
        emitByte(deferrable_flags);
    }

    uint8_t flags = 0;
    if (is_temp) {
        flags |= 0x01;
    }
    if (on_commit_flag != 0) {
        flags |= static_cast<uint8_t>((on_commit_flag & 0x03) << 2);
    }
    if (is_unlogged) {
        flags |= 0x10;
    }
    if (emit_enabled_ && flags_pos < bytecode_.size()) {
        bytecode_[flags_pos] = flags;
    }

    emitString(tablespace_name);

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
}

// ============================================================================
// CREATE POLICY
// ============================================================================

void Parser::parseCreatePolicy() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_POLICY));

    std::string policy_name = parseIdentifier();
    emitString(policy_name);

    consumeKeyword(TokenType::KW_ON, "Expected ON");
    std::string table_name = parseQualifiedName();
    emitString(table_name);

    // Optional AS PERMISSIVE/RESTRICTIVE (ignored for now)
    if (matchKeyword(TokenType::KW_AS)) {
        if (matchIdentifierKeyword("PERMISSIVE") ||
            matchIdentifierKeyword("RESTRICTIVE"))
        {
            // no-op
        }
    }

    uint8_t policy_command = 0; // ALL
    if (matchKeyword(TokenType::KW_FOR)) {
        if (matchKeyword(TokenType::KW_ALL)) {
            policy_command = 0;
        } else if (matchKeyword(TokenType::KW_SELECT)) {
            policy_command = 1;
        } else if (matchKeyword(TokenType::KW_INSERT)) {
            policy_command = 2;
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            policy_command = 3;
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            policy_command = 4;
        } else {
            error("Expected ALL, SELECT, INSERT, UPDATE, or DELETE after FOR");
        }
    }

    std::vector<std::string> roles;
    if (matchKeyword(TokenType::KW_TO)) {
        do {
            if (matchKeyword(TokenType::KW_PUBLIC)) {
                roles.emplace_back("PUBLIC");
            } else if (matchKeyword(TokenType::KW_CURRENT_USER)) {
                roles.emplace_back("CURRENT_USER");
            } else if (matchKeyword(TokenType::KW_CURRENT_ROLE)) {
                roles.emplace_back("CURRENT_ROLE");
            } else if (matchKeyword(TokenType::KW_SESSION_USER)) {
                roles.emplace_back("SESSION_USER");
            } else {
                roles.push_back(parseIdentifier());
            }
        } while (match(TokenType::COMMA));
    } else {
        roles.emplace_back("PUBLIC");
    }

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

    bool has_using = false;
    bool has_with_check = false;
    std::vector<uint8_t> using_expr;
    std::vector<uint8_t> with_check_expr;

    if (matchKeyword(TokenType::KW_USING)) {
        has_using = true;
        if (match(TokenType::LEFT_PAREN)) {
            using_expr = capture_expr();
            consume(TokenType::RIGHT_PAREN, "Expected ) after USING");
        } else {
            using_expr = capture_expr();
        }
    }

    if (matchKeyword(TokenType::KW_WITH)) {
        consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
        has_with_check = true;
        if (match(TokenType::LEFT_PAREN)) {
            with_check_expr = capture_expr();
            consume(TokenType::RIGHT_PAREN, "Expected ) after WITH CHECK");
        } else {
            with_check_expr = capture_expr();
        }
    }

    emitByte(policy_command);
    emitU32(static_cast<uint32_t>(roles.size()));
    for (const auto& role : roles) {
        emitString(role);
    }

    uint8_t flags = 0;
    if (has_using) flags |= 0x01;
    if (has_with_check) flags |= 0x02;
    emitByte(flags);

    if (has_using) {
        bytecode_.insert(bytecode_.end(), using_expr.begin(), using_expr.end());
    }
    if (has_with_check) {
        bytecode_.insert(bytecode_.end(), with_check_expr.begin(), with_check_expr.end());
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

    // Parse column constraints
    while (true) {
        if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_NULL, "Expected NULL");
            col.not_null = true;
        } else if (matchKeyword(TokenType::KW_NULL)) {
            col.not_null = false;
        } else if (matchKeyword(TokenType::KW_PRIMARY)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY");
            col.primary_key = true;
        } else if (matchKeyword(TokenType::KW_UNIQUE)) {
            col.unique = true;
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
        } else if (matchKeyword(TokenType::KW_GENERATED)) {
            col.is_generated = true;
            if (matchKeyword(TokenType::KW_ALWAYS)) {
                col.identity_always = true;
            } else if (matchKeyword(TokenType::KW_BY)) {
                consumeKeyword(TokenType::KW_DEFAULT, "Expected DEFAULT");
                col.identity_always = false;
            }
            consumeKeyword(TokenType::KW_AS, "Expected AS");

            if (matchKeyword(TokenType::KW_IDENTITY)) {
                col.is_identity = true;
                col.is_generated = false;
            } else {
                consume(TokenType::LEFT_PAREN, "Expected (");
                col.generated_expr_bytecode = captureExpressionBytecode();
                consume(TokenType::RIGHT_PAREN, "Expected )");
                if (matchKeyword(TokenType::KW_STORED)) {
                    col.generated_stored = true;
                } else if (matchKeyword(TokenType::KW_VIRTUAL)) {
                    col.generated_stored = false;
                }
            }
        } else if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            bool prev_emit = emit_enabled_;
            emit_enabled_ = false;
            parseExpression();
            emit_enabled_ = prev_emit;
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else if (matchKeyword(TokenType::KW_REFERENCES)) {
            // Inline foreign key
            std::string ref_table = parseQualifiedName();
            if (match(TokenType::LEFT_PAREN)) {
                parseIdentifier();  // ref column
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
            // ON DELETE/UPDATE
            while (matchKeyword(TokenType::KW_ON)) {
                if (matchKeyword(TokenType::KW_DELETE) || matchKeyword(TokenType::KW_UPDATE)) {
                    // CASCADE, SET NULL, SET DEFAULT, RESTRICT, NO ACTION
                    matchKeyword(TokenType::KW_CASCADE) ||
                    matchKeyword(TokenType::KW_RESTRICT) ||
                    (matchKeyword(TokenType::KW_SET) &&
                     (matchKeyword(TokenType::KW_NULL) || matchKeyword(TokenType::KW_DEFAULT))) ||
                    (matchKeyword(TokenType::KW_NO) && matchKeyword(TokenType::KW_ACTION));
                }
            }
        } else if (matchKeyword(TokenType::KW_COLLATE)) {
            col.collation = parseIdentifier();
        } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            parseIdentifier();  // Skip constraint name, reparse
        } else {
            break;
        }
    }

    return col;
}

PgDataType Parser::parseDataType() {
    PgDataType type;

    // Check for type name
    if (matchKeyword(TokenType::KW_SMALLINT) || matchKeyword(TokenType::KW_INT2)) {
        type.kind = PgDataType::Kind::SMALLINT;
    } else if (matchKeyword(TokenType::KW_INTEGER) || matchKeyword(TokenType::KW_INT) ||
               matchKeyword(TokenType::KW_INT4)) {
        type.kind = PgDataType::Kind::INTEGER;
    } else if (matchKeyword(TokenType::KW_BIGINT) || matchKeyword(TokenType::KW_INT8)) {
        type.kind = PgDataType::Kind::BIGINT;
    } else if (matchKeyword(TokenType::KW_INT128)) {
        type.kind = PgDataType::Kind::INT128;
    } else if (matchKeyword(TokenType::KW_UINT128)) {
        type.kind = PgDataType::Kind::UINT128;
    } else if (matchKeyword(TokenType::KW_REAL) || matchKeyword(TokenType::KW_FLOAT4)) {
        type.kind = PgDataType::Kind::REAL;
    } else if (matchKeyword(TokenType::KW_DOUBLE)) {
        matchKeyword(TokenType::KW_PRECISION);
        type.kind = PgDataType::Kind::DOUBLE_PRECISION;
    } else if (matchKeyword(TokenType::KW_FLOAT8)) {
        type.kind = PgDataType::Kind::DOUBLE_PRECISION;
    } else if (matchKeyword(TokenType::KW_NUMERIC) || matchKeyword(TokenType::KW_DECIMAL)) {
        type.kind = PgDataType::Kind::NUMERIC;
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                type.precision = static_cast<int>(current_token_.value.int_value);
                advance();
            }
            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    type.scale = static_cast<int>(current_token_.value.int_value);
                    advance();
                }
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    } else if (matchKeyword(TokenType::KW_MONEY)) {
        type.kind = PgDataType::Kind::MONEY;
    } else if (matchKeyword(TokenType::KW_SMALLSERIAL) || matchKeyword(TokenType::KW_SERIAL2)) {
        type.kind = PgDataType::Kind::SMALLSERIAL;
    } else if (matchKeyword(TokenType::KW_SERIAL) || matchKeyword(TokenType::KW_SERIAL4)) {
        type.kind = PgDataType::Kind::SERIAL;
    } else if (matchKeyword(TokenType::KW_BIGSERIAL) || matchKeyword(TokenType::KW_SERIAL8)) {
        type.kind = PgDataType::Kind::BIGSERIAL;
    } else if (matchKeyword(TokenType::KW_CHAR) || matchKeyword(TokenType::KW_CHARACTER)) {
        if (matchKeyword(TokenType::KW_VARYING)) {
            type.kind = PgDataType::Kind::VARCHAR;
        } else {
            type.kind = PgDataType::Kind::CHAR;
        }
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                type.length = static_cast<int>(current_token_.value.int_value);
                advance();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    } else if (matchKeyword(TokenType::KW_VARCHAR)) {
        type.kind = PgDataType::Kind::VARCHAR;
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                type.length = static_cast<int>(current_token_.value.int_value);
                advance();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    } else if (matchKeyword(TokenType::KW_TEXT)) {
        type.kind = PgDataType::Kind::TEXT;
    } else if (matchKeyword(TokenType::KW_BYTEA)) {
        type.kind = PgDataType::Kind::BYTEA;
    } else if (matchKeyword(TokenType::KW_DATE)) {
        type.kind = PgDataType::Kind::DATE;
    } else if (matchKeyword(TokenType::KW_TIME)) {
        type.kind = PgDataType::Kind::TIME;
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                type.precision = static_cast<int>(current_token_.value.int_value);
                advance();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        if (matchKeyword(TokenType::KW_WITH)) {
            consumeKeyword(TokenType::KW_TIME, "Expected TIME");
            consumeKeyword(TokenType::KW_ZONE, "Expected ZONE");
            type.kind = PgDataType::Kind::TIMETZ;
            type.with_time_zone = true;
        } else if (matchKeyword(TokenType::KW_WITHOUT)) {
            consumeKeyword(TokenType::KW_TIME, "Expected TIME");
            consumeKeyword(TokenType::KW_ZONE, "Expected ZONE");
        }
    } else if (matchKeyword(TokenType::KW_TIMESTAMP)) {
        type.kind = PgDataType::Kind::TIMESTAMP;
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                type.precision = static_cast<int>(current_token_.value.int_value);
                advance();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        if (matchKeyword(TokenType::KW_WITH)) {
            consumeKeyword(TokenType::KW_TIME, "Expected TIME");
            consumeKeyword(TokenType::KW_ZONE, "Expected ZONE");
            type.kind = PgDataType::Kind::TIMESTAMPTZ;
            type.with_time_zone = true;
        } else if (matchKeyword(TokenType::KW_WITHOUT)) {
            consumeKeyword(TokenType::KW_TIME, "Expected TIME");
            consumeKeyword(TokenType::KW_ZONE, "Expected ZONE");
        }
    } else if (matchKeyword(TokenType::KW_INTERVAL)) {
        type.kind = PgDataType::Kind::INTERVAL;
    } else if (matchKeyword(TokenType::KW_BOOLEAN) || matchKeyword(TokenType::KW_BOOL)) {
        type.kind = PgDataType::Kind::BOOLEAN;
    } else if (matchKeyword(TokenType::KW_UUID)) {
        type.kind = PgDataType::Kind::UUID;
    } else if (matchKeyword(TokenType::KW_JSON)) {
        type.kind = PgDataType::Kind::JSON;
    } else if (matchKeyword(TokenType::KW_JSONB)) {
        type.kind = PgDataType::Kind::JSONB;
    } else if (matchKeyword(TokenType::KW_XML)) {
        type.kind = PgDataType::Kind::XML;
    } else if (matchKeyword(TokenType::KW_POINT)) {
        type.kind = PgDataType::Kind::POINT;
    } else if (matchKeyword(TokenType::KW_LINE)) {
        type.kind = PgDataType::Kind::LINE;
    } else if (matchKeyword(TokenType::KW_LSEG)) {
        type.kind = PgDataType::Kind::LSEG;
    } else if (matchKeyword(TokenType::KW_BOX)) {
        type.kind = PgDataType::Kind::BOX;
    } else if (matchKeyword(TokenType::KW_PATH)) {
        type.kind = PgDataType::Kind::PATH;
    } else if (matchKeyword(TokenType::KW_POLYGON)) {
        type.kind = PgDataType::Kind::POLYGON;
    } else if (matchKeyword(TokenType::KW_CIRCLE)) {
        type.kind = PgDataType::Kind::CIRCLE;
    } else if (matchKeyword(TokenType::KW_CIDR)) {
        type.kind = PgDataType::Kind::CIDR;
    } else if (matchKeyword(TokenType::KW_INET)) {
        type.kind = PgDataType::Kind::INET;
    } else if (matchKeyword(TokenType::KW_MACADDR)) {
        type.kind = PgDataType::Kind::MACADDR;
    } else if (matchKeyword(TokenType::KW_MACADDR8)) {
        type.kind = PgDataType::Kind::MACADDR8;
    } else if (matchKeyword(TokenType::KW_BIT)) {
        if (matchKeyword(TokenType::KW_VARYING)) {
            type.kind = PgDataType::Kind::VARBIT;
        } else {
            type.kind = PgDataType::Kind::BIT;
        }
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::INTEGER_LITERAL)) {
                type.length = static_cast<int>(current_token_.value.int_value);
                advance();
            }
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    } else if (matchKeyword(TokenType::KW_TSVECTOR)) {
        type.kind = PgDataType::Kind::TSVECTOR;
    } else if (matchKeyword(TokenType::KW_TSQUERY)) {
        type.kind = PgDataType::Kind::TSQUERY;
    } else {
        // User-defined type or domain
        type.type_name = parseQualifiedName();
        type.kind = PgDataType::Kind::DOMAIN;
    }

    // Check for array modifier
    int array_dimensions = 0;
    int array_size = 0;
    while (match(TokenType::LEFT_BRACKET)) {
        array_dimensions++;
        if (check(TokenType::INTEGER_LITERAL)) {
            if (array_dimensions == 1) {
                array_size = static_cast<int>(current_token_.value.int_value);
            }
            advance();  // Array dimension
        }
        consume(TokenType::RIGHT_BRACKET, "Expected ]");
    }
    if (array_dimensions > 0) {
        PgDataType base = type;
        type.kind = PgDataType::Kind::ARRAY;
        type.element_kind = base.kind;
        type.element_type = base.type_name;
        type.array_dimensions = array_dimensions;
        type.array_size = array_size;
        type.length = base.length;
        type.precision = base.precision;
        type.scale = base.scale;
        type.with_time_zone = base.with_time_zone;
        type.nullable = base.nullable;
    }

    return type;
}

ForeignKeyDef Parser::parseForeignKeyDef() {
    ForeignKeyDef fk;

    // (columns)
    consume(TokenType::LEFT_PAREN, "Expected (");
    do {
        fk.columns.push_back(parseIdentifier());
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected )");

    // REFERENCES table (columns)
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

    // MATCH type
    if (matchKeyword(TokenType::KW_MATCH)) {
        if (matchKeyword(TokenType::KW_FULL)) {
            fk.match_type = "FULL";
        } else if (matchKeyword(TokenType::KW_PARTIAL)) {
            fk.match_type = "PARTIAL";
        } else if (matchKeyword(TokenType::KW_SIMPLE)) {
            fk.match_type = "SIMPLE";
        }
    }

    // ON DELETE/UPDATE
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

    // DEFERRABLE
    if (matchKeyword(TokenType::KW_DEFERRABLE)) {
        fk.deferrable = true;
        if (matchKeyword(TokenType::KW_INITIALLY)) {
            if (matchKeyword(TokenType::KW_DEFERRED)) {
                fk.initially_deferred = true;
            } else {
                matchKeyword(TokenType::KW_IMMEDIATE);
            }
        }
    } else if (matchKeyword(TokenType::KW_NOT)) {
        consumeKeyword(TokenType::KW_DEFERRABLE, "Expected DEFERRABLE");
        fk.deferrable = false;
    }

    return fk;
}

// ============================================================================
// CREATE INDEX
// ============================================================================

void Parser::parseCreateIndex() {
    bool unique = pending_index_unique_;
    pending_index_unique_ = false;

    // CONCURRENTLY
    matchKeyword(TokenType::KW_CONCURRENTLY);

    // IF NOT EXISTS
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
    }

    // Index name (optional for inline definitions)
    std::string index_name;
    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        index_name = parseIdentifier();
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");

    // Table name
    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);
    std::string table_path = schema + "/" + table;

    // USING method
    uint8_t index_type = 0xFF;
    if (matchKeyword(TokenType::KW_USING)) {
        std::string method_name = parseIdentifier();
        std::transform(method_name.begin(), method_name.end(), method_name.begin(), ::tolower);
        if (method_name == "btree") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
        } else if (method_name == "hash") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::HASH);
        } else if (method_name == "gin") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIN);
        } else if (method_name == "gist") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::GIST);
        } else if (method_name == "spgist") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::SPGIST);
        } else if (method_name == "brin") {
            index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BRIN);
        }
    }

    // Column list
    consume(TokenType::LEFT_PAREN, "Expected (");
    std::vector<std::string> columns;
    bool has_expressions = false;

    do {
        if (match(TokenType::LEFT_PAREN)) {
            has_expressions = true;
            bool prev_emit = emit_enabled_;
            emit_enabled_ = false;
            parseExpression();
            emit_enabled_ = prev_emit;
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            columns.push_back(parseIdentifier());
        }

        if (matchKeyword(TokenType::KW_COLLATE)) {
            parseIdentifier();
        }

        if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
            parseIdentifier();  // Operator class
        }

        if (matchKeyword(TokenType::KW_DESC)) {
            // DESC
        } else {
            matchKeyword(TokenType::KW_ASC);
        }

        if (matchKeyword(TokenType::KW_NULLS)) {
            matchKeyword(TokenType::KW_FIRST) || matchKeyword(TokenType::KW_LAST);
        }
    } while (match(TokenType::COMMA));

    consume(TokenType::RIGHT_PAREN, "Expected )");

    std::vector<std::string> include_columns;
    if (matchKeyword(TokenType::KW_INCLUDE)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            include_columns.push_back(parseIdentifier());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    bool has_predicate = false;
    std::vector<uint8_t> predicate_bytecode;
    if (matchKeyword(TokenType::KW_WHERE)) {
        has_predicate = true;
        predicate_bytecode = captureExpressionBytecode();
    }

    std::string tablespace_name;
    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        tablespace_name = parseIdentifier();
    }

    bool emit_expressions = false;
    if (has_expressions) {
        error("PostgreSQL expression indexes are not supported in current bytecode yet");
    }
    emit(sblr::Opcode::CREATE_INDEX);
    emitString(index_name);
    emitString(table_path);
    emitByte(unique ? 1 : 0);
    emitU32(static_cast<uint32_t>(columns.size()));
    for (const auto& col : columns) {
        emitString(col);
    }
    emitU32(static_cast<uint32_t>(include_columns.size()));
    for (const auto& col : include_columns) {
        emitString(col);
    }
    emitString(tablespace_name);
    emitByte(index_type);
    emitU32(0);  // options_flags
    emitByte(emit_expressions ? 1 : 0);
    emitByte(has_predicate ? 1 : 0);
    if (emit_expressions) {
        emitU32(0);
        emitU32(0);
    }
    if (has_predicate) {
        emitU32(static_cast<uint32_t>(predicate_bytecode.size()));
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(),
                             predicate_bytecode.begin(),
                             predicate_bytecode.end());
        }
        emitString("");
    }
}

// ============================================================================
// Other CREATE statements (stubs for now)
// ============================================================================

void Parser::parseCreateView() {
    emit(sblr::Opcode::CREATE_VIEW);

    bool or_replace = pending_or_replace_;
    pending_or_replace_ = false;
    bool is_temp = pending_create_temp_;
    pending_create_temp_ = false;

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    (void)if_not_exists;

    std::string schema;
    std::string view_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = view_name;
        view_name = parseIdentifier();
    }
    resolveTableName(schema, view_name);
    std::string view_path = schema.empty() ? view_name : schema + "/" + view_name;

    // Column list
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

    // WITH CHECK OPTION
    bool check_option = false;
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_CHECK)) {
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        } else if (matchKeyword(TokenType::KW_LOCAL)) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        } else if (matchKeyword(TokenType::KW_CASCADED)) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            check_option = true;
        }
    }

    uint8_t flags = 0;
    if (or_replace) {
        flags |= 0x01;
    }
    if (check_option) {
        flags |= 0x02;
    }
    if (!column_names.empty()) {
        flags |= 0x04;
    }
    if (is_temp) {
        flags |= 0x20;
    }
    if (if_not_exists) {
        flags |= 0x40;
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

void Parser::parseCreateMaterializedView() {
    emit(sblr::Opcode::CREATE_VIEW);
    bool or_replace = pending_or_replace_;
    pending_or_replace_ = false;

    // Similar to CREATE VIEW but materialized
    std::string schema;
    std::string view_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = view_name;
        view_name = parseIdentifier();
    }
    resolveTableName(schema, view_name);
    std::string view_path = schema.empty() ? view_name : schema + "/" + view_name;

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

    // WITH [NO] DATA
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_DATA, "Expected DATA");
        } else {
            consumeKeyword(TokenType::KW_DATA, "Expected DATA");
        }
    }

    uint8_t flags = 0;
    if (or_replace) {
        flags |= 0x01;
    }
    flags |= 0x08;  // materialized

    emitString(view_path);
    emitByte(flags);
    emitString(definition);
}

void Parser::parseCreateSequence() {
    emit(sblr::Opcode::CREATE_SEQUENCE);

    bool non_durable = pending_create_temp_ || pending_create_unlogged_;
    pending_create_temp_ = false;
    pending_create_unlogged_ = false;

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    (void)if_not_exists;

    std::string schema;
    std::string seq_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = seq_name;
        seq_name = parseIdentifier();
    }
    resolveTableName(schema, seq_name);
    emitString(schema + "/" + seq_name);

    auto parse_int64 = [&]() -> int64_t {
        bool negative = false;
        if (match(TokenType::MINUS)) {
            negative = true;
        } else {
            match(TokenType::PLUS);
        }
        if (!check(TokenType::INTEGER_LITERAL)) {
            error("Expected integer literal in CREATE SEQUENCE option");
            return 0;
        }
        int64_t value = current_token_.value.int_value;
        advance();
        return negative ? -value : value;
    };

    bool has_increment = false;
    bool has_minvalue = false;
    bool has_maxvalue = false;
    bool has_start = false;
    bool has_cache = false;
    bool has_cycle = false;
    int64_t increment_by = 1;
    int64_t min_value = 1;
    int64_t max_value = 0;
    int64_t start_value = 0;
    int64_t cache_size = 1;
    bool cycle = false;
    bool has_owned_by = false;
    std::string owned_by_table;
    std::string owned_by_column;

    // Sequence options
    while (true) {
        if (matchKeyword(TokenType::KW_START)) {
            matchKeyword(TokenType::KW_WITH);
            start_value = parse_int64();
            has_start = true;
        } else if (matchIdentifierKeyword("INCREMENT")) {
            matchKeyword(TokenType::KW_BY);
            increment_by = parse_int64();
            has_increment = true;
        } else if (matchIdentifierKeyword("MINVALUE")) {
            min_value = parse_int64();
            has_minvalue = true;
        } else if (matchKeyword(TokenType::KW_NO)) {
            if (matchIdentifierKeyword("MINVALUE")) {
                has_minvalue = false;
            } else if (matchIdentifierKeyword("MAXVALUE")) {
                has_maxvalue = false;
            } else if (matchIdentifierKeyword("CYCLE")) {
                has_cycle = true;
                cycle = false;
            }
        } else if (matchIdentifierKeyword("MAXVALUE")) {
            max_value = parse_int64();
            has_maxvalue = true;
        } else if (matchIdentifierKeyword("CYCLE")) {
            has_cycle = true;
            cycle = true;
        } else if (matchIdentifierKeyword("CACHE")) {
            cache_size = parse_int64();
            has_cache = true;
        } else if (matchKeyword(TokenType::KW_OWNED)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY");
            if (matchKeyword(TokenType::KW_NONE)) {
                // Not owned
                has_owned_by = false;
            } else {
                std::string part1 = parseIdentifier();
                consume(TokenType::DOT, "Expected '.' after table name");
                std::string part2 = parseIdentifier();
                std::string schema;
                std::string table;
                std::string column;
                if (match(TokenType::DOT)) {
                    schema = part1;
                    table = part2;
                    column = parseIdentifier();
                } else {
                    table = part1;
                    column = part2;
                }
                resolveTableName(schema, table);
                owned_by_table = schema.empty() ? table : (schema + "/" + table);
                owned_by_column = column;
                has_owned_by = true;
            }
        } else {
            break;
        }
    }

    uint8_t flags = 0;
    if (has_increment) flags |= 0x01;
    if (has_minvalue) flags |= 0x02;
    if (has_maxvalue) flags |= 0x04;
    if (has_start) flags |= 0x08;
    if (has_cache) flags |= 0x10;
    if (has_cycle) flags |= 0x20;
    if (if_not_exists) flags |= 0x40;
    if (non_durable) flags |= 0x80;

    emitByte(flags);
    if (has_increment) emitI64(increment_by);
    if (has_minvalue) emitI64(min_value);
    if (has_maxvalue) emitI64(max_value);
    if (has_start) emitI64(start_value);
    if (has_cache) emitI64(cache_size);
    if (has_cycle) emitByte(cycle ? 1 : 0);

    uint8_t flags2 = 0;
    if (has_owned_by) flags2 |= 0x01;
    emitByte(flags2);
    if (has_owned_by) {
        emitString(owned_by_table);
        emitString(owned_by_column);
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

    // Database options
    if (matchKeyword(TokenType::KW_WITH)) {
        while (true) {
            if (matchKeyword(TokenType::KW_OWNER)) {
                match(TokenType::EQUAL);
                std::string owner = parseIdentifier();
                options.emplace_back("owner", owner);
            } else if (matchKeyword(TokenType::KW_TEMPLATE)) {
                match(TokenType::EQUAL);
                std::string tmpl = parseIdentifier();
                options.emplace_back("template", tmpl);
            } else if (matchKeyword(TokenType::KW_ENCODING)) {
                match(TokenType::EQUAL);
                if (check(TokenType::STRING_LITERAL)) {
                    std::string value = std::string(stringPool().get(current_token_.value.string_id));
                    advance();
                    options.emplace_back("encoding", value);
                } else {
                    std::string value = parseIdentifier();
                    options.emplace_back("encoding", value);
                }
            } else if (matchKeyword(TokenType::KW_TABLESPACE)) {
                match(TokenType::EQUAL);
                std::string value = parseIdentifier();
                error("TABLESPACE options are not supported in emulated parsers");
            } else {
                break;
            }
        }
    }

    emitU32(static_cast<uint32_t>(options.size()));
    for (const auto& opt : options) {
        emitString(opt.first);
        emitString(opt.second);
    }
    emitU32(0);  // alias count
}

void Parser::parseCreateSchema() {
    std::string schema_name;
    std::string owner_name;

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }

    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        schema_name = parseIdentifier();
    }

    if (matchKeyword(TokenType::KW_AUTHORIZATION)) {
        owner_name = parseIdentifier();
    }

    if (schema_name.empty() && !owner_name.empty()) {
        schema_name = owner_name;
    }
    if (schema_name.empty()) {
        error("Expected schema name");
        return;
    }

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

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_SCHEMA));
    emitByte(if_not_exists ? 0x01 : 0x00);
    std::string schema_path = normalize_path(default_schema_);
    if (!schema_path.empty()) {
        schema_path.push_back('.');
    }
    schema_path += schema_name;
    emitString(schema_path);
    emitString(owner_name);
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

    // Flags: bit0 = OR REPLACE, bit2 = SQL SECURITY DEFINER (not parsed yet), bit3 = dependency list
    uint8_t flags = 0;
    size_t flags_pos = bytecode_.size();
    emitByte(flags);
    emitString(func_name);

    consume(TokenType::LEFT_PAREN, "Expected (");

    std::vector<PgDataType> param_types;
    std::vector<std::string> param_names;
    std::vector<uint8_t> param_modes;

    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            uint8_t mode = 0; // IN
            if (matchKeyword(TokenType::KW_OUT)) mode = 1;
            else if (matchKeyword(TokenType::KW_INOUT)) mode = 2;
            else if (matchKeyword(TokenType::KW_IN)) mode = 0;

            param_modes.push_back(mode);

            std::string param_name = parseIdentifier();
            param_names.push_back(param_name);

            PgDataType param_type = parseDataType();
            param_types.push_back(param_type);

            if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                parseExpression(); // ignore value
            }
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_PAREN, "Expected )");

    emitByte(static_cast<uint8_t>(param_names.size()));
    for (size_t i = 0; i < param_names.size(); ++i) {
        emitByte(param_modes[i]);
        emitString(param_names[i]);
        emitByte(encodeDataType(param_types[i]));
        emitU32(param_types[i].precision);
        emitU32(param_types[i].scale);
    }

    PgDataType return_type{};
    if (matchKeyword(TokenType::KW_RETURNS)) {
        if (matchKeyword(TokenType::KW_TABLE)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else if (matchKeyword(TokenType::KW_SETOF)) {
            return_type = parseDataType();
        } else {
            return_type = parseDataType();
        }
    }

    emitByte(encodeDataType(return_type));
    emitU32(return_type.precision);
    emitU32(return_type.scale);

    std::string body;
    if (matchKeyword(TokenType::KW_AS)) {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING)) {
            uint32_t id = current_token_.value.string_id;
            body = lexer_.stringPool().get(id);
            advance();
        }
    }
    emitString(body);

    std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>> deps;
    bool deps_ready = false;
    if (db_ && !body.empty()) {
        sblr::QueryCompilerV2 dep_compiler(db_);
        if (auto* catalog = db_->catalog_manager()) {
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

            std::string schema_path = normalize_path(default_schema_);
            if (!schema.empty()) {
                if (!schema_path.empty()) {
                    schema_path.push_back('.');
                }
                schema_path += schema;
            }
            if (!schema_path.empty()) {
                core::CatalogManager::SchemaInfo schema_info;
                if (catalog->getSchema(schema_path, schema_info, nullptr) == core::Status::OK) {
                    dep_compiler.setCurrentSchema(schema_info.schema_id);
                }
            }
        }

        auto dep_result = dep_compiler.compile(body);
        if (dep_result.success()) {
            deps = dep_result.dependencies();
            deps_ready = true;
        }
    }

    if (deps_ready) {
        bytecode_[flags_pos] |= 0x08;
        emitU32(static_cast<uint32_t>(deps.size()));
        for (const auto& dep : deps) {
            for (auto byte : dep.first.bytes) {
                emitByte(byte);
            }
            emitByte(static_cast<uint8_t>(dep.second));
        }
    }
}

void Parser::parseCreateProcedure() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_PROCEDURE_STMT));

    std::string proc_name = parseIdentifier();

    uint8_t flags = 0;
    size_t flags_pos = bytecode_.size();
    emitByte(flags);
    emitString(proc_name);

    consume(TokenType::LEFT_PAREN, "Expected (");
    std::vector<PgDataType> param_types;
    std::vector<std::string> param_names;
    std::vector<uint8_t> param_modes;

    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            uint8_t mode = 0; // IN
            if (matchKeyword(TokenType::KW_OUT)) mode = 1;
            else if (matchKeyword(TokenType::KW_INOUT)) mode = 2;
            else if (matchKeyword(TokenType::KW_IN)) mode = 0;

            param_modes.push_back(mode);
            std::string param_name = parseIdentifier();
            param_names.push_back(param_name);
            PgDataType param_type = parseDataType();
            param_types.push_back(param_type);

            if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                parseExpression(); // ignore
            }
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_PAREN, "Expected )");

    emitByte(static_cast<uint8_t>(param_names.size()));
    for (size_t i = 0; i < param_names.size(); ++i) {
        emitByte(param_modes[i]);
        emitString(param_names[i]);
        emitByte(encodeDataType(param_types[i]));
        emitU32(param_types[i].precision);
        emitU32(param_types[i].scale);
    }

    std::string body;
    if (matchKeyword(TokenType::KW_AS)) {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING)) {
            uint32_t id = current_token_.value.string_id;
            body = lexer_.stringPool().get(id);
            advance();
        }
    }
    emitString(body);

    std::vector<std::pair<core::ID, core::CatalogManager::ObjectType>> deps;
    bool deps_ready = false;
    if (db_ && !body.empty()) {
        sblr::QueryCompilerV2 dep_compiler(db_);
        if (auto* catalog = db_->catalog_manager()) {
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

            std::string schema_path = normalize_path(default_schema_);
            if (!schema_path.empty()) {
                core::CatalogManager::SchemaInfo schema_info;
                if (catalog->getSchema(schema_path, schema_info, nullptr) == core::Status::OK) {
                    dep_compiler.setCurrentSchema(schema_info.schema_id);
                }
            }
        }

        auto dep_result = dep_compiler.compile(body);
        if (dep_result.success()) {
            deps = dep_result.dependencies();
            deps_ready = true;
        }
    }

    if (deps_ready) {
        bytecode_[flags_pos] |= 0x08;
        emitU32(static_cast<uint32_t>(deps.size()));
        for (const auto& dep : deps) {
            for (auto byte : dep.first.bytes) {
                emitByte(byte);
            }
            emitByte(static_cast<uint8_t>(dep.second));
        }
    }
}

void Parser::parseCreateTrigger() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_TRIGGER));

    std::string trigger_name = parseIdentifier();
    emitString(trigger_name);

    // BEFORE/AFTER/INSTEAD OF
    uint8_t timing = 0;
    if (matchKeyword(TokenType::KW_BEFORE)) {
        timing = static_cast<uint8_t>(core::CatalogManager::TriggerTiming::BEFORE);
    } else if (matchKeyword(TokenType::KW_AFTER)) {
        timing = static_cast<uint8_t>(core::CatalogManager::TriggerTiming::AFTER);
    } else if (matchKeyword(TokenType::KW_INSTEAD)) {
        consumeKeyword(TokenType::KW_OF, "Expected OF");
        timing = 2;  // Unsupported INSTEAD OF (kept distinct from BEFORE/AFTER)
    }
    emitByte(timing);

    // Event (INSERT/UPDATE/DELETE/TRUNCATE)
    uint8_t event_mask = 0;
    do {
        if (matchKeyword(TokenType::KW_INSERT)) {
            event_mask |= 1u << static_cast<uint8_t>(core::CatalogManager::TriggerEvent::INSERT);
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            event_mask |= 1u << static_cast<uint8_t>(core::CatalogManager::TriggerEvent::UPDATE);
            // OF columns
            if (matchKeyword(TokenType::KW_OF)) {
                do {
                    parseIdentifier();
                } while (match(TokenType::COMMA));
            }
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            event_mask |= 1u << static_cast<uint8_t>(core::CatalogManager::TriggerEvent::DELETE);
        } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
            event_mask |= 1u << 3;
        }
    } while (matchKeyword(TokenType::KW_OR));
    emitByte(event_mask);

    consumeKeyword(TokenType::KW_ON, "Expected ON");
    std::string table_name = parseQualifiedName();
    emitString(table_name);

    // FOR EACH ROW/STATEMENT
    if (matchKeyword(TokenType::KW_FOR)) {
        matchKeyword(TokenType::KW_EACH);
        if (matchKeyword(TokenType::KW_ROW)) {
            emitByte(1);
        } else if (matchKeyword(TokenType::KW_STATEMENT)) {
            emitByte(2);
        }
    }

    // WHEN condition
    if (matchKeyword(TokenType::KW_WHEN)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    // EXECUTE FUNCTION/PROCEDURE
    consumeKeyword(TokenType::KW_EXECUTE, "Expected EXECUTE");
    matchKeyword(TokenType::KW_FUNCTION) || matchKeyword(TokenType::KW_PROCEDURE);
    std::string func_name = parseQualifiedName();
    emitString(func_name);
    consume(TokenType::LEFT_PAREN, "Expected (");
    // Arguments
    consume(TokenType::RIGHT_PAREN, "Expected )");
}

void Parser::parseCreateType() {
    // Spec: docs/specifications/SBLR_DOMAIN_PAYLOADS.md
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }

    auto name_pair = splitQualifiedName(parseQualifiedName());
    std::string schema = name_pair.first;
    std::string type_name = name_pair.second;
    resolveTableName(schema, type_name);
    std::string type_path = schema.empty() ? type_name : (schema + "." + type_name);

    consumeKeyword(TokenType::KW_AS, "Expected AS");

    auto is_supported_type = [](PgDataType::Kind kind) {
        switch (kind) {
            case PgDataType::Kind::SMALLINT:
            case PgDataType::Kind::INTEGER:
            case PgDataType::Kind::SERIAL:
            case PgDataType::Kind::SMALLSERIAL:
            case PgDataType::Kind::BIGINT:
            case PgDataType::Kind::BIGSERIAL:
            case PgDataType::Kind::REAL:
            case PgDataType::Kind::DOUBLE_PRECISION:
            case PgDataType::Kind::DECIMAL:
            case PgDataType::Kind::NUMERIC:
            case PgDataType::Kind::MONEY:
            case PgDataType::Kind::CHAR:
            case PgDataType::Kind::VARCHAR:
            case PgDataType::Kind::TEXT:
            case PgDataType::Kind::BYTEA:
            case PgDataType::Kind::DATE:
            case PgDataType::Kind::TIME:
            case PgDataType::Kind::TIMETZ:
            case PgDataType::Kind::TIMESTAMP:
            case PgDataType::Kind::TIMESTAMPTZ:
            case PgDataType::Kind::BOOLEAN:
            case PgDataType::Kind::UUID:
            case PgDataType::Kind::JSON:
            case PgDataType::Kind::JSONB:
                return true;
            default:
                return false;
        }
    };

    auto emit_domain_header = [&](uint8_t domain_kind) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));
        uint8_t flags = if_not_exists ? 0x01 : 0x00;
        emitByte(flags);
        emitByte(domain_kind);
        emitString(type_path);
    };

    auto emit_type_ref = [&](const PgDataType& type) {
        emitByte(0);  // base type ref
        emitTypeDefinition(type);
    };

    if (matchKeyword(TokenType::KW_ENUM)) {
        // Map ENUM type to a ScratchBird ENUM domain.
        std::vector<std::string> labels;
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            if (check(TokenType::STRING_LITERAL)) {
                uint32_t id = current_token_.value.string_id;
                labels.emplace_back(lexer_.stringPool().get(id));
                advance();
            } else {
                error("Expected string literal in ENUM definition");
                synchronize();
                return;
            }
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");

        emit_domain_header(2);
        emitU32(static_cast<uint32_t>(labels.size()));
        uint32_t position = 1;
        for (const auto& label : labels) {
            emitString(label);
            emitU32(position++);
        }
        emitByte(0);  // ENUM wrap disabled

        emitByte(1);  // nullable
        emitString("");  // default
        emitString("");  // collation
        emitU32(0);  // constraints
        emitByte(0);  // no inherits
        emitString("postgresql");
        emitString("");
        return;
    }

    if (matchKeyword(TokenType::KW_RANGE)) {
        error("PostgreSQL CREATE TYPE RANGE is not supported in ScratchBird parser");
        synchronize();
        return;
    }

    if (match(TokenType::LEFT_PAREN)) {
        struct RecordFieldDef {
            std::string name;
            PgDataType type;
        };
        std::vector<RecordFieldDef> fields;
        do {
            RecordFieldDef field;
            field.name = parseIdentifier();
            field.type = parseDataType();
            if (!is_supported_type(field.type.kind)) {
                error("PostgreSQL composite types only support scalar base types in this release");
                synchronize();
                return;
            }
            if (matchKeyword(TokenType::KW_COLLATE)) {
                parseIdentifier();
            }
            if (check(TokenType::KW_NOT) || check(TokenType::KW_NULL) ||
                check(TokenType::KW_DEFAULT) || check(TokenType::KW_CONSTRAINT) ||
                check(TokenType::KW_CHECK)) {
                error("PostgreSQL composite type attributes do not support constraints in this parser");
                synchronize();
                return;
            }
            fields.push_back(std::move(field));
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");

        emit_domain_header(1);
        emitU32(static_cast<uint32_t>(fields.size()));
        for (const auto& field : fields) {
            emitString(field.name);
            emit_type_ref(field.type);
            emitByte(1);  // field nullable
            emitString("");  // default
        }

        emitByte(1);  // nullable
        emitString("");  // default
        emitString("");  // collation
        emitU32(0);  // constraints
        emitByte(0);  // no inherits
        emitString("postgresql");
        emitString("");
        return;
    }

    error("Expected ENUM, RANGE, or composite definition after CREATE TYPE");
}

std::string Parser::parseExpressionText() {
    size_t bytecode_start = bytecode_.size();
    size_t start_offset = current_token_.span.start.offset;

    parseExpression();

    size_t end_offset = current_token_.span.start.offset;
    bytecode_.resize(bytecode_start);

    if (end_offset <= start_offset) {
        return {};
    }
    std::string_view text = lexer_.input().substr(start_offset, end_offset - start_offset);
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

void Parser::parseCreateDomain() {
    // Spec: docs/specifications/SBLR_DOMAIN_PAYLOADS.md
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }

    auto name_pair = splitQualifiedName(parseQualifiedName());
    std::string schema = name_pair.first;
    std::string domain_name = name_pair.second;
    resolveTableName(schema, domain_name);
    std::string domain_path = schema.empty() ? domain_name : (schema + "." + domain_name);

    uint8_t flags = if_not_exists ? 0x01 : 0x00;
    emitByte(flags);

    emitByte(0);  // BASIC domain kind
    emitString(domain_path);

    // Optional AS keyword
    matchKeyword(TokenType::KW_AS);

    PgDataType base_type = parseDataType();
    bool supported_base_type = true;
    switch (base_type.kind) {
        case PgDataType::Kind::SMALLINT:
        case PgDataType::Kind::INTEGER:
        case PgDataType::Kind::SERIAL:
        case PgDataType::Kind::SMALLSERIAL:
        case PgDataType::Kind::BIGINT:
        case PgDataType::Kind::INT128:
        case PgDataType::Kind::UINT128:
        case PgDataType::Kind::BIGSERIAL:
        case PgDataType::Kind::REAL:
        case PgDataType::Kind::DOUBLE_PRECISION:
        case PgDataType::Kind::DECIMAL:
        case PgDataType::Kind::NUMERIC:
        case PgDataType::Kind::MONEY:
        case PgDataType::Kind::CHAR:
        case PgDataType::Kind::VARCHAR:
        case PgDataType::Kind::TEXT:
        case PgDataType::Kind::BYTEA:
        case PgDataType::Kind::DATE:
        case PgDataType::Kind::TIME:
        case PgDataType::Kind::TIMETZ:
        case PgDataType::Kind::TIMESTAMP:
        case PgDataType::Kind::TIMESTAMPTZ:
        case PgDataType::Kind::BOOLEAN:
        case PgDataType::Kind::UUID:
        case PgDataType::Kind::JSON:
        case PgDataType::Kind::JSONB:
            break;
        default:
            supported_base_type = false;
            break;
    }
    if (!supported_base_type) {
        error("CREATE DOMAIN base type is not supported in PostgreSQL parser");
    }

    emitTypeDefinition(base_type);

    bool nullable = true;
    std::string default_value;
    bool has_collation = false;
    std::string collation_name;
    std::vector<std::pair<std::string, std::string>> checks;

    while (true) {
        if (matchKeyword(TokenType::KW_COLLATE)) {
            collation_name = parseIdentifier();
            has_collation = true;
            continue;
        }

        std::string constraint_name;
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            if (!check(TokenType::KW_CHECK) && !check(TokenType::KW_DEFAULT) &&
                !check(TokenType::KW_NOT) && !check(TokenType::KW_NULL)) {
                constraint_name = parseIdentifier();
            }
        }

        if (matchKeyword(TokenType::KW_DEFAULT)) {
            default_value = parseExpressionText();
            continue;
        }
        if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_NULL, "Expected NULL");
            nullable = false;
            continue;
        }
        if (matchKeyword(TokenType::KW_NULL)) {
            nullable = true;
            continue;
        }
        if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            std::string expr_text = parseExpressionText();
            consume(TokenType::RIGHT_PAREN, "Expected )");
            checks.emplace_back(constraint_name, std::move(expr_text));
            continue;
        }

        if (!constraint_name.empty()) {
            error("Expected domain constraint after CONSTRAINT");
        }
        break;
    }

    if (check(TokenType::KW_WITH) || check(TokenType::KW_INHERITS) ||
        check(TokenType::KW_SECURITY)) {
        error("PostgreSQL CREATE DOMAIN does not support WITH/INHERITS options");
        synchronize();
        return;
    }

    emitByte(nullable ? 1 : 0);
    emitString(default_value);
    emitString(has_collation ? collation_name : std::string());

    emitU32(static_cast<uint32_t>(checks.size()));
    for (const auto& constraint : checks) {
        emitByte(3);  // CHECK constraint
        emitString(constraint.first);
        emitString(constraint.second);
    }

    emitByte(0);  // no inherits
    emitString("postgresql");
    emitString("");
}

void Parser::parseAlterDomain() {
    // Spec: docs/specifications/SBLR_DOMAIN_PAYLOADS.md
    auto name_pair = splitQualifiedName(parseQualifiedName());
    std::string schema = name_pair.first;
    std::string domain_name = name_pair.second;
    resolveTableName(schema, domain_name);
    std::string domain_path = schema.empty() ? domain_name : (schema + "." + domain_name);

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

    auto emit_move = [&](const std::vector<std::string>& components,
                         const std::vector<std::string>& target_schema) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MOVE_OBJECT));
        emitByte(0);
        emitByte(static_cast<uint8_t>(core::CatalogManager::ObjectType::DOMAIN));
        emit_object_path(components);
        emit_object_path(target_schema);
        emit_string16(std::string_view());
    };

    if (matchKeyword(TokenType::KW_SET)) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DOMAIN));
            emitByte(static_cast<uint8_t>(sblr::AlterDomainAction::SET_DEFAULT));
            emitString(domain_path);
            emitString(parseExpressionText());
            return;
        }
        if (matchKeyword(TokenType::KW_SCHEMA)) {
            std::string new_schema = parseIdentifier();
            std::string dummy = "x";
            resolveTableName(new_schema, dummy);
            auto target_schema = splitPath(new_schema);

            auto object_components = splitPath(schema);
            object_components.push_back(domain_name);
            emit_move(object_components, target_schema);
            return;
        }
        error("Expected DEFAULT or SCHEMA after SET in ALTER DOMAIN");
        return;
    }

    if (matchKeyword(TokenType::KW_DROP)) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DOMAIN));
            emitByte(static_cast<uint8_t>(sblr::AlterDomainAction::DROP_DEFAULT));
            emitString(domain_path);
            return;
        }
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DOMAIN));
            emitByte(static_cast<uint8_t>(sblr::AlterDomainAction::DROP_CONSTRAINT));
            emitString(domain_path);
            emitString(parseIdentifier());
            return;
        }
        error("Expected DEFAULT or CONSTRAINT after DROP in ALTER DOMAIN");
        return;
    }

    if (matchKeyword(TokenType::KW_ADD)) {
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            if (!check(TokenType::KW_CHECK) && !check(TokenType::KW_DEFAULT) &&
                !check(TokenType::KW_NOT) && !check(TokenType::KW_NULL)) {
                parseIdentifier();
            }
        }

        if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DOMAIN));
            emitByte(static_cast<uint8_t>(sblr::AlterDomainAction::ADD_CHECK));
            emitString(domain_path);
            emitString(parseExpressionText());
            consume(TokenType::RIGHT_PAREN, "Expected )");
            return;
        }
        error("Expected CHECK after ADD in ALTER DOMAIN");
        return;
    }

    if (matchKeyword(TokenType::KW_RENAME)) {
        consumeKeyword(TokenType::KW_TO, "Expected TO");
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DOMAIN));
        emitByte(static_cast<uint8_t>(sblr::AlterDomainAction::RENAME));
        emitString(domain_path);
        emitString(parseIdentifier());
        return;
    }

    error("Expected SET, DROP, ADD, or RENAME after domain name");
}

// ============================================================================
// ALTER Statement
// ============================================================================

void Parser::parseAlterStmt() {
    consume(TokenType::KW_ALTER, "Expected ALTER");

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

    auto normalize_path = [&](const std::string& path) {
        auto parts = split_path(path);
        std::string normalized;
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) {
                normalized.push_back('.');
            }
            normalized += parts[i];
        }
        return normalized;
    };

    auto split_qualified = [&](const std::string& qualified) {
        std::vector<std::string> parts;
        std::string current;
        for (char ch : qualified) {
            if (ch == '.') {
                parts.push_back(current);
                current.clear();
            } else {
                current.push_back(ch);
            }
        }
        parts.push_back(current);
        std::string schema;
        std::string object = parts.back();
        if (parts.size() > 1) {
            for (size_t i = 0; i + 1 < parts.size(); ++i) {
                if (!schema.empty()) schema += ".";
                schema += parts[i];
            }
        }
        return std::make_pair(schema, object);
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

    auto build_schema_path_string = [&](const std::string& schema_name) {
        std::string schema_path = normalize_path(default_schema_);
        if (!schema_path.empty()) {
            schema_path.push_back('.');
        }
        return schema_path + schema_name;
    };

    auto build_database_path_string = [&](const std::string& db_name) {
        std::string db_path = buildEmulatedServerRoot(default_schema_);
        if (!db_path.empty()) {
            db_path += ".databases.";
        }
        return db_path + db_name;
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

    auto emit_rename = [&](core::CatalogManager::ObjectType object_type,
                           const std::vector<std::string>& components,
                           bool if_exists,
                           std::string_view new_name) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_RENAME_OBJECT));
        uint8_t flags = if_exists ? 0x02 : 0x00;
        emitByte(flags);
        emitByte(static_cast<uint8_t>(object_type));
        emit_object_path(components);
        emit_string16(new_name);
    };

    auto emit_move = [&](core::CatalogManager::ObjectType object_type,
                         const std::vector<std::string>& components,
                         bool if_exists,
                         const std::vector<std::string>& target_schema,
                         std::string_view new_name) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_MOVE_OBJECT));
        uint8_t flags = if_exists ? 0x02 : 0x00;
        emitByte(flags);
        emitByte(static_cast<uint8_t>(object_type));
        emit_object_path(components);
        emit_object_path(target_schema);
        emit_string16(new_name);
    };

    auto parse_rename_move = [&](core::CatalogManager::ObjectType object_type) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        auto name_pair = split_qualified(parseQualifiedName());
        auto components = build_object_path(name_pair.first, name_pair.second);

        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            std::string new_name = parseIdentifier();
            emit_rename(object_type, components, if_exists, new_name);
            return;
        }

        if (matchKeyword(TokenType::KW_SET)) {
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            std::string new_schema = parseIdentifier();
            auto target_schema = build_schema_path(new_schema);
            emit_move(object_type, components, if_exists, target_schema, std::string_view());
            return;
        }

        error("Expected RENAME TO or SET SCHEMA after object name");
    };

    if (matchKeyword(TokenType::KW_DEFAULT)) {
        consumeKeyword(TokenType::KW_PRIVILEGES, "Expected PRIVILEGES after DEFAULT");
        error("ALTER DEFAULT PRIVILEGES is not supported yet");
        return;
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

        auto table_components = build_object_path(schema, table);

        uint8_t rls_action = 0;
        bool rls_action_set = false;
        if (matchKeyword(TokenType::KW_ENABLE)) {
            rls_action = 0;
            rls_action_set = true;
        } else if (matchKeyword(TokenType::KW_DISABLE)) {
            rls_action = 1;
            rls_action_set = true;
        } else if (matchKeyword(TokenType::KW_FORCE)) {
            rls_action = 2;
            rls_action_set = true;
        } else if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_FORCE, "Expected FORCE after NO");
            rls_action = 3;
            rls_action_set = true;
        }

        if (rls_action_set) {
            consumeKeyword(TokenType::KW_ROW, "Expected ROW");
            consumeKeyword(TokenType::KW_LEVEL, "Expected LEVEL");
            consumeKeyword(TokenType::KW_SECURITY, "Expected SECURITY");
            resolveTableName(schema, table);
            std::string table_path = schema.empty() ? table : (schema + "/" + table);
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_TABLE_RLS));
            emitString(table_path);
            emitByte(rls_action);
            return;
        }

        if (matchKeyword(TokenType::KW_RENAME)) {
            if (matchKeyword(TokenType::KW_COLUMN)) {
                std::string old_name = parseIdentifier();
                consumeKeyword(TokenType::KW_TO, "Expected TO");
                std::string new_name = parseIdentifier();
                auto column_components = table_components;
                column_components.push_back(old_name);
                emit_rename(core::CatalogManager::ObjectType::COLUMN,
                            column_components, if_exists, new_name);
            } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                std::string old_name = parseIdentifier();
                consumeKeyword(TokenType::KW_TO, "Expected TO");
                std::string new_name = parseIdentifier();
                auto constraint_components = table_components;
                constraint_components.push_back(old_name);
                emit_rename(core::CatalogManager::ObjectType::CONSTRAINT,
                            constraint_components, if_exists, new_name);
            } else if (matchKeyword(TokenType::KW_TO)) {
                std::string new_name = parseIdentifier();
                emit_rename(core::CatalogManager::ObjectType::TABLE,
                            table_components, if_exists, new_name);
            } else {
                error("Expected COLUMN, CONSTRAINT, or TO after RENAME");
            }
            return;
        }

        if (matchKeyword(TokenType::KW_SET)) {
            if (matchKeyword(TokenType::KW_SCHEMA)) {
                std::string new_schema = parseIdentifier();
                auto target_schema = build_schema_path(new_schema);
                emit_move(core::CatalogManager::ObjectType::TABLE,
                          table_components, if_exists, target_schema, std::string_view());
                return;
            }
        }

        resolveTableName(schema, table);
        std::string table_path = schema.empty() ? table : (schema + "/" + table);

        auto emit_alter = [&](uint8_t action, const auto& emit_payload) {
            emit(sblr::Opcode::ALTER_TABLE);
            emitString(table_path);
            emitByte(action);
            emit_payload();
        };

        auto emit_type_payload = [&](const PgDataType& type,
                                     uint16_t& type_code,
                                     uint32_t& precision,
                                     uint32_t& scale) {
            type_code = static_cast<uint16_t>(encodeDataType(type));
            precision = 0;
            scale = 0;
            switch (type.kind) {
                case PgDataType::Kind::CHAR:
                case PgDataType::Kind::VARCHAR:
                case PgDataType::Kind::BIT:
                case PgDataType::Kind::VARBIT:
                    precision = static_cast<uint32_t>(type.length);
                    break;
                case PgDataType::Kind::DECIMAL:
                case PgDataType::Kind::NUMERIC:
                case PgDataType::Kind::MONEY:
                    precision = static_cast<uint32_t>(type.precision);
                    scale = static_cast<uint32_t>(type.scale);
                    break;
                case PgDataType::Kind::TIME:
                case PgDataType::Kind::TIMETZ:
                case PgDataType::Kind::TIMESTAMP:
                case PgDataType::Kind::TIMESTAMPTZ:
                    precision = static_cast<uint32_t>(type.precision);
                    break;
                default:
                    break;
            }
        };

        do {
            if (matchKeyword(TokenType::KW_ADD)) {
                matchKeyword(TokenType::KW_COLUMN);
                ColumnDef col = parseColumnDef();
                if (col.has_default || col.primary_key || col.unique ||
                    col.is_identity || col.is_generated || col.not_null) {
                    error("ALTER TABLE ADD COLUMN does not support constraints yet");
                    return;
                }
                if (col.type.kind == PgDataType::Kind::DOMAIN ||
                    col.type.kind == PgDataType::Kind::ARRAY) {
                    error("ALTER TABLE ADD COLUMN does not support domain or array types yet");
                    return;
                }
                emit_alter(0, [&]() {
                    uint16_t type_code = 0;
                    uint32_t precision = 0;
                    uint32_t scale = 0;
                    emit_type_payload(col.type, type_code, precision, scale);
                    emitString(col.name);
                    emitU16(type_code);
                    emitU32(precision);
                    emitU32(scale);
                    emitByte(col.not_null ? 0 : 1);
                });
            } else if (matchKeyword(TokenType::KW_DROP)) {
                if (matchKeyword(TokenType::KW_COLUMN)) {
                    bool if_exists = false;
                    if (matchKeyword(TokenType::KW_IF)) {
                        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
                        if_exists = true;
                    }
                    std::string col_name = parseIdentifier();
                    bool cascade = false;
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        cascade = true;
                    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                        cascade = false;
                    }
                    emit_alter(1, [&]() {
                        emitString(col_name);
                        emitByte(if_exists ? 1 : 0);
                        emitByte(cascade ? 1 : 0);
                    });
                } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                    error("ALTER TABLE DROP CONSTRAINT is not supported yet");
                    return;
                } else {
                    error("Expected COLUMN after DROP");
                    return;
                }
            } else if (matchKeyword(TokenType::KW_ALTER)) {
                consumeKeyword(TokenType::KW_COLUMN, "Expected COLUMN");
                std::string col_name = parseIdentifier();
                bool alter_type = false;
                if (matchKeyword(TokenType::KW_TYPE)) {
                    alter_type = true;
                } else if (matchKeyword(TokenType::KW_SET)) {
                    if (matchKeyword(TokenType::KW_DATA)) {
                        consumeKeyword(TokenType::KW_TYPE, "Expected TYPE");
                        alter_type = true;
                    } else if (matchKeyword(TokenType::KW_DEFAULT) || matchKeyword(TokenType::KW_NOT)) {
                        error("ALTER TABLE ALTER COLUMN SET DEFAULT/NOT NULL is not supported yet");
                        return;
                    }
                } else if (matchKeyword(TokenType::KW_DROP)) {
                    if (matchKeyword(TokenType::KW_DEFAULT) || matchKeyword(TokenType::KW_NOT)) {
                        error("ALTER TABLE ALTER COLUMN DROP DEFAULT/NOT NULL is not supported yet");
                        return;
                    }
                }

                if (alter_type) {
                    PgDataType type = parseDataType();
                    if (type.kind == PgDataType::Kind::DOMAIN ||
                        type.kind == PgDataType::Kind::ARRAY) {
                        error("ALTER TABLE ALTER COLUMN does not support domain or array types yet");
                        return;
                    }
                    if (matchKeyword(TokenType::KW_USING)) {
                        error("ALTER TABLE ALTER COLUMN ... USING is not supported yet");
                        return;
                    }
                    emit_alter(2, [&]() {
                        uint16_t type_code = 0;
                        uint32_t precision = 0;
                        uint32_t scale = 0;
                        emit_type_payload(type, type_code, precision, scale);
                        emitString(col_name);
                        emitU16(type_code);
                        emitU32(precision);
                        emitU32(scale);
                    });
                } else if (!alter_type) {
                    error("Expected TYPE, SET, or DROP after ALTER COLUMN");
                    return;
                }
            } else {
                error("Unsupported ALTER TABLE action");
                return;
            }
        } while (match(TokenType::COMMA));
        return;
    }

    if (matchKeyword(TokenType::KW_VIEW)) {
        parse_rename_move(core::CatalogManager::ObjectType::VIEW);
        return;
    }

    if (matchKeyword(TokenType::KW_INDEX)) {
        parse_rename_move(core::CatalogManager::ObjectType::INDEX);
        return;
    }

    if (matchKeyword(TokenType::KW_SEQUENCE)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        auto name_pair = split_qualified(parseQualifiedName());
        auto components = build_object_path(name_pair.first, name_pair.second);

        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            std::string new_name = parseIdentifier();
            emit_rename(core::CatalogManager::ObjectType::SEQUENCE,
                        components, if_exists, new_name);
            return;
        }

        if (matchKeyword(TokenType::KW_SET)) {
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            std::string new_schema = parseIdentifier();
            auto target_schema = build_schema_path(new_schema);
            emit_move(core::CatalogManager::ObjectType::SEQUENCE,
                      components, if_exists, target_schema, std::string_view());
            return;
        }

        emit(sblr::Opcode::ALTER_SEQUENCE);
        std::string seq_name = name_pair.first.empty()
                                   ? name_pair.second
                                   : (name_pair.first + "." + name_pair.second);
        emitString(seq_name);
        return;
    }

    if (matchKeyword(TokenType::KW_FUNCTION)) {
        parse_rename_move(core::CatalogManager::ObjectType::FUNCTION);
        return;
    }

    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        parse_rename_move(core::CatalogManager::ObjectType::PROCEDURE);
        return;
    }

    if (matchKeyword(TokenType::KW_TRIGGER)) {
        parse_rename_move(core::CatalogManager::ObjectType::TRIGGER);
        return;
    }

    if (matchKeyword(TokenType::KW_DOMAIN)) {
        parseAlterDomain();
        return;
    }

    if (matchKeyword(TokenType::KW_SCHEMA)) {
        std::string schema_name = parseIdentifier();
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            std::string new_name = parseIdentifier();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_SCHEMA));
            emitByte(static_cast<uint8_t>(sblr::AlterSchemaAction::RENAME));
            emitString(build_schema_path_string(schema_name));
            emitString(new_name);
            return;
        }
        if (matchKeyword(TokenType::KW_OWNER)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            std::string owner = parseIdentifier();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_SCHEMA));
            emitByte(static_cast<uint8_t>(sblr::AlterSchemaAction::SET_OWNER));
            emitString(build_schema_path_string(schema_name));
            emitString(owner);
            return;
        }
        error("Expected RENAME TO or OWNER TO after schema name");
        return;
    }

    if (matchKeyword(TokenType::KW_DATABASE)) {
        std::string db_name = parseIdentifier();
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            std::string new_name = parseIdentifier();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DATABASE));
            emitByte(static_cast<uint8_t>(sblr::AlterDatabaseAction::RENAME));
            emitString(build_database_path_string(db_name));
            emitString(new_name);
            return;
        }
        if (matchKeyword(TokenType::KW_OWNER)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            std::string owner = parseIdentifier();
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DATABASE));
            emitByte(static_cast<uint8_t>(sblr::AlterDatabaseAction::SET_OWNER));
            emitString(build_database_path_string(db_name));
            emitString(owner);
            return;
        }
        error("Expected RENAME TO or OWNER TO after database name");
        return;
    }

    error("ALTER statement for this object type not yet implemented");
}

// ============================================================================
// DROP Statement
// ============================================================================

void Parser::parseDropDomain() {
    // Spec: docs/specifications/SBLR_DOMAIN_PAYLOADS.md
    bool if_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_exists = true;
    }

    std::vector<std::string> domain_paths;
    do {
        auto name_pair = splitQualifiedName(parseQualifiedName());
        std::string schema = name_pair.first;
        std::string domain_name = name_pair.second;
        resolveTableName(schema, domain_name);
        std::string domain_path = schema.empty() ? domain_name : (schema + "." + domain_name);
        domain_paths.push_back(std::move(domain_path));
    } while (match(TokenType::COMMA));

    bool restrict = false;
    if (matchKeyword(TokenType::KW_RESTRICT)) {
        restrict = true;
    } else if (matchKeyword(TokenType::KW_CASCADE)) {
        // CASCADE not supported in domain drop payloads.
    }

    uint8_t flags = 0;
    if (if_exists) {
        flags |= 0x01;
    }
    if (restrict) {
        flags |= 0x02;
    }

    for (const auto& domain_path : domain_paths) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_DOMAIN));
        emitByte(flags);
        emitString(domain_path);
    }
}

void Parser::parseDropPolicy() {
    bool if_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_exists = true;
    }

    std::string policy_name = parseIdentifier();
    consumeKeyword(TokenType::KW_ON, "Expected ON");
    std::string table_name = parseQualifiedName();

    bool cascade = false;
    if (matchKeyword(TokenType::KW_CASCADE)) {
        cascade = true;
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
        cascade = false;
    }

    uint8_t flags = 0;
    if (if_exists) {
        flags |= 0x01;
    }
    if (cascade) {
        flags |= 0x02;
    }

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_POLICY));
    emitString(policy_name);
    emitString(table_name);
    emitByte(flags);
}

void Parser::parseDropStmt() {
    consume(TokenType::KW_DROP, "Expected DROP");

    if (matchKeyword(TokenType::KW_DOMAIN)) {
        parseDropDomain();
        return;
    }

    if (matchKeyword(TokenType::KW_POLICY)) {
        parseDropPolicy();
        return;
    }

    if (matchKeyword(TokenType::KW_DATABASE)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::string db_name = parseIdentifier();

        uint8_t flags = if_exists ? 0x01 : 0x00;
        if (matchKeyword(TokenType::KW_WITH)) {
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    std::string opt;
                    if (matchKeyword(TokenType::KW_FORCE)) {
                        opt = "FORCE";
                    } else {
                        opt = parseIdentifier();
                    }
                    std::string upper = opt;
                    std::transform(upper.begin(), upper.end(), upper.begin(),
                                   [](unsigned char ch) { return std::toupper(ch); });
                    if (upper == "FORCE") {
                        flags |= 0x02;
                    }
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
        }

        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_DATABASE));
        emitByte(flags);

        std::string db_path = buildEmulatedServerRoot(default_schema_);
        if (!db_path.empty()) {
            db_path += ".databases.";
        }
        db_path += db_name;
        emitString(db_path);
        return;
    }

    if (matchKeyword(TokenType::KW_SCHEMA)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        std::vector<std::string> schemas;
        do {
            schemas.push_back(parseQualifiedName());
        } while (match(TokenType::COMMA));

        uint8_t flags = if_exists ? 0x01 : 0x00;
        if (matchKeyword(TokenType::KW_CASCADE)) {
            flags |= 0x02;
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            flags |= 0x04;
        }

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

        for (const auto& schema_name : schemas) {
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_SCHEMA));
            emitByte(flags);
            std::string schema_path = normalize_path(default_schema_);
            if (!schema_path.empty()) {
                schema_path.push_back('.');
            }
            schema_path += schema_name;
            emitString(schema_path);
        }
        return;
    }

    if (matchKeyword(TokenType::KW_TABLE)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }

        // Table names
        std::vector<std::string> tables;
        do {
            std::string schema;
            std::string table = parseIdentifier();
            if (match(TokenType::DOT)) {
                schema = table;
                table = parseIdentifier();
            }
            resolveTableName(schema, table);
            std::string path = schema.empty() ? table : (schema + "/" + table);
            tables.push_back(std::move(path));
        } while (match(TokenType::COMMA));

        // CASCADE/RESTRICT
        bool cascade = false;
        if (matchKeyword(TokenType::KW_CASCADE)) {
            cascade = true;
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            cascade = false;
        }

        uint8_t flags = 0;
        if (if_exists) {
            flags |= 0x01;
        }
        if (cascade) {
            flags |= 0x02;
        }

        for (const auto& table_name : tables) {
            emit(sblr::Opcode::DROP_TABLE);
            emitString(table_name);
            emitByte(flags);
        }
    } else if (matchKeyword(TokenType::KW_INDEX)) {
        matchKeyword(TokenType::KW_CONCURRENTLY);

        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        std::vector<std::string> indexes;
        do {
            auto name_pair = splitQualifiedName(parseQualifiedName());
            std::string schema = name_pair.first;
            std::string index_name = name_pair.second;
            resolveTableName(schema, index_name);
            std::string path = schema.empty() ? index_name : (schema + "/" + index_name);
            indexes.push_back(std::move(path));
        } while (match(TokenType::COMMA));

        for (const auto& index_name : indexes) {
            emit(sblr::Opcode::DROP_INDEX);
            emitString(index_name);
            emitByte(if_exists ? 1 : 0);
        }
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        std::vector<std::string> views;
        do {
            auto name_pair = splitQualifiedName(parseQualifiedName());
            std::string schema = name_pair.first;
            std::string view_name = name_pair.second;
            resolveTableName(schema, view_name);
            std::string path = schema.empty() ? view_name : (schema + "/" + view_name);
            views.push_back(std::move(path));
        } while (match(TokenType::COMMA));

        bool cascade = false;
        if (matchKeyword(TokenType::KW_CASCADE)) {
            cascade = true;
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            cascade = false;
        }

        uint8_t flags = 0;
        if (if_exists) {
            flags |= 0x01;
        }
        if (cascade) {
            flags |= 0x02;
        }

        for (const auto& view_name : views) {
            emit(sblr::Opcode::DROP_VIEW);
            emitString(view_name);
            emitByte(flags);
        }
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        std::vector<std::string> sequences;
        do {
            auto name_pair = splitQualifiedName(parseQualifiedName());
            std::string schema = name_pair.first;
            std::string seq_name = name_pair.second;
            resolveTableName(schema, seq_name);
            std::string path = schema.empty() ? seq_name : (schema + "/" + seq_name);
            sequences.push_back(std::move(path));
        } while (match(TokenType::COMMA));

        bool cascade = false;
        if (matchKeyword(TokenType::KW_CASCADE)) {
            cascade = true;
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            cascade = false;
        }

        uint8_t flags = 0;
        if (cascade) {
            flags |= 0x01;
        }
        if (if_exists) {
            flags |= 0x02;
        }

        for (const auto& seq_name : sequences) {
            emit(sblr::Opcode::DROP_SEQUENCE);
            emitString(seq_name);
            emitByte(flags);
        }
    } else if (matchKeyword(TokenType::KW_FUNCTION)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_FUNCTION));
        // ... function signature
    } else if (matchKeyword(TokenType::KW_TRIGGER)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_TRIGGER));
        std::string trigger_name = parseIdentifier();
        emitString(trigger_name);
        consumeKeyword(TokenType::KW_ON, "Expected ON");
        std::string table_name = parseQualifiedName();
        emitString(table_name);
    } else if (matchKeyword(TokenType::KW_ROLE)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_ROLE));
        std::string role_name = parseIdentifier();
        emitString(role_name);
    } else if (matchKeyword(TokenType::KW_USER)) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_DROP_USER));
        std::string user_name = parseIdentifier();
        emitString(user_name);
    }
}

// ============================================================================
// TRUNCATE Statement
// ============================================================================

void Parser::parseTruncateStmt() {
    consume(TokenType::KW_TRUNCATE, "Expected TRUNCATE");
    matchKeyword(TokenType::KW_TABLE);  // Optional

    // Table names
    std::vector<std::string> tables;
    do {
        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        resolveTableName(schema, table);
        std::string path = schema.empty() ? table : (schema + "/" + table);
        tables.push_back(std::move(path));
    } while (match(TokenType::COMMA));

    // RESTART IDENTITY / CONTINUE IDENTITY
    bool has_identity_option = false;
    if (matchKeyword(TokenType::KW_RESTART)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        has_identity_option = true;
    } else if (matchKeyword(TokenType::KW_CONTINUE)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        has_identity_option = true;
    }

    // CASCADE/RESTRICT
    bool has_cascade = false;
    if (matchKeyword(TokenType::KW_CASCADE)) {
        has_cascade = true;
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
        has_cascade = true;
    }

    if (has_identity_option || has_cascade) {
        error("TRUNCATE options are not supported in PostgreSQL emulation yet");
        return;
    }

    for (const auto& table_name : tables) {
        emit(sblr::Opcode::TRUNCATE_TABLE);
        emitString(table_name);
        emitByte(0);  // ASYNC mode
    }
}

} // namespace scratchbird::parser::postgresql
