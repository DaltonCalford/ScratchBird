/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/duckdb_catalog.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

DuckDBCatalogHandler::DuckDBCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType DuckDBCatalogHandler::getProtocolType() const {
    return ProtocolType::DUCKDB;
}

bool DuckDBCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "duckdb_catalog");
}

bool DuckDBCatalogHandler::ownsTable(const std::string& schema_name,
                                     const std::string& table_name) const {
    if (!ownsSchema(schema_name)) {
        return false;
    }
    for (const auto& name : table_names_) {
        if (equalsCaseInsensitive(name, table_name)) {
            return true;
        }
    }
    return false;
}

Status DuckDBCatalogHandler::queryTable(const std::string& schema_name,
                                        const std::string& table_name,
                                        const std::string& /*where_clause*/,
                                        VirtualResultSet& results,
                                        ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    if (!ownsTable(schema_name, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }

    const ColumnDefs* def = getTableDefinition(table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Unknown table: " + table_name).c_str());
        return Status::NOT_FOUND;
    }
    setResultColumns(*def, results);

    if (equalsCaseInsensitive(table_name, "duckdb_tables")) {
        return queryDuckdbTables(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "duckdb_columns")) {
        return queryDuckdbColumns(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "duckdb_views")) {
        return queryDuckdbViews(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "duckdb_types")) {
        return queryDuckdbTypes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "duckdb_external_files")) {
        return queryDuckdbExternalFiles(results, ctx);
    }
    return Status::OK;
}

Status DuckDBCatalogHandler::getTableColumns(
    const std::string& schema_name,
    const std::string& table_name,
    std::vector<CatalogManager::ColumnInfo>& columns,
    ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    if (!ownsTable(schema_name, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }
    const ColumnDefs* def = getTableDefinition(table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Unknown table: " + table_name).c_str());
        return Status::NOT_FOUND;
    }
    setColumnInfo(*def, columns);
    return Status::OK;
}

Status DuckDBCatalogHandler::listTables(const std::string& schema_name,
                                        std::vector<std::string>& table_names,
                                        ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status DuckDBCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                         ErrorContext* /*ctx*/) {
    schema_names = {"duckdb_catalog"};
    return Status::OK;
}

void DuckDBCatalogHandler::initializeTableNames() {
    table_names_ = {"duckdb_tables", "duckdb_columns", "duckdb_views", "duckdb_types",
                    "duckdb_external_files"};
}

const DuckDBCatalogHandler::ColumnDefs* DuckDBCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"duckdb_tables",
         {
             {"database_name", DataType::VARCHAR, false},
             {"schema_name", DataType::VARCHAR, false},
             {"table_name", DataType::VARCHAR, false},
             {"table_oid", DataType::VARCHAR, false},
             {"temporary", DataType::BOOLEAN, false},
             {"column_count", DataType::INT32, false},
         }},
        {"duckdb_columns",
         {
             {"database_name", DataType::VARCHAR, false},
             {"schema_name", DataType::VARCHAR, false},
             {"table_name", DataType::VARCHAR, false},
             {"column_name", DataType::VARCHAR, false},
             {"column_index", DataType::INT32, false},
             {"data_type", DataType::VARCHAR, false},
         }},
        {"duckdb_views",
         {
             {"database_name", DataType::VARCHAR, false},
             {"schema_name", DataType::VARCHAR, false},
             {"view_name", DataType::VARCHAR, false},
             {"sql", DataType::TEXT, false},
         }},
        {"duckdb_types",
         {
             {"database_name", DataType::VARCHAR, false},
             {"schema_name", DataType::VARCHAR, false},
             {"type_name", DataType::VARCHAR, false},
             {"type_kind", DataType::VARCHAR, false},
         }},
        {"duckdb_external_files",
         {
             {"database_name", DataType::VARCHAR, false},
             {"schema_name", DataType::VARCHAR, false},
             {"object_name", DataType::VARCHAR, false},
             {"source_uri", DataType::TEXT, false},
             {"format", DataType::VARCHAR, false},
         }},
    };
    std::string key = table_name;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto it = kDefs.find(key);
    if (it == kDefs.end()) {
        return nullptr;
    }
    return &it->second;
}

void DuckDBCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void DuckDBCatalogHandler::setColumnInfo(const ColumnDefs& def,
                                         std::vector<CatalogManager::ColumnInfo>& columns) const {
    columns.clear();
    uint16_t ordinal = 0;
    for (const auto& col : def) {
        CatalogManager::ColumnInfo info{};
        info.column_name = col.name;
        info.ordinal = ++ordinal;
        info.data_type = static_cast<uint16_t>(col.type);
        info.nullable = col.nullable;
        columns.push_back(std::move(info));
    }
}

std::string DuckDBCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

std::string DuckDBCatalogHandler::toDuckType(const CatalogManager::ColumnInfo& col) {
    const DataType type = static_cast<DataType>(col.data_type);
    switch (type) {
        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
            return "INTEGER";
        case DataType::INT64:
            return "BIGINT";
        case DataType::FLOAT32:
        case DataType::FLOAT64:
            return "DOUBLE";
        case DataType::DECIMAL:
            return "DECIMAL";
        case DataType::BOOLEAN:
            return "BOOLEAN";
        case DataType::DATE:
            return "DATE";
        case DataType::TIMESTAMP:
        case DataType::TIMESTAMP_WITH_ZONE:
            return "TIMESTAMP";
        case DataType::UUID:
            return "UUID";
        case DataType::TEXT:
        case DataType::CHAR:
        case DataType::VARCHAR:
            return "VARCHAR";
        default:
            return "VARCHAR";
    }
}

Status DuckDBCatalogHandler::queryDuckdbTables(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK) {
        return status;
    }
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        const std::string schema_name = normalizeSchemaName(schema);
        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"database_name", TypedValue::makeVarchar("main")},
                {"schema_name", TypedValue::makeVarchar(schema_name)},
                {"table_name", TypedValue::makeVarchar(table.table_name)},
                {"table_oid", TypedValue::makeVarchar(table.table_id.toString())},
                {"temporary", TypedValue::makeBoolean(table.temp_metadata_scope != CatalogManager::TempMetadataScope::NONE)},
                {"column_count", TypedValue::makeInt32(static_cast<int32_t>(table.column_count))},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status DuckDBCatalogHandler::queryDuckdbColumns(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        const std::string schema_name = normalizeSchemaName(schema);
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<CatalogManager::ColumnInfo> columns;
            if (catalog_manager_->getColumns(table.table_id, columns, ctx) != Status::OK) {
                continue;
            }
            for (const auto& col : columns) {
                VirtualRow row;
                row.columns = {
                    {"database_name", TypedValue::makeVarchar("main")},
                    {"schema_name", TypedValue::makeVarchar(schema_name)},
                    {"table_name", TypedValue::makeVarchar(table.table_name)},
                    {"column_name", TypedValue::makeVarchar(col.column_name)},
                    {"column_index", TypedValue::makeInt32(static_cast<int32_t>(col.ordinal - 1))},
                    {"data_type", TypedValue::makeVarchar(toDuckType(col))},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status DuckDBCatalogHandler::queryDuckdbViews(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::ViewInfo> views;
        if (catalog_manager_->listViewsForSchema(schema.schema_id, views, ctx) != Status::OK) {
            continue;
        }
        const std::string schema_name = normalizeSchemaName(schema);
        for (const auto& view : views) {
            VirtualRow row;
            row.columns = {
                {"database_name", TypedValue::makeVarchar("main")},
                {"schema_name", TypedValue::makeVarchar(schema_name)},
                {"view_name", TypedValue::makeVarchar(view.name)},
                {"sql", TypedValue::makeText(view.definition)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status DuckDBCatalogHandler::queryDuckdbTypes(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TypeCatalogInfo> types;
        if (catalog_manager_->listTypeCatalogEntries(schema.schema_id, types, ctx) != Status::OK) {
            continue;
        }
        const std::string schema_name = normalizeSchemaName(schema);
        for (const auto& type : types) {
            VirtualRow row;
            row.columns = {
                {"database_name", TypedValue::makeVarchar("main")},
                {"schema_name", TypedValue::makeVarchar(schema_name)},
                {"type_name", TypedValue::makeVarchar(type.type_name)},
                {"type_kind", TypedValue::makeVarchar(std::to_string(static_cast<int>(type.type_kind)))},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status DuckDBCatalogHandler::queryDuckdbExternalFiles(VirtualResultSet& /*results*/,
                                                      ErrorContext* /*ctx*/) {
    return Status::OK;
}

} // namespace scratchbird::catalog

