#pragma once

/**
 * SQL Standard information_schema Implementation
 *
 * Phase D: Catalog Cleanup - information_schema virtual catalog handler
 *
 * This module implements the SQL standard information_schema views that provide
 * standardized metadata about database objects. All major databases support
 * information_schema for tool compatibility and standards compliance.
 *
 * Implemented Views (SQL:2011 Standard):
 * - SCHEMATA: List of schemas
 * - TABLES: List of tables and views
 * - COLUMNS: Column definitions
 * - TABLE_CONSTRAINTS: Primary keys, unique, check, foreign key constraints
 * - KEY_COLUMN_USAGE: Columns participating in constraints
 * - VIEWS: View definitions
 * - ROUTINES: Functions and procedures
 * - PARAMETERS: Routine parameters
 * - TRIGGERS: Trigger definitions
 * - SEQUENCES: Sequence definitions
 * - DOMAINS: Domain definitions
 * - USER_DEFINED_TYPES: User-defined types
 *
 * Created: November 26, 2025
 * Phase: Catalog Cleanup Phase D
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/typed_value.h"
#include <string>
#include <vector>
#include <unordered_set>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * InformationSchemaHandler - SQL standard information_schema implementation
 *
 * Provides virtual views that query the ScratchBird internal catalog and
 * present data in the SQL standard information_schema format.
 *
 * Usage:
 *   SELECT * FROM information_schema.tables WHERE table_schema = 'public';
 *   SELECT * FROM information_schema.columns WHERE table_name = 'users';
 */
class InformationSchemaHandler : public VirtualCatalogHandler {
public:
    /**
     * Constructor - takes catalog manager reference
     */
    explicit InformationSchemaHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    /**
     * Get the protocol type (SCRATCHBIRD for information_schema)
     */
    ProtocolType getProtocolType() const override {
        return ProtocolType::SCRATCHBIRD;
    }

    /**
     * Check if this handler owns the given schema
     * @return true for "information_schema" (case-insensitive)
     */
    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "information_schema");
    }

    /**
     * Check if this handler owns the given table
     */
    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override {
        if (!ownsSchema(schema_name)) {
            return false;
        }
        // Check if table_name is one of our views
        for (const auto& name : table_names_) {
            if (equalsCaseInsensitive(table_name, name)) {
                return true;
            }
        }
        return false;
    }

    /**
     * Execute a query against a virtual table
     */
    Status queryTable(const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& where_clause,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        // Route to appropriate view handler
        if (equalsCaseInsensitive(table_name, "schemata")) {
            return querySchemata(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "tables")) {
            return queryTables(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "columns")) {
            return queryColumns(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "table_constraints")) {
            return queryTableConstraints(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "key_column_usage")) {
            return queryKeyColumnUsage(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "views")) {
            return queryViews(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "routines")) {
            return queryRoutines(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "parameters")) {
            return queryParameters(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "triggers")) {
            return queryTriggers(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "sequences")) {
            return querySequences(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "domains")) {
            return queryDomains(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "user_defined_types")) {
            return queryUserDefinedTypes(where_clause, results, ctx);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: information_schema." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * Get column definitions for a virtual table
     */
    Status getTableColumns(const std::string& schema_name,
                           const std::string& table_name,
                           std::vector<CatalogManager::ColumnInfo>& columns,
                           ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        // Get column definitions based on view name
        if (equalsCaseInsensitive(table_name, "schemata")) {
            return getSchemataColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "tables")) {
            return getTablesColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "columns")) {
            return getColumnsColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "table_constraints")) {
            return getTableConstraintsColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "key_column_usage")) {
            return getKeyColumnUsageColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "views")) {
            return getViewsColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "routines")) {
            return getRoutinesColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "parameters")) {
            return getParametersColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "triggers")) {
            return getTriggersColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "sequences")) {
            return getSequencesColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "domains")) {
            return getDomainsColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "user_defined_types")) {
            return getUserDefinedTypesColumns(columns);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: information_schema." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * List all virtual tables in information_schema
     */
    Status listTables(const std::string& schema_name,
                      std::vector<std::string>& table_names,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        table_names = table_names_;
        return Status::OK;
    }

    /**
     * List all schemas owned by this handler
     */
    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* ctx = nullptr) override {
        schema_names.clear();
        schema_names.push_back("information_schema");
        return Status::OK;
    }

private:
    // List of table names in information_schema
    std::vector<std::string> table_names_;

    /**
     * Initialize the list of table names
     */
    void initializeTableNames() {
        table_names_ = {
            "SCHEMATA",
            "TABLES",
            "COLUMNS",
            "TABLE_CONSTRAINTS",
            "KEY_COLUMN_USAGE",
            "VIEWS",
            "ROUTINES",
            "PARAMETERS",
            "TRIGGERS",
            "SEQUENCES",
            "DOMAINS",
            "USER_DEFINED_TYPES"
        };
    }

    // ========================================================================
    // View Query Implementations
    // ========================================================================

    /**
     * SCHEMATA view - list of schemas
     */
    Status querySchemata(const std::string& where_clause,
                         VirtualResultSet& results,
                         ErrorContext* ctx = nullptr) {
        results.column_names = {
            "CATALOG_NAME", "SCHEMA_NAME", "SCHEMA_OWNER",
            "DEFAULT_CHARACTER_SET_CATALOG", "DEFAULT_CHARACTER_SET_SCHEMA",
            "DEFAULT_CHARACTER_SET_NAME", "SQL_PATH"
        };

        results.column_types = {
            DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
            DataType::VARCHAR, DataType::VARCHAR,
            DataType::VARCHAR, DataType::VARCHAR
        };

        // Get all schemas from catalog
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            VirtualRow row;
            row.columns = {
                {"CATALOG_NAME", TypedValue::makeString("scratchbird")},
                {"SCHEMA_NAME", TypedValue::makeString(schema.schema_name)},
                {"SCHEMA_OWNER", TypedValue::makeString(schema.owner_name)},
                {"DEFAULT_CHARACTER_SET_CATALOG", TypedValue::makeNull()},
                {"DEFAULT_CHARACTER_SET_SCHEMA", TypedValue::makeNull()},
                {"DEFAULT_CHARACTER_SET_NAME", TypedValue::makeString("UTF8")},
                {"SQL_PATH", TypedValue::makeNull()}
            };
            results.rows.push_back(std::move(row));
        }

        // TODO: Apply where_clause filter
        return Status::OK;
    }

    /**
     * TABLES view - list of tables and views
     */
    Status queryTables(const std::string& where_clause,
                       VirtualResultSet& results,
                       ErrorContext* ctx = nullptr) {
        results.column_names = {
            "TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME", "TABLE_TYPE",
            "SELF_REFERENCING_COLUMN_NAME", "REFERENCE_GENERATION",
            "USER_DEFINED_TYPE_CATALOG", "USER_DEFINED_TYPE_SCHEMA",
            "USER_DEFINED_TYPE_NAME", "IS_INSERTABLE_INTO", "IS_TYPED",
            "COMMIT_ACTION"
        };

        results.column_types = {
            DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
            DataType::VARCHAR, DataType::VARCHAR,
            DataType::VARCHAR, DataType::VARCHAR,
            DataType::VARCHAR, DataType::VARCHAR, DataType::VARCHAR,
            DataType::VARCHAR
        };

        // Get all schemas first
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get tables in each schema
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                VirtualRow row;
                row.columns = {
                    {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                    {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                    {"TABLE_NAME", TypedValue::makeString(table.table_name)},
                    {"TABLE_TYPE", TypedValue::makeString(table.is_temp ? "LOCAL TEMPORARY" : "BASE TABLE")},
                    {"SELF_REFERENCING_COLUMN_NAME", TypedValue::makeNull()},
                    {"REFERENCE_GENERATION", TypedValue::makeNull()},
                    {"USER_DEFINED_TYPE_CATALOG", TypedValue::makeNull()},
                    {"USER_DEFINED_TYPE_SCHEMA", TypedValue::makeNull()},
                    {"USER_DEFINED_TYPE_NAME", TypedValue::makeNull()},
                    {"IS_INSERTABLE_INTO", TypedValue::makeString("YES")},
                    {"IS_TYPED", TypedValue::makeString("NO")},
                    {"COMMIT_ACTION", TypedValue::makeNull()}
                };
                results.rows.push_back(std::move(row));
            }

            // Get views in each schema
            std::vector<CatalogManager::ViewInfo> views;
            s = catalog_manager_->listViews(schema.schema_id, views, ctx);
            if (!s.ok()) continue;

            for (const auto& view : views) {
                VirtualRow row;
                row.columns = {
                    {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                    {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                    {"TABLE_NAME", TypedValue::makeString(view.view_name)},
                    {"TABLE_TYPE", TypedValue::makeString("VIEW")},
                    {"SELF_REFERENCING_COLUMN_NAME", TypedValue::makeNull()},
                    {"REFERENCE_GENERATION", TypedValue::makeNull()},
                    {"USER_DEFINED_TYPE_CATALOG", TypedValue::makeNull()},
                    {"USER_DEFINED_TYPE_SCHEMA", TypedValue::makeNull()},
                    {"USER_DEFINED_TYPE_NAME", TypedValue::makeNull()},
                    {"IS_INSERTABLE_INTO", TypedValue::makeString(view.is_updatable ? "YES" : "NO")},
                    {"IS_TYPED", TypedValue::makeString("NO")},
                    {"COMMIT_ACTION", TypedValue::makeNull()}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    /**
     * COLUMNS view - column definitions
     */
    Status queryColumns(const std::string& where_clause,
                        VirtualResultSet& results,
                        ErrorContext* ctx = nullptr) {
        results.column_names = {
            "TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME", "COLUMN_NAME",
            "ORDINAL_POSITION", "COLUMN_DEFAULT", "IS_NULLABLE", "DATA_TYPE",
            "CHARACTER_MAXIMUM_LENGTH", "CHARACTER_OCTET_LENGTH",
            "NUMERIC_PRECISION", "NUMERIC_PRECISION_RADIX", "NUMERIC_SCALE",
            "DATETIME_PRECISION", "INTERVAL_TYPE", "INTERVAL_PRECISION",
            "CHARACTER_SET_CATALOG", "CHARACTER_SET_SCHEMA", "CHARACTER_SET_NAME",
            "COLLATION_CATALOG", "COLLATION_SCHEMA", "COLLATION_NAME",
            "DOMAIN_CATALOG", "DOMAIN_SCHEMA", "DOMAIN_NAME",
            "UDT_CATALOG", "UDT_SCHEMA", "UDT_NAME",
            "SCOPE_CATALOG", "SCOPE_SCHEMA", "SCOPE_NAME",
            "MAXIMUM_CARDINALITY", "DTD_IDENTIFIER", "IS_SELF_REFERENCING",
            "IS_IDENTITY", "IDENTITY_GENERATION", "IDENTITY_START",
            "IDENTITY_INCREMENT", "IDENTITY_MAXIMUM", "IDENTITY_MINIMUM",
            "IDENTITY_CYCLE", "IS_GENERATED", "GENERATION_EXPRESSION",
            "IS_UPDATABLE"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[4] = DataType::INT32;  // ORDINAL_POSITION
        results.column_types[8] = DataType::INT64;  // CHARACTER_MAXIMUM_LENGTH
        results.column_types[9] = DataType::INT64;  // CHARACTER_OCTET_LENGTH
        results.column_types[10] = DataType::INT32; // NUMERIC_PRECISION
        results.column_types[11] = DataType::INT32; // NUMERIC_PRECISION_RADIX
        results.column_types[12] = DataType::INT32; // NUMERIC_SCALE
        results.column_types[13] = DataType::INT32; // DATETIME_PRECISION
        results.column_types[31] = DataType::INT32; // MAXIMUM_CARDINALITY

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get tables in each schema
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                // Get columns for each table
                std::vector<CatalogManager::ColumnInfo> columns;
                s = catalog_manager_->getColumns(table.table_id, columns, ctx);
                if (!s.ok()) continue;

                for (const auto& col : columns) {
                    VirtualRow row;
                    row.columns = {
                        {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                        {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"TABLE_NAME", TypedValue::makeString(table.table_name)},
                        {"COLUMN_NAME", TypedValue::makeString(col.column_name)},
                        {"ORDINAL_POSITION", TypedValue::makeInt32(col.ordinal)},
                        {"COLUMN_DEFAULT", col.default_value.empty() ?
                            TypedValue::makeNull() : TypedValue::makeString(col.default_value)},
                        {"IS_NULLABLE", TypedValue::makeString(col.nullable ? "YES" : "NO")},
                        {"DATA_TYPE", TypedValue::makeString(dataTypeToString(col.data_type))},
                        {"CHARACTER_MAXIMUM_LENGTH", col.max_length > 0 ?
                            TypedValue::makeInt64(col.max_length) : TypedValue::makeNull()},
                        {"CHARACTER_OCTET_LENGTH", col.max_length > 0 ?
                            TypedValue::makeInt64(col.max_length * 4) : TypedValue::makeNull()},
                        {"NUMERIC_PRECISION", isNumericType(col.data_type) ?
                            TypedValue::makeInt32(col.precision) : TypedValue::makeNull()},
                        {"NUMERIC_PRECISION_RADIX", isNumericType(col.data_type) ?
                            TypedValue::makeInt32(10) : TypedValue::makeNull()},
                        {"NUMERIC_SCALE", isNumericType(col.data_type) ?
                            TypedValue::makeInt32(col.scale) : TypedValue::makeNull()},
                        {"DATETIME_PRECISION", isDateTimeType(col.data_type) ?
                            TypedValue::makeInt32(col.precision) : TypedValue::makeNull()},
                        {"INTERVAL_TYPE", TypedValue::makeNull()},
                        {"INTERVAL_PRECISION", TypedValue::makeNull()},
                        {"CHARACTER_SET_CATALOG", TypedValue::makeNull()},
                        {"CHARACTER_SET_SCHEMA", TypedValue::makeNull()},
                        {"CHARACTER_SET_NAME", isStringType(col.data_type) ?
                            TypedValue::makeString("UTF8") : TypedValue::makeNull()},
                        {"COLLATION_CATALOG", TypedValue::makeNull()},
                        {"COLLATION_SCHEMA", TypedValue::makeNull()},
                        {"COLLATION_NAME", TypedValue::makeNull()},
                        {"DOMAIN_CATALOG", TypedValue::makeNull()},
                        {"DOMAIN_SCHEMA", TypedValue::makeNull()},
                        {"DOMAIN_NAME", TypedValue::makeNull()},
                        {"UDT_CATALOG", TypedValue::makeNull()},
                        {"UDT_SCHEMA", TypedValue::makeNull()},
                        {"UDT_NAME", TypedValue::makeNull()},
                        {"SCOPE_CATALOG", TypedValue::makeNull()},
                        {"SCOPE_SCHEMA", TypedValue::makeNull()},
                        {"SCOPE_NAME", TypedValue::makeNull()},
                        {"MAXIMUM_CARDINALITY", TypedValue::makeNull()},
                        {"DTD_IDENTIFIER", TypedValue::makeString(std::to_string(col.ordinal))},
                        {"IS_SELF_REFERENCING", TypedValue::makeString("NO")},
                        {"IS_IDENTITY", TypedValue::makeString(col.is_identity ? "YES" : "NO")},
                        {"IDENTITY_GENERATION", col.is_identity ?
                            TypedValue::makeString("ALWAYS") : TypedValue::makeNull()},
                        {"IDENTITY_START", TypedValue::makeNull()},
                        {"IDENTITY_INCREMENT", TypedValue::makeNull()},
                        {"IDENTITY_MAXIMUM", TypedValue::makeNull()},
                        {"IDENTITY_MINIMUM", TypedValue::makeNull()},
                        {"IDENTITY_CYCLE", TypedValue::makeNull()},
                        {"IS_GENERATED", TypedValue::makeString(col.is_computed ? "ALWAYS" : "NEVER")},
                        {"GENERATION_EXPRESSION", col.is_computed ?
                            TypedValue::makeString(col.computed_expression) : TypedValue::makeNull()},
                        {"IS_UPDATABLE", TypedValue::makeString("YES")}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * TABLE_CONSTRAINTS view - constraint definitions
     */
    Status queryTableConstraints(const std::string& where_clause,
                                 VirtualResultSet& results,
                                 ErrorContext* ctx = nullptr) {
        results.column_names = {
            "CONSTRAINT_CATALOG", "CONSTRAINT_SCHEMA", "CONSTRAINT_NAME",
            "TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME",
            "CONSTRAINT_TYPE", "IS_DEFERRABLE", "INITIALLY_DEFERRED",
            "ENFORCED"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get tables in each schema
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                // Get constraints for each table
                std::vector<CatalogManager::ConstraintInfo> constraints;
                s = catalog_manager_->listConstraints(table.table_id, constraints, ctx);
                if (!s.ok()) continue;

                for (const auto& constraint : constraints) {
                    VirtualRow row;
                    row.columns = {
                        {"CONSTRAINT_CATALOG", TypedValue::makeString("scratchbird")},
                        {"CONSTRAINT_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"CONSTRAINT_NAME", TypedValue::makeString(constraint.constraint_name)},
                        {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                        {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"TABLE_NAME", TypedValue::makeString(table.table_name)},
                        {"CONSTRAINT_TYPE", TypedValue::makeString(constraintTypeToString(constraint.constraint_type))},
                        {"IS_DEFERRABLE", TypedValue::makeString(constraint.is_deferrable ? "YES" : "NO")},
                        {"INITIALLY_DEFERRED", TypedValue::makeString(constraint.initially_deferred ? "YES" : "NO")},
                        {"ENFORCED", TypedValue::makeString("YES")}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * KEY_COLUMN_USAGE view - columns in constraints
     */
    Status queryKeyColumnUsage(const std::string& where_clause,
                               VirtualResultSet& results,
                               ErrorContext* ctx = nullptr) {
        results.column_names = {
            "CONSTRAINT_CATALOG", "CONSTRAINT_SCHEMA", "CONSTRAINT_NAME",
            "TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME", "COLUMN_NAME",
            "ORDINAL_POSITION", "POSITION_IN_UNIQUE_CONSTRAINT",
            "REFERENCED_TABLE_CATALOG", "REFERENCED_TABLE_SCHEMA",
            "REFERENCED_TABLE_NAME", "REFERENCED_COLUMN_NAME"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[7] = DataType::INT32;  // ORDINAL_POSITION
        results.column_types[8] = DataType::INT32;  // POSITION_IN_UNIQUE_CONSTRAINT

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get tables in each schema
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                // Get constraints for each table
                std::vector<CatalogManager::ConstraintInfo> constraints;
                s = catalog_manager_->listConstraints(table.table_id, constraints, ctx);
                if (!s.ok()) continue;

                for (const auto& constraint : constraints) {
                    // Only PK, UNIQUE, and FK constraints have column usage
                    if (constraint.constraint_type != CatalogManager::ConstraintType::PRIMARY_KEY &&
                        constraint.constraint_type != CatalogManager::ConstraintType::UNIQUE &&
                        constraint.constraint_type != CatalogManager::ConstraintType::FOREIGN_KEY) {
                        continue;
                    }

                    int ordinal = 1;
                    for (const auto& col_name : constraint.column_names) {
                        VirtualRow row;
                        row.columns = {
                            {"CONSTRAINT_CATALOG", TypedValue::makeString("scratchbird")},
                            {"CONSTRAINT_SCHEMA", TypedValue::makeString(schema.schema_name)},
                            {"CONSTRAINT_NAME", TypedValue::makeString(constraint.constraint_name)},
                            {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                            {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                            {"TABLE_NAME", TypedValue::makeString(table.table_name)},
                            {"COLUMN_NAME", TypedValue::makeString(col_name)},
                            {"ORDINAL_POSITION", TypedValue::makeInt32(ordinal++)},
                            {"POSITION_IN_UNIQUE_CONSTRAINT", TypedValue::makeNull()},
                            {"REFERENCED_TABLE_CATALOG", TypedValue::makeNull()},
                            {"REFERENCED_TABLE_SCHEMA", TypedValue::makeNull()},
                            {"REFERENCED_TABLE_NAME", TypedValue::makeNull()},
                            {"REFERENCED_COLUMN_NAME", TypedValue::makeNull()}
                        };
                        results.rows.push_back(std::move(row));
                    }
                }

                // Also get foreign keys for referenced table info
                std::vector<CatalogManager::ForeignKeyInfo> fks;
                s = catalog_manager_->listForeignKeys(table.table_id, fks, ctx);
                if (s.ok()) {
                    for (const auto& fk : fks) {
                        int ordinal = 1;
                        for (size_t i = 0; i < fk.column_names.size(); ++i) {
                            VirtualRow row;
                            row.columns = {
                                {"CONSTRAINT_CATALOG", TypedValue::makeString("scratchbird")},
                                {"CONSTRAINT_SCHEMA", TypedValue::makeString(schema.schema_name)},
                                {"CONSTRAINT_NAME", TypedValue::makeString(fk.constraint_name)},
                                {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                                {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                                {"TABLE_NAME", TypedValue::makeString(table.table_name)},
                                {"COLUMN_NAME", TypedValue::makeString(fk.column_names[i])},
                                {"ORDINAL_POSITION", TypedValue::makeInt32(ordinal++)},
                                {"POSITION_IN_UNIQUE_CONSTRAINT", TypedValue::makeInt32(static_cast<int32_t>(i + 1))},
                                {"REFERENCED_TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                                {"REFERENCED_TABLE_SCHEMA", TypedValue::makeString(fk.referenced_schema_name)},
                                {"REFERENCED_TABLE_NAME", TypedValue::makeString(fk.referenced_table_name)},
                                {"REFERENCED_COLUMN_NAME", TypedValue::makeString(fk.referenced_column_names[i])}
                            };
                            results.rows.push_back(std::move(row));
                        }
                    }
                }
            }
        }

        return Status::OK;
    }

    /**
     * VIEWS view - view definitions
     */
    Status queryViews(const std::string& where_clause,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) {
        results.column_names = {
            "TABLE_CATALOG", "TABLE_SCHEMA", "TABLE_NAME",
            "VIEW_DEFINITION", "CHECK_OPTION", "IS_UPDATABLE",
            "INSERTABLE_INTO", "IS_TRIGGER_UPDATABLE",
            "IS_TRIGGER_DELETABLE", "IS_TRIGGER_INSERTABLE_INTO"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get views in each schema
            std::vector<CatalogManager::ViewInfo> views;
            s = catalog_manager_->listViews(schema.schema_id, views, ctx);
            if (!s.ok()) continue;

            for (const auto& view : views) {
                VirtualRow row;
                row.columns = {
                    {"TABLE_CATALOG", TypedValue::makeString("scratchbird")},
                    {"TABLE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                    {"TABLE_NAME", TypedValue::makeString(view.view_name)},
                    {"VIEW_DEFINITION", TypedValue::makeString(view.definition)},
                    {"CHECK_OPTION", TypedValue::makeString(view.check_option ? "CASCADED" : "NONE")},
                    {"IS_UPDATABLE", TypedValue::makeString(view.is_updatable ? "YES" : "NO")},
                    {"INSERTABLE_INTO", TypedValue::makeString(view.is_updatable ? "YES" : "NO")},
                    {"IS_TRIGGER_UPDATABLE", TypedValue::makeString("NO")},
                    {"IS_TRIGGER_DELETABLE", TypedValue::makeString("NO")},
                    {"IS_TRIGGER_INSERTABLE_INTO", TypedValue::makeString("NO")}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    /**
     * ROUTINES view - functions and procedures
     */
    Status queryRoutines(const std::string& where_clause,
                         VirtualResultSet& results,
                         ErrorContext* ctx = nullptr) {
        results.column_names = {
            "SPECIFIC_CATALOG", "SPECIFIC_SCHEMA", "SPECIFIC_NAME",
            "ROUTINE_CATALOG", "ROUTINE_SCHEMA", "ROUTINE_NAME",
            "ROUTINE_TYPE", "MODULE_CATALOG", "MODULE_SCHEMA", "MODULE_NAME",
            "UDT_CATALOG", "UDT_SCHEMA", "UDT_NAME",
            "DATA_TYPE", "CHARACTER_MAXIMUM_LENGTH", "CHARACTER_OCTET_LENGTH",
            "NUMERIC_PRECISION", "NUMERIC_PRECISION_RADIX", "NUMERIC_SCALE",
            "DATETIME_PRECISION", "ROUTINE_BODY", "ROUTINE_DEFINITION",
            "EXTERNAL_NAME", "EXTERNAL_LANGUAGE", "PARAMETER_STYLE",
            "IS_DETERMINISTIC", "SQL_DATA_ACCESS", "IS_NULL_CALL",
            "SQL_PATH", "SCHEMA_LEVEL_ROUTINE", "MAX_DYNAMIC_RESULT_SETS",
            "IS_USER_DEFINED_CAST", "IS_IMPLICITLY_INVOCABLE",
            "SECURITY_TYPE", "TO_SQL_SPECIFIC_CATALOG", "TO_SQL_SPECIFIC_SCHEMA",
            "TO_SQL_SPECIFIC_NAME", "AS_LOCATOR", "CREATED", "LAST_ALTERED",
            "NEW_SAVEPOINT_LEVEL", "IS_UDT_DEPENDENT", "RESULT_CAST_FROM_DATA_TYPE",
            "RESULT_CAST_AS_LOCATOR", "RESULT_CAST_CHAR_MAX_LENGTH",
            "RESULT_CAST_CHAR_OCTET_LENGTH"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get functions in each schema
            std::vector<CatalogManager::FunctionInfo> functions;
            s = catalog_manager_->listFunctions(schema.schema_id, functions, ctx);
            if (s.ok()) {
                for (const auto& func : functions) {
                    VirtualRow row;
                    row.columns = {
                        {"SPECIFIC_CATALOG", TypedValue::makeString("scratchbird")},
                        {"SPECIFIC_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"SPECIFIC_NAME", TypedValue::makeString(func.function_name)},
                        {"ROUTINE_CATALOG", TypedValue::makeString("scratchbird")},
                        {"ROUTINE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"ROUTINE_NAME", TypedValue::makeString(func.function_name)},
                        {"ROUTINE_TYPE", TypedValue::makeString("FUNCTION")},
                        {"MODULE_CATALOG", TypedValue::makeNull()},
                        {"MODULE_SCHEMA", TypedValue::makeNull()},
                        {"MODULE_NAME", TypedValue::makeNull()},
                        {"UDT_CATALOG", TypedValue::makeNull()},
                        {"UDT_SCHEMA", TypedValue::makeNull()},
                        {"UDT_NAME", TypedValue::makeNull()},
                        {"DATA_TYPE", TypedValue::makeString(dataTypeToString(func.return_type))},
                        {"CHARACTER_MAXIMUM_LENGTH", TypedValue::makeNull()},
                        {"CHARACTER_OCTET_LENGTH", TypedValue::makeNull()},
                        {"NUMERIC_PRECISION", TypedValue::makeNull()},
                        {"NUMERIC_PRECISION_RADIX", TypedValue::makeNull()},
                        {"NUMERIC_SCALE", TypedValue::makeNull()},
                        {"DATETIME_PRECISION", TypedValue::makeNull()},
                        {"ROUTINE_BODY", TypedValue::makeString("SQL")},
                        {"ROUTINE_DEFINITION", TypedValue::makeString(func.body)},
                        {"EXTERNAL_NAME", TypedValue::makeNull()},
                        {"EXTERNAL_LANGUAGE", TypedValue::makeNull()},
                        {"PARAMETER_STYLE", TypedValue::makeString("SQL")},
                        {"IS_DETERMINISTIC", TypedValue::makeString(func.is_deterministic ? "YES" : "NO")},
                        {"SQL_DATA_ACCESS", TypedValue::makeString("READS SQL DATA")},
                        {"IS_NULL_CALL", TypedValue::makeString("YES")},
                        {"SQL_PATH", TypedValue::makeNull()},
                        {"SCHEMA_LEVEL_ROUTINE", TypedValue::makeString("YES")},
                        {"MAX_DYNAMIC_RESULT_SETS", TypedValue::makeString("0")},
                        {"IS_USER_DEFINED_CAST", TypedValue::makeString("NO")},
                        {"IS_IMPLICITLY_INVOCABLE", TypedValue::makeString("NO")},
                        {"SECURITY_TYPE", TypedValue::makeString("DEFINER")},
                        {"TO_SQL_SPECIFIC_CATALOG", TypedValue::makeNull()},
                        {"TO_SQL_SPECIFIC_SCHEMA", TypedValue::makeNull()},
                        {"TO_SQL_SPECIFIC_NAME", TypedValue::makeNull()},
                        {"AS_LOCATOR", TypedValue::makeString("NO")},
                        {"CREATED", TypedValue::makeNull()},
                        {"LAST_ALTERED", TypedValue::makeNull()},
                        {"NEW_SAVEPOINT_LEVEL", TypedValue::makeString("YES")},
                        {"IS_UDT_DEPENDENT", TypedValue::makeString("NO")},
                        {"RESULT_CAST_FROM_DATA_TYPE", TypedValue::makeNull()},
                        {"RESULT_CAST_AS_LOCATOR", TypedValue::makeNull()},
                        {"RESULT_CAST_CHAR_MAX_LENGTH", TypedValue::makeNull()},
                        {"RESULT_CAST_CHAR_OCTET_LENGTH", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Get procedures in each schema
            std::vector<CatalogManager::ProcedureInfo> procedures;
            s = catalog_manager_->listProcedures(schema.schema_id, procedures, ctx);
            if (s.ok()) {
                for (const auto& proc : procedures) {
                    VirtualRow row;
                    row.columns = {
                        {"SPECIFIC_CATALOG", TypedValue::makeString("scratchbird")},
                        {"SPECIFIC_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"SPECIFIC_NAME", TypedValue::makeString(proc.procedure_name)},
                        {"ROUTINE_CATALOG", TypedValue::makeString("scratchbird")},
                        {"ROUTINE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"ROUTINE_NAME", TypedValue::makeString(proc.procedure_name)},
                        {"ROUTINE_TYPE", TypedValue::makeString("PROCEDURE")},
                        {"MODULE_CATALOG", TypedValue::makeNull()},
                        {"MODULE_SCHEMA", TypedValue::makeNull()},
                        {"MODULE_NAME", TypedValue::makeNull()},
                        {"UDT_CATALOG", TypedValue::makeNull()},
                        {"UDT_SCHEMA", TypedValue::makeNull()},
                        {"UDT_NAME", TypedValue::makeNull()},
                        {"DATA_TYPE", TypedValue::makeNull()},  // Procedures don't return values
                        {"CHARACTER_MAXIMUM_LENGTH", TypedValue::makeNull()},
                        {"CHARACTER_OCTET_LENGTH", TypedValue::makeNull()},
                        {"NUMERIC_PRECISION", TypedValue::makeNull()},
                        {"NUMERIC_PRECISION_RADIX", TypedValue::makeNull()},
                        {"NUMERIC_SCALE", TypedValue::makeNull()},
                        {"DATETIME_PRECISION", TypedValue::makeNull()},
                        {"ROUTINE_BODY", TypedValue::makeString("SQL")},
                        {"ROUTINE_DEFINITION", TypedValue::makeString(proc.body)},
                        {"EXTERNAL_NAME", TypedValue::makeNull()},
                        {"EXTERNAL_LANGUAGE", TypedValue::makeNull()},
                        {"PARAMETER_STYLE", TypedValue::makeString("SQL")},
                        {"IS_DETERMINISTIC", TypedValue::makeString("NO")},
                        {"SQL_DATA_ACCESS", TypedValue::makeString("MODIFIES SQL DATA")},
                        {"IS_NULL_CALL", TypedValue::makeString("YES")},
                        {"SQL_PATH", TypedValue::makeNull()},
                        {"SCHEMA_LEVEL_ROUTINE", TypedValue::makeString("YES")},
                        {"MAX_DYNAMIC_RESULT_SETS", TypedValue::makeString("0")},
                        {"IS_USER_DEFINED_CAST", TypedValue::makeString("NO")},
                        {"IS_IMPLICITLY_INVOCABLE", TypedValue::makeString("NO")},
                        {"SECURITY_TYPE", TypedValue::makeString("DEFINER")},
                        {"TO_SQL_SPECIFIC_CATALOG", TypedValue::makeNull()},
                        {"TO_SQL_SPECIFIC_SCHEMA", TypedValue::makeNull()},
                        {"TO_SQL_SPECIFIC_NAME", TypedValue::makeNull()},
                        {"AS_LOCATOR", TypedValue::makeString("NO")},
                        {"CREATED", TypedValue::makeNull()},
                        {"LAST_ALTERED", TypedValue::makeNull()},
                        {"NEW_SAVEPOINT_LEVEL", TypedValue::makeString("YES")},
                        {"IS_UDT_DEPENDENT", TypedValue::makeString("NO")},
                        {"RESULT_CAST_FROM_DATA_TYPE", TypedValue::makeNull()},
                        {"RESULT_CAST_AS_LOCATOR", TypedValue::makeNull()},
                        {"RESULT_CAST_CHAR_MAX_LENGTH", TypedValue::makeNull()},
                        {"RESULT_CAST_CHAR_OCTET_LENGTH", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * PARAMETERS view - routine parameters
     */
    Status queryParameters(const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx = nullptr) {
        results.column_names = {
            "SPECIFIC_CATALOG", "SPECIFIC_SCHEMA", "SPECIFIC_NAME",
            "ORDINAL_POSITION", "PARAMETER_MODE", "IS_RESULT",
            "AS_LOCATOR", "PARAMETER_NAME", "DATA_TYPE",
            "CHARACTER_MAXIMUM_LENGTH", "CHARACTER_OCTET_LENGTH",
            "NUMERIC_PRECISION", "NUMERIC_PRECISION_RADIX", "NUMERIC_SCALE",
            "DATETIME_PRECISION", "INTERVAL_TYPE", "INTERVAL_PRECISION",
            "UDT_CATALOG", "UDT_SCHEMA", "UDT_NAME",
            "SCOPE_CATALOG", "SCOPE_SCHEMA", "SCOPE_NAME",
            "MAXIMUM_CARDINALITY", "DTD_IDENTIFIER", "PARAMETER_DEFAULT"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[3] = DataType::INT32;  // ORDINAL_POSITION

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get functions in each schema
            std::vector<CatalogManager::FunctionInfo> functions;
            s = catalog_manager_->listFunctions(schema.schema_id, functions, ctx);
            if (s.ok()) {
                for (const auto& func : functions) {
                    int ordinal = 1;
                    for (const auto& param : func.parameters) {
                        VirtualRow row;
                        row.columns = {
                            {"SPECIFIC_CATALOG", TypedValue::makeString("scratchbird")},
                            {"SPECIFIC_SCHEMA", TypedValue::makeString(schema.schema_name)},
                            {"SPECIFIC_NAME", TypedValue::makeString(func.function_name)},
                            {"ORDINAL_POSITION", TypedValue::makeInt32(ordinal++)},
                            {"PARAMETER_MODE", TypedValue::makeString("IN")},
                            {"IS_RESULT", TypedValue::makeString("NO")},
                            {"AS_LOCATOR", TypedValue::makeString("NO")},
                            {"PARAMETER_NAME", TypedValue::makeString(param.param_name)},
                            {"DATA_TYPE", TypedValue::makeString(dataTypeToString(param.data_type))},
                            {"CHARACTER_MAXIMUM_LENGTH", TypedValue::makeNull()},
                            {"CHARACTER_OCTET_LENGTH", TypedValue::makeNull()},
                            {"NUMERIC_PRECISION", TypedValue::makeNull()},
                            {"NUMERIC_PRECISION_RADIX", TypedValue::makeNull()},
                            {"NUMERIC_SCALE", TypedValue::makeNull()},
                            {"DATETIME_PRECISION", TypedValue::makeNull()},
                            {"INTERVAL_TYPE", TypedValue::makeNull()},
                            {"INTERVAL_PRECISION", TypedValue::makeNull()},
                            {"UDT_CATALOG", TypedValue::makeNull()},
                            {"UDT_SCHEMA", TypedValue::makeNull()},
                            {"UDT_NAME", TypedValue::makeNull()},
                            {"SCOPE_CATALOG", TypedValue::makeNull()},
                            {"SCOPE_SCHEMA", TypedValue::makeNull()},
                            {"SCOPE_NAME", TypedValue::makeNull()},
                            {"MAXIMUM_CARDINALITY", TypedValue::makeNull()},
                            {"DTD_IDENTIFIER", TypedValue::makeString(std::to_string(ordinal - 1))},
                            {"PARAMETER_DEFAULT", param.default_value.empty() ?
                                TypedValue::makeNull() : TypedValue::makeString(param.default_value)}
                        };
                        results.rows.push_back(std::move(row));
                    }
                }
            }

            // Get procedures in each schema
            std::vector<CatalogManager::ProcedureInfo> procedures;
            s = catalog_manager_->listProcedures(schema.schema_id, procedures, ctx);
            if (s.ok()) {
                for (const auto& proc : procedures) {
                    int ordinal = 1;
                    for (const auto& param : proc.parameters) {
                        std::string mode = "IN";
                        if (param.mode == CatalogManager::ParamMode::OUT) mode = "OUT";
                        else if (param.mode == CatalogManager::ParamMode::INOUT) mode = "INOUT";

                        VirtualRow row;
                        row.columns = {
                            {"SPECIFIC_CATALOG", TypedValue::makeString("scratchbird")},
                            {"SPECIFIC_SCHEMA", TypedValue::makeString(schema.schema_name)},
                            {"SPECIFIC_NAME", TypedValue::makeString(proc.procedure_name)},
                            {"ORDINAL_POSITION", TypedValue::makeInt32(ordinal++)},
                            {"PARAMETER_MODE", TypedValue::makeString(mode)},
                            {"IS_RESULT", TypedValue::makeString("NO")},
                            {"AS_LOCATOR", TypedValue::makeString("NO")},
                            {"PARAMETER_NAME", TypedValue::makeString(param.param_name)},
                            {"DATA_TYPE", TypedValue::makeString(dataTypeToString(param.data_type))},
                            {"CHARACTER_MAXIMUM_LENGTH", TypedValue::makeNull()},
                            {"CHARACTER_OCTET_LENGTH", TypedValue::makeNull()},
                            {"NUMERIC_PRECISION", TypedValue::makeNull()},
                            {"NUMERIC_PRECISION_RADIX", TypedValue::makeNull()},
                            {"NUMERIC_SCALE", TypedValue::makeNull()},
                            {"DATETIME_PRECISION", TypedValue::makeNull()},
                            {"INTERVAL_TYPE", TypedValue::makeNull()},
                            {"INTERVAL_PRECISION", TypedValue::makeNull()},
                            {"UDT_CATALOG", TypedValue::makeNull()},
                            {"UDT_SCHEMA", TypedValue::makeNull()},
                            {"UDT_NAME", TypedValue::makeNull()},
                            {"SCOPE_CATALOG", TypedValue::makeNull()},
                            {"SCOPE_SCHEMA", TypedValue::makeNull()},
                            {"SCOPE_NAME", TypedValue::makeNull()},
                            {"MAXIMUM_CARDINALITY", TypedValue::makeNull()},
                            {"DTD_IDENTIFIER", TypedValue::makeString(std::to_string(ordinal - 1))},
                            {"PARAMETER_DEFAULT", param.default_value.empty() ?
                                TypedValue::makeNull() : TypedValue::makeString(param.default_value)}
                        };
                        results.rows.push_back(std::move(row));
                    }
                }
            }
        }

        return Status::OK;
    }

    /**
     * TRIGGERS view - trigger definitions
     */
    Status queryTriggers(const std::string& where_clause,
                         VirtualResultSet& results,
                         ErrorContext* ctx = nullptr) {
        results.column_names = {
            "TRIGGER_CATALOG", "TRIGGER_SCHEMA", "TRIGGER_NAME",
            "EVENT_MANIPULATION", "EVENT_OBJECT_CATALOG", "EVENT_OBJECT_SCHEMA",
            "EVENT_OBJECT_TABLE", "ACTION_ORDER", "ACTION_CONDITION",
            "ACTION_STATEMENT", "ACTION_ORIENTATION", "ACTION_TIMING",
            "ACTION_REFERENCE_OLD_TABLE", "ACTION_REFERENCE_NEW_TABLE",
            "ACTION_REFERENCE_OLD_ROW", "ACTION_REFERENCE_NEW_ROW",
            "CREATED"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[7] = DataType::INT32;  // ACTION_ORDER
        results.column_types[16] = DataType::TIMESTAMP;  // CREATED

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get tables in each schema
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                // Get triggers for each table
                std::vector<CatalogManager::TriggerInfo> triggers;
                s = catalog_manager_->listTriggers(table.table_id, triggers, ctx);
                if (!s.ok()) continue;

                for (const auto& trigger : triggers) {
                    VirtualRow row;
                    row.columns = {
                        {"TRIGGER_CATALOG", TypedValue::makeString("scratchbird")},
                        {"TRIGGER_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"TRIGGER_NAME", TypedValue::makeString(trigger.trigger_name)},
                        {"EVENT_MANIPULATION", TypedValue::makeString(triggerEventToString(trigger.event))},
                        {"EVENT_OBJECT_CATALOG", TypedValue::makeString("scratchbird")},
                        {"EVENT_OBJECT_SCHEMA", TypedValue::makeString(schema.schema_name)},
                        {"EVENT_OBJECT_TABLE", TypedValue::makeString(table.table_name)},
                        {"ACTION_ORDER", TypedValue::makeInt32(trigger.position)},
                        {"ACTION_CONDITION", trigger.when_clause.empty() ?
                            TypedValue::makeNull() : TypedValue::makeString(trigger.when_clause)},
                        {"ACTION_STATEMENT", TypedValue::makeString(trigger.body)},
                        {"ACTION_ORIENTATION", TypedValue::makeString(trigger.for_each_row ? "ROW" : "STATEMENT")},
                        {"ACTION_TIMING", TypedValue::makeString(triggerTimingToString(trigger.timing))},
                        {"ACTION_REFERENCE_OLD_TABLE", TypedValue::makeNull()},
                        {"ACTION_REFERENCE_NEW_TABLE", TypedValue::makeNull()},
                        {"ACTION_REFERENCE_OLD_ROW", TypedValue::makeString("OLD")},
                        {"ACTION_REFERENCE_NEW_ROW", TypedValue::makeString("NEW")},
                        {"CREATED", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * SEQUENCES view - sequence definitions
     */
    Status querySequences(const std::string& where_clause,
                          VirtualResultSet& results,
                          ErrorContext* ctx = nullptr) {
        results.column_names = {
            "SEQUENCE_CATALOG", "SEQUENCE_SCHEMA", "SEQUENCE_NAME",
            "DATA_TYPE", "NUMERIC_PRECISION", "NUMERIC_PRECISION_RADIX",
            "NUMERIC_SCALE", "START_VALUE", "MINIMUM_VALUE", "MAXIMUM_VALUE",
            "INCREMENT", "CYCLE_OPTION", "DECLARED_DATA_TYPE",
            "DECLARED_NUMERIC_PRECISION", "DECLARED_NUMERIC_SCALE"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[4] = DataType::INT32;   // NUMERIC_PRECISION
        results.column_types[5] = DataType::INT32;   // NUMERIC_PRECISION_RADIX
        results.column_types[6] = DataType::INT32;   // NUMERIC_SCALE
        results.column_types[7] = DataType::INT64;   // START_VALUE
        results.column_types[8] = DataType::INT64;   // MINIMUM_VALUE
        results.column_types[9] = DataType::INT64;   // MAXIMUM_VALUE
        results.column_types[10] = DataType::INT64;  // INCREMENT
        results.column_types[13] = DataType::INT32;  // DECLARED_NUMERIC_PRECISION
        results.column_types[14] = DataType::INT32;  // DECLARED_NUMERIC_SCALE

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get sequences in each schema
            std::vector<CatalogManager::SequenceInfo> sequences;
            s = catalog_manager_->listSequences(schema.schema_id, sequences, ctx);
            if (!s.ok()) continue;

            for (const auto& seq : sequences) {
                VirtualRow row;
                row.columns = {
                    {"SEQUENCE_CATALOG", TypedValue::makeString("scratchbird")},
                    {"SEQUENCE_SCHEMA", TypedValue::makeString(schema.schema_name)},
                    {"SEQUENCE_NAME", TypedValue::makeString(seq.sequence_name)},
                    {"DATA_TYPE", TypedValue::makeString("BIGINT")},
                    {"NUMERIC_PRECISION", TypedValue::makeInt32(64)},
                    {"NUMERIC_PRECISION_RADIX", TypedValue::makeInt32(2)},
                    {"NUMERIC_SCALE", TypedValue::makeInt32(0)},
                    {"START_VALUE", TypedValue::makeInt64(seq.start_value)},
                    {"MINIMUM_VALUE", TypedValue::makeInt64(seq.min_value)},
                    {"MAXIMUM_VALUE", TypedValue::makeInt64(seq.max_value)},
                    {"INCREMENT", TypedValue::makeInt64(seq.increment_by)},
                    {"CYCLE_OPTION", TypedValue::makeString(seq.cycle ? "YES" : "NO")},
                    {"DECLARED_DATA_TYPE", TypedValue::makeString("BIGINT")},
                    {"DECLARED_NUMERIC_PRECISION", TypedValue::makeInt32(64)},
                    {"DECLARED_NUMERIC_SCALE", TypedValue::makeInt32(0)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    /**
     * DOMAINS view - domain definitions
     */
    Status queryDomains(const std::string& where_clause,
                        VirtualResultSet& results,
                        ErrorContext* ctx = nullptr) {
        results.column_names = {
            "DOMAIN_CATALOG", "DOMAIN_SCHEMA", "DOMAIN_NAME",
            "DATA_TYPE", "CHARACTER_MAXIMUM_LENGTH", "CHARACTER_OCTET_LENGTH",
            "NUMERIC_PRECISION", "NUMERIC_PRECISION_RADIX", "NUMERIC_SCALE",
            "DATETIME_PRECISION", "DOMAIN_DEFAULT"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        // Get all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Get domains in each schema
            std::vector<CatalogManager::DomainInfo> domains;
            s = catalog_manager_->listDomains(schema.schema_id, domains, ctx);
            if (!s.ok()) continue;

            for (const auto& domain : domains) {
                VirtualRow row;
                row.columns = {
                    {"DOMAIN_CATALOG", TypedValue::makeString("scratchbird")},
                    {"DOMAIN_SCHEMA", TypedValue::makeString(schema.schema_name)},
                    {"DOMAIN_NAME", TypedValue::makeString(domain.domain_name)},
                    {"DATA_TYPE", TypedValue::makeString(dataTypeToString(domain.data_type))},
                    {"CHARACTER_MAXIMUM_LENGTH", domain.max_length > 0 ?
                        TypedValue::makeString(std::to_string(domain.max_length)) : TypedValue::makeNull()},
                    {"CHARACTER_OCTET_LENGTH", domain.max_length > 0 ?
                        TypedValue::makeString(std::to_string(domain.max_length * 4)) : TypedValue::makeNull()},
                    {"NUMERIC_PRECISION", isNumericType(domain.data_type) ?
                        TypedValue::makeString(std::to_string(domain.precision)) : TypedValue::makeNull()},
                    {"NUMERIC_PRECISION_RADIX", isNumericType(domain.data_type) ?
                        TypedValue::makeString("10") : TypedValue::makeNull()},
                    {"NUMERIC_SCALE", isNumericType(domain.data_type) ?
                        TypedValue::makeString(std::to_string(domain.scale)) : TypedValue::makeNull()},
                    {"DATETIME_PRECISION", TypedValue::makeNull()},
                    {"DOMAIN_DEFAULT", domain.default_value.empty() ?
                        TypedValue::makeNull() : TypedValue::makeString(domain.default_value)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    /**
     * USER_DEFINED_TYPES view - UDT definitions (placeholder)
     */
    Status queryUserDefinedTypes(const std::string& where_clause,
                                 VirtualResultSet& results,
                                 ErrorContext* ctx = nullptr) {
        results.column_names = {
            "USER_DEFINED_TYPE_CATALOG", "USER_DEFINED_TYPE_SCHEMA",
            "USER_DEFINED_TYPE_NAME", "USER_DEFINED_TYPE_CATEGORY",
            "IS_INSTANTIABLE", "IS_FINAL", "ORDERING_FORM", "ORDERING_CATEGORY",
            "ORDERING_ROUTINE_CATALOG", "ORDERING_ROUTINE_SCHEMA",
            "ORDERING_ROUTINE_NAME", "REFERENCE_TYPE", "DATA_TYPE",
            "CHARACTER_MAXIMUM_LENGTH", "CHARACTER_OCTET_LENGTH",
            "NUMERIC_PRECISION", "NUMERIC_PRECISION_RADIX", "NUMERIC_SCALE",
            "DATETIME_PRECISION", "INTERVAL_TYPE", "INTERVAL_PRECISION",
            "SOURCE_DTD_IDENTIFIER", "REF_DTD_IDENTIFIER"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);

        // UDTs not fully implemented yet - return empty result set
        return Status::OK;
    }

    // ========================================================================
    // Column Definition Helpers
    // ========================================================================

    Status getSchemataColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("CATALOG_NAME", DataType::VARCHAR, 1),
            makeColumnInfo("SCHEMA_NAME", DataType::VARCHAR, 2),
            makeColumnInfo("SCHEMA_OWNER", DataType::VARCHAR, 3),
            makeColumnInfo("DEFAULT_CHARACTER_SET_CATALOG", DataType::VARCHAR, 4),
            makeColumnInfo("DEFAULT_CHARACTER_SET_SCHEMA", DataType::VARCHAR, 5),
            makeColumnInfo("DEFAULT_CHARACTER_SET_NAME", DataType::VARCHAR, 6),
            makeColumnInfo("SQL_PATH", DataType::VARCHAR, 7)
        };
        return Status::OK;
    }

    Status getTablesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("TABLE_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("TABLE_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("TABLE_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("TABLE_TYPE", DataType::VARCHAR, 4),
            makeColumnInfo("SELF_REFERENCING_COLUMN_NAME", DataType::VARCHAR, 5),
            makeColumnInfo("REFERENCE_GENERATION", DataType::VARCHAR, 6),
            makeColumnInfo("USER_DEFINED_TYPE_CATALOG", DataType::VARCHAR, 7),
            makeColumnInfo("USER_DEFINED_TYPE_SCHEMA", DataType::VARCHAR, 8),
            makeColumnInfo("USER_DEFINED_TYPE_NAME", DataType::VARCHAR, 9),
            makeColumnInfo("IS_INSERTABLE_INTO", DataType::VARCHAR, 10),
            makeColumnInfo("IS_TYPED", DataType::VARCHAR, 11),
            makeColumnInfo("COMMIT_ACTION", DataType::VARCHAR, 12)
        };
        return Status::OK;
    }

    Status getColumnsColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("TABLE_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("TABLE_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("TABLE_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("COLUMN_NAME", DataType::VARCHAR, 4),
            makeColumnInfo("ORDINAL_POSITION", DataType::INT32, 5),
            makeColumnInfo("COLUMN_DEFAULT", DataType::VARCHAR, 6),
            makeColumnInfo("IS_NULLABLE", DataType::VARCHAR, 7),
            makeColumnInfo("DATA_TYPE", DataType::VARCHAR, 8)
            // Simplified - full version has 44 columns
        };
        return Status::OK;
    }

    Status getTableConstraintsColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("CONSTRAINT_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("CONSTRAINT_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("CONSTRAINT_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("TABLE_CATALOG", DataType::VARCHAR, 4),
            makeColumnInfo("TABLE_SCHEMA", DataType::VARCHAR, 5),
            makeColumnInfo("TABLE_NAME", DataType::VARCHAR, 6),
            makeColumnInfo("CONSTRAINT_TYPE", DataType::VARCHAR, 7),
            makeColumnInfo("IS_DEFERRABLE", DataType::VARCHAR, 8),
            makeColumnInfo("INITIALLY_DEFERRED", DataType::VARCHAR, 9),
            makeColumnInfo("ENFORCED", DataType::VARCHAR, 10)
        };
        return Status::OK;
    }

    Status getKeyColumnUsageColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("CONSTRAINT_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("CONSTRAINT_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("CONSTRAINT_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("TABLE_CATALOG", DataType::VARCHAR, 4),
            makeColumnInfo("TABLE_SCHEMA", DataType::VARCHAR, 5),
            makeColumnInfo("TABLE_NAME", DataType::VARCHAR, 6),
            makeColumnInfo("COLUMN_NAME", DataType::VARCHAR, 7),
            makeColumnInfo("ORDINAL_POSITION", DataType::INT32, 8),
            makeColumnInfo("POSITION_IN_UNIQUE_CONSTRAINT", DataType::INT32, 9),
            makeColumnInfo("REFERENCED_TABLE_CATALOG", DataType::VARCHAR, 10),
            makeColumnInfo("REFERENCED_TABLE_SCHEMA", DataType::VARCHAR, 11),
            makeColumnInfo("REFERENCED_TABLE_NAME", DataType::VARCHAR, 12),
            makeColumnInfo("REFERENCED_COLUMN_NAME", DataType::VARCHAR, 13)
        };
        return Status::OK;
    }

    Status getViewsColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("TABLE_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("TABLE_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("TABLE_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("VIEW_DEFINITION", DataType::VARCHAR, 4),
            makeColumnInfo("CHECK_OPTION", DataType::VARCHAR, 5),
            makeColumnInfo("IS_UPDATABLE", DataType::VARCHAR, 6),
            makeColumnInfo("INSERTABLE_INTO", DataType::VARCHAR, 7),
            makeColumnInfo("IS_TRIGGER_UPDATABLE", DataType::VARCHAR, 8),
            makeColumnInfo("IS_TRIGGER_DELETABLE", DataType::VARCHAR, 9),
            makeColumnInfo("IS_TRIGGER_INSERTABLE_INTO", DataType::VARCHAR, 10)
        };
        return Status::OK;
    }

    Status getRoutinesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("SPECIFIC_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("SPECIFIC_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("SPECIFIC_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("ROUTINE_CATALOG", DataType::VARCHAR, 4),
            makeColumnInfo("ROUTINE_SCHEMA", DataType::VARCHAR, 5),
            makeColumnInfo("ROUTINE_NAME", DataType::VARCHAR, 6),
            makeColumnInfo("ROUTINE_TYPE", DataType::VARCHAR, 7),
            makeColumnInfo("ROUTINE_DEFINITION", DataType::VARCHAR, 8)
            // Simplified - full version has 46 columns
        };
        return Status::OK;
    }

    Status getParametersColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("SPECIFIC_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("SPECIFIC_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("SPECIFIC_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("ORDINAL_POSITION", DataType::INT32, 4),
            makeColumnInfo("PARAMETER_MODE", DataType::VARCHAR, 5),
            makeColumnInfo("PARAMETER_NAME", DataType::VARCHAR, 6),
            makeColumnInfo("DATA_TYPE", DataType::VARCHAR, 7)
            // Simplified
        };
        return Status::OK;
    }

    Status getTriggersColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("TRIGGER_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("TRIGGER_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("TRIGGER_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("EVENT_MANIPULATION", DataType::VARCHAR, 4),
            makeColumnInfo("EVENT_OBJECT_CATALOG", DataType::VARCHAR, 5),
            makeColumnInfo("EVENT_OBJECT_SCHEMA", DataType::VARCHAR, 6),
            makeColumnInfo("EVENT_OBJECT_TABLE", DataType::VARCHAR, 7),
            makeColumnInfo("ACTION_ORDER", DataType::INT32, 8),
            makeColumnInfo("ACTION_STATEMENT", DataType::VARCHAR, 9),
            makeColumnInfo("ACTION_ORIENTATION", DataType::VARCHAR, 10),
            makeColumnInfo("ACTION_TIMING", DataType::VARCHAR, 11)
        };
        return Status::OK;
    }

    Status getSequencesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("SEQUENCE_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("SEQUENCE_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("SEQUENCE_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("DATA_TYPE", DataType::VARCHAR, 4),
            makeColumnInfo("START_VALUE", DataType::INT64, 5),
            makeColumnInfo("MINIMUM_VALUE", DataType::INT64, 6),
            makeColumnInfo("MAXIMUM_VALUE", DataType::INT64, 7),
            makeColumnInfo("INCREMENT", DataType::INT64, 8),
            makeColumnInfo("CYCLE_OPTION", DataType::VARCHAR, 9)
        };
        return Status::OK;
    }

    Status getDomainsColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("DOMAIN_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("DOMAIN_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("DOMAIN_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("DATA_TYPE", DataType::VARCHAR, 4),
            makeColumnInfo("DOMAIN_DEFAULT", DataType::VARCHAR, 5)
        };
        return Status::OK;
    }

    Status getUserDefinedTypesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("USER_DEFINED_TYPE_CATALOG", DataType::VARCHAR, 1),
            makeColumnInfo("USER_DEFINED_TYPE_SCHEMA", DataType::VARCHAR, 2),
            makeColumnInfo("USER_DEFINED_TYPE_NAME", DataType::VARCHAR, 3),
            makeColumnInfo("USER_DEFINED_TYPE_CATEGORY", DataType::VARCHAR, 4),
            makeColumnInfo("IS_INSTANTIABLE", DataType::VARCHAR, 5),
            makeColumnInfo("IS_FINAL", DataType::VARCHAR, 6)
        };
        return Status::OK;
    }

    // ========================================================================
    // Helper Functions
    // ========================================================================

    CatalogManager::ColumnInfo makeColumnInfo(const std::string& name,
                                               DataType type,
                                               int ordinal) {
        CatalogManager::ColumnInfo col;
        col.column_name = name;
        col.data_type = type;
        col.ordinal = ordinal;
        col.nullable = true;
        col.max_length = (type == DataType::VARCHAR) ? 256 : 0;
        return col;
    }

    static const char* dataTypeToString(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:   return "BOOLEAN";
            case DataType::INT16:     return "SMALLINT";
            case DataType::INT32:     return "INTEGER";
            case DataType::INT64:     return "BIGINT";
            case DataType::FLOAT32:   return "REAL";
            case DataType::FLOAT64:   return "DOUBLE PRECISION";
            case DataType::DECIMAL:   return "DECIMAL";
            case DataType::VARCHAR:   return "CHARACTER VARYING";
            case DataType::CHAR:      return "CHARACTER";
            case DataType::TEXT:      return "TEXT";
            case DataType::BLOB:      return "BLOB";
            case DataType::DATE:      return "DATE";
            case DataType::TIME:      return "TIME";
            case DataType::TIMESTAMP: return "TIMESTAMP";
            case DataType::UUID:      return "UUID";
            default:                  return "UNKNOWN";
        }
    }

    static bool isNumericType(DataType type) {
        return type == DataType::INT16 || type == DataType::INT32 ||
               type == DataType::INT64 || type == DataType::FLOAT32 ||
               type == DataType::FLOAT64 || type == DataType::DECIMAL;
    }

    static bool isDateTimeType(DataType type) {
        return type == DataType::DATE || type == DataType::TIME ||
               type == DataType::TIMESTAMP;
    }

    static bool isStringType(DataType type) {
        return type == DataType::VARCHAR || type == DataType::CHAR ||
               type == DataType::TEXT;
    }

    static const char* constraintTypeToString(CatalogManager::ConstraintType type) {
        switch (type) {
            case CatalogManager::ConstraintType::PRIMARY_KEY: return "PRIMARY KEY";
            case CatalogManager::ConstraintType::UNIQUE:      return "UNIQUE";
            case CatalogManager::ConstraintType::CHECK:       return "CHECK";
            case CatalogManager::ConstraintType::FOREIGN_KEY: return "FOREIGN KEY";
            case CatalogManager::ConstraintType::NOT_NULL:    return "NOT NULL";
            default:                                          return "UNKNOWN";
        }
    }

    static const char* triggerEventToString(CatalogManager::TriggerEvent event) {
        switch (event) {
            case CatalogManager::TriggerEvent::INSERT: return "INSERT";
            case CatalogManager::TriggerEvent::UPDATE: return "UPDATE";
            case CatalogManager::TriggerEvent::DELETE: return "DELETE";
            default:                                   return "UNKNOWN";
        }
    }

    static const char* triggerTimingToString(CatalogManager::TriggerTiming timing) {
        switch (timing) {
            case CatalogManager::TriggerTiming::BEFORE:      return "BEFORE";
            case CatalogManager::TriggerTiming::AFTER:       return "AFTER";
            case CatalogManager::TriggerTiming::INSTEAD_OF:  return "INSTEAD OF";
            default:                                         return "UNKNOWN";
        }
    }
};

} // namespace scratchbird::catalog
