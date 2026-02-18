/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/influxdb_catalog.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

InfluxDBCatalogHandler::InfluxDBCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType InfluxDBCatalogHandler::getProtocolType() const {
    return ProtocolType::INFLUXDB;
}

bool InfluxDBCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "influxdb_meta");
}

bool InfluxDBCatalogHandler::ownsTable(const std::string& schema_name,
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

Status InfluxDBCatalogHandler::queryTable(const std::string& schema_name,
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

    if (equalsCaseInsensitive(table_name, "measurements")) {
        return queryMeasurements(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "series")) {
        return querySeries(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "retention_policies")) {
        return queryRetentionPolicies(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "cache_descriptors")) {
        return queryCacheDescriptors(results, ctx);
    }
    return Status::OK;
}

Status InfluxDBCatalogHandler::getTableColumns(
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

Status InfluxDBCatalogHandler::listTables(const std::string& schema_name,
                                          std::vector<std::string>& table_names,
                                          ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status InfluxDBCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                           ErrorContext* /*ctx*/) {
    schema_names = {"influxdb_meta"};
    return Status::OK;
}

void InfluxDBCatalogHandler::initializeTableNames() {
    table_names_ = {"measurements", "series", "retention_policies", "cache_descriptors"};
}

const InfluxDBCatalogHandler::ColumnDefs* InfluxDBCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"measurements",
         {
             {"measurement_id", DataType::VARCHAR, false},
             {"measurement_name", DataType::VARCHAR, false},
             {"retention_policy", DataType::VARCHAR, false},
         }},
        {"series",
         {
             {"measurement_name", DataType::VARCHAR, false},
             {"series_key", DataType::TEXT, false},
             {"field_count", DataType::INT32, false},
             {"tag_count", DataType::INT32, false},
         }},
        {"retention_policies",
         {
             {"policy_name", DataType::VARCHAR, false},
             {"duration_ns", DataType::INT64, false},
             {"shard_group_duration_ns", DataType::INT64, false},
             {"replication", DataType::INT32, false},
         }},
        {"cache_descriptors",
         {
             {"measurement_name", DataType::VARCHAR, false},
             {"cache_enabled", DataType::BOOLEAN, false},
             {"max_memory_bytes", DataType::INT64, false},
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

void InfluxDBCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void InfluxDBCatalogHandler::setColumnInfo(const ColumnDefs& def,
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

std::string InfluxDBCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

Status InfluxDBCatalogHandler::queryMeasurements(VirtualResultSet& results, ErrorContext* ctx) {
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
        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"measurement_id", TypedValue::makeVarchar(table.table_id.toString())},
                {"measurement_name", TypedValue::makeVarchar(normalizeSchemaName(schema) + "." + table.table_name)},
                {"retention_policy", TypedValue::makeVarchar("autogen")},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status InfluxDBCatalogHandler::querySeries(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<CatalogManager::ColumnInfo> columns;
            if (catalog_manager_->getColumns(table.table_id, columns, ctx) != Status::OK) {
                continue;
            }
            VirtualRow row;
            row.columns = {
                {"measurement_name", TypedValue::makeVarchar(normalizeSchemaName(schema) + "." + table.table_name)},
                {"series_key", TypedValue::makeText(table.table_id.toString())},
                {"field_count", TypedValue::makeInt32(static_cast<int32_t>(columns.size()))},
                {"tag_count", TypedValue::makeInt32(0)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status InfluxDBCatalogHandler::queryRetentionPolicies(VirtualResultSet& results, ErrorContext* /*ctx*/) {
    VirtualRow row;
    row.columns = {
        {"policy_name", TypedValue::makeVarchar("autogen")},
        {"duration_ns", TypedValue::makeInt64(0)},
        {"shard_group_duration_ns", TypedValue::makeInt64(3600000000000LL)},
        {"replication", TypedValue::makeInt32(1)},
    };
    results.rows.push_back(std::move(row));
    return Status::OK;
}

Status InfluxDBCatalogHandler::queryCacheDescriptors(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"measurement_name", TypedValue::makeVarchar(normalizeSchemaName(schema) + "." + table.table_name)},
                {"cache_enabled", TypedValue::makeBoolean(true)},
                {"max_memory_bytes", TypedValue::makeInt64(67108864)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

} // namespace scratchbird::catalog

