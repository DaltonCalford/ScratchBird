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
struct EmulatedViewDefinition {
    std::string view_name;              // e.g., "RDB$RELATIONS"
    std::string source_query;           // SQL that maps to internal catalog
    std::vector<std::string> columns;   // Column names
    std::vector<DataType> column_types; // Column types
    std::string description;            // Documentation
};

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
     * Creates the schema path /remote/emulated/{protocol}/{server_name}/
     *
     * @param server_name Name of the emulation server
     * @param protocol Protocol type being emulated
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status generateServerSchema(const std::string& server_name,
                                ProtocolType protocol,
                                ErrorContext* ctx = nullptr) {
        // Build schema path: remote.emulated.{protocol}.{server}
        std::string protocolName = protocolTypeToString(protocol);
        std::string schemaPath = "remote.emulated." + protocolName + "." + server_name;
        ID schema_id;
        return catalog_->createSchemaPath(schemaPath, SchemaType::REMOTE_EMULATED,
                                          schema_id, ctx);
    }

    /**
     * Generate all emulated views for a database
     *
     * Creates the database schema and all protocol-specific system views
     * as virtual tables.
     *
     * @param server_name Name of the emulation server
     * @param database_name Name of the database to emulate
     * @param protocol Protocol type being emulated
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status generateEmulatedViews(const std::string& server_name,
                                 const std::string& database_name,
                                 ProtocolType protocol,
                                 ErrorContext* ctx = nullptr) {
        // Build database schema path
        std::string protocolName = protocolTypeToString(protocol);
        std::string schemaPath = "remote.emulated." + protocolName + "." +
                                  server_name + "." + database_name;

        ID schema_id;
        Status s = catalog_->createSchemaPath(schemaPath, SchemaType::REMOTE_EMULATED,
                                              schema_id, ctx);
        if (s != Status::OK) return s;

        // Get view definitions for this protocol
        std::vector<EmulatedViewDefinition> views = getViewDefinitions(protocol);

        // Create each view
        Status first_error = Status::OK;
        for (const auto& viewDef : views) {
            s = createEmulatedView(schema_id, viewDef, server_name, database_name, ctx);
            if (s != Status::OK && first_error == Status::OK) {
                first_error = s;
                // Log error but continue with other views
                // In production, might want to rollback all
            }
        }

        return first_error;
    }

    /**
     * Drop all emulated views for a database
     *
     * @param server_name Name of the emulation server
     * @param database_name Name of the database
     * @param protocol Protocol type
     * @param ctx Error context
     * @return Status::OK on success
     */
    Status dropEmulatedViews(const std::string& server_name,
                             const std::string& database_name,
                             ProtocolType protocol,
                             ErrorContext* ctx = nullptr) {
        std::string protocolName = protocolTypeToString(protocol);
        std::string schemaPath = "remote.emulated." + protocolName + "." +
                                  server_name + "." + database_name;

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

    /**
     * Create a single emulated view
     */
    Status createEmulatedView(const ID& schema_id,
                              const EmulatedViewDefinition& viewDef,
                              const std::string& server_name,
                              const std::string& database_name,
                              ErrorContext* ctx) {
        std::string query = renderViewQuery(viewDef, schema_id, server_name, database_name);
        return catalog_->createView(schema_id, viewDef.view_name, query,
                                    true, false, false, viewDef.columns, ID{}, ctx);
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
                                       const std::string& server_name,
                                       const std::string& database_name) {
        std::string query = viewDef.source_query;
        std::string schema_literal = "UUID '" + schema_id.toString() + "'";
        replaceAll(query, "{schema_id}", schema_literal);
        replaceAll(query, "{server_name}", server_name);
        replaceAll(query, "{database_name}", database_name);
        return query;
    }

    // ========================================================================
    // Protocol-Specific View Definitions
    // ========================================================================

    /**
     * Firebird RDB$* system table definitions
     */
    std::vector<EmulatedViewDefinition> getFirebirdViews() {
        std::vector<EmulatedViewDefinition> views;

        // RDB$RELATIONS - Tables
        views.push_back({
            "RDB$RELATIONS",
            R"SQL(
                SELECT
                    t.table_name AS RDB$RELATION_NAME,
                    CAST(t.table_id AS INTEGER) AS RDB$RELATION_ID,
                    CASE WHEN s.schema_name = 'sys' THEN 1 ELSE 0 END AS RDB$SYSTEM_FLAG,
                    s.owner_name AS RDB$OWNER_NAME,
                    t.description AS RDB$DESCRIPTION,
                    0 AS RDB$VIEW_BLR,
                    NULL AS RDB$VIEW_SOURCE,
                    t.row_count AS RDB$RELATION_COUNTS,
                    0 AS RDB$FORMAT,
                    0 AS RDB$FIELD_ID,
                    0 AS RDB$FLAGS,
                    0 AS RDB$RELATION_TYPE,
                    NULL AS RDB$EXTERNAL_FILE,
                    NULL AS RDB$EXTERNAL_DESCRIPTION,
                    NULL AS RDB$SECURITY_CLASS
                FROM sys.catalog.tables t
                JOIN sys.catalog.schemas s ON t.schema_id = s.schema_id
                WHERE t.schema_id = {schema_id}
            )SQL",
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
            R"SQL(
                SELECT DISTINCT
                    c.column_name AS RDB$FIELD_NAME,
                    0 AS RDB$QUERY_NAME,
                    NULL AS RDB$VALIDATION_BLR,
                    NULL AS RDB$VALIDATION_SOURCE,
                    NULL AS RDB$COMPUTED_BLR,
                    c.computed_expression AS RDB$COMPUTED_SOURCE,
                    c.default_value AS RDB$DEFAULT_VALUE,
                    NULL AS RDB$DEFAULT_SOURCE,
                    c.max_length AS RDB$FIELD_LENGTH,
                    c.scale AS RDB$FIELD_SCALE,
                    CASE c.data_type
                        WHEN 'VARCHAR' THEN 37
                        WHEN 'CHAR' THEN 14
                        WHEN 'INT32' THEN 8
                        WHEN 'INT64' THEN 16
                        WHEN 'INT16' THEN 7
                        WHEN 'FLOAT32' THEN 10
                        WHEN 'FLOAT64' THEN 27
                        WHEN 'DECIMAL' THEN 16
                        WHEN 'DATE' THEN 12
                        WHEN 'TIME' THEN 13
                        WHEN 'TIMESTAMP' THEN 35
                        WHEN 'BLOB' THEN 261
                        WHEN 'BOOLEAN' THEN 23
                        ELSE 37
                    END AS RDB$FIELD_TYPE,
                    CASE c.data_type
                        WHEN 'VARCHAR' THEN 0
                        WHEN 'CHAR' THEN 1
                        WHEN 'INT32' THEN 0
                        WHEN 'INT64' THEN 1
                        WHEN 'INT16' THEN 0
                        WHEN 'FLOAT32' THEN 0
                        WHEN 'FLOAT64' THEN 0
                        WHEN 'DECIMAL' THEN 2
                        WHEN 'DATE' THEN 0
                        WHEN 'TIME' THEN 0
                        WHEN 'TIMESTAMP' THEN 0
                        WHEN 'BLOB' THEN 0
                        WHEN 'BOOLEAN' THEN 0
                        ELSE 0
                    END AS RDB$FIELD_SUB_TYPE,
                    NULL AS RDB$MISSING_VALUE,
                    NULL AS RDB$MISSING_SOURCE,
                    c.description AS RDB$DESCRIPTION,
                    0 AS RDB$SYSTEM_FLAG,
                    NULL AS RDB$QUERY_HEADER,
                    0 AS RDB$SEGMENT_LENGTH,
                    NULL AS RDB$EDIT_STRING,
                    0 AS RDB$EXTERNAL_LENGTH,
                    0 AS RDB$EXTERNAL_SCALE,
                    0 AS RDB$EXTERNAL_TYPE,
                    0 AS RDB$DIMENSIONS,
                    CASE c.nullable WHEN 1 THEN 0 ELSE 1 END AS RDB$NULL_FLAG,
                    c.max_length * 4 AS RDB$CHARACTER_LENGTH,
                    NULL AS RDB$COLLATION_ID,
                    NULL AS RDB$CHARACTER_SET_ID,
                    c.precision AS RDB$FIELD_PRECISION,
                    NULL AS RDB$SECURITY_CLASS,
                    NULL AS RDB$OWNER_NAME
                FROM sys.catalog.columns c
                JOIN sys.catalog.tables t ON c.table_id = t.table_id
                WHERE t.schema_id = {schema_id}
            )SQL",
            {"RDB$FIELD_NAME", "RDB$QUERY_NAME", "RDB$FIELD_TYPE",
             "RDB$FIELD_LENGTH", "RDB$FIELD_SCALE", "RDB$NULL_FLAG"},
            {DataType::VARCHAR, DataType::INT16, DataType::INT16,
             DataType::INT32, DataType::INT32, DataType::INT16},
            "Maps columns to Firebird RDB$FIELDS format"
        });

        // RDB$RELATION_FIELDS - Relation-specific field definitions
        views.push_back({
            "RDB$RELATION_FIELDS",
            R"SQL(
                SELECT
                    t.table_name AS RDB$RELATION_NAME,
                    c.column_name AS RDB$FIELD_NAME,
                    c.column_name AS RDB$FIELD_SOURCE,
                    NULL AS RDB$QUERY_NAME,
                    NULL AS RDB$BASE_FIELD,
                    NULL AS RDB$EDIT_STRING,
                    c.ordinal AS RDB$FIELD_POSITION,
                    NULL AS RDB$QUERY_HEADER,
                    0 AS RDB$UPDATE_FLAG,
                    c.ordinal AS RDB$FIELD_ID,
                    NULL AS RDB$VIEW_CONTEXT,
                    c.description AS RDB$DESCRIPTION,
                    c.default_value AS RDB$DEFAULT_VALUE,
                    NULL AS RDB$DEFAULT_SOURCE,
                    0 AS RDB$SYSTEM_FLAG,
                    NULL AS RDB$SECURITY_CLASS,
                    NULL AS RDB$COMPLEX_NAME,
                    CASE c.nullable WHEN 1 THEN 0 ELSE 1 END AS RDB$NULL_FLAG,
                    NULL AS RDB$COLLATION_ID,
                    c.is_identity AS RDB$GENERATOR_NAME,
                    NULL AS RDB$IDENTITY_TYPE
                FROM sys.catalog.columns c
                JOIN sys.catalog.tables t ON c.table_id = t.table_id
                WHERE t.schema_id = {schema_id}
            )SQL",
            {"RDB$RELATION_NAME", "RDB$FIELD_NAME", "RDB$FIELD_SOURCE",
             "RDB$FIELD_POSITION", "RDB$NULL_FLAG"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
             DataType::INT16, DataType::INT16},
            "Maps table columns to Firebird RDB$RELATION_FIELDS format"
        });

        // RDB$INDICES - Indexes
        views.push_back({
            "RDB$INDICES",
            R"SQL(
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
                FROM sys.catalog.indexes i
                JOIN sys.catalog.tables t ON i.table_id = t.table_id
                WHERE t.schema_id = {schema_id}
            )SQL",
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
                    p.procedure_name AS RDB$PROCEDURE_NAME,
                    CAST(p.procedure_id AS INTEGER) AS RDB$PROCEDURE_ID,
                    ARRAY_LENGTH(p.parameters) AS RDB$PROCEDURE_INPUTS,
                    0 AS RDB$PROCEDURE_OUTPUTS,
                    p.description AS RDB$DESCRIPTION,
                    NULL AS RDB$PROCEDURE_BLR,
                    p.body AS RDB$PROCEDURE_SOURCE,
                    NULL AS RDB$SECURITY_CLASS,
                    p.owner_name AS RDB$OWNER_NAME,
                    0 AS RDB$PROCEDURE_TYPE,
                    1 AS RDB$VALID_BLR,
                    0 AS RDB$DEBUG_INFO,
                    NULL AS RDB$ENGINE_NAME,
                    NULL AS RDB$ENTRYPOINT,
                    NULL AS RDB$PACKAGE_NAME,
                    NULL AS RDB$PRIVATE_FLAG,
                    0 AS RDB$SYSTEM_FLAG,
                    NULL AS RDB$SQL_SECURITY
                FROM sys.catalog.procedures p
                WHERE p.schema_id = {schema_id}
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
                    tr.trigger_name AS RDB$TRIGGER_NAME,
                    t.table_name AS RDB$RELATION_NAME,
                    tr.position AS RDB$TRIGGER_SEQUENCE,
                    CASE tr.timing
                        WHEN 'BEFORE' THEN
                            CASE tr.event
                                WHEN 'INSERT' THEN 1
                                WHEN 'UPDATE' THEN 3
                                WHEN 'DELETE' THEN 5
                                ELSE 1
                            END
                        WHEN 'AFTER' THEN
                            CASE tr.event
                                WHEN 'INSERT' THEN 2
                                WHEN 'UPDATE' THEN 4
                                WHEN 'DELETE' THEN 6
                                ELSE 2
                            END
                        ELSE 0
                    END AS RDB$TRIGGER_TYPE,
                    tr.body AS RDB$TRIGGER_SOURCE,
                    NULL AS RDB$TRIGGER_BLR,
                    tr.description AS RDB$DESCRIPTION,
                    CASE tr.is_enabled WHEN 1 THEN 0 ELSE 1 END AS RDB$TRIGGER_INACTIVE,
                    0 AS RDB$SYSTEM_FLAG,
                    0 AS RDB$FLAGS,
                    1 AS RDB$VALID_BLR,
                    0 AS RDB$DEBUG_INFO,
                    NULL AS RDB$ENGINE_NAME,
                    NULL AS RDB$ENTRYPOINT,
                    NULL AS RDB$SQL_SECURITY
                FROM sys.catalog.triggers tr
                JOIN sys.catalog.tables t ON tr.table_id = t.table_id
                WHERE t.schema_id = {schema_id}
            )SQL",
            {"RDB$TRIGGER_NAME", "RDB$RELATION_NAME", "RDB$TRIGGER_SEQUENCE",
             "RDB$TRIGGER_TYPE", "RDB$TRIGGER_SOURCE", "RDB$TRIGGER_INACTIVE"},
            {DataType::VARCHAR, DataType::VARCHAR, DataType::INT16,
             DataType::INT16, DataType::VARCHAR, DataType::INT16},
            "Maps triggers to Firebird RDB$TRIGGERS format"
        });

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
