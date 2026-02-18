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
 * Cassandra system catalog emulation
 *
 * Provides virtual overlays for:
 * - system.local
 * - system.peers
 * - system.peers_v2
 * - system_schema.keyspaces
 * - system_schema.tables
 * - system_schema.columns
 * - system_schema.indexes
 * - system_schema.types
 * - system_schema.functions
 * - system_schema.aggregates
 * - system_schema.views
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/typed_value.h"

#include <string>
#include <vector>

namespace scratchbird::catalog {

using namespace scratchbird::core;

class CassandraCatalogHandler : public VirtualCatalogHandler {
public:
    explicit CassandraCatalogHandler(CatalogManager* catalog);

    ProtocolType getProtocolType() const override;
    bool ownsSchema(const std::string& schema_name) const override;
    bool ownsTable(const std::string& schema_name, const std::string& table_name) const override;

    Status queryTable(const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& where_clause,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) override;

    Status getTableColumns(const std::string& schema_name,
                           const std::string& table_name,
                           std::vector<CatalogManager::ColumnInfo>& columns,
                           ErrorContext* ctx = nullptr) override;

    Status listTables(const std::string& schema_name,
                      std::vector<std::string>& table_names,
                      ErrorContext* ctx = nullptr) override;

    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* ctx = nullptr) override;

private:
    struct ColumnDef {
        const char* name;
        DataType type;
        bool nullable;
    };
    using ColumnDefs = std::vector<ColumnDef>;

    std::vector<std::string> system_table_names_;
    std::vector<std::string> system_schema_table_names_;

    void initializeTableNames();
    const std::vector<std::string>& tableNamesForSchema(const std::string& schema_name) const;
    const ColumnDefs* getTableDefinition(const std::string& schema_name,
                                         const std::string& table_name) const;
    void setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const;
    void setColumnInfo(const ColumnDefs& def, std::vector<CatalogManager::ColumnInfo>& columns) const;
    static std::string normalizeSchemaName(const CatalogManager::SchemaInfo& schema);
    static std::string toCqlType(const CatalogManager::ColumnInfo& col);

    Status querySystemLocal(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemPeers(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemPeersV2(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaKeyspaces(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaTables(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaColumns(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaIndexes(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaTypes(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaFunctions(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaAggregates(VirtualResultSet& results, ErrorContext* ctx);
    Status querySystemSchemaViews(VirtualResultSet& results, ErrorContext* ctx);
};

} // namespace scratchbird::catalog

