/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */

#include "scratchbird/catalog/opensearch_catalog.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_map>

namespace scratchbird::catalog {

using namespace scratchbird::core;

namespace {

std::string jsonEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    return out;
}

std::string buildAnalyzerConfigJson(const CatalogManager::TsConfigCatalogInfo& cfg,
                                    const std::string& parser_name,
                                    const std::vector<CatalogManager::TsConfigMapCatalogInfo>& maps) {
    std::ostringstream os;
    os << "{\"source\":\"canonical_ts_config\""
       << ",\"config_id\":\"" << cfg.config_id.toString() << "\""
       << ",\"parser\":\"" << jsonEscape(parser_name) << "\"";
    if (cfg.has_default_dictionary_id) {
        os << ",\"default_dictionary_id\":\"" << cfg.default_dictionary_id.toString() << "\"";
    }
    os << ",\"token_maps\":[";
    for (size_t i = 0; i < maps.size(); ++i) {
        if (i != 0) {
            os << ",";
        }
        os << "{\"token_type\":\"" << jsonEscape(maps[i].token_type) << "\""
           << ",\"dictionary_count\":" << maps[i].dictionary_ids.size()
           << ",\"is_override\":" << (maps[i].is_override ? "true" : "false") << "}";
    }
    os << "]}";
    return os.str();
}

} // namespace

OpenSearchCatalogHandler::OpenSearchCatalogHandler(CatalogManager* catalog) {
    catalog_manager_ = catalog;
    initializeTableNames();
}

ProtocolType OpenSearchCatalogHandler::getProtocolType() const {
    return ProtocolType::OPENSEARCH;
}

bool OpenSearchCatalogHandler::ownsSchema(const std::string& schema_name) const {
    return equalsCaseInsensitive(schema_name, "opensearch_meta");
}

bool OpenSearchCatalogHandler::ownsTable(const std::string& schema_name,
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

Status OpenSearchCatalogHandler::queryTable(const std::string& schema_name,
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

    if (equalsCaseInsensitive(table_name, "index_metadata")) {
        return queryIndexMetadata(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "mapping_fields")) {
        return queryMappingFields(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "analyzer_settings")) {
        return queryAnalyzerSettings(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "knn_index_metadata")) {
        return queryKnnIndexMetadata(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "aliases")) {
        return queryAliases(results, ctx);
    }

    return Status::OK;
}

Status OpenSearchCatalogHandler::getTableColumns(
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

Status OpenSearchCatalogHandler::listTables(const std::string& schema_name,
                                            std::vector<std::string>& table_names,
                                            ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }
    table_names = table_names_;
    return Status::OK;
}

Status OpenSearchCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                             ErrorContext* /*ctx*/) {
    schema_names = {"opensearch_meta"};
    return Status::OK;
}

void OpenSearchCatalogHandler::initializeTableNames() {
    table_names_ = {"index_metadata", "mapping_fields", "analyzer_settings",
                    "knn_index_metadata", "aliases"};
}

const OpenSearchCatalogHandler::ColumnDefs* OpenSearchCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const std::unordered_map<std::string, ColumnDefs> kDefs = {
        {"index_metadata",
         {
             {"index_name", DataType::VARCHAR, false},
             {"index_uuid", DataType::VARCHAR, false},
             {"docs_count", DataType::INT64, false},
             {"health", DataType::VARCHAR, false},
         }},
        {"mapping_fields",
         {
             {"index_name", DataType::VARCHAR, false},
             {"field_name", DataType::VARCHAR, false},
             {"field_type", DataType::VARCHAR, false},
             {"searchable", DataType::BOOLEAN, false},
         }},
        {"analyzer_settings",
         {
             {"index_name", DataType::VARCHAR, false},
             {"analyzer_name", DataType::VARCHAR, false},
             {"config_json", DataType::TEXT, false},
         }},
        {"knn_index_metadata",
         {
             {"index_name", DataType::VARCHAR, false},
             {"field_name", DataType::VARCHAR, false},
             {"engine", DataType::VARCHAR, false},
             {"method", DataType::VARCHAR, false},
         }},
        {"aliases",
         {
             {"alias_name", DataType::VARCHAR, false},
             {"index_name", DataType::VARCHAR, false},
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

void OpenSearchCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const {
    results.column_names.clear();
    results.column_types.clear();
    results.rows.clear();
    for (const auto& col : def) {
        results.column_names.emplace_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void OpenSearchCatalogHandler::setColumnInfo(const ColumnDefs& def,
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

std::string OpenSearchCatalogHandler::normalizeSchemaName(const CatalogManager::SchemaInfo& schema) {
    if (!schema.schema_name.empty()) {
        return schema.schema_name;
    }
    if (!schema.full_path.empty()) {
        return schema.full_path;
    }
    return "unknown";
}

std::string OpenSearchCatalogHandler::indexTypeName(CatalogManager::IndexType type) {
    switch (type) {
        case CatalogManager::IndexType::BTREE: return "btree";
        case CatalogManager::IndexType::GIN: return "inverted";
        case CatalogManager::IndexType::HNSW: return "hnsw";
        case CatalogManager::IndexType::IVF: return "ivf";
        case CatalogManager::IndexType::IVF_PQ: return "ivf_pq";
        case CatalogManager::IndexType::IVF_SQ8:
        case CatalogManager::IndexType::IVF_SQ8_HYBRID: return "ivf_sq8";
        default: return "generic";
    }
}

bool OpenSearchCatalogHandler::isVectorIndex(CatalogManager::IndexType type) {
    switch (type) {
        case CatalogManager::IndexType::HNSW:
        case CatalogManager::IndexType::IVF:
        case CatalogManager::IndexType::IVF_FLAT:
        case CatalogManager::IndexType::IVF_PQ:
        case CatalogManager::IndexType::IVF_SQ8:
        case CatalogManager::IndexType::IVF_SQ8_HYBRID:
        case CatalogManager::IndexType::NSG:
        case CatalogManager::IndexType::DISKANN:
            return true;
        default:
            return false;
    }
}

Status OpenSearchCatalogHandler::queryIndexMetadata(VirtualResultSet& results, ErrorContext* ctx) {
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
            VirtualRow row;
            row.columns = {
                {"index_name", TypedValue::makeVarchar(schema_name + "." + table.table_name)},
                {"index_uuid", TypedValue::makeVarchar(table.table_id.toString())},
                {"docs_count", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
                {"health", TypedValue::makeVarchar("green")},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status OpenSearchCatalogHandler::queryMappingFields(VirtualResultSet& results, ErrorContext* ctx) {
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
                    {"index_name", TypedValue::makeVarchar(schema_name + "." + table.table_name)},
                    {"field_name", TypedValue::makeVarchar(col.column_name)},
                    {"field_type", TypedValue::makeVarchar(std::to_string(col.data_type))},
                    {"searchable", TypedValue::makeBoolean(true)},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status OpenSearchCatalogHandler::queryAnalyzerSettings(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<CatalogManager::TsConfigCatalogInfo> configs;
    if (catalog_manager_->listTsConfigCatalogEntries(ID{}, configs, ctx) == Status::OK && !configs.empty()) {
        for (const auto& cfg : configs) {
            if (!cfg.is_valid) {
                continue;
            }
            CatalogManager::TsParserCatalogInfo parser{};
            std::string parser_name = "unknown";
            if (catalog_manager_->getTsParserCatalogEntry(cfg.parser_id, parser, ctx) == Status::OK &&
                !parser.parser_name.empty()) {
                parser_name = parser.parser_name;
            }

            std::vector<CatalogManager::TsConfigMapCatalogInfo> maps;
            if (catalog_manager_->listTsConfigMapCatalogEntries(cfg.config_id, maps, ctx) != Status::OK) {
                maps.clear();
            }

            VirtualRow row;
            row.columns = {
                {"index_name", TypedValue::makeVarchar(cfg.config_name)},
                {"analyzer_name", TypedValue::makeVarchar(cfg.config_name)},
                {"config_json", TypedValue::makeText(buildAnalyzerConfigJson(cfg, parser_name, maps))},
            };
            results.rows.push_back(std::move(row));
        }
        return Status::OK;
    }

    // Compatibility fallback while canonical text-search catalog is empty.
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
            VirtualRow row;
            row.columns = {
                {"index_name", TypedValue::makeVarchar(schema_name + "." + table.table_name)},
                {"analyzer_name", TypedValue::makeVarchar("standard")},
                {"config_json", TypedValue::makeText("{\"source\":\"fallback\",\"type\":\"standard\"}")},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

Status OpenSearchCatalogHandler::queryKnnIndexMetadata(VirtualResultSet& results, ErrorContext* ctx) {
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
            std::vector<CatalogManager::IndexInfo> indexes;
            if (catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx) != Status::OK) {
                continue;
            }
            for (const auto& index : indexes) {
                if (!isVectorIndex(index.index_type)) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"index_name", TypedValue::makeVarchar(schema_name + "." + table.table_name)},
                    {"field_name", TypedValue::makeVarchar(index.index_name)},
                    {"engine", TypedValue::makeVarchar("faiss")},
                    {"method", TypedValue::makeVarchar(indexTypeName(index.index_type))},
                };
                results.rows.push_back(std::move(row));
            }
        }
    }
    return Status::OK;
}

Status OpenSearchCatalogHandler::queryAliases(VirtualResultSet& results, ErrorContext* ctx) {
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
            const std::string index_name = schema_name + "." + table.table_name;
            VirtualRow row;
            row.columns = {
                {"alias_name", TypedValue::makeVarchar(table.table_name)},
                {"index_name", TypedValue::makeVarchar(index_name)},
            };
            results.rows.push_back(std::move(row));
        }
    }
    return Status::OK;
}

} // namespace scratchbird::catalog
