/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/clickhouse_catalog.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

ClickHouseCatalogHandler::ClickHouseCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType ClickHouseCatalogHandler::getProtocolType() const {
    return ProtocolType::CLICKHOUSE;
}

bool ClickHouseCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "system");
}

bool ClickHouseCatalogHandler::ownsTable(const std::string& schema_name,
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

Status ClickHouseCatalogHandler::queryTable(const std::string& schema_name,
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

    if (equalsCaseInsensitive(table_name, "databases")) {
        return queryDatabases(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "tables")) {
        return queryTables(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "columns")) {
        return queryColumns(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "parts")) {
        return queryParts(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "codecs")) {
        return queryCodecs(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "ttl_policies")) {
        return queryTtlPolicies(results, ctx);
    }

    return Status::OK;
}

Status ClickHouseCatalogHandler::getTableColumns(
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

Status ClickHouseCatalogHandler::listTables(const std::string& schema_name,
                                            std::vector<std::string>& table_names,
                                            ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status ClickHouseCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                             ErrorContext* /*ctx*/) {
    schema_names = {"system"};
    return Status::OK;
}

void ClickHouseCatalogHandler::initializeTableNames() {
    table_names_ = {"databases", "tables", "columns", "parts", "codecs", "ttl_policies"};
}

const ClickHouseCatalogHandler::ColumnDefs* ClickHouseCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"databases",
         {
             {"name", DataType::VARCHAR, false},
             {"engine", DataType::VARCHAR, false},
             {"uuid", DataType::VARCHAR, false},
         }},
        {"tables",
         {
             {"database", DataType::VARCHAR, false},
             {"name", DataType::VARCHAR, false},
             {"engine", DataType::VARCHAR, false},
             {"uuid", DataType::VARCHAR, false},
             {"total_rows", DataType::INT64, false},
         }},
        {"columns",
         {
             {"database", DataType::VARCHAR, false},
             {"table", DataType::VARCHAR, false},
             {"name", DataType::VARCHAR, false},
             {"type", DataType::VARCHAR, false},
             {"default_kind", DataType::VARCHAR, true},
             {"compression_codec", DataType::VARCHAR, true},
         }},
        {"parts",
         {
             {"database", DataType::VARCHAR, false},
             {"table", DataType::VARCHAR, false},
             {"partition", DataType::VARCHAR, false},
             {"name", DataType::VARCHAR, false},
             {"rows", DataType::INT64, false},
             {"active", DataType::BOOLEAN, false},
         }},
        {"codecs",
         {
             {"database", DataType::VARCHAR, false},
             {"table", DataType::VARCHAR, false},
             {"column", DataType::VARCHAR, false},
             {"codec_expression", DataType::VARCHAR, false},
         }},
        {"ttl_policies",
         {
             {"database", DataType::VARCHAR, false},
             {"table", DataType::VARCHAR, false},
             {"ttl_expression", DataType::TEXT, true},
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

void ClickHouseCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void ClickHouseCatalogHandler::setColumnInfo(const ColumnDefs& def,
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

std::string ClickHouseCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

std::string ClickHouseCatalogHandler::toClickHouseType(const CatalogManager::ColumnInfo& col) {
    const DataType type = static_cast<DataType>(col.data_type);
    switch (type) {
        case DataType::INT8: return "Int8";
        case DataType::INT16: return "Int16";
        case DataType::INT32: return "Int32";
        case DataType::INT64: return "Int64";
        case DataType::UINT8: return "UInt8";
        case DataType::UINT16: return "UInt16";
        case DataType::UINT32: return "UInt32";
        case DataType::UINT64: return "UInt64";
        case DataType::FLOAT32: return "Float32";
        case DataType::FLOAT64: return "Float64";
        case DataType::DECIMAL: return "Decimal";
        case DataType::BOOLEAN: return "UInt8";
        case DataType::DATE: return "Date";
        case DataType::TIMESTAMP:
        case DataType::TIMESTAMP_WITH_ZONE: return "DateTime64";
        case DataType::UUID: return "UUID";
        case DataType::TEXT:
        case DataType::CHAR:
        case DataType::VARCHAR: return "String";
        default: return "String";
    }
}

Status ClickHouseCatalogHandler::queryDatabases(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK) {
        return status;
    }
    for (const auto& schema : schemas) {
        VirtualRow row;
        row.columns = {
            {"name", TypedValue::makeVarchar(normalizeSchemaName(schema))},
            {"engine", TypedValue::makeVarchar("Atomic")},
            {"uuid", TypedValue::makeVarchar(schema.schema_id.toString())},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status ClickHouseCatalogHandler::queryTables(VirtualResultSet& results, ErrorContext* ctx) {
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
        const std::string db_name = normalizeSchemaName(schema);
        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"database", TypedValue::makeVarchar(db_name)},
                {"name", TypedValue::makeVarchar(table.table_name)},
                {"engine", TypedValue::makeVarchar("MergeTree")},
                {"uuid", TypedValue::makeVarchar(table.table_id.toString())},
                {"total_rows", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status ClickHouseCatalogHandler::queryColumns(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK) {
        return status;
    }
    for (const auto& schema : schemas) {
        const std::string db_name = normalizeSchemaName(schema);
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
                    {"database", TypedValue::makeVarchar(db_name)},
                    {"table", TypedValue::makeVarchar(table.table_name)},
                    {"name", TypedValue::makeVarchar(col.column_name)},
                    {"type", TypedValue::makeVarchar(toClickHouseType(col))},
                    {"default_kind", TypedValue()},
                    {"compression_codec", TypedValue::makeVarchar("LZ4")},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status ClickHouseCatalogHandler::queryParts(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK) {
        return status;
    }
    for (const auto& schema : schemas) {
        const std::string db_name = normalizeSchemaName(schema);
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"database", TypedValue::makeVarchar(db_name)},
                {"table", TypedValue::makeVarchar(table.table_name)},
                {"partition", TypedValue::makeVarchar("all")},
                {"name", TypedValue::makeVarchar(table.table_name + "_part_0")},
                {"rows", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
                {"active", TypedValue::makeBoolean(true)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status ClickHouseCatalogHandler::queryCodecs(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        const std::string db_name = normalizeSchemaName(schema);
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
                    {"database", TypedValue::makeVarchar(db_name)},
                    {"table", TypedValue::makeVarchar(table.table_name)},
                    {"column", TypedValue::makeVarchar(col.column_name)},
                    {"codec_expression", TypedValue::makeVarchar("CODEC(LZ4)")},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status ClickHouseCatalogHandler::queryTtlPolicies(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        const std::string db_name = normalizeSchemaName(schema);
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"database", TypedValue::makeVarchar(db_name)},
                {"table", TypedValue::makeVarchar(table.table_name)},
                {"ttl_expression", TypedValue()},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

} // namespace scratchbird::catalog

