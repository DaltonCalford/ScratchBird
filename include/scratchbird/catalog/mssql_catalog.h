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
 * SQL Server sys.* Emulation Handler
 *
 * Phase D: Catalog Cleanup - SQL Server system catalog emulation
 *
 * This module implements SQL Server-compatible sys.* system views that allow
 * SQL Server client tools (SSMS, sqlcmd, etc.) to introspect the ScratchBird
 * catalog as if it were a SQL Server database.
 *
 * Implemented Views (SQL Server-compatible):
 * - sys.schemas: Schemas
 * - sys.tables: Tables
 * - sys.columns: Columns
 * - sys.indexes: Indexes
 * - sys.types: Data types
 * - sys.objects: All database objects
 * - sys.procedures: Stored procedures
 * - sys.sql_modules: Module definitions (stored code)
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

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * MSSQLCatalogHandler - SQL Server sys.* schema emulation
 *
 * Provides virtual views that query the ScratchBird internal catalog and
 * present data in SQL Server sys.* format for tool compatibility.
 *
 * Note: SQL Server uses object_id as unique identifiers. We map
 * ScratchBird IDs to object_ids for compatibility.
 *
 * Usage:
 *   SELECT * FROM sys.tables WHERE name = 'users';
 *   SELECT * FROM sys.columns WHERE object_id = 12345;
 */
class MSSQLCatalogHandler : public VirtualCatalogHandler {
public:
    /**
     * Constructor - takes catalog manager reference
     */
    explicit MSSQLCatalogHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    /**
     * Get the protocol type
     */
    ProtocolType getProtocolType() const override {
        return ProtocolType::MSSQL;
    }

    /**
     * Check if this handler owns the given schema
     * @return true for "sys" (case-insensitive)
     */
    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "sys");
    }

    /**
     * Check if this handler owns the given table
     */
    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override {
        if (!ownsSchema(schema_name)) {
            return false;
        }
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

        if (equalsCaseInsensitive(table_name, "schemas")) {
            return querySysSchemas(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "tables")) {
            return querySysTables(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "columns")) {
            return querySysColumns(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "indexes")) {
            return querySysIndexes(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "types")) {
            return querySysTypes(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "objects")) {
            return querySysObjects(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "procedures")) {
            return querySysProcedures(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "sql_modules")) {
            return querySysSqlModules(where_clause, results, ctx);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: sys." + table_name);
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

        if (equalsCaseInsensitive(table_name, "schemas")) {
            return getSysSchemasColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "tables")) {
            return getSysTablesColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "columns")) {
            return getSysColumnsColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "indexes")) {
            return getSysIndexesColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "types")) {
            return getSysTypesColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "objects")) {
            return getSysObjectsColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "procedures")) {
            return getSysProceduresColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "sql_modules")) {
            return getSysSqlModulesColumns(columns);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: sys." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * List all virtual tables in sys schema
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
        schema_names.push_back("sys");
        return Status::OK;
    }

private:
    std::vector<std::string> table_names_;

    void initializeTableNames() {
        table_names_ = {
            "schemas",
            "tables",
            "columns",
            "indexes",
            "types",
            "objects",
            "procedures",
            "sql_modules"
        };
    }

    // ========================================================================
    // Helper: Convert ID to SQL Server object_id
    // ========================================================================

    static int32_t idToObjectId(const ID& id) {
        return static_cast<int32_t>(id.value() & 0x7FFFFFFF);
    }

    /**
     * Map ScratchBird DataType to SQL Server system_type_id
     */
    static int32_t dataTypeToSystemTypeId(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:   return 104;  // bit
            case DataType::INT16:     return 52;   // smallint
            case DataType::INT32:     return 56;   // int
            case DataType::INT64:     return 127;  // bigint
            case DataType::FLOAT32:   return 59;   // real
            case DataType::FLOAT64:   return 62;   // float
            case DataType::DECIMAL:   return 106;  // decimal
            case DataType::VARCHAR:   return 167;  // varchar
            case DataType::CHAR:      return 175;  // char
            case DataType::TEXT:      return 35;   // text
            case DataType::BLOB:      return 34;   // image
            case DataType::DATE:      return 40;   // date
            case DataType::TIME:      return 41;   // time
            case DataType::TIMESTAMP: return 61;   // datetime
            case DataType::UUID:      return 36;   // uniqueidentifier
            default:                  return 167;  // varchar
        }
    }

    // ========================================================================
    // View Query Implementations
    // ========================================================================

    /**
     * sys.schemas - schemas
     */
    Status querySysSchemas(const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx = nullptr) {
        results.column_names = {"name", "schema_id", "principal_id"};
        results.column_types = {DataType::VARCHAR, DataType::INT32, DataType::INT32};

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            VirtualRow row;
            row.columns = {
                {"name", TypedValue::makeVarchar(schema.schema_name)},
                {"schema_id", TypedValue::makeInt32(idToObjectId(schema.schema_id))},
                {"principal_id", TypedValue::makeInt32(idToObjectId(schema.owner_id))}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * sys.tables - tables
     */
    Status querySysTables(const std::string& where_clause,
                          VirtualResultSet& results,
                          ErrorContext* ctx = nullptr) {
        results.column_names = {
            "name", "object_id", "principal_id", "schema_id", "parent_object_id",
            "type", "type_desc", "create_date", "modify_date", "is_ms_shipped",
            "is_published", "is_schema_published", "lob_data_space_id",
            "filestream_data_space_id", "max_column_id_used",
            "lock_on_bulk_load", "uses_ansi_nulls", "is_replicated",
            "has_replication_filter", "is_merge_published",
            "is_sync_tran_subscribed", "has_unchecked_assembly_data",
            "text_in_row_limit", "large_value_types_out_of_row",
            "is_tracked_by_cdc", "lock_escalation", "lock_escalation_desc",
            "is_filetable", "is_memory_optimized", "durability",
            "durability_desc", "temporal_type", "temporal_type_desc",
            "history_table_id", "is_remote_data_archive_enabled",
            "is_external", "history_retention_period",
            "history_retention_period_unit", "history_retention_period_unit_desc",
            "is_node", "is_edge"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[1] = DataType::INT32;   // object_id
        results.column_types[2] = DataType::INT32;   // principal_id
        results.column_types[3] = DataType::INT32;   // schema_id
        results.column_types[4] = DataType::INT32;   // parent_object_id
        results.column_types[7] = DataType::TIMESTAMP;  // create_date
        results.column_types[8] = DataType::TIMESTAMP;  // modify_date

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                VirtualRow row;
                row.columns = {
                    {"name", TypedValue::makeVarchar(table.table_name)},
                    {"object_id", TypedValue::makeInt32(idToObjectId(table.table_id))},
                    {"principal_id", TypedValue::makeNull()},
                    {"schema_id", TypedValue::makeInt32(idToObjectId(schema.schema_id))},
                    {"parent_object_id", TypedValue::makeInt32(0)},
                    {"type", TypedValue::makeVarchar("U")},  // U = User table
                    {"type_desc", TypedValue::makeVarchar("USER_TABLE")},
                    {"create_date", TypedValue::makeNull()},
                    {"modify_date", TypedValue::makeNull()},
                    {"is_ms_shipped", TypedValue::makeVarchar("0")},
                    {"is_published", TypedValue::makeVarchar("0")},
                    {"is_schema_published", TypedValue::makeVarchar("0")},
                    {"lob_data_space_id", TypedValue::makeInt32(0)},
                    {"filestream_data_space_id", TypedValue::makeNull()},
                    {"max_column_id_used", TypedValue::makeInt32(static_cast<int32_t>(table.column_count))},
                    {"lock_on_bulk_load", TypedValue::makeVarchar("0")},
                    {"uses_ansi_nulls", TypedValue::makeVarchar("1")},
                    {"is_replicated", TypedValue::makeVarchar("0")},
                    {"has_replication_filter", TypedValue::makeVarchar("0")},
                    {"is_merge_published", TypedValue::makeVarchar("0")},
                    {"is_sync_tran_subscribed", TypedValue::makeVarchar("0")},
                    {"has_unchecked_assembly_data", TypedValue::makeVarchar("0")},
                    {"text_in_row_limit", TypedValue::makeInt32(0)},
                    {"large_value_types_out_of_row", TypedValue::makeVarchar("0")},
                    {"is_tracked_by_cdc", TypedValue::makeVarchar("0")},
                    {"lock_escalation", TypedValue::makeVarchar("0")},
                    {"lock_escalation_desc", TypedValue::makeVarchar("TABLE")},
                    {"is_filetable", TypedValue::makeVarchar("0")},
                    {"is_memory_optimized", TypedValue::makeVarchar("0")},
                    {"durability", TypedValue::makeVarchar("0")},
                    {"durability_desc", TypedValue::makeVarchar("SCHEMA_AND_DATA")},
                    {"temporal_type", TypedValue::makeVarchar("0")},
                    {"temporal_type_desc", TypedValue::makeVarchar("NON_TEMPORAL_TABLE")},
                    {"history_table_id", TypedValue::makeNull()},
                    {"is_remote_data_archive_enabled", TypedValue::makeVarchar("0")},
                    {"is_external", TypedValue::makeVarchar("0")},
                    {"history_retention_period", TypedValue::makeNull()},
                    {"history_retention_period_unit", TypedValue::makeNull()},
                    {"history_retention_period_unit_desc", TypedValue::makeNull()},
                    {"is_node", TypedValue::makeVarchar("0")},
                    {"is_edge", TypedValue::makeVarchar("0")}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    /**
     * sys.columns - columns
     */
    Status querySysColumns(const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx = nullptr) {
        results.column_names = {
            "object_id", "name", "column_id", "system_type_id", "user_type_id",
            "max_length", "precision", "scale", "collation_name", "is_nullable",
            "is_ansi_padded", "is_rowguidcol", "is_identity", "is_computed",
            "is_filestream", "is_replicated", "is_non_sql_subscribed",
            "is_merge_published", "is_dts_replicated", "is_xml_document",
            "xml_collection_id", "default_object_id", "rule_object_id",
            "is_sparse", "is_column_set", "generated_always_type",
            "generated_always_type_desc", "encryption_type",
            "encryption_type_desc", "encryption_algorithm_name",
            "column_encryption_key_id", "column_encryption_key_database_name",
            "is_hidden", "is_masked", "graph_type", "graph_type_desc"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // object_id
        results.column_types[2] = DataType::INT32;   // column_id
        results.column_types[3] = DataType::INT32;   // system_type_id
        results.column_types[4] = DataType::INT32;   // user_type_id
        results.column_types[5] = DataType::INT16;   // max_length
        results.column_types[6] = DataType::INT32;   // precision
        results.column_types[7] = DataType::INT32;   // scale

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                std::vector<CatalogManager::ColumnInfo> columns;
                s = catalog_manager_->getColumns(table.table_id, columns, ctx);
                if (!s.ok()) continue;

                for (const auto& col : columns) {
                    int32_t typeId = dataTypeToSystemTypeId(col.data_type);

                    VirtualRow row;
                    row.columns = {
                        {"object_id", TypedValue::makeInt32(idToObjectId(table.table_id))},
                        {"name", TypedValue::makeVarchar(col.column_name)},
                        {"column_id", TypedValue::makeInt32(col.ordinal)},
                        {"system_type_id", TypedValue::makeInt32(typeId)},
                        {"user_type_id", TypedValue::makeInt32(typeId)},
                        {"max_length", TypedValue::makeInt16(static_cast<int16_t>(col.max_length > 0 ? col.max_length : -1))},
                        {"precision", TypedValue::makeInt32(col.precision)},
                        {"scale", TypedValue::makeInt32(col.scale)},
                        {"collation_name", TypedValue::makeNull()},
                        {"is_nullable", TypedValue::makeVarchar(col.nullable ? "1" : "0")},
                        {"is_ansi_padded", TypedValue::makeVarchar("1")},
                        {"is_rowguidcol", TypedValue::makeVarchar("0")},
                        {"is_identity", TypedValue::makeVarchar(col.is_identity ? "1" : "0")},
                        {"is_computed", TypedValue::makeVarchar(col.is_computed ? "1" : "0")},
                        {"is_filestream", TypedValue::makeVarchar("0")},
                        {"is_replicated", TypedValue::makeVarchar("0")},
                        {"is_non_sql_subscribed", TypedValue::makeVarchar("0")},
                        {"is_merge_published", TypedValue::makeVarchar("0")},
                        {"is_dts_replicated", TypedValue::makeVarchar("0")},
                        {"is_xml_document", TypedValue::makeVarchar("0")},
                        {"xml_collection_id", TypedValue::makeInt32(0)},
                        {"default_object_id", TypedValue::makeInt32(0)},
                        {"rule_object_id", TypedValue::makeInt32(0)},
                        {"is_sparse", TypedValue::makeVarchar("0")},
                        {"is_column_set", TypedValue::makeVarchar("0")},
                        {"generated_always_type", TypedValue::makeVarchar("0")},
                        {"generated_always_type_desc", TypedValue::makeVarchar("NOT_APPLICABLE")},
                        {"encryption_type", TypedValue::makeNull()},
                        {"encryption_type_desc", TypedValue::makeNull()},
                        {"encryption_algorithm_name", TypedValue::makeNull()},
                        {"column_encryption_key_id", TypedValue::makeNull()},
                        {"column_encryption_key_database_name", TypedValue::makeNull()},
                        {"is_hidden", TypedValue::makeVarchar("0")},
                        {"is_masked", TypedValue::makeVarchar("0")},
                        {"graph_type", TypedValue::makeNull()},
                        {"graph_type_desc", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * sys.indexes - indexes
     */
    Status querySysIndexes(const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx = nullptr) {
        results.column_names = {
            "object_id", "name", "index_id", "type", "type_desc",
            "is_unique", "data_space_id", "ignore_dup_key", "is_primary_key",
            "is_unique_constraint", "fill_factor", "is_padded", "is_disabled",
            "is_hypothetical", "is_ignored_in_optimization", "allow_row_locks",
            "allow_page_locks", "has_filter", "filter_definition",
            "compression_delay", "suppress_dup_key_messages",
            "auto_created"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;  // object_id
        results.column_types[2] = DataType::INT32;  // index_id
        results.column_types[3] = DataType::INT32;  // type
        results.column_types[6] = DataType::INT32;  // data_space_id

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                std::vector<CatalogManager::IndexInfo> indexes;
                s = catalog_manager_->listIndexes(table.table_id, indexes, ctx);
                if (!s.ok()) continue;

                int indexId = 1;
                for (const auto& idx : indexes) {
                    // SQL Server index types: 0=heap, 1=clustered, 2=nonclustered
                    int type = 2;  // Default nonclustered
                    std::string typeDesc = "NONCLUSTERED";
                    if (idx.is_clustered) {
                        type = 1;
                        typeDesc = "CLUSTERED";
                    }

                    VirtualRow row;
                    row.columns = {
                        {"object_id", TypedValue::makeInt32(idToObjectId(table.table_id))},
                        {"name", TypedValue::makeVarchar(idx.index_name)},
                        {"index_id", TypedValue::makeInt32(indexId++)},
                        {"type", TypedValue::makeInt32(type)},
                        {"type_desc", TypedValue::makeVarchar(typeDesc)},
                        {"is_unique", TypedValue::makeVarchar(idx.is_unique ? "1" : "0")},
                        {"data_space_id", TypedValue::makeInt32(1)},
                        {"ignore_dup_key", TypedValue::makeVarchar("0")},
                        {"is_primary_key", TypedValue::makeVarchar(idx.is_primary_key ? "1" : "0")},
                        {"is_unique_constraint", TypedValue::makeVarchar(idx.is_unique && !idx.is_primary_key ? "1" : "0")},
                        {"fill_factor", TypedValue::makeVarchar("0")},
                        {"is_padded", TypedValue::makeVarchar("0")},
                        {"is_disabled", TypedValue::makeVarchar("0")},
                        {"is_hypothetical", TypedValue::makeVarchar("0")},
                        {"is_ignored_in_optimization", TypedValue::makeVarchar("0")},
                        {"allow_row_locks", TypedValue::makeVarchar("1")},
                        {"allow_page_locks", TypedValue::makeVarchar("1")},
                        {"has_filter", TypedValue::makeVarchar(idx.filter_expression.empty() ? "0" : "1")},
                        {"filter_definition", idx.filter_expression.empty() ?
                            TypedValue::makeNull() : TypedValue::makeVarchar(idx.filter_expression)},
                        {"compression_delay", TypedValue::makeNull()},
                        {"suppress_dup_key_messages", TypedValue::makeVarchar("0")},
                        {"auto_created", TypedValue::makeVarchar("0")}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * sys.types - data types
     */
    Status querySysTypes(const std::string& where_clause,
                         VirtualResultSet& results,
                         ErrorContext* ctx = nullptr) {
        results.column_names = {
            "name", "system_type_id", "user_type_id", "schema_id",
            "principal_id", "max_length", "precision", "scale",
            "collation_name", "is_nullable", "is_user_defined",
            "is_assembly_type", "default_object_id", "rule_object_id",
            "is_table_type"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[1] = DataType::INT32;  // system_type_id
        results.column_types[2] = DataType::INT32;  // user_type_id
        results.column_types[3] = DataType::INT32;  // schema_id
        results.column_types[5] = DataType::INT16;  // max_length

        // Built-in SQL Server types
        struct MSSQLTypeInfo {
            const char* name;
            int32_t typeId;
            int16_t maxLen;
        };

        MSSQLTypeInfo builtinTypes[] = {
            {"bit", 104, 1},
            {"tinyint", 48, 1},
            {"smallint", 52, 2},
            {"int", 56, 4},
            {"bigint", 127, 8},
            {"real", 59, 4},
            {"float", 62, 8},
            {"decimal", 106, 17},
            {"numeric", 108, 17},
            {"money", 60, 8},
            {"smallmoney", 122, 4},
            {"char", 175, 8000},
            {"varchar", 167, 8000},
            {"nchar", 239, 4000},
            {"nvarchar", 231, 4000},
            {"text", 35, 16},
            {"ntext", 99, 16},
            {"binary", 173, 8000},
            {"varbinary", 165, 8000},
            {"image", 34, 16},
            {"date", 40, 3},
            {"time", 41, 5},
            {"datetime", 61, 8},
            {"datetime2", 42, 8},
            {"datetimeoffset", 43, 10},
            {"smalldatetime", 58, 4},
            {"uniqueidentifier", 36, 16},
            {"xml", 241, -1}
        };

        for (const auto& t : builtinTypes) {
            VirtualRow row;
            row.columns = {
                {"name", TypedValue::makeVarchar(t.name)},
                {"system_type_id", TypedValue::makeInt32(t.typeId)},
                {"user_type_id", TypedValue::makeInt32(t.typeId)},
                {"schema_id", TypedValue::makeInt32(4)},  // sys schema
                {"principal_id", TypedValue::makeNull()},
                {"max_length", TypedValue::makeInt16(t.maxLen)},
                {"precision", TypedValue::makeVarchar("0")},
                {"scale", TypedValue::makeVarchar("0")},
                {"collation_name", TypedValue::makeNull()},
                {"is_nullable", TypedValue::makeVarchar("1")},
                {"is_user_defined", TypedValue::makeVarchar("0")},
                {"is_assembly_type", TypedValue::makeVarchar("0")},
                {"default_object_id", TypedValue::makeInt32(0)},
                {"rule_object_id", TypedValue::makeInt32(0)},
                {"is_table_type", TypedValue::makeVarchar("0")}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * sys.objects - all database objects
     */
    Status querySysObjects(const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx = nullptr) {
        results.column_names = {
            "name", "object_id", "principal_id", "schema_id", "parent_object_id",
            "type", "type_desc", "create_date", "modify_date", "is_ms_shipped",
            "is_published", "is_schema_published"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[1] = DataType::INT32;   // object_id
        results.column_types[2] = DataType::INT32;   // principal_id
        results.column_types[3] = DataType::INT32;   // schema_id
        results.column_types[4] = DataType::INT32;   // parent_object_id

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Tables
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (s.ok()) {
                for (const auto& table : tables) {
                    VirtualRow row;
                    row.columns = {
                        {"name", TypedValue::makeVarchar(table.table_name)},
                        {"object_id", TypedValue::makeInt32(idToObjectId(table.table_id))},
                        {"principal_id", TypedValue::makeNull()},
                        {"schema_id", TypedValue::makeInt32(idToObjectId(schema.schema_id))},
                        {"parent_object_id", TypedValue::makeInt32(0)},
                        {"type", TypedValue::makeVarchar("U ")},  // User table
                        {"type_desc", TypedValue::makeVarchar("USER_TABLE")},
                        {"create_date", TypedValue::makeNull()},
                        {"modify_date", TypedValue::makeNull()},
                        {"is_ms_shipped", TypedValue::makeVarchar("0")},
                        {"is_published", TypedValue::makeVarchar("0")},
                        {"is_schema_published", TypedValue::makeVarchar("0")}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Views
            std::vector<CatalogManager::ViewInfo> views;
            s = catalog_manager_->listViews(schema.schema_id, views, ctx);
            if (s.ok()) {
                for (const auto& view : views) {
                    VirtualRow row;
                    row.columns = {
                        {"name", TypedValue::makeVarchar(view.view_name)},
                        {"object_id", TypedValue::makeInt32(idToObjectId(view.view_id))},
                        {"principal_id", TypedValue::makeNull()},
                        {"schema_id", TypedValue::makeInt32(idToObjectId(schema.schema_id))},
                        {"parent_object_id", TypedValue::makeInt32(0)},
                        {"type", TypedValue::makeVarchar("V ")},  // View
                        {"type_desc", TypedValue::makeVarchar("VIEW")},
                        {"create_date", TypedValue::makeNull()},
                        {"modify_date", TypedValue::makeNull()},
                        {"is_ms_shipped", TypedValue::makeVarchar("0")},
                        {"is_published", TypedValue::makeVarchar("0")},
                        {"is_schema_published", TypedValue::makeVarchar("0")}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Procedures
            std::vector<CatalogManager::ProcedureInfo> procedures;
            s = catalog_manager_->listProcedures(schema.schema_id, procedures, ctx);
            if (s.ok()) {
                for (const auto& proc : procedures) {
                    VirtualRow row;
                    row.columns = {
                        {"name", TypedValue::makeVarchar(proc.procedure_name)},
                        {"object_id", TypedValue::makeInt32(idToObjectId(proc.procedure_id))},
                        {"principal_id", TypedValue::makeNull()},
                        {"schema_id", TypedValue::makeInt32(idToObjectId(schema.schema_id))},
                        {"parent_object_id", TypedValue::makeInt32(0)},
                        {"type", TypedValue::makeVarchar("P ")},  // Stored procedure
                        {"type_desc", TypedValue::makeVarchar("SQL_STORED_PROCEDURE")},
                        {"create_date", TypedValue::makeNull()},
                        {"modify_date", TypedValue::makeNull()},
                        {"is_ms_shipped", TypedValue::makeVarchar("0")},
                        {"is_published", TypedValue::makeVarchar("0")},
                        {"is_schema_published", TypedValue::makeVarchar("0")}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Functions
            std::vector<CatalogManager::FunctionInfo> functions;
            s = catalog_manager_->listFunctions(schema.schema_id, functions, ctx);
            if (s.ok()) {
                for (const auto& func : functions) {
                    VirtualRow row;
                    row.columns = {
                        {"name", TypedValue::makeVarchar(func.function_name)},
                        {"object_id", TypedValue::makeInt32(idToObjectId(func.function_id))},
                        {"principal_id", TypedValue::makeNull()},
                        {"schema_id", TypedValue::makeInt32(idToObjectId(schema.schema_id))},
                        {"parent_object_id", TypedValue::makeInt32(0)},
                        {"type", TypedValue::makeVarchar("FN")},  // Scalar function
                        {"type_desc", TypedValue::makeVarchar("SQL_SCALAR_FUNCTION")},
                        {"create_date", TypedValue::makeNull()},
                        {"modify_date", TypedValue::makeNull()},
                        {"is_ms_shipped", TypedValue::makeVarchar("0")},
                        {"is_published", TypedValue::makeVarchar("0")},
                        {"is_schema_published", TypedValue::makeVarchar("0")}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * sys.procedures - stored procedures
     */
    Status querySysProcedures(const std::string& where_clause,
                              VirtualResultSet& results,
                              ErrorContext* ctx = nullptr) {
        results.column_names = {
            "name", "object_id", "principal_id", "schema_id", "parent_object_id",
            "type", "type_desc", "create_date", "modify_date", "is_ms_shipped",
            "is_published", "is_schema_published", "is_auto_executed",
            "is_execution_replicated", "is_repl_serializable_only",
            "skips_repl_constraints"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[1] = DataType::INT32;   // object_id
        results.column_types[3] = DataType::INT32;   // schema_id
        results.column_types[4] = DataType::INT32;   // parent_object_id

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::ProcedureInfo> procedures;
            s = catalog_manager_->listProcedures(schema.schema_id, procedures, ctx);
            if (!s.ok()) continue;

            for (const auto& proc : procedures) {
                VirtualRow row;
                row.columns = {
                    {"name", TypedValue::makeVarchar(proc.procedure_name)},
                    {"object_id", TypedValue::makeInt32(idToObjectId(proc.procedure_id))},
                    {"principal_id", TypedValue::makeNull()},
                    {"schema_id", TypedValue::makeInt32(idToObjectId(schema.schema_id))},
                    {"parent_object_id", TypedValue::makeInt32(0)},
                    {"type", TypedValue::makeVarchar("P ")},
                    {"type_desc", TypedValue::makeVarchar("SQL_STORED_PROCEDURE")},
                    {"create_date", TypedValue::makeNull()},
                    {"modify_date", TypedValue::makeNull()},
                    {"is_ms_shipped", TypedValue::makeVarchar("0")},
                    {"is_published", TypedValue::makeVarchar("0")},
                    {"is_schema_published", TypedValue::makeVarchar("0")},
                    {"is_auto_executed", TypedValue::makeVarchar("0")},
                    {"is_execution_replicated", TypedValue::makeVarchar("0")},
                    {"is_repl_serializable_only", TypedValue::makeVarchar("0")},
                    {"skips_repl_constraints", TypedValue::makeVarchar("0")}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    /**
     * sys.sql_modules - module definitions
     */
    Status querySysSqlModules(const std::string& where_clause,
                              VirtualResultSet& results,
                              ErrorContext* ctx = nullptr) {
        results.column_names = {
            "object_id", "definition", "uses_ansi_nulls", "uses_quoted_identifier",
            "is_schema_bound", "uses_database_collation", "is_recompiled",
            "null_on_null_input", "execute_as_principal_id", "uses_native_compilation",
            "inline_type", "is_inlineable"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // object_id
        results.column_types[8] = DataType::INT32;   // execute_as_principal_id

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Views
            std::vector<CatalogManager::ViewInfo> views;
            s = catalog_manager_->listViews(schema.schema_id, views, ctx);
            if (s.ok()) {
                for (const auto& view : views) {
                    VirtualRow row;
                    row.columns = {
                        {"object_id", TypedValue::makeInt32(idToObjectId(view.view_id))},
                        {"definition", TypedValue::makeVarchar("CREATE VIEW " + view.view_name + " AS " + view.definition)},
                        {"uses_ansi_nulls", TypedValue::makeVarchar("1")},
                        {"uses_quoted_identifier", TypedValue::makeVarchar("1")},
                        {"is_schema_bound", TypedValue::makeVarchar("0")},
                        {"uses_database_collation", TypedValue::makeVarchar("1")},
                        {"is_recompiled", TypedValue::makeVarchar("0")},
                        {"null_on_null_input", TypedValue::makeNull()},
                        {"execute_as_principal_id", TypedValue::makeNull()},
                        {"uses_native_compilation", TypedValue::makeVarchar("0")},
                        {"inline_type", TypedValue::makeNull()},
                        {"is_inlineable", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Procedures
            std::vector<CatalogManager::ProcedureInfo> procedures;
            s = catalog_manager_->listProcedures(schema.schema_id, procedures, ctx);
            if (s.ok()) {
                for (const auto& proc : procedures) {
                    VirtualRow row;
                    row.columns = {
                        {"object_id", TypedValue::makeInt32(idToObjectId(proc.procedure_id))},
                        {"definition", TypedValue::makeVarchar("CREATE PROCEDURE " + proc.procedure_name + " AS " + proc.body)},
                        {"uses_ansi_nulls", TypedValue::makeVarchar("1")},
                        {"uses_quoted_identifier", TypedValue::makeVarchar("1")},
                        {"is_schema_bound", TypedValue::makeVarchar("0")},
                        {"uses_database_collation", TypedValue::makeVarchar("1")},
                        {"is_recompiled", TypedValue::makeVarchar("0")},
                        {"null_on_null_input", TypedValue::makeNull()},
                        {"execute_as_principal_id", TypedValue::makeNull()},
                        {"uses_native_compilation", TypedValue::makeVarchar("0")},
                        {"inline_type", TypedValue::makeNull()},
                        {"is_inlineable", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Functions
            std::vector<CatalogManager::FunctionInfo> functions;
            s = catalog_manager_->listFunctions(schema.schema_id, functions, ctx);
            if (s.ok()) {
                for (const auto& func : functions) {
                    VirtualRow row;
                    row.columns = {
                        {"object_id", TypedValue::makeInt32(idToObjectId(func.function_id))},
                        {"definition", TypedValue::makeVarchar("CREATE FUNCTION " + func.name + " AS " + func.source_text)},
                        {"uses_ansi_nulls", TypedValue::makeVarchar("1")},
                        {"uses_quoted_identifier", TypedValue::makeVarchar("1")},
                        {"is_schema_bound", TypedValue::makeVarchar("0")},
                        {"uses_database_collation", TypedValue::makeVarchar("1")},
                        {"is_recompiled", TypedValue::makeVarchar("0")},
                        {"null_on_null_input", TypedValue::makeVarchar(func.deterministic ? "1" : "0")},
                        {"execute_as_principal_id", TypedValue::makeNull()},
                        {"uses_native_compilation", TypedValue::makeVarchar("0")},
                        {"inline_type", TypedValue::makeNull()},
                        {"is_inlineable", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    // ========================================================================
    // Column Definition Helpers
    // ========================================================================

    Status getSysSchemasColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("name", DataType::VARCHAR, 1),
            makeColumnInfo("schema_id", DataType::INT32, 2),
            makeColumnInfo("principal_id", DataType::INT32, 3)
        };
        return Status::OK;
    }

    Status getSysTablesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("name", DataType::VARCHAR, 1),
            makeColumnInfo("object_id", DataType::INT32, 2),
            makeColumnInfo("schema_id", DataType::INT32, 3),
            makeColumnInfo("type", DataType::CHAR, 4),
            makeColumnInfo("type_desc", DataType::VARCHAR, 5)
        };
        return Status::OK;
    }

    Status getSysColumnsColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("object_id", DataType::INT32, 1),
            makeColumnInfo("name", DataType::VARCHAR, 2),
            makeColumnInfo("column_id", DataType::INT32, 3),
            makeColumnInfo("system_type_id", DataType::INT32, 4),
            makeColumnInfo("is_nullable", DataType::BOOLEAN, 5)
        };
        return Status::OK;
    }

    Status getSysIndexesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("object_id", DataType::INT32, 1),
            makeColumnInfo("name", DataType::VARCHAR, 2),
            makeColumnInfo("index_id", DataType::INT32, 3),
            makeColumnInfo("type_desc", DataType::VARCHAR, 4),
            makeColumnInfo("is_unique", DataType::BOOLEAN, 5)
        };
        return Status::OK;
    }

    Status getSysTypesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("name", DataType::VARCHAR, 1),
            makeColumnInfo("system_type_id", DataType::INT32, 2),
            makeColumnInfo("user_type_id", DataType::INT32, 3),
            makeColumnInfo("max_length", DataType::INT16, 4)
        };
        return Status::OK;
    }

    Status getSysObjectsColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("name", DataType::VARCHAR, 1),
            makeColumnInfo("object_id", DataType::INT32, 2),
            makeColumnInfo("schema_id", DataType::INT32, 3),
            makeColumnInfo("type", DataType::CHAR, 4),
            makeColumnInfo("type_desc", DataType::VARCHAR, 5)
        };
        return Status::OK;
    }

    Status getSysProceduresColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("name", DataType::VARCHAR, 1),
            makeColumnInfo("object_id", DataType::INT32, 2),
            makeColumnInfo("schema_id", DataType::INT32, 3),
            makeColumnInfo("type_desc", DataType::VARCHAR, 4)
        };
        return Status::OK;
    }

    Status getSysSqlModulesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("object_id", DataType::INT32, 1),
            makeColumnInfo("definition", DataType::VARCHAR, 2),
            makeColumnInfo("uses_ansi_nulls", DataType::BOOLEAN, 3)
        };
        return Status::OK;
    }

    CatalogManager::ColumnInfo makeColumnInfo(const std::string& name,
                                               DataType type,
                                               int ordinal) {
        CatalogManager::ColumnInfo col;
        col.column_name = name;
        col.data_type = static_cast<uint16_t>(type);
        col.ordinal = ordinal;
        col.nullable = true;
        col.max_length = (type == DataType::VARCHAR) ? 256 : 0;
        return col;
    }
};

} // namespace scratchbird::catalog
