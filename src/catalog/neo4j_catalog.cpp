/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/neo4j_catalog.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

Neo4jCatalogHandler::Neo4jCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType Neo4jCatalogHandler::getProtocolType() const {
    return ProtocolType::NEO4J;
}

bool Neo4jCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "neo4j_meta");
}

bool Neo4jCatalogHandler::ownsTable(const std::string& schema_name,
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

Status Neo4jCatalogHandler::queryTable(const std::string& schema_name,
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
    if (equalsCaseInsensitive(table_name, "indexes")) {
        return queryIndexes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "constraints")) {
        return queryConstraints(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "functions")) {
        return queryFunctions(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "procedures")) {
        return queryProcedures(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "node_type_properties")) {
        return queryNodeTypeProperties(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "relationship_type_properties")) {
        return queryRelationshipTypeProperties(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "stats")) {
        return queryStats(results, ctx);
    }

    return Status::OK;
}

Status Neo4jCatalogHandler::getTableColumns(
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

Status Neo4jCatalogHandler::listTables(const std::string& schema_name,
                                       std::vector<std::string>& table_names,
                                       ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status Neo4jCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                        ErrorContext* /*ctx*/) {
    schema_names = {"neo4j_meta"};
    return Status::OK;
}

void Neo4jCatalogHandler::initializeTableNames() {
    table_names_ = {"databases", "indexes", "constraints", "functions", "procedures",
                    "node_type_properties", "relationship_type_properties", "stats"};
}

const Neo4jCatalogHandler::ColumnDefs* Neo4jCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"databases",
         {
             {"name", DataType::VARCHAR, false},
             {"status", DataType::VARCHAR, false},
             {"default", DataType::BOOLEAN, false},
         }},
        {"indexes",
         {
             {"name", DataType::VARCHAR, false},
             {"entity_type", DataType::VARCHAR, false},
             {"labels_or_types", DataType::VARCHAR, false},
             {"properties", DataType::VARCHAR, false},
             {"type", DataType::VARCHAR, false},
             {"state", DataType::VARCHAR, false},
         }},
        {"constraints",
         {
             {"name", DataType::VARCHAR, false},
             {"type", DataType::VARCHAR, false},
             {"entity_type", DataType::VARCHAR, false},
             {"labels_or_types", DataType::VARCHAR, false},
             {"properties", DataType::VARCHAR, false},
         }},
        {"functions",
         {
             {"name", DataType::VARCHAR, false},
             {"signature", DataType::VARCHAR, false},
             {"is_built_in", DataType::BOOLEAN, false},
         }},
        {"procedures",
         {
             {"name", DataType::VARCHAR, false},
             {"signature", DataType::VARCHAR, false},
             {"mode", DataType::VARCHAR, false},
         }},
        {"node_type_properties",
         {
             {"node_type", DataType::VARCHAR, false},
             {"property_name", DataType::VARCHAR, false},
             {"property_type", DataType::VARCHAR, false},
         }},
        {"relationship_type_properties",
         {
             {"relationship_type", DataType::VARCHAR, false},
             {"property_name", DataType::VARCHAR, false},
             {"property_type", DataType::VARCHAR, false},
         }},
        {"stats",
         {
             {"metric_name", DataType::VARCHAR, false},
             {"metric_value", DataType::INT64, false},
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

void Neo4jCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void Neo4jCatalogHandler::setColumnInfo(const ColumnDefs& def,
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

std::string Neo4jCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

std::string Neo4jCatalogHandler::indexTypeName(CatalogManager::IndexType type) {
    switch (type) {
        case CatalogManager::IndexType::NEO4J_LOOKUP: return "LOOKUP";
        case CatalogManager::IndexType::NEO4J_TEXT: return "TEXT";
        case CatalogManager::IndexType::NEO4J_RANGE: return "RANGE";
        case CatalogManager::IndexType::NEO4J_POINT: return "POINT";
        case CatalogManager::IndexType::NEO4J_VECTOR: return "VECTOR";
        case CatalogManager::IndexType::HNSW:
        case CatalogManager::IndexType::IVF:
            return "VECTOR";
        default: return "RANGE";
    }
}

std::string Neo4jCatalogHandler::constraintTypeName(CatalogManager::ConstraintType type) {
    switch (type) {
        case CatalogManager::ConstraintType::PRIMARY_KEY: return "NODE_KEY";
        case CatalogManager::ConstraintType::UNIQUE: return "UNIQUENESS";
        case CatalogManager::ConstraintType::NOT_NULL: return "NODE_PROPERTY_EXISTENCE";
        case CatalogManager::ConstraintType::FOREIGN_KEY: return "RELATIONSHIP_PROPERTY_EXISTENCE";
        case CatalogManager::ConstraintType::CHECK: return "CHECK";
        case CatalogManager::ConstraintType::EXCLUSION: return "EXCLUSION";
        default: return "UNKNOWN";
    }
}

Status Neo4jCatalogHandler::queryDatabases(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        VirtualRow row;
        row.columns = {
            {"name", TypedValue::makeVarchar(normalizeSchemaName(schema))},
            {"status", TypedValue::makeVarchar("online")},
            {"default", TypedValue::makeBoolean(equalsCaseInsensitive(schema.schema_name, "public"))},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status Neo4jCatalogHandler::queryIndexes(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        const std::string label = normalizeSchemaName(schema);
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
                    {"name", TypedValue::makeVarchar(index.index_name)},
                    {"entity_type", TypedValue::makeVarchar("NODE")},
                    {"labels_or_types", TypedValue::makeVarchar(label)},
                    {"properties", TypedValue::makeVarchar(table.table_name)},
                    {"type", TypedValue::makeVarchar(indexTypeName(index.index_type))},
                    {"state", TypedValue::makeVarchar("ONLINE")},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status Neo4jCatalogHandler::queryConstraints(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        const std::string label = normalizeSchemaName(schema);
        std::vector<CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<CatalogManager::ConstraintInfo> constraints;
            if (catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx) != Status::OK) {
                continue;
            }
            for (const auto& constraint : constraints) {
                VirtualRow row;
                const std::string property = constraint.column_names.empty() ? "" : constraint.column_names.front();
                row.columns = {
                    {"name", TypedValue::makeVarchar(constraint.constraint_name)},
                    {"type", TypedValue::makeVarchar(constraintTypeName(constraint.constraint_type))},
                    {"entity_type", TypedValue::makeVarchar("NODE")},
                    {"labels_or_types", TypedValue::makeVarchar(label)},
                    {"properties", TypedValue::makeVarchar(property)},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status Neo4jCatalogHandler::queryFunctions(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::FunctionInfo> functions;
    if (catalog_manager_->listFunctions(functions, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& fn : functions) {
        VirtualRow row;
        row.columns = {
            {"name", TypedValue::makeVarchar(fn.name)},
            {"signature", TypedValue::makeVarchar(fn.name + "(...) :: value")},
            {"is_built_in", TypedValue::makeBoolean(false)},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status Neo4jCatalogHandler::queryProcedures(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::ProcedureInfo> procedures;
    if (catalog_manager_->listProcedures(procedures, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& proc : procedures) {
        VirtualRow row;
        row.columns = {
            {"name", TypedValue::makeVarchar(proc.name)},
            {"signature", TypedValue::makeVarchar(proc.name + "(...)")},
            {"mode", TypedValue::makeVarchar("READ_WRITE")},
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status Neo4jCatalogHandler::queryNodeTypeProperties(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) != Status::OK) {
        return Status::OK;
    }
    for (const auto& schema : schemas) {
        const std::string node_type = normalizeSchemaName(schema);
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
                    {"node_type", TypedValue::makeVarchar(node_type + "." + table.table_name)},
                    {"property_name", TypedValue::makeVarchar(col.column_name)},
                    {"property_type", TypedValue::makeVarchar(std::to_string(col.data_type))},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status Neo4jCatalogHandler::queryRelationshipTypeProperties(VirtualResultSet& results, ErrorContext* ctx) {
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
            for (const auto& col : columns) {
                VirtualRow row;
                row.columns = {
                    {"relationship_type", TypedValue::makeVarchar(table.table_name)},
                    {"property_name", TypedValue::makeVarchar(col.column_name)},
                    {"property_type", TypedValue::makeVarchar(std::to_string(col.data_type))},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status Neo4jCatalogHandler::queryStats(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    int64_t label_count = 0;
    int64_t relationship_count = 0;

    std::vector<CatalogManager::SchemaInfo> schemas;
    if (catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
        label_count = static_cast<int64_t>(schemas.size());
        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
                continue;
            }
            relationship_count += static_cast<int64_t>(tables.size());
        }
    }

    VirtualRow labels_row;
    labels_row.columns = {
        {"metric_name", TypedValue::makeVarchar("label_count")},
        {"metric_value", TypedValue::makeInt64(label_count)},
    };
    results.rows.push_back(std::move(labels_row));

    VirtualRow rel_row;
    rel_row.columns = {
        {"metric_name", TypedValue::makeVarchar("relationship_type_count")},
        {"metric_value", TypedValue::makeInt64(relationship_count)},
    };
    results.rows.push_back(std::move(rel_row));
    return Status::OK;
}

} // namespace scratchbird::catalog
