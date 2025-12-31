/**
 * PostgreSQL Parser - DDL Statement Parsing
 *
 * Handles CREATE, ALTER, DROP, TRUNCATE and other DDL statements.
 */

#include "scratchbird/parser/postgresql/pg_parser.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/catalog_manager.h"
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

    // Handle TEMP/TEMPORARY
    bool is_temp = matchKeyword(TokenType::KW_TEMP) || matchKeyword(TokenType::KW_TEMPORARY);

    // Handle UNLOGGED
    bool is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);

    // What to create?
    if (matchKeyword(TokenType::KW_TABLE)) {
        parseCreateTable();
    } else if (matchKeyword(TokenType::KW_INDEX)) {
        parseCreateIndex();
    } else if (matchKeyword(TokenType::KW_UNIQUE)) {
        consumeKeyword(TokenType::KW_INDEX, "Expected INDEX");
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
        // Optional WITH PASSWORD
        if (matchKeyword(TokenType::KW_WITH)) {
            if (matchKeyword(TokenType::KW_PASSWORD)) {
                if (check(TokenType::STRING_LITERAL)) {
                    uint32_t id = current_token_.value.string_id;
                    std::string password(lexer_.stringPool().get(id));
                    emitString(password);
                    advance();
                }
            }
        }
    } else {
        error("Expected TABLE, INDEX, VIEW, SEQUENCE, FUNCTION, etc. after CREATE");
    }
}

// ============================================================================
// CREATE TABLE
// ============================================================================

void Parser::parseCreateTable() {
    emit(sblr::Opcode::CREATE_TABLE);

    // IF NOT EXISTS
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    emitByte(if_not_exists ? 1 : 0);

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

    consume(TokenType::LEFT_PAREN, "Expected (");

    // Parse column definitions and constraints
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);
    uint32_t count = 0;

    do {
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
                emit(sblr::Opcode::PRIMARY_KEY);
                emitString(constraint_name);

                consume(TokenType::LEFT_PAREN, "Expected (");
                emit(sblr::Opcode::BEGIN_LIST);
                size_t pk_count_pos = bytecode_.size();
                emitU32(0);
                uint32_t pk_count = 0;
                do {
                    std::string col = parseIdentifier();
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(col);
                    pk_count++;
                } while (match(TokenType::COMMA));
                sblr::writeInt32(&bytecode_[pk_count_pos], pk_count);
                emit(sblr::Opcode::END_LIST);
                consume(TokenType::RIGHT_PAREN, "Expected )");
            } else if (matchKeyword(TokenType::KW_UNIQUE)) {
                emit(sblr::Opcode::UNIQUE_CONSTRAINT);
                emitString(constraint_name);

                consume(TokenType::LEFT_PAREN, "Expected (");
                emit(sblr::Opcode::BEGIN_LIST);
                size_t uq_count_pos = bytecode_.size();
                emitU32(0);
                uint32_t uq_count = 0;
                do {
                    std::string col = parseIdentifier();
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(col);
                    uq_count++;
                } while (match(TokenType::COMMA));
                sblr::writeInt32(&bytecode_[uq_count_pos], uq_count);
                emit(sblr::Opcode::END_LIST);
                consume(TokenType::RIGHT_PAREN, "Expected )");
            } else if (matchKeyword(TokenType::KW_FOREIGN)) {
                consumeKeyword(TokenType::KW_KEY, "Expected KEY");
                ForeignKeyDef fk = parseForeignKeyDef();
                fk.name = constraint_name;

                emit(sblr::Opcode::TABLE_FK);
                emitString(fk.name);
                emit(sblr::Opcode::BEGIN_LIST);
                emitU32(static_cast<uint32_t>(fk.columns.size()));
                for (const auto& col : fk.columns) {
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(col);
                }
                emit(sblr::Opcode::END_LIST);
                emitString(fk.ref_table);
                emit(sblr::Opcode::BEGIN_LIST);
                emitU32(static_cast<uint32_t>(fk.ref_columns.size()));
                for (const auto& col : fk.ref_columns) {
                    emit(sblr::Opcode::COLUMN_REF);
                    emitString(col);
                }
                emit(sblr::Opcode::END_LIST);
            } else if (matchKeyword(TokenType::KW_CHECK)) {
                emit(sblr::Opcode::CHECK_CONSTRAINT);
                emitString(constraint_name);
                consume(TokenType::LEFT_PAREN, "Expected (");
                parseExpression();
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
        } else {
            // Column definition
            ColumnDef col = parseColumnDef();

            emit(sblr::Opcode::COLUMN_DEF);
            emitString(col.name);
            emit(typeToOpcode(col.type.kind));
            if (col.type.length > 0) {
                emitU32(col.type.length);
            }
            if (col.type.precision > 0) {
                emitU16(col.type.precision);
                emitU16(col.type.scale);
            }

            // Constraints
            if (col.not_null) {
                emit(sblr::Opcode::NOT_NULL);
            }
            if (col.primary_key) {
                emit(sblr::Opcode::PRIMARY_KEY);
                emitString("");
            }
            if (col.unique) {
                emit(sblr::Opcode::UNIQUE_CONSTRAINT);
                emitString("");
            }
            if (col.has_default) {
                emit(sblr::Opcode::DEFAULT_VALUE);
                if (col.default_is_null) {
                    emit(sblr::Opcode::LITERAL_NULL);
                } else {
                    emitString(col.default_value);
                }
            }
            if (col.is_identity) {
                emit(sblr::Opcode::IDENTITY_COLUMN);
                emitByte(col.identity_always ? 1 : 0);
            }
            if (col.is_generated) {
                emit(sblr::Opcode::GENERATED_COLUMN);
                emitString(col.generated_expr);
                emitByte(col.generated_stored ? 1 : 0);
            }
        }
        count++;
    } while (match(TokenType::COMMA));

    sblr::writeInt32(&bytecode_[count_pos], count);
    emit(sblr::Opcode::END_LIST);

    consume(TokenType::RIGHT_PAREN, "Expected )");

    // Table options
    if (matchKeyword(TokenType::KW_WITH)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        // Parse storage options
        while (!check(TokenType::RIGHT_PAREN)) {
            parseIdentifier();  // option name
            if (match(TokenType::EQUAL)) {
                parseExpression();  // option value
            }
            if (!match(TokenType::COMMA)) break;
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    // TABLESPACE
    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        parseIdentifier();  // tablespace name
    }
}

ColumnDef Parser::parseColumnDef() {
    ColumnDef col;
    col.name = parseIdentifier();
    col.type = parseDataType();

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
            } else {
                // For now, just capture the expression as a string
                col.default_is_expr = true;
                // We'd need to capture the expression text here
                parseExpression();  // Parse but don't capture
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
                // Capture expression - for now parse and discard
                parseExpression();
                consume(TokenType::RIGHT_PAREN, "Expected )");
                if (matchKeyword(TokenType::KW_STORED)) {
                    col.generated_stored = true;
                } else if (matchKeyword(TokenType::KW_VIRTUAL)) {
                    col.generated_stored = false;
                }
            }
        } else if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            parseExpression();  // Check constraint
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
    while (match(TokenType::LEFT_BRACKET)) {
        type.kind = PgDataType::Kind::ARRAY;
        if (check(TokenType::INTEGER_LITERAL)) {
            advance();  // Array dimension
        }
        consume(TokenType::RIGHT_BRACKET, "Expected ]");
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
    emit(sblr::Opcode::CREATE_INDEX);

    // CONCURRENTLY
    bool concurrent = matchKeyword(TokenType::KW_CONCURRENTLY);
    emitByte(concurrent ? 1 : 0);

    // IF NOT EXISTS
    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    emitByte(if_not_exists ? 1 : 0);

    // Index name (optional for inline definitions)
    std::string index_name;
    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        index_name = parseIdentifier();
    }
    emitString(index_name);

    consumeKeyword(TokenType::KW_ON, "Expected ON");

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

    // USING method
    uint8_t method = 0;  // Default BTREE
    if (matchKeyword(TokenType::KW_USING)) {
        std::string method_name = parseIdentifier();
        std::transform(method_name.begin(), method_name.end(), method_name.begin(), ::tolower);
        if (method_name == "btree") method = 0;
        else if (method_name == "hash") method = 1;
        else if (method_name == "gin") method = 2;
        else if (method_name == "gist") method = 3;
        else if (method_name == "spgist") method = 4;
        else if (method_name == "brin") method = 5;
    }
    emitByte(method);

    // Column list
    consume(TokenType::LEFT_PAREN, "Expected (");
    emit(sblr::Opcode::BEGIN_LIST);
    size_t count_pos = bytecode_.size();
    emitU32(0);
    uint32_t count = 0;

    do {
        // Column or expression
        if (match(TokenType::LEFT_PAREN)) {
            // Expression index
            parseExpression();
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else {
            std::string col = parseIdentifier();
            emit(sblr::Opcode::COLUMN_REF);
            emitString(col);
        }

        // Collation
        if (matchKeyword(TokenType::KW_COLLATE)) {
            parseIdentifier();
        }

        // Operator class
        if (check(TokenType::IDENTIFIER) && !check(TokenType::KW_ASC) &&
            !check(TokenType::KW_DESC) && !check(TokenType::KW_NULLS)) {
            parseIdentifier();
        }

        // ASC/DESC
        if (matchKeyword(TokenType::KW_DESC)) {
            emit(sblr::Opcode::SORT_DESC);
        } else {
            matchKeyword(TokenType::KW_ASC);
            emit(sblr::Opcode::SORT_ASC);
        }

        // NULLS FIRST/LAST
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
    consume(TokenType::RIGHT_PAREN, "Expected )");

    // INCLUDE columns
    if (matchKeyword(TokenType::KW_INCLUDE)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        emit(sblr::Opcode::BEGIN_LIST);
        size_t inc_count_pos = bytecode_.size();
        emitU32(0);
        uint32_t inc_count = 0;
        do {
            std::string col = parseIdentifier();
            emit(sblr::Opcode::COLUMN_REF);
            emitString(col);
            inc_count++;
        } while (match(TokenType::COMMA));
        sblr::writeInt32(&bytecode_[inc_count_pos], inc_count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    // WHERE clause (partial index)
    if (matchKeyword(TokenType::KW_WHERE)) {
        emit(sblr::Opcode::WHERE_CLAUSE);
        parseExpression();
    }
}

// ============================================================================
// Other CREATE statements (stubs for now)
// ============================================================================

void Parser::parseCreateView() {
    emit(sblr::Opcode::CREATE_VIEW);

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    emitByte(if_not_exists ? 1 : 0);

    std::string schema;
    std::string view_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = view_name;
        view_name = parseIdentifier();
    }
    resolveTableName(schema, view_name);
    emitString(schema + "/" + view_name);

    // Column list
    if (match(TokenType::LEFT_PAREN)) {
        emit(sblr::Opcode::BEGIN_LIST);
        size_t count_pos = bytecode_.size();
        emitU32(0);
        uint32_t count = 0;
        do {
            emitString(parseIdentifier());
            count++;
        } while (match(TokenType::COMMA));
        sblr::writeInt32(&bytecode_[count_pos], count);
        emit(sblr::Opcode::END_LIST);
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    consumeKeyword(TokenType::KW_AS, "Expected AS");
    parseSelectStmt();

    // WITH CHECK OPTION
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_CHECK)) {
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            emitByte(1);
        } else if (matchKeyword(TokenType::KW_LOCAL)) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            emitByte(2);
        } else if (matchKeyword(TokenType::KW_CASCADED)) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
            emitByte(3);
        }
    }
}

void Parser::parseCreateMaterializedView() {
    emit(sblr::Opcode::CREATE_VIEW);
    emitByte(1);  // Materialized flag

    // Similar to CREATE VIEW but materialized
    std::string schema;
    std::string view_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = view_name;
        view_name = parseIdentifier();
    }
    resolveTableName(schema, view_name);
    emitString(schema + "/" + view_name);

    consumeKeyword(TokenType::KW_AS, "Expected AS");
    parseSelectStmt();

    // WITH [NO] DATA
    if (matchKeyword(TokenType::KW_WITH)) {
        if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_DATA, "Expected DATA");
            emitByte(0);  // No data
        } else {
            consumeKeyword(TokenType::KW_DATA, "Expected DATA");
            emitByte(1);  // With data
        }
    }
}

void Parser::parseCreateSequence() {
    emit(sblr::Opcode::CREATE_SEQUENCE);

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    emitByte(if_not_exists ? 1 : 0);

    std::string schema;
    std::string seq_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = seq_name;
        seq_name = parseIdentifier();
    }
    resolveTableName(schema, seq_name);
    emitString(schema + "/" + seq_name);

    // Sequence options
    while (true) {
        if (matchKeyword(TokenType::KW_START)) {
            matchKeyword(TokenType::KW_WITH);
            parseExpression();
        } else if (matchKeyword(TokenType::KW_INCREMENT)) {
            matchKeyword(TokenType::KW_BY);
            parseExpression();
        } else if (matchKeyword(TokenType::KW_MINVALUE)) {
            parseExpression();
        } else if (matchKeyword(TokenType::KW_NO)) {
            if (matchKeyword(TokenType::KW_MINVALUE)) {
                // No minimum
            } else if (matchKeyword(TokenType::KW_MAXVALUE)) {
                // No maximum
            } else if (matchKeyword(TokenType::KW_CYCLE)) {
                // No cycle
            }
        } else if (matchKeyword(TokenType::KW_MAXVALUE)) {
            parseExpression();
        } else if (matchKeyword(TokenType::KW_CYCLE)) {
            // Cycle enabled
        } else if (matchKeyword(TokenType::KW_CACHE)) {
            parseExpression();
        } else if (matchKeyword(TokenType::KW_OWNED)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY");
            if (matchKeyword(TokenType::KW_NONE)) {
                // Not owned
            } else {
                parseQualifiedName();  // table.column
            }
        } else {
            break;
        }
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

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DATABASE));
    emitByte(if_not_exists ? 0x01 : 0x00);
    std::string db_path = buildEmulatedServerRoot(default_schema_);
    if (!db_path.empty()) {
        db_path.push_back('.');
    }
    db_path += db_name;
    emitString(db_path);

    // Database options
    if (matchKeyword(TokenType::KW_WITH)) {
        while (true) {
            if (matchKeyword(TokenType::KW_OWNER)) {
                match(TokenType::EQUAL);
                parseIdentifier();
            } else if (matchKeyword(TokenType::KW_TEMPLATE)) {
                match(TokenType::EQUAL);
                parseIdentifier();
            } else if (matchKeyword(TokenType::KW_ENCODING)) {
                match(TokenType::EQUAL);
                if (check(TokenType::STRING_LITERAL)) {
                    advance();
                } else {
                    parseIdentifier();
                }
            } else if (matchKeyword(TokenType::KW_TABLESPACE)) {
                match(TokenType::EQUAL);
                parseIdentifier();
            } else {
                break;
            }
        }
    }
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

    // Flags: bit0 = OR REPLACE, bit2 = SQL SECURITY DEFINER (not parsed yet)
    uint8_t flags = 0;
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
}

void Parser::parseCreateProcedure() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_PROCEDURE_STMT));

    std::string proc_name = parseIdentifier();

    uint8_t flags = 0;
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
}

void Parser::parseCreateTrigger() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_TRIGGER));

    std::string trigger_name = parseIdentifier();
    emitString(trigger_name);

    // BEFORE/AFTER/INSTEAD OF
    if (matchKeyword(TokenType::KW_BEFORE)) {
        emitByte(1);
    } else if (matchKeyword(TokenType::KW_AFTER)) {
        emitByte(2);
    } else if (matchKeyword(TokenType::KW_INSTEAD)) {
        consumeKeyword(TokenType::KW_OF, "Expected OF");
        emitByte(3);
    }

    // Event (INSERT/UPDATE/DELETE/TRUNCATE)
    do {
        if (matchKeyword(TokenType::KW_INSERT)) {
            emitByte(1);
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            emitByte(2);
            // OF columns
            if (matchKeyword(TokenType::KW_OF)) {
                do {
                    parseIdentifier();
                } while (match(TokenType::COMMA));
            }
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            emitByte(3);
        } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
            emitByte(4);
        }
    } while (matchKeyword(TokenType::KW_OR));

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
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_TYPE));

    std::string type_name = parseQualifiedName();
    emitString(type_name);

    consumeKeyword(TokenType::KW_AS, "Expected AS");

    if (matchKeyword(TokenType::KW_ENUM)) {
        // ENUM type
        emitByte(1);
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            if (check(TokenType::STRING_LITERAL)) {
                uint32_t id = current_token_.value.string_id;
                emitString(lexer_.stringPool().get(id));
                advance();
            }
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else if (matchKeyword(TokenType::KW_RANGE)) {
        // RANGE type
        emitByte(2);
        consume(TokenType::LEFT_PAREN, "Expected (");
        // Parse range options
        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else if (match(TokenType::LEFT_PAREN)) {
        // Composite type
        emitByte(3);
        do {
            std::string attr_name = parseIdentifier();
            emitString(attr_name);
            parseDataType();
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }
}

void Parser::parseCreateDomain() {
    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));

    std::string domain_name = parseQualifiedName();
    emitString(domain_name);

    consumeKeyword(TokenType::KW_AS, "Expected AS");
    parseDataType();

    // Constraints
    while (true) {
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            parseExpression();
        } else if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_NULL, "Expected NULL");
        } else if (matchKeyword(TokenType::KW_NULL)) {
            // Allow NULL
        } else if (matchKeyword(TokenType::KW_CHECK)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            parseExpression();
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            parseIdentifier();  // constraint name
        } else {
            break;
        }
    }
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
            db_path.push_back('.');
        }
        return db_path + db_name;
    };

    auto emit_object_path = [&](const std::vector<std::string>& components) {
        emitByte(static_cast<uint8_t>(core::PathType::ABSOLUTE));
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

        // Fallback to legacy ALTER TABLE encoding
        emit(sblr::Opcode::ALTER_TABLE);
        resolveTableName(schema, table);
        emit(sblr::Opcode::TABLE_REF);
        emitString(schema + "/" + table);

        do {
            if (matchKeyword(TokenType::KW_ADD)) {
                if (matchKeyword(TokenType::KW_COLUMN)) {
                    ColumnDef col = parseColumnDef();
                    emit(sblr::Opcode::COLUMN_DEF);
                    emitString(col.name);
                    emit(typeToOpcode(col.type.kind));
                } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                    parseIdentifier();  // constraint name
                } else {
                    ColumnDef col = parseColumnDef();
                    emit(sblr::Opcode::COLUMN_DEF);
                    emitString(col.name);
                    emit(typeToOpcode(col.type.kind));
                }
            } else if (matchKeyword(TokenType::KW_DROP)) {
                if (matchKeyword(TokenType::KW_COLUMN)) {
                    matchKeyword(TokenType::KW_IF) && matchKeyword(TokenType::KW_EXISTS);
                    std::string col_name = parseIdentifier();
                    emitString(col_name);
                    matchKeyword(TokenType::KW_CASCADE) || matchKeyword(TokenType::KW_RESTRICT);
                } else if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                    matchKeyword(TokenType::KW_IF) && matchKeyword(TokenType::KW_EXISTS);
                    std::string constraint_name = parseIdentifier();
                    emitString(constraint_name);
                }
            } else if (matchKeyword(TokenType::KW_ALTER)) {
                consumeKeyword(TokenType::KW_COLUMN, "Expected COLUMN");
                std::string col_name = parseIdentifier();
                emitString(col_name);
                if (matchKeyword(TokenType::KW_SET)) {
                    if (matchKeyword(TokenType::KW_DEFAULT)) {
                        parseExpression();
                    } else if (matchKeyword(TokenType::KW_NOT)) {
                        consumeKeyword(TokenType::KW_NULL, "Expected NULL");
                    }
                } else if (matchKeyword(TokenType::KW_DROP)) {
                    if (matchKeyword(TokenType::KW_DEFAULT)) {
                        // Drop default
                    } else if (matchKeyword(TokenType::KW_NOT)) {
                        consumeKeyword(TokenType::KW_NULL, "Expected NULL");
                    }
                } else if (matchKeyword(TokenType::KW_TYPE)) {
                    parseDataType();
                }
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
        parse_rename_move(core::CatalogManager::ObjectType::DOMAIN);
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

void Parser::parseDropStmt() {
    consume(TokenType::KW_DROP, "Expected DROP");

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
            db_path.push_back('.');
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

        for (const auto& schema_name : schemas) {
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
        emit(sblr::Opcode::DROP_TABLE);

        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        emitByte(if_exists ? 1 : 0);

        // Table names
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
        } while (match(TokenType::COMMA));

        // CASCADE/RESTRICT
        if (matchKeyword(TokenType::KW_CASCADE)) {
            emitByte(1);
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            emitByte(2);
        }
    } else if (matchKeyword(TokenType::KW_INDEX)) {
        emit(sblr::Opcode::DROP_INDEX);

        bool concurrent = matchKeyword(TokenType::KW_CONCURRENTLY);
        emitByte(concurrent ? 1 : 0);

        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        emitByte(if_exists ? 1 : 0);

        std::string index_name = parseQualifiedName();
        emitString(index_name);
    } else if (matchKeyword(TokenType::KW_VIEW)) {
        emit(sblr::Opcode::DROP_VIEW);

        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        emitByte(if_exists ? 1 : 0);

        std::string view_name = parseQualifiedName();
        emitString(view_name);
    } else if (matchKeyword(TokenType::KW_SEQUENCE)) {
        emit(sblr::Opcode::DROP_SEQUENCE);

        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        emitByte(if_exists ? 1 : 0);

        std::string seq_name = parseQualifiedName();
        emitString(seq_name);
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

    emit(sblr::Opcode::TRUNCATE_TABLE);

    // Table names
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
    } while (match(TokenType::COMMA));

    // RESTART IDENTITY / CONTINUE IDENTITY
    if (matchKeyword(TokenType::KW_RESTART)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        emitByte(1);
    } else if (matchKeyword(TokenType::KW_CONTINUE)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        emitByte(2);
    }

    // CASCADE/RESTRICT
    if (matchKeyword(TokenType::KW_CASCADE)) {
        emitByte(1);
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
        emitByte(2);
    }
}

} // namespace scratchbird::parser::postgresql
