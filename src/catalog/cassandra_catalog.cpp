/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/cassandra_catalog.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <unordered_map>

namespace scratchbird::catalog {

CassandraCatalogHandler::CassandraCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType CassandraCatalogHandler::getProtocolType() const {
    return ProtocolType::CASSANDRA;
}

bool CassandraCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "system") ||
           equalsCaseInsensitive(schema_name, "system_schema");
}

bool CassandraCatalogHandler::ownsTable(const std::string& schema_name,
                                        const std::string& table_name) const {
    if (!ownsSchema(schema_name)) {
        return false;
    }
    const auto& names = tableNamesForSchema(schema_name);
    for (const auto& name : names) {
        if (equalsCaseInsensitive(name, table_name)) {
            return true;
        }
    }
    return false;
}

Status CassandraCatalogHandler::queryTable(const std::string& schema_name,
                                           const std::string& table_name,
                                           const std::string& /*where_clause*/,
                                           VirtualResultSet& results,
                                           ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    if (!ownsTable(schema_name, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }

    const ColumnDefs* def = getTableDefinition(schema_name, table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table definition not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }
    setResultColumns(*def, results);

    if (equalsCaseInsensitive(schema_name, "system")) {
        if (equalsCaseInsensitive(table_name, "local")) {
            return querySystemLocal(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "peers")) {
            return querySystemPeers(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "peers_v2")) {
            return querySystemPeersV2(results, ctx);
        }
    }

    if (equalsCaseInsensitive(table_name, "keyspaces")) {
        return querySystemSchemaKeyspaces(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "tables")) {
        return querySystemSchemaTables(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "columns")) {
        return querySystemSchemaColumns(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "indexes")) {
        return querySystemSchemaIndexes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "types")) {
        return querySystemSchemaTypes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "functions")) {
        return querySystemSchemaFunctions(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "aggregates")) {
        return querySystemSchemaAggregates(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "views")) {
        return querySystemSchemaViews(results, ctx);
    }

    return Status::OK;
}

Status CassandraCatalogHandler::getTableColumns(
    const std::string& schema_name,
    const std::string& table_name,
    std::vector<CatalogManager::ColumnInfo>& columns,
    ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    if (!ownsTable(schema_name, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }

    const ColumnDefs* def = getTableDefinition(schema_name, table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table definition not found: " + schema_name + "." + table_name).c_str());
        return Status::NOT_FOUND;
    }
    setColumnInfo(*def, columns);
    return Status::OK;
}

Status CassandraCatalogHandler::listTables(const std::string& schema_name,
                                           std::vector<std::string>& table_names,
                                           ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = tableNamesForSchema(schema_name);
    return Status::OK;
}

Status CassandraCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                            ErrorContext* /*ctx*/) {
    schema_names.clear();
    schema_names.push_back("system");
    schema_names.push_back("system_schema");
    return Status::OK;
}

void CassandraCatalogHandler::initializeTableNames() {
    system_table_names_ = {
        "local",
        "peers",
        "peers_v2",
    };
    system_schema_table_names_ = {
        "keyspaces",
        "tables",
        "columns",
        "indexes",
        "types",
        "functions",
        "aggregates",
        "views",
    };
}

const std::vector<std::string>& CassandraCatalogHandler::tableNamesForSchema(
    const std::string& schema_name) const {
    if (equalsCaseInsensitive(schema_name, "system")) {
        return system_table_names_;
    }
    return system_schema_table_names_;
}

const CassandraCatalogHandler::ColumnDefs* CassandraCatalogHandler::getTableDefinition(
    const std::string& schema_name, const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"system.local",
         {
             {"key", DataType::VARCHAR, false},
             {"bootstrapped", DataType::VARCHAR, true},
             {"cluster_name", DataType::VARCHAR, true},
             {"cql_version", DataType::VARCHAR, true},
             {"data_center", DataType::VARCHAR, true},
             {"host_id", DataType::VARCHAR, true},
             {"listen_address", DataType::VARCHAR, true},
             {"native_protocol_version", DataType::VARCHAR, true},
             {"partitioner", DataType::VARCHAR, true},
             {"rack", DataType::VARCHAR, true},
             {"release_version", DataType::VARCHAR, true},
             {"rpc_address", DataType::VARCHAR, true},
             {"schema_version", DataType::VARCHAR, true},
         }},
        {"system.peers",
         {
             {"peer", DataType::VARCHAR, false},
             {"data_center", DataType::VARCHAR, true},
             {"rack", DataType::VARCHAR, true},
             {"host_id", DataType::VARCHAR, true},
             {"release_version", DataType::VARCHAR, true},
             {"schema_version", DataType::VARCHAR, true},
             {"rpc_address", DataType::VARCHAR, true},
         }},
        {"system.peers_v2",
         {
             {"peer", DataType::VARCHAR, false},
             {"peer_port", DataType::INT32, false},
             {"data_center", DataType::VARCHAR, true},
             {"rack", DataType::VARCHAR, true},
             {"host_id", DataType::VARCHAR, true},
             {"release_version", DataType::VARCHAR, true},
             {"schema_version", DataType::VARCHAR, true},
             {"native_address", DataType::VARCHAR, true},
             {"native_port", DataType::INT32, true},
         }},
        {"system_schema.keyspaces",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"durable_writes", DataType::BOOLEAN, false},
             {"replication", DataType::TEXT, false},
         }},
        {"system_schema.tables",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"table_name", DataType::VARCHAR, false},
             {"id", DataType::VARCHAR, false},
             {"flags", DataType::TEXT, false},
         }},
        {"system_schema.columns",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"table_name", DataType::VARCHAR, false},
             {"column_name", DataType::VARCHAR, false},
             {"kind", DataType::VARCHAR, false},
             {"position", DataType::INT32, false},
             {"type", DataType::TEXT, false},
         }},
        {"system_schema.indexes",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"table_name", DataType::VARCHAR, false},
             {"index_name", DataType::VARCHAR, false},
             {"kind", DataType::VARCHAR, false},
             {"options", DataType::TEXT, false},
         }},
        {"system_schema.types",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"type_name", DataType::VARCHAR, false},
             {"field_names", DataType::TEXT, false},
             {"field_types", DataType::TEXT, false},
         }},
        {"system_schema.functions",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"function_name", DataType::VARCHAR, false},
             {"argument_types", DataType::TEXT, false},
             {"return_type", DataType::TEXT, false},
             {"language", DataType::VARCHAR, false},
             {"body", DataType::TEXT, true},
         }},
        {"system_schema.aggregates",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"aggregate_name", DataType::VARCHAR, false},
             {"argument_types", DataType::TEXT, false},
             {"state_func", DataType::VARCHAR, true},
             {"final_func", DataType::VARCHAR, true},
             {"state_type", DataType::TEXT, true},
             {"return_type", DataType::TEXT, true},
         }},
        {"system_schema.views",
         {
             {"keyspace_name", DataType::VARCHAR, false},
             {"view_name", DataType::VARCHAR, false},
             {"base_table_name", DataType::VARCHAR, false},
             {"where_clause", DataType::TEXT, true},
             {"include_all_columns", DataType::BOOLEAN, false},
         }},
    };

    std::string key = schema_name + "." + table_name;
    std::transform(key.begin(), key.end(), key.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    auto it = kDefs.find(key);
    if (it == kDefs.end()) {
        return nullptr;
    }
    return &it->second;
}

void CassandraCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void CassandraCatalogHandler::setColumnInfo(const ColumnDefs& def,
                                            std::vector<CatalogManager::ColumnInfo>& columns) const {
    columns.clear();
    uint16_t ordinal = 0;
    for (const auto& col : def) {
        CatalogManager::ColumnInfo out{};
        out.column_name = col.name;
        out.ordinal = ++ordinal;
        out.data_type = static_cast<uint16_t>(col.type);
        out.nullable = col.nullable;
        columns.push_back(std::move(out));
    }
}

std::string CassandraCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

std::string CassandraCatalogHandler::toCqlType(const CatalogManager::ColumnInfo& col) {
    const DataType type = static_cast<DataType>(col.data_type);
    if (col.is_array) {
        return "list<text>";
    }
    switch (type) {
        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
            return "int";
        case DataType::INT64:
            return "bigint";
        case DataType::INT128:
        case DataType::UINT128:
            return "decimal";
        case DataType::UINT8:
        case DataType::UINT16:
        case DataType::UINT32:
        case DataType::UINT64:
            return "bigint";
        case DataType::FLOAT32:
            return "float";
        case DataType::FLOAT64:
            return "double";
        case DataType::DECIMAL:
            return "decimal";
        case DataType::BOOLEAN:
            return "boolean";
        case DataType::TEXT:
        case DataType::CHAR:
        case DataType::VARCHAR:
            return "text";
        case DataType::UUID:
            return "uuid";
        case DataType::DATE:
            return "date";
        case DataType::TIME:
            return "time";
        case DataType::TIMESTAMP:
        case DataType::TIMESTAMP_WITH_ZONE:
            return "timestamp";
        case DataType::BLOB:
        case DataType::BINARY:
        case DataType::VARBINARY:
        case DataType::BYTEA:
            return "blob";
        case DataType::JSON:
        case DataType::JSONB:
        case DataType::BSON:
            return "text";
        default:
            break;
    }
    return "text";
}

Status CassandraCatalogHandler::querySystemLocal(VirtualResultSet& results, ErrorContext* ctx) {
    std::string cluster_name = "scratchbird_cluster";
    std::string release_version = "5.0-scratchbird";

    if (catalog_manager_) {
        CatalogManager::EmulationProfileCatalogInfo profile{};
        if (catalog_manager_->getEmulationProfileCatalogEntryByEngine(
                CatalogManager::EmulationEngine::CASSANDRA, profile, ctx) == Status::OK &&
            !profile.requested_engine_version.empty()) {
            release_version = profile.requested_engine_version;
        }

        std::vector<CatalogManager::ClusterCatalogInfo> clusters;
        if (catalog_manager_->listClusterCatalogEntries(clusters, ctx) == Status::OK &&
            !clusters.empty() && !clusters.front().cluster_name.empty()) {
            cluster_name = clusters.front().cluster_name;
        }
    }

    VirtualRow row;
    row.columns = {
        {"key", TypedValue::makeVarchar("local")},
        {"bootstrapped", TypedValue::makeVarchar("COMPLETED")},
        {"cluster_name", TypedValue::makeVarchar(cluster_name)},
        {"cql_version", TypedValue::makeVarchar("3.4.5")},
        {"data_center", TypedValue::makeVarchar("dc1")},
        {"host_id", TypedValue::makeVarchar("00000000-0000-7000-8000-000000000001")},
        {"listen_address", TypedValue::makeVarchar("127.0.0.1")},
        {"native_protocol_version", TypedValue::makeVarchar("v5")},
        {"partitioner", TypedValue::makeVarchar("org.apache.cassandra.dht.Murmur3Partitioner")},
        {"rack", TypedValue::makeVarchar("rack1")},
        {"release_version", TypedValue::makeVarchar(release_version)},
        {"rpc_address", TypedValue::makeVarchar("127.0.0.1")},
        {"schema_version", TypedValue::makeVarchar("00000000-0000-7000-8000-000000000002")},
    };
    results.rows.push_back(std::move(row));
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemPeers(VirtualResultSet& /*results*/, ErrorContext* /*ctx*/) {
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemPeersV2(VirtualResultSet& /*results*/, ErrorContext* /*ctx*/) {
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaKeyspaces(VirtualResultSet& results, ErrorContext* ctx) {
    std::set<std::string> names = {"system", "system_schema"};

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_ && catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
        for (const auto& schema : schemas) {
            names.insert(normalizeSchemaName(schema));
        }
    }

    for (const auto& name : names) {
        VirtualRow row;
        row.columns = {
            {"keyspace_name", TypedValue::makeVarchar(name)},
            {"durable_writes", TypedValue::makeBoolean(true)},
            {"replication", TypedValue::makeText("{'class':'SimpleStrategy','replication_factor':'1'}")},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaTables(VirtualResultSet& results, ErrorContext* ctx) {
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
        const std::string keyspace_name = normalizeSchemaName(schema);
        for (const auto& table : tables) {
            VirtualRow row;
            row.columns = {
                {"keyspace_name", TypedValue::makeVarchar(keyspace_name)},
                {"table_name", TypedValue::makeVarchar(table.table_name)},
                {"id", TypedValue::makeVarchar(table.table_id.toString())},
                {"flags", TypedValue::makeText("compound")},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaColumns(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK) {
        return status;
    }

    for (const auto& schema : schemas) {
        const std::string keyspace_name = normalizeSchemaName(schema);
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<CatalogManager::ColumnInfo> cols;
            if (catalog_manager_->getColumns(table.table_id, cols, ctx) != Status::OK) {
                continue;
            }
            for (const auto& col : cols) {
                const bool primary = col.is_primary_key;
                const std::string kind = primary
                    ? (col.ordinal <= 1 ? "partition_key" : "clustering")
                    : "regular";
                const int32_t position = primary ? static_cast<int32_t>(col.ordinal - 1) : -1;
                VirtualRow row;
                row.columns = {
                    {"keyspace_name", TypedValue::makeVarchar(keyspace_name)},
                    {"table_name", TypedValue::makeVarchar(table.table_name)},
                    {"column_name", TypedValue::makeVarchar(col.column_name)},
                    {"kind", TypedValue::makeVarchar(kind)},
                    {"position", TypedValue::makeInt32(position)},
                    {"type", TypedValue::makeText(toCqlType(col))},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaIndexes(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK) {
        return status;
    }

    for (const auto& schema : schemas) {
        const std::string keyspace_name = normalizeSchemaName(schema);
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
                    {"keyspace_name", TypedValue::makeVarchar(keyspace_name)},
                    {"table_name", TypedValue::makeVarchar(table.table_name)},
                    {"index_name", TypedValue::makeVarchar(index.index_name)},
                    {"kind", TypedValue::makeVarchar("CUSTOM")},
                    {"options", TypedValue::makeText("{}")},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaTypes(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<CatalogManager::TypeCatalogInfo> types;
        if (catalog_manager_->listTypeCatalogEntries(schema.schema_id, types, ctx) != Status::OK) {
            continue;
        }
        const std::string keyspace_name = normalizeSchemaName(schema);
        for (const auto& type : types) {
            VirtualRow row;
            row.columns = {
                {"keyspace_name", TypedValue::makeVarchar(keyspace_name)},
                {"type_name", TypedValue::makeVarchar(type.type_name)},
                {"field_names", TypedValue::makeText("[]")},
                {"field_types", TypedValue::makeText("[]")},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaFunctions(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::FunctionInfo> functions;
    if (catalog_manager_->listFunctions(functions, ctx) != Status::OK) {
        return Status::OK;
    }

    for (const auto& fn : functions) {
        CatalogManager::SchemaInfo schema{};
        std::string keyspace_name = "unknown";
        if (catalog_manager_->getSchema(fn.schema_id, schema, ctx) == Status::OK) {
            keyspace_name = normalizeSchemaName(schema);
        }
        VirtualRow row;
        row.columns = {
            {"keyspace_name", TypedValue::makeVarchar(keyspace_name)},
            {"function_name", TypedValue::makeVarchar(fn.name)},
            {"argument_types", TypedValue::makeText("[]")},
            {"return_type", TypedValue::makeText("text")},
            {"language", TypedValue::makeVarchar("sblr")},
            {"body", fn.source_text.empty() ? TypedValue() : TypedValue::makeText(fn.source_text)},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaAggregates(VirtualResultSet& /*results*/,
                                                            ErrorContext* /*ctx*/) {
    return Status::OK;
}

Status CassandraCatalogHandler::querySystemSchemaViews(VirtualResultSet& results, ErrorContext* ctx) {
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
        const std::string keyspace_name = normalizeSchemaName(schema);
        for (const auto& view : views) {
            std::string base_table_name;
            if (!view.base_table_ids.empty()) {
                CatalogManager::TableInfo table{};
                if (catalog_manager_->getTable(view.base_table_ids.front(), table, ctx) == Status::OK) {
                    base_table_name = table.table_name;
                }
            }
            VirtualRow row;
            row.columns = {
                {"keyspace_name", TypedValue::makeVarchar(keyspace_name)},
                {"view_name", TypedValue::makeVarchar(view.name)},
                {"base_table_name", base_table_name.empty() ? TypedValue() : TypedValue::makeVarchar(base_table_name)},
                {"where_clause",
                 view.definition.empty() ? TypedValue() : TypedValue::makeText(view.definition)},
                {"include_all_columns", TypedValue::makeBoolean(true)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

} // namespace scratchbird::catalog
