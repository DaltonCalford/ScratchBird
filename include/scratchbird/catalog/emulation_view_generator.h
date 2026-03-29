/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

/**
 * On-Demand Emulation View Generator
 *
 * Phase D: Catalog Cleanup - Dynamic emulation view creation
 *
 * This module generates protocol-specific system views when an emulated
 * database connection is established. Emulated views are NOT pre-created;
 * they are generated dynamically when:
 * 1. CREATE EMULATED SERVER - creates server schema
 * 2. CONNECT TO {server} DATABASE {db} - creates database schema + views
 *
 * The generated views query the ScratchBird internal catalog and transform
 * results to match the emulated format.
 *
 * Supported Emulations:
 * - Firebird (RDB$RELATIONS, RDB$FIELDS, RDB$INDICES, etc.)
 * - PostgreSQL (pg_catalog.pg_class, pg_attribute, etc.)
 * - MySQL (mysql.*, information_schema)
 * - SQL Server (sys.*, INFORMATION_SCHEMA)
 *
 * Created: November 26, 2025
 * Phase: Catalog Cleanup Phase D
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * EmulatedViewDefinition - Template for generating emulated system views
 *
 * Each view definition specifies:
 * - view_name: The name as it appears in the emulated catalog (e.g., "RDB$RELATIONS")
 * - source_query: SQL template that maps to internal catalog
 * - columns: Column names in the emulated view
 * - column_types: Data types for each column
 * - description: Documentation for the view
 *
 * Note: The source_query uses placeholders:
 * - {schema_id} - ID of the emulated database schema
 * - {server_name} - Name of the emulation server
 * - {database_name} - Name of the emulated database
 */

// EmulatedViewDefinition is defined in virtual_catalog.h

/**
 * EmulationViewGenerator - Generates protocol-specific emulation views
 *
 * This class is responsible for creating the view definitions and
 * registering them when an emulated database is connected.
 *
 * Usage:
 * 1. When CREATE EMULATED SERVER is executed:
 *    - Call generateServerSchema() to create /remote/emulated/{type}/{server}/
 *
 * 2. When CONNECT TO {server} DATABASE {db} is executed:
 *    - Call generateEmulatedViews() to create views in the database schema
 *
 * 3. When disconnecting or dropping:
 *    - Call dropEmulatedViews() to remove the views
 */
class EmulationViewGenerator {
public:
    /**
     * Constructor
     *
     * @param catalog CatalogManager instance
     */
    explicit EmulationViewGenerator(CatalogManager* catalog)
        : catalog_(catalog) {}

    /**
     * Generate server schema for a new emulation server
     *
     * Creates the schema path /emulation/{protocol}/{server_name}/
     *
     * @param server_name Name of the emulation server
     * @param protocol Protocol type being emulated
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status generateServerSchema(const std::string& server_name,
                                ProtocolType protocol,
                                ErrorContext* ctx = nullptr) {
        // Build schema path: emulation.{protocol}.{server}
        std::string protocolName = protocolTypeToString(protocol);
        std::string schemaPath = "emulation." + protocolName + "." + server_name;
        ID schema_id;
        return catalog_->createSchemaPath(schemaPath, CatalogManager::SchemaType::REMOTE_EMULATED,
                                          schema_id, ctx);
    }

    /**
     * Generate all emulated views for a database
     *
     * Creates the database schema and all protocol-specific system views
     * as virtual tables.
     *
     * @param schema_path Full emulated database schema path
     * @param server_name Name of the emulation server
     * @param database_name Name of the database to emulate
     * @param protocol Protocol type being emulated
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status generateEmulatedViews(const std::string& schema_path,
                                 const std::string& server_name,
                                 const std::string& database_name,
                                 ProtocolType protocol,
                                 ErrorContext* ctx = nullptr) {
        std::string schemaPath = schema_path;

        ID schema_id;
        Status s = catalog_->createSchemaPath(schemaPath, CatalogManager::SchemaType::REMOTE_EMULATED,
                                              schema_id, ctx);
        if (s != Status::OK) return s;

        // Get view definitions for this protocol
        std::vector<EmulatedViewDefinition> views = getViewDefinitions(protocol);

        // Create each view
        Status first_error = Status::OK;
        for (const auto& viewDef : views) {
            s = createEmulatedView(schema_id,
                                   schemaPath,
                                   protocol,
                                   viewDef,
                                   server_name,
                                   database_name,
                                   ctx);
            if (s != Status::OK && first_error == Status::OK) {
                first_error = s;
                // Log error but continue with other views
                // In production, might want to rollback all
            }
        }

        std::vector<CatalogManager::TableInfo> schema_tables;
        s = catalog_->listTables(schema_id, schema_tables, ctx);
        if (s == Status::OK) {
            ID public_grantee{};
            ID system_grantor{};
            for (const auto& table : schema_tables) {
                Status grant_status = catalog_->grantPermission(
                    table.table_id,
                    CatalogManager::PermissionObjectType::TABLE,
                    public_grantee,
                    CatalogManager::GranteeType::PUBLIC,
                    CatalogManager::PERM_SELECT,
                    false,
                    system_grantor,
                    ctx);
                if (grant_status != Status::OK && first_error == Status::OK) {
                    first_error = grant_status;
                }
            }
        } else if (first_error == Status::OK) {
            first_error = s;
        }

        return first_error;
    }

    /**
     * Drop all emulated views for a database
     *
     * @param schema_path Full emulated database schema path
     * @param server_name Name of the emulation server
     * @param database_name Name of the database
     * @param protocol Protocol type
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status dropEmulatedViews(const std::string& schema_path,
                             const std::string& server_name,
                             const std::string& database_name,
                             ProtocolType protocol,
                             ErrorContext* ctx = nullptr) {
        std::string schemaPath = schema_path;

        // Get schema info
        CatalogManager::SchemaInfo schemaInfo;
        Status s = catalog_->getSchema(schemaPath, schemaInfo, ctx);
        if (s != Status::OK) {
            // Schema doesn't exist, nothing to drop
            return Status::OK;
        }

        // Drop all views in the schema
        std::vector<CatalogManager::ViewInfo> views;
        s = catalog_->listViewsForSchema(schemaInfo.schema_id, views, ctx);
        if (s == Status::OK) {
            for (const auto& view : views) {
                catalog_->dropView(view.view_id, true, ctx);
            }
        }

        // Drop the schema itself
        return catalog_->dropSchema(schemaInfo.schema_id, true, ctx);
    }

    /**
     * Get view definitions for a protocol
     *
     * @param protocol Protocol type
     * @return Vector of view definitions
     */
    std::vector<EmulatedViewDefinition> getViewDefinitions(ProtocolType protocol) {
        switch (protocol) {
            case ProtocolType::FIREBIRD:
                return getFirebirdViews();
            case ProtocolType::POSTGRESQL:
                return getPostgreSQLViews();
            case ProtocolType::MYSQL:
                return getMySQLViews();
            case ProtocolType::MSSQL:
                return getMSSQLViews();
            default:
                return {};
        }
    }

private:
    CatalogManager* catalog_;

    static std::string compilerProfileIdForProtocol(ProtocolType protocol) {
        switch (protocol) {
            case ProtocolType::FIREBIRD:
                return "firebirdsql";
            case ProtocolType::POSTGRESQL:
                return "postgresql";
            case ProtocolType::MYSQL:
                return "mysql";
            default:
                return {};
        }
    }

    static std::string compilerDialectTagForProtocol(ProtocolType protocol) {
        switch (protocol) {
            case ProtocolType::FIREBIRD:
                return "firebird";
            case ProtocolType::POSTGRESQL:
                return "postgresql";
            case ProtocolType::MYSQL:
                return "mysql";
            default:
                return {};
        }
    }

    static std::string compilerModuleNameForProtocol(ProtocolType protocol) {
        std::string protocol_name = protocolTypeToString(protocol);
        if (protocol_name.empty()) {
            return {};
        }
        return protocol_name + "_emulation";
    }

    Status compileEmulatedViewQuery(const ID& schema_id,
                                    const std::string& schema_path,
                                    ProtocolType protocol,
                                    const std::string& query,
                                    std::vector<uint8_t>& bytecode_out,
                                    ErrorContext* ctx);

    /**
     * Create a single emulated view
     */
    Status createEmulatedView(const ID& schema_id,
                              const std::string& schema_path,
                              ProtocolType protocol,
                              const EmulatedViewDefinition& viewDef,
                              const std::string& server_name,
                              const std::string& database_name,
                              ErrorContext* ctx) {
        auto decorateError = [&](Status status, const char* stage) {
            if (ctx != nullptr) {
                std::string message = "Emulated view '" + viewDef.view_name + "' failed during ";
                message += stage;
                if (!ctx->message.empty()) {
                    message += ": " + ctx->message;
                }
                ctx->set(status, message.c_str(), __FILE__, __LINE__, __func__);
            }
            return status;
        };

        std::string query =
            renderViewQuery(viewDef, schema_id, schema_path, server_name, database_name);
        std::vector<uint8_t> compiled_query_sblr;
        Status s = compileEmulatedViewQuery(schema_id,
                                            schema_path,
                                            protocol,
                                            query,
                                            compiled_query_sblr,
                                            ctx);
        if (s != Status::OK) {
            return decorateError(s, "compile");
        }
        s = catalog_->createView(schema_id, viewDef.view_name, query,
                                 true, false, false, viewDef.columns, ID{}, ctx);
        if (s != Status::OK) {
            return decorateError(s, "create");
        }

        CatalogManager::ViewInfo view_info;
        s = catalog_->getView(schema_id, viewDef.view_name, view_info, ctx);
        if (s != Status::OK) {
            return decorateError(s, "lookup");
        }

        const std::vector<CatalogManager::ViewColumnBinding> empty_insert_bindings;
        s = catalog_->setViewExecutionMetadata(view_info.view_id,
                                               compiled_query_sblr,
                                               empty_insert_bindings,
                                               "scratchbird_v3",
                                               {},
                                               ctx);
        if (s != Status::OK) {
            return decorateError(s, "metadata persist");
        }

        ID public_grantee{};
        ID system_grantor{};
        s = catalog_->grantPermission(view_info.view_id,
                                      CatalogManager::PermissionObjectType::VIEW,
                                      public_grantee,
                                      CatalogManager::GranteeType::PUBLIC,
                                      CatalogManager::PERM_SELECT,
                                      false,
                                      system_grantor,
                                      ctx);
        if (s != Status::OK) {
            return decorateError(s, "permission grant");
        }

        return Status::OK;
    }

    static void replaceAll(std::string& text, const std::string& token,
                           const std::string& replacement) {
        if (token.empty()) {
            return;
        }
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) {
            text.replace(pos, token.size(), replacement);
            pos += replacement.size();
        }
    }

    static std::string renderViewQuery(const EmulatedViewDefinition& viewDef,
                                       const ID& schema_id,
                                       const std::string& schema_path,
                                       const std::string& server_name,
                                       const std::string& database_name) {
        auto quoteSqlLiteral = [](std::string_view text) {
            std::string out;
            out.reserve(text.size() + 2);
            out.push_back('\'');
            for (char ch : text) {
                out.push_back(ch);
                if (ch == '\'') {
                    out.push_back('\'');
                }
            }
            out.push_back('\'');
            return out;
        };

        std::string query = viewDef.source_query;
        std::string schema_literal = "UUID '" + schema_id.toString() + "'";
        replaceAll(query, "{schema_id}", schema_literal);
        replaceAll(query, "{schema_path}", quoteSqlLiteral(schema_path));
        replaceAll(query, "{server_name}", server_name);
        replaceAll(query, "{database_name}", database_name);
        return query;
    }

    enum class FirebirdSeedFieldKind {
        TEXT,
        SMALLINT,
        INTEGER,
        BIGINT,
        BOOLEAN
    };

    struct FirebirdSeedRelationSpec {
        const char* relation_name;
        int relation_id;
        int relation_type;
        const char* view_source;
    };

    struct FirebirdSeedRelationFieldSpec {
        const char* relation_name;
        const char* field_name;
        FirebirdSeedFieldKind kind;
        int field_length;
        int field_scale;
        bool not_nullable;
        bool has_charset;
        int charset_id;
        bool has_collation;
        int collation_id;
        bool has_dimensions;
        int dimensions;
    };

    static std::string quoteSqlLiteral(std::string_view text) {
        std::string out;
        out.reserve(text.size() + 2);
        out.push_back('\'');
        for (char ch : text) {
            out.push_back(ch);
            if (ch == '\'') {
                out.push_back('\'');
            }
        }
        out.push_back('\'');
        return out;
    }

    static std::string castSqlExpr(std::string_view expr, std::string_view sql_type) {
        std::string out;
        out.reserve(expr.size() + sql_type.size() + 16);
        out.append("CAST(");
        out.append(expr);
        out.append(" AS ");
        out.append(sql_type);
        out.push_back(')');
        return out;
    }

    static int firebirdSeedFieldTypeCode(FirebirdSeedFieldKind kind) {
        switch (kind) {
            case FirebirdSeedFieldKind::TEXT:
                return 37;
            case FirebirdSeedFieldKind::SMALLINT:
                return 7;
            case FirebirdSeedFieldKind::INTEGER:
                return 8;
            case FirebirdSeedFieldKind::BIGINT:
                return 16;
            case FirebirdSeedFieldKind::BOOLEAN:
                return 23;
        }
        return 37;
    }

    static int firebirdSeedFieldLogicalLength(FirebirdSeedFieldKind kind, int declared_length) {
        switch (kind) {
            case FirebirdSeedFieldKind::TEXT:
                return declared_length;
            case FirebirdSeedFieldKind::SMALLINT:
                return 2;
            case FirebirdSeedFieldKind::INTEGER:
                return 4;
            case FirebirdSeedFieldKind::BIGINT:
                return 8;
            case FirebirdSeedFieldKind::BOOLEAN:
                return 1;
        }
        return declared_length;
    }

    static std::string quoteFirebirdAliasIdentifiers(std::string sql) {
        std::string out;
        out.reserve(sql.size() + 64);

        bool in_string = false;
        size_t i = 0;
        while (i < sql.size()) {
            const char ch = sql[i];
            if (ch == '\'') {
                out.push_back(ch);
                if (in_string && i + 1 < sql.size() && sql[i + 1] == '\'') {
                    out.push_back(sql[i + 1]);
                    i += 2;
                    continue;
                }
                in_string = !in_string;
                ++i;
                continue;
            }

            if (!in_string &&
                i + 2 < sql.size() &&
                std::toupper(static_cast<unsigned char>(sql[i])) == 'A' &&
                std::toupper(static_cast<unsigned char>(sql[i + 1])) == 'S' &&
                std::isspace(static_cast<unsigned char>(sql[i + 2])) &&
                (i == 0 || std::isspace(static_cast<unsigned char>(sql[i - 1])))) {
                out.push_back(sql[i]);
                out.push_back(sql[i + 1]);
                i += 2;

                while (i < sql.size() &&
                       std::isspace(static_cast<unsigned char>(sql[i]))) {
                    out.push_back(sql[i]);
                    ++i;
                }

                const size_t alias_start = i;
                while (i < sql.size() &&
                       (std::isalnum(static_cast<unsigned char>(sql[i])) ||
                        sql[i] == '_' ||
                        sql[i] == '$')) {
                    ++i;
                }

                if (i > alias_start) {
                    std::string_view alias(sql.data() + alias_start, i - alias_start);
                    if (alias.find('$') != std::string_view::npos) {
                        out.push_back('"');
                        out.append(alias);
                        out.push_back('"');
                    } else {
                        out.append(alias);
                    }
                    continue;
                }
            }

            out.push_back(ch);
            ++i;
        }

        return out;
    }

    static std::string buildRightNestedUnionAll(const std::vector<std::string>& selects,
                                                size_t index = 0) {
        if (index >= selects.size()) {
            return {};
        }
        if (index + 1 == selects.size()) {
            return selects[index];
        }

        std::string out;
        out.reserve(selects[index].size() + 32);
        out.append(selects[index]);
        out.append("\nUNION ALL\n(\n");
        out.append(buildRightNestedUnionAll(selects, index + 1));
        out.append("\n)");
        return out;
    }

    static std::string buildFirebirdSchemaScopePredicate(std::string_view schema_name_expr) {
        const std::string base_schema_name = "{schema_path}";
        const std::string prefix_schema_name = "(" + base_schema_name + " || '.%')";
        return "(" + std::string(schema_name_expr) + " = " + base_schema_name +
               " OR " + std::string(schema_name_expr) + " LIKE " + prefix_schema_name + ")";
    }

    static std::string buildFirebirdFieldSourceExpr(std::string_view relation_name_expr,
                                                    std::string_view field_name_expr) {
        return "(" + std::string(relation_name_expr) + " || '$' || " +
               std::string(field_name_expr) + ")";
    }

    static std::string buildFirebirdSearchedCase(
        std::string_view normalized_type_expr,
        std::initializer_list<std::pair<std::string_view, std::string_view>> mappings,
        std::string_view default_expr) {
        std::string out = "CASE";
        const std::string normalized_expr(normalized_type_expr);
        for (const auto& [type_name, value_expr] : mappings) {
            out.append(" WHEN ");
            out.append(normalized_expr);
            out.append(" = ");
            out.append(quoteSqlLiteral(type_name));
            out.append(" THEN ");
            out.append(value_expr);
        }
        out.append(" ELSE ");
        out.append(default_expr);
        out.append(" END");
        return out;
    }

    static std::string buildFirebirdSingleRowSource() {
        return " FROM (SELECT MIN(schema_id) AS SB_EMU_SCHEMA_ID FROM sys.schemas) "
               "AS SB_EMU_SINGLETON";
    }

    static std::string buildFirebirdDatabaseViewQuery() {
        return R"SQL(
                SELECT
                    CAST(NULL AS TEXT) AS RDB$DESCRIPTION,
                    CAST(0 AS BIGINT) AS RDB$RELATION_ID,
                    CAST(NULL AS TEXT) AS RDB$SECURITY_CLASS,
                    CAST('UTF8' AS TEXT) AS RDB$CHARACTER_SET_NAME,
                    CAST(0 AS BIGINT) AS RDB$LINGER,
                    CAST(FALSE AS BOOLEAN) AS RDB$SQL_SECURITY
            )SQL" + buildFirebirdSingleRowSource();
    }

    static std::string buildFirebirdRelationsViewSourcePrototype() {
        return "SELECT '' AS RDB$RELATION_NAME, "
               "0 AS RDB$RELATION_ID, "
               "0 AS RDB$SYSTEM_FLAG, "
               "'SYSDBA' AS RDB$OWNER_NAME, "
               "NULL AS RDB$DESCRIPTION, "
               "0 AS RDB$VIEW_BLR, "
               "'' AS RDB$VIEW_SOURCE, "
               "0 AS RDB$RELATION_COUNTS, "
               "0 AS RDB$FORMAT, "
               "0 AS RDB$FIELD_ID, "
               "0 AS RDB$FLAGS, "
               "0 AS RDB$RELATION_TYPE, "
               "NULL AS RDB$EXTERNAL_FILE, "
               "NULL AS RDB$EXTERNAL_DESCRIPTION, "
               "NULL AS RDB$SECURITY_CLASS "
               "FROM RDB$DATABASE";
    }

    static std::string buildFirebirdFieldsViewSourcePrototype() {
        return "SELECT '' AS RDB$FIELD_NAME, "
               "0 AS RDB$FIELD_TYPE, "
               "0 AS RDB$FIELD_LENGTH, "
               "0 AS RDB$FIELD_SCALE, "
               "0 AS RDB$CHARACTER_SET_ID, "
               "0 AS RDB$COLLATION_ID, "
               "0 AS RDB$DIMENSIONS "
               "FROM RDB$DATABASE";
    }

    static std::string buildFirebirdRelationFieldsViewSourcePrototype() {
        return "SELECT '' AS RDB$RELATION_NAME, "
               "'' AS RDB$FIELD_NAME, "
               "'' AS RDB$FIELD_SOURCE, "
               "0 AS RDB$FIELD_POSITION, "
               "0 AS RDB$NULL_FLAG "
               "FROM RDB$DATABASE";
    }

    static std::string buildFirebirdRelationsViewQuery() {
        static const FirebirdSeedRelationSpec kSeedRelations[] = {
            {"RDB$DATABASE", 0, 0, nullptr},
            {"RDB$RELATIONS", 1, 1, "/* firebird emulation catalog view */"},
            {"RDB$FIELDS", 2, 1, "/* firebird emulation catalog view */"},
            {"RDB$RELATION_FIELDS", 3, 1, "/* firebird emulation catalog view */"},
        };

        std::vector<std::string> selects;

        std::ostringstream base_sql;
        base_sql << R"SQL(
                SELECT
                    t.table_name AS RDB$RELATION_NAME,
                    0 AS RDB$RELATION_ID,
                    0 AS RDB$SYSTEM_FLAG,
                    'SYSDBA' AS RDB$OWNER_NAME,
                    NULL AS RDB$DESCRIPTION,
                    0 AS RDB$VIEW_BLR,
                    NULL AS RDB$VIEW_SOURCE,
                    COALESCE(t.row_count, 0) AS RDB$RELATION_COUNTS,
                    0 AS RDB$FORMAT,
                    0 AS RDB$FIELD_ID,
                    0 AS RDB$FLAGS,
                    0 AS RDB$RELATION_TYPE,
                    NULL AS RDB$EXTERNAL_FILE,
                    NULL AS RDB$EXTERNAL_DESCRIPTION,
                    NULL AS RDB$SECURITY_CLASS
                FROM sys.tables t
                JOIN sys.schemas s ON t.schema_id = s.schema_id
                WHERE )SQL"
            << buildFirebirdSchemaScopePredicate("s.schema_name")
            << R"SQL(
            )SQL";
        selects.push_back(base_sql.str());

        for (const auto& relation : kSeedRelations) {
            std::string relation_view_source;
            if (std::string_view(relation.relation_name) == "RDB$RELATIONS") {
                relation_view_source = buildFirebirdRelationsViewSourcePrototype();
            } else if (std::string_view(relation.relation_name) == "RDB$FIELDS") {
                relation_view_source = buildFirebirdFieldsViewSourcePrototype();
            } else if (std::string_view(relation.relation_name) == "RDB$RELATION_FIELDS") {
                relation_view_source = buildFirebirdRelationFieldsViewSourcePrototype();
            } else if (relation.view_source != nullptr) {
                relation_view_source = relation.view_source;
            }

            std::ostringstream seed_sql;
            seed_sql << "SELECT "
                     << quoteSqlLiteral(relation.relation_name) << " AS RDB$RELATION_NAME, "
                     << relation.relation_id << " AS RDB$RELATION_ID, "
                     << "1 AS RDB$SYSTEM_FLAG, "
                     << quoteSqlLiteral("SYSDBA") << " AS RDB$OWNER_NAME, "
                     << "NULL AS RDB$DESCRIPTION, "
                     << "0 AS RDB$VIEW_BLR, ";
            if (!relation_view_source.empty()) {
                seed_sql << quoteSqlLiteral(relation_view_source);
            } else {
                seed_sql << "NULL";
            }
            seed_sql << " AS RDB$VIEW_SOURCE, "
                     << "0 AS RDB$RELATION_COUNTS, "
                     << "0 AS RDB$FORMAT, "
                     << "0 AS RDB$FIELD_ID, "
                     << "0 AS RDB$FLAGS, "
                     << relation.relation_type << " AS RDB$RELATION_TYPE, "
                     << "NULL AS RDB$EXTERNAL_FILE, "
                     << "NULL AS RDB$EXTERNAL_DESCRIPTION, "
                     << "NULL AS RDB$SECURITY_CLASS"
                     << buildFirebirdSingleRowSource();
            selects.push_back(seed_sql.str());
        }

        return buildRightNestedUnionAll(selects);
    }

    static std::string buildFirebirdFieldsViewQuery() {
        static const FirebirdSeedRelationFieldSpec kSeedFields[] = {
            {"RDB$DATABASE", "RDB$DESCRIPTION", FirebirdSeedFieldKind::TEXT, 32765, 0, false, true, 4, true, 0, false, 0},
            {"RDB$DATABASE", "RDB$RELATION_ID", FirebirdSeedFieldKind::BIGINT, 8, 0, false, false, 0, false, 0, false, 0},
            {"RDB$DATABASE", "RDB$SECURITY_CLASS", FirebirdSeedFieldKind::TEXT, 255, 0, false, true, 4, true, 0, false, 0},
            {"RDB$DATABASE", "RDB$CHARACTER_SET_NAME", FirebirdSeedFieldKind::TEXT, 63, 0, false, true, 4, true, 0, false, 0},
            {"RDB$DATABASE", "RDB$LINGER", FirebirdSeedFieldKind::BIGINT, 8, 0, false, false, 0, false, 0, false, 0},
            {"RDB$DATABASE", "RDB$SQL_SECURITY", FirebirdSeedFieldKind::BOOLEAN, 1, 0, false, false, 0, false, 0, false, 0},
            {"RDB$RELATIONS", "RDB$RELATION_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATIONS", "RDB$VIEW_SOURCE", FirebirdSeedFieldKind::TEXT, 32765, 0, false, true, 4, true, 0, false, 0},
            {"RDB$RELATIONS", "RDB$OWNER_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, false, true, 4, true, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_TYPE", FirebirdSeedFieldKind::SMALLINT, 2, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_LENGTH", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_SCALE", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$CHARACTER_SET_ID", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$COLLATION_ID", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$DIMENSIONS", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$RELATION_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$FIELD_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$FIELD_SOURCE", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$FIELD_POSITION", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$NULL_FLAG", FirebirdSeedFieldKind::SMALLINT, 2, 0, false, false, 0, false, 0, false, 0},
        };

        std::vector<std::string> selects;
        const std::string field_source_expr =
            buildFirebirdFieldSourceExpr("t.table_name", "c.column_name");
        const std::string normalized_type_expr = "UPPER(TRIM(c.data_type_name))";
        const std::string field_length_case = buildFirebirdSearchedCase(
            normalized_type_expr,
            {
                {"CHARACTER", "255"},
                {"CHAR", "255"},
                {"CHARACTER VARYING", "255"},
                {"VARCHAR", "255"},
                {"TEXT", "32765"},
                {"JSON", "32765"},
                {"JSONB", "32765"},
                {"XML", "32765"},
                {"UUID", "16"},
                {"SMALLINT", "2"},
                {"INT16", "2"},
                {"INTEGER", "4"},
                {"INT", "4"},
                {"INT32", "4"},
                {"BIGINT", "8"},
                {"INT64", "8"},
                {"REAL", "4"},
                {"FLOAT32", "4"},
                {"DOUBLE", "8"},
                {"DOUBLE PRECISION", "8"},
                {"FLOAT64", "8"},
                {"NUMERIC", "8"},
                {"DECIMAL", "8"},
                {"DATE", "4"},
                {"TIME", "4"},
                {"TIMESTAMP", "8"},
                {"BOOL", "1"},
                {"BOOLEAN", "1"},
                {"BYTEA", "8"},
                {"BLOB", "8"},
            },
            "255");
        const std::string field_type_case = buildFirebirdSearchedCase(
            normalized_type_expr,
            {
                {"CHARACTER VARYING", "37"},
                {"VARCHAR", "37"},
                {"CHARACTER", "14"},
                {"CHAR", "14"},
                {"TEXT", "261"},
                {"JSON", "261"},
                {"JSONB", "261"},
                {"XML", "261"},
                {"UUID", "14"},
                {"INTEGER", "8"},
                {"INT", "8"},
                {"INT32", "8"},
                {"BIGINT", "16"},
                {"INT64", "16"},
                {"SMALLINT", "7"},
                {"INT16", "7"},
                {"REAL", "10"},
                {"FLOAT32", "10"},
                {"DOUBLE", "27"},
                {"DOUBLE PRECISION", "27"},
                {"FLOAT64", "27"},
                {"NUMERIC", "16"},
                {"DECIMAL", "16"},
                {"DATE", "12"},
                {"TIME", "13"},
                {"TIMESTAMP", "35"},
                {"BYTEA", "261"},
                {"BLOB", "261"},
                {"BOOL", "23"},
                {"BOOLEAN", "23"},
            },
            "37");
        const std::string field_subtype_case = buildFirebirdSearchedCase(
            normalized_type_expr,
            {
                {"CHARACTER VARYING", "0"},
                {"VARCHAR", "0"},
                {"CHARACTER", "1"},
                {"CHAR", "1"},
                {"INTEGER", "0"},
                {"INT", "0"},
                {"INT32", "0"},
                {"BIGINT", "1"},
                {"INT64", "1"},
                {"SMALLINT", "0"},
                {"INT16", "0"},
                {"REAL", "0"},
                {"FLOAT32", "0"},
                {"DOUBLE", "0"},
                {"DOUBLE PRECISION", "0"},
                {"FLOAT64", "0"},
                {"NUMERIC", "2"},
                {"DECIMAL", "2"},
                {"DATE", "0"},
                {"TIME", "0"},
                {"TIMESTAMP", "0"},
                {"BYTEA", "0"},
                {"BLOB", "0"},
                {"BOOL", "0"},
                {"BOOLEAN", "0"},
            },
            "0");
        const std::string character_length_case = buildFirebirdSearchedCase(
            normalized_type_expr,
            {
                {"CHARACTER VARYING", "255"},
                {"VARCHAR", "255"},
                {"CHARACTER", "255"},
                {"CHAR", "255"},
            },
            "0");
        const std::string field_name_expr = castSqlExpr(field_source_expr, "VARCHAR(255)");
        const std::string field_query_name_expr = castSqlExpr("0", "SMALLINT");
        const std::string validation_blr_expr = castSqlExpr("NULL", "BLOB");
        const std::string validation_source_expr = castSqlExpr("NULL", "TEXT");
        const std::string computed_blr_expr = castSqlExpr("NULL", "BLOB");
        const std::string computed_source_expr =
            castSqlExpr("c.generation_expression", "TEXT");
        const std::string default_value_expr = castSqlExpr("c.default_value", "TEXT");
        const std::string default_source_expr = castSqlExpr("NULL", "TEXT");
        const std::string field_length_expr = castSqlExpr(field_length_case, "INTEGER");
        const std::string field_scale_expr = castSqlExpr("0", "INTEGER");
        const std::string field_type_expr = castSqlExpr(field_type_case, "SMALLINT");
        const std::string field_subtype_expr = castSqlExpr(field_subtype_case, "SMALLINT");
        const std::string missing_value_expr = castSqlExpr("NULL", "TEXT");
        const std::string missing_source_expr = castSqlExpr("NULL", "TEXT");
        const std::string description_expr = castSqlExpr("NULL", "TEXT");
        const std::string system_flag_expr = castSqlExpr("0", "SMALLINT");
        const std::string query_header_expr = castSqlExpr("NULL", "TEXT");
        const std::string segment_length_expr = castSqlExpr("0", "INTEGER");
        const std::string edit_string_expr = castSqlExpr("NULL", "TEXT");
        const std::string external_length_expr = castSqlExpr("0", "INTEGER");
        const std::string external_scale_expr = castSqlExpr("0", "INTEGER");
        const std::string external_type_expr = castSqlExpr("0", "INTEGER");
        const std::string dimensions_expr = castSqlExpr("0", "INTEGER");
        const std::string null_flag_expr =
            castSqlExpr("CASE WHEN c.is_nullable THEN 0 ELSE 1 END", "SMALLINT");
        const std::string character_length_expr =
            castSqlExpr(character_length_case, "INTEGER");
        const std::string collation_id_expr =
            castSqlExpr("CASE WHEN c.collation_id = 0 THEN NULL ELSE c.collation_id END",
                        "INTEGER");
        const std::string charset_id_expr =
            castSqlExpr("CASE WHEN c.charset_id IS NULL THEN NULL ELSE 4 END", "INTEGER");
        const std::string field_precision_expr = castSqlExpr("0", "INTEGER");
        const std::string security_class_expr = castSqlExpr("NULL", "TEXT");
        const std::string owner_name_expr = castSqlExpr("NULL", "TEXT");
        std::ostringstream base_sql;
        base_sql << R"SQL(
                SELECT
                    )SQL" << field_name_expr << R"SQL( AS RDB$FIELD_NAME,
                    )SQL" << field_query_name_expr << R"SQL( AS RDB$QUERY_NAME,
                    )SQL" << validation_blr_expr << R"SQL( AS RDB$VALIDATION_BLR,
                    )SQL" << validation_source_expr << R"SQL( AS RDB$VALIDATION_SOURCE,
                    )SQL" << computed_blr_expr << R"SQL( AS RDB$COMPUTED_BLR,
                    )SQL" << computed_source_expr << R"SQL( AS RDB$COMPUTED_SOURCE,
                    )SQL" << default_value_expr << R"SQL( AS RDB$DEFAULT_VALUE,
                    )SQL" << default_source_expr << R"SQL( AS RDB$DEFAULT_SOURCE,
                    )SQL" << field_length_expr << R"SQL( AS RDB$FIELD_LENGTH,
                    )SQL" << field_scale_expr << R"SQL( AS RDB$FIELD_SCALE,
                    )SQL" << field_type_expr << R"SQL( AS RDB$FIELD_TYPE,
                    )SQL" << field_subtype_expr << R"SQL( AS RDB$FIELD_SUB_TYPE,
                    )SQL" << missing_value_expr << R"SQL( AS RDB$MISSING_VALUE,
                    )SQL" << missing_source_expr << R"SQL( AS RDB$MISSING_SOURCE,
                    )SQL" << description_expr << R"SQL( AS RDB$DESCRIPTION,
                    )SQL" << system_flag_expr << R"SQL( AS RDB$SYSTEM_FLAG,
                    )SQL" << query_header_expr << R"SQL( AS RDB$QUERY_HEADER,
                    )SQL" << segment_length_expr << R"SQL( AS RDB$SEGMENT_LENGTH,
                    )SQL" << edit_string_expr << R"SQL( AS RDB$EDIT_STRING,
                    )SQL" << external_length_expr << R"SQL( AS RDB$EXTERNAL_LENGTH,
                    )SQL" << external_scale_expr << R"SQL( AS RDB$EXTERNAL_SCALE,
                    )SQL" << external_type_expr << R"SQL( AS RDB$EXTERNAL_TYPE,
                    )SQL" << dimensions_expr << R"SQL( AS RDB$DIMENSIONS,
                    )SQL" << null_flag_expr << R"SQL( AS RDB$NULL_FLAG,
                    )SQL" << character_length_expr << R"SQL( AS RDB$CHARACTER_LENGTH,
                    )SQL" << collation_id_expr << R"SQL( AS RDB$COLLATION_ID,
                    )SQL" << charset_id_expr << R"SQL( AS RDB$CHARACTER_SET_ID,
                    )SQL" << field_precision_expr << R"SQL( AS RDB$FIELD_PRECISION,
                    )SQL" << security_class_expr << R"SQL( AS RDB$SECURITY_CLASS,
                    )SQL" << owner_name_expr << R"SQL( AS RDB$OWNER_NAME
                FROM sys.columns c
                JOIN sys.tables t ON c.table_id = t.table_id
                JOIN sys.schemas s ON t.schema_id = s.schema_id
                WHERE )SQL"
            << buildFirebirdSchemaScopePredicate("s.schema_name")
            << R"SQL(
            )SQL";
        selects.push_back(base_sql.str());

        for (size_t i = 0; i < std::size(kSeedFields); ++i) {
            bool already_emitted = false;
            for (size_t j = 0; j < i; ++j) {
                if (std::string_view(kSeedFields[j].field_name) ==
                    std::string_view(kSeedFields[i].field_name)) {
                    already_emitted = true;
                    break;
                }
            }
            if (already_emitted) {
                continue;
            }

            const auto& field = kSeedFields[i];
            const int field_type = firebirdSeedFieldTypeCode(field.kind);
            const int field_length = firebirdSeedFieldLogicalLength(field.kind, field.field_length);
            const int character_length =
                field.kind == FirebirdSeedFieldKind::TEXT ? field.field_length : 0;

            std::ostringstream seed_sql;
            seed_sql << "SELECT "
                     << "CAST(" << quoteSqlLiteral(field.field_name)
                     << " AS VARCHAR(255)) AS RDB$FIELD_NAME, "
                     << "CAST(0 AS SMALLINT) AS RDB$QUERY_NAME, "
                     << "CAST(NULL AS BLOB) AS RDB$VALIDATION_BLR, "
                     << "CAST(NULL AS TEXT) AS RDB$VALIDATION_SOURCE, "
                     << "CAST(NULL AS BLOB) AS RDB$COMPUTED_BLR, "
                     << "CAST(NULL AS TEXT) AS RDB$COMPUTED_SOURCE, "
                     << "CAST(NULL AS TEXT) AS RDB$DEFAULT_VALUE, "
                     << "CAST(NULL AS TEXT) AS RDB$DEFAULT_SOURCE, "
                     << "CAST(" << field_length << " AS INTEGER) AS RDB$FIELD_LENGTH, "
                     << "CAST(" << field.field_scale << " AS INTEGER) AS RDB$FIELD_SCALE, "
                     << "CAST(" << field_type << " AS SMALLINT) AS RDB$FIELD_TYPE, "
                     << "CAST(0 AS SMALLINT) AS RDB$FIELD_SUB_TYPE, "
                     << "CAST(NULL AS TEXT) AS RDB$MISSING_VALUE, "
                     << "CAST(NULL AS TEXT) AS RDB$MISSING_SOURCE, "
                     << "CAST(NULL AS TEXT) AS RDB$DESCRIPTION, "
                     << "CAST(1 AS SMALLINT) AS RDB$SYSTEM_FLAG, "
                     << "CAST(NULL AS TEXT) AS RDB$QUERY_HEADER, "
                     << "CAST(0 AS INTEGER) AS RDB$SEGMENT_LENGTH, "
                     << "CAST(NULL AS TEXT) AS RDB$EDIT_STRING, "
                     << "CAST(0 AS INTEGER) AS RDB$EXTERNAL_LENGTH, "
                     << "CAST(0 AS INTEGER) AS RDB$EXTERNAL_SCALE, "
                     << "CAST(0 AS INTEGER) AS RDB$EXTERNAL_TYPE, ";
            if (field.has_dimensions) {
                seed_sql << "CAST(" << field.dimensions << " AS INTEGER)";
            } else {
                seed_sql << "CAST(0 AS INTEGER)";
            }
            seed_sql << " AS RDB$DIMENSIONS, "
                     << "CAST(" << (field.not_nullable ? "1" : "0")
                     << " AS SMALLINT) AS RDB$NULL_FLAG, "
                     << "CAST(" << character_length << " AS INTEGER) AS RDB$CHARACTER_LENGTH, ";
            if (field.has_collation) {
                seed_sql << "CAST(" << field.collation_id << " AS INTEGER)";
            } else {
                seed_sql << "CAST(NULL AS INTEGER)";
            }
            seed_sql << " AS RDB$COLLATION_ID, ";
            if (field.has_charset) {
                seed_sql << "CAST(" << field.charset_id << " AS INTEGER)";
            } else {
                seed_sql << "CAST(NULL AS INTEGER)";
            }
            seed_sql << " AS RDB$CHARACTER_SET_ID, "
                     << "CAST(0 AS INTEGER) AS RDB$FIELD_PRECISION, "
                     << "CAST(NULL AS TEXT) AS RDB$SECURITY_CLASS, "
                     << "CAST(NULL AS TEXT) AS RDB$OWNER_NAME"
                     << buildFirebirdSingleRowSource();
            selects.push_back(seed_sql.str());
        }

        return buildRightNestedUnionAll(selects);
    }

    static std::string buildFirebirdRelationFieldsViewQuery() {
        static const FirebirdSeedRelationFieldSpec kSeedFields[] = {
            {"RDB$DATABASE", "RDB$DESCRIPTION", FirebirdSeedFieldKind::TEXT, 32765, 0, false, true, 4, true, 0, false, 0},
            {"RDB$DATABASE", "RDB$RELATION_ID", FirebirdSeedFieldKind::BIGINT, 8, 0, false, false, 0, false, 0, false, 0},
            {"RDB$DATABASE", "RDB$SECURITY_CLASS", FirebirdSeedFieldKind::TEXT, 255, 0, false, true, 4, true, 0, false, 0},
            {"RDB$DATABASE", "RDB$CHARACTER_SET_NAME", FirebirdSeedFieldKind::TEXT, 63, 0, false, true, 4, true, 0, false, 0},
            {"RDB$DATABASE", "RDB$LINGER", FirebirdSeedFieldKind::BIGINT, 8, 0, false, false, 0, false, 0, false, 0},
            {"RDB$DATABASE", "RDB$SQL_SECURITY", FirebirdSeedFieldKind::BOOLEAN, 1, 0, false, false, 0, false, 0, false, 0},
            {"RDB$RELATIONS", "RDB$RELATION_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATIONS", "RDB$VIEW_SOURCE", FirebirdSeedFieldKind::TEXT, 32765, 0, false, true, 4, true, 0, false, 0},
            {"RDB$RELATIONS", "RDB$OWNER_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, false, true, 4, true, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_TYPE", FirebirdSeedFieldKind::SMALLINT, 2, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_LENGTH", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$FIELD_SCALE", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$CHARACTER_SET_ID", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$COLLATION_ID", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$FIELDS", "RDB$DIMENSIONS", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$RELATION_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$FIELD_NAME", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$FIELD_SOURCE", FirebirdSeedFieldKind::TEXT, 255, 0, true, true, 4, true, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$FIELD_POSITION", FirebirdSeedFieldKind::INTEGER, 4, 0, false, false, 0, false, 0, false, 0},
            {"RDB$RELATION_FIELDS", "RDB$NULL_FLAG", FirebirdSeedFieldKind::SMALLINT, 2, 0, false, false, 0, false, 0, false, 0},
        };

        std::vector<std::string> selects;
        const std::string relation_field_source_expr =
            buildFirebirdFieldSourceExpr("t.table_name", "c.column_name");
        std::ostringstream base_sql;
        base_sql << R"SQL(
                SELECT
                    t.table_name AS RDB$RELATION_NAME,
                    c.column_name AS RDB$FIELD_NAME,
                    )SQL" << relation_field_source_expr << R"SQL( AS RDB$FIELD_SOURCE,
                    NULL AS RDB$QUERY_NAME,
                    NULL AS RDB$BASE_FIELD,
                    NULL AS RDB$EDIT_STRING,
                    c.ordinal_position AS RDB$FIELD_POSITION,
                    NULL AS RDB$QUERY_HEADER,
                    0 AS RDB$UPDATE_FLAG,
                    c.ordinal_position AS RDB$FIELD_ID,
                    NULL AS RDB$VIEW_CONTEXT,
                    NULL AS RDB$DESCRIPTION,
                    c.default_value AS RDB$DEFAULT_VALUE,
                    NULL AS RDB$DEFAULT_SOURCE,
                    0 AS RDB$SYSTEM_FLAG,
                    NULL AS RDB$SECURITY_CLASS,
                    NULL AS RDB$COMPLEX_NAME,
                    CASE WHEN c.is_nullable THEN 0 ELSE 1 END AS RDB$NULL_FLAG,
                    NULL AS RDB$COLLATION_ID,
                    CASE WHEN c.is_identity THEN )SQL" << relation_field_source_expr << R"SQL( ELSE NULL END AS RDB$GENERATOR_NAME,
                    NULL AS RDB$IDENTITY_TYPE
                FROM sys.columns c
                JOIN sys.tables t ON c.table_id = t.table_id
                JOIN sys.schemas s ON t.schema_id = s.schema_id
                WHERE )SQL"
            << buildFirebirdSchemaScopePredicate("s.schema_name")
            << R"SQL(
            )SQL";
        selects.push_back(base_sql.str());

        std::string current_relation;
        int position = 0;
        for (const auto& field : kSeedFields) {
            if (current_relation != field.relation_name) {
                current_relation = field.relation_name;
                position = 0;
            }

            std::ostringstream seed_sql;
            seed_sql << "SELECT "
                     << quoteSqlLiteral(field.relation_name) << " AS RDB$RELATION_NAME, "
                     << quoteSqlLiteral(field.field_name) << " AS RDB$FIELD_NAME, "
                     << quoteSqlLiteral(field.field_name) << " AS RDB$FIELD_SOURCE, "
                     << "NULL AS RDB$QUERY_NAME, "
                     << "NULL AS RDB$BASE_FIELD, "
                     << "NULL AS RDB$EDIT_STRING, "
                     << position << " AS RDB$FIELD_POSITION, "
                     << "NULL AS RDB$QUERY_HEADER, "
                     << "0 AS RDB$UPDATE_FLAG, "
                     << position << " AS RDB$FIELD_ID, "
                     << "NULL AS RDB$VIEW_CONTEXT, "
                     << "NULL AS RDB$DESCRIPTION, "
                     << "NULL AS RDB$DEFAULT_VALUE, "
                     << "NULL AS RDB$DEFAULT_SOURCE, "
                     << "1 AS RDB$SYSTEM_FLAG, "
                     << "NULL AS RDB$SECURITY_CLASS, "
                     << "NULL AS RDB$COMPLEX_NAME, "
                     << (field.not_nullable ? "1" : "0") << " AS RDB$NULL_FLAG, "
                     << "NULL AS RDB$COLLATION_ID, "
                     << "NULL AS RDB$GENERATOR_NAME, "
                     << "NULL AS RDB$IDENTITY_TYPE"
                     << buildFirebirdSingleRowSource();
            selects.push_back(seed_sql.str());
            ++position;
        }

        return buildRightNestedUnionAll(selects);
    }

    // ========================================================================
    // Protocol-Specific View Definitions
    // ========================================================================

    /**
     * Firebird RDB$* system table definitions
     */
    std::vector<EmulatedViewDefinition> getFirebirdViews() {
        std::vector<EmulatedViewDefinition> views;

        views.push_back({
            "RDB$DATABASE",
            buildFirebirdDatabaseViewQuery(),
            {"RDB$DESCRIPTION", "RDB$RELATION_ID", "RDB$SECURITY_CLASS",
             "RDB$CHARACTER_SET_NAME", "RDB$LINGER", "RDB$SQL_SECURITY"},
            {DataType::VARCHAR, DataType::INT64, DataType::VARCHAR,
             DataType::VARCHAR, DataType::INT64, DataType::BOOLEAN},
            "Single-row Firebird compatibility relation backed by the emulated catalog"
        });

        // RDB$RELATIONS - Tables
        views.push_back({
            "RDB$RELATIONS",
            buildFirebirdRelationsViewQuery(),
            {"RDB$RELATION_NAME", "RDB$RELATION_ID", "RDB$SYSTEM_FLAG",
             "RDB$OWNER_NAME", "RDB$DESCRIPTION", "RDB$VIEW_BLR",
             "RDB$VIEW_SOURCE", "RDB$RELATION_COUNTS", "RDB$FORMAT",
             "RDB$FIELD_ID", "RDB$FLAGS", "RDB$RELATION_TYPE",
             "RDB$EXTERNAL_FILE", "RDB$EXTERNAL_DESCRIPTION", "RDB$SECURITY_CLASS"},
            {DataType::VARCHAR, DataType::INT32, DataType::INT16,
             DataType::VARCHAR, DataType::VARCHAR, DataType::BLOB,
             DataType::VARCHAR, DataType::INT64, DataType::INT16,
             DataType::INT16, DataType::INT16, DataType::INT16,
             DataType::VARCHAR, DataType::BLOB, DataType::VARCHAR},
            "Maps tables to Firebird RDB$RELATIONS format"
        });

        // RDB$FIELDS - Global field definitions (columns)
        views.push_back({
            "RDB$FIELDS",
            buildFirebirdFieldsViewQuery(),
            {"RDB$FIELD_NAME", "RDB$QUERY_NAME", "RDB$VALIDATION_BLR",
             "RDB$VALIDATION_SOURCE", "RDB$COMPUTED_BLR", "RDB$COMPUTED_SOURCE",
             "RDB$DEFAULT_VALUE", "RDB$DEFAULT_SOURCE", "RDB$FIELD_LENGTH",
             "RDB$FIELD_SCALE", "RDB$FIELD_TYPE", "RDB$FIELD_SUB_TYPE",
             "RDB$MISSING_VALUE", "RDB$MISSING_SOURCE", "RDB$DESCRIPTION",
             "RDB$SYSTEM_FLAG", "RDB$QUERY_HEADER", "RDB$SEGMENT_LENGTH",
             "RDB$EDIT_STRING", "RDB$EXTERNAL_LENGTH", "RDB$EXTERNAL_SCALE",
             "RDB$EXTERNAL_TYPE", "RDB$DIMENSIONS", "RDB$NULL_FLAG",
             "RDB$CHARACTER_LENGTH", "RDB$COLLATION_ID", "RDB$CHARACTER_SET_ID",
             "RDB$FIELD_PRECISION", "RDB$SECURITY_CLASS", "RDB$OWNER_NAME"},
            {DataType::VARCHAR, DataType::INT16, DataType::BLOB,
             DataType::VARCHAR, DataType::BLOB, DataType::VARCHAR,
             DataType::VARCHAR, DataType::VARCHAR, DataType::INT32,
             DataType::INT32, DataType::INT16, DataType::INT16,
             DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
             DataType::INT16, DataType::VARCHAR, DataType::INT32,
             DataType::VARCHAR, DataType::INT32, DataType::INT32,
             DataType::INT32, DataType::INT32, DataType::INT16,
             DataType::INT32, DataType::INT32, DataType::INT32,
             DataType::INT32, DataType::VARCHAR, DataType::VARCHAR},
            "Maps columns to Firebird RDB$FIELDS format"
        });

        // RDB$RELATION_FIELDS - Relation-specific field definitions
        views.push_back({
            "RDB$RELATION_FIELDS",
            buildFirebirdRelationFieldsViewQuery(),
            {"RDB$RELATION_NAME", "RDB$FIELD_NAME", "RDB$FIELD_SOURCE",
             "RDB$QUERY_NAME", "RDB$BASE_FIELD", "RDB$EDIT_STRING",
             "RDB$FIELD_POSITION", "RDB$QUERY_HEADER", "RDB$UPDATE_FLAG",
             "RDB$FIELD_ID", "RDB$VIEW_CONTEXT", "RDB$DESCRIPTION",
             "RDB$DEFAULT_VALUE", "RDB$DEFAULT_SOURCE", "RDB$SYSTEM_FLAG",
             "RDB$SECURITY_CLASS", "RDB$COMPLEX_NAME", "RDB$NULL_FLAG",
             "RDB$COLLATION_ID", "RDB$GENERATOR_NAME", "RDB$IDENTITY_TYPE"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
             DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
             DataType::INT32, DataType::VARCHAR, DataType::INT16,
             DataType::INT32, DataType::INT32, DataType::VARCHAR,
             DataType::VARCHAR, DataType::VARCHAR, DataType::INT16,
             DataType::VARCHAR, DataType::VARCHAR, DataType::INT16,
             DataType::INT32, DataType::VARCHAR, DataType::INT16},
            "Maps table columns to Firebird RDB$RELATION_FIELDS format"
        });

        // RDB$INDICES - Indexes
        views.push_back({
            "RDB$INDICES",
            std::string(R"SQL(
                SELECT
                    i.index_name AS RDB$INDEX_NAME,
                    t.table_name AS RDB$RELATION_NAME,
                    CAST(i.index_id AS INTEGER) AS RDB$INDEX_ID,
                    CASE WHEN i.is_unique THEN 1 ELSE 0 END AS RDB$UNIQUE_FLAG,
                    i.description AS RDB$DESCRIPTION,
                    ARRAY_LENGTH(i.column_ordinals) AS RDB$SEGMENT_COUNT,
                    0 AS RDB$INDEX_INACTIVE,
                    CASE WHEN i.index_type = 'BTREE' THEN 0 ELSE 1 END AS RDB$INDEX_TYPE,
                    NULL AS RDB$FOREIGN_KEY,
                    0 AS RDB$SYSTEM_FLAG,
                    NULL AS RDB$EXPRESSION_BLR,
                    NULL AS RDB$EXPRESSION_SOURCE,
                    NULL AS RDB$STATISTICS
                FROM sys.indexes i
                JOIN sys.tables t ON i.table_id = t.table_id
                JOIN sys.schemas s ON t.schema_id = s.schema_id
                WHERE )SQL") + buildFirebirdSchemaScopePredicate("s.schema_name"),
            {"RDB$INDEX_NAME", "RDB$RELATION_NAME", "RDB$INDEX_ID",
             "RDB$UNIQUE_FLAG", "RDB$SEGMENT_COUNT", "RDB$INDEX_TYPE"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::INT32,
             DataType::INT16, DataType::INT16, DataType::INT16},
            "Maps indexes to Firebird RDB$INDICES format"
        });

        // RDB$PROCEDURES - Stored procedures
        views.push_back({
            "RDB$PROCEDURES",
            R"SQL(
                SELECT
                    CAST(NULL AS TEXT) AS RDB$PROCEDURE_NAME,
                    CAST(NULL AS INTEGER) AS RDB$PROCEDURE_ID,
                    CAST(NULL AS SMALLINT) AS RDB$PROCEDURE_INPUTS,
                    CAST(NULL AS SMALLINT) AS RDB$PROCEDURE_OUTPUTS,
                    CAST(NULL AS TEXT) AS RDB$DESCRIPTION,
                    CAST(NULL AS BLOB) AS RDB$PROCEDURE_BLR,
                    CAST(NULL AS TEXT) AS RDB$PROCEDURE_SOURCE,
                    CAST(NULL AS TEXT) AS RDB$SECURITY_CLASS,
                    CAST(NULL AS TEXT) AS RDB$OWNER_NAME,
                    CAST(NULL AS SMALLINT) AS RDB$PROCEDURE_TYPE,
                    CAST(NULL AS SMALLINT) AS RDB$VALID_BLR,
                    CAST(NULL AS INTEGER) AS RDB$DEBUG_INFO,
                    CAST(NULL AS TEXT) AS RDB$ENGINE_NAME,
                    CAST(NULL AS TEXT) AS RDB$ENTRYPOINT,
                    CAST(NULL AS TEXT) AS RDB$PACKAGE_NAME,
                    CAST(NULL AS SMALLINT) AS RDB$PRIVATE_FLAG,
                    CAST(NULL AS SMALLINT) AS RDB$SYSTEM_FLAG,
                    CAST(NULL AS SMALLINT) AS RDB$SQL_SECURITY
                FROM RDB$DATABASE
                WHERE 1 = 0
            )SQL",
            {"RDB$PROCEDURE_NAME", "RDB$PROCEDURE_ID", "RDB$PROCEDURE_INPUTS",
             "RDB$PROCEDURE_SOURCE", "RDB$OWNER_NAME"},
            {DataType::VARCHAR, DataType::INT32, DataType::INT16,
             DataType::VARCHAR, DataType::VARCHAR},
            "Maps procedures to Firebird RDB$PROCEDURES format"
        });

        // RDB$TRIGGERS - Triggers
        views.push_back({
            "RDB$TRIGGERS",
            R"SQL(
                SELECT
                    CAST(NULL AS TEXT) AS RDB$TRIGGER_NAME,
                    CAST(NULL AS TEXT) AS RDB$RELATION_NAME,
                    CAST(NULL AS SMALLINT) AS RDB$TRIGGER_SEQUENCE,
                    CAST(NULL AS SMALLINT) AS RDB$TRIGGER_TYPE,
                    CAST(NULL AS TEXT) AS RDB$TRIGGER_SOURCE,
                    CAST(NULL AS BLOB) AS RDB$TRIGGER_BLR,
                    CAST(NULL AS TEXT) AS RDB$DESCRIPTION,
                    CAST(NULL AS SMALLINT) AS RDB$TRIGGER_INACTIVE,
                    CAST(NULL AS SMALLINT) AS RDB$SYSTEM_FLAG,
                    CAST(NULL AS SMALLINT) AS RDB$FLAGS,
                    CAST(NULL AS SMALLINT) AS RDB$VALID_BLR,
                    CAST(NULL AS INTEGER) AS RDB$DEBUG_INFO,
                    CAST(NULL AS TEXT) AS RDB$ENGINE_NAME,
                    CAST(NULL AS TEXT) AS RDB$ENTRYPOINT,
                    CAST(NULL AS SMALLINT) AS RDB$SQL_SECURITY
                FROM RDB$DATABASE
                WHERE 1 = 0
            )SQL",
            {"RDB$TRIGGER_NAME", "RDB$RELATION_NAME", "RDB$TRIGGER_SEQUENCE",
             "RDB$TRIGGER_TYPE", "RDB$TRIGGER_SOURCE", "RDB$TRIGGER_INACTIVE"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::INT16,
             DataType::INT16, DataType::VARCHAR, DataType::INT16},
            "Maps triggers to Firebird RDB$TRIGGERS format"
        });

        // MON$DATABASE - Monitoring database metadata
        views.push_back({
            "MON$DATABASE",
            R"SQL(
                SELECT
                    COALESCE((SELECT database_name FROM sys.sessions
                              WHERE database_name IS NOT NULL LIMIT 1),
                             'scratchbird') AS MON$DATABASE_NAME,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'page_size_bytes' LIMIT 1), 0) AS MON$PAGE_SIZE,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'ods_major' LIMIT 1), 0) AS MON$ODS_MAJOR,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'ods_minor' LIMIT 1), 0) AS MON$ODS_MINOR,
                    COALESCE((SELECT MIN(transaction_id) FROM sys.transactions), 0) AS MON$OLDEST_TRANSACTION,
                    COALESCE((SELECT MIN(transaction_id) FROM sys.transactions
                              WHERE state = 'active'), 0) AS MON$OLDEST_ACTIVE,
                    COALESCE((SELECT MIN(transaction_id) FROM sys.transactions), 0) AS MON$OLDEST_SNAPSHOT,
                    COALESCE((SELECT MAX(transaction_id) FROM sys.transactions), 0) AS MON$NEXT_TRANSACTION,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'buffer_pool_pages_total' LIMIT 1), 0) AS MON$PAGE_BUFFERS,
                    3 AS MON$SQL_DIALECT,
                    0 AS MON$SHUTDOWN_MODE,
                    0 AS MON$SWEEP_INTERVAL,
                    0 AS MON$READ_ONLY,
                    1 AS MON$FORCED_WRITES,
                    1 AS MON$RESERVE_SPACE,
                    NULL AS MON$CREATION_DATE,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'allocated_pages' LIMIT 1), 0) AS MON$ALLOCATED_PAGES,
                    1 AS MON$STAT_ID,
                    0 AS MON$BACKUP_STATE,
                    0 AS MON$CRYPT_PAGE,
                    'SYSDBA' AS MON$OWNER,
                    'Default' AS MON$SEC_DATABASE
            )SQL",
            {"MON$DATABASE_NAME", "MON$PAGE_SIZE", "MON$ODS_MAJOR", "MON$ODS_MINOR",
             "MON$OLDEST_TRANSACTION", "MON$OLDEST_ACTIVE", "MON$OLDEST_SNAPSHOT",
             "MON$NEXT_TRANSACTION", "MON$PAGE_BUFFERS", "MON$SQL_DIALECT",
             "MON$SHUTDOWN_MODE", "MON$SWEEP_INTERVAL", "MON$READ_ONLY",
             "MON$FORCED_WRITES", "MON$RESERVE_SPACE", "MON$CREATION_DATE",
             "MON$ALLOCATED_PAGES", "MON$STAT_ID", "MON$BACKUP_STATE",
             "MON$CRYPT_PAGE", "MON$OWNER", "MON$SEC_DATABASE"},
            {DataType::TEXT, DataType::INT64, DataType::INT16, DataType::INT16,
             DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT16,
             DataType::INT16, DataType::INT64, DataType::INT16,
             DataType::INT16, DataType::INT16, DataType::TIMESTAMP,
             DataType::INT64, DataType::INT64, DataType::INT16,
             DataType::INT64, DataType::TEXT, DataType::TEXT},
            "Maps sys.performance/sys.transactions to Firebird MON$DATABASE"
        });

        // MON$ATTACHMENTS - Active sessions
        views.push_back({
            "MON$ATTACHMENTS",
            R"SQL(
                SELECT
                    s.connection_id AS MON$ATTACHMENT_ID,
                    NULL AS MON$SERVER_PID,
                    CASE s.state
                        WHEN 'active' THEN 1
                        WHEN 'waiting' THEN 2
                        WHEN 'idle_in_txn' THEN 1
                        WHEN 'idle' THEN 0
                        ELSE 0
                    END AS MON$STATE,
                    s.database_name AS MON$ATTACHMENT_NAME,
                    s.user_name AS MON$USER,
                    s.role_name AS MON$ROLE,
                    s.protocol AS MON$REMOTE_PROTOCOL,
                    s.client_addr AS MON$REMOTE_ADDRESS,
                    NULL AS MON$REMOTE_PID,
                    4 AS MON$CHARACTER_SET_ID,
                    s.connected_at AS MON$TIMESTAMP,
                    1 AS MON$GARBAGE_COLLECTION,
                    NULL AS MON$REMOTE_PROCESS,
                    s.connection_id AS MON$STAT_ID,
                    'ScratchBird' AS MON$CLIENT_VERSION,
                    'ScratchBird' AS MON$REMOTE_VERSION
                FROM sys.sessions s
            )SQL",
            {"MON$ATTACHMENT_ID", "MON$SERVER_PID", "MON$STATE", "MON$ATTACHMENT_NAME",
             "MON$USER", "MON$ROLE", "MON$REMOTE_PROTOCOL", "MON$REMOTE_ADDRESS",
             "MON$REMOTE_PID", "MON$CHARACTER_SET_ID", "MON$TIMESTAMP", "MON$GARBAGE_COLLECTION",
             "MON$REMOTE_PROCESS", "MON$STAT_ID", "MON$CLIENT_VERSION", "MON$REMOTE_VERSION"},
            {DataType::INT64, DataType::INT64, DataType::INT16, DataType::TEXT,
             DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT,
             DataType::INT64, DataType::INT16, DataType::TIMESTAMP, DataType::INT16,
             DataType::TEXT, DataType::INT64, DataType::TEXT, DataType::TEXT},
            "Maps sys.sessions to Firebird MON$ATTACHMENTS"
        });

        // MON$TRANSACTIONS - Active transactions
        views.push_back({
            "MON$TRANSACTIONS",
            R"SQL(
                SELECT
                    t.transaction_id AS MON$TRANSACTION_ID,
                    s.connection_id AS MON$ATTACHMENT_ID,
                    CASE t.state
                        WHEN 'active' THEN 1
                        WHEN 'waiting' THEN 2
                        WHEN 'committed' THEN 3
                        WHEN 'rolledback' THEN 4
                        ELSE 0
                    END AS MON$STATE,
                    t.start_time AS MON$TIMESTAMP,
                    t.transaction_id AS MON$TOP_TRANSACTION,
                    COALESCE((SELECT MIN(transaction_id) FROM sys.transactions), 0) AS MON$OLDEST_TRANSACTION,
                    COALESCE((SELECT MIN(transaction_id) FROM sys.transactions
                              WHERE state = 'active'), 0) AS MON$OLDEST_ACTIVE,
                    CASE t.isolation_level
                        WHEN 'read_committed' THEN 0
                        WHEN 'repeatable_read' THEN 1
                        WHEN 'serializable' THEN 2
                        ELSE 0
                    END AS MON$ISOLATION_MODE,
                    NULL AS MON$LOCK_TIMEOUT,
                    CASE t.read_only WHEN true THEN 1 ELSE 0 END AS MON$READ_ONLY,
                    0 AS MON$AUTO_COMMIT,
                    1 AS MON$AUTO_UNDO,
                    t.transaction_id AS MON$STAT_ID
                FROM sys.transactions t
                LEFT JOIN sys.sessions s ON s.session_id = t.session_id
            )SQL",
            {"MON$TRANSACTION_ID", "MON$ATTACHMENT_ID", "MON$STATE", "MON$TIMESTAMP",
             "MON$TOP_TRANSACTION", "MON$OLDEST_TRANSACTION", "MON$OLDEST_ACTIVE",
             "MON$ISOLATION_MODE", "MON$LOCK_TIMEOUT", "MON$READ_ONLY",
             "MON$AUTO_COMMIT", "MON$AUTO_UNDO", "MON$STAT_ID"},
            {DataType::INT64, DataType::INT64, DataType::INT16, DataType::TIMESTAMP,
             DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT16, DataType::INT64, DataType::INT16,
             DataType::INT16, DataType::INT16, DataType::INT64},
            "Maps sys.transactions to Firebird MON$TRANSACTIONS"
        });

        // MON$STATEMENTS - Active statements
        views.push_back({
            "MON$STATEMENTS",
            R"SQL(
                SELECT
                    st.statement_id AS MON$STATEMENT_ID,
                    s.connection_id AS MON$ATTACHMENT_ID,
                    st.transaction_id AS MON$TRANSACTION_ID,
                    CASE st.state
                        WHEN 'running' THEN 1
                        WHEN 'waiting' THEN 2
                        WHEN 'idle' THEN 0
                        ELSE 0
                    END AS MON$STATE,
                    st.start_time AS MON$TIMESTAMP,
                    st.sql_text AS MON$SQL_TEXT,
                    st.statement_id AS MON$STAT_ID
                FROM sys.statements st
                LEFT JOIN sys.sessions s ON s.session_id = st.session_id
            )SQL",
            {"MON$STATEMENT_ID", "MON$ATTACHMENT_ID", "MON$TRANSACTION_ID",
             "MON$STATE", "MON$TIMESTAMP", "MON$SQL_TEXT", "MON$STAT_ID"},
            {DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT16, DataType::TIMESTAMP, DataType::TEXT, DataType::INT64},
            "Maps sys.statements to Firebird MON$STATEMENTS"
        });

        // MON$COMPILED_STATEMENTS - Compiled statement metadata
        views.push_back({
            "MON$COMPILED_STATEMENTS",
            R"SQL(
                SELECT
                    st.statement_id AS MON$COMPILED_STATEMENT_ID,
                    st.sql_text AS MON$SQL_TEXT,
                    NULL AS MON$EXPLAINED_PLAN,
                    NULL AS MON$OBJECT_NAME,
                    NULL AS MON$OBJECT_TYPE,
                    NULL AS MON$PACKAGE_NAME,
                    st.statement_id AS MON$STAT_ID
                FROM sys.statements st
            )SQL",
            {"MON$COMPILED_STATEMENT_ID", "MON$SQL_TEXT", "MON$EXPLAINED_PLAN",
             "MON$OBJECT_NAME", "MON$OBJECT_TYPE", "MON$PACKAGE_NAME", "MON$STAT_ID"},
            {DataType::INT64, DataType::TEXT, DataType::TEXT,
             DataType::TEXT, DataType::INT16, DataType::TEXT, DataType::INT64},
            "Maps sys.statements to Firebird MON$COMPILED_STATEMENTS"
        });

        // MON$LOCKS - Current locks
        views.push_back({
            "MON$LOCKS",
            R"SQL(
                SELECT
                    l.lock_id AS MON$LOCK_ID,
                    l.lock_type AS MON$LOCK_TYPE,
                    l.lock_mode AS MON$LOCK_MODE,
                    CASE l.lock_state
                        WHEN 'granted' THEN 1
                        WHEN 'waiting' THEN 2
                        ELSE 0
                    END AS MON$LOCK_STATE,
                    s.connection_id AS MON$ATTACHMENT_ID,
                    l.transaction_id AS MON$TRANSACTION_ID,
                    l.relation_name AS MON$OBJECT_NAME
                FROM sys.locks l
                LEFT JOIN sys.sessions s ON s.session_id = l.session_id
            )SQL",
            {"MON$LOCK_ID", "MON$LOCK_TYPE", "MON$LOCK_MODE", "MON$LOCK_STATE",
             "MON$ATTACHMENT_ID", "MON$TRANSACTION_ID", "MON$OBJECT_NAME"},
            {DataType::INT64, DataType::TEXT, DataType::TEXT, DataType::INT16,
             DataType::INT64, DataType::INT64, DataType::TEXT},
            "Maps sys.locks to Firebird MON$LOCKS"
        });

        // MON$IO_STATS - I/O statistics
        views.push_back({
            "MON$IO_STATS",
            R"SQL(
                SELECT
                    stat_id AS MON$STAT_ID,
                    stat_group AS MON$STAT_GROUP,
                    page_reads AS MON$PAGE_READS,
                    page_writes AS MON$PAGE_WRITES,
                    page_fetches AS MON$PAGE_FETCHES,
                    page_marks AS MON$PAGE_MARKS
                FROM sys.io_stats
            )SQL",
            {"MON$STAT_ID", "MON$STAT_GROUP", "MON$PAGE_READS", "MON$PAGE_WRITES",
             "MON$PAGE_FETCHES", "MON$PAGE_MARKS"},
            {DataType::INT64, DataType::INT16, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64},
            "Maps sys.io_stats to Firebird MON$IO_STATS"
        });

        // MON$TABLE_STATS - Table statistics
        views.push_back({
            "MON$TABLE_STATS",
            R"SQL(
                SELECT
                    0 AS MON$STAT_ID,
                    0 AS MON$STAT_GROUP,
                    table_name AS MON$TABLE_NAME,
                    0 AS MON$RECORD_STAT_ID
                FROM sys.table_stats
            )SQL",
            {"MON$STAT_ID", "MON$STAT_GROUP", "MON$TABLE_NAME", "MON$RECORD_STAT_ID"},
            {DataType::INT64, DataType::INT16, DataType::TEXT, DataType::INT64},
            "Maps sys.table_stats to Firebird MON$TABLE_STATS"
        });

        // MON$CALL_STACK - Stubbed to sys.statements
        views.push_back({
            "MON$CALL_STACK",
            R"SQL(
                SELECT
                    0 AS MON$CALL_ID,
                    statement_id AS MON$STATEMENT_ID,
                    NULL AS MON$CALLER_ID,
                    NULL AS MON$OBJECT_NAME,
                    NULL AS MON$OBJECT_TYPE,
                    start_time AS MON$TIMESTAMP,
                    NULL AS MON$SOURCE_LINE,
                    NULL AS MON$SOURCE_COLUMN,
                    statement_id AS MON$STAT_ID,
                    NULL AS MON$PACKAGE_NAME,
                    NULL AS MON$COMPILED_STATEMENT_ID
                FROM sys.statements
            )SQL",
            {"MON$CALL_ID", "MON$STATEMENT_ID", "MON$CALLER_ID",
             "MON$OBJECT_NAME", "MON$OBJECT_TYPE", "MON$TIMESTAMP",
             "MON$SOURCE_LINE", "MON$SOURCE_COLUMN", "MON$STAT_ID",
             "MON$PACKAGE_NAME", "MON$COMPILED_STATEMENT_ID"},
            {DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::TEXT, DataType::INT16, DataType::TIMESTAMP,
             DataType::INT32, DataType::INT32, DataType::INT64,
             DataType::TEXT, DataType::INT64},
            "Stub view for MON$CALL_STACK backed by sys.statements"
        });

        // MON$RECORD_STATS - Stubbed to sys.table_stats
        views.push_back({
            "MON$RECORD_STATS",
            R"SQL(
                SELECT
                    0 AS MON$STAT_ID,
                    0 AS MON$STAT_GROUP,
                    COALESCE(seq_rows_read, 0) AS MON$RECORD_SEQ_READS,
                    COALESCE(idx_rows_fetch, 0) AS MON$RECORD_IDX_READS,
                    COALESCE(rows_inserted, 0) AS MON$RECORD_INSERTS,
                    COALESCE(rows_updated, 0) AS MON$RECORD_UPDATES,
                    COALESCE(rows_deleted, 0) AS MON$RECORD_DELETES,
                    0 AS MON$RECORD_BACKOUTS,
                    0 AS MON$RECORD_PURGES,
                    0 AS MON$RECORD_EXPUNGES,
                    0 AS MON$RECORD_LOCKS,
                    0 AS MON$RECORD_WAITS,
                    0 AS MON$RECORD_CONFLICTS,
                    0 AS MON$BACKVERSION_READS,
                    0 AS MON$FRAGMENT_READS,
                    0 AS MON$RECORD_RPT_READS,
                    0 AS MON$RECORD_IMGC
                FROM sys.table_stats
            )SQL",
            {"MON$STAT_ID", "MON$STAT_GROUP", "MON$RECORD_SEQ_READS",
             "MON$RECORD_IDX_READS", "MON$RECORD_INSERTS", "MON$RECORD_UPDATES",
             "MON$RECORD_DELETES", "MON$RECORD_BACKOUTS", "MON$RECORD_PURGES",
             "MON$RECORD_EXPUNGES", "MON$RECORD_LOCKS", "MON$RECORD_WAITS",
             "MON$RECORD_CONFLICTS", "MON$BACKVERSION_READS", "MON$FRAGMENT_READS",
             "MON$RECORD_RPT_READS", "MON$RECORD_IMGC"},
            {DataType::INT64, DataType::INT16, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64},
            "Stub view for MON$RECORD_STATS backed by sys.table_stats"
        });

        // MON$MEMORY_USAGE - Stubbed to sys.performance
        views.push_back({
            "MON$MEMORY_USAGE",
            R"SQL(
                SELECT
                    1 AS MON$STAT_ID,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'memory_used_bytes' LIMIT 1), 0) AS MON$MEMORY_USED,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'memory_allocated_bytes' LIMIT 1), 0) AS MON$MEMORY_ALLOCATED
            )SQL",
            {"MON$STAT_ID", "MON$MEMORY_USED", "MON$MEMORY_ALLOCATED"},
            {DataType::INT64, DataType::INT64, DataType::INT64},
            "Stub view for MON$MEMORY_USAGE backed by sys.performance"
        });

        // MON$CONTEXT_VARIABLES - Stubbed to sys.sessions
        views.push_back({
            "MON$CONTEXT_VARIABLES",
            R"SQL(
                SELECT
                    attachment_id AS MON$ATTACHMENT_ID,
                    transaction_id AS MON$TRANSACTION_ID,
                    variable_name AS MON$VARIABLE_NAME,
                    variable_value AS MON$VARIABLE_VALUE
                FROM sys.context_variables
            )SQL",
            {"MON$ATTACHMENT_ID", "MON$TRANSACTION_ID", "MON$VARIABLE_NAME", "MON$VARIABLE_VALUE"},
            {DataType::INT64, DataType::INT64, DataType::TEXT, DataType::TEXT},
            "MON$CONTEXT_VARIABLES backed by sys.context_variables"
        });

        for (auto& view : views) {
            view.source_query = quoteFirebirdAliasIdentifiers(std::move(view.source_query));
        }

        return views;
    }

    /**
     * PostgreSQL pg_catalog view definitions (on-demand emulation)
     */
    std::vector<EmulatedViewDefinition> getPostgreSQLViews() {
        std::vector<EmulatedViewDefinition> views;

        // For PostgreSQL, we use the static PgCatalogHandler for most tables
        // On-demand emulation is mainly for remote PostgreSQL databases
        // The view definitions here are for when we need to create actual views
        // in an emulated database schema

        views.push_back({
            "pg_tables",
            R"SQL(
                SELECT
                    s.schema_name AS schemaname,
                    t.table_name AS tablename,
                    s.owner_name AS tableowner,
                    NULL AS tablespace,
                    t.has_indexes AS hasindexes,
                    t.has_triggers AS hastriggers,
                    FALSE AS hasrules
                FROM sys.catalog.tables t
                JOIN sys.catalog.schemas s ON t.schema_id = s.schema_id
                WHERE t.schema_id = {schema_id}
            )SQL",
            {"schemaname", "tablename", "tableowner", "tablespace",
             "hasindexes", "hastriggers", "hasrules"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
             DataType::VARCHAR, DataType::BOOLEAN, DataType::BOOLEAN, DataType::BOOLEAN},
            "PostgreSQL pg_tables view"
        });

        views.push_back({
            "pg_views",
            R"SQL(
                SELECT
                    s.schema_name AS schemaname,
                    v.view_name AS viewname,
                    s.owner_name AS viewowner,
                    v.definition AS definition
                FROM sys.catalog.views v
                JOIN sys.catalog.schemas s ON v.schema_id = s.schema_id
                WHERE v.schema_id = {schema_id}
            )SQL",
            {"schemaname", "viewname", "viewowner", "definition"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR},
            "PostgreSQL pg_views view"
        });

        views.push_back({
            "pg_stat_activity",
            R"SQL(
                SELECT
                    NULL AS datid,
                    s.database_name AS datname,
                    CAST(s.connection_id AS INT32) AS pid,
                    NULL AS usesysid,
                    s.user_name AS usename,
                    NULL AS application_name,
                    s.client_addr AS client_addr,
                    s.client_port AS client_port,
                    s.connected_at AS backend_start,
                    t.start_time AS xact_start,
                    st.start_time AS query_start,
                    s.last_activity_at AS state_change,
                    CASE s.state
                        WHEN 'idle' THEN 'idle'
                        WHEN 'active' THEN 'active'
                        WHEN 'idle_in_txn' THEN 'idle in transaction'
                        WHEN 'waiting' THEN 'active'
                        ELSE NULL
                    END AS state,
                    CASE WHEN s.wait_event IS NULL THEN NULL ELSE 'Lock' END AS wait_event_type,
                    s.wait_event AS wait_event,
                    t.transaction_id AS backend_xid,
                    NULL AS backend_xmin,
                    COALESCE(st.sql_text, s.current_query) AS query,
                    'client backend' AS backend_type
                FROM sys.sessions s
                LEFT JOIN sys.transactions t ON t.session_id = s.session_id
                LEFT JOIN sys.statements st ON st.session_id = s.session_id
            )SQL",
            {"datid", "datname", "pid", "usesysid", "usename", "application_name",
             "client_addr", "client_port", "backend_start", "xact_start", "query_start",
             "state_change", "state", "wait_event_type", "wait_event", "backend_xid",
             "backend_xmin", "query", "backend_type"},
            {DataType::INT32, DataType::TEXT, DataType::INT32, DataType::INT32, DataType::TEXT,
             DataType::TEXT, DataType::TEXT, DataType::INT32, DataType::TIMESTAMP,
             DataType::TIMESTAMP, DataType::TIMESTAMP, DataType::TIMESTAMP, DataType::TEXT,
             DataType::TEXT, DataType::TEXT, DataType::INT64, DataType::INT64, DataType::TEXT,
             DataType::TEXT},
            "PostgreSQL pg_stat_activity from sys.sessions/sys.statements/sys.transactions"
        });

        views.push_back({
            "pg_locks",
            R"SQL(
                SELECT
                    l.lock_type AS locktype,
                    NULL AS database,
                    NULL AS relation,
                    l.page AS page,
                    l.tuple AS tuple,
                    l.virtual_xid AS virtualxid,
                    l.transaction_id AS transactionid,
                    NULL AS classid,
                    NULL AS objid,
                    NULL AS objsubid,
                    CAST(l.session_id AS TEXT) AS virtualtransaction,
                    s.connection_id AS pid,
                    l.lock_mode AS mode,
                    l.granted AS granted,
                    FALSE AS fastpath
                FROM sys.locks l
                LEFT JOIN sys.sessions s ON s.session_id = l.session_id
            )SQL",
            {"locktype", "database", "relation", "page", "tuple", "virtualxid",
             "transactionid", "classid", "objid", "objsubid", "virtualtransaction",
             "pid", "mode", "granted", "fastpath"},
            {DataType::TEXT, DataType::INT32, DataType::INT32, DataType::INT64, DataType::INT64,
             DataType::TEXT, DataType::INT64, DataType::INT32, DataType::INT32, DataType::INT32,
             DataType::TEXT, DataType::INT32, DataType::TEXT, DataType::BOOLEAN, DataType::BOOLEAN},
            "PostgreSQL pg_locks from sys.locks"
        });

        views.push_back({
            "pg_stat_database",
            R"SQL(
                SELECT
                    NULL AS datid,
                    '{database_name}' AS datname,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'connections_active' LIMIT 1), 0) AS numbackends,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'transactions_committed_total' LIMIT 1), 0) AS xact_commit,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'transactions_rolled_back_total' LIMIT 1), 0) AS xact_rollback,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'buffer_pool_reads_total{source=disk}' LIMIT 1), 0) AS blks_read,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'buffer_pool_reads_total{source=cache}' LIMIT 1), 0) AS blks_hit,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'query_rows_returned_total' LIMIT 1), 0) AS tup_returned,
                    NULL AS tup_fetched,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'query_rows_affected_total{type=insert}' LIMIT 1), 0) AS tup_inserted,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'query_rows_affected_total{type=update}' LIMIT 1), 0) AS tup_updated,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'query_rows_affected_total{type=delete}' LIMIT 1), 0) AS tup_deleted,
                    0 AS conflicts,
                    0 AS temp_files,
                    0 AS temp_bytes,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'deadlocks_total' LIMIT 1), 0) AS deadlocks,
                    NULL AS blk_read_time,
                    NULL AS blk_write_time,
                    NULL AS stats_reset
            )SQL",
            {"datid", "datname", "numbackends", "xact_commit", "xact_rollback",
             "blks_read", "blks_hit", "tup_returned", "tup_fetched", "tup_inserted",
             "tup_updated", "tup_deleted", "conflicts", "temp_files", "temp_bytes",
             "deadlocks", "blk_read_time", "blk_write_time", "stats_reset"},
            {DataType::INT32, DataType::TEXT, DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::FLOAT64, DataType::FLOAT64, DataType::TIMESTAMP},
            "PostgreSQL pg_stat_database from sys.performance"
        });

        views.push_back({
            "pg_stat_bgwriter",
            R"SQL(
                SELECT
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'buffer_pool_writes_total' LIMIT 1), 0) AS buffers_clean,
                    NULL AS maxwritten_clean,
                    COALESCE((SELECT CAST(value AS BIGINT) FROM sys.performance
                              WHERE metric = 'page_buffers' LIMIT 1), 0) AS buffers_alloc,
                    NULL AS stats_reset
            )SQL",
            {"buffers_clean", "maxwritten_clean", "buffers_alloc", "stats_reset"},
            {DataType::INT64, DataType::INT64, DataType::INT64, DataType::TIMESTAMP},
            "PostgreSQL pg_stat_bgwriter from sys.performance"
        });

        views.push_back({
            "pg_stat_all_tables",
            R"SQL(
                SELECT
                    NULL AS relid,
                    schema_name AS schemaname,
                    table_name AS relname,
                    seq_scan_count AS seq_scan,
                    last_seq_scan_at AS last_seq_scan,
                    seq_rows_read AS seq_tup_read,
                    idx_scan_count AS idx_scan,
                    last_idx_scan_at AS last_idx_scan,
                    idx_rows_fetch AS idx_tup_fetch,
                    rows_inserted AS n_tup_ins,
                    rows_updated AS n_tup_upd,
                    rows_deleted AS n_tup_del,
                    rows_hot_updated AS n_tup_hot_upd,
                    rows_newpage_updated AS n_tup_newpage_upd,
                    live_rows_estimate AS n_live_tup,
                    dead_rows_estimate AS n_dead_tup,
                    mod_since_analyze AS n_mod_since_analyze,
                    ins_since_vacuum AS n_ins_since_vacuum,
                    last_vacuum_at AS last_vacuum,
                    last_autovacuum_at AS last_autovacuum,
                    last_analyze_at AS last_analyze,
                    last_autoanalyze_at AS last_autoanalyze,
                    vacuum_count AS vacuum_count,
                    autovacuum_count AS autovacuum_count,
                    analyze_count AS analyze_count,
                    autoanalyze_count AS autoanalyze_count,
                    total_vacuum_time_ms / 1000.0 AS total_vacuum_time,
                    total_autovacuum_time_ms / 1000.0 AS total_autovacuum_time,
                    total_analyze_time_ms / 1000.0 AS total_analyze_time,
                    total_autoanalyze_time_ms / 1000.0 AS total_autoanalyze_time
                FROM sys.table_stats
            )SQL",
            {"relid", "schemaname", "relname", "seq_scan", "last_seq_scan",
             "seq_tup_read", "idx_scan", "last_idx_scan", "idx_tup_fetch",
             "n_tup_ins", "n_tup_upd", "n_tup_del", "n_tup_hot_upd",
             "n_tup_newpage_upd", "n_live_tup", "n_dead_tup",
             "n_mod_since_analyze", "n_ins_since_vacuum", "last_vacuum",
             "last_autovacuum", "last_analyze", "last_autoanalyze",
             "vacuum_count", "autovacuum_count", "analyze_count",
             "autoanalyze_count", "total_vacuum_time", "total_autovacuum_time",
             "total_analyze_time", "total_autoanalyze_time"},
            {DataType::INT32, DataType::TEXT, DataType::TEXT, DataType::INT64,
             DataType::TIMESTAMP, DataType::INT64, DataType::INT64, DataType::TIMESTAMP,
             DataType::INT64, DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::TIMESTAMP, DataType::TIMESTAMP,
             DataType::TIMESTAMP, DataType::TIMESTAMP, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::INT64, DataType::FLOAT64, DataType::FLOAT64,
             DataType::FLOAT64, DataType::FLOAT64},
            "PostgreSQL pg_stat_all_tables from sys.table_stats"
        });

        return views;
    }

    /**
     * MySQL system table view definitions
     */
    std::vector<EmulatedViewDefinition> getMySQLViews() {
        std::vector<EmulatedViewDefinition> views;

        // MySQL-specific on-demand views (beyond what MySQLCatalogHandler provides)
        views.push_back({
            "TABLES",
            R"SQL(
                SELECT
                    'scratchbird' AS TABLE_CATALOG,
                    s.schema_name AS TABLE_SCHEMA,
                    t.table_name AS TABLE_NAME,
                    CASE WHEN t.is_temp THEN 'LOCAL TEMPORARY' ELSE 'BASE TABLE' END AS TABLE_TYPE,
                    'ScratchBird' AS ENGINE,
                    10 AS VERSION,
                    'Dynamic' AS ROW_FORMAT,
                    t.row_count AS TABLE_ROWS,
                    0 AS AVG_ROW_LENGTH,
                    0 AS DATA_LENGTH,
                    0 AS MAX_DATA_LENGTH,
                    0 AS INDEX_LENGTH,
                    0 AS DATA_FREE,
                    NULL AS AUTO_INCREMENT,
                    NULL AS CREATE_TIME,
                    NULL AS UPDATE_TIME,
                    NULL AS CHECK_TIME,
                    'utf8mb4_general_ci' AS TABLE_COLLATION,
                    NULL AS CHECKSUM,
                    '' AS CREATE_OPTIONS,
                    t.description AS TABLE_COMMENT
                FROM sys.catalog.tables t
                JOIN sys.catalog.schemas s ON t.schema_id = s.schema_id
                WHERE t.schema_id = {schema_id}
            )SQL",
            {"TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME", "TABLE_TYPE",
             "ENGINE", "TABLE_ROWS", "TABLE_COMMENT"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
             DataType::VARCHAR, DataType::INT64, DataType::VARCHAR},
            "MySQL INFORMATION_SCHEMA.TABLES view"
        });

        views.push_back({
            "PROCESSLIST",
            R"SQL(
                SELECT
                    s.connection_id AS ID,
                    s.user_name AS USER,
                    CASE
                        WHEN s.client_addr IS NULL THEN NULL
                        WHEN s.client_port IS NULL THEN s.client_addr
                        ELSE s.client_addr || ':' || CAST(s.client_port AS TEXT)
                    END AS HOST,
                    s.database_name AS DB,
                    CASE s.state
                        WHEN 'idle' THEN 'Sleep'
                        WHEN 'idle_in_txn' THEN 'Sleep'
                        WHEN 'active' THEN 'Query'
                        WHEN 'waiting' THEN 'Query'
                        ELSE 'Sleep'
                    END AS COMMAND,
                    NULL AS TIME,
                    s.state AS STATE,
                    COALESCE(st.sql_text, s.current_query) AS INFO
                FROM sys.sessions s
                LEFT JOIN sys.statements st ON st.session_id = s.session_id
            )SQL",
            {"ID", "USER", "HOST", "DB", "COMMAND", "TIME", "STATE", "INFO"},
            {DataType::INT64, DataType::TEXT, DataType::TEXT, DataType::TEXT,
             DataType::TEXT, DataType::INT64, DataType::TEXT, DataType::TEXT},
            "MySQL information_schema.PROCESSLIST from sys.sessions/sys.statements"
        });

        views.push_back({
            "data_locks",
            R"SQL(
                SELECT
                    'SCRATCHBIRD' AS ENGINE,
                    l.lock_id AS LOCK_ID,
                    CAST(l.lock_id AS TEXT) AS ENGINE_LOCK_ID,
                    l.transaction_id AS ENGINE_TRANSACTION_ID,
                    s.connection_id AS THREAD_ID,
                    NULL AS EVENT_ID,
                    NULL AS OBJECT_SCHEMA,
                    l.relation_name AS OBJECT_NAME,
                    NULL AS PARTITION_NAME,
                    NULL AS SUBPARTITION_NAME,
                    NULL AS INDEX_NAME,
                    NULL AS OBJECT_INSTANCE_BEGIN,
                    l.lock_type AS LOCK_TYPE,
                    l.lock_mode AS LOCK_MODE,
                    l.lock_state AS LOCK_STATUS,
                    COALESCE(CAST(l.tuple AS TEXT), CAST(l.page AS TEXT)) AS LOCK_DATA
                FROM sys.locks l
                LEFT JOIN sys.sessions s ON s.session_id = l.session_id
            )SQL",
            {"ENGINE", "LOCK_ID", "ENGINE_LOCK_ID", "ENGINE_TRANSACTION_ID", "THREAD_ID",
             "EVENT_ID", "OBJECT_SCHEMA", "OBJECT_NAME", "PARTITION_NAME", "SUBPARTITION_NAME",
             "INDEX_NAME", "OBJECT_INSTANCE_BEGIN", "LOCK_TYPE", "LOCK_MODE",
             "LOCK_STATUS", "LOCK_DATA"},
            {DataType::TEXT, DataType::INT64, DataType::TEXT, DataType::INT64, DataType::INT64,
             DataType::INT64, DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT,
             DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT, DataType::TEXT,
             DataType::TEXT},
            "MySQL performance_schema.data_locks from sys.locks"
        });

        views.push_back({
            "global_status",
            R"SQL(
                SELECT
                    CASE p.metric
                        WHEN 'connections_total' THEN 'Connections'
                        WHEN 'connections_active' THEN 'Threads_connected'
                        WHEN 'query_currently_running' THEN 'Threads_running'
                        WHEN 'queries_total{type=select}' THEN 'Com_select'
                        WHEN 'queries_total{type=insert}' THEN 'Com_insert'
                        WHEN 'queries_total{type=update}' THEN 'Com_update'
                        WHEN 'queries_total{type=delete}' THEN 'Com_delete'
                        WHEN 'buffer_pool_reads_total{source=cache}' THEN 'Innodb_buffer_pool_read_requests'
                        WHEN 'buffer_pool_reads_total{source=disk}' THEN 'Innodb_buffer_pool_reads'
                        WHEN 'lock_waits_total' THEN 'Innodb_row_lock_waits'
                        WHEN 'uptime_seconds' THEN 'Uptime'
                        ELSE p.metric
                    END AS VARIABLE_NAME,
                    CAST(p.value AS BIGINT) AS VARIABLE_VALUE
                FROM sys.performance p
                WHERE p.metric IN (
                    'connections_total',
                    'connections_active',
                    'query_currently_running',
                    'queries_total{type=select}',
                    'queries_total{type=insert}',
                    'queries_total{type=update}',
                    'queries_total{type=delete}',
                    'buffer_pool_reads_total{source=cache}',
                    'buffer_pool_reads_total{source=disk}',
                    'lock_waits_total',
                    'uptime_seconds'
                )
            )SQL",
            {"VARIABLE_NAME", "VARIABLE_VALUE"},
            {DataType::TEXT, DataType::INT64},
            "MySQL performance_schema.global_status from sys.performance"
        });

        return views;
    }

    /**
     * SQL Server system table view definitions
     */
    std::vector<EmulatedViewDefinition> getMSSQLViews() {
        std::vector<EmulatedViewDefinition> views;

        // SQL Server-specific on-demand views
        views.push_back({
            "sysobjects",
            R"SQL(
                SELECT
                    t.table_name AS name,
                    CAST(t.table_id AS INTEGER) AS id,
                    0 AS xtype,
                    NULL AS uid,
                    NULL AS info,
                    0 AS status,
                    0 AS base_schema_ver,
                    0 AS replinfo,
                    0 AS parent_obj,
                    NULL AS crdate,
                    0 AS ftcatid,
                    0 AS schema_ver,
                    0 AS stats_schema_ver,
                    'U' AS type,
                    0 AS userstat,
                    0 AS sysstat,
                    0 AS indexdel,
                    NULL AS refdate,
                    0 AS version,
                    0 AS deltrig,
                    0 AS instrig,
                    0 AS updtrig,
                    0 AS seltrig,
                    0 AS category,
                    0 AS cache
                FROM sys.catalog.tables t
                WHERE t.schema_id = {schema_id}
            )SQL",
            {"name", "id", "type"},
            {DataType::VARCHAR, DataType::INT32, DataType::CHAR},
            "SQL Server sysobjects compatibility view"
        });

        return views;
    }
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Create and initialize an EmulationViewGenerator instance
 */
inline std::unique_ptr<EmulationViewGenerator> createEmulationViewGenerator(
    CatalogManager* catalog) {
    return std::make_unique<EmulationViewGenerator>(catalog);
}

} // namespace scratchbird::catalog
