/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/mongodb_catalog.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

MongoDBCatalogHandler::MongoDBCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType MongoDBCatalogHandler::getProtocolType() const {
    return ProtocolType::MONGODB;
}

bool MongoDBCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "mongo_meta");
}

bool MongoDBCatalogHandler::ownsTable(const std::string& schema_name,
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

Status MongoDBCatalogHandler::queryTable(const std::string& schema_name,
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

    if (equalsCaseInsensitive(table_name, "list_databases")) {
        return queryListDatabases(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "list_collections")) {
        return queryListCollections(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "list_indexes")) {
        return queryListIndexes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "system_views")) {
        return querySystemViews(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "system_js")) {
        return querySystemJs(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "system_profile")) {
        return querySystemProfile(results, ctx);
    }

    return Status::OK;
}

Status MongoDBCatalogHandler::getTableColumns(
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

Status MongoDBCatalogHandler::listTables(const std::string& schema_name,
                                         std::vector<std::string>& table_names,
                                         ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status MongoDBCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                          ErrorContext* /*ctx*/) {
    schema_names = {"mongo_meta"};
    return Status::OK;
}

void MongoDBCatalogHandler::initializeTableNames() {
    table_names_ = {"list_databases", "list_collections", "list_indexes",
                    "system_views", "system_js", "system_profile"};
}

const MongoDBCatalogHandler::ColumnDefs* MongoDBCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"list_databases",
         {
             {"db_name", DataType::VARCHAR, false},
             {"size_on_disk", DataType::INT64, false},
             {"empty", DataType::BOOLEAN, false},
         }},
        {"list_collections",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"collection_type", DataType::VARCHAR, false},
         }},
        {"list_indexes",
         {
             {"db_name", DataType::VARCHAR, false},
             {"collection_name", DataType::VARCHAR, false},
             {"index_name", DataType::VARCHAR, false},
             {"index_type", DataType::VARCHAR, false},
         }},
        {"system_views",
         {
             {"db_name", DataType::VARCHAR, false},
             {"view_name", DataType::VARCHAR, false},
             {"view_on", DataType::VARCHAR, true},
             {"pipeline_json", DataType::TEXT, false},
         }},
        {"system_js",
         {
             {"db_name", DataType::VARCHAR, false},
             {"function_name", DataType::VARCHAR, false},
             {"source_text", DataType::TEXT, false},
         }},
        {"system_profile",
         {
             {"op", DataType::VARCHAR, false},
             {"namespace", DataType::VARCHAR, false},
             {"millis", DataType::INT64, false},
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

void MongoDBCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void MongoDBCatalogHandler::setColumnInfo(const ColumnDefs& def,
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

std::string MongoDBCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

std::string MongoDBCatalogHandler::indexTypeName(CatalogManager::IndexType type) {
    switch (type) {
        case CatalogManager::IndexType::BTREE: return "btree";
        case CatalogManager::IndexType::HASH: return "hashed";
        case CatalogManager::IndexType::MONGODB_2D: return "2d";
        case CatalogManager::IndexType::MONGODB_2DSPHERE: return "2dsphere";
        case CatalogManager::IndexType::MONGODB_WILDCARD: return "wildcard";
        default: return "generic";
    }
}

Status MongoDBCatalogHandler::queryListDatabases(VirtualResultSet& results, ErrorContext* ctx) {
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
            {"db_name", TypedValue::makeVarchar(normalizeSchemaName(schema))},
            {"size_on_disk", TypedValue::makeInt64(0)},
            {"empty", TypedValue::makeBoolean(false)},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status MongoDBCatalogHandler::queryListCollections(VirtualResultSet& results, ErrorContext* ctx) {
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
                {"db_name", TypedValue::makeVarchar(db_name)},
                {"collection_name", TypedValue::makeVarchar(table.table_name)},
                {"collection_type", TypedValue::makeVarchar("collection")},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status MongoDBCatalogHandler::queryListIndexes(VirtualResultSet& results, ErrorContext* ctx) {
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

Status MongoDBCatalogHandler::querySystemViews(VirtualResultSet& results, ErrorContext* ctx) {
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
        const std::string db_name = normalizeSchemaName(schema);
        for (const auto& view : views) {
            VirtualRow row;
            row.columns = {
                {"db_name", TypedValue::makeVarchar(db_name)},
                {"view_name", TypedValue::makeVarchar(view.name)},
                {"view_on", TypedValue()},
                {"pipeline_json", TypedValue::makeText(view.definition)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status MongoDBCatalogHandler::querySystemJs(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::FunctionInfo> functions;
    if (catalog_manager_->listFunctions(functions, ctx) != Status::OK) {
        return Status::OK;
    }

    std::unordered_map<ID, std::string> schema_names;
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
        for (const auto& schema : schemas) {
            schema_names[schema.schema_id] = normalizeSchemaName(schema);
        }
    }

    for (const auto& fn : functions) {
        std::string db_name = "system";
        auto it = schema_names.find(fn.schema_id);
        if (it != schema_names.end()) {
            db_name = it->second;
        }
        VirtualRow row;
        row.columns = {
            {"db_name", TypedValue::makeVarchar(db_name)},
            {"function_name", TypedValue::makeVarchar(fn.name)},
            {"source_text", TypedValue::makeText(fn.source_text)},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status MongoDBCatalogHandler::querySystemProfile(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SessionInfo> sessions;
    if (catalog_manager_->listSessions(sessions, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& session : sessions) {
        VirtualRow row;
        row.columns = {
            {"op", TypedValue::makeVarchar("query")},
            {"namespace", TypedValue::makeVarchar(session.emulation_mode)},
            {"millis", TypedValue::makeInt64(0)},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

} // namespace scratchbird::catalog

