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
parser::v3::SchemaPath buildPathFromQualified(parser::v3::StringPool& pool,
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
    if (!cur.empty()) {
        comps.push_back(pool.intern(cur));
    }
    parser::v3::PathType path_type = comps.size() > 1 ? parser::v3::PathType::ABSOLUTE
                                                       : parser::v3::PathType::UNQUALIFIED;
    return parser::v3::SchemaPath(path_type, std::move(comps));
}

std::vector<std::string> splitPath(std::string_view path) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : path) {
        if (ch == '.' || ch == '/') {
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

parser::v3::SchemaPath buildPathFromDefault(parser::v3::StringPool& pool,
                                            const std::string& base,
                                            std::string_view leaf) {
    std::vector<parser::v3::StringPool::StringId> comps;
    std::vector<std::string> parts = splitPath(base);
    for (const auto& part : parts) {
        comps.push_back(pool.intern(part));
    }
    if (!leaf.empty()) {
        comps.push_back(pool.intern(std::string(leaf)));
    }
    parser::v3::PathType type = comps.size() > 1 ? parser::v3::PathType::ABSOLUTE
                                                 : parser::v3::PathType::UNQUALIFIED;
    return parser::v3::SchemaPath(type, std::move(comps));
}

parser::v3::TypeName pgTypeToTypeName(const PgDataType& type,
                                      parser::v3::StringPool& pool) {
    parser::v3::TypeName out;
    out.length = type.length > 0 ? std::optional<int32_t>(type.length) : std::nullopt;
    out.precision = type.precision > 0 ? std::optional<int32_t>(type.precision) : std::nullopt;
    out.scale = type.scale > 0 ? std::optional<int32_t>(type.scale) : std::nullopt;
    out.with_time_zone = type.with_time_zone;

    switch (type.kind) {
        case PgDataType::Kind::SMALLINT:
            out.name = pool.intern("SMALLINT");
            break;
        case PgDataType::Kind::INTEGER:
            out.name = pool.intern("INTEGER");
            break;
        case PgDataType::Kind::BIGINT:
            out.name = pool.intern("BIGINT");
            break;
        case PgDataType::Kind::INT128:
            out.name = pool.intern("INT128");
            break;
        case PgDataType::Kind::UINT128:
            out.name = pool.intern("UINT128");
            break;
        case PgDataType::Kind::REAL:
            out.name = pool.intern("REAL");
            break;
        case PgDataType::Kind::DOUBLE_PRECISION:
            out.name = pool.intern("DOUBLE PRECISION");
            break;
        case PgDataType::Kind::DECIMAL:
        case PgDataType::Kind::NUMERIC:
            out.name = pool.intern("NUMERIC");
            break;
        case PgDataType::Kind::MONEY:
            out.name = pool.intern("MONEY");
            break;
        case PgDataType::Kind::CHAR:
            out.name = pool.intern("CHAR");
            break;
        case PgDataType::Kind::VARCHAR:
            out.name = pool.intern("VARCHAR");
            break;
        case PgDataType::Kind::TEXT:
            out.name = pool.intern("TEXT");
            break;
        case PgDataType::Kind::BYTEA:
            out.name = pool.intern("BYTEA");
            break;
        case PgDataType::Kind::DATE:
            out.name = pool.intern("DATE");
            break;
        case PgDataType::Kind::TIME:
            out.name = pool.intern("TIME");
            break;
        case PgDataType::Kind::TIMETZ:
            out.name = pool.intern("TIME");
            out.with_time_zone = true;
            break;
        case PgDataType::Kind::TIMESTAMP:
            out.name = pool.intern("TIMESTAMP");
            break;
        case PgDataType::Kind::TIMESTAMPTZ:
            out.name = pool.intern("TIMESTAMP");
            out.with_time_zone = true;
            break;
        case PgDataType::Kind::INTERVAL:
            out.name = pool.intern("INTERVAL");
            break;
        case PgDataType::Kind::BOOLEAN:
            out.name = pool.intern("BOOLEAN");
            break;
        case PgDataType::Kind::UUID:
            out.name = pool.intern("UUID");
            break;
        case PgDataType::Kind::JSON:
            out.name = pool.intern("JSON");
            break;
        case PgDataType::Kind::JSONB:
            out.name = pool.intern("JSONB");
            break;
        case PgDataType::Kind::XML:
            out.name = pool.intern("XML");
            break;
        case PgDataType::Kind::POINT:
            out.name = pool.intern("POINT");
            break;
        case PgDataType::Kind::LINE:
            out.name = pool.intern("LINE");
            break;
        case PgDataType::Kind::LSEG:
            out.name = pool.intern("LSEG");
            break;
        case PgDataType::Kind::BOX:
            out.name = pool.intern("BOX");
            break;
        case PgDataType::Kind::PATH:
            out.name = pool.intern("PATH");
            break;
        case PgDataType::Kind::POLYGON:
            out.name = pool.intern("POLYGON");
            break;
        case PgDataType::Kind::CIRCLE:
            out.name = pool.intern("CIRCLE");
            break;
        case PgDataType::Kind::CIDR:
            out.name = pool.intern("CIDR");
            break;
        case PgDataType::Kind::INET:
            out.name = pool.intern("INET");
            break;
        case PgDataType::Kind::MACADDR:
            out.name = pool.intern("MACADDR");
            break;
        case PgDataType::Kind::MACADDR8:
            out.name = pool.intern("MACADDR8");
            break;
        case PgDataType::Kind::BIT:
            out.name = pool.intern("BIT");
            break;
        case PgDataType::Kind::VARBIT:
            out.name = pool.intern("VARBIT");
            break;
        case PgDataType::Kind::TSVECTOR:
            out.name = pool.intern("TSVECTOR");
            break;
        case PgDataType::Kind::TSQUERY:
            out.name = pool.intern("TSQUERY");
            break;
        case PgDataType::Kind::SMALLSERIAL:
            out.name = pool.intern("SMALLSERIAL");
            break;
        case PgDataType::Kind::SERIAL:
            out.name = pool.intern("SERIAL");
            break;
        case PgDataType::Kind::BIGSERIAL:
            out.name = pool.intern("BIGSERIAL");
            break;
        case PgDataType::Kind::DOMAIN:
            out.name = pool.intern(type.type_name);
            out.has_schema_path = true;
            out.schema_path = buildPathFromQualified(pool, type.type_name);
            break;
        case PgDataType::Kind::ARRAY: {
            PgDataType elem;
            elem.kind = type.element_kind;
            elem.type_name = type.element_type;
            elem.length = type.length;
            elem.precision = type.precision;
            elem.scale = type.scale;
            elem.with_time_zone = type.with_time_zone;
            out = pgTypeToTypeName(elem, pool);
            out.is_array = true;
            if (type.array_size > 0) {
                out.array_size = static_cast<int32_t>(type.array_size);
            }
            break;
        }
        default:
            out.name = pool.intern("VARCHAR");
            break;
    }
    return out;
}

uint8_t encodeDataType(const PgDataType& dt) {
    using core::DataType;
    switch (dt.kind) {
        case PgDataType::Kind::SMALLINT:
        case PgDataType::Kind::INTEGER:
        case PgDataType::Kind::SMALLSERIAL:
            return static_cast<uint8_t>(DataType::INT16);
        case PgDataType::Kind::SERIAL:
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
// CREATE Statement Dispatch (v3 AST)
// ============================================================================

parser::v3::Statement* Parser::parseCreateStmtV3() {
    bool or_replace = false;
    if (matchKeyword(TokenType::KW_OR)) {
        consumeKeyword(TokenType::KW_REPLACE, "Expected REPLACE");
        or_replace = true;
    }

    bool is_temp = matchKeyword(TokenType::KW_TEMP) || matchKeyword(TokenType::KW_TEMPORARY);
    bool is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);
    (void)is_temp;
    (void)is_unlogged;

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

    auto prepend_payload_flag = [](std::string& payload, std::string_view flag) {
        if (!payload.empty()) {
            payload.insert(0, ";");
        }
        payload.insert(0, flag);
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

    if (matchKeyword(TokenType::KW_TABLE)) {
        return parseCreateTableV3(or_replace, is_temp, is_unlogged);
    }

    if (matchKeyword(TokenType::KW_INDEX)) {
        return parseCreateIndexV3(false);
    }

    if (matchKeyword(TokenType::KW_UNIQUE)) {
        consumeKeyword(TokenType::KW_INDEX, "Expected INDEX");
        return parseCreateIndexV3(true);
    }

    if (matchKeyword(TokenType::KW_VIEW)) {
        return parseCreateViewV3(false, or_replace, is_temp);
    }

    if (matchKeyword(TokenType::KW_MATERIALIZED)) {
        consumeKeyword(TokenType::KW_VIEW, "Expected VIEW");
        return parseCreateViewV3(true, or_replace, is_temp);
    }

    if (matchKeyword(TokenType::KW_SEQUENCE)) {
        return parseCreateSequenceV3(or_replace, is_temp);
    }

    if (matchKeyword(TokenType::KW_FUNCTION)) {
        return parseCreateFunctionV3(or_replace);
    }

    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        return parseCreateProcedureV3(or_replace);
    }

    if (matchKeyword(TokenType::KW_TRIGGER)) {
        return parseCreateTriggerV3(or_replace);
    }

    if (matchKeyword(TokenType::KW_TYPE)) {
        return parseCreateTypeV3();
    }

    if (matchKeyword(TokenType::KW_DOMAIN)) {
        return parseCreateDomainV3();
    }

    if (matchIdentifierKeyword("OPERATOR")) {
        if (matchIdentifierKeyword("CLASS")) {
            std::string payload = capture_remaining_clause();
            return make_alter_system_stmt("postgresql.operator_class.create", payload);
        }
    }

    if (matchIdentifierKeyword("EXTENSION")) {
        bool if_not_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_NOT, "Expected NOT after IF");
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
            if_not_exists = true;
        }

        std::string extension_name = parseIdentifier();
        std::string payload = capture_remaining_clause();
        if (if_not_exists) {
            prepend_payload_flag(payload, "IF_NOT_EXISTS=1");
        }
        return make_alter_system_stmt("platform.extension.create." + extension_name, payload);
    }

    if (matchIdentifierKeyword("PUBLICATION")) {
        bool if_not_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_NOT, "Expected NOT after IF");
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
            if_not_exists = true;
        }

        std::string publication_name = parseIdentifier();
        std::string payload = capture_remaining_clause();
        if (payload.empty()) {
            error("CREATE PUBLICATION requires FOR clause");
        }
        if (if_not_exists) {
            prepend_payload_flag(payload, "IF_NOT_EXISTS=1");
        }
        return make_alter_system_stmt("replication.publication.create." + publication_name, payload);
    }

    if (matchIdentifierKeyword("SUBSCRIPTION")) {
        bool if_not_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_NOT, "Expected NOT after IF");
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
            if_not_exists = true;
        }

        std::string subscription_name = parseIdentifier();
        std::string payload = capture_remaining_clause();
        if (payload.empty()) {
            error("CREATE SUBSCRIPTION requires CONNECTION/PUBLICATION clauses");
        }
        if (if_not_exists) {
            prepend_payload_flag(payload, "IF_NOT_EXISTS=1");
        }
        return make_alter_system_stmt("replication.subscription.create." + subscription_name, payload);
    }

    if (matchKeyword(TokenType::KW_ROLE)) {
        auto* stmt = arena()->create<parser::v3::CreateRoleStmt>();
        stmt->role_name = parseIdentifierId();
        return stmt;
    }

    if (matchKeyword(TokenType::KW_USER)) {
        auto* stmt = arena()->create<parser::v3::CreateUserStmt>();
        stmt->user_name = parseIdentifierId();

        bool has_password = false;
        bool is_superuser = false;
        parser::v3::StringPool::StringId password = parser::v3::StringPool::INVALID_ID;

        if (matchKeyword(TokenType::KW_WITH)) {
            // Optional WITH keyword before options
        }
        while (true) {
            if (matchKeyword(TokenType::KW_PASSWORD)) {
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for PASSWORD");
                    break;
                }
                password = internFromLexer(current_token_.value.string_id);
                advance();
                has_password = true;
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

        stmt->has_password = has_password;
        stmt->password = password;
        stmt->is_superuser = is_superuser;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SCHEMA)) {
        auto* stmt = arena()->create<parser::v3::CreateSchemaStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_NOT, "Expected NOT");
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_not_exists = true;
        }

        std::string schema_name;
        std::string owner_name;

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
            return nullptr;
        }

        stmt->schema_path = buildPathFromDefault(string_pool_, default_schema_, schema_name);
        if (!owner_name.empty()) {
            stmt->has_owner = true;
            stmt->owner = string_pool_.intern(owner_name);
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_DATABASE)) {
        auto* stmt = arena()->create<parser::v3::CreateDatabaseStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_NOT, "Expected NOT");
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_not_exists = true;
        }

        std::string db_name = parseIdentifier();
        std::string db_root = buildEmulatedServerRoot(default_schema_);
        if (!db_root.empty()) {
            db_root += ".databases";
        }
        std::string db_path = db_root.empty() ? db_name : db_root + "." + db_name;
        stmt->database_path = buildPathFromQualified(string_pool_, db_path);
        stmt->source_spec = string_pool_.intern(db_name);

        if (matchKeyword(TokenType::KW_WITH)) {
            while (true) {
                if (matchKeyword(TokenType::KW_OWNER)) {
                    match(TokenType::EQUAL);
                    std::string owner = parseIdentifier();
                    stmt->options.push_back({string_pool_.intern("owner"), string_pool_.intern(owner)});
                } else if (matchKeyword(TokenType::KW_TEMPLATE)) {
                    match(TokenType::EQUAL);
                    std::string tmpl = parseIdentifier();
                    stmt->options.push_back({string_pool_.intern("template"), string_pool_.intern(tmpl)});
                } else if (matchKeyword(TokenType::KW_ENCODING)) {
                    match(TokenType::EQUAL);
                    if (check(TokenType::STRING_LITERAL)) {
                        std::string value = std::string(stringPool().get(current_token_.value.string_id));
                        advance();
                        stmt->options.push_back({string_pool_.intern("encoding"), string_pool_.intern(value)});
                    } else {
                        std::string value = parseIdentifier();
                        stmt->options.push_back({string_pool_.intern("encoding"), string_pool_.intern(value)});
                    }
                } else if (matchKeyword(TokenType::KW_TABLESPACE)) {
                    match(TokenType::EQUAL);
                    std::string value = parseIdentifier();
                    stmt->options.push_back({string_pool_.intern("tablespace"), string_pool_.intern(value)});
                } else {
                    break;
                }
            }
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        auto* stmt = arena()->create<parser::v3::CreateTablespaceStmt>();
        std::string tablespace_name = parseIdentifier();
        stmt->tablespace_name = string_pool_.intern(tablespace_name);

        if (matchKeyword(TokenType::KW_OWNER)) {
            parseIdentifier();  // Owner ignored for emulation
        }

        consumeKeyword(TokenType::KW_LOCATION, "Expected LOCATION for CREATE TABLESPACE");
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal for tablespace LOCATION");
            synchronize();
            return nullptr;
        }
        stmt->location = std::string(lexer_.stringPool().get(current_token_.value.string_id));
        advance();

        if (matchKeyword(TokenType::KW_WITH)) {
            if (match(TokenType::LEFT_PAREN)) {
                while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                if (check(TokenType::RIGHT_PAREN)) {
                    advance();
                }
            }
        }

        return stmt;
    }

    if (matchKeyword(TokenType::KW_POLICY)) {
        auto* stmt = arena()->create<parser::v3::CreatePolicyStmt>();
        stmt->policy_name = parseIdentifierId();

        consumeKeyword(TokenType::KW_ON, "Expected ON");
        stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());

        if (matchKeyword(TokenType::KW_AS)) {
            if (matchIdentifierKeyword("PERMISSIVE")) {
                stmt->is_permissive = true;
            } else if (matchIdentifierKeyword("RESTRICTIVE")) {
                stmt->is_permissive = false;
            }
        }

        if (matchKeyword(TokenType::KW_FOR)) {
            if (matchKeyword(TokenType::KW_ALL)) {
                stmt->policy_type = parser::v3::PolicyType::ALL;
            } else if (matchKeyword(TokenType::KW_SELECT)) {
                stmt->policy_type = parser::v3::PolicyType::SELECT;
            } else if (matchKeyword(TokenType::KW_INSERT)) {
                stmt->policy_type = parser::v3::PolicyType::INSERT;
            } else if (matchKeyword(TokenType::KW_UPDATE)) {
                stmt->policy_type = parser::v3::PolicyType::UPDATE;
            } else if (matchKeyword(TokenType::KW_DELETE)) {
                stmt->policy_type = parser::v3::PolicyType::DELETE;
            } else {
                error("Expected ALL, SELECT, INSERT, UPDATE, or DELETE after FOR");
            }
        }

        if (matchKeyword(TokenType::KW_TO)) {
            do {
                if (matchKeyword(TokenType::KW_PUBLIC)) {
                    stmt->roles.push_back(string_pool_.intern("PUBLIC"));
                } else if (matchKeyword(TokenType::KW_CURRENT_USER)) {
                    stmt->roles.push_back(string_pool_.intern("CURRENT_USER"));
                } else if (matchKeyword(TokenType::KW_CURRENT_ROLE)) {
                    stmt->roles.push_back(string_pool_.intern("CURRENT_ROLE"));
                } else if (matchKeyword(TokenType::KW_SESSION_USER)) {
                    stmt->roles.push_back(string_pool_.intern("SESSION_USER"));
                } else {
                    stmt->roles.push_back(parseIdentifierId());
                }
            } while (match(TokenType::COMMA));
        }

        if (matchKeyword(TokenType::KW_USING)) {
            if (match(TokenType::LEFT_PAREN)) {
                stmt->using_expr = parseExpression();
                consume(TokenType::RIGHT_PAREN, "Expected ) after USING");
            } else {
                stmt->using_expr = parseExpression();
            }
        }

        if (matchKeyword(TokenType::KW_WITH)) {
            consumeKeyword(TokenType::KW_CHECK, "Expected CHECK");
            if (match(TokenType::LEFT_PAREN)) {
                stmt->with_check_expr = parseExpression();
                consume(TokenType::RIGHT_PAREN, "Expected ) after WITH CHECK");
            } else {
                stmt->with_check_expr = parseExpression();
            }
        }

        return stmt;
    }

    pending_or_replace_ = or_replace;
    pending_create_temp_ = is_temp;
    pending_create_unlogged_ = is_unlogged;
    return nullptr;
}

parser::v3::TypeName Parser::parseTypeNameV3() {
    PgDataType type = parseDataType();
    return pgTypeToTypeName(type, string_pool_);
}

parser::v3::ColumnDef* Parser::parseColumnDefV3() {
    auto* col = arena()->create<parser::v3::ColumnDef>();
    col->name = parseIdentifierId();
    col->type = parseTypeNameV3();

    parser::v3::StringPool::StringId pending_name = parser::v3::StringPool::INVALID_ID;

    auto apply_name = [&](parser::v3::ColumnConstraint& constraint) {
        if (pending_name != parser::v3::StringPool::INVALID_ID) {
            constraint.name = pending_name;
            pending_name = parser::v3::StringPool::INVALID_ID;
        }
    };

    while (true) {
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            pending_name = parseIdentifierId();
            continue;
        }
        if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_NULL, "Expected NULL");
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::NOT_NULL;
            apply_name(constraint);
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_NULL)) {
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::NULL_ALLOWED;
            apply_name(constraint);
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_PRIMARY)) {
            consumeKeyword(TokenType::KW_KEY, "Expected KEY");
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::PRIMARY_KEY;
            apply_name(constraint);
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_UNIQUE)) {
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::UNIQUE;
            apply_name(constraint);
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::DEFAULT;
            apply_name(constraint);
            constraint.default_expr = parseExpression();
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_GENERATED)) {
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::GENERATED;
            apply_name(constraint);
            if (matchKeyword(TokenType::KW_ALWAYS)) {
                constraint.generated_always = true;
            } else if (matchKeyword(TokenType::KW_BY)) {
                consumeKeyword(TokenType::KW_DEFAULT, "Expected DEFAULT");
                constraint.generated_always = false;
            }
            consumeKeyword(TokenType::KW_AS, "Expected AS");
            if (matchKeyword(TokenType::KW_IDENTITY)) {
                constraint.generated_as_identity = true;
            } else {
                consume(TokenType::LEFT_PAREN, "Expected (");
                constraint.generated_expr = parseExpression();
                consume(TokenType::RIGHT_PAREN, "Expected )");
                constraint.generated_as_identity = false;
                // PostgreSQL generated expressions require STORED today; consume
                // optional storage keyword to keep token stream aligned.
                if (matchKeyword(TokenType::KW_STORED)) {
                    // consumed
                } else if (matchIdentifierKeyword("VIRTUAL")) {
                    // consumed
                }
            }
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_CHECK)) {
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::CHECK;
            apply_name(constraint);
            consume(TokenType::LEFT_PAREN, "Expected (");
            constraint.check_expr = parseExpression();
            consume(TokenType::RIGHT_PAREN, "Expected )");
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_REFERENCES)) {
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::REFERENCES;
            apply_name(constraint);
            std::string ref = parseQualifiedName();
            constraint.ref_table = buildPathFromQualified(string_pool_, ref);
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    constraint.ref_columns.push_back(parseIdentifierId());
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
            while (matchKeyword(TokenType::KW_ON)) {
                if (matchKeyword(TokenType::KW_DELETE)) {
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        constraint.on_delete = parser::v3::ForeignKeyAction::CASCADE;
                    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                        constraint.on_delete = parser::v3::ForeignKeyAction::RESTRICT;
                    } else if (matchKeyword(TokenType::KW_SET)) {
                        if (matchKeyword(TokenType::KW_NULL)) {
                            constraint.on_delete = parser::v3::ForeignKeyAction::SET_NULL;
                        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                            constraint.on_delete = parser::v3::ForeignKeyAction::SET_DEFAULT;
                        }
                    } else if (matchKeyword(TokenType::KW_NO)) {
                        consumeKeyword(TokenType::KW_ACTION, "Expected ACTION");
                        constraint.on_delete = parser::v3::ForeignKeyAction::NO_ACTION;
                    }
                } else if (matchKeyword(TokenType::KW_UPDATE)) {
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        constraint.on_update = parser::v3::ForeignKeyAction::CASCADE;
                    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                        constraint.on_update = parser::v3::ForeignKeyAction::RESTRICT;
                    } else if (matchKeyword(TokenType::KW_SET)) {
                        if (matchKeyword(TokenType::KW_NULL)) {
                            constraint.on_update = parser::v3::ForeignKeyAction::SET_NULL;
                        } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                            constraint.on_update = parser::v3::ForeignKeyAction::SET_DEFAULT;
                        }
                    } else if (matchKeyword(TokenType::KW_NO)) {
                        consumeKeyword(TokenType::KW_ACTION, "Expected ACTION");
                        constraint.on_update = parser::v3::ForeignKeyAction::NO_ACTION;
                    }
                }
            }
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_COLLATE)) {
            parser::v3::ColumnConstraint constraint;
            constraint.type = parser::v3::ConstraintType::COLLATE;
            apply_name(constraint);
            constraint.collation = parseIdentifierId();
            col->constraints.push_back(std::move(constraint));
            continue;
        }
        break;
    }

    return col;
}

parser::v3::TableConstraint* Parser::parseTableConstraintV3() {
    auto* constraint = arena()->create<parser::v3::TableConstraint>();
    if (matchKeyword(TokenType::KW_CONSTRAINT)) {
        constraint->name = parseIdentifierId();
    }

    if (matchKeyword(TokenType::KW_PRIMARY)) {
        consumeKeyword(TokenType::KW_KEY, "Expected KEY");
        constraint->type = parser::v3::TableConstraintType::PRIMARY_KEY;
    } else if (matchKeyword(TokenType::KW_UNIQUE)) {
        constraint->type = parser::v3::TableConstraintType::UNIQUE;
    } else if (matchKeyword(TokenType::KW_FOREIGN)) {
        consumeKeyword(TokenType::KW_KEY, "Expected KEY");
        constraint->type = parser::v3::TableConstraintType::FOREIGN_KEY;
    } else if (matchKeyword(TokenType::KW_CHECK)) {
        constraint->type = parser::v3::TableConstraintType::CHECK;
    } else if (matchKeyword(TokenType::KW_EXCLUDE)) {
        constraint->type = parser::v3::TableConstraintType::EXCLUDE;
    } else {
        error("Expected table constraint type");
        return constraint;
    }

    if (constraint->type == parser::v3::TableConstraintType::CHECK) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        constraint->check_expr = parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return constraint;
    }

    if (constraint->type == parser::v3::TableConstraintType::EXCLUDE) {
        if (matchKeyword(TokenType::KW_USING)) {
            constraint->using_index = true;
            constraint->index_method = parseIdentifierId();
        }
    }

    consume(TokenType::LEFT_PAREN, "Expected (");
    do {
        constraint->columns.push_back(parseIdentifierId());
        if (constraint->type == parser::v3::TableConstraintType::EXCLUDE) {
            if (matchKeyword(TokenType::KW_WITH)) {
                match(TokenType::LEFT_PAREN);
                while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
                    advance();
                }
                if (check(TokenType::RIGHT_PAREN)) {
                    advance();
                }
            }
        }
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected )");

    if (constraint->type == parser::v3::TableConstraintType::FOREIGN_KEY) {
        consumeKeyword(TokenType::KW_REFERENCES, "Expected REFERENCES");
        std::string ref = parseQualifiedName();
        constraint->ref_table = buildPathFromQualified(string_pool_, ref);
        if (match(TokenType::LEFT_PAREN)) {
            do {
                constraint->ref_columns.push_back(parseIdentifierId());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        while (matchKeyword(TokenType::KW_ON)) {
            if (matchKeyword(TokenType::KW_DELETE)) {
                if (matchKeyword(TokenType::KW_CASCADE)) {
                    constraint->on_delete = parser::v3::ForeignKeyAction::CASCADE;
                } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                    constraint->on_delete = parser::v3::ForeignKeyAction::RESTRICT;
                } else if (matchKeyword(TokenType::KW_SET)) {
                    if (matchKeyword(TokenType::KW_NULL)) {
                        constraint->on_delete = parser::v3::ForeignKeyAction::SET_NULL;
                    } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                        constraint->on_delete = parser::v3::ForeignKeyAction::SET_DEFAULT;
                    }
                } else if (matchKeyword(TokenType::KW_NO)) {
                    consumeKeyword(TokenType::KW_ACTION, "Expected ACTION");
                    constraint->on_delete = parser::v3::ForeignKeyAction::NO_ACTION;
                }
            } else if (matchKeyword(TokenType::KW_UPDATE)) {
                if (matchKeyword(TokenType::KW_CASCADE)) {
                    constraint->on_update = parser::v3::ForeignKeyAction::CASCADE;
                } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                    constraint->on_update = parser::v3::ForeignKeyAction::RESTRICT;
                } else if (matchKeyword(TokenType::KW_SET)) {
                    if (matchKeyword(TokenType::KW_NULL)) {
                        constraint->on_update = parser::v3::ForeignKeyAction::SET_NULL;
                    } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                        constraint->on_update = parser::v3::ForeignKeyAction::SET_DEFAULT;
                    }
                } else if (matchKeyword(TokenType::KW_NO)) {
                    consumeKeyword(TokenType::KW_ACTION, "Expected ACTION");
                    constraint->on_update = parser::v3::ForeignKeyAction::NO_ACTION;
                }
            }
        }
    }

    return constraint;
}

parser::v3::CreateTableStmt* Parser::parseCreateTableV3(bool or_replace,
                                                        bool is_temp,
                                                        bool is_unlogged) {
    auto* stmt = arena()->create<parser::v3::CreateTableStmt>();
    stmt->or_replace = or_replace;
    stmt->unlogged = is_unlogged;

    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        stmt->if_not_exists = true;
    }

    std::string schema;
    std::string table = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = table;
        table = parseIdentifier();
    }
    resolveTableName(schema, table);
    std::string full = schema.empty() ? table : schema + "." + table;
    stmt->table_path = buildPathFromQualified(string_pool_, full);

    if (is_temp) {
        stmt->temp_type = parser::v3::TempTableType::SESSION;
    }

    auto parse_with_storage_options = [&]() {
        consume(TokenType::LEFT_PAREN, "Expected (");
        while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
            parseIdentifier();
            if (match(TokenType::EQUAL)) {
                parseExpression();
            }
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
    };

    bool has_table_definition = false;
    if (match(TokenType::LEFT_PAREN)) {
        has_table_definition = true;
        // PostgreSQL permits zero-column tables (e.g., CREATE TEMP TABLE t();).
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                if (check(TokenType::KW_PRIMARY) || check(TokenType::KW_UNIQUE) ||
                    check(TokenType::KW_FOREIGN) || check(TokenType::KW_CHECK) ||
                    check(TokenType::KW_CONSTRAINT) || check(TokenType::KW_EXCLUDE)) {
                    stmt->constraints.push_back(parseTableConstraintV3());
                } else {
                    stmt->columns.push_back(parseColumnDefV3());
                }
            } while (match(TokenType::COMMA));
        }
        consume(TokenType::RIGHT_PAREN, "Expected )");
    } else if (matchKeyword(TokenType::KW_AS)) {
        stmt->as_query = parseSelectStmt();
    } else {
        error("Expected ( or AS after table name");
        return stmt;
    }

    while (true) {
        if (matchKeyword(TokenType::KW_INHERITS)) {
            consume(TokenType::LEFT_PAREN, "Expected ( after INHERITS");
            do {
                stmt->inherits.push_back(
                    buildPathFromQualified(string_pool_, parseQualifiedName()));
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ) after INHERITS list");
            continue;
        }

        if (matchKeyword(TokenType::KW_WITH)) {
            if (check(TokenType::LEFT_PAREN)) {
                parse_with_storage_options();
                continue;
            }
            if (stmt->as_query) {
                if (matchKeyword(TokenType::KW_NO)) {
                    consumeKeyword(TokenType::KW_DATA, "Expected DATA after WITH NO");
                } else {
                    matchKeyword(TokenType::KW_DATA);
                }
                continue;
            }
            error("Expected ( after WITH in CREATE TABLE");
        }

        if (matchKeyword(TokenType::KW_ON)) {
            consumeKeyword(TokenType::KW_COMMIT, "Expected COMMIT");
            if (matchKeyword(TokenType::KW_DELETE)) {
                matchKeyword(TokenType::KW_ROWS);
                stmt->on_commit = parser::v3::TempOnCommitAction::DELETE_ROWS;
            } else if (matchKeyword(TokenType::KW_PRESERVE)) {
                matchKeyword(TokenType::KW_ROWS);
                stmt->on_commit = parser::v3::TempOnCommitAction::PRESERVE_ROWS;
            } else if (matchKeyword(TokenType::KW_DROP)) {
                stmt->on_commit = parser::v3::TempOnCommitAction::DROP;
            } else {
                error("Expected DELETE, PRESERVE, or DROP after ON COMMIT");
            }
            continue;
        }

        if (matchKeyword(TokenType::KW_TABLESPACE)) {
            stmt->tablespace = buildPathFromQualified(string_pool_, parseIdentifier());
            stmt->has_tablespace = true;
            continue;
        }

        if (matchKeyword(TokenType::KW_PARTITION)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY after PARTITION");
            if (matchKeyword(TokenType::KW_RANGE)) {
                stmt->partition_by = string_pool_.intern("RANGE");
            } else if (matchKeyword(TokenType::KW_LIST)) {
                stmt->partition_by = string_pool_.intern("LIST");
            } else if (matchKeyword(TokenType::KW_HASH)) {
                stmt->partition_by = string_pool_.intern("HASH");
            } else {
                error("Expected RANGE, LIST, or HASH after PARTITION BY");
            }
            consume(TokenType::LEFT_PAREN, "Expected ( after PARTITION BY");
            do {
                stmt->partition_columns.push_back(parseIdentifierId());
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected ) after partition columns");
            stmt->is_partitioned = true;
            continue;
        }

        if (!stmt->as_query && !has_table_definition &&
            matchKeyword(TokenType::KW_AS)) {
            stmt->as_query = parseSelectStmt();
            continue;
        }

        break;
    }

    return stmt;
}

parser::v3::CreateIndexStmt* Parser::parseCreateIndexV3(bool unique) {
    auto* stmt = arena()->create<parser::v3::CreateIndexStmt>();
    stmt->unique = unique;

    if (matchKeyword(TokenType::KW_CONCURRENTLY)) {
        stmt->concurrent = true;
    }

    bool if_not_exists = false;
    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        if_not_exists = true;
    }
    stmt->if_not_exists = if_not_exists;

    std::string index_name;
    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
        index_name = parseIdentifier();
    }

    consumeKeyword(TokenType::KW_ON, "Expected ON");
    std::string table_path = parseQualifiedName();
    stmt->table_path = buildPathFromQualified(string_pool_, table_path);
    if (index_name.empty()) {
        std::string base = std::string(string_pool_.get(stmt->table_path.objectName()));
        index_name = base.empty() ? std::string("index") : base + "_idx";
    }
    stmt->index_name = string_pool_.intern(index_name);

    if (matchKeyword(TokenType::KW_USING)) {
        std::string method_name = parseIdentifier();
        std::string lower = method_name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "btree") stmt->index_type = parser::v3::IndexType::BTREE;
        else if (lower == "hash") stmt->index_type = parser::v3::IndexType::HASH;
        else if (lower == "gin") stmt->index_type = parser::v3::IndexType::GIN;
        else if (lower == "gist") stmt->index_type = parser::v3::IndexType::GIST;
        else if (lower == "spgist") stmt->index_type = parser::v3::IndexType::SPGIST;
        else if (lower == "brin") stmt->index_type = parser::v3::IndexType::BRIN;
        else error("PostgreSQL index method must be btree, hash, gin, gist, spgist, or brin");
    }

    if (stmt->unique && stmt->index_type == parser::v3::IndexType::HASH) {
        error("CREATE UNIQUE INDEX USING hash is not supported in PostgreSQL dialect");
    }

    consume(TokenType::LEFT_PAREN, "Expected (");
    do {
        parser::v3::IndexColumn col;
        parser::v3::Expression* expr = parseExpression();
        if (expr && expr->kind() == parser::v3::ASTKind::ColumnRefExpr) {
            auto* cref = static_cast<parser::v3::ColumnRefExpr*>(expr);
            if (!cref->column.has_table_qualifier) {
                col.column = cref->column.column_name;
            } else {
                col.expr = expr;
            }
        } else {
            col.expr = expr;
        }

        if (matchKeyword(TokenType::KW_COLLATE)) {
            parseIdentifier();
        }

        if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
            col.opclass = parseIdentifierId();
        }

        if (matchKeyword(TokenType::KW_DESC)) {
            col.ascending = false;
        } else {
            matchKeyword(TokenType::KW_ASC);
            col.ascending = true;
        }

        if (matchKeyword(TokenType::KW_NULLS)) {
            if (matchKeyword(TokenType::KW_FIRST)) {
                col.nulls_first = true;
            } else if (matchKeyword(TokenType::KW_LAST)) {
                col.nulls_last = true;
            }
        }

        stmt->columns.push_back(std::move(col));
    } while (match(TokenType::COMMA));
    consume(TokenType::RIGHT_PAREN, "Expected )");

    if (matchKeyword(TokenType::KW_INCLUDE)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        do {
            stmt->include_columns.push_back(parseIdentifierId());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    if (matchKeyword(TokenType::KW_WHERE)) {
        stmt->where_clause = parseExpression();
    }

    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        stmt->tablespace = buildPathFromQualified(string_pool_, parseIdentifier());
        stmt->has_tablespace = true;
    }

    return stmt;
}

parser::v3::CreateViewStmt* Parser::parseCreateViewV3(bool materialized,
                                                      bool or_replace,
                                                      bool temporary) {
    auto* stmt = arena()->create<parser::v3::CreateViewStmt>();
    stmt->or_replace = or_replace;
    stmt->temporary = temporary;
    stmt->materialized = materialized;

    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        stmt->if_not_exists = true;
    }

    std::string schema;
    std::string view_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = view_name;
        view_name = parseIdentifier();
    }
    resolveTableName(schema, view_name);
    std::string view_path = schema.empty() ? view_name : schema + "." + view_name;
    stmt->view_path = buildPathFromQualified(string_pool_, view_path);

    if (match(TokenType::LEFT_PAREN)) {
        do {
            stmt->column_names.push_back(parseIdentifierId());
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    consumeKeyword(TokenType::KW_AS, "Expected AS");
    stmt->query = parseSelectStmt();

    if (materialized) {
        if (matchKeyword(TokenType::KW_WITH)) {
            consumeKeyword(TokenType::KW_DATA, "Expected DATA");
            stmt->with_data = true;
        } else if (matchKeyword(TokenType::KW_WITHOUT)) {
            consumeKeyword(TokenType::KW_DATA, "Expected DATA");
            stmt->with_data = false;
        }
    }

    return stmt;
}

parser::v3::CreateSequenceStmt* Parser::parseCreateSequenceV3(bool or_replace,
                                                              bool temporary) {
    auto* stmt = arena()->create<parser::v3::CreateSequenceStmt>();
    stmt->or_replace = or_replace;
    stmt->temporary = temporary;

    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        stmt->if_not_exists = true;
    }

    std::string schema;
    std::string seq_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = seq_name;
        seq_name = parseIdentifier();
    }
    resolveTableName(schema, seq_name);
    std::string seq_path = schema.empty() ? seq_name : schema + "." + seq_name;
    stmt->sequence_path = buildPathFromQualified(string_pool_, seq_path);

    bool has_owned_by = false;
    std::string owned_by_table;
    std::string owned_by_column;

    while (true) {
        if (matchKeyword(TokenType::KW_START)) {
            matchKeyword(TokenType::KW_WITH);
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->start_with = current_token_.value.int_value;
                advance();
            }
        } else if (matchKeyword(TokenType::KW_INCREMENT) || matchIdentifierKeyword("INCREMENT")) {
            matchKeyword(TokenType::KW_BY);
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->increment_by = current_token_.value.int_value;
                advance();
            }
        } else if (matchKeyword(TokenType::KW_MINVALUE) || matchIdentifierKeyword("MINVALUE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->min_value = current_token_.value.int_value;
                advance();
            } else {
                stmt->no_min_value = true;
            }
        } else if (matchKeyword(TokenType::KW_NO)) {
            if (matchKeyword(TokenType::KW_MINVALUE) || matchIdentifierKeyword("MINVALUE")) {
                stmt->no_min_value = true;
            } else if (matchKeyword(TokenType::KW_MAXVALUE) || matchIdentifierKeyword("MAXVALUE")) {
                stmt->no_max_value = true;
            } else if (matchKeyword(TokenType::KW_CYCLE) || matchIdentifierKeyword("CYCLE")) {
                stmt->cycle = false;
            }
        } else if (matchKeyword(TokenType::KW_MAXVALUE) || matchIdentifierKeyword("MAXVALUE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->max_value = current_token_.value.int_value;
                advance();
            } else {
                stmt->no_max_value = true;
            }
        } else if (matchKeyword(TokenType::KW_CACHE) || matchIdentifierKeyword("CACHE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->cache = current_token_.value.int_value;
                advance();
            }
        } else if (matchKeyword(TokenType::KW_CYCLE) || matchIdentifierKeyword("CYCLE")) {
            stmt->cycle = true;
        } else if (matchKeyword(TokenType::KW_OWNED)) {
            consumeKeyword(TokenType::KW_BY, "Expected BY");
            has_owned_by = true;
            std::string part1 = parseIdentifier();
            consume(TokenType::DOT, "Expected .");
            std::string part2 = parseIdentifier();
            std::string table_schema;
            std::string table_name;
            if (match(TokenType::DOT)) {
                table_schema = part1;
                table_name = part2;
                owned_by_column = parseIdentifier();
            } else {
                table_schema.clear();
                table_name = part1;
                owned_by_column = part2;
            }
            resolveTableName(table_schema, table_name);
            owned_by_table = table_schema.empty() ? table_name : table_schema + "." + table_name;
        } else {
            break;
        }
    }

    if (has_owned_by) {
        stmt->has_owned_by = true;
        stmt->owned_by_table = buildPathFromQualified(string_pool_, owned_by_table);
        stmt->owned_by_column = string_pool_.intern(owned_by_column);
    }

    return stmt;
}

parser::v3::CreateFunctionStmt* Parser::parseCreateFunctionV3(bool or_replace) {
    auto* stmt = arena()->create<parser::v3::CreateFunctionStmt>();
    stmt->or_replace = or_replace;

    std::string schema;
    std::string func_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = func_name;
        func_name = parseIdentifier();
    }
    resolveTableName(schema, func_name);
    std::string full = schema.empty() ? func_name : schema + "." + func_name;
    stmt->function_path = buildPathFromQualified(string_pool_, full);

    consume(TokenType::LEFT_PAREN, "Expected (");
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            parser::v3::RoutineParam param;
            if (matchKeyword(TokenType::KW_OUT)) param.mode = parser::v3::RoutineParamMode::OUT;
            else if (matchKeyword(TokenType::KW_INOUT)) param.mode = parser::v3::RoutineParamMode::INOUT;
            else if (matchKeyword(TokenType::KW_IN)) param.mode = parser::v3::RoutineParamMode::IN;

            auto starts_keyword_type = [&]() {
                return check(TokenType::KW_SMALLINT) || check(TokenType::KW_INT2) ||
                       check(TokenType::KW_INTEGER) || check(TokenType::KW_INT) ||
                       check(TokenType::KW_INT4) || check(TokenType::KW_OID) ||
                       check(TokenType::KW_BIGINT) || check(TokenType::KW_INT8) ||
                       check(TokenType::KW_REAL) || check(TokenType::KW_FLOAT4) ||
                       check(TokenType::KW_FLOAT8) || check(TokenType::KW_DOUBLE) ||
                       check(TokenType::KW_NUMERIC) || check(TokenType::KW_DECIMAL) ||
                       check(TokenType::KW_CHAR) || check(TokenType::KW_CHARACTER) ||
                       check(TokenType::KW_VARCHAR) || check(TokenType::KW_TEXT) ||
                       check(TokenType::KW_BYTEA) || check(TokenType::KW_BOOLEAN) ||
                       check(TokenType::KW_BOOL) || check(TokenType::KW_UUID) ||
                       check(TokenType::KW_DATE) || check(TokenType::KW_TIME) ||
                       check(TokenType::KW_TIMESTAMP) || check(TokenType::KW_JSON) ||
                       check(TokenType::KW_JSONB) || check(TokenType::KW_XML);
            };

            if (starts_keyword_type()) {
                param.name = parser::v3::StringPool::INVALID_ID;
                param.type = parseTypeNameV3();
            } else {
                auto first_id = parseIdentifierId();
                if (first_id == parser::v3::StringPool::INVALID_ID) {
                    synchronize();
                    return stmt;
                }
                if (check(TokenType::COMMA) || check(TokenType::RIGHT_PAREN) ||
                    check(TokenType::KW_DEFAULT) || check(TokenType::EQUAL)) {
                    // Unnamed parameter using identifier-backed type (e.g., oid).
                    std::string type_name = std::string(string_pool_.get(first_id));
                    param.name = parser::v3::StringPool::INVALID_ID;
                    param.type.name = string_pool_.intern(type_name);
                    param.type.has_schema_path = true;
                    param.type.schema_path = buildPathFromQualified(string_pool_, type_name);
                } else {
                    param.name = first_id;
                    param.type = parseTypeNameV3();
                }
            }

            if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                param.has_default = true;
                param.default_value = parseExpression();
            }
            stmt->params.push_back(std::move(param));
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_PAREN, "Expected )");

    if (matchKeyword(TokenType::KW_RETURNS)) {
        if (matchKeyword(TokenType::KW_TABLE)) {
            consume(TokenType::LEFT_PAREN, "Expected (");
            consume(TokenType::RIGHT_PAREN, "Expected )");
        } else if (matchKeyword(TokenType::KW_SETOF)) {
            stmt->return_type = parseTypeNameV3();
        } else {
            stmt->return_type = parseTypeNameV3();
        }
    }

    if (matchKeyword(TokenType::KW_AS)) {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING)) {
            stmt->body = internFromLexer(current_token_.value.string_id);
            advance();
            if (match(TokenType::COMMA)) {
                if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING)) {
                    advance();
                } else {
                    parseExpression();
                }
            }
        }
    }

    // Consume PostgreSQL function attributes and alternate SQL-body tails.
    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        advance();
    }

    return stmt;
}

parser::v3::CreateProcedureStmt* Parser::parseCreateProcedureV3(bool or_replace) {
    auto* stmt = arena()->create<parser::v3::CreateProcedureStmt>();
    stmt->or_replace = or_replace;

    std::string schema;
    std::string proc_name = parseIdentifier();
    if (match(TokenType::DOT)) {
        schema = proc_name;
        proc_name = parseIdentifier();
    }
    resolveTableName(schema, proc_name);
    std::string full = schema.empty() ? proc_name : schema + "." + proc_name;
    stmt->procedure_path = buildPathFromQualified(string_pool_, full);

    consume(TokenType::LEFT_PAREN, "Expected (");
    if (!check(TokenType::RIGHT_PAREN)) {
        do {
            parser::v3::RoutineParam param;
            if (matchKeyword(TokenType::KW_OUT)) param.mode = parser::v3::RoutineParamMode::OUT;
            else if (matchKeyword(TokenType::KW_INOUT)) param.mode = parser::v3::RoutineParamMode::INOUT;
            else if (matchKeyword(TokenType::KW_IN)) param.mode = parser::v3::RoutineParamMode::IN;

            param.name = parseIdentifierId();
            param.type = parseTypeNameV3();
            if (matchKeyword(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                param.has_default = true;
                param.default_value = parseExpression();
            }
            stmt->params.push_back(std::move(param));
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RIGHT_PAREN, "Expected )");

    if (matchKeyword(TokenType::KW_AS)) {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::DOLLAR_STRING)) {
            stmt->body = internFromLexer(current_token_.value.string_id);
            advance();
        }
    }

    return stmt;
}

parser::v3::CreateTriggerStmt* Parser::parseCreateTriggerV3(bool or_replace) {
    auto* stmt = arena()->create<parser::v3::CreateTriggerStmt>();
    stmt->or_replace = or_replace;

    stmt->trigger_name = parseIdentifierId();

    if (matchKeyword(TokenType::KW_BEFORE)) {
        stmt->timing = parser::v3::TriggerTiming::BEFORE;
    } else if (matchKeyword(TokenType::KW_AFTER)) {
        stmt->timing = parser::v3::TriggerTiming::AFTER;
    } else if (matchKeyword(TokenType::KW_INSTEAD)) {
        consumeKeyword(TokenType::KW_OF, "Expected OF");
        stmt->timing = parser::v3::TriggerTiming::INSTEAD_OF;
    }

    stmt->event_mask = 0;
    do {
        if (matchKeyword(TokenType::KW_INSERT)) {
            stmt->event_mask |= 1u << static_cast<uint8_t>(parser::v3::TriggerEvent::INSERT);
        } else if (matchKeyword(TokenType::KW_UPDATE)) {
            stmt->event_mask |= 1u << static_cast<uint8_t>(parser::v3::TriggerEvent::UPDATE);
            if (matchKeyword(TokenType::KW_OF)) {
                do { parseIdentifier(); } while (match(TokenType::COMMA));
            }
        } else if (matchKeyword(TokenType::KW_DELETE)) {
            stmt->event_mask |= 1u << static_cast<uint8_t>(parser::v3::TriggerEvent::DELETE);
        } else if (matchKeyword(TokenType::KW_TRUNCATE)) {
            stmt->event_mask |= 1u << 3;
        }
    } while (matchKeyword(TokenType::KW_OR));

    consumeKeyword(TokenType::KW_ON, "Expected ON");
    stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());

    if (matchKeyword(TokenType::KW_FOR)) {
        matchKeyword(TokenType::KW_EACH);
        if (matchKeyword(TokenType::KW_ROW)) {
            stmt->granularity = parser::v3::TriggerGranularity::FOR_EACH_ROW;
        } else if (matchKeyword(TokenType::KW_STATEMENT)) {
            stmt->granularity = parser::v3::TriggerGranularity::FOR_EACH_STATEMENT;
        }
    }

    if (matchKeyword(TokenType::KW_WHEN)) {
        consume(TokenType::LEFT_PAREN, "Expected (");
        parseExpression();
        consume(TokenType::RIGHT_PAREN, "Expected )");
    }

    consumeKeyword(TokenType::KW_EXECUTE, "Expected EXECUTE");
    matchKeyword(TokenType::KW_FUNCTION) || matchKeyword(TokenType::KW_PROCEDURE);
    std::string func_name = parseQualifiedName();
    stmt->body = string_pool_.intern(func_name);
    consume(TokenType::LEFT_PAREN, "Expected (");
    consume(TokenType::RIGHT_PAREN, "Expected )");

    return stmt;
}

parser::v3::CreateTypeStmt* Parser::parseCreateTypeV3() {
    auto* stmt = arena()->create<parser::v3::CreateTypeStmt>();

    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        stmt->if_not_exists = true;
    }

    auto name_pair = splitQualifiedName(parseQualifiedName());
    std::string schema = name_pair.first;
    std::string type_name = name_pair.second;
    resolveTableName(schema, type_name);
    std::string full = schema.empty() ? type_name : schema + "." + type_name;
    stmt->type_path = buildPathFromQualified(string_pool_, full);

    auto emit_domain_marker = [&](uint8_t domain_kind) {
        emit(sblr::Opcode::EXTENDED_OPCODE);
        emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));
        emitByte(stmt->if_not_exists ? 0x01 : 0x00);
        emitByte(domain_kind);
        emitString(full);
    };

    consumeKeyword(TokenType::KW_AS, "Expected AS");

    if (matchKeyword(TokenType::KW_ENUM)) {
        stmt->type_kind = parser::v3::TypeKind::ENUM;
        emit_domain_marker(2);
        consume(TokenType::LEFT_PAREN, "Expected (");
        int32_t position = 1;
        do {
            if (check(TokenType::STRING_LITERAL)) {
                parser::v3::DomainEnumValue value;
                value.label = internFromLexer(current_token_.value.string_id);
                value.has_position = true;
                value.position = position++;
                stmt->enum_values.push_back(std::move(value));
                advance();
            } else {
                error("Expected string literal in ENUM definition");
                synchronize();
                return stmt;
            }
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return stmt;
    }

    if (matchKeyword(TokenType::KW_RANGE)) {
        stmt->type_kind = parser::v3::TypeKind::RANGE;
        if (match(TokenType::LEFT_PAREN)) {
            do {
                std::string opt = parseIdentifier();
                std::string opt_lower = opt;
                std::transform(opt_lower.begin(), opt_lower.end(), opt_lower.begin(), ::tolower);
                if (match(TokenType::EQUAL)) {
                    // consume '='
                }
                if (opt_lower == "subtype") {
                    stmt->range_options.subtype = parseTypeNameV3();
                    stmt->range_options.has_subtype = true;
                } else if (opt_lower == "subtype_opclass") {
                    stmt->range_options.subtype_opclass = parseQualifiedName();
                    stmt->range_options.has_subtype_opclass = true;
                } else if (opt_lower == "collation") {
                    stmt->range_options.subtype_collation = parseQualifiedName();
                    stmt->range_options.has_subtype_collation = true;
                } else if (opt_lower == "canonical") {
                    stmt->range_options.canonical = parseQualifiedName();
                    stmt->range_options.has_canonical = true;
                } else if (opt_lower == "subtype_diff") {
                    stmt->range_options.subtype_diff = parseQualifiedName();
                    stmt->range_options.has_subtype_diff = true;
                } else if (opt_lower == "multirange_type_name") {
                    parseQualifiedName();
                    stmt->range_options.has_multirange = true;
                    stmt->range_options.multirange = true;
                } else {
                    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                        parseIdentifier();
                    } else {
                        parseExpression();
                    }
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        return stmt;
    }

    if (match(TokenType::LEFT_PAREN)) {
        stmt->type_kind = parser::v3::TypeKind::RECORD;
        emit_domain_marker(1);
        do {
            parser::v3::DomainRecordField field;
            field.name = parseIdentifierId();
            field.type = parseTypeNameV3();
            if (matchKeyword(TokenType::KW_COLLATE)) {
                parseIdentifier();
            }
            stmt->record_fields.push_back(std::move(field));
        } while (match(TokenType::COMMA));
        consume(TokenType::RIGHT_PAREN, "Expected )");
        return stmt;
    }

    error("Expected ENUM, RANGE, or composite definition after CREATE TYPE");
    return stmt;
}

parser::v3::CreateDomainStmt* Parser::parseCreateDomainV3() {
    auto* stmt = arena()->create<parser::v3::CreateDomainStmt>();

    if (matchKeyword(TokenType::KW_IF)) {
        consumeKeyword(TokenType::KW_NOT, "Expected NOT");
        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
        stmt->if_not_exists = true;
    }

    auto name_pair = splitQualifiedName(parseQualifiedName());
    std::string schema = name_pair.first;
    std::string domain_name = name_pair.second;
    resolveTableName(schema, domain_name);
    std::string full = schema.empty() ? domain_name : schema + "." + domain_name;
    stmt->domain_path = buildPathFromQualified(string_pool_, full);

    emit(sblr::Opcode::EXTENDED_OPCODE);
    emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_CREATE_DOMAIN));
    emitByte(stmt->if_not_exists ? 0x01 : 0x00);
    emitByte(0);  // BASIC domain kind
    emitString(full);

    matchKeyword(TokenType::KW_AS);
    stmt->base_type = parseTypeNameV3();

    parser::v3::StringPool::StringId pending_name = parser::v3::StringPool::INVALID_ID;
    auto apply_name = [&](parser::v3::DomainConstraint& constraint) {
        if (pending_name != parser::v3::StringPool::INVALID_ID) {
            constraint.name = pending_name;
            pending_name = parser::v3::StringPool::INVALID_ID;
        }
    };

    while (true) {
        if (matchKeyword(TokenType::KW_COLLATE)) {
            stmt->has_collation = true;
            stmt->collation_name = parseIdentifier();
            continue;
        }
        if (matchKeyword(TokenType::KW_CONSTRAINT)) {
            pending_name = parseIdentifierId();
            continue;
        }
        if (matchKeyword(TokenType::KW_DEFAULT)) {
            parser::v3::DomainConstraint constraint;
            constraint.type = parser::v3::DomainConstraintType::DEFAULT;
            apply_name(constraint);
            constraint.expression = parseExpressionText();
            stmt->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_NOT)) {
            consumeKeyword(TokenType::KW_NULL, "Expected NULL");
            parser::v3::DomainConstraint constraint;
            constraint.type = parser::v3::DomainConstraintType::NOT_NULL;
            apply_name(constraint);
            stmt->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_NULL)) {
            parser::v3::DomainConstraint constraint;
            constraint.type = parser::v3::DomainConstraintType::NULL_ALLOWED;
            apply_name(constraint);
            stmt->constraints.push_back(std::move(constraint));
            continue;
        }
        if (matchKeyword(TokenType::KW_CHECK)) {
            parser::v3::DomainConstraint constraint;
            constraint.type = parser::v3::DomainConstraintType::CHECK;
            apply_name(constraint);
            consume(TokenType::LEFT_PAREN, "Expected (");
            constraint.expression = parseExpressionText();
            consume(TokenType::RIGHT_PAREN, "Expected )");
            stmt->constraints.push_back(std::move(constraint));
            continue;
        }
        break;
    }

    return stmt;
}

parser::v3::Statement* Parser::parseAlterStmtV3() {
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

    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        auto* stmt = arena()->create<parser::v3::AlterTablespaceStmt>();
        stmt->tablespace_name = parseIdentifierId();

        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO after RENAME");
            parser::v3::TablespaceAlteration alter;
            alter.action = parser::v3::TablespaceAlterAction::RENAME_TO;
            alter.new_name = parseIdentifierId();
            stmt->alterations.push_back(std::move(alter));
            return stmt;
        }

        bool is_reset = false;
        bool set_reset = false;
        if (matchKeyword(TokenType::KW_SET)) {
            is_reset = false;
            set_reset = true;
        } else if (matchKeyword(TokenType::KW_RESET)) {
            is_reset = true;
            set_reset = true;
        }
        if (!set_reset) {
            return stmt;
        }

        if (match(TokenType::LEFT_PAREN)) {
            do {
                std::string option = parseIdentifier();
                std::string lower = option;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (match(TokenType::EQUAL)) {
                    if (lower == "autoextend") {
                        parser::v3::TablespaceAlteration alter;
                        alter.action = parser::v3::TablespaceAlterAction::SET_AUTOEXTEND;
                        uint8_t enabled = 1;
                        if (matchKeyword(TokenType::KW_OFF) || matchKeyword(TokenType::KW_FALSE)) {
                            enabled = 0;
                        } else if (matchKeyword(TokenType::KW_ON) || matchKeyword(TokenType::KW_TRUE)) {
                            enabled = 1;
                        }
                        alter.autoextend_enabled = enabled != 0;
                        stmt->alterations.push_back(std::move(alter));
                    } else if (lower == "autoextend_size" || lower == "autoextend_size_mb") {
                        parser::v3::TablespaceAlteration alter;
                        alter.action = parser::v3::TablespaceAlterAction::SET_AUTOEXTEND_SIZE;
                        if (check(TokenType::INTEGER_LITERAL)) {
                            alter.size_mb = static_cast<uint32_t>(current_token_.value.int_value);
                            advance();
                        } else {
                            error("Expected integer for autoextend_size");
                        }
                        stmt->alterations.push_back(std::move(alter));
                    } else if (lower == "maxsize") {
                        parser::v3::TablespaceAlteration alter;
                        alter.action = parser::v3::TablespaceAlterAction::SET_MAXSIZE;
                        if (check(TokenType::INTEGER_LITERAL)) {
                            alter.size_mb = static_cast<uint32_t>(current_token_.value.int_value);
                            advance();
                        } else if (matchKeyword(TokenType::KW_UNLIMITED)) {
                            alter.size_mb = 0;
                        } else {
                            error("Expected integer or UNLIMITED for maxsize");
                        }
                        stmt->alterations.push_back(std::move(alter));
                    }
                } else if (is_reset && lower == "autoextend") {
                    parser::v3::TablespaceAlteration alter;
                    alter.action = parser::v3::TablespaceAlterAction::SET_AUTOEXTEND;
                    alter.autoextend_enabled = false;
                    stmt->alterations.push_back(std::move(alter));
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
        return stmt;
    }

    if (matchIdentifierKeyword("EXTENSION")) {
        std::string extension_name = parseIdentifier();
        std::string payload = capture_remaining_clause();
        if (payload.empty()) {
            error("ALTER EXTENSION requires action clause");
        }
        return make_alter_system_stmt("platform.extension.alter." + extension_name, payload);
    }

    if (matchIdentifierKeyword("PUBLICATION")) {
        std::string publication_name = parseIdentifier();
        std::string payload = capture_remaining_clause();
        if (payload.empty()) {
            error("ALTER PUBLICATION requires action clause");
        }
        return make_alter_system_stmt("replication.publication.alter." + publication_name, payload);
    }

    if (matchIdentifierKeyword("SUBSCRIPTION")) {
        std::string subscription_name = parseIdentifier();
        std::string payload = capture_remaining_clause();
        if (payload.empty()) {
            error("ALTER SUBSCRIPTION requires action clause");
        }
        return make_alter_system_stmt("replication.subscription.alter." + subscription_name, payload);
    }

    if (matchKeyword(TokenType::KW_TABLE)) {
        auto* stmt = arena()->create<parser::v3::AlterTableStmt>();

        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }

        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        resolveTableName(schema, table);
        std::string full = schema.empty() ? table : schema + "." + table;
        stmt->table_path = buildPathFromQualified(string_pool_, full);

        if (matchKeyword(TokenType::KW_RENAME)) {
            if (matchKeyword(TokenType::KW_COLUMN)) {
                stmt->action = parser::v3::AlterTableAction::RENAME_COLUMN;
                stmt->column_name = parseIdentifierId();
                consumeKeyword(TokenType::KW_TO, "Expected TO");
                stmt->new_name = parseIdentifierId();
                return stmt;
            }
            if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                stmt->action = parser::v3::AlterTableAction::RENAME_CONSTRAINT;
                stmt->constraint_name = parseIdentifierId();
                consumeKeyword(TokenType::KW_TO, "Expected TO");
                stmt->new_name = parseIdentifierId();
                return stmt;
            }
            if (matchKeyword(TokenType::KW_TO)) {
                stmt->action = parser::v3::AlterTableAction::RENAME_TABLE;
                stmt->new_name = parseIdentifierId();
                return stmt;
            }
            error("Expected COLUMN, CONSTRAINT, or TO after RENAME");
            return stmt;
        }

        if (matchKeyword(TokenType::KW_SET)) {
            if (matchKeyword(TokenType::KW_SCHEMA)) {
                stmt->action = parser::v3::AlterTableAction::SET_SCHEMA;
                std::string new_schema = parseIdentifier();
                stmt->target_schema = buildPathFromDefault(string_pool_, default_schema_, new_schema);
                return stmt;
            }
            if (matchIdentifierKeyword("STATISTICS")) {
                stmt->action = parser::v3::AlterTableAction::SET_STATISTICS;
                stmt->column_name = parseIdentifierId();
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->statistics_target = static_cast<int32_t>(current_token_.value.int_value);
                    stmt->has_statistics_target = true;
                    advance();
                } else {
                    error("Expected integer for SET STATISTICS");
                }
                return stmt;
            }
            if (matchIdentifierKeyword("STORAGE")) {
                stmt->action = parser::v3::AlterTableAction::SET_STORAGE;
                stmt->column_name = parseIdentifierId();
                stmt->storage_type = parseIdentifierId();
                return stmt;
            }
        }

        uint8_t rls_action = 0;
        if (matchKeyword(TokenType::KW_ENABLE)) rls_action = 1;
        else if (matchKeyword(TokenType::KW_DISABLE)) rls_action = 2;
        else if (matchKeyword(TokenType::KW_FORCE)) rls_action = 3;
        else if (matchKeyword(TokenType::KW_NO)) {
            consumeKeyword(TokenType::KW_FORCE, "Expected FORCE after NO");
            rls_action = 4;
        }
        if (rls_action != 0) {
            consumeKeyword(TokenType::KW_ROW, "Expected ROW");
            consumeKeyword(TokenType::KW_LEVEL, "Expected LEVEL");
            consumeKeyword(TokenType::KW_SECURITY, "Expected SECURITY");
            switch (rls_action) {
                case 1: stmt->action = parser::v3::AlterTableAction::ENABLE_RLS; break;
                case 2: stmt->action = parser::v3::AlterTableAction::DISABLE_RLS; break;
                case 3: stmt->action = parser::v3::AlterTableAction::FORCE_RLS; break;
                case 4: stmt->action = parser::v3::AlterTableAction::NO_FORCE_RLS; break;
                default: break;
            }
            return stmt;
        }

        if (matchKeyword(TokenType::KW_ADD)) {
            if (matchKeyword(TokenType::KW_COLUMN)) {
                stmt->action = parser::v3::AlterTableAction::ADD_COLUMN;
                stmt->column = parseColumnDefV3();
                return stmt;
            }
            if (check(TokenType::KW_CONSTRAINT) || check(TokenType::KW_PRIMARY) ||
                check(TokenType::KW_UNIQUE) || check(TokenType::KW_FOREIGN) ||
                check(TokenType::KW_CHECK) || check(TokenType::KW_EXCLUDE)) {
                stmt->action = parser::v3::AlterTableAction::ADD_CONSTRAINT;
                stmt->constraint = parseTableConstraintV3();
                return stmt;
            }
        }

        if (matchKeyword(TokenType::KW_DROP)) {
            if (matchKeyword(TokenType::KW_COLUMN)) {
                if (matchKeyword(TokenType::KW_IF)) {
                    consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
                    stmt->if_exists = true;
                }
                stmt->action = parser::v3::AlterTableAction::DROP_COLUMN;
                stmt->column_name = parseIdentifierId();
                if (matchKeyword(TokenType::KW_CASCADE)) {
                    stmt->cascade = true;
                } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                    stmt->cascade = false;
                }
                return stmt;
            }
            if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                if (matchKeyword(TokenType::KW_IF)) {
                    consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
                    stmt->if_exists = true;
                }
                stmt->action = parser::v3::AlterTableAction::DROP_CONSTRAINT;
                stmt->constraint_name = parseIdentifierId();
                if (matchKeyword(TokenType::KW_CASCADE)) {
                    stmt->cascade = true;
                } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                    stmt->cascade = false;
                }
                return stmt;
            }
        }

        if (matchKeyword(TokenType::KW_ALTER)) {
            matchKeyword(TokenType::KW_COLUMN);
            stmt->column_name = parseIdentifierId();
            if (matchKeyword(TokenType::KW_TYPE)) {
                stmt->action = parser::v3::AlterTableAction::ALTER_COLUMN;
                auto* col = arena()->create<parser::v3::ColumnDef>();
                col->name = stmt->column_name;
                col->type = parseTypeNameV3();
                stmt->column = col;
                if (matchKeyword(TokenType::KW_USING)) {
                    parseExpression();
                }
                return stmt;
            }
            if (matchKeyword(TokenType::KW_SET)) {
                if (matchKeyword(TokenType::KW_DATA)) {
                    consumeKeyword(TokenType::KW_TYPE, "Expected TYPE");
                    stmt->action = parser::v3::AlterTableAction::ALTER_COLUMN;
                    auto* col = arena()->create<parser::v3::ColumnDef>();
                    col->name = stmt->column_name;
                    col->type = parseTypeNameV3();
                    stmt->column = col;
                    if (matchKeyword(TokenType::KW_USING)) {
                        parseExpression();
                    }
                    return stmt;
                }
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    stmt->action = parser::v3::AlterTableAction::ALTER_COLUMN_SET_DEFAULT;
                    stmt->default_expr = parseExpression();
                    stmt->has_default_expr = true;
                    return stmt;
                }
                if (matchKeyword(TokenType::KW_NOT)) {
                    consumeKeyword(TokenType::KW_NULL, "Expected NULL");
                    stmt->action = parser::v3::AlterTableAction::ALTER_COLUMN_SET_NOT_NULL;
                    return stmt;
                }
            }
            if (matchKeyword(TokenType::KW_DROP)) {
                if (matchKeyword(TokenType::KW_DEFAULT)) {
                    stmt->action = parser::v3::AlterTableAction::ALTER_COLUMN_DROP_DEFAULT;
                    return stmt;
                }
                if (matchKeyword(TokenType::KW_NOT)) {
                    consumeKeyword(TokenType::KW_NULL, "Expected NULL");
                    stmt->action = parser::v3::AlterTableAction::ALTER_COLUMN_DROP_NOT_NULL;
                    return stmt;
                }
            }
            error("Expected TYPE, SET, or DROP after ALTER COLUMN");
            return stmt;
        }

        if (matchKeyword(TokenType::KW_ENABLE)) {
            stmt->action = parser::v3::AlterTableAction::ENABLE_TRIGGER;
            consumeKeyword(TokenType::KW_TRIGGER, "Expected TRIGGER");
            if (matchKeyword(TokenType::KW_ALL)) {
                stmt->trigger_all = true;
            } else if (matchKeyword(TokenType::KW_USER)) {
                stmt->trigger_all = true;
            } else {
                stmt->trigger_name = parseIdentifierId();
            }
            return stmt;
        }
        if (matchKeyword(TokenType::KW_DISABLE)) {
            stmt->action = parser::v3::AlterTableAction::DISABLE_TRIGGER;
            consumeKeyword(TokenType::KW_TRIGGER, "Expected TRIGGER");
            if (matchKeyword(TokenType::KW_ALL)) {
                stmt->trigger_all = true;
            } else if (matchKeyword(TokenType::KW_USER)) {
                stmt->trigger_all = true;
            } else {
                stmt->trigger_name = parseIdentifierId();
            }
            return stmt;
        }

        error("Unsupported ALTER TABLE action");
        return stmt;
    }

    if (matchKeyword(TokenType::KW_VIEW)) {
        auto* stmt = arena()->create<parser::v3::RenameObjectStmt>();
        stmt->object_type = parser::v3::DdlObjectType::VIEW;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        stmt->object_path = buildPathFromQualified(string_pool_, parseQualifiedName());
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            stmt->new_name = parseIdentifierId();
            return stmt;
        }
        if (matchKeyword(TokenType::KW_SET)) {
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            auto* move = arena()->create<parser::v3::MoveObjectStmt>();
            move->object_type = parser::v3::DdlObjectType::VIEW;
            move->if_exists = stmt->if_exists;
            move->object_path = stmt->object_path;
            std::string new_schema = parseIdentifier();
            move->target_schema = buildPathFromDefault(string_pool_, default_schema_, new_schema);
            return move;
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_INDEX)) {
        auto* stmt = arena()->create<parser::v3::RenameObjectStmt>();
        stmt->object_type = parser::v3::DdlObjectType::INDEX;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        stmt->object_path = buildPathFromQualified(string_pool_, parseQualifiedName());
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            stmt->new_name = parseIdentifierId();
            return stmt;
        }
        if (matchKeyword(TokenType::KW_SET)) {
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA");
            auto* move = arena()->create<parser::v3::MoveObjectStmt>();
            move->object_type = parser::v3::DdlObjectType::INDEX;
            move->if_exists = stmt->if_exists;
            move->object_path = stmt->object_path;
            std::string new_schema = parseIdentifier();
            move->target_schema = buildPathFromDefault(string_pool_, default_schema_, new_schema);
            return move;
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SEQUENCE)) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            if_exists = true;
        }
        std::string seq = parseQualifiedName();
        parser::v3::SchemaPath seq_path = buildPathFromQualified(string_pool_, seq);

        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            auto* stmt = arena()->create<parser::v3::RenameObjectStmt>();
            stmt->object_type = parser::v3::DdlObjectType::SEQUENCE;
            stmt->if_exists = if_exists;
            stmt->object_path = seq_path;
            stmt->new_name = parseIdentifierId();
            return stmt;
        }
        if (matchKeyword(TokenType::KW_SET)) {
            if (matchKeyword(TokenType::KW_SCHEMA)) {
                auto* move = arena()->create<parser::v3::MoveObjectStmt>();
                move->object_type = parser::v3::DdlObjectType::SEQUENCE;
                move->if_exists = if_exists;
                move->object_path = seq_path;
                std::string new_schema = parseIdentifier();
                move->target_schema = buildPathFromDefault(string_pool_, default_schema_, new_schema);
                return move;
            }
        }

        auto* stmt = arena()->create<parser::v3::AlterSequenceStmt>();
        stmt->sequence_path = seq_path;
        while (true) {
            if (matchKeyword(TokenType::KW_RESTART)) {
                matchKeyword(TokenType::KW_WITH);
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->restart_with = current_token_.value.int_value;
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_INCREMENT)) {
                matchKeyword(TokenType::KW_BY);
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->increment_by = current_token_.value.int_value;
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_MINVALUE)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->min_value = current_token_.value.int_value;
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_MAXVALUE)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->max_value = current_token_.value.int_value;
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_CACHE)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->cache = current_token_.value.int_value;
                    advance();
                }
            } else if (matchKeyword(TokenType::KW_CYCLE)) {
                stmt->cycle = true;
            } else if (matchKeyword(TokenType::KW_NO)) {
                if (matchKeyword(TokenType::KW_CYCLE)) {
                    stmt->cycle = false;
                }
            } else {
                break;
            }
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SCHEMA)) {
        auto* stmt = arena()->create<parser::v3::AlterSchemaStmt>();
        std::string schema_name = parseIdentifier();
        stmt->schema_path = buildPathFromDefault(string_pool_, default_schema_, schema_name);
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            stmt->action = parser::v3::AlterSchemaAction::RENAME;
            stmt->new_name = parseIdentifierId();
            return stmt;
        }
        if (matchKeyword(TokenType::KW_OWNER)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            stmt->action = parser::v3::AlterSchemaAction::SET_OWNER;
            stmt->owner = parseIdentifierId();
            return stmt;
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_DATABASE)) {
        auto* stmt = arena()->create<parser::v3::AlterDatabaseStmt>();
        std::string db_name = parseIdentifier();
        std::string db_path = buildEmulatedServerRoot(default_schema_);
        if (!db_path.empty()) db_path += ".databases.";
        db_path += db_name;
        stmt->database_path = buildPathFromQualified(string_pool_, db_path);
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            stmt->action = parser::v3::AlterDatabaseAction::RENAME;
            stmt->new_name = parseIdentifierId();
            return stmt;
        }
        if (matchKeyword(TokenType::KW_OWNER)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            stmt->action = parser::v3::AlterDatabaseAction::SET_OWNER;
            stmt->owner = parseIdentifierId();
            return stmt;
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_DOMAIN)) {
        auto* stmt = arena()->create<parser::v3::AlterDomainStmt>();
        auto name_pair = splitQualifiedName(parseQualifiedName());
        std::string schema = name_pair.first;
        std::string domain_name = name_pair.second;
        resolveTableName(schema, domain_name);
        std::string full = schema.empty() ? domain_name : schema + "." + domain_name;
        stmt->domain_path = buildPathFromQualified(string_pool_, full);

        if (matchKeyword(TokenType::KW_SET)) {
            if (matchKeyword(TokenType::KW_DEFAULT)) {
                stmt->action = parser::v3::AlterDomainAction::SET_DEFAULT;
                stmt->value = parseExpressionText();
                return stmt;
            }
        }
        if (matchKeyword(TokenType::KW_DROP)) {
            if (matchKeyword(TokenType::KW_DEFAULT)) {
                stmt->action = parser::v3::AlterDomainAction::DROP_DEFAULT;
                return stmt;
            }
            if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                stmt->action = parser::v3::AlterDomainAction::DROP_CONSTRAINT;
                stmt->constraint_name = parseIdentifierId();
                return stmt;
            }
        }
        if (matchKeyword(TokenType::KW_ADD)) {
            if (matchKeyword(TokenType::KW_CONSTRAINT)) {
                stmt->constraint_name = parseIdentifierId();
            }
            if (matchKeyword(TokenType::KW_CHECK)) {
                stmt->action = parser::v3::AlterDomainAction::ADD_CHECK;
                consume(TokenType::LEFT_PAREN, "Expected (");
                stmt->value = parseExpressionText();
                consume(TokenType::RIGHT_PAREN, "Expected )");
                return stmt;
            }
        }
        if (matchKeyword(TokenType::KW_RENAME)) {
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            stmt->action = parser::v3::AlterDomainAction::RENAME;
            stmt->new_name = parseIdentifierId();
            return stmt;
        }
        return nullptr;
    }

    return nullptr;
}

parser::v3::Statement* Parser::parseDropStmtV3() {
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

    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        auto* stmt = arena()->create<parser::v3::DropTablespaceStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        stmt->tablespace_name = parseIdentifierId();
        if (matchKeyword(TokenType::KW_WITH)) {
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    if (matchKeyword(TokenType::KW_FORCE)) {
                        stmt->force = true;
                    } else {
                        parseIdentifier();
                    }
                } while (match(TokenType::COMMA));
                consume(TokenType::RIGHT_PAREN, "Expected )");
            }
        }
        return stmt;
    }

    if (matchIdentifierKeyword("EXTENSION")) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        std::string extension_name = parseIdentifier();
        std::string payload;
        if (if_exists) {
            payload = "IF_EXISTS=1";
        }
        if (matchKeyword(TokenType::KW_CASCADE)) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("CASCADE=1");
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("RESTRICT=1");
        }
        return make_alter_system_stmt("platform.extension.drop." + extension_name, payload);
    }

    if (matchIdentifierKeyword("PUBLICATION")) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        std::string publication_name = parseIdentifier();
        std::string payload;
        if (if_exists) {
            payload = "IF_EXISTS=1";
        }
        if (matchKeyword(TokenType::KW_CASCADE)) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("CASCADE=1");
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("RESTRICT=1");
        }
        return make_alter_system_stmt(
            "replication.publication.drop." + publication_name,
            payload);
    }

    if (matchIdentifierKeyword("SUBSCRIPTION")) {
        bool if_exists = false;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }
        std::string subscription_name = parseIdentifier();
        std::string payload;
        if (if_exists) {
            payload = "IF_EXISTS=1";
        }
        if (matchKeyword(TokenType::KW_CASCADE)) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("CASCADE=1");
        } else if (matchKeyword(TokenType::KW_RESTRICT)) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("RESTRICT=1");
        }
        return make_alter_system_stmt(
            "replication.subscription.drop." + subscription_name,
            payload);
    }

    if (matchKeyword(TokenType::KW_DATABASE)) {
        auto* stmt = arena()->create<parser::v3::DropDatabaseStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        std::string db_name = parseIdentifier();
        std::string db_path = buildEmulatedServerRoot(default_schema_);
        if (!db_path.empty()) db_path += ".databases.";
        db_path += db_name;
        stmt->database_path = buildPathFromQualified(string_pool_, db_path);
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SCHEMA)) {
        auto* stmt = arena()->create<parser::v3::DropSchemaStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string schema_name = parseQualifiedName();
            stmt->schemas.push_back(buildPathFromDefault(string_pool_, default_schema_, schema_name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        else if (matchKeyword(TokenType::KW_RESTRICT)) stmt->restrict = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_TABLE)) {
        auto* stmt = arena()->create<parser::v3::DropTableStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string schema;
            std::string table = parseIdentifier();
            if (match(TokenType::DOT)) {
                schema = table;
                table = parseIdentifier();
            }
            resolveTableName(schema, table);
            std::string full = schema.empty() ? table : schema + "." + table;
            stmt->tables.push_back(buildPathFromQualified(string_pool_, full));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        else if (matchKeyword(TokenType::KW_RESTRICT)) stmt->restrict = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_INDEX)) {
        auto* stmt = arena()->create<parser::v3::DropIndexStmt>();
        matchKeyword(TokenType::KW_CONCURRENTLY);
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->indexes.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        return stmt;
    }

    if (matchKeyword(TokenType::KW_MATERIALIZED)) {
        consumeKeyword(TokenType::KW_VIEW, "Expected VIEW");
        auto* stmt = arena()->create<parser::v3::DropViewStmt>();
        stmt->materialized = true;
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->views.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_VIEW)) {
        auto* stmt = arena()->create<parser::v3::DropViewStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->views.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_SEQUENCE)) {
        auto* stmt = arena()->create<parser::v3::DropSequenceStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->sequences.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_DOMAIN)) {
        auto* stmt = arena()->create<parser::v3::DropDomainStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->domains.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_RESTRICT)) stmt->restrict = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_TYPE)) {
        auto* stmt = arena()->create<parser::v3::DropTypeStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->types.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_ROLE)) {
        auto* stmt = arena()->create<parser::v3::DropRoleStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseIdentifier();
            stmt->roles.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_USER)) {
        auto* stmt = arena()->create<parser::v3::DropUserStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseIdentifier();
            stmt->users.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_GROUP)) {
        auto* stmt = arena()->create<parser::v3::DropGroupStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseIdentifier();
            stmt->groups.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        if (matchKeyword(TokenType::KW_CASCADE)) stmt->cascade = true;
        return stmt;
    }

    if (matchKeyword(TokenType::KW_POLICY)) {
        auto* stmt = arena()->create<parser::v3::DropPolicyStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        stmt->policy_name = parseIdentifierId();
        consumeKeyword(TokenType::KW_ON, "Expected ON");
        stmt->table_path = buildPathFromQualified(string_pool_, parseQualifiedName());
        return stmt;
    }

    if (matchKeyword(TokenType::KW_TRIGGER)) {
        auto* stmt = arena()->create<parser::v3::DropTriggerStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        std::string name = parseIdentifier();
        stmt->triggers.push_back(buildPathFromQualified(string_pool_, name));
        if (matchKeyword(TokenType::KW_ON)) {
            parseQualifiedName();
        }
        return stmt;
    }

    if (matchKeyword(TokenType::KW_FUNCTION)) {
        auto* stmt = arena()->create<parser::v3::DropFunctionStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->functions.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        return stmt;
    }

    if (matchKeyword(TokenType::KW_PROCEDURE)) {
        auto* stmt = arena()->create<parser::v3::DropProcedureStmt>();
        if (matchKeyword(TokenType::KW_IF)) {
            consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
            stmt->if_exists = true;
        }
        do {
            std::string name = parseQualifiedName();
            stmt->procedures.push_back(buildPathFromQualified(string_pool_, name));
        } while (match(TokenType::COMMA));
        return stmt;
    }

    return nullptr;
}

parser::v3::TruncateTableStmt* Parser::parseTruncateStmtV3() {
    auto* stmt = arena()->create<parser::v3::TruncateTableStmt>();
    matchKeyword(TokenType::KW_TABLE);
    do {
        std::string schema;
        std::string table = parseIdentifier();
        if (match(TokenType::DOT)) {
            schema = table;
            table = parseIdentifier();
        }
        resolveTableName(schema, table);
        std::string full = schema.empty() ? table : schema + "." + table;
        stmt->tables.push_back(buildPathFromQualified(string_pool_, full));
    } while (match(TokenType::COMMA));

    if (matchKeyword(TokenType::KW_RESTART)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        stmt->restart_identity = true;
    } else if (matchKeyword(TokenType::KW_CONTINUE)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        stmt->continue_identity = true;
    }

    if (matchKeyword(TokenType::KW_CASCADE)) {
        stmt->cascade = true;
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
        stmt->cascade = false;
    }

    return stmt;
}

// ============================================================================
// CREATE Statement Dispatch
// ============================================================================

void Parser::parseCreateStmt() {
    // Handle OR REPLACE
    bool or_replace = pending_or_replace_;
    if (!pending_or_replace_ && matchKeyword(TokenType::KW_OR)) {
        consumeKeyword(TokenType::KW_REPLACE, "Expected REPLACE");
        or_replace = true;
    }
    pending_or_replace_ = or_replace;

    // Handle TEMP/TEMPORARY
    bool is_temp = pending_create_temp_;
    if (!pending_create_temp_) {
        is_temp = matchKeyword(TokenType::KW_TEMP) || matchKeyword(TokenType::KW_TEMPORARY);
    }

    // Handle UNLOGGED
    bool is_unlogged = pending_create_unlogged_;
    if (!pending_create_unlogged_) {
        is_unlogged = matchKeyword(TokenType::KW_UNLOGGED);
    }

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
    } else if (matchKeyword(TokenType::KW_TABLESPACE)) {
        parseCreateTablespace();
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

    if (!check(TokenType::RIGHT_PAREN)) {
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
                // C2: Table-level CHECK constraint
                consume(TokenType::LEFT_PAREN, "Expected (");
                std::string check_expr = parseExpressionText();
                consume(TokenType::RIGHT_PAREN, "Expected )");
                
                // C2: Table-level CHECK constraint - use standard CHECK_CONSTRAINT opcode
                emit(sblr::Opcode::CHECK_CONSTRAINT);
                emitString(constraint_name);
                emitString(check_expr);
                emitted_entry = true;
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
    }

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

void Parser::parseCreateTablespace() {
    std::string tablespace_name = parseIdentifier();

    if (matchKeyword(TokenType::KW_OWNER)) {
        parseIdentifier();  // Owner ignored for emulation
    }

    consumeKeyword(TokenType::KW_LOCATION, "Expected LOCATION for CREATE TABLESPACE");
    if (!check(TokenType::STRING_LITERAL)) {
        error("Expected string literal for tablespace LOCATION");
        synchronize();
        return;
    }
    std::string location(lexer_.stringPool().get(current_token_.value.string_id));
    advance();

    if (matchKeyword(TokenType::KW_WITH)) {
        if (match(TokenType::LEFT_PAREN)) {
            while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
                advance();
            }
            if (check(TokenType::RIGHT_PAREN)) {
                advance();
            }
        }
    }

    emit(sblr::Opcode::CREATE_TABLESPACE);
    emitString(tablespace_name);
    emitString(location);
    emitByte(0);   // autoextend disabled
    emitU32(0);    // autoextend size
    emitU32(0);    // max size
    emitU32(0);    // prealloc
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
    } else if (matchKeyword(TokenType::KW_OID) || matchIdentifierKeyword("OID")) {
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
    } else if (matchKeyword(TokenType::KW_NAME) || matchIdentifierKeyword("NAME")) {
        // PostgreSQL internal NAME type (catalog identifier), surfaced as bounded VARCHAR.
        type.kind = PgDataType::Kind::VARCHAR;
        type.length = 63;
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
    } else if (matchKeyword(TokenType::KW_POINT) || matchIdentifierKeyword("POINT")) {
        type.kind = PgDataType::Kind::POINT;
    } else if (matchKeyword(TokenType::KW_LINE)) {
        type.kind = PgDataType::Kind::LINE;
    } else if (matchKeyword(TokenType::KW_LSEG)) {
        type.kind = PgDataType::Kind::LSEG;
    } else if (matchKeyword(TokenType::KW_BOX)) {
        type.kind = PgDataType::Kind::BOX;
    } else if (matchKeyword(TokenType::KW_PATH) || matchIdentifierKeyword("PATH")) {
        // PostgreSQL PATH compatibility currently degrades to TEXT in v3.
        type.kind = PgDataType::Kind::TEXT;
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
    uint8_t index_type = static_cast<uint8_t>(core::CatalogManager::IndexType::BTREE);
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
        } else {
            error("PostgreSQL index method must be btree, hash, gin, gist, spgist, or brin");
        }
    }

    if (unique &&
        index_type == static_cast<uint8_t>(core::CatalogManager::IndexType::HASH)) {
        error("CREATE UNIQUE INDEX USING hash is not supported in PostgreSQL dialect");
    }

    // Column/expression list
    consume(TokenType::LEFT_PAREN, "Expected (");
    std::vector<std::string> columns;
    std::vector<std::vector<uint8_t>> expression_list;
    bool all_simple_columns = true;

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

    do {
        std::vector<uint8_t> expr_bytes = captureExpressionBytecode();
        std::string column_name;
        if (decode_simple_column(expr_bytes, column_name)) {
            columns.push_back(column_name);
        } else {
            all_simple_columns = false;
        }
        expression_list.push_back(std::move(expr_bytes));

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

    bool emit_expressions = !all_simple_columns;
    std::vector<uint8_t> expression_data;
    if (emit_expressions) {
        auto write_u32 = [&](uint32_t value) {
            expression_data.push_back(static_cast<uint8_t>(value & 0xFF));
            expression_data.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            expression_data.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            expression_data.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        };
        expression_data.reserve(8);
        expression_data.push_back('S');
        expression_data.push_back('B');
        expression_data.push_back('L');
        expression_data.push_back('R');
        write_u32(static_cast<uint32_t>(expression_list.size()));
        for (const auto& expr : expression_list) {
            write_u32(static_cast<uint32_t>(expr.size()));
            expression_data.insert(expression_data.end(), expr.begin(), expr.end());
        }
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
        emitU32(static_cast<uint32_t>(expression_data.size()));
        if (emit_enabled_) {
            bytecode_.insert(bytecode_.end(),
                             expression_data.begin(),
                             expression_data.end());
        }
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
                options.emplace_back("tablespace", value);
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

    // Dependency extraction is disabled until V3 dependency graph is emitted directly.
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

    // Dependency extraction is disabled until V3 dependency graph is emitted directly.
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
        PgDataType subtype;
        subtype.kind = PgDataType::Kind::INTEGER;
        std::string subtype_collation;
        std::string subtype_opclass;
        std::string canonical_func;
        std::string subtype_diff_func;
        bool multirange = false;

        if (match(TokenType::LEFT_PAREN)) {
            do {
                std::string opt = parseIdentifier();
                std::string opt_lower = opt;
                std::transform(opt_lower.begin(), opt_lower.end(), opt_lower.begin(), ::tolower);
                if (match(TokenType::EQUAL)) {
                    // consume '='
                }
                if (opt_lower == "subtype") {
                    subtype = parseDataType();
                } else if (opt_lower == "subtype_opclass") {
                    subtype_opclass = parseQualifiedName();
                } else if (opt_lower == "collation") {
                    subtype_collation = parseQualifiedName();
                } else if (opt_lower == "canonical") {
                    canonical_func = parseQualifiedName();
                } else if (opt_lower == "subtype_diff") {
                    subtype_diff_func = parseQualifiedName();
                } else if (opt_lower == "multirange_type_name") {
                    parseQualifiedName();
                    multirange = true;
                } else {
                    // Unknown option - consume an identifier or data type to move on
                    if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                        parseIdentifier();
                    } else {
                        parseExpression();
                    }
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }

        emit_domain_header(static_cast<uint8_t>(parser::DomainKind::RANGE));
        emit_type_ref(subtype);
        emitString(subtype_collation);
        emitString(subtype_opclass);
        emitString(canonical_func);
        emitString(subtype_diff_func);
        emitByte(multirange ? 1 : 0);

        emitByte(1);  // nullable
        emitString("");  // default
        emitString("");  // collation
        emitU32(0);  // constraints
        emitByte(0);  // no inherits
        emitString("postgresql");
        emitString("");
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
    // C2: Expanded CREATE DOMAIN base type support
    bool supported_base_type = true;
    switch (base_type.kind) {
        // Numeric types
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
        // Character types
        case PgDataType::Kind::CHAR:
        case PgDataType::Kind::VARCHAR:
        case PgDataType::Kind::TEXT:
        // Binary type
        case PgDataType::Kind::BYTEA:
        // Date/Time types
        case PgDataType::Kind::DATE:
        case PgDataType::Kind::TIME:
        case PgDataType::Kind::TIMETZ:
        case PgDataType::Kind::TIMESTAMP:
        case PgDataType::Kind::TIMESTAMPTZ:
        case PgDataType::Kind::INTERVAL:
        // Boolean
        case PgDataType::Kind::BOOLEAN:
        // UUID
        case PgDataType::Kind::UUID:
        // JSON types
        case PgDataType::Kind::JSON:
        case PgDataType::Kind::JSONB:
        case PgDataType::Kind::JSONPATH:
        // XML
        case PgDataType::Kind::XML:
        // Network types
        case PgDataType::Kind::INET:
        case PgDataType::Kind::CIDR:
        case PgDataType::Kind::MACADDR:
        case PgDataType::Kind::MACADDR8:
        // Text search types
        case PgDataType::Kind::TSVECTOR:
        case PgDataType::Kind::TSQUERY:
        // Bit string types
        case PgDataType::Kind::BIT:
        case PgDataType::Kind::VARBIT:
            break;
        // Array types are supported as base types for domains
        case PgDataType::Kind::ARRAY:
            // C2: Array domains support
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
    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        parseAlterTablespace();
        return;
    }

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
        std::string grantor_name;
        if (matchKeyword(TokenType::KW_FOR)) {
            if (matchKeyword(TokenType::KW_ROLE) || matchKeyword(TokenType::KW_USER)) {
                grantor_name = parseIdentifier();
            } else {
                grantor_name = parseIdentifier();
            }
        }
        std::vector<std::string> schema_names;
        if (matchKeyword(TokenType::KW_IN)) {
            consumeKeyword(TokenType::KW_SCHEMA, "Expected SCHEMA after IN");
            do {
                schema_names.push_back(parseQualifiedName());
            } while (match(TokenType::COMMA));
        }
        if (schema_names.empty()) {
            schema_names.push_back(default_schema_.empty() ? "public" : default_schema_);
        }

        auto parse_privilege_list = [&]() {
            uint32_t privileges = 0;
            if (matchKeyword(TokenType::KW_ALL)) {
                matchKeyword(TokenType::KW_PRIVILEGES);
                privileges = static_cast<uint32_t>(core::CatalogManager::Privilege::ALL);
                return privileges;
            }
            do {
                if (check(TokenType::IDENTIFIER) || check(TokenType::QUOTED_IDENTIFIER)) {
                    std::string priv = parseIdentifier();
                    std::string lower = priv;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == "select") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::SELECT);
                    else if (lower == "insert") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::INSERT);
                    else if (lower == "update") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::UPDATE);
                    else if (lower == "delete") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::DELETE);
                    else if (lower == "truncate") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::TRUNCATE);
                    else if (lower == "references") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::REFERENCES);
                    else if (lower == "trigger") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::TRIGGER);
                    else if (lower == "execute") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::EXECUTE);
                    else if (lower == "usage") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::USAGE);
                    else if (lower == "create") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::CREATE);
                    else if (lower == "connect") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::CONNECT);
                    else if (lower == "temporary") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::TEMPORARY);
                    else if (lower == "copy") privileges |= static_cast<uint32_t>(core::CatalogManager::Privilege::COPY_FILE);
                    else error("Unsupported privilege in ALTER DEFAULT PRIVILEGES");
                } else {
                    advance();
                }
            } while (match(TokenType::COMMA));
            return privileges;
        };

        auto parse_object_type = [&]() {
            if (matchKeyword(TokenType::KW_TABLES)) {
                return static_cast<uint8_t>(core::CatalogManager::PermissionObjectType::TABLE);
            }
            if (matchKeyword(TokenType::KW_SEQUENCES)) {
                return static_cast<uint8_t>(core::CatalogManager::PermissionObjectType::SEQUENCE);
            }
            if (matchKeyword(TokenType::KW_FUNCTIONS)) {
                return static_cast<uint8_t>(core::CatalogManager::PermissionObjectType::FUNCTION);
            }
            if (matchKeyword(TokenType::KW_TYPES)) {
                return static_cast<uint8_t>(core::CatalogManager::PermissionObjectType::DOMAIN);
            }
            if (matchKeyword(TokenType::KW_SCHEMAS)) {
                return static_cast<uint8_t>(core::CatalogManager::PermissionObjectType::SCHEMA);
            }
            parseIdentifier();
            return static_cast<uint8_t>(core::CatalogManager::PermissionObjectType::TABLE);
        };

        auto parse_grantees = [&]() {
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
                error("ALTER DEFAULT PRIVILEGES supports a single grantee in current bytecode");
                parseIdentifier();
            }
            return std::make_pair(grantee_type, grantee_name);
        };

        if (matchKeyword(TokenType::KW_GRANT)) {
            uint32_t privileges = parse_privilege_list();
            consumeKeyword(TokenType::KW_ON, "Expected ON");
            uint8_t object_type = parse_object_type();
            consumeKeyword(TokenType::KW_TO, "Expected TO");
            auto grantee = parse_grantees();
            bool with_grant_option = false;
            if (matchKeyword(TokenType::KW_WITH)) {
                consumeKeyword(TokenType::KW_GRANT, "Expected GRANT");
                consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
                with_grant_option = true;
            }
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DEFAULT_PRIVILEGES));
            emitByte(1);
            emitString(grantor_name);
            emitByte(object_type);
            emitU32(privileges);
            emitByte(static_cast<uint8_t>(grantee.first));
            emitString(grantee.second);
            emitByte(with_grant_option ? 1 : 0);
            emitU32(static_cast<uint32_t>(schema_names.size()));
            for (const auto& schema : schema_names) {
                emitString(schema);
            }
            return;
        }
        if (matchKeyword(TokenType::KW_REVOKE)) {
            bool grant_option_for = false;
            if (matchKeyword(TokenType::KW_GRANT)) {
                consumeKeyword(TokenType::KW_OPTION, "Expected OPTION");
                consumeKeyword(TokenType::KW_FOR, "Expected FOR");
                grant_option_for = true;
            }
            uint32_t privileges = parse_privilege_list();
            consumeKeyword(TokenType::KW_ON, "Expected ON");
            uint8_t object_type = parse_object_type();
            consumeKeyword(TokenType::KW_FROM, "Expected FROM");
            auto grantee = parse_grantees();
            if (matchKeyword(TokenType::KW_CASCADE)) {
                // optional
            } else {
                matchKeyword(TokenType::KW_RESTRICT);
            }
            emit(sblr::Opcode::EXTENDED_OPCODE);
            emitU16(static_cast<uint16_t>(sblr::ExtendedOpcode::EXT_ALTER_DEFAULT_PRIVILEGES));
            emitByte(2);
            emitString(grantor_name);
            emitByte(object_type);
            emitU32(privileges);
            emitByte(static_cast<uint8_t>(grantee.first));
            emitString(grantee.second);
            emitByte(grant_option_for ? 1 : 0);
            emitU32(static_cast<uint32_t>(schema_names.size()));
            for (const auto& schema : schema_names) {
                emitString(schema);
            }
            return;
        }

        error("Expected GRANT or REVOKE in ALTER DEFAULT PRIVILEGES");
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
                // C2: Support constraints and domain/array types in ADD COLUMN
                emit_alter(0, [&]() {
                    emitString(col.name);
                    emitTypeDefinition(col.type);
                    emitByte(col.not_null ? 1 : 0);
                    emitByte(col.has_default ? 1 : 0);
                    if (col.has_default) {
                        emitString(col.default_value);
                    }
                    emitByte(col.primary_key ? 1 : 0);
                    emitByte(col.unique ? 1 : 0);
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
                    // C2: ALTER TABLE DROP CONSTRAINT
                    bool if_exists = false;
                    if (matchKeyword(TokenType::KW_IF)) {
                        consumeKeyword(TokenType::KW_EXISTS, "Expected EXISTS");
                        if_exists = true;
                    }
                    std::string constraint_name = parseIdentifier();
                    bool cascade = false;
                    if (matchKeyword(TokenType::KW_CASCADE)) {
                        cascade = true;
                    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
                        cascade = false;
                    }
                    emit_alter(4, [&]() {
                        emitString(constraint_name);
                        emitByte(if_exists ? 1 : 0);
                        emitByte(cascade ? 1 : 0);
                    });
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
                    } else if (matchKeyword(TokenType::KW_DEFAULT)) {
                        // C2: ALTER TABLE ALTER COLUMN SET DEFAULT
                        std::string default_expr = parseExpressionText();
                        emit_alter(5, [&]() {
                            emitString(col_name);
                            emitByte(1);  // has_default = true
                            emitString(default_expr);
                        });
                        return;
                    } else if (matchKeyword(TokenType::KW_NOT)) {
                        consumeKeyword(TokenType::KW_NULL, "Expected NULL");
                        // C2: ALTER TABLE ALTER COLUMN SET NOT NULL
                        emit_alter(6, [&]() {
                            emitString(col_name);
                            emitByte(1);  // not_null = true
                        });
                        return;
                    }
                } else if (matchKeyword(TokenType::KW_DROP)) {
                    if (matchKeyword(TokenType::KW_DEFAULT)) {
                        // C2: ALTER TABLE ALTER COLUMN DROP DEFAULT
                        emit_alter(5, [&]() {
                            emitString(col_name);
                            emitByte(0);  // has_default = false
                            emitString("");  // empty default expression
                        });
                        return;
                    } else if (matchKeyword(TokenType::KW_NOT)) {
                        consumeKeyword(TokenType::KW_NULL, "Expected NULL");
                        // C2: ALTER TABLE ALTER COLUMN DROP NOT NULL
                        emit_alter(6, [&]() {
                            emitString(col_name);
                            emitByte(0);  // not_null = false
                        });
                        return;
                    }
                }

                if (alter_type) {
                    PgDataType type = parseDataType();
                    // C2: Support domain and array types in ALTER COLUMN
                    std::string using_expr;
                    if (matchKeyword(TokenType::KW_USING)) {
                        // C2: ALTER TABLE ALTER COLUMN ... USING
                        using_expr = parseExpressionText();
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
                        emitByte(using_expr.empty() ? 0 : 1);
                        if (!using_expr.empty()) {
                            emitString(using_expr);
                        }
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

void Parser::parseAlterTablespace() {
    std::string tablespace_name = parseIdentifier();

    std::vector<parser::v3::TablespaceAlterAction> actions;
    std::vector<uint32_t> size_values;
    std::vector<std::string> rename_values;
    std::vector<uint8_t> bool_values;

    if (matchKeyword(TokenType::KW_RENAME)) {
        consumeKeyword(TokenType::KW_TO, "Expected TO after RENAME");
        std::string new_name = parseIdentifier();
        actions.push_back(parser::v3::TablespaceAlterAction::RENAME_TO);
        rename_values.push_back(new_name);
    } else {
        bool is_reset = false;
        bool set_reset = false;
        if (matchKeyword(TokenType::KW_SET)) {
            is_reset = false;
            set_reset = true;
        } else if (matchKeyword(TokenType::KW_RESET)) {
            is_reset = true;
            set_reset = true;
        }
        if (!set_reset) {
            error("Expected RENAME or SET/RESET for ALTER TABLESPACE");
            synchronize();
            return;
        }
        if (match(TokenType::LEFT_PAREN)) {
            do {
                std::string option = parseIdentifier();
                std::string lower = option;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                               [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                if (match(TokenType::EQUAL)) {
                    if (lower == "autoextend") {
                        uint8_t enabled = 1;
                        if (matchKeyword(TokenType::KW_OFF) || matchKeyword(TokenType::KW_FALSE)) {
                            enabled = 0;
                        } else if (matchKeyword(TokenType::KW_ON) || matchKeyword(TokenType::KW_TRUE)) {
                            enabled = 1;
                        }
                        actions.push_back(parser::v3::TablespaceAlterAction::SET_AUTOEXTEND);
                        bool_values.push_back(enabled);
                    } else if (lower == "autoextend_size" || lower == "autoextend_size_mb") {
                        if (!check(TokenType::INTEGER_LITERAL)) {
                            error("Expected integer for autoextend_size");
                        } else {
                            actions.push_back(parser::v3::TablespaceAlterAction::SET_AUTOEXTEND_SIZE);
                            size_values.push_back(static_cast<uint32_t>(current_token_.value.int_value));
                            advance();
                        }
                    } else if (lower == "maxsize") {
                        if (check(TokenType::INTEGER_LITERAL)) {
                            actions.push_back(parser::v3::TablespaceAlterAction::SET_MAXSIZE);
                            size_values.push_back(static_cast<uint32_t>(current_token_.value.int_value));
                            advance();
                        } else if (matchKeyword(TokenType::KW_UNLIMITED)) {
                            actions.push_back(parser::v3::TablespaceAlterAction::SET_MAXSIZE);
                            size_values.push_back(0);
                        } else {
                            error("Expected integer or UNLIMITED for maxsize");
                        }
                    }
                } else if (is_reset && lower == "autoextend") {
                    actions.push_back(parser::v3::TablespaceAlterAction::SET_AUTOEXTEND);
                    bool_values.push_back(0);
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    }

    emit(sblr::Opcode::ALTER_TABLESPACE);
    emitString(tablespace_name);
    emitU32(static_cast<uint32_t>(actions.size()));

    size_t size_idx = 0;
    size_t rename_idx = 0;
    size_t bool_idx = 0;
    for (const auto& action : actions) {
        emitByte(static_cast<uint8_t>(action));
        switch (action) {
            case parser::v3::TablespaceAlterAction::SET_AUTOEXTEND:
                emitByte(bool_values[bool_idx++]);
                break;
            case parser::v3::TablespaceAlterAction::SET_AUTOEXTEND_SIZE:
            case parser::v3::TablespaceAlterAction::SET_MAXSIZE:
                emitU32(size_values[size_idx++]);
                break;
            case parser::v3::TablespaceAlterAction::RENAME_TO:
                emitString(rename_values[rename_idx++]);
                break;
            default:
                emitByte(0);
                break;
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

    bool force = false;
    if (matchKeyword(TokenType::KW_WITH)) {
        if (match(TokenType::LEFT_PAREN)) {
            do {
                if (matchKeyword(TokenType::KW_FORCE)) {
                    force = true;
                } else {
                    parseIdentifier();
                }
            } while (match(TokenType::COMMA));
            consume(TokenType::RIGHT_PAREN, "Expected )");
        }
    }
    (void)if_exists;

    emit(sblr::Opcode::DROP_TABLESPACE);
    emitString(tablespace_name);
    emitByte(force ? 1 : 0);
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
    if (matchKeyword(TokenType::KW_DOMAIN)) {
        parseDropDomain();
        return;
    }

    if (matchKeyword(TokenType::KW_TABLESPACE)) {
        parseDropTablespace();
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

    // C2: TRUNCATE options - RESTART IDENTITY / CONTINUE IDENTITY
    bool restart_identity = false;
    if (matchKeyword(TokenType::KW_RESTART)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        restart_identity = true;
    } else if (matchKeyword(TokenType::KW_CONTINUE)) {
        consumeKeyword(TokenType::KW_IDENTITY, "Expected IDENTITY");
        restart_identity = false;
    }

    // C2: CASCADE/RESTRICT
    bool cascade = false;
    if (matchKeyword(TokenType::KW_CASCADE)) {
        cascade = true;
    } else if (matchKeyword(TokenType::KW_RESTRICT)) {
        cascade = false;
    }

    for (const auto& table_name : tables) {
        emit(sblr::Opcode::TRUNCATE_TABLE);
        emitString(table_name);
        emitByte(0);  // ASYNC mode
        emitByte(restart_identity ? 1 : 0);  // restart_identity
        emitByte(cascade ? 1 : 0);  // cascade
    }
}

} // namespace scratchbird::parser::postgresql
