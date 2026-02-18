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
 * MongoDB catalog emulation
 *
 * Provides deterministic virtual overlays in `mongo_meta.*`:
 * - list_databases
 * - list_collections
 * - list_indexes
 * - system_views
 * - system_js
 * - system_profile
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"

#include <string>
#include <vector>

namespace scratchbird::catalog {

class MongoDBCatalogHandler : public VirtualCatalogHandler {
public:
    explicit MongoDBCatalogHandler(core::CatalogManager* catalog);

    ProtocolType getProtocolType() const override;
    bool ownsSchema(const std::string& schema_name) const override;
    bool ownsTable(const std::string& schema_name, const std::string& table_name) const override;

    core::Status queryTable(const std::string& schema_name,
                            const std::string& table_name,
                            const std::string& where_clause,
                            VirtualResultSet& results,
                            core::ErrorContext* ctx = nullptr) override;

    core::Status getTableColumns(const std::string& schema_name,
                                 const std::string& table_name,
                                 std::vector<core::CatalogManager::ColumnInfo>& columns,
                                 core::ErrorContext* ctx = nullptr) override;

    core::Status listTables(const std::string& schema_name,
                            std::vector<std::string>& table_names,
                            core::ErrorContext* ctx = nullptr) override;

    core::Status listSchemas(std::vector<std::string>& schema_names,
                             core::ErrorContext* ctx = nullptr) override;

private:
    struct ColumnDef {
        const char* name;
        core::DataType type;
        bool nullable;
    };
    using ColumnDefs = std::vector<ColumnDef>;

    std::vector<std::string> table_names_;

    void initializeTableNames();
    const ColumnDefs* getTableDefinition(const std::string& table_name) const;
    void setResultColumns(const ColumnDefs& def, VirtualResultSet& results) const;
    void setColumnInfo(const ColumnDefs& def,
                       std::vector<core::CatalogManager::ColumnInfo>& columns) const;
    static std::string normalizeSchemaName(const core::CatalogManager::SchemaInfo& schema);
    static std::string indexTypeName(core::CatalogManager::IndexType type);

    core::Status queryListDatabases(VirtualResultSet& results, core::ErrorContext* ctx);
    core::Status queryListCollections(VirtualResultSet& results, core::ErrorContext* ctx);
    core::Status queryListIndexes(VirtualResultSet& results, core::ErrorContext* ctx);
    core::Status querySystemViews(VirtualResultSet& results, core::ErrorContext* ctx);
    core::Status querySystemJs(VirtualResultSet& results, core::ErrorContext* ctx);
    core::Status querySystemProfile(VirtualResultSet& results, core::ErrorContext* ctx);
};

} // namespace scratchbird::catalog

