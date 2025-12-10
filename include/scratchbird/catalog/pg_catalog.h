#pragma once

/**
 * PostgreSQL pg_catalog Implementation (Stub)
 *
 * Phase D: Catalog Cleanup - pg_catalog virtual catalog handler
 *
 * NOTE: This is a stub implementation that returns empty results.
 * Full implementation requires catalog API fixes.
 *
 * Created: November 26, 2025
 * Phase: Catalog Cleanup Phase D
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/typed_value.h"
#include <string>
#include <vector>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * PgCatalogHandler - PostgreSQL pg_catalog implementation (Stub)
 */
class PgCatalogHandler : public VirtualCatalogHandler {
public:
    explicit PgCatalogHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    ProtocolType getProtocolType() const override {
        return ProtocolType::POSTGRESQL;
    }

    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "pg_catalog");
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
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Schema not found: " + schema_name).c_str());
            return Status::NOT_FOUND;
        }

        if (!ownsTable(schema_name, table_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table not found: pg_catalog." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        // Stub: return empty result set with appropriate columns
        if (equalsCaseInsensitive(table_name, "pg_namespace")) {
            results.column_names = {"oid", "nspname", "nspowner", "nspacl"};
            results.column_types = {DataType::INT64, DataType::VARCHAR, DataType::INT64, DataType::VARCHAR};
        } else if (equalsCaseInsensitive(table_name, "pg_class")) {
            results.column_names = {"oid", "relname", "relnamespace", "relkind"};
            results.column_types = {DataType::INT64, DataType::VARCHAR, DataType::INT64, DataType::VARCHAR};
        } else if (equalsCaseInsensitive(table_name, "pg_attribute")) {
            results.column_names = {"attrelid", "attname", "atttypid", "attnum"};
            results.column_types = {DataType::INT64, DataType::VARCHAR, DataType::INT64, DataType::INT16};
        } else {
            results.column_names = {"STUB"};
            results.column_types = {DataType::VARCHAR};
        }

        return Status::OK;
    }

    Status getTableColumns(const std::string& schema_name,
                           const std::string& table_name,
                           std::vector<CatalogManager::ColumnInfo>& columns,
                           ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Schema not found: " + schema_name).c_str());
            return Status::NOT_FOUND;
        }

        if (!ownsTable(schema_name, table_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table not found: pg_catalog." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        // Stub: return minimal column info
        columns.clear();
        CatalogManager::ColumnInfo col;
        col.column_name = "STUB";
        col.data_type = static_cast<uint16_t>(DataType::VARCHAR);
        col.nullable = true;
        col.ordinal = 1;
        columns.push_back(col);
        return Status::OK;
    }

    Status listTables(const std::string& schema_name,
                      std::vector<std::string>& table_names,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Schema not found: " + schema_name).c_str());
            return Status::NOT_FOUND;
        }

        table_names = table_names_;
        return Status::OK;
    }

    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* /* ctx */ = nullptr) override {
        schema_names.clear();
        schema_names.push_back("pg_catalog");
        return Status::OK;
    }

private:
    std::vector<std::string> table_names_;

    void initializeTableNames() {
        table_names_ = {
            "pg_namespace", "pg_class", "pg_attribute", "pg_type",
            "pg_constraint", "pg_index", "pg_proc", "pg_trigger",
            "pg_authid", "pg_database", "pg_tablespace", "pg_settings",
            "pg_stat_user_tables", "pg_stat_activity"
        };
    }
};

} // namespace scratchbird::catalog
