#pragma once

/**
 * PostgreSQL pg_catalog Emulation Handler
 *
 * Phase D: Catalog Cleanup - PostgreSQL system catalog emulation
 *
 * This module implements PostgreSQL-compatible pg_catalog views that allow
 * PostgreSQL client tools (pgAdmin, psql, etc.) to introspect the ScratchBird
 * catalog as if it were a PostgreSQL database.
 *
 * Implemented Views (PostgreSQL-compatible):
 * - pg_namespace: Schemas
 * - pg_class: Tables, indexes, sequences, views
 * - pg_attribute: Columns
 * - pg_type: Data types
 * - pg_index: Index metadata
 * - pg_constraint: Constraints
 * - pg_proc: Functions and procedures
 * - pg_trigger: Triggers
 * - pg_user: Users
 * - pg_roles: Roles
 * - pg_database: Databases
 * - pg_tablespace: Tablespaces
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
 * PgCatalogHandler - PostgreSQL pg_catalog emulation
 *
 * Provides virtual views that query the ScratchBird internal catalog and
 * present data in PostgreSQL pg_catalog format for tool compatibility.
 *
 * Note: PostgreSQL uses OIDs (Object Identifiers) extensively. We map
 * ScratchBird IDs to OIDs for compatibility.
 *
 * Usage:
 *   SELECT * FROM pg_catalog.pg_class WHERE relname = 'users';
 *   SELECT * FROM pg_catalog.pg_attribute WHERE attrelid = 12345;
 */
class PgCatalogHandler : public VirtualCatalogHandler {
public:
    /**
     * Constructor - takes catalog manager reference
     */
    explicit PgCatalogHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    /**
     * Get the protocol type
     */
    ProtocolType getProtocolType() const override {
        return ProtocolType::POSTGRESQL;
    }

    /**
     * Check if this handler owns the given schema
     * @return true for "pg_catalog" (case-insensitive)
     */
    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "pg_catalog");
    }

    /**
     * Check if this handler owns the given table
     */
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

    /**
     * Execute a query against a virtual table
     */
    Status queryTable(const std::string& schema_name,
                      const std::string& table_name,
                      const std::string& where_clause,
                      VirtualResultSet& results,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        // Route to appropriate view handler
        if (equalsCaseInsensitive(table_name, "pg_namespace")) {
            return queryPgNamespace(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_class")) {
            return queryPgClass(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_attribute")) {
            return queryPgAttribute(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_type")) {
            return queryPgType(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_index")) {
            return queryPgIndex(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_constraint")) {
            return queryPgConstraint(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_proc")) {
            return queryPgProc(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_trigger")) {
            return queryPgTrigger(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_user")) {
            return queryPgUser(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_roles")) {
            return queryPgRoles(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_database")) {
            return queryPgDatabase(where_clause, results, ctx);
        } else if (equalsCaseInsensitive(table_name, "pg_tablespace")) {
            return queryPgTablespace(where_clause, results, ctx);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: pg_catalog." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * Get column definitions for a virtual table
     */
    Status getTableColumns(const std::string& schema_name,
                           const std::string& table_name,
                           std::vector<CatalogManager::ColumnInfo>& columns,
                           ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        if (equalsCaseInsensitive(table_name, "pg_namespace")) {
            return getPgNamespaceColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_class")) {
            return getPgClassColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_attribute")) {
            return getPgAttributeColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_type")) {
            return getPgTypeColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_index")) {
            return getPgIndexColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_constraint")) {
            return getPgConstraintColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_proc")) {
            return getPgProcColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_trigger")) {
            return getPgTriggerColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_user")) {
            return getPgUserColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_roles")) {
            return getPgRolesColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_database")) {
            return getPgDatabaseColumns(columns);
        } else if (equalsCaseInsensitive(table_name, "pg_tablespace")) {
            return getPgTablespaceColumns(columns);
        }

        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          "Table not found: pg_catalog." + table_name);
        return Status::NOT_FOUND;
    }

    /**
     * List all virtual tables in pg_catalog
     */
    Status listTables(const std::string& schema_name,
                      std::vector<std::string>& table_names,
                      ErrorContext* ctx = nullptr) override {
        if (!ownsSchema(schema_name)) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "Schema not found: " + schema_name);
            return Status::NOT_FOUND;
        }

        table_names = table_names_;
        return Status::OK;
    }

    /**
     * List all schemas owned by this handler
     */
    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* ctx = nullptr) override {
        schema_names.clear();
        schema_names.push_back("pg_catalog");
        return Status::OK;
    }

private:
    std::vector<std::string> table_names_;

    void initializeTableNames() {
        table_names_ = {
            "pg_namespace",
            "pg_class",
            "pg_attribute",
            "pg_type",
            "pg_index",
            "pg_constraint",
            "pg_proc",
            "pg_trigger",
            "pg_user",
            "pg_roles",
            "pg_database",
            "pg_tablespace"
        };
    }

    // ========================================================================
    // PostgreSQL OID Mapping
    // ========================================================================

    /**
     * Convert ScratchBird ID to PostgreSQL-style OID
     * We use the raw ID value as the OID for simplicity
     */
    static int32_t idToOid(const ID& id) {
        return static_cast<int32_t>(id.value() & 0x7FFFFFFF);
    }

    /**
     * Map ScratchBird DataType to PostgreSQL type OID
     */
    static int32_t dataTypeToPgTypeOid(DataType type) {
        // PostgreSQL built-in type OIDs
        switch (type) {
            case DataType::BOOLEAN:   return 16;    // bool
            case DataType::INT16:     return 21;    // int2
            case DataType::INT32:     return 23;    // int4
            case DataType::INT64:     return 20;    // int8
            case DataType::FLOAT32:   return 700;   // float4
            case DataType::FLOAT64:   return 701;   // float8
            case DataType::DECIMAL:   return 1700;  // numeric
            case DataType::VARCHAR:   return 1043;  // varchar
            case DataType::CHAR:      return 1042;  // bpchar
            case DataType::TEXT:      return 25;    // text
            case DataType::BLOB:      return 17;    // bytea
            case DataType::DATE:      return 1082;  // date
            case DataType::TIME:      return 1083;  // time
            case DataType::TIMESTAMP: return 1114;  // timestamp
            case DataType::UUID:      return 2950;  // uuid
            default:                  return 0;     // unknown
        }
    }

    // ========================================================================
    // View Query Implementations
    // ========================================================================

    /**
     * pg_namespace - schemas
     */
    Status queryPgNamespace(const std::string& where_clause,
                            VirtualResultSet& results,
                            ErrorContext* ctx = nullptr) {
        results.column_names = {"oid", "nspname", "nspowner", "nspacl"};
        results.column_types = {DataType::INT32, DataType::VARCHAR, DataType::INT32, DataType::VARCHAR};

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt32(idToOid(schema.schema_id))},
                {"nspname", TypedValue::makeString(schema.schema_name)},
                {"nspowner", TypedValue::makeInt32(idToOid(schema.owner_id))},
                {"nspacl", TypedValue::makeNull()}  // Access control list
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * pg_class - tables, indexes, sequences, views
     */
    Status queryPgClass(const std::string& where_clause,
                        VirtualResultSet& results,
                        ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "relname", "relnamespace", "reltype", "reloftype",
            "relowner", "relam", "relfilenode", "reltablespace",
            "relpages", "reltuples", "relallvisible", "reltoastrelid",
            "relhasindex", "relisshared", "relpersistence", "relkind",
            "relnatts", "relchecks", "relhasrules", "relhastriggers",
            "relhassubclass", "relrowsecurity", "relforcerowsecurity",
            "relispopulated", "relreplident", "relispartition", "relrewrite",
            "relfrozenxid", "relminmxid", "relacl", "reloptions", "relpartbound"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // oid
        results.column_types[2] = DataType::INT32;   // relnamespace
        results.column_types[3] = DataType::INT32;   // reltype
        results.column_types[4] = DataType::INT32;   // reloftype
        results.column_types[5] = DataType::INT32;   // relowner
        results.column_types[9] = DataType::INT32;   // relpages
        results.column_types[10] = DataType::FLOAT32; // reltuples
        results.column_types[17] = DataType::INT16;  // relnatts
        results.column_types[18] = DataType::INT16;  // relchecks

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            // Add tables
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (s.ok()) {
                for (const auto& table : tables) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt32(idToOid(table.table_id))},
                        {"relname", TypedValue::makeString(table.table_name)},
                        {"relnamespace", TypedValue::makeInt32(idToOid(schema.schema_id))},
                        {"reltype", TypedValue::makeInt32(0)},
                        {"reloftype", TypedValue::makeInt32(0)},
                        {"relowner", TypedValue::makeInt32(idToOid(table.owner_id))},
                        {"relam", TypedValue::makeInt32(0)},
                        {"relfilenode", TypedValue::makeInt32(idToOid(table.table_id))},
                        {"reltablespace", TypedValue::makeInt32(0)},
                        {"relpages", TypedValue::makeInt32(static_cast<int32_t>(table.page_count))},
                        {"reltuples", TypedValue::makeFloat(static_cast<float>(table.row_count))},
                        {"relallvisible", TypedValue::makeInt32(0)},
                        {"reltoastrelid", TypedValue::makeInt32(0)},
                        {"relhasindex", TypedValue::makeString(table.has_indexes ? "t" : "f")},
                        {"relisshared", TypedValue::makeString("f")},
                        {"relpersistence", TypedValue::makeString(table.is_temp ? "t" : "p")},
                        {"relkind", TypedValue::makeString("r")},  // r = ordinary table
                        {"relnatts", TypedValue::makeInt16(static_cast<int16_t>(table.column_count))},
                        {"relchecks", TypedValue::makeInt16(0)},
                        {"relhasrules", TypedValue::makeString("f")},
                        {"relhastriggers", TypedValue::makeString(table.has_triggers ? "t" : "f")},
                        {"relhassubclass", TypedValue::makeString("f")},
                        {"relrowsecurity", TypedValue::makeString("f")},
                        {"relforcerowsecurity", TypedValue::makeString("f")},
                        {"relispopulated", TypedValue::makeString("t")},
                        {"relreplident", TypedValue::makeString("d")},
                        {"relispartition", TypedValue::makeString("f")},
                        {"relrewrite", TypedValue::makeInt32(0)},
                        {"relfrozenxid", TypedValue::makeInt32(0)},
                        {"relminmxid", TypedValue::makeInt32(0)},
                        {"relacl", TypedValue::makeNull()},
                        {"reloptions", TypedValue::makeNull()},
                        {"relpartbound", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Add views
            std::vector<CatalogManager::ViewInfo> views;
            s = catalog_manager_->listViews(schema.schema_id, views, ctx);
            if (s.ok()) {
                for (const auto& view : views) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt32(idToOid(view.view_id))},
                        {"relname", TypedValue::makeString(view.view_name)},
                        {"relnamespace", TypedValue::makeInt32(idToOid(schema.schema_id))},
                        {"reltype", TypedValue::makeInt32(0)},
                        {"reloftype", TypedValue::makeInt32(0)},
                        {"relowner", TypedValue::makeInt32(idToOid(view.owner_id))},
                        {"relam", TypedValue::makeInt32(0)},
                        {"relfilenode", TypedValue::makeInt32(0)},
                        {"reltablespace", TypedValue::makeInt32(0)},
                        {"relpages", TypedValue::makeInt32(0)},
                        {"reltuples", TypedValue::makeFloat(0.0f)},
                        {"relallvisible", TypedValue::makeInt32(0)},
                        {"reltoastrelid", TypedValue::makeInt32(0)},
                        {"relhasindex", TypedValue::makeString("f")},
                        {"relisshared", TypedValue::makeString("f")},
                        {"relpersistence", TypedValue::makeString("p")},
                        {"relkind", TypedValue::makeString("v")},  // v = view
                        {"relnatts", TypedValue::makeInt16(0)},
                        {"relchecks", TypedValue::makeInt16(0)},
                        {"relhasrules", TypedValue::makeString("t")},  // Views have rules
                        {"relhastriggers", TypedValue::makeString("f")},
                        {"relhassubclass", TypedValue::makeString("f")},
                        {"relrowsecurity", TypedValue::makeString("f")},
                        {"relforcerowsecurity", TypedValue::makeString("f")},
                        {"relispopulated", TypedValue::makeString("t")},
                        {"relreplident", TypedValue::makeString("n")},
                        {"relispartition", TypedValue::makeString("f")},
                        {"relrewrite", TypedValue::makeInt32(0)},
                        {"relfrozenxid", TypedValue::makeInt32(0)},
                        {"relminmxid", TypedValue::makeInt32(0)},
                        {"relacl", TypedValue::makeNull()},
                        {"reloptions", TypedValue::makeNull()},
                        {"relpartbound", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            // Add sequences
            std::vector<CatalogManager::SequenceInfo> sequences;
            s = catalog_manager_->listSequences(schema.schema_id, sequences, ctx);
            if (s.ok()) {
                for (const auto& seq : sequences) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt32(idToOid(seq.sequence_id))},
                        {"relname", TypedValue::makeString(seq.sequence_name)},
                        {"relnamespace", TypedValue::makeInt32(idToOid(schema.schema_id))},
                        {"reltype", TypedValue::makeInt32(0)},
                        {"reloftype", TypedValue::makeInt32(0)},
                        {"relowner", TypedValue::makeInt32(idToOid(seq.owner_id))},
                        {"relam", TypedValue::makeInt32(0)},
                        {"relfilenode", TypedValue::makeInt32(0)},
                        {"reltablespace", TypedValue::makeInt32(0)},
                        {"relpages", TypedValue::makeInt32(1)},
                        {"reltuples", TypedValue::makeFloat(1.0f)},
                        {"relallvisible", TypedValue::makeInt32(0)},
                        {"reltoastrelid", TypedValue::makeInt32(0)},
                        {"relhasindex", TypedValue::makeString("f")},
                        {"relisshared", TypedValue::makeString("f")},
                        {"relpersistence", TypedValue::makeString("p")},
                        {"relkind", TypedValue::makeString("S")},  // S = sequence
                        {"relnatts", TypedValue::makeInt16(3)},
                        {"relchecks", TypedValue::makeInt16(0)},
                        {"relhasrules", TypedValue::makeString("f")},
                        {"relhastriggers", TypedValue::makeString("f")},
                        {"relhassubclass", TypedValue::makeString("f")},
                        {"relrowsecurity", TypedValue::makeString("f")},
                        {"relforcerowsecurity", TypedValue::makeString("f")},
                        {"relispopulated", TypedValue::makeString("t")},
                        {"relreplident", TypedValue::makeString("n")},
                        {"relispartition", TypedValue::makeString("f")},
                        {"relrewrite", TypedValue::makeInt32(0)},
                        {"relfrozenxid", TypedValue::makeInt32(0)},
                        {"relminmxid", TypedValue::makeInt32(0)},
                        {"relacl", TypedValue::makeNull()},
                        {"reloptions", TypedValue::makeNull()},
                        {"relpartbound", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * pg_attribute - columns
     */
    Status queryPgAttribute(const std::string& where_clause,
                            VirtualResultSet& results,
                            ErrorContext* ctx = nullptr) {
        results.column_names = {
            "attrelid", "attname", "atttypid", "attstattarget", "attlen",
            "attnum", "attndims", "attcacheoff", "atttypmod", "attbyval",
            "attalign", "attstorage", "attcompression", "attnotnull",
            "atthasdef", "atthasmissing", "attidentity", "attgenerated",
            "attisdropped", "attislocal", "attinhcount", "attcollation",
            "attacl", "attoptions", "attfdwoptions", "attmissingval"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // attrelid
        results.column_types[2] = DataType::INT32;   // atttypid
        results.column_types[3] = DataType::INT32;   // attstattarget
        results.column_types[4] = DataType::INT16;   // attlen
        results.column_types[5] = DataType::INT16;   // attnum
        results.column_types[6] = DataType::INT32;   // attndims
        results.column_types[7] = DataType::INT32;   // attcacheoff
        results.column_types[8] = DataType::INT32;   // atttypmod
        results.column_types[20] = DataType::INT32;  // attinhcount
        results.column_types[21] = DataType::INT32;  // attcollation

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                std::vector<CatalogManager::ColumnInfo> columns;
                s = catalog_manager_->getColumns(table.table_id, columns, ctx);
                if (!s.ok()) continue;

                for (const auto& col : columns) {
                    VirtualRow row;
                    row.columns = {
                        {"attrelid", TypedValue::makeInt32(idToOid(table.table_id))},
                        {"attname", TypedValue::makeString(col.column_name)},
                        {"atttypid", TypedValue::makeInt32(dataTypeToPgTypeOid(col.data_type))},
                        {"attstattarget", TypedValue::makeInt32(-1)},
                        {"attlen", TypedValue::makeInt16(getTypeLength(col.data_type))},
                        {"attnum", TypedValue::makeInt16(static_cast<int16_t>(col.ordinal))},
                        {"attndims", TypedValue::makeInt32(0)},
                        {"attcacheoff", TypedValue::makeInt32(-1)},
                        {"atttypmod", TypedValue::makeInt32(getTypeMod(col))},
                        {"attbyval", TypedValue::makeString(isPassByValue(col.data_type) ? "t" : "f")},
                        {"attalign", TypedValue::makeString(getTypeAlign(col.data_type))},
                        {"attstorage", TypedValue::makeString(getTypeStorage(col.data_type))},
                        {"attcompression", TypedValue::makeString("")},
                        {"attnotnull", TypedValue::makeString(col.nullable ? "f" : "t")},
                        {"atthasdef", TypedValue::makeString(col.default_value.empty() ? "f" : "t")},
                        {"atthasmissing", TypedValue::makeString("f")},
                        {"attidentity", TypedValue::makeString(col.is_identity ? "a" : "")},
                        {"attgenerated", TypedValue::makeString(col.is_computed ? "s" : "")},
                        {"attisdropped", TypedValue::makeString("f")},
                        {"attislocal", TypedValue::makeString("t")},
                        {"attinhcount", TypedValue::makeInt32(0)},
                        {"attcollation", TypedValue::makeInt32(100)},  // Default collation
                        {"attacl", TypedValue::makeNull()},
                        {"attoptions", TypedValue::makeNull()},
                        {"attfdwoptions", TypedValue::makeNull()},
                        {"attmissingval", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * pg_type - data types
     */
    Status queryPgType(const std::string& where_clause,
                       VirtualResultSet& results,
                       ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "typname", "typnamespace", "typowner", "typlen",
            "typbyval", "typtype", "typcategory", "typispreferred",
            "typisdefined", "typdelim", "typrelid", "typsubscript",
            "typelem", "typarray", "typinput", "typoutput", "typreceive",
            "typsend", "typmodin", "typmodout", "typanalyze", "typalign",
            "typstorage", "typnotnull", "typbasetype", "typtypmod",
            "typndims", "typcollation", "typdefaultbin", "typdefault", "typacl"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // oid
        results.column_types[2] = DataType::INT32;   // typnamespace
        results.column_types[3] = DataType::INT32;   // typowner
        results.column_types[4] = DataType::INT16;   // typlen

        // Add built-in PostgreSQL types
        struct PgTypeInfo {
            int32_t oid;
            const char* name;
            int16_t len;
            char type;
            char category;
        };

        PgTypeInfo builtinTypes[] = {
            {16, "bool", 1, 'b', 'B'},
            {17, "bytea", -1, 'b', 'U'},
            {20, "int8", 8, 'b', 'N'},
            {21, "int2", 2, 'b', 'N'},
            {23, "int4", 4, 'b', 'N'},
            {25, "text", -1, 'b', 'S'},
            {700, "float4", 4, 'b', 'N'},
            {701, "float8", 8, 'b', 'N'},
            {1042, "bpchar", -1, 'b', 'S'},
            {1043, "varchar", -1, 'b', 'S'},
            {1082, "date", 4, 'b', 'D'},
            {1083, "time", 8, 'b', 'D'},
            {1114, "timestamp", 8, 'b', 'D'},
            {1700, "numeric", -1, 'b', 'N'},
            {2950, "uuid", 16, 'b', 'U'}
        };

        int32_t pgCatalogOid = 11;  // pg_catalog namespace OID

        for (const auto& t : builtinTypes) {
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt32(t.oid)},
                {"typname", TypedValue::makeString(t.name)},
                {"typnamespace", TypedValue::makeInt32(pgCatalogOid)},
                {"typowner", TypedValue::makeInt32(10)},  // postgres superuser
                {"typlen", TypedValue::makeInt16(t.len)},
                {"typbyval", TypedValue::makeString(t.len > 0 && t.len <= 8 ? "t" : "f")},
                {"typtype", TypedValue::makeString(std::string(1, t.type))},
                {"typcategory", TypedValue::makeString(std::string(1, t.category))},
                {"typispreferred", TypedValue::makeString("f")},
                {"typisdefined", TypedValue::makeString("t")},
                {"typdelim", TypedValue::makeString(",")},
                {"typrelid", TypedValue::makeInt32(0)},
                {"typsubscript", TypedValue::makeString("-")},
                {"typelem", TypedValue::makeInt32(0)},
                {"typarray", TypedValue::makeInt32(0)},
                {"typinput", TypedValue::makeString(std::string(t.name) + "in")},
                {"typoutput", TypedValue::makeString(std::string(t.name) + "out")},
                {"typreceive", TypedValue::makeString(std::string(t.name) + "recv")},
                {"typsend", TypedValue::makeString(std::string(t.name) + "send")},
                {"typmodin", TypedValue::makeString("-")},
                {"typmodout", TypedValue::makeString("-")},
                {"typanalyze", TypedValue::makeString("-")},
                {"typalign", TypedValue::makeString("i")},
                {"typstorage", TypedValue::makeString("p")},
                {"typnotnull", TypedValue::makeString("f")},
                {"typbasetype", TypedValue::makeInt32(0)},
                {"typtypmod", TypedValue::makeInt32(-1)},
                {"typndims", TypedValue::makeInt32(0)},
                {"typcollation", TypedValue::makeInt32(0)},
                {"typdefaultbin", TypedValue::makeNull()},
                {"typdefault", TypedValue::makeNull()},
                {"typacl", TypedValue::makeNull()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * pg_index - index metadata
     */
    Status queryPgIndex(const std::string& where_clause,
                        VirtualResultSet& results,
                        ErrorContext* ctx = nullptr) {
        results.column_names = {
            "indexrelid", "indrelid", "indnatts", "indnkeyatts",
            "indisunique", "indnullsnotdistinct", "indisprimary",
            "indisexclusion", "indimmediate", "indisclustered",
            "indisvalid", "indcheckxmin", "indisready", "indislive",
            "indisreplident", "indkey", "indcollation", "indclass",
            "indoption", "indexprs", "indpred"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;  // indexrelid
        results.column_types[1] = DataType::INT32;  // indrelid
        results.column_types[2] = DataType::INT16;  // indnatts
        results.column_types[3] = DataType::INT16;  // indnkeyatts

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                std::vector<CatalogManager::IndexInfo> indexes;
                s = catalog_manager_->listIndexes(table.table_id, indexes, ctx);
                if (!s.ok()) continue;

                for (const auto& idx : indexes) {
                    // Build indkey array string
                    std::string indkey;
                    for (size_t i = 0; i < idx.column_ordinals.size(); ++i) {
                        if (i > 0) indkey += " ";
                        indkey += std::to_string(idx.column_ordinals[i]);
                    }

                    VirtualRow row;
                    row.columns = {
                        {"indexrelid", TypedValue::makeInt32(idToOid(idx.index_id))},
                        {"indrelid", TypedValue::makeInt32(idToOid(table.table_id))},
                        {"indnatts", TypedValue::makeInt16(static_cast<int16_t>(idx.column_ordinals.size()))},
                        {"indnkeyatts", TypedValue::makeInt16(static_cast<int16_t>(idx.column_ordinals.size()))},
                        {"indisunique", TypedValue::makeString(idx.is_unique ? "t" : "f")},
                        {"indnullsnotdistinct", TypedValue::makeString("f")},
                        {"indisprimary", TypedValue::makeString(idx.is_primary_key ? "t" : "f")},
                        {"indisexclusion", TypedValue::makeString("f")},
                        {"indimmediate", TypedValue::makeString("t")},
                        {"indisclustered", TypedValue::makeString("f")},
                        {"indisvalid", TypedValue::makeString("t")},
                        {"indcheckxmin", TypedValue::makeString("f")},
                        {"indisready", TypedValue::makeString("t")},
                        {"indislive", TypedValue::makeString("t")},
                        {"indisreplident", TypedValue::makeString("f")},
                        {"indkey", TypedValue::makeString(indkey)},
                        {"indcollation", TypedValue::makeString("")},
                        {"indclass", TypedValue::makeString("")},
                        {"indoption", TypedValue::makeString("")},
                        {"indexprs", TypedValue::makeNull()},
                        {"indpred", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * pg_constraint - constraints
     */
    Status queryPgConstraint(const std::string& where_clause,
                             VirtualResultSet& results,
                             ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "conname", "connamespace", "contype", "condeferrable",
            "condeferred", "convalidated", "conrelid", "contypid",
            "conindid", "conparentid", "confrelid", "confupdtype",
            "confdeltype", "confmatchtype", "conislocal", "coninhcount",
            "connoinherit", "conkey", "confkey", "conpfeqop", "conppeqop",
            "conffeqop", "confdelsetcols", "conexclop", "conbin"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // oid
        results.column_types[2] = DataType::INT32;   // connamespace
        results.column_types[7] = DataType::INT32;   // conrelid
        results.column_types[8] = DataType::INT32;   // contypid
        results.column_types[9] = DataType::INT32;   // conindid
        results.column_types[10] = DataType::INT32;  // conparentid
        results.column_types[11] = DataType::INT32;  // confrelid
        results.column_types[16] = DataType::INT32;  // coninhcount

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                std::vector<CatalogManager::ConstraintInfo> constraints;
                s = catalog_manager_->listConstraints(table.table_id, constraints, ctx);
                if (!s.ok()) continue;

                for (const auto& con : constraints) {
                    char contype = 'c';  // default: check
                    switch (con.constraint_type) {
                        case CatalogManager::ConstraintType::PRIMARY_KEY: contype = 'p'; break;
                        case CatalogManager::ConstraintType::UNIQUE:      contype = 'u'; break;
                        case CatalogManager::ConstraintType::FOREIGN_KEY: contype = 'f'; break;
                        case CatalogManager::ConstraintType::CHECK:       contype = 'c'; break;
                        case CatalogManager::ConstraintType::NOT_NULL:    contype = 'n'; break;
                    }

                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt32(idToOid(con.constraint_id))},
                        {"conname", TypedValue::makeString(con.constraint_name)},
                        {"connamespace", TypedValue::makeInt32(idToOid(schema.schema_id))},
                        {"contype", TypedValue::makeString(std::string(1, contype))},
                        {"condeferrable", TypedValue::makeString(con.is_deferrable ? "t" : "f")},
                        {"condeferred", TypedValue::makeString(con.initially_deferred ? "t" : "f")},
                        {"convalidated", TypedValue::makeString("t")},
                        {"conrelid", TypedValue::makeInt32(idToOid(table.table_id))},
                        {"contypid", TypedValue::makeInt32(0)},
                        {"conindid", TypedValue::makeInt32(0)},
                        {"conparentid", TypedValue::makeInt32(0)},
                        {"confrelid", TypedValue::makeInt32(0)},
                        {"confupdtype", TypedValue::makeString(" ")},
                        {"confdeltype", TypedValue::makeString(" ")},
                        {"confmatchtype", TypedValue::makeString(" ")},
                        {"conislocal", TypedValue::makeString("t")},
                        {"coninhcount", TypedValue::makeInt32(0)},
                        {"connoinherit", TypedValue::makeString("f")},
                        {"conkey", TypedValue::makeNull()},
                        {"confkey", TypedValue::makeNull()},
                        {"conpfeqop", TypedValue::makeNull()},
                        {"conppeqop", TypedValue::makeNull()},
                        {"conffeqop", TypedValue::makeNull()},
                        {"confdelsetcols", TypedValue::makeNull()},
                        {"conexclop", TypedValue::makeNull()},
                        {"conbin", con.check_expression.empty() ?
                            TypedValue::makeNull() : TypedValue::makeString(con.check_expression)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * pg_proc - functions and procedures
     */
    Status queryPgProc(const std::string& where_clause,
                       VirtualResultSet& results,
                       ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "proname", "pronamespace", "proowner", "prolang",
            "procost", "prorows", "provariadic", "prosupport", "prokind",
            "prosecdef", "proleakproof", "proisstrict", "proretset",
            "provolatile", "proparallel", "pronargs", "pronargdefaults",
            "prorettype", "proargtypes", "proallargtypes", "proargmodes",
            "proargnames", "proargdefaults", "protrftypes", "prosrc",
            "probin", "prosqlbody", "proconfig", "proacl"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // oid
        results.column_types[2] = DataType::INT32;   // pronamespace
        results.column_types[3] = DataType::INT32;   // proowner
        results.column_types[4] = DataType::INT32;   // prolang
        results.column_types[16] = DataType::INT16;  // pronargs
        results.column_types[17] = DataType::INT16;  // pronargdefaults
        results.column_types[18] = DataType::INT32;  // prorettype

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::FunctionInfo> functions;
            s = catalog_manager_->listFunctions(schema.schema_id, functions, ctx);
            if (s.ok()) {
                for (const auto& func : functions) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt32(idToOid(func.function_id))},
                        {"proname", TypedValue::makeString(func.function_name)},
                        {"pronamespace", TypedValue::makeInt32(idToOid(schema.schema_id))},
                        {"proowner", TypedValue::makeInt32(idToOid(func.owner_id))},
                        {"prolang", TypedValue::makeInt32(14)},  // SQL language
                        {"procost", TypedValue::makeString("100")},
                        {"prorows", TypedValue::makeString("0")},
                        {"provariadic", TypedValue::makeInt32(0)},
                        {"prosupport", TypedValue::makeString("-")},
                        {"prokind", TypedValue::makeString("f")},  // f = function
                        {"prosecdef", TypedValue::makeString("f")},
                        {"proleakproof", TypedValue::makeString("f")},
                        {"proisstrict", TypedValue::makeString("f")},
                        {"proretset", TypedValue::makeString("f")},
                        {"provolatile", TypedValue::makeString(func.is_deterministic ? "i" : "v")},
                        {"proparallel", TypedValue::makeString("u")},
                        {"pronargs", TypedValue::makeInt16(static_cast<int16_t>(func.parameters.size()))},
                        {"pronargdefaults", TypedValue::makeInt16(0)},
                        {"prorettype", TypedValue::makeInt32(dataTypeToPgTypeOid(func.return_type))},
                        {"proargtypes", TypedValue::makeString("")},
                        {"proallargtypes", TypedValue::makeNull()},
                        {"proargmodes", TypedValue::makeNull()},
                        {"proargnames", TypedValue::makeNull()},
                        {"proargdefaults", TypedValue::makeNull()},
                        {"protrftypes", TypedValue::makeNull()},
                        {"prosrc", TypedValue::makeString(func.body)},
                        {"probin", TypedValue::makeNull()},
                        {"prosqlbody", TypedValue::makeNull()},
                        {"proconfig", TypedValue::makeNull()},
                        {"proacl", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            std::vector<CatalogManager::ProcedureInfo> procedures;
            s = catalog_manager_->listProcedures(schema.schema_id, procedures, ctx);
            if (s.ok()) {
                for (const auto& proc : procedures) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt32(idToOid(proc.procedure_id))},
                        {"proname", TypedValue::makeString(proc.procedure_name)},
                        {"pronamespace", TypedValue::makeInt32(idToOid(schema.schema_id))},
                        {"proowner", TypedValue::makeInt32(idToOid(proc.owner_id))},
                        {"prolang", TypedValue::makeInt32(14)},
                        {"procost", TypedValue::makeString("100")},
                        {"prorows", TypedValue::makeString("0")},
                        {"provariadic", TypedValue::makeInt32(0)},
                        {"prosupport", TypedValue::makeString("-")},
                        {"prokind", TypedValue::makeString("p")},  // p = procedure
                        {"prosecdef", TypedValue::makeString("f")},
                        {"proleakproof", TypedValue::makeString("f")},
                        {"proisstrict", TypedValue::makeString("f")},
                        {"proretset", TypedValue::makeString("f")},
                        {"provolatile", TypedValue::makeString("v")},
                        {"proparallel", TypedValue::makeString("u")},
                        {"pronargs", TypedValue::makeInt16(static_cast<int16_t>(proc.parameters.size()))},
                        {"pronargdefaults", TypedValue::makeInt16(0)},
                        {"prorettype", TypedValue::makeInt32(0)},  // void
                        {"proargtypes", TypedValue::makeString("")},
                        {"proallargtypes", TypedValue::makeNull()},
                        {"proargmodes", TypedValue::makeNull()},
                        {"proargnames", TypedValue::makeNull()},
                        {"proargdefaults", TypedValue::makeNull()},
                        {"protrftypes", TypedValue::makeNull()},
                        {"prosrc", TypedValue::makeString(proc.body)},
                        {"probin", TypedValue::makeNull()},
                        {"prosqlbody", TypedValue::makeNull()},
                        {"proconfig", TypedValue::makeNull()},
                        {"proacl", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * pg_trigger - triggers
     */
    Status queryPgTrigger(const std::string& where_clause,
                          VirtualResultSet& results,
                          ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "tgrelid", "tgparentid", "tgname", "tgfoid",
            "tgtype", "tgenabled", "tgisinternal", "tgconstrrelid",
            "tgconstrindid", "tgconstraint", "tgdeferrable", "tginitdeferred",
            "tgnargs", "tgattr", "tgargs", "tgqual", "tgoldtable", "tgnewtable"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;   // oid
        results.column_types[1] = DataType::INT32;   // tgrelid
        results.column_types[2] = DataType::INT32;   // tgparentid
        results.column_types[4] = DataType::INT32;   // tgfoid
        results.column_types[5] = DataType::INT16;   // tgtype
        results.column_types[8] = DataType::INT32;   // tgconstrrelid
        results.column_types[9] = DataType::INT32;   // tgconstrindid
        results.column_types[10] = DataType::INT32;  // tgconstraint
        results.column_types[13] = DataType::INT16;  // tgnargs

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status s = catalog_manager_->listSchemas(schemas, ctx);
        if (!s.ok()) return s;

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            s = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (!s.ok()) continue;

            for (const auto& table : tables) {
                std::vector<CatalogManager::TriggerInfo> triggers;
                s = catalog_manager_->listTriggers(table.table_id, triggers, ctx);
                if (!s.ok()) continue;

                for (const auto& trig : triggers) {
                    // Build tgtype bitmask
                    int16_t tgtype = 0;
                    if (trig.for_each_row) tgtype |= 1;  // ROW
                    else tgtype |= 2;  // STATEMENT
                    if (trig.timing == CatalogManager::TriggerTiming::BEFORE) tgtype |= 2;
                    else if (trig.timing == CatalogManager::TriggerTiming::AFTER) tgtype |= 0;
                    else if (trig.timing == CatalogManager::TriggerTiming::INSTEAD_OF) tgtype |= 64;
                    if (trig.event == CatalogManager::TriggerEvent::INSERT) tgtype |= 4;
                    if (trig.event == CatalogManager::TriggerEvent::DELETE) tgtype |= 8;
                    if (trig.event == CatalogManager::TriggerEvent::UPDATE) tgtype |= 16;

                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt32(idToOid(trig.trigger_id))},
                        {"tgrelid", TypedValue::makeInt32(idToOid(table.table_id))},
                        {"tgparentid", TypedValue::makeInt32(0)},
                        {"tgname", TypedValue::makeString(trig.trigger_name)},
                        {"tgfoid", TypedValue::makeInt32(0)},
                        {"tgtype", TypedValue::makeInt16(tgtype)},
                        {"tgenabled", TypedValue::makeString(trig.is_enabled ? "O" : "D")},
                        {"tgisinternal", TypedValue::makeString("f")},
                        {"tgconstrrelid", TypedValue::makeInt32(0)},
                        {"tgconstrindid", TypedValue::makeInt32(0)},
                        {"tgconstraint", TypedValue::makeInt32(0)},
                        {"tgdeferrable", TypedValue::makeString("f")},
                        {"tginitdeferred", TypedValue::makeString("f")},
                        {"tgnargs", TypedValue::makeInt16(0)},
                        {"tgattr", TypedValue::makeString("")},
                        {"tgargs", TypedValue::makeString("")},
                        {"tgqual", trig.when_clause.empty() ?
                            TypedValue::makeNull() : TypedValue::makeString(trig.when_clause)},
                        {"tgoldtable", TypedValue::makeNull()},
                        {"tgnewtable", TypedValue::makeNull()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    /**
     * pg_user - users
     */
    Status queryPgUser(const std::string& where_clause,
                       VirtualResultSet& results,
                       ErrorContext* ctx = nullptr) {
        results.column_names = {
            "usename", "usesysid", "usecreatedb", "usesuper",
            "userepl", "usebypassrls", "passwd", "valuntil", "useconfig"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[1] = DataType::INT32;  // usesysid

        std::vector<CatalogManager::UserInfo> users;
        Status s = catalog_manager_->listUsers(users, ctx);
        if (!s.ok()) return s;

        for (const auto& user : users) {
            VirtualRow row;
            row.columns = {
                {"usename", TypedValue::makeString(user.username)},
                {"usesysid", TypedValue::makeInt32(idToOid(user.user_id))},
                {"usecreatedb", TypedValue::makeString(user.can_create_db ? "t" : "f")},
                {"usesuper", TypedValue::makeString(user.is_superuser ? "t" : "f")},
                {"userepl", TypedValue::makeString("f")},
                {"usebypassrls", TypedValue::makeString("f")},
                {"passwd", TypedValue::makeString("********")},
                {"valuntil", TypedValue::makeNull()},
                {"useconfig", TypedValue::makeNull()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * pg_roles - roles
     */
    Status queryPgRoles(const std::string& where_clause,
                        VirtualResultSet& results,
                        ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "rolname", "rolsuper", "rolinherit", "rolcreaterole",
            "rolcreatedb", "rolcanlogin", "rolreplication", "rolconnlimit",
            "rolpassword", "rolvaliduntil", "rolbypassrls", "rolconfig"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;  // oid
        results.column_types[8] = DataType::INT32;  // rolconnlimit

        std::vector<CatalogManager::RoleInfo> roles;
        Status s = catalog_manager_->listRoles(roles, ctx);
        if (!s.ok()) return s;

        for (const auto& role : roles) {
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt32(idToOid(role.role_id))},
                {"rolname", TypedValue::makeString(role.role_name)},
                {"rolsuper", TypedValue::makeString("f")},
                {"rolinherit", TypedValue::makeString("t")},
                {"rolcreaterole", TypedValue::makeString("f")},
                {"rolcreatedb", TypedValue::makeString("f")},
                {"rolcanlogin", TypedValue::makeString("f")},
                {"rolreplication", TypedValue::makeString("f")},
                {"rolconnlimit", TypedValue::makeInt32(-1)},
                {"rolpassword", TypedValue::makeNull()},
                {"rolvaliduntil", TypedValue::makeNull()},
                {"rolbypassrls", TypedValue::makeString("f")},
                {"rolconfig", TypedValue::makeNull()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    /**
     * pg_database - databases
     */
    Status queryPgDatabase(const std::string& where_clause,
                           VirtualResultSet& results,
                           ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "datname", "datdba", "encoding", "datlocprovider",
            "datistemplate", "datallowconn", "datconnlimit", "datfrozenxid",
            "datminmxid", "dattablespace", "datcollate", "datctype",
            "daticulocale", "daticurules", "datcollversion", "datacl"
        };

        results.column_types.resize(results.column_names.size(), DataType::VARCHAR);
        results.column_types[0] = DataType::INT32;  // oid
        results.column_types[2] = DataType::INT32;  // datdba
        results.column_types[3] = DataType::INT32;  // encoding
        results.column_types[7] = DataType::INT32;  // datconnlimit
        results.column_types[8] = DataType::INT32;  // datfrozenxid
        results.column_types[9] = DataType::INT32;  // datminmxid
        results.column_types[10] = DataType::INT32; // dattablespace

        // ScratchBird has a single database (the open database)
        VirtualRow row;
        row.columns = {
            {"oid", TypedValue::makeInt32(1)},
            {"datname", TypedValue::makeString("scratchbird")},
            {"datdba", TypedValue::makeInt32(10)},  // postgres superuser OID
            {"encoding", TypedValue::makeInt32(6)},  // UTF8
            {"datlocprovider", TypedValue::makeString("c")},
            {"datistemplate", TypedValue::makeString("f")},
            {"datallowconn", TypedValue::makeString("t")},
            {"datconnlimit", TypedValue::makeInt32(-1)},
            {"datfrozenxid", TypedValue::makeInt32(0)},
            {"datminmxid", TypedValue::makeInt32(1)},
            {"dattablespace", TypedValue::makeInt32(1663)},  // pg_default
            {"datcollate", TypedValue::makeString("en_US.UTF-8")},
            {"datctype", TypedValue::makeString("en_US.UTF-8")},
            {"daticulocale", TypedValue::makeNull()},
            {"daticurules", TypedValue::makeNull()},
            {"datcollversion", TypedValue::makeNull()},
            {"datacl", TypedValue::makeNull()}
        };
        results.rows.push_back(std::move(row));

        return Status::OK;
    }

    /**
     * pg_tablespace - tablespaces
     */
    Status queryPgTablespace(const std::string& where_clause,
                             VirtualResultSet& results,
                             ErrorContext* ctx = nullptr) {
        results.column_names = {
            "oid", "spcname", "spcowner", "spcacl", "spcoptions"
        };

        results.column_types = {
            DataType::INT32, DataType::VARCHAR, DataType::INT32,
            DataType::VARCHAR, DataType::VARCHAR
        };

        std::vector<CatalogManager::TablespaceInfo> tablespaces;
        Status s = catalog_manager_->listTablespaces(tablespaces, ctx);
        if (!s.ok()) return s;

        for (const auto& ts : tablespaces) {
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt32(idToOid(ts.tablespace_id))},
                {"spcname", TypedValue::makeString(ts.tablespace_name)},
                {"spcowner", TypedValue::makeInt32(idToOid(ts.owner_id))},
                {"spcacl", TypedValue::makeNull()},
                {"spcoptions", TypedValue::makeNull()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    // ========================================================================
    // Column Definition Helpers
    // ========================================================================

    Status getPgNamespaceColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("nspname", DataType::VARCHAR, 2),
            makeColumnInfo("nspowner", DataType::INT32, 3),
            makeColumnInfo("nspacl", DataType::VARCHAR, 4)
        };
        return Status::OK;
    }

    Status getPgClassColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("relname", DataType::VARCHAR, 2),
            makeColumnInfo("relnamespace", DataType::INT32, 3),
            makeColumnInfo("relkind", DataType::CHAR, 4)
        };
        return Status::OK;
    }

    Status getPgAttributeColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("attrelid", DataType::INT32, 1),
            makeColumnInfo("attname", DataType::VARCHAR, 2),
            makeColumnInfo("atttypid", DataType::INT32, 3),
            makeColumnInfo("attnum", DataType::INT16, 4),
            makeColumnInfo("attnotnull", DataType::BOOLEAN, 5)
        };
        return Status::OK;
    }

    Status getPgTypeColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("typname", DataType::VARCHAR, 2),
            makeColumnInfo("typnamespace", DataType::INT32, 3),
            makeColumnInfo("typlen", DataType::INT16, 4)
        };
        return Status::OK;
    }

    Status getPgIndexColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("indexrelid", DataType::INT32, 1),
            makeColumnInfo("indrelid", DataType::INT32, 2),
            makeColumnInfo("indisunique", DataType::BOOLEAN, 3),
            makeColumnInfo("indisprimary", DataType::BOOLEAN, 4)
        };
        return Status::OK;
    }

    Status getPgConstraintColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("conname", DataType::VARCHAR, 2),
            makeColumnInfo("connamespace", DataType::INT32, 3),
            makeColumnInfo("contype", DataType::CHAR, 4),
            makeColumnInfo("conrelid", DataType::INT32, 5)
        };
        return Status::OK;
    }

    Status getPgProcColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("proname", DataType::VARCHAR, 2),
            makeColumnInfo("pronamespace", DataType::INT32, 3),
            makeColumnInfo("prokind", DataType::CHAR, 4)
        };
        return Status::OK;
    }

    Status getPgTriggerColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("tgrelid", DataType::INT32, 2),
            makeColumnInfo("tgname", DataType::VARCHAR, 3),
            makeColumnInfo("tgenabled", DataType::CHAR, 4)
        };
        return Status::OK;
    }

    Status getPgUserColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("usename", DataType::VARCHAR, 1),
            makeColumnInfo("usesysid", DataType::INT32, 2),
            makeColumnInfo("usesuper", DataType::BOOLEAN, 3)
        };
        return Status::OK;
    }

    Status getPgRolesColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("rolname", DataType::VARCHAR, 2),
            makeColumnInfo("rolsuper", DataType::BOOLEAN, 3)
        };
        return Status::OK;
    }

    Status getPgDatabaseColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("datname", DataType::VARCHAR, 2),
            makeColumnInfo("datdba", DataType::INT32, 3),
            makeColumnInfo("encoding", DataType::INT32, 4)
        };
        return Status::OK;
    }

    Status getPgTablespaceColumns(std::vector<CatalogManager::ColumnInfo>& columns) {
        columns = {
            makeColumnInfo("oid", DataType::INT32, 1),
            makeColumnInfo("spcname", DataType::VARCHAR, 2),
            makeColumnInfo("spcowner", DataType::INT32, 3)
        };
        return Status::OK;
    }

    // ========================================================================
    // Helper Functions
    // ========================================================================

    CatalogManager::ColumnInfo makeColumnInfo(const std::string& name,
                                               DataType type,
                                               int ordinal) {
        CatalogManager::ColumnInfo col;
        col.column_name = name;
        col.data_type = type;
        col.ordinal = ordinal;
        col.nullable = true;
        col.max_length = (type == DataType::VARCHAR) ? 256 : 0;
        return col;
    }

    static int16_t getTypeLength(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:   return 1;
            case DataType::INT16:     return 2;
            case DataType::INT32:     return 4;
            case DataType::INT64:     return 8;
            case DataType::FLOAT32:   return 4;
            case DataType::FLOAT64:   return 8;
            case DataType::DATE:      return 4;
            case DataType::TIME:      return 8;
            case DataType::TIMESTAMP: return 8;
            case DataType::UUID:      return 16;
            default:                  return -1;  // Variable length
        }
    }

    static int32_t getTypeMod(const CatalogManager::ColumnInfo& col) {
        if (col.data_type == DataType::VARCHAR || col.data_type == DataType::CHAR) {
            return col.max_length + 4;  // PostgreSQL adds 4 to varchar/char typemod
        }
        if (col.data_type == DataType::DECIMAL) {
            return ((col.precision << 16) | col.scale) + 4;
        }
        return -1;
    }

    static bool isPassByValue(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:
            case DataType::INT16:
            case DataType::INT32:
            case DataType::INT64:
            case DataType::FLOAT32:
            case DataType::FLOAT64:
                return true;
            default:
                return false;
        }
    }

    static const char* getTypeAlign(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:
            case DataType::CHAR:
                return "c";  // char alignment
            case DataType::INT16:
                return "s";  // short alignment
            case DataType::INT32:
            case DataType::FLOAT32:
            case DataType::DATE:
                return "i";  // int alignment
            case DataType::INT64:
            case DataType::FLOAT64:
            case DataType::TIME:
            case DataType::TIMESTAMP:
                return "d";  // double alignment
            default:
                return "i";
        }
    }

    static const char* getTypeStorage(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:
            case DataType::INT16:
            case DataType::INT32:
            case DataType::INT64:
            case DataType::FLOAT32:
            case DataType::FLOAT64:
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
            case DataType::UUID:
                return "p";  // plain (not compressible)
            case DataType::VARCHAR:
            case DataType::TEXT:
                return "x";  // extended (can be compressed/toasted)
            case DataType::BLOB:
                return "e";  // external (always toast)
            default:
                return "p";
        }
    }
};

} // namespace scratchbird::catalog
