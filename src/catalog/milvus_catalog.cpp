/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/milvus_catalog.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

MilvusCatalogHandler::MilvusCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType MilvusCatalogHandler::getProtocolType() const {
    return ProtocolType::MILVUS;
}

bool MilvusCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "milvus_meta");
}

bool MilvusCatalogHandler::ownsTable(const std::string& schema_name,
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

Status MilvusCatalogHandler::queryTable(const std::string& schema_name,
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

    if (equalsCaseInsensitive(table_name, "collections")) {
        return queryCollections(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "partitions")) {
        return queryPartitions(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "aliases")) {
        return queryAliases(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "indexes")) {
        return queryIndexes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "collection_stats")) {
        return queryCollectionStats(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "partition_stats")) {
        return queryPartitionStats(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "index_build_progress")) {
        return queryIndexBuildProgress(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "index_state")) {
        return queryIndexState(results, ctx);
    }

    return Status::OK;
}

Status MilvusCatalogHandler::getTableColumns(
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

Status MilvusCatalogHandler::listTables(const std::string& schema_name,
                                        std::vector<std::string>& table_names,
                                        ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status MilvusCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                         ErrorContext* /*ctx*/) {
    schema_names = {"milvus_meta"};
    return Status::OK;
}

void MilvusCatalogHandler::initializeTableNames() {
    table_names_ = {"collections", "partitions", "aliases", "indexes", "collection_stats",
                    "partition_stats", "index_build_progress", "index_state"};
}

const MilvusCatalogHandler::ColumnDefs* MilvusCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"collections",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"collection_id", DataType::VARCHAR, false},
             {"shards_num", DataType::INT32, false},
         }},
        {"partitions",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"partition_name", DataType::VARCHAR, false},
             {"partition_id", DataType::VARCHAR, false},
         }},
        {"aliases",
         {
             {"db_name", DataType::VARCHAR, false},
             {"alias_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
         }},
        {"indexes",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"index_name", DataType::VARCHAR, false},
             {"index_type", DataType::VARCHAR, false},
         }},
        {"collection_stats",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"row_count", DataType::INT64, false},
         }},
        {"partition_stats",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"partition_name", DataType::VARCHAR, false},
             {"row_count", DataType::INT64, false},
         }},
        {"index_build_progress",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"index_name", DataType::VARCHAR, false},
             {"indexed_rows", DataType::INT64, false},
             {"total_rows", DataType::INT64, false},
         }},
        {"index_state",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"index_name", DataType::VARCHAR, false},
             {"state", DataType::VARCHAR, false},
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

void MilvusCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void MilvusCatalogHandler::setColumnInfo(const ColumnDefs& def,
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

std::string MilvusCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

std::string MilvusCatalogHandler::indexTypeName(CatalogManager::IndexType type) {
    switch (type) {
        case CatalogManager::IndexType::HNSW:
            return "HNSW";
        case CatalogManager::IndexType::IVF:
            return "IVF_FLAT";
        case CatalogManager::IndexType::IVF_SQ8:
            return "IVF_SQ8";
        case CatalogManager::IndexType::IVF_PQ:
            return "IVF_PQ";
        default:
            return "AUTOINDEX";
    }
}

Status MilvusCatalogHandler::queryCollections(VirtualResultSet& results, ErrorContext* ctx) {
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
                {"db_name", TypedValue::makeVarchar(db_name)},
                {"collection_name", TypedValue::makeVarchar(table.table_name)},
                {"collection_id", TypedValue::makeVarchar(table.table_id.toString())},
                {"shards_num", TypedValue::makeInt32(1)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status MilvusCatalogHandler::queryPartitions(VirtualResultSet& results, ErrorContext* ctx) {
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
                {"db_name", TypedValue::makeVarchar(db_name)},
                {"collection_name", TypedValue::makeVarchar(table.table_name)},
                {"partition_name", TypedValue::makeVarchar("_default")},
                {"partition_id", TypedValue::makeVarchar(table.table_id.toString())},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status MilvusCatalogHandler::queryAliases(VirtualResultSet& results, ErrorContext* ctx) {
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
                {"db_name", TypedValue::makeVarchar(db_name)},
                {"alias_name", TypedValue::makeVarchar(table.table_name)},
                {"collection_name", TypedValue::makeVarchar(table.table_name)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status MilvusCatalogHandler::queryIndexes(VirtualResultSet& results, ErrorContext* ctx) {
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
            std::vector<CatalogManager::IndexInfo> indexes;
            if (catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx) != Status::OK) {
                continue;
            }
            for (const auto& index : indexes) {
                VirtualRow row;
                row.columns = {
                    {"db_name", TypedValue::makeVarchar(db_name)},
                    {"collection_name", TypedValue::makeVarchar(table.table_name)},
                    {"index_name", TypedValue::makeVarchar(index.index_name)},
                    {"index_type", TypedValue::makeVarchar(indexTypeName(index.index_type))},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status MilvusCatalogHandler::queryCollectionStats(VirtualResultSet& results, ErrorContext* ctx) {
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
                {"db_name", TypedValue::makeVarchar(db_name)},
                {"collection_name", TypedValue::makeVarchar(table.table_name)},
                {"row_count", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status MilvusCatalogHandler::queryPartitionStats(VirtualResultSet& results, ErrorContext* ctx) {
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
                {"db_name", TypedValue::makeVarchar(db_name)},
                {"collection_name", TypedValue::makeVarchar(table.table_name)},
                {"partition_name", TypedValue::makeVarchar("_default")},
                {"row_count", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status MilvusCatalogHandler::queryIndexBuildProgress(VirtualResultSet& results, ErrorContext* ctx) {
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
            std::vector<CatalogManager::IndexInfo> indexes;
            if (catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx) != Status::OK) {
                continue;
            }
            for (const auto& index : indexes) {
                const int64_t rows = static_cast<int64_t>(table.row_count);
                VirtualRow row;
                row.columns = {
                    {"db_name", TypedValue::makeVarchar(db_name)},
                    {"collection_name", TypedValue::makeVarchar(table.table_name)},
                    {"index_name", TypedValue::makeVarchar(index.index_name)},
                    {"indexed_rows", TypedValue::makeInt64(rows)},
                    {"total_rows", TypedValue::makeInt64(rows)},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status MilvusCatalogHandler::queryIndexState(VirtualResultSet& results, ErrorContext* ctx) {
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
            std::vector<CatalogManager::IndexInfo> indexes;
            if (catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx) != Status::OK) {
                continue;
            }
            for (const auto& index : indexes) {
                VirtualRow row;
                row.columns = {
                    {"db_name", TypedValue::makeVarchar(db_name)},
                    {"collection_name", TypedValue::makeVarchar(table.table_name)},
                    {"index_name", TypedValue::makeVarchar(index.index_name)},
                    {"state", TypedValue::makeVarchar("Finished")},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

} // namespace scratchbird::catalog
