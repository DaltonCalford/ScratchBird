#pragma once

/**
 * PostgreSQL pg_catalog Implementation
 *
 * Phase D: Catalog Cleanup - pg_catalog virtual catalog handler
 *
 * This handler exposes core pg_catalog tables by mapping ScratchBird metadata
 * into PostgreSQL-compatible shapes. It focuses on introspection paths used by
 * compatibility tests and tooling, with conservative defaults for fields that
 * are not yet modeled in ScratchBird.
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/uuidv7.h"
#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * PgCatalogHandler - PostgreSQL pg_catalog implementation
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

        const ColumnDefs* def = getTableDefinition(table_name);
        if (!def) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table definition not found: pg_catalog." + table_name).c_str());
            return Status::NOT_FOUND;
        }
        setResultColumns(*def, results);

        if (equalsCaseInsensitive(table_name, "pg_namespace")) {
            return queryPgNamespace(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_class")) {
            return queryPgClass(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_attribute")) {
            return queryPgAttribute(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_type")) {
            return queryPgType(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_enum")) {
            return queryPgEnum(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_proc")) {
            return queryPgProc(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_constraint")) {
            return queryPgConstraint(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_index")) {
            return queryPgIndex(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_roles")) {
            return queryPgRoles(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_authid")) {
            return queryPgAuthid(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_database")) {
            return queryPgDatabase(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_tablespace")) {
            return queryPgTablespace(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_stat_activity")) {
            return queryPgStatActivity(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_stat_user_tables")) {
            return queryPgStatUserTables(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_locks")) {
            return queryPgLocks(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_settings")) {
            return queryPgSettings(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_trigger")) {
            return queryPgTrigger(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "pg_inherits")) {
            return queryPgInherits(results, ctx);
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

        const ColumnDefs* def = getTableDefinition(table_name);
        if (!def) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table definition not found: pg_catalog." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        setColumnInfo(*def, columns);
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
    struct ColumnDef {
        const char* name;
        DataType type;
        bool nullable;
    };

    using ColumnDefs = std::vector<ColumnDef>;

    static constexpr int64_t kPgCatalogOid = 11;
    static constexpr int64_t kInformationSchemaOid = 16660;
    static constexpr int64_t kPgDefaultTablespaceOid = 1663;
    static constexpr int64_t kPgVoidOid = 2278;

    std::vector<std::string> table_names_;

    void initializeTableNames() {
        table_names_ = {
            "pg_namespace", "pg_class", "pg_attribute", "pg_type",
            "pg_enum", "pg_constraint", "pg_index", "pg_proc",
            "pg_trigger", "pg_authid", "pg_roles", "pg_database",
            "pg_tablespace", "pg_settings", "pg_locks", "pg_inherits",
            "pg_stat_user_tables", "pg_stat_activity"
        };
    }

    static bool isZeroId(const ID& id) {
        return std::all_of(id.bytes.begin(), id.bytes.end(), [](uint8_t b) { return b == 0; });
    }

    static int64_t oidFromUuid(const ID& id) {
        uint64_t hash = 1469598103934665603ULL;
        for (uint8_t b : id.bytes) {
            hash ^= b;
            hash *= 1099511628211ULL;
        }
        hash &= 0x3fffffffffffffffULL;
        hash |= 0x4000000000000000ULL;
        return static_cast<int64_t>(hash);
    }

    static std::string schemaName(const CatalogManager::SchemaInfo& schema) {
        return schema.full_path.empty() ? schema.schema_name : schema.full_path;
    }

    static int64_t pgBuiltinTypeOid(DataType type) {
        switch (type) {
            case DataType::BOOLEAN: return 16;   // bool
            case DataType::INT16: return 21;     // int2
            case DataType::INT32: return 23;     // int4
            case DataType::INT64: return 20;     // int8
            case DataType::INT8: return 21;      // int2
            case DataType::FLOAT32: return 700;  // float4
            case DataType::FLOAT64: return 701;  // float8
            case DataType::DECIMAL: return 1700; // numeric
            case DataType::MONEY: return 790;    // money
            case DataType::CHAR: return 1042;    // bpchar
            case DataType::VARCHAR: return 1043; // varchar
            case DataType::TEXT: return 25;      // text
            case DataType::DATE: return 1082;    // date
            case DataType::TIME: return 1083;    // time
            case DataType::TIMESTAMP: return 1114; // timestamp
            case DataType::UUID: return 2950;    // uuid
            case DataType::BYTEA: return 17;     // bytea
            case DataType::BINARY: return 17;
            case DataType::VARBINARY: return 17;
            case DataType::BLOB: return 17;
            case DataType::JSON: return 114;     // json
            case DataType::JSONB: return 3802;   // jsonb
            default: return 0;
        }
    }

    static const char* pgBuiltinTypeName(DataType type) {
        switch (type) {
            case DataType::BOOLEAN: return "bool";
            case DataType::INT16: return "int2";
            case DataType::INT32: return "int4";
            case DataType::INT64: return "int8";
            case DataType::INT8: return "int2";
            case DataType::FLOAT32: return "float4";
            case DataType::FLOAT64: return "float8";
            case DataType::DECIMAL: return "numeric";
            case DataType::MONEY: return "money";
            case DataType::CHAR: return "bpchar";
            case DataType::VARCHAR: return "varchar";
            case DataType::TEXT: return "text";
            case DataType::DATE: return "date";
            case DataType::TIME: return "time";
            case DataType::TIMESTAMP: return "timestamp";
            case DataType::UUID: return "uuid";
            case DataType::BYTEA: return "bytea";
            case DataType::BINARY: return "bytea";
            case DataType::VARBINARY: return "bytea";
            case DataType::BLOB: return "bytea";
            case DataType::JSON: return "json";
            case DataType::JSONB: return "jsonb";
            default: return "unknown";
        }
    }

    static int64_t pgTypeLen(DataType type) {
        switch (type) {
            case DataType::BOOLEAN: return 1;
            case DataType::INT16: return 2;
            case DataType::INT32: return 4;
            case DataType::INT64: return 8;
            case DataType::INT8: return 2;
            case DataType::FLOAT32: return 4;
            case DataType::FLOAT64: return 8;
            case DataType::DATE: return 4;
            case DataType::TIME: return 8;
            case DataType::TIMESTAMP: return 8;
            case DataType::UUID: return 16;
            default: return -1;
        }
    }

    static bool pgTypeByVal(DataType type) {
        switch (type) {
            case DataType::BOOLEAN:
            case DataType::INT16:
            case DataType::INT32:
            case DataType::INT64:
            case DataType::INT8:
            case DataType::FLOAT32:
            case DataType::FLOAT64:
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
                return true;
            default:
                return false;
        }
    }

    static char pgTypeCategory(DataType type) {
        switch (type) {
            case DataType::BOOLEAN: return 'B';
            case DataType::INT16:
            case DataType::INT32:
            case DataType::INT64:
            case DataType::INT8:
            case DataType::UINT8:
            case DataType::UINT16:
            case DataType::UINT32:
            case DataType::UINT64:
            case DataType::FLOAT32:
            case DataType::FLOAT64:
            case DataType::DECIMAL:
            case DataType::MONEY:
                return 'N';
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
                return 'S';
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
                return 'D';
            default:
                return 'U';
        }
    }

    static char pgRelKind(CatalogManager::TableType type, bool materialized) {
        switch (type) {
            case CatalogManager::TableType::INDEX: return 'i';
            case CatalogManager::TableType::TEMPORARY: return 'r';
            case CatalogManager::TableType::EXTERNAL: return 'f';
            case CatalogManager::TableType::MATERIALIZED_VIEW: return 'm';
            case CatalogManager::TableType::TOAST: return 't';
            case CatalogManager::TableType::HEAP: default:
                return materialized ? 'm' : 'r';
        }
    }

    static std::string pgRelPersistence(CatalogManager::TableType type) {
        switch (type) {
            case CatalogManager::TableType::TEMPORARY: return "t";
            default: return "p";
        }
    }

    static char pgConstraintType(CatalogManager::ConstraintType type) {
        switch (type) {
            case CatalogManager::ConstraintType::PRIMARY_KEY: return 'p';
            case CatalogManager::ConstraintType::UNIQUE: return 'u';
            case CatalogManager::ConstraintType::CHECK: return 'c';
            case CatalogManager::ConstraintType::FOREIGN_KEY: return 'f';
            case CatalogManager::ConstraintType::NOT_NULL: return 'n';
            case CatalogManager::ConstraintType::EXCLUSION: return 'x';
            default: return 'c';
        }
    }

    static const char* pgLockTypeName(LockTarget target) {
        switch (target) {
            case LockTarget::LOCK_TARGET_DATABASE: return "database";
            case LockTarget::LOCK_TARGET_TABLE: return "relation";
            case LockTarget::LOCK_TARGET_PAGE: return "page";
            case LockTarget::LOCK_TARGET_TUPLE: return "tuple";
            default: return "relation";
        }
    }

    static const char* pgLockModeName(LockMode mode) {
        switch (mode) {
            case LockMode::LOCK_ACCESS_SHARE: return "AccessShareLock";
            case LockMode::LOCK_ROW_SHARE: return "RowShareLock";
            case LockMode::LOCK_ROW_EXCLUSIVE: return "RowExclusiveLock";
            case LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE: return "ShareUpdateExclusiveLock";
            case LockMode::LOCK_SHARE: return "ShareLock";
            case LockMode::LOCK_SHARE_ROW_EXCLUSIVE: return "ShareRowExclusiveLock";
            case LockMode::LOCK_EXCLUSIVE: return "ExclusiveLock";
            case LockMode::LOCK_ACCESS_EXCLUSIVE: return "AccessExclusiveLock";
            default: return "AccessShareLock";
        }
    }

    static void setResultColumns(const ColumnDefs& cols, VirtualResultSet& results) {
        results.column_names.clear();
        results.column_types.clear();
        results.column_names.reserve(cols.size());
        results.column_types.reserve(cols.size());
        for (const auto& col : cols) {
            results.column_names.push_back(col.name);
            results.column_types.push_back(col.type);
        }
    }

    static void setColumnInfo(const ColumnDefs& cols,
                              std::vector<CatalogManager::ColumnInfo>& columns) {
        columns.clear();
        uint16_t ordinal = 1;
        for (const auto& col : cols) {
            CatalogManager::ColumnInfo info;
            info.column_name = col.name;
            info.data_type = static_cast<uint16_t>(col.type);
            info.nullable = col.nullable;
            info.ordinal = ordinal++;
            columns.push_back(std::move(info));
        }
    }

    const ColumnDefs* getTableDefinition(const std::string& table_name) const {
        static const ColumnDefs pg_namespace_cols = {
            {"oid", DataType::INT64, false},
            {"nspname", DataType::VARCHAR, false},
            {"nspowner", DataType::INT64, true},
            {"nspacl", DataType::TEXT, true}
        };
        static const ColumnDefs pg_class_cols = {
            {"oid", DataType::INT64, false},
            {"relname", DataType::VARCHAR, false},
            {"relnamespace", DataType::INT64, true},
            {"relkind", DataType::VARCHAR, true},
            {"relowner", DataType::INT64, true},
            {"reltablespace", DataType::INT64, true},
            {"reltuples", DataType::INT64, true},
            {"relpages", DataType::INT64, true},
            {"relnatts", DataType::INT64, true},
            {"relhasindex", DataType::BOOLEAN, true},
            {"relisshared", DataType::BOOLEAN, true},
            {"relpersistence", DataType::VARCHAR, true},
            {"reloptions", DataType::TEXT, true}
        };
        static const ColumnDefs pg_attribute_cols = {
            {"attrelid", DataType::INT64, false},
            {"attname", DataType::VARCHAR, false},
            {"atttypid", DataType::INT64, true},
            {"attnum", DataType::INT64, true},
            {"attnotnull", DataType::BOOLEAN, true},
            {"attisdropped", DataType::BOOLEAN, true},
            {"atttypmod", DataType::INT64, true}
        };
        static const ColumnDefs pg_type_cols = {
            {"oid", DataType::INT64, false},
            {"typname", DataType::VARCHAR, false},
            {"typnamespace", DataType::INT64, true},
            {"typowner", DataType::INT64, true},
            {"typlen", DataType::INT64, true},
            {"typbyval", DataType::BOOLEAN, true},
            {"typtype", DataType::VARCHAR, true},
            {"typcategory", DataType::VARCHAR, true},
            {"typrelid", DataType::INT64, true},
            {"typelem", DataType::INT64, true},
            {"typarray", DataType::INT64, true},
            {"typbasetype", DataType::INT64, true},
            {"typnotnull", DataType::BOOLEAN, true}
        };
        static const ColumnDefs pg_enum_cols = {
            {"enumtypid", DataType::INT64, false},
            {"enumsortorder", DataType::INT64, true},
            {"enumlabel", DataType::VARCHAR, false}
        };
        static const ColumnDefs pg_proc_cols = {
            {"oid", DataType::INT64, false},
            {"proname", DataType::VARCHAR, false},
            {"pronamespace", DataType::INT64, true},
            {"proowner", DataType::INT64, true},
            {"prorettype", DataType::INT64, true},
            {"prokind", DataType::VARCHAR, true},
            {"proargtypes", DataType::TEXT, true}
        };
        static const ColumnDefs pg_trigger_cols = {
            {"oid", DataType::INT64, false},
            {"tgname", DataType::VARCHAR, false},
            {"tgrelid", DataType::INT64, true},
            {"tgenabled", DataType::VARCHAR, true}
        };
        static const ColumnDefs pg_constraint_cols = {
            {"oid", DataType::INT64, false},
            {"conname", DataType::VARCHAR, false},
            {"connamespace", DataType::INT64, true},
            {"conrelid", DataType::INT64, true},
            {"contype", DataType::VARCHAR, true},
            {"condeferrable", DataType::BOOLEAN, true},
            {"condeferred", DataType::BOOLEAN, true},
            {"confrelid", DataType::INT64, true}
        };
        static const ColumnDefs pg_index_cols = {
            {"indexrelid", DataType::INT64, false},
            {"indrelid", DataType::INT64, false},
            {"indisunique", DataType::BOOLEAN, true},
            {"indisprimary", DataType::BOOLEAN, true},
            {"indisvalid", DataType::BOOLEAN, true},
            {"indkey", DataType::TEXT, true}
        };
        static const ColumnDefs pg_roles_cols = {
            {"oid", DataType::INT64, false},
            {"rolname", DataType::VARCHAR, false},
            {"rolsuper", DataType::BOOLEAN, true},
            {"rolcanlogin", DataType::BOOLEAN, true},
            {"rolcreaterole", DataType::BOOLEAN, true},
            {"rolcreatedb", DataType::BOOLEAN, true},
            {"rolreplication", DataType::BOOLEAN, true},
            {"rolbypassrls", DataType::BOOLEAN, true}
        };
        static const ColumnDefs pg_authid_cols = {
            {"oid", DataType::INT64, false},
            {"rolname", DataType::VARCHAR, false},
            {"rolsuper", DataType::BOOLEAN, true},
            {"rolcanlogin", DataType::BOOLEAN, true},
            {"rolcreaterole", DataType::BOOLEAN, true},
            {"rolcreatedb", DataType::BOOLEAN, true},
            {"rolreplication", DataType::BOOLEAN, true},
            {"rolbypassrls", DataType::BOOLEAN, true},
            {"rolpassword", DataType::TEXT, true}
        };
        static const ColumnDefs pg_database_cols = {
            {"oid", DataType::INT64, false},
            {"datname", DataType::VARCHAR, false},
            {"datdba", DataType::INT64, true},
            {"encoding", DataType::INT64, true}
        };
        static const ColumnDefs pg_tablespace_cols = {
            {"oid", DataType::INT64, false},
            {"spcname", DataType::VARCHAR, false},
            {"spcowner", DataType::INT64, true}
        };
        static const ColumnDefs pg_stat_activity_cols = {
            {"datid", DataType::INT64, true},
            {"datname", DataType::VARCHAR, true},
            {"pid", DataType::INT64, true},
            {"leader_pid", DataType::INT64, true},
            {"usesysid", DataType::INT64, true},
            {"usename", DataType::VARCHAR, true},
            {"application_name", DataType::VARCHAR, true},
            {"client_addr", DataType::VARCHAR, true},
            {"client_hostname", DataType::VARCHAR, true},
            {"client_port", DataType::INT64, true},
            {"backend_start", DataType::TIMESTAMP, true},
            {"xact_start", DataType::TIMESTAMP, true},
            {"query_start", DataType::TIMESTAMP, true},
            {"state_change", DataType::TIMESTAMP, true},
            {"wait_event_type", DataType::VARCHAR, true},
            {"wait_event", DataType::VARCHAR, true},
            {"state", DataType::VARCHAR, true},
            {"backend_xid", DataType::INT64, true},
            {"backend_xmin", DataType::INT64, true},
            {"query_id", DataType::INT64, true},
            {"query", DataType::TEXT, true},
            {"backend_type", DataType::VARCHAR, true}
        };
        static const ColumnDefs pg_stat_user_tables_cols = {
            {"relid", DataType::INT64, false},
            {"schemaname", DataType::VARCHAR, false},
            {"relname", DataType::VARCHAR, false},
            {"seq_scan", DataType::INT64, true},
            {"seq_tup_read", DataType::INT64, true},
            {"idx_scan", DataType::INT64, true},
            {"idx_tup_fetch", DataType::INT64, true},
            {"n_tup_ins", DataType::INT64, true},
            {"n_tup_upd", DataType::INT64, true},
            {"n_tup_del", DataType::INT64, true},
            {"n_live_tup", DataType::INT64, true},
            {"n_dead_tup", DataType::INT64, true}
        };
        static const ColumnDefs pg_locks_cols = {
            {"locktype", DataType::VARCHAR, true},
            {"database", DataType::INT64, true},
            {"relation", DataType::INT64, true},
            {"page", DataType::INT64, true},
            {"tuple", DataType::INT64, true},
            {"virtualxid", DataType::VARCHAR, true},
            {"transactionid", DataType::INT64, true},
            {"classid", DataType::INT64, true},
            {"objid", DataType::INT64, true},
            {"objsubid", DataType::INT64, true},
            {"pid", DataType::INT64, true},
            {"mode", DataType::VARCHAR, true},
            {"granted", DataType::BOOLEAN, true}
        };
        static const ColumnDefs pg_settings_cols = {
            {"name", DataType::VARCHAR, false},
            {"setting", DataType::VARCHAR, true}
        };
        static const ColumnDefs pg_inherits_cols = {
            {"inhrelid", DataType::INT64, false},
            {"inhparent", DataType::INT64, false},
            {"inhseqno", DataType::INT64, true}
        };

        if (equalsCaseInsensitive(table_name, "pg_namespace")) return &pg_namespace_cols;
        if (equalsCaseInsensitive(table_name, "pg_class")) return &pg_class_cols;
        if (equalsCaseInsensitive(table_name, "pg_attribute")) return &pg_attribute_cols;
        if (equalsCaseInsensitive(table_name, "pg_type")) return &pg_type_cols;
        if (equalsCaseInsensitive(table_name, "pg_enum")) return &pg_enum_cols;
        if (equalsCaseInsensitive(table_name, "pg_proc")) return &pg_proc_cols;
        if (equalsCaseInsensitive(table_name, "pg_trigger")) return &pg_trigger_cols;
        if (equalsCaseInsensitive(table_name, "pg_constraint")) return &pg_constraint_cols;
        if (equalsCaseInsensitive(table_name, "pg_index")) return &pg_index_cols;
        if (equalsCaseInsensitive(table_name, "pg_roles")) return &pg_roles_cols;
        if (equalsCaseInsensitive(table_name, "pg_authid")) return &pg_authid_cols;
        if (equalsCaseInsensitive(table_name, "pg_database")) return &pg_database_cols;
        if (equalsCaseInsensitive(table_name, "pg_tablespace")) return &pg_tablespace_cols;
        if (equalsCaseInsensitive(table_name, "pg_stat_activity")) return &pg_stat_activity_cols;
        if (equalsCaseInsensitive(table_name, "pg_stat_user_tables")) return &pg_stat_user_tables_cols;
        if (equalsCaseInsensitive(table_name, "pg_locks")) return &pg_locks_cols;
        if (equalsCaseInsensitive(table_name, "pg_settings")) return &pg_settings_cols;
        if (equalsCaseInsensitive(table_name, "pg_inherits")) return &pg_inherits_cols;

        return nullptr;
    }

    Status queryPgNamespace(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        bool has_pg_catalog = false;
        bool has_info_schema = false;

        for (const auto& schema : schemas) {
            std::string name = schemaName(schema);
            if (equalsCaseInsensitive(name, "pg_catalog")) {
                has_pg_catalog = true;
            }
            if (equalsCaseInsensitive(name, "information_schema")) {
                has_info_schema = true;
            }

            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt64(oidFromUuid(schema.schema_id))},
                {"nspname", TypedValue::makeVarchar(name)},
                {"nspowner", isZeroId(schema.owner_id) ? TypedValue() : TypedValue::makeInt64(oidFromUuid(schema.owner_id))},
                {"nspacl", TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        if (!has_pg_catalog) {
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt64(kPgCatalogOid)},
                {"nspname", TypedValue::makeVarchar("pg_catalog")},
                {"nspowner", TypedValue()},
                {"nspacl", TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        if (!has_info_schema) {
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt64(kInformationSchemaOid)},
                {"nspname", TypedValue::makeVarchar("information_schema")},
                {"nspowner", TypedValue()},
                {"nspacl", TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryPgClass(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::unordered_map<uint16_t, int64_t> tablespace_oid_by_id;
        std::vector<TablespaceInfo> tablespaces;
        Status ts_status = catalog_manager_->listTablespaces(tablespaces, ctx);
        if (ts_status == Status::OK) {
            for (const auto& ts : tablespaces) {
                tablespace_oid_by_id[ts.tablespace_id] = oidFromUuid(ts.tablespace_uuid);
            }
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            int64_t schema_oid = oidFromUuid(schema.schema_id);

            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status == Status::OK) {
                for (const auto& table : tables) {
                    std::vector<CatalogManager::IndexInfo> indexes;
                    catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx);

                    int64_t reltablespace = 0;
                    auto ts_it = tablespace_oid_by_id.find(table.tablespace_id);
                    if (ts_it != tablespace_oid_by_id.end()) {
                        reltablespace = ts_it->second;
                    }

                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt64(oidFromUuid(table.table_id))},
                        {"relname", TypedValue::makeVarchar(table.table_name)},
                        {"relnamespace", TypedValue::makeInt64(schema_oid)},
                        {"relkind", TypedValue::makeVarchar(std::string(1, pgRelKind(table.table_type, false)))},
                        {"relowner", isZeroId(table.owner_id) ? TypedValue() : TypedValue::makeInt64(oidFromUuid(table.owner_id))},
                        {"reltablespace", reltablespace == 0 ? TypedValue() : TypedValue::makeInt64(reltablespace)},
                        {"reltuples", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
                        {"relpages", TypedValue::makeInt64(0)},
                        {"relnatts", TypedValue::makeInt64(static_cast<int64_t>(table.column_count))},
                        {"relhasindex", TypedValue::makeBool(!indexes.empty())},
                        {"relisshared", TypedValue::makeBool(false)},
                        {"relpersistence", TypedValue::makeVarchar(pgRelPersistence(table.table_type))},
                        {"reloptions", TypedValue()}
                    };
                    results.rows.push_back(std::move(row));

                    for (const auto& index : indexes) {
                        int64_t index_ts = 0;
                        auto idx_ts_it = tablespace_oid_by_id.find(index.tablespace_id);
                        if (idx_ts_it != tablespace_oid_by_id.end()) {
                            index_ts = idx_ts_it->second;
                        }

                        VirtualRow idx_row;
                        idx_row.columns = {
                            {"oid", TypedValue::makeInt64(oidFromUuid(index.index_id))},
                            {"relname", TypedValue::makeVarchar(index.index_name)},
                            {"relnamespace", TypedValue::makeInt64(schema_oid)},
                            {"relkind", TypedValue::makeVarchar("i")},
                            {"relowner", isZeroId(index.owner_id) ? TypedValue() : TypedValue::makeInt64(oidFromUuid(index.owner_id))},
                            {"reltablespace", index_ts == 0 ? TypedValue() : TypedValue::makeInt64(index_ts)},
                            {"reltuples", TypedValue::makeInt64(0)},
                            {"relpages", TypedValue::makeInt64(0)},
                            {"relnatts", TypedValue::makeInt64(static_cast<int64_t>(index.column_ids.size()))},
                            {"relhasindex", TypedValue::makeBool(false)},
                            {"relisshared", TypedValue::makeBool(false)},
                            {"relpersistence", TypedValue::makeVarchar("p")},
                            {"reloptions", TypedValue()}
                        };
                        results.rows.push_back(std::move(idx_row));
                    }
                }
            }

            std::vector<CatalogManager::ViewInfo> views;
            status = catalog_manager_->listViewsForSchema(schema.schema_id, views, ctx);
            if (status == Status::OK) {
                for (const auto& view : views) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt64(oidFromUuid(view.view_id))},
                        {"relname", TypedValue::makeVarchar(view.name)},
                        {"relnamespace", TypedValue::makeInt64(schema_oid)},
                        {"relkind", TypedValue::makeVarchar(std::string(1, view.materialized ? 'm' : 'v'))},
                        {"relowner", isZeroId(view.owner_id) ? TypedValue() : TypedValue::makeInt64(oidFromUuid(view.owner_id))},
                        {"reltablespace", TypedValue()},
                        {"reltuples", TypedValue::makeInt64(0)},
                        {"relpages", TypedValue::makeInt64(0)},
                        {"relnatts", TypedValue::makeInt64(static_cast<int64_t>(view.column_names.size()))},
                        {"relhasindex", TypedValue::makeBool(false)},
                        {"relisshared", TypedValue::makeBool(false)},
                        {"relpersistence", TypedValue::makeVarchar("p")},
                        {"reloptions", TypedValue()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }

            std::vector<CatalogManager::SequenceInfo> sequences;
            status = catalog_manager_->listSequences(schema.schema_id, sequences, ctx);
            if (status == Status::OK) {
                for (const auto& seq : sequences) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt64(oidFromUuid(seq.sequence_id))},
                        {"relname", TypedValue::makeVarchar(seq.name)},
                        {"relnamespace", TypedValue::makeInt64(schema_oid)},
                        {"relkind", TypedValue::makeVarchar("S")},
                        {"relowner", isZeroId(seq.owner_id) ? TypedValue() : TypedValue::makeInt64(oidFromUuid(seq.owner_id))},
                        {"reltablespace", TypedValue()},
                        {"reltuples", TypedValue::makeInt64(0)},
                        {"relpages", TypedValue::makeInt64(0)},
                        {"relnatts", TypedValue::makeInt64(0)},
                        {"relhasindex", TypedValue::makeBool(false)},
                        {"relisshared", TypedValue::makeBool(false)},
                        {"relpersistence", TypedValue::makeVarchar("p")},
                        {"reloptions", TypedValue()}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        ID zero{};
        std::vector<DomainInfo> domains;
        Status domain_status = catalog_manager_->listDomains(zero, domains, ctx);
        if (domain_status == Status::OK) {
            for (const auto& domain : domains) {
                if (domain.domain_type != DomainType::RECORD) {
                    continue;
                }
                int64_t schema_oid = oidFromUuid(domain.schema_id);
                VirtualRow row;
                row.columns = {
                    {"oid", TypedValue::makeInt64(oidFromUuid(domain.domain_id))},
                    {"relname", TypedValue::makeVarchar(domain.domain_name)},
                    {"relnamespace", TypedValue::makeInt64(schema_oid)},
                    {"relkind", TypedValue::makeVarchar("c")},
                    {"relowner", TypedValue()},
                    {"reltablespace", TypedValue()},
                    {"reltuples", TypedValue::makeInt64(0)},
                    {"relpages", TypedValue::makeInt64(0)},
                    {"relnatts", TypedValue::makeInt64(static_cast<int64_t>(domain.fields.size()))},
                    {"relhasindex", TypedValue::makeBool(false)},
                    {"relisshared", TypedValue::makeBool(false)},
                    {"relpersistence", TypedValue::makeVarchar("p")},
                    {"reloptions", TypedValue()}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryPgAttribute(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        ID zero{};
        std::vector<DomainInfo> domains;
        std::unordered_map<ID, DomainInfo, IDHash> domain_map;
        if (catalog_manager_->listDomains(zero, domains, ctx) == Status::OK) {
            for (const auto& domain : domains) {
                domain_map.emplace(domain.domain_id, domain);
            }
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::ColumnInfo> columns;
                status = catalog_manager_->getColumns(table.table_id, columns, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& column : columns) {
                    int64_t typ_oid = 0;
                    if (!isZeroId(column.domain_id)) {
                        auto it = domain_map.find(column.domain_id);
                        if (it != domain_map.end()) {
                            typ_oid = oidFromUuid(it->second.domain_id);
                        }
                    }
                    if (typ_oid == 0) {
                        typ_oid = pgBuiltinTypeOid(static_cast<DataType>(column.data_type));
                    }

                    VirtualRow row;
                    row.columns = {
                        {"attrelid", TypedValue::makeInt64(oidFromUuid(table.table_id))},
                        {"attname", TypedValue::makeVarchar(column.column_name)},
                        {"atttypid", TypedValue::makeInt64(typ_oid)},
                        {"attnum", TypedValue::makeInt64(static_cast<int64_t>(column.ordinal))},
                        {"attnotnull", TypedValue::makeBool(!column.nullable)},
                        {"attisdropped", TypedValue::makeBool(false)},
                        {"atttypmod", TypedValue::makeInt64(-1)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        for (const auto& domain : domains) {
            if (domain.domain_type != DomainType::RECORD) {
                continue;
            }

            int64_t rel_oid = oidFromUuid(domain.domain_id);
            int64_t position = 1;
            for (const auto& field : domain.fields) {
                int64_t typ_oid = 0;
                if (!isZeroId(field.domain_id)) {
                    auto it = domain_map.find(field.domain_id);
                    if (it != domain_map.end()) {
                        typ_oid = oidFromUuid(it->second.domain_id);
                    }
                }
                if (typ_oid == 0) {
                    typ_oid = pgBuiltinTypeOid(field.type);
                }

                VirtualRow row;
                row.columns = {
                    {"attrelid", TypedValue::makeInt64(rel_oid)},
                    {"attname", TypedValue::makeVarchar(field.name)},
                    {"atttypid", TypedValue::makeInt64(typ_oid)},
                    {"attnum", TypedValue::makeInt64(position++)},
                    {"attnotnull", TypedValue::makeBool(!field.nullable)},
                    {"attisdropped", TypedValue::makeBool(false)},
                    {"atttypmod", TypedValue::makeInt64(-1)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryPgType(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        const DataType builtin_types[] = {
            DataType::BOOLEAN, DataType::INT16, DataType::INT32, DataType::INT64,
            DataType::INT8, DataType::FLOAT32, DataType::FLOAT64, DataType::DECIMAL,
            DataType::MONEY, DataType::CHAR, DataType::VARCHAR, DataType::TEXT,
            DataType::DATE, DataType::TIME, DataType::TIMESTAMP, DataType::UUID,
            DataType::BYTEA, DataType::JSON, DataType::JSONB
        };

        for (DataType type : builtin_types) {
            int64_t oid = pgBuiltinTypeOid(type);
            if (oid == 0) {
                continue;
            }
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt64(oid)},
                {"typname", TypedValue::makeVarchar(pgBuiltinTypeName(type))},
                {"typnamespace", TypedValue::makeInt64(kPgCatalogOid)},
                {"typowner", TypedValue()},
                {"typlen", TypedValue::makeInt64(pgTypeLen(type))},
                {"typbyval", TypedValue::makeBool(pgTypeByVal(type))},
                {"typtype", TypedValue::makeVarchar("b")},
                {"typcategory", TypedValue::makeVarchar(std::string(1, pgTypeCategory(type)))},
                {"typrelid", TypedValue::makeInt64(0)},
                {"typelem", TypedValue::makeInt64(0)},
                {"typarray", TypedValue::makeInt64(0)},
                {"typbasetype", TypedValue::makeInt64(0)},
                {"typnotnull", TypedValue::makeBool(false)}
            };
            results.rows.push_back(std::move(row));
        }

        ID zero{};
        std::vector<DomainInfo> domains;
        Status status = catalog_manager_->listDomains(zero, domains, ctx);
        if (status != Status::OK) {
            return Status::OK;
        }

        for (const auto& domain : domains) {
            char typtype = 'd';
            char typcategory = 'U';
            int64_t typbasetype = 0;
            int64_t typrelid = 0;

            switch (domain.domain_type) {
                case DomainType::BASIC:
                    typtype = 'd';
                    typcategory = pgTypeCategory(domain.base_type);
                    typbasetype = pgBuiltinTypeOid(domain.base_type);
                    break;
                case DomainType::ENUM:
                    typtype = 'e';
                    typcategory = 'E';
                    break;
                case DomainType::RECORD:
                    typtype = 'c';
                    typcategory = 'C';
                    typrelid = oidFromUuid(domain.domain_id);
                    break;
                case DomainType::SET:
                case DomainType::VARIANT:
                default:
                    typtype = 'p';
                    typcategory = 'U';
                    break;
            }

            DataType base_type = domain.base_type;
            int64_t type_len = (domain.domain_type == DomainType::BASIC)
                ? pgTypeLen(base_type)
                : -1;
            bool type_byval = (domain.domain_type == DomainType::BASIC)
                ? pgTypeByVal(base_type)
                : false;

            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt64(oidFromUuid(domain.domain_id))},
                {"typname", TypedValue::makeVarchar(domain.domain_name)},
                {"typnamespace", TypedValue::makeInt64(oidFromUuid(domain.schema_id))},
                {"typowner", TypedValue()},
                {"typlen", TypedValue::makeInt64(type_len)},
                {"typbyval", TypedValue::makeBool(type_byval)},
                {"typtype", TypedValue::makeVarchar(std::string(1, typtype))},
                {"typcategory", TypedValue::makeVarchar(std::string(1, typcategory))},
                {"typrelid", TypedValue::makeInt64(typrelid)},
                {"typelem", TypedValue::makeInt64(0)},
                {"typarray", TypedValue::makeInt64(0)},
                {"typbasetype", TypedValue::makeInt64(typbasetype)},
                {"typnotnull", TypedValue::makeBool(!domain.nullable)}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryPgEnum(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        ID zero{};
        std::vector<DomainInfo> domains;
        Status status = catalog_manager_->listDomains(zero, domains, ctx);
        if (status != Status::OK) {
            return Status::OK;
        }

        for (const auto& domain : domains) {
            if (domain.domain_type != DomainType::ENUM) {
                continue;
            }

            int64_t typ_oid = oidFromUuid(domain.domain_id);
            for (const auto& value : domain.enum_values) {
                VirtualRow row;
                row.columns = {
                    {"enumtypid", TypedValue::makeInt64(typ_oid)},
                    {"enumsortorder", TypedValue::makeInt64(static_cast<int64_t>(value.position))},
                    {"enumlabel", TypedValue::makeVarchar(value.label)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    static std::string formatArgTypes(const std::vector<CatalogManager::ParameterInfo>& params) {
        std::string result;
        bool first = true;
        for (const auto& param : params) {
            if (param.mode == CatalogManager::ParameterMode::OUT) {
                continue;
            }
            int64_t oid = pgBuiltinTypeOid(param.type);
            if (!first) {
                result.push_back(' ');
            }
            result += std::to_string(oid);
            first = false;
        }
        return result;
    }

    Status queryPgProc(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::FunctionInfo> functions;
        Status status = catalog_manager_->listFunctions(functions, ctx);
        if (status == Status::OK) {
            for (const auto& func : functions) {
                int64_t ret_oid = pgBuiltinTypeOid(func.return_type);
                if (ret_oid == 0) {
                    ret_oid = kPgVoidOid;
                }

                VirtualRow row;
                row.columns = {
                    {"oid", TypedValue::makeInt64(oidFromUuid(func.function_id))},
                    {"proname", TypedValue::makeVarchar(func.name)},
                    {"pronamespace", TypedValue::makeInt64(oidFromUuid(func.schema_id))},
                    {"proowner", isZeroId(func.owner_id) ? TypedValue() : TypedValue::makeInt64(oidFromUuid(func.owner_id))},
                    {"prorettype", TypedValue::makeInt64(ret_oid)},
                    {"prokind", TypedValue::makeVarchar("f")},
                    {"proargtypes", TypedValue::makeText(formatArgTypes(func.parameters))}
                };
                results.rows.push_back(std::move(row));
            }
        }

        std::vector<CatalogManager::ProcedureInfo> procedures;
        status = catalog_manager_->listProcedures(procedures, ctx);
        if (status == Status::OK) {
            for (const auto& proc : procedures) {
                VirtualRow row;
                row.columns = {
                    {"oid", TypedValue::makeInt64(oidFromUuid(proc.procedure_id))},
                    {"proname", TypedValue::makeVarchar(proc.name)},
                    {"pronamespace", TypedValue::makeInt64(oidFromUuid(proc.schema_id))},
                    {"proowner", isZeroId(proc.owner_id) ? TypedValue() : TypedValue::makeInt64(oidFromUuid(proc.owner_id))},
                    {"prorettype", TypedValue::makeInt64(kPgVoidOid)},
                    {"prokind", TypedValue::makeVarchar("p")},
                    {"proargtypes", TypedValue::makeText(formatArgTypes(proc.parameters))}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryPgConstraint(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            int64_t schema_oid = oidFromUuid(schema.schema_id);
            for (const auto& table : tables) {
                std::vector<CatalogManager::ConstraintInfo> constraints;
                status = catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx);
                if (status != Status::OK) {
                    continue;
                }

                for (const auto& constraint : constraints) {
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt64(oidFromUuid(constraint.constraint_id))},
                        {"conname", TypedValue::makeVarchar(constraint.constraint_name)},
                        {"connamespace", TypedValue::makeInt64(schema_oid)},
                        {"conrelid", TypedValue::makeInt64(oidFromUuid(table.table_id))},
                        {"contype", TypedValue::makeVarchar(std::string(1, pgConstraintType(constraint.constraint_type)))},
                        {"condeferrable", TypedValue::makeBool(constraint.is_deferrable)},
                        {"condeferred", TypedValue::makeBool(constraint.initially_deferred)},
                        {"confrelid", TypedValue::makeInt64(0)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    Status queryPgIndex(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                std::vector<CatalogManager::IndexInfo> indexes;
                status = catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx);
                if (status != Status::OK) {
                    continue;
                }

                std::unordered_map<ID, uint16_t, IDHash> column_ordinals;
                std::vector<CatalogManager::ColumnInfo> columns;
                if (catalog_manager_->getColumns(table.table_id, columns, ctx) == Status::OK) {
                    for (const auto& col : columns) {
                        column_ordinals[col.column_id] = col.ordinal;
                    }
                }

                for (const auto& index : indexes) {
                    std::string indkey;
                    bool first = true;
                    for (const auto& col_id : index.column_ids) {
                        auto it = column_ordinals.find(col_id);
                        if (it == column_ordinals.end()) {
                            continue;
                        }
                        if (!first) {
                            indkey.push_back(' ');
                        }
                        indkey += std::to_string(it->second);
                        first = false;
                    }

                    VirtualRow row;
                    row.columns = {
                        {"indexrelid", TypedValue::makeInt64(oidFromUuid(index.index_id))},
                        {"indrelid", TypedValue::makeInt64(oidFromUuid(table.table_id))},
                        {"indisunique", TypedValue::makeBool(index.is_unique)},
                        {"indisprimary", TypedValue::makeBool(false)},
                        {"indisvalid", TypedValue::makeBool(index.state == static_cast<uint8_t>(CatalogManager::IndexState::ACTIVE))},
                        {"indkey", indkey.empty() ? TypedValue() : TypedValue::makeText(indkey)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        return Status::OK;
    }

    Status queryPgRoles(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::UserInfo> users;
        if (catalog_manager_->listUsers(users, ctx) == Status::OK) {
            for (const auto& user : users) {
                VirtualRow row;
                row.columns = {
                    {"oid", TypedValue::makeInt64(oidFromUuid(user.user_id))},
                    {"rolname", TypedValue::makeVarchar(user.username)},
                    {"rolsuper", TypedValue::makeBool(user.is_superuser)},
                    {"rolcanlogin", TypedValue::makeBool(true)},
                    {"rolcreaterole", TypedValue::makeBool(false)},
                    {"rolcreatedb", TypedValue::makeBool(false)},
                    {"rolreplication", TypedValue::makeBool(false)},
                    {"rolbypassrls", TypedValue::makeBool(user.is_superuser)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        std::vector<CatalogManager::RoleInfo> roles;
        if (catalog_manager_->listRoles(roles, ctx) == Status::OK) {
            for (const auto& role : roles) {
                VirtualRow row;
                row.columns = {
                    {"oid", TypedValue::makeInt64(oidFromUuid(role.role_id))},
                    {"rolname", TypedValue::makeVarchar(role.role_name)},
                    {"rolsuper", TypedValue::makeBool(false)},
                    {"rolcanlogin", TypedValue::makeBool(false)},
                    {"rolcreaterole", TypedValue::makeBool(false)},
                    {"rolcreatedb", TypedValue::makeBool(false)},
                    {"rolreplication", TypedValue::makeBool(false)},
                    {"rolbypassrls", TypedValue::makeBool(false)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryPgAuthid(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::UserInfo> users;
        if (catalog_manager_->listUsers(users, ctx) == Status::OK) {
            for (const auto& user : users) {
                VirtualRow row;
                row.columns = {
                    {"oid", TypedValue::makeInt64(oidFromUuid(user.user_id))},
                    {"rolname", TypedValue::makeVarchar(user.username)},
                    {"rolsuper", TypedValue::makeBool(user.is_superuser)},
                    {"rolcanlogin", TypedValue::makeBool(true)},
                    {"rolcreaterole", TypedValue::makeBool(false)},
                    {"rolcreatedb", TypedValue::makeBool(false)},
                    {"rolreplication", TypedValue::makeBool(false)},
                    {"rolbypassrls", TypedValue::makeBool(user.is_superuser)},
                    {"rolpassword", user.password_hash.empty() ? TypedValue() : TypedValue::makeText(user.password_hash)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        std::vector<CatalogManager::RoleInfo> roles;
        if (catalog_manager_->listRoles(roles, ctx) == Status::OK) {
            for (const auto& role : roles) {
                VirtualRow row;
                row.columns = {
                    {"oid", TypedValue::makeInt64(oidFromUuid(role.role_id))},
                    {"rolname", TypedValue::makeVarchar(role.role_name)},
                    {"rolsuper", TypedValue::makeBool(false)},
                    {"rolcanlogin", TypedValue::makeBool(false)},
                    {"rolcreaterole", TypedValue::makeBool(false)},
                    {"rolcreatedb", TypedValue::makeBool(false)},
                    {"rolreplication", TypedValue::makeBool(false)},
                    {"rolbypassrls", TypedValue::makeBool(false)},
                    {"rolpassword", TypedValue()}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryPgDatabase(VirtualResultSet& results, ErrorContext* /* ctx */) {
        VirtualRow row;
        row.columns = {
            {"oid", TypedValue::makeInt64(1)},
            {"datname", TypedValue::makeVarchar("scratchbird")},
            {"datdba", TypedValue::makeInt64(0)},
            {"encoding", TypedValue::makeInt64(6)}
        };
        results.rows.push_back(std::move(row));
        return Status::OK;
    }

    Status queryPgTablespace(VirtualResultSet& results, ErrorContext* ctx) {
        bool has_default = false;
        if (catalog_manager_) {
            std::vector<TablespaceInfo> tablespaces;
            Status status = catalog_manager_->listTablespaces(tablespaces, ctx);
            if (status == Status::OK) {
                for (const auto& ts : tablespaces) {
                    if (equalsCaseInsensitive(ts.tablespace_name, "pg_default")) {
                        has_default = true;
                    }
                    VirtualRow row;
                    row.columns = {
                        {"oid", TypedValue::makeInt64(oidFromUuid(ts.tablespace_uuid))},
                        {"spcname", TypedValue::makeVarchar(ts.tablespace_name)},
                        {"spcowner", TypedValue::makeInt64(0)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }

        if (!has_default) {
            VirtualRow row;
            row.columns = {
                {"oid", TypedValue::makeInt64(kPgDefaultTablespaceOid)},
                {"spcname", TypedValue::makeVarchar("pg_default")},
                {"spcowner", TypedValue::makeInt64(0)}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryPgStatActivity(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<ProcessControlBlock> backends;
        core::ErrorContext proc_ctx;
        if (ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) != Status::OK) {
            return Status::OK;
        }

        std::unordered_map<ID, CatalogManager::SessionInfo, IDHash> sessions_by_id;
        std::vector<CatalogManager::SessionInfo> sessions;
        if (catalog_manager_->listSessions(sessions, ctx) == Status::OK) {
            for (const auto& session : sessions) {
                sessions_by_id[session.session_id] = session;
            }
        }

        auto toTimestamp = [](uint64_t micros) -> TypedValue {
            return micros == 0 ? TypedValue() : TypedValue::makeTimestamp(static_cast<int64_t>(micros));
        };

        for (const auto& backend : backends) {
            const CatalogManager::SessionInfo* session = nullptr;
            auto it = sessions_by_id.find(backend.session_id);
            if (it != sessions_by_id.end()) {
                session = &it->second;
            }

            std::string state;
            if (backend.query_start_time != 0) {
                state = "active";
            } else if (backend.xid != 0) {
                state = "idle in transaction";
            } else {
                state = "idle";
            }

            std::string query_text;
            if (backend.query_text[0] != '\0') {
                query_text = backend.query_text;
            }

            TypedValue usesysid;
            TypedValue usename;
            if (session) {
                usesysid = TypedValue::makeInt64(oidFromUuid(session->user_id));
                usename = TypedValue::makeVarchar(session->username);
            }

            VirtualRow row;
            row.columns = {
                {"datid", TypedValue::makeInt64(1)},
                {"datname", TypedValue::makeVarchar("scratchbird")},
                {"pid", backend.backend_pid == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(backend.backend_pid))},
                {"leader_pid", TypedValue()},
                {"usesysid", usesysid},
                {"usename", usename},
                {"application_name", TypedValue()},
                {"client_addr", TypedValue()},
                {"client_hostname", TypedValue()},
                {"client_port", TypedValue()},
                {"backend_start", toTimestamp(backend.start_time)},
                {"xact_start", toTimestamp(backend.xact_start_time)},
                {"query_start", toTimestamp(backend.query_start_time)},
                {"state_change", toTimestamp(backend.state_change_time == 0 ? backend.start_time : backend.state_change_time)},
                {"wait_event_type", TypedValue()},
                {"wait_event", TypedValue()},
                {"state", TypedValue::makeVarchar(state)},
                {"backend_xid", backend.xid == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
                {"backend_xmin", backend.backend_xmin == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(backend.backend_xmin))},
                {"query_id", TypedValue()},
                {"query", query_text.empty() ? TypedValue() : TypedValue::makeText(query_text)},
                {"backend_type", TypedValue::makeVarchar("client backend")}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryPgStatUserTables(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::string schema_name = schemaName(schema);
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                VirtualRow row;
                row.columns = {
                    {"relid", TypedValue::makeInt64(oidFromUuid(table.table_id))},
                    {"schemaname", TypedValue::makeVarchar(schema_name)},
                    {"relname", TypedValue::makeVarchar(table.table_name)},
                    {"seq_scan", TypedValue::makeInt64(0)},
                    {"seq_tup_read", TypedValue::makeInt64(0)},
                    {"idx_scan", TypedValue::makeInt64(0)},
                    {"idx_tup_fetch", TypedValue::makeInt64(0)},
                    {"n_tup_ins", TypedValue::makeInt64(0)},
                    {"n_tup_upd", TypedValue::makeInt64(0)},
                    {"n_tup_del", TypedValue::makeInt64(0)},
                    {"n_live_tup", TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
                    {"n_dead_tup", TypedValue::makeInt64(0)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryPgLocks(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<LockSnapshot> locks;
        if (catalog_manager_->listLocks(locks, ctx) != Status::OK) {
            return Status::OK;
        }

        std::unordered_map<uint32_t, int64_t> pid_by_proc;
        std::vector<ProcessControlBlock> backends;
        core::ErrorContext proc_ctx;
        if (ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) == Status::OK) {
            for (const auto& backend : backends) {
                pid_by_proc[backend.proc_id] = static_cast<int64_t>(backend.backend_pid);
            }
        }

        for (const auto& lock : locks) {
            const auto& tag = lock.tag;
            bool has_relation = tag.target_type == LockTarget::LOCK_TARGET_TABLE ||
                                tag.target_type == LockTarget::LOCK_TARGET_PAGE ||
                                tag.target_type == LockTarget::LOCK_TARGET_TUPLE;
            bool has_page = tag.target_type == LockTarget::LOCK_TARGET_PAGE ||
                            tag.target_type == LockTarget::LOCK_TARGET_TUPLE;
            bool has_tuple = tag.target_type == LockTarget::LOCK_TARGET_TUPLE;

            TypedValue relation_val;
            if (has_relation && !isZeroId(tag.object_uuid)) {
                relation_val = TypedValue::makeInt64(oidFromUuid(tag.object_uuid));
            }

            TypedValue pid_val;
            auto pid_it = pid_by_proc.find(lock.proc_id);
            if (pid_it != pid_by_proc.end()) {
                pid_val = TypedValue::makeInt64(pid_it->second);
            }

            VirtualRow row;
            row.columns = {
                {"locktype", TypedValue::makeVarchar(pgLockTypeName(tag.target_type))},
                {"database", TypedValue::makeInt64(1)},
                {"relation", relation_val},
                {"page", has_page ? TypedValue::makeInt64(static_cast<int64_t>(tag.page_num)) : TypedValue()},
                {"tuple", has_tuple ? TypedValue::makeInt64(static_cast<int64_t>(tag.offset_num)) : TypedValue()},
                {"virtualxid", TypedValue()},
                {"transactionid", TypedValue()},
                {"classid", TypedValue()},
                {"objid", TypedValue()},
                {"objsubid", TypedValue()},
                {"pid", pid_val},
                {"mode", TypedValue::makeVarchar(pgLockModeName(lock.mode))},
                {"granted", TypedValue::makeBool(lock.granted)}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryPgSettings(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }

    Status queryPgTrigger(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog_manager_->listSchemas(schemas, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& schema : schemas) {
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog_manager_->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK) {
                continue;
            }

            for (const auto& table : tables) {
                for (auto timing : {CatalogManager::TriggerTiming::BEFORE,
                                    CatalogManager::TriggerTiming::AFTER}) {
                    for (auto event : {CatalogManager::TriggerEvent::INSERT,
                                       CatalogManager::TriggerEvent::UPDATE,
                                       CatalogManager::TriggerEvent::DELETE}) {
                        std::vector<CatalogManager::TriggerInfo> triggers;
                        status = catalog_manager_->listTriggersForTable(table.table_id, event, timing, triggers, ctx);
                        if (status != Status::OK) {
                            continue;
                        }
                        for (const auto& trigger : triggers) {
                            VirtualRow row;
                            row.columns = {
                                {"oid", TypedValue::makeInt64(oidFromUuid(trigger.trigger_id))},
                                {"tgname", TypedValue::makeVarchar(trigger.trigger_name)},
                                {"tgrelid", TypedValue::makeInt64(oidFromUuid(trigger.table_id))},
                                {"tgenabled", TypedValue::makeVarchar(trigger.enabled ? "O" : "D")}
                            };
                            results.rows.push_back(std::move(row));
                        }
                    }
                }
            }
        }

        return Status::OK;
    }

    Status queryPgInherits(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }
};

} // namespace scratchbird::catalog
