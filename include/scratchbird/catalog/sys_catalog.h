#pragma once

/**
 * ScratchBird sys.* Catalog Handler
 *
 * Exposes system monitoring and scheduler/job data as queryable sys.* tables.
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/typed_value.h"
#include <string>
#include <vector>

namespace scratchbird::catalog {

class SysCatalogHandler : public VirtualCatalogHandler {
public:
    explicit SysCatalogHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    ProtocolType getProtocolType() const override {
        return ProtocolType::SCRATCHBIRD;
    }

    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "sys");
    }

    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override {
        if (!ownsSchema(schema_name)) {
            return false;
        }
        for (const auto& name : table_names_) {
            if (equalsCaseInsensitive(table_name, name)) {
                return true;
            }
        }
        return false;
    }

    Status queryTable(const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& /* where_clause */,
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

    std::vector<std::string> table_names_;

    void initializeTableNames();
    const ColumnDefs* getTableDefinition(const std::string& table_name) const;
    static void setResultColumns(const ColumnDefs& def, VirtualResultSet& results);
    static void setColumnInfo(const ColumnDefs& def, std::vector<CatalogManager::ColumnInfo>& columns);

    Status querySessions(VirtualResultSet& results, ErrorContext* ctx);
    Status queryTransactions(VirtualResultSet& results, ErrorContext* ctx);
    Status queryLocks(VirtualResultSet& results, ErrorContext* ctx);
    Status queryStatements(VirtualResultSet& results, ErrorContext* ctx);
    Status queryJobs(VirtualResultSet& results, ErrorContext* ctx);
    Status queryJobRuns(VirtualResultSet& results, ErrorContext* ctx);
    Status queryJobDependencies(VirtualResultSet& results, ErrorContext* ctx);
    Status queryPerformance(VirtualResultSet& results, ErrorContext* ctx);
    Status queryContextVariables(VirtualResultSet& results, ErrorContext* ctx);
    Status queryIoStats(VirtualResultSet& results, ErrorContext* ctx);
    Status queryCacheStats(VirtualResultSet& results, ErrorContext* ctx);
    Status queryBufferPoolStats(VirtualResultSet& results, ErrorContext* ctx);
    Status queryStatementCache(VirtualResultSet& results, ErrorContext* ctx);
};

} // namespace scratchbird::catalog
