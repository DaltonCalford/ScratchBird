#pragma once

/**
 * MySQL mysql.* Implementation
 *
 * Phase D: Catalog Cleanup - MySQL system catalog emulation
 *
 * Exposes minimal mysql.* tables required for catalog introspection and
 * compatibility tooling. Rows are derived from ScratchBird catalog state.
 */

#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/typed_value.h"
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::catalog {

using namespace scratchbird::core;

/**
 * MySQLCatalogHandler - MySQL mysql.* schema emulation
 */
class MySQLCatalogHandler : public VirtualCatalogHandler {
public:
    explicit MySQLCatalogHandler(CatalogManager* catalog) {
        catalog_manager_ = catalog;
        initializeTableNames();
    }

    ProtocolType getProtocolType() const override {
        return ProtocolType::MYSQL;
    }

    bool ownsSchema(const std::string& schema_name) const override {
        return equalsCaseInsensitive(schema_name, "mysql") ||
               equalsCaseInsensitive(schema_name, "performance_schema");
    }

    bool ownsTable(const std::string& schema_name,
                   const std::string& table_name) const override {
        if (!ownsSchema(schema_name)) {
            return false;
        }
        const auto& names = tableNamesForSchema(schema_name);
        for (const auto& name : names) {
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
                              ("Table not found: " + schema_name + "." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        const ColumnDefs* def = getTableDefinition(table_name);
        if (!def) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table definition not found: " + schema_name + "." + table_name).c_str());
            return Status::NOT_FOUND;
        }
        setResultColumns(*def, results);

        if (equalsCaseInsensitive(schema_name, "performance_schema")) {
            if (equalsCaseInsensitive(table_name, "processlist")) {
                return queryProcesslist(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "threads")) {
                return queryThreads(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_current")) {
                return queryEventsStatementsCurrent(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_history_long")) {
                return queryEventsStatementsHistoryLong(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_summary_by_digest")) {
                return queryEventsStatementsSummaryByDigest(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_summary_by_account_by_digest")) {
                return queryEventsStatementsSummaryByAccountByDigest(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_summary_by_user_by_digest")) {
                return queryEventsStatementsSummaryByUserByDigest(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_summary_by_host_by_digest")) {
                return queryEventsStatementsSummaryByHostByDigest(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_histogram_by_digest")) {
                return queryEventsStatementsHistogramByDigest(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_statements_histogram_global")) {
                return queryEventsStatementsHistogramGlobal(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_transactions_current")) {
                return queryEventsTransactionsCurrent(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_transactions_history_long")) {
                return queryEventsTransactionsHistoryLong(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_waits_current")) {
                return queryEventsWaitsCurrent(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "events_waits_history_long")) {
                return queryEventsWaitsHistoryLong(results, ctx);
            }
            if (equalsCaseInsensitive(table_name, "metadata_locks")) {
                return queryMetadataLocks(results, ctx);
            }
        }

        if (equalsCaseInsensitive(table_name, "user")) {
            return queryUser(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "db")) {
            return queryDb(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "proc")) {
            return queryProc(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "tables_priv")) {
            return queryTablesPriv(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "columns_priv")) {
            return queryColumnsPriv(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "event")) {
            return queryEvent(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "func")) {
            return queryFunc(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "plugin")) {
            return queryPlugin(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "servers")) {
            return queryServers(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "time_zone")) {
            return queryTimeZone(results, ctx);
        }
        if (equalsCaseInsensitive(table_name, "time_zone_name")) {
            return queryTimeZoneName(results, ctx);
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
                              ("Table not found: " + schema_name + "." + table_name).c_str());
            return Status::NOT_FOUND;
        }

        const ColumnDefs* def = getTableDefinition(table_name);
        if (!def) {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              ("Table definition not found: " + schema_name + "." + table_name).c_str());
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

        table_names = tableNamesForSchema(schema_name);
        return Status::OK;
    }

    Status listSchemas(std::vector<std::string>& schema_names,
                       ErrorContext* /* ctx */ = nullptr) override {
        schema_names.clear();
        schema_names.push_back("mysql");
        schema_names.push_back("performance_schema");
        return Status::OK;
    }

private:
    struct ColumnDef {
        const char* name;
        DataType type;
        bool nullable;
    };

    using ColumnDefs = std::vector<ColumnDef>;

    std::vector<std::string> mysql_table_names_;
    std::vector<std::string> perf_table_names_;

    void initializeTableNames() {
        mysql_table_names_ = {
            "user", "db", "tables_priv", "columns_priv",
            "proc", "event", "func", "plugin",
            "servers", "time_zone", "time_zone_name"
        };
        perf_table_names_ = {
            "processlist",
            "threads",
            "events_statements_current",
            "events_statements_history_long",
            "events_statements_summary_by_digest",
            "events_statements_summary_by_account_by_digest",
            "events_statements_summary_by_user_by_digest",
            "events_statements_summary_by_host_by_digest",
            "events_statements_histogram_by_digest",
            "events_statements_histogram_global",
            "events_transactions_current",
            "events_transactions_history_long",
            "events_waits_current",
            "events_waits_history_long",
            "metadata_locks"
        };
    }

    const std::vector<std::string>& tableNamesForSchema(const std::string& schema_name) const {
        if (equalsCaseInsensitive(schema_name, "mysql")) {
            return mysql_table_names_;
        }
        if (equalsCaseInsensitive(schema_name, "performance_schema")) {
            return perf_table_names_;
        }
        static const std::vector<std::string> empty;
        return empty;
    }

    static std::string schemaName(const CatalogManager::SchemaInfo& schema) {
        return schema.full_path.empty() ? schema.schema_name : schema.full_path;
    }

    static bool isZeroId(const ID& id) {
        for (auto byte : id.bytes) {
            if (byte != 0) {
                return false;
            }
        }
        return true;
    }

    static const char* mysqlIsolationLevelName(uint8_t isolation_level) {
        switch (isolation_level) {
            case 0: return "READ COMMITTED";
            case 1: return "READ COMMITTED";
            case 2: return "REPEATABLE READ";
            case 3: return "SERIALIZABLE";
            default: return "READ COMMITTED";
        }
    }

    static const char* mysqlMetadataObjectType(LockTarget target) {
        switch (target) {
            case LockTarget::LOCK_TARGET_DATABASE: return "SCHEMA";
            case LockTarget::LOCK_TARGET_TABLE: return "TABLE";
            case LockTarget::LOCK_TARGET_PAGE: return "TABLE";
            case LockTarget::LOCK_TARGET_TUPLE: return "TABLE";
            default: return "TABLE";
        }
    }

    static const char* mysqlLockType(LockMode mode) {
        switch (mode) {
            case LockMode::LOCK_ACCESS_SHARE:
            case LockMode::LOCK_ROW_SHARE:
            case LockMode::LOCK_SHARE:
                return "READ";
            case LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE:
            case LockMode::LOCK_ROW_EXCLUSIVE:
            case LockMode::LOCK_SHARE_ROW_EXCLUSIVE:
            case LockMode::LOCK_EXCLUSIVE:
            case LockMode::LOCK_ACCESS_EXCLUSIVE:
            default:
                return "WRITE";
        }
    }

    static uint64_t digestQuantileValue(const std::array<uint64_t, CatalogManager::kDigestHistogramBuckets>& counts,
                                        double quantile)
    {
        uint64_t total = 0;
        for (uint64_t count : counts) {
            total += count;
        }
        if (total == 0) {
            return 0;
        }
        uint64_t target = static_cast<uint64_t>(std::ceil(quantile * static_cast<double>(total)));
        uint64_t cumulative = 0;
        for (size_t i = 0; i < counts.size(); ++i) {
            cumulative += counts[i];
            if (cumulative >= target) {
                return CatalogManager::digestHistogramUpperBound(i);
            }
        }
        return CatalogManager::digestHistogramUpperBound(counts.size() - 1);
    }

    static void appendDigestSummaryMetrics(VirtualRow& row,
                                           const CatalogManager::StatementDigestEntry& entry)
    {
        int64_t avg_timer = 0;
        if (entry.count_star != 0)
        {
            avg_timer = static_cast<int64_t>(entry.sum_timer_wait / entry.count_star);
        }
        uint64_t q95 = digestQuantileValue(entry.histogram_counts, 0.95);
        uint64_t q99 = digestQuantileValue(entry.histogram_counts, 0.99);
        uint64_t q999 = digestQuantileValue(entry.histogram_counts, 0.999);

        row.columns.push_back({"COUNT_STAR", TypedValue::makeInt64(static_cast<int64_t>(entry.count_star))});
        row.columns.push_back({"SUM_TIMER_WAIT", TypedValue::makeInt64(static_cast<int64_t>(entry.sum_timer_wait))});
        row.columns.push_back({"MIN_TIMER_WAIT", TypedValue::makeInt64(static_cast<int64_t>(entry.min_timer_wait))});
        row.columns.push_back({"AVG_TIMER_WAIT", TypedValue::makeInt64(avg_timer)});
        row.columns.push_back({"MAX_TIMER_WAIT", TypedValue::makeInt64(static_cast<int64_t>(entry.max_timer_wait))});
        row.columns.push_back({"SUM_LOCK_TIME", TypedValue::makeInt64(static_cast<int64_t>(entry.sum_lock_time))});
        row.columns.push_back({"SUM_ERRORS", TypedValue::makeInt64(static_cast<int64_t>(entry.sum_errors))});
        row.columns.push_back({"SUM_WARNINGS", TypedValue::makeInt64(static_cast<int64_t>(entry.sum_warnings))});
        row.columns.push_back({"SUM_ROWS_AFFECTED", TypedValue::makeInt64(static_cast<int64_t>(entry.sum_rows_affected))});
        row.columns.push_back({"SUM_ROWS_SENT", TypedValue::makeInt64(static_cast<int64_t>(entry.sum_rows_sent))});
        row.columns.push_back({"SUM_ROWS_EXAMINED", TypedValue::makeInt64(static_cast<int64_t>(entry.sum_rows_examined))});
        row.columns.push_back({"SUM_CREATED_TMP_DISK_TABLES",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_created_tmp_disk_tables))});
        row.columns.push_back({"SUM_CREATED_TMP_TABLES",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_created_tmp_tables))});
        row.columns.push_back({"SUM_SELECT_FULL_JOIN",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_select_full_join))});
        row.columns.push_back({"SUM_SELECT_FULL_RANGE_JOIN",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_select_full_range_join))});
        row.columns.push_back({"SUM_SELECT_RANGE",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_select_range))});
        row.columns.push_back({"SUM_SELECT_RANGE_CHECK",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_select_range_check))});
        row.columns.push_back({"SUM_SELECT_SCAN",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_select_scan))});
        row.columns.push_back({"SUM_SORT_MERGE_PASSES",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_sort_merge_passes))});
        row.columns.push_back({"SUM_SORT_RANGE",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_sort_range))});
        row.columns.push_back({"SUM_SORT_ROWS",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_sort_rows))});
        row.columns.push_back({"SUM_SORT_SCAN",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_sort_scan))});
        row.columns.push_back({"SUM_NO_INDEX_USED",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_no_index_used))});
        row.columns.push_back({"SUM_NO_GOOD_INDEX_USED",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_no_good_index_used))});
        row.columns.push_back({"SUM_CPU_TIME",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.sum_cpu_time))});
        row.columns.push_back({"MAX_CONTROLLED_MEMORY",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.max_controlled_memory))});
        row.columns.push_back({"MAX_TOTAL_MEMORY",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.max_total_memory))});
        row.columns.push_back({"COUNT_SECONDARY",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.count_secondary))});
        row.columns.push_back({"FIRST_SEEN", entry.first_seen == 0
            ? TypedValue()
            : TypedValue::makeTimestamp(static_cast<int64_t>(entry.first_seen))});
        row.columns.push_back({"LAST_SEEN", entry.last_seen == 0
            ? TypedValue()
            : TypedValue::makeTimestamp(static_cast<int64_t>(entry.last_seen))});
        row.columns.push_back({"QUANTILE_95", TypedValue::makeInt64(static_cast<int64_t>(q95))});
        row.columns.push_back({"QUANTILE_99", TypedValue::makeInt64(static_cast<int64_t>(q99))});
        row.columns.push_back({"QUANTILE_999", TypedValue::makeInt64(static_cast<int64_t>(q999))});
        row.columns.push_back({"QUERY_SAMPLE_TEXT", entry.query_sample_text.empty()
            ? TypedValue()
            : TypedValue::makeText(entry.query_sample_text)});
        row.columns.push_back({"QUERY_SAMPLE_SEEN", entry.query_sample_seen == 0
            ? TypedValue()
            : TypedValue::makeTimestamp(static_cast<int64_t>(entry.query_sample_seen))});
        row.columns.push_back({"QUERY_SAMPLE_TIMER_WAIT",
                               TypedValue::makeInt64(static_cast<int64_t>(entry.query_sample_timer_wait))});
    }

    static void appendDigestSummaryRow(VirtualResultSet& results,
                                       const CatalogManager::StatementDigestEntry& entry,
                                       bool include_user,
                                       bool include_host)
    {
        VirtualRow row;
        if (include_user)
        {
            row.columns.push_back({"USER", entry.user_name.empty()
                ? TypedValue()
                : TypedValue::makeVarchar(entry.user_name)});
        }
        if (include_host)
        {
            row.columns.push_back({"HOST", entry.host_name.empty()
                ? TypedValue()
                : TypedValue::makeVarchar(entry.host_name)});
        }
        row.columns.push_back({"SCHEMA_NAME", entry.schema_name.empty()
            ? TypedValue()
            : TypedValue::makeVarchar(entry.schema_name)});
        row.columns.push_back({"DIGEST", TypedValue::makeVarchar(entry.digest)});
        row.columns.push_back({"DIGEST_TEXT", entry.digest_text.empty()
            ? TypedValue()
            : TypedValue::makeText(entry.digest_text)});
        appendDigestSummaryMetrics(row, entry);
        results.rows.push_back(std::move(row));
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
        static const ColumnDefs user_cols = {
            {"Host", DataType::VARCHAR, false},
            {"User", DataType::VARCHAR, false},
            {"plugin", DataType::VARCHAR, true},
            {"authentication_string", DataType::TEXT, true}
        };
        static const ColumnDefs db_cols = {
            {"Host", DataType::VARCHAR, true},
            {"Db", DataType::VARCHAR, true},
            {"User", DataType::VARCHAR, true}
        };
        static const ColumnDefs proc_cols = {
            {"db", DataType::VARCHAR, true},
            {"name", DataType::VARCHAR, true},
            {"type", DataType::VARCHAR, true},
            {"body", DataType::TEXT, true}
        };
        static const ColumnDefs tables_priv_cols = {
            {"Host", DataType::VARCHAR, true},
            {"Db", DataType::VARCHAR, true},
            {"User", DataType::VARCHAR, true},
            {"Table_name", DataType::VARCHAR, true},
            {"Table_priv", DataType::TEXT, true},
            {"Column_priv", DataType::TEXT, true},
            {"Timestamp", DataType::TEXT, true}
        };
        static const ColumnDefs columns_priv_cols = {
            {"Host", DataType::VARCHAR, true},
            {"Db", DataType::VARCHAR, true},
            {"User", DataType::VARCHAR, true},
            {"Table_name", DataType::VARCHAR, true},
            {"Column_name", DataType::VARCHAR, true},
            {"Column_priv", DataType::TEXT, true},
            {"Timestamp", DataType::TEXT, true}
        };
        static const ColumnDefs event_cols = {
            {"db", DataType::VARCHAR, true},
            {"name", DataType::VARCHAR, true},
            {"body", DataType::TEXT, true}
        };
        static const ColumnDefs func_cols = {
            {"name", DataType::VARCHAR, true},
            {"dl", DataType::VARCHAR, true},
            {"type", DataType::VARCHAR, true}
        };
        static const ColumnDefs plugin_cols = {
            {"name", DataType::VARCHAR, true},
            {"dl", DataType::VARCHAR, true},
            {"type", DataType::VARCHAR, true}
        };
        static const ColumnDefs servers_cols = {
            {"Server_name", DataType::VARCHAR, true},
            {"Host", DataType::VARCHAR, true},
            {"Db", DataType::VARCHAR, true}
        };
        static const ColumnDefs time_zone_cols = {
            {"Time_zone_id", DataType::VARCHAR, true},
            {"Use_leap_seconds", DataType::VARCHAR, true}
        };
        static const ColumnDefs time_zone_name_cols = {
            {"Name", DataType::VARCHAR, true},
            {"Time_zone_id", DataType::VARCHAR, true}
        };
        static const ColumnDefs processlist_cols = {
            {"ID", DataType::INT64, false},
            {"USER", DataType::VARCHAR, true},
            {"HOST", DataType::VARCHAR, true},
            {"DB", DataType::VARCHAR, true},
            {"COMMAND", DataType::VARCHAR, true},
            {"TIME", DataType::INT64, true},
            {"STATE", DataType::VARCHAR, true},
            {"INFO", DataType::TEXT, true}
        };
        static const ColumnDefs threads_cols = {
            {"THREAD_ID", DataType::INT64, false},
            {"NAME", DataType::VARCHAR, true},
            {"TYPE", DataType::VARCHAR, true},
            {"PROCESSLIST_ID", DataType::INT64, true},
            {"PROCESSLIST_USER", DataType::VARCHAR, true},
            {"PROCESSLIST_HOST", DataType::VARCHAR, true},
            {"PROCESSLIST_DB", DataType::VARCHAR, true},
            {"PROCESSLIST_COMMAND", DataType::VARCHAR, true},
            {"PROCESSLIST_TIME", DataType::INT64, true},
            {"PROCESSLIST_STATE", DataType::VARCHAR, true},
            {"PROCESSLIST_INFO", DataType::TEXT, true},
            {"PARENT_THREAD_ID", DataType::INT64, true},
            {"ROLE", DataType::VARCHAR, true},
            {"INSTRUMENTED", DataType::VARCHAR, true},
            {"HISTORY", DataType::VARCHAR, true},
            {"CONNECTION_TYPE", DataType::VARCHAR, true},
            {"THREAD_OS_ID", DataType::INT64, true},
            {"RESOURCE_GROUP", DataType::VARCHAR, true}
        };
        static const ColumnDefs events_statements_cols = {
            {"THREAD_ID", DataType::INT64, false},
            {"EVENT_ID", DataType::INT64, false},
            {"END_EVENT_ID", DataType::INT64, true},
            {"EVENT_NAME", DataType::VARCHAR, true},
            {"TIMER_START", DataType::INT64, true},
            {"TIMER_END", DataType::INT64, true},
            {"TIMER_WAIT", DataType::INT64, true},
            {"LOCK_TIME", DataType::INT64, true},
            {"SQL_TEXT", DataType::TEXT, true},
            {"CURRENT_SCHEMA", DataType::VARCHAR, true},
            {"DIGEST", DataType::VARCHAR, true},
            {"DIGEST_TEXT", DataType::TEXT, true},
            {"MYSQL_ERRNO", DataType::INT64, true},
            {"RETURNED_SQLSTATE", DataType::VARCHAR, true},
            {"MESSAGE_TEXT", DataType::TEXT, true}
        };
        static const ColumnDefs events_statements_summary_by_digest_cols = {
            {"SCHEMA_NAME", DataType::VARCHAR, true},
            {"DIGEST", DataType::VARCHAR, false},
            {"DIGEST_TEXT", DataType::TEXT, true},
            {"COUNT_STAR", DataType::INT64, false},
            {"SUM_TIMER_WAIT", DataType::INT64, true},
            {"MIN_TIMER_WAIT", DataType::INT64, true},
            {"AVG_TIMER_WAIT", DataType::INT64, true},
            {"MAX_TIMER_WAIT", DataType::INT64, true},
            {"SUM_LOCK_TIME", DataType::INT64, true},
            {"SUM_ERRORS", DataType::INT64, true},
            {"SUM_WARNINGS", DataType::INT64, true},
            {"SUM_ROWS_AFFECTED", DataType::INT64, true},
            {"SUM_ROWS_SENT", DataType::INT64, true},
            {"SUM_ROWS_EXAMINED", DataType::INT64, true},
            {"SUM_CREATED_TMP_DISK_TABLES", DataType::INT64, true},
            {"SUM_CREATED_TMP_TABLES", DataType::INT64, true},
            {"SUM_SELECT_FULL_JOIN", DataType::INT64, true},
            {"SUM_SELECT_FULL_RANGE_JOIN", DataType::INT64, true},
            {"SUM_SELECT_RANGE", DataType::INT64, true},
            {"SUM_SELECT_RANGE_CHECK", DataType::INT64, true},
            {"SUM_SELECT_SCAN", DataType::INT64, true},
            {"SUM_SORT_MERGE_PASSES", DataType::INT64, true},
            {"SUM_SORT_RANGE", DataType::INT64, true},
            {"SUM_SORT_ROWS", DataType::INT64, true},
            {"SUM_SORT_SCAN", DataType::INT64, true},
            {"SUM_NO_INDEX_USED", DataType::INT64, true},
            {"SUM_NO_GOOD_INDEX_USED", DataType::INT64, true},
            {"SUM_CPU_TIME", DataType::INT64, true},
            {"MAX_CONTROLLED_MEMORY", DataType::INT64, true},
            {"MAX_TOTAL_MEMORY", DataType::INT64, true},
            {"COUNT_SECONDARY", DataType::INT64, true},
            {"FIRST_SEEN", DataType::TIMESTAMP, true},
            {"LAST_SEEN", DataType::TIMESTAMP, true},
            {"QUANTILE_95", DataType::INT64, true},
            {"QUANTILE_99", DataType::INT64, true},
            {"QUANTILE_999", DataType::INT64, true},
            {"QUERY_SAMPLE_TEXT", DataType::TEXT, true},
            {"QUERY_SAMPLE_SEEN", DataType::TIMESTAMP, true},
            {"QUERY_SAMPLE_TIMER_WAIT", DataType::INT64, true}
        };
        static const ColumnDefs events_statements_summary_by_account_by_digest_cols = {
            {"USER", DataType::VARCHAR, true},
            {"HOST", DataType::VARCHAR, true},
            {"SCHEMA_NAME", DataType::VARCHAR, true},
            {"DIGEST", DataType::VARCHAR, false},
            {"DIGEST_TEXT", DataType::TEXT, true},
            {"COUNT_STAR", DataType::INT64, false},
            {"SUM_TIMER_WAIT", DataType::INT64, true},
            {"MIN_TIMER_WAIT", DataType::INT64, true},
            {"AVG_TIMER_WAIT", DataType::INT64, true},
            {"MAX_TIMER_WAIT", DataType::INT64, true},
            {"SUM_LOCK_TIME", DataType::INT64, true},
            {"SUM_ERRORS", DataType::INT64, true},
            {"SUM_WARNINGS", DataType::INT64, true},
            {"SUM_ROWS_AFFECTED", DataType::INT64, true},
            {"SUM_ROWS_SENT", DataType::INT64, true},
            {"SUM_ROWS_EXAMINED", DataType::INT64, true},
            {"SUM_CREATED_TMP_DISK_TABLES", DataType::INT64, true},
            {"SUM_CREATED_TMP_TABLES", DataType::INT64, true},
            {"SUM_SELECT_FULL_JOIN", DataType::INT64, true},
            {"SUM_SELECT_FULL_RANGE_JOIN", DataType::INT64, true},
            {"SUM_SELECT_RANGE", DataType::INT64, true},
            {"SUM_SELECT_RANGE_CHECK", DataType::INT64, true},
            {"SUM_SELECT_SCAN", DataType::INT64, true},
            {"SUM_SORT_MERGE_PASSES", DataType::INT64, true},
            {"SUM_SORT_RANGE", DataType::INT64, true},
            {"SUM_SORT_ROWS", DataType::INT64, true},
            {"SUM_SORT_SCAN", DataType::INT64, true},
            {"SUM_NO_INDEX_USED", DataType::INT64, true},
            {"SUM_NO_GOOD_INDEX_USED", DataType::INT64, true},
            {"SUM_CPU_TIME", DataType::INT64, true},
            {"MAX_CONTROLLED_MEMORY", DataType::INT64, true},
            {"MAX_TOTAL_MEMORY", DataType::INT64, true},
            {"COUNT_SECONDARY", DataType::INT64, true},
            {"FIRST_SEEN", DataType::TIMESTAMP, true},
            {"LAST_SEEN", DataType::TIMESTAMP, true},
            {"QUANTILE_95", DataType::INT64, true},
            {"QUANTILE_99", DataType::INT64, true},
            {"QUANTILE_999", DataType::INT64, true},
            {"QUERY_SAMPLE_TEXT", DataType::TEXT, true},
            {"QUERY_SAMPLE_SEEN", DataType::TIMESTAMP, true},
            {"QUERY_SAMPLE_TIMER_WAIT", DataType::INT64, true}
        };
        static const ColumnDefs events_statements_summary_by_user_by_digest_cols = {
            {"USER", DataType::VARCHAR, true},
            {"SCHEMA_NAME", DataType::VARCHAR, true},
            {"DIGEST", DataType::VARCHAR, false},
            {"DIGEST_TEXT", DataType::TEXT, true},
            {"COUNT_STAR", DataType::INT64, false},
            {"SUM_TIMER_WAIT", DataType::INT64, true},
            {"MIN_TIMER_WAIT", DataType::INT64, true},
            {"AVG_TIMER_WAIT", DataType::INT64, true},
            {"MAX_TIMER_WAIT", DataType::INT64, true},
            {"SUM_LOCK_TIME", DataType::INT64, true},
            {"SUM_ERRORS", DataType::INT64, true},
            {"SUM_WARNINGS", DataType::INT64, true},
            {"SUM_ROWS_AFFECTED", DataType::INT64, true},
            {"SUM_ROWS_SENT", DataType::INT64, true},
            {"SUM_ROWS_EXAMINED", DataType::INT64, true},
            {"SUM_CREATED_TMP_DISK_TABLES", DataType::INT64, true},
            {"SUM_CREATED_TMP_TABLES", DataType::INT64, true},
            {"SUM_SELECT_FULL_JOIN", DataType::INT64, true},
            {"SUM_SELECT_FULL_RANGE_JOIN", DataType::INT64, true},
            {"SUM_SELECT_RANGE", DataType::INT64, true},
            {"SUM_SELECT_RANGE_CHECK", DataType::INT64, true},
            {"SUM_SELECT_SCAN", DataType::INT64, true},
            {"SUM_SORT_MERGE_PASSES", DataType::INT64, true},
            {"SUM_SORT_RANGE", DataType::INT64, true},
            {"SUM_SORT_ROWS", DataType::INT64, true},
            {"SUM_SORT_SCAN", DataType::INT64, true},
            {"SUM_NO_INDEX_USED", DataType::INT64, true},
            {"SUM_NO_GOOD_INDEX_USED", DataType::INT64, true},
            {"SUM_CPU_TIME", DataType::INT64, true},
            {"MAX_CONTROLLED_MEMORY", DataType::INT64, true},
            {"MAX_TOTAL_MEMORY", DataType::INT64, true},
            {"COUNT_SECONDARY", DataType::INT64, true},
            {"FIRST_SEEN", DataType::TIMESTAMP, true},
            {"LAST_SEEN", DataType::TIMESTAMP, true},
            {"QUANTILE_95", DataType::INT64, true},
            {"QUANTILE_99", DataType::INT64, true},
            {"QUANTILE_999", DataType::INT64, true},
            {"QUERY_SAMPLE_TEXT", DataType::TEXT, true},
            {"QUERY_SAMPLE_SEEN", DataType::TIMESTAMP, true},
            {"QUERY_SAMPLE_TIMER_WAIT", DataType::INT64, true}
        };
        static const ColumnDefs events_statements_summary_by_host_by_digest_cols = {
            {"HOST", DataType::VARCHAR, true},
            {"SCHEMA_NAME", DataType::VARCHAR, true},
            {"DIGEST", DataType::VARCHAR, false},
            {"DIGEST_TEXT", DataType::TEXT, true},
            {"COUNT_STAR", DataType::INT64, false},
            {"SUM_TIMER_WAIT", DataType::INT64, true},
            {"MIN_TIMER_WAIT", DataType::INT64, true},
            {"AVG_TIMER_WAIT", DataType::INT64, true},
            {"MAX_TIMER_WAIT", DataType::INT64, true},
            {"SUM_LOCK_TIME", DataType::INT64, true},
            {"SUM_ERRORS", DataType::INT64, true},
            {"SUM_WARNINGS", DataType::INT64, true},
            {"SUM_ROWS_AFFECTED", DataType::INT64, true},
            {"SUM_ROWS_SENT", DataType::INT64, true},
            {"SUM_ROWS_EXAMINED", DataType::INT64, true},
            {"SUM_CREATED_TMP_DISK_TABLES", DataType::INT64, true},
            {"SUM_CREATED_TMP_TABLES", DataType::INT64, true},
            {"SUM_SELECT_FULL_JOIN", DataType::INT64, true},
            {"SUM_SELECT_FULL_RANGE_JOIN", DataType::INT64, true},
            {"SUM_SELECT_RANGE", DataType::INT64, true},
            {"SUM_SELECT_RANGE_CHECK", DataType::INT64, true},
            {"SUM_SELECT_SCAN", DataType::INT64, true},
            {"SUM_SORT_MERGE_PASSES", DataType::INT64, true},
            {"SUM_SORT_RANGE", DataType::INT64, true},
            {"SUM_SORT_ROWS", DataType::INT64, true},
            {"SUM_SORT_SCAN", DataType::INT64, true},
            {"SUM_NO_INDEX_USED", DataType::INT64, true},
            {"SUM_NO_GOOD_INDEX_USED", DataType::INT64, true},
            {"SUM_CPU_TIME", DataType::INT64, true},
            {"MAX_CONTROLLED_MEMORY", DataType::INT64, true},
            {"MAX_TOTAL_MEMORY", DataType::INT64, true},
            {"COUNT_SECONDARY", DataType::INT64, true},
            {"FIRST_SEEN", DataType::TIMESTAMP, true},
            {"LAST_SEEN", DataType::TIMESTAMP, true},
            {"QUANTILE_95", DataType::INT64, true},
            {"QUANTILE_99", DataType::INT64, true},
            {"QUANTILE_999", DataType::INT64, true},
            {"QUERY_SAMPLE_TEXT", DataType::TEXT, true},
            {"QUERY_SAMPLE_SEEN", DataType::TIMESTAMP, true},
            {"QUERY_SAMPLE_TIMER_WAIT", DataType::INT64, true}
        };
        static const ColumnDefs events_statements_histogram_by_digest_cols = {
            {"SCHEMA_NAME", DataType::VARCHAR, true},
            {"DIGEST", DataType::VARCHAR, false},
            {"BUCKET_NUMBER", DataType::INT64, false},
            {"BUCKET_TIMER_LOW", DataType::INT64, true},
            {"BUCKET_TIMER_HIGH", DataType::INT64, true},
            {"COUNT_BUCKET", DataType::INT64, false},
            {"COUNT_BUCKET_AND_LOWER", DataType::INT64, false},
            {"BUCKET_QUANTILE", DataType::FLOAT64, true}
        };
        static const ColumnDefs events_statements_histogram_global_cols = {
            {"BUCKET_NUMBER", DataType::INT64, false},
            {"BUCKET_TIMER_LOW", DataType::INT64, true},
            {"BUCKET_TIMER_HIGH", DataType::INT64, true},
            {"COUNT_BUCKET", DataType::INT64, false},
            {"COUNT_BUCKET_AND_LOWER", DataType::INT64, false},
            {"BUCKET_QUANTILE", DataType::FLOAT64, true}
        };
        static const ColumnDefs events_transactions_cols = {
            {"THREAD_ID", DataType::INT64, false},
            {"EVENT_ID", DataType::INT64, false},
            {"END_EVENT_ID", DataType::INT64, true},
            {"EVENT_NAME", DataType::VARCHAR, true},
            {"STATE", DataType::VARCHAR, true},
            {"TRX_ID", DataType::INT64, true},
            {"SOURCE", DataType::VARCHAR, true},
            {"TIMER_START", DataType::INT64, true},
            {"TIMER_END", DataType::INT64, true},
            {"TIMER_WAIT", DataType::INT64, true},
            {"ACCESS_MODE", DataType::VARCHAR, true},
            {"ISOLATION_LEVEL", DataType::VARCHAR, true},
            {"AUTOCOMMIT", DataType::VARCHAR, true}
        };
        static const ColumnDefs events_waits_cols = {
            {"THREAD_ID", DataType::INT64, false},
            {"EVENT_ID", DataType::INT64, false},
            {"END_EVENT_ID", DataType::INT64, true},
            {"EVENT_NAME", DataType::VARCHAR, true},
            {"SOURCE", DataType::VARCHAR, true},
            {"TIMER_START", DataType::INT64, true},
            {"TIMER_END", DataType::INT64, true},
            {"TIMER_WAIT", DataType::INT64, true},
            {"SPINS", DataType::INT64, true},
            {"OBJECT_INSTANCE_BEGIN", DataType::INT64, true},
            {"NESTING_EVENT_ID", DataType::INT64, true},
            {"NESTING_EVENT_TYPE", DataType::VARCHAR, true},
            {"OPERATION", DataType::VARCHAR, true},
            {"NUMBER_OF_BYTES", DataType::INT64, true},
            {"FLAGS", DataType::VARCHAR, true}
        };
        static const ColumnDefs metadata_locks_cols = {
            {"OBJECT_TYPE", DataType::VARCHAR, false},
            {"OBJECT_SCHEMA", DataType::VARCHAR, true},
            {"OBJECT_NAME", DataType::VARCHAR, true},
            {"COLUMN_NAME", DataType::VARCHAR, true},
            {"OBJECT_INSTANCE_BEGIN", DataType::INT64, true},
            {"LOCK_TYPE", DataType::VARCHAR, true},
            {"LOCK_DURATION", DataType::VARCHAR, true},
            {"LOCK_STATUS", DataType::VARCHAR, true},
            {"SOURCE", DataType::VARCHAR, true},
            {"OWNER_THREAD_ID", DataType::INT64, true},
            {"OWNER_EVENT_ID", DataType::INT64, true}
        };

        if (equalsCaseInsensitive(table_name, "user")) return &user_cols;
        if (equalsCaseInsensitive(table_name, "db")) return &db_cols;
        if (equalsCaseInsensitive(table_name, "proc")) return &proc_cols;
        if (equalsCaseInsensitive(table_name, "tables_priv")) return &tables_priv_cols;
        if (equalsCaseInsensitive(table_name, "columns_priv")) return &columns_priv_cols;
        if (equalsCaseInsensitive(table_name, "event")) return &event_cols;
        if (equalsCaseInsensitive(table_name, "func")) return &func_cols;
        if (equalsCaseInsensitive(table_name, "plugin")) return &plugin_cols;
        if (equalsCaseInsensitive(table_name, "servers")) return &servers_cols;
        if (equalsCaseInsensitive(table_name, "time_zone")) return &time_zone_cols;
        if (equalsCaseInsensitive(table_name, "time_zone_name")) return &time_zone_name_cols;
        if (equalsCaseInsensitive(table_name, "processlist")) return &processlist_cols;
        if (equalsCaseInsensitive(table_name, "threads")) return &threads_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_current")) return &events_statements_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_history_long")) return &events_statements_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_summary_by_digest")) return &events_statements_summary_by_digest_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_summary_by_account_by_digest")) return &events_statements_summary_by_account_by_digest_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_summary_by_user_by_digest")) return &events_statements_summary_by_user_by_digest_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_summary_by_host_by_digest")) return &events_statements_summary_by_host_by_digest_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_histogram_by_digest")) return &events_statements_histogram_by_digest_cols;
        if (equalsCaseInsensitive(table_name, "events_statements_histogram_global")) return &events_statements_histogram_global_cols;
        if (equalsCaseInsensitive(table_name, "events_transactions_current")) return &events_transactions_cols;
        if (equalsCaseInsensitive(table_name, "events_transactions_history_long")) return &events_transactions_cols;
        if (equalsCaseInsensitive(table_name, "events_waits_current")) return &events_waits_cols;
        if (equalsCaseInsensitive(table_name, "events_waits_history_long")) return &events_waits_cols;
        if (equalsCaseInsensitive(table_name, "metadata_locks")) return &metadata_locks_cols;

        return nullptr;
    }

    Status queryUser(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::UserInfo> users;
        if (catalog_manager_->listUsers(users, ctx) == Status::OK) {
            for (const auto& user : users) {
                VirtualRow row;
                row.columns = {
                    {"Host", TypedValue::makeVarchar("%")},
                    {"User", TypedValue::makeVarchar(user.username)},
                    {"plugin", TypedValue::makeVarchar("scratchbird_native")},
                    {"authentication_string", user.password_hash.empty() ? TypedValue() : TypedValue::makeText(user.password_hash)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        std::vector<CatalogManager::RoleInfo> roles;
        if (catalog_manager_->listRoles(roles, ctx) == Status::OK) {
            for (const auto& role : roles) {
                VirtualRow row;
                row.columns = {
                    {"Host", TypedValue::makeVarchar("%")},
                    {"User", TypedValue::makeVarchar(role.role_name)},
                    {"plugin", TypedValue::makeVarchar("scratchbird_role")},
                    {"authentication_string", TypedValue()}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryDb(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::unordered_map<ID, std::string, IDHash> schema_names;
        std::vector<CatalogManager::SchemaInfo> schemas;
        if (catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
            for (const auto& schema : schemas) {
                schema_names.emplace(schema.schema_id, schemaName(schema));
            }
        }

        std::unordered_map<ID, std::string, IDHash> user_names;
        std::vector<CatalogManager::UserInfo> users;
        if (catalog_manager_->listUsers(users, ctx) == Status::OK) {
            for (const auto& user : users) {
                user_names.emplace(user.user_id, user.username);
            }
        }

        std::unordered_map<ID, std::string, IDHash> role_names;
        std::vector<CatalogManager::RoleInfo> roles;
        if (catalog_manager_->listRoles(roles, ctx) == Status::OK) {
            for (const auto& role : roles) {
                role_names.emplace(role.role_id, role.role_name);
            }
        }

        std::unordered_map<ID, std::string, IDHash> group_names;
        std::vector<CatalogManager::GroupInfo> groups;
        if (catalog_manager_->listGroups(groups, ctx) == Status::OK) {
            for (const auto& group : groups) {
                group_names.emplace(group.group_id, group.group_name);
            }
        }

        auto grantee_name = [&](const CatalogManager::PermissionInfo& perm) -> std::string {
            switch (perm.grantee_type) {
                case CatalogManager::GranteeType::USER: {
                    auto it = user_names.find(perm.grantee_id);
                    return it == user_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::ROLE: {
                    auto it = role_names.find(perm.grantee_id);
                    return it == role_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::GROUP: {
                    auto it = group_names.find(perm.grantee_id);
                    return it == group_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::PUBLIC:
                default:
                    return "PUBLIC";
            }
        };

        std::vector<CatalogManager::PermissionInfo> perms;
        if (catalog_manager_->listPermissions(perms, ctx) != Status::OK) {
            return Status::OK;
        }

        for (const auto& perm : perms) {
            if (perm.object_type != CatalogManager::PermissionObjectType::SCHEMA) {
                continue;
            }
            auto schema_it = schema_names.find(perm.object_id);
            if (schema_it == schema_names.end()) {
                continue;
            }
            std::string user = grantee_name(perm);
            if (user.empty()) {
                continue;
            }
            VirtualRow row;
            row.columns = {
                {"Host", TypedValue::makeVarchar("%")},
                {"Db", TypedValue::makeVarchar(schema_it->second)},
                {"User", TypedValue::makeVarchar(user)}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryProc(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::unordered_map<ID, std::string, IDHash> schema_names;
        std::vector<CatalogManager::SchemaInfo> schemas;
        if (catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
            for (const auto& schema : schemas) {
                schema_names.emplace(schema.schema_id, schemaName(schema));
            }
        }

        std::vector<CatalogManager::FunctionInfo> functions;
        if (catalog_manager_->listFunctions(functions, ctx) == Status::OK) {
            for (const auto& func : functions) {
                std::string db_name = schema_names.count(func.schema_id)
                    ? schema_names[func.schema_id]
                    : "public";

                VirtualRow row;
                row.columns = {
                    {"db", TypedValue::makeVarchar(db_name)},
                    {"name", TypedValue::makeVarchar(func.name)},
                    {"type", TypedValue::makeVarchar("FUNCTION")},
                    {"body", func.source_text.empty() ? TypedValue() : TypedValue::makeText(func.source_text)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        std::vector<CatalogManager::ProcedureInfo> procedures;
        if (catalog_manager_->listProcedures(procedures, ctx) == Status::OK) {
            for (const auto& proc : procedures) {
                std::string db_name = schema_names.count(proc.schema_id)
                    ? schema_names[proc.schema_id]
                    : "public";

                VirtualRow row;
                row.columns = {
                    {"db", TypedValue::makeVarchar(db_name)},
                    {"name", TypedValue::makeVarchar(proc.name)},
                    {"type", TypedValue::makeVarchar("PROCEDURE")},
                    {"body", proc.source_text.empty() ? TypedValue() : TypedValue::makeText(proc.source_text)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryTablesPriv(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::unordered_map<ID, std::string, IDHash> user_names;
        std::vector<CatalogManager::UserInfo> users;
        if (catalog_manager_->listUsers(users, ctx) == Status::OK) {
            for (const auto& user : users) {
                user_names.emplace(user.user_id, user.username);
            }
        }

        std::unordered_map<ID, std::string, IDHash> role_names;
        std::vector<CatalogManager::RoleInfo> roles;
        if (catalog_manager_->listRoles(roles, ctx) == Status::OK) {
            for (const auto& role : roles) {
                role_names.emplace(role.role_id, role.role_name);
            }
        }

        std::unordered_map<ID, std::string, IDHash> group_names;
        std::vector<CatalogManager::GroupInfo> groups;
        if (catalog_manager_->listGroups(groups, ctx) == Status::OK) {
            for (const auto& group : groups) {
                group_names.emplace(group.group_id, group.group_name);
            }
        }

        auto grantee_name = [&](const CatalogManager::PermissionInfo& perm) -> std::string {
            switch (perm.grantee_type) {
                case CatalogManager::GranteeType::USER: {
                    auto it = user_names.find(perm.grantee_id);
                    return it == user_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::ROLE: {
                    auto it = role_names.find(perm.grantee_id);
                    return it == role_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::GROUP: {
                    auto it = group_names.find(perm.grantee_id);
                    return it == group_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::PUBLIC:
                default:
                    return "PUBLIC";
            }
        };

        auto privilege_list = [](uint32_t privs) -> std::string {
            std::vector<std::string> names;
            auto add = [&](CatalogManager::Privilege priv, const char* name) {
                if ((privs & static_cast<uint32_t>(priv)) != 0) {
                    names.emplace_back(name);
                }
            };
            add(CatalogManager::Privilege::SELECT, "Select");
            add(CatalogManager::Privilege::INSERT, "Insert");
            add(CatalogManager::Privilege::UPDATE, "Update");
            add(CatalogManager::Privilege::DELETE, "Delete");
            add(CatalogManager::Privilege::REFERENCES, "References");
            add(CatalogManager::Privilege::TRIGGER, "Trigger");
            add(CatalogManager::Privilege::TRUNCATE, "Truncate");

            std::string out;
            for (size_t i = 0; i < names.size(); ++i) {
                if (i > 0) {
                    out.append(", ");
                }
                out.append(names[i]);
            }
            return out;
        };

        std::unordered_map<ID, CatalogManager::TableInfo, IDHash> tables_by_id;
        std::unordered_map<ID, std::string, IDHash> schema_names;
        std::vector<CatalogManager::SchemaInfo> schemas;
        if (catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
            for (const auto& schema : schemas) {
                schema_names.emplace(schema.schema_id, schemaName(schema));
                std::vector<CatalogManager::TableInfo> tables;
                if (catalog_manager_->listTables(schema.schema_id, tables, ctx) == Status::OK) {
                    for (const auto& table : tables) {
                        tables_by_id.emplace(table.table_id, table);
                    }
                }
            }
        }

        struct ColumnKeyHash {
            size_t operator()(const std::string& key) const {
                return std::hash<std::string>()(key);
            }
        };

        std::unordered_map<std::string, uint32_t, ColumnKeyHash> column_privs;
        for (const auto& table_entry : tables_by_id) {
            std::vector<CatalogManager::ColumnPermissionInfo> col_perms;
            if (catalog_manager_->getColumnPermissions(table_entry.first, col_perms, ctx) != Status::OK) {
                continue;
            }
            for (const auto& perm : col_perms) {
                std::string key = table_entry.first.toString() + "|" +
                                  perm.grantee_id.toString() + "|" +
                                  std::to_string(static_cast<uint8_t>(perm.grantee_type));
                column_privs[key] |= perm.privileges;
            }
        }

        std::vector<CatalogManager::PermissionInfo> perms;
        if (catalog_manager_->listPermissions(perms, ctx) != Status::OK) {
            return Status::OK;
        }

        for (const auto& perm : perms) {
            if (perm.object_type != CatalogManager::PermissionObjectType::TABLE) {
                continue;
            }
            auto table_it = tables_by_id.find(perm.object_id);
            if (table_it == tables_by_id.end()) {
                continue;
            }
            std::string user = grantee_name(perm);
            if (user.empty()) {
                continue;
            }
            std::string schema_name;
            auto schema_it = schema_names.find(table_it->second.schema_id);
            if (schema_it != schema_names.end()) {
                schema_name = schema_it->second;
            }

            std::string key = perm.object_id.toString() + "|" +
                              perm.grantee_id.toString() + "|" +
                              std::to_string(static_cast<uint8_t>(perm.grantee_type));
            uint32_t column_priv_mask = 0;
            auto col_it = column_privs.find(key);
            if (col_it != column_privs.end()) {
                column_priv_mask = col_it->second;
            }

            VirtualRow row;
            row.columns = {
                {"Host", TypedValue::makeVarchar("%")},
                {"Db", TypedValue::makeVarchar(schema_name)},
                {"User", TypedValue::makeVarchar(user)},
                {"Table_name", TypedValue::makeVarchar(table_it->second.table_name)},
                {"Table_priv", TypedValue::makeText(privilege_list(perm.privileges))},
                {"Column_priv", TypedValue::makeText(privilege_list(column_priv_mask))},
                {"Timestamp", TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryColumnsPriv(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::unordered_map<ID, std::string, IDHash> user_names;
        std::vector<CatalogManager::UserInfo> users;
        if (catalog_manager_->listUsers(users, ctx) == Status::OK) {
            for (const auto& user : users) {
                user_names.emplace(user.user_id, user.username);
            }
        }

        std::unordered_map<ID, std::string, IDHash> role_names;
        std::vector<CatalogManager::RoleInfo> roles;
        if (catalog_manager_->listRoles(roles, ctx) == Status::OK) {
            for (const auto& role : roles) {
                role_names.emplace(role.role_id, role.role_name);
            }
        }

        std::unordered_map<ID, std::string, IDHash> group_names;
        std::vector<CatalogManager::GroupInfo> groups;
        if (catalog_manager_->listGroups(groups, ctx) == Status::OK) {
            for (const auto& group : groups) {
                group_names.emplace(group.group_id, group.group_name);
            }
        }

        auto grantee_name = [&](const CatalogManager::ColumnPermissionInfo& perm) -> std::string {
            switch (perm.grantee_type) {
                case CatalogManager::GranteeType::USER: {
                    auto it = user_names.find(perm.grantee_id);
                    return it == user_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::ROLE: {
                    auto it = role_names.find(perm.grantee_id);
                    return it == role_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::GROUP: {
                    auto it = group_names.find(perm.grantee_id);
                    return it == group_names.end() ? std::string() : it->second;
                }
                case CatalogManager::GranteeType::PUBLIC:
                default:
                    return "PUBLIC";
            }
        };

        auto privilege_list = [](uint32_t privs) -> std::string {
            std::vector<std::string> names;
            auto add = [&](CatalogManager::Privilege priv, const char* name) {
                if ((privs & static_cast<uint32_t>(priv)) != 0) {
                    names.emplace_back(name);
                }
            };
            add(CatalogManager::Privilege::SELECT, "Select");
            add(CatalogManager::Privilege::INSERT, "Insert");
            add(CatalogManager::Privilege::UPDATE, "Update");
            add(CatalogManager::Privilege::REFERENCES, "References");

            std::string out;
            for (size_t i = 0; i < names.size(); ++i) {
                if (i > 0) {
                    out.append(", ");
                }
                out.append(names[i]);
            }
            return out;
        };

        std::unordered_map<ID, CatalogManager::TableInfo, IDHash> tables_by_id;
        std::unordered_map<ID, std::string, IDHash> schema_names;
        std::vector<CatalogManager::SchemaInfo> schemas;
        if (catalog_manager_->listSchemas(schemas, ctx) == Status::OK) {
            for (const auto& schema : schemas) {
                schema_names.emplace(schema.schema_id, schemaName(schema));
                std::vector<CatalogManager::TableInfo> tables;
                if (catalog_manager_->listTables(schema.schema_id, tables, ctx) == Status::OK) {
                    for (const auto& table : tables) {
                        tables_by_id.emplace(table.table_id, table);
                    }
                }
            }
        }

        for (const auto& table_entry : tables_by_id) {
            std::vector<CatalogManager::ColumnPermissionInfo> col_perms;
            if (catalog_manager_->getColumnPermissions(table_entry.first, col_perms, ctx) != Status::OK) {
                continue;
            }
            for (const auto& perm : col_perms) {
                std::string user = grantee_name(perm);
                if (user.empty()) {
                    continue;
                }
                std::string schema_name;
                auto schema_it = schema_names.find(table_entry.second.schema_id);
                if (schema_it != schema_names.end()) {
                    schema_name = schema_it->second;
                }

                VirtualRow row;
                row.columns = {
                    {"Host", TypedValue::makeVarchar("%")},
                    {"Db", TypedValue::makeVarchar(schema_name)},
                    {"User", TypedValue::makeVarchar(user)},
                    {"Table_name", TypedValue::makeVarchar(table_entry.second.table_name)},
                    {"Column_name", TypedValue::makeVarchar(perm.column_name)},
                    {"Column_priv", TypedValue::makeText(privilege_list(perm.privileges))},
                    {"Timestamp", TypedValue()}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryEvent(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }

    Status queryFunc(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }

    Status queryPlugin(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }

    Status queryServers(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }

    Status queryTimeZone(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }

    Status queryTimeZoneName(VirtualResultSet& /* results */, ErrorContext* /* ctx */) {
        return Status::OK;
    }

    Status queryProcesslist(VirtualResultSet& results, ErrorContext* ctx) {
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

        auto now_micros = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        for (const auto& backend : backends) {
            const CatalogManager::SessionInfo* session = nullptr;
            auto it = sessions_by_id.find(backend.session_id);
            if (it != sessions_by_id.end()) {
                session = &it->second;
            }

            uint64_t base_time = backend.state_change_time != 0
                ? backend.state_change_time
                : backend.start_time;
            if (backend.query_start_time != 0) {
                base_time = backend.query_start_time;
            }

            uint64_t elapsed_seconds = 0;
            if (base_time > 0 && now_micros >= base_time) {
                elapsed_seconds = (now_micros - base_time) / 1000000ULL;
            }

            std::string command = backend.query_start_time != 0 ? "Query" : "Sleep";
            TypedValue info;
            if (backend.query_start_time != 0 && backend.query_text[0] != '\0') {
                info = TypedValue::makeText(backend.query_text);
            }

            std::string user = session ? session->username : "unknown";
            int64_t id_value = backend.proc_id;

            VirtualRow row;
            row.columns = {
                {"ID", TypedValue::makeInt64(id_value)},
                {"USER", TypedValue::makeVarchar(user)},
                {"HOST", TypedValue::makeVarchar("local")},
                {"DB", TypedValue::makeVarchar("scratchbird")},
                {"COMMAND", TypedValue::makeVarchar(command)},
                {"TIME", TypedValue::makeInt64(static_cast<int64_t>(elapsed_seconds))},
                {"STATE", backend.query_start_time != 0 ? TypedValue::makeVarchar("executing") : TypedValue()},
                {"INFO", info}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryThreads(VirtualResultSet& results, ErrorContext* ctx) {
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

        auto now_micros = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        for (const auto& backend : backends) {
            const CatalogManager::SessionInfo* session = nullptr;
            auto it = sessions_by_id.find(backend.session_id);
            if (it != sessions_by_id.end()) {
                session = &it->second;
            }

            uint64_t base_time = backend.state_change_time != 0
                ? backend.state_change_time
                : backend.start_time;
            if (backend.query_start_time != 0) {
                base_time = backend.query_start_time;
            }

            uint64_t elapsed_seconds = 0;
            if (base_time > 0 && now_micros >= base_time) {
                elapsed_seconds = (now_micros - base_time) / 1000000ULL;
            }

            std::string command = backend.query_start_time != 0 ? "Query" : "Sleep";
            TypedValue info;
            if (backend.query_start_time != 0 && backend.query_text[0] != '\0') {
                info = TypedValue::makeText(backend.query_text);
            }

            std::string user = session ? session->username : "unknown";
            std::string name = "thread/" + std::to_string(backend.proc_id);

            VirtualRow row;
            row.columns = {
                {"THREAD_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id))},
                {"NAME", TypedValue::makeVarchar(name)},
                {"TYPE", TypedValue::makeVarchar("FOREGROUND")},
                {"PROCESSLIST_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id))},
                {"PROCESSLIST_USER", TypedValue::makeVarchar(user)},
                {"PROCESSLIST_HOST", TypedValue::makeVarchar("local")},
                {"PROCESSLIST_DB", TypedValue::makeVarchar("scratchbird")},
                {"PROCESSLIST_COMMAND", TypedValue::makeVarchar(command)},
                {"PROCESSLIST_TIME", TypedValue::makeInt64(static_cast<int64_t>(elapsed_seconds))},
                {"PROCESSLIST_STATE", backend.query_start_time != 0 ? TypedValue::makeVarchar("executing") : TypedValue()},
                {"PROCESSLIST_INFO", info},
                {"PARENT_THREAD_ID", TypedValue()},
                {"ROLE", TypedValue()},
                {"INSTRUMENTED", TypedValue::makeVarchar("YES")},
                {"HISTORY", TypedValue::makeVarchar("YES")},
                {"CONNECTION_TYPE", TypedValue::makeVarchar("LOCAL")},
                {"THREAD_OS_ID", backend.backend_pid == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(backend.backend_pid))},
                {"RESOURCE_GROUP", TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryEventsStatementsCurrent(VirtualResultSet& results, ErrorContext* ctx) {
        return queryEventsStatements(results, ctx, true);
    }

    Status queryEventsStatementsHistoryLong(VirtualResultSet& results, ErrorContext* ctx) {
        return queryEventsStatements(results, ctx, false);
    }

    Status queryEventsStatementsSummaryByDigest(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::StatementDigestEntry> digests;
        catalog_manager_->listStatementDigestSummary(digests, ctx);

        for (const auto& entry : digests) {
            appendDigestSummaryRow(results, entry, false, false);
        }

        return Status::OK;
    }

    Status queryEventsStatementsSummaryByAccountByDigest(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::StatementDigestEntry> digests;
        catalog_manager_->listStatementDigestSummaryByAccount(digests, ctx);

        for (const auto& entry : digests) {
            appendDigestSummaryRow(results, entry, true, true);
        }

        return Status::OK;
    }

    Status queryEventsStatementsSummaryByUserByDigest(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::StatementDigestEntry> digests;
        catalog_manager_->listStatementDigestSummaryByUser(digests, ctx);

        for (const auto& entry : digests) {
            appendDigestSummaryRow(results, entry, true, false);
        }

        return Status::OK;
    }

    Status queryEventsStatementsSummaryByHostByDigest(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::StatementDigestEntry> digests;
        catalog_manager_->listStatementDigestSummaryByHost(digests, ctx);

        for (const auto& entry : digests) {
            appendDigestSummaryRow(results, entry, false, true);
        }

        return Status::OK;
    }

    Status queryEventsStatementsHistogramByDigest(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::StatementDigestEntry> digests;
        catalog_manager_->listStatementDigestSummary(digests, ctx);

        for (const auto& entry : digests) {
            uint64_t total = 0;
            for (uint64_t count : entry.histogram_counts) {
                total += count;
            }
            uint64_t cumulative = 0;

            for (size_t i = 0; i < entry.histogram_counts.size(); ++i) {
                uint64_t count_bucket = entry.histogram_counts[i];
                cumulative += count_bucket;
                double quantile = total == 0 ? 0.0
                    : static_cast<double>(cumulative) / static_cast<double>(total);

                VirtualRow row;
                row.columns = {
                    {"SCHEMA_NAME", entry.schema_name.empty() ? TypedValue() : TypedValue::makeVarchar(entry.schema_name)},
                    {"DIGEST", TypedValue::makeVarchar(entry.digest)},
                    {"BUCKET_NUMBER", TypedValue::makeInt64(static_cast<int64_t>(i))},
                    {"BUCKET_TIMER_LOW", TypedValue::makeInt64(static_cast<int64_t>(CatalogManager::digestHistogramLowerBound(i)))},
                    {"BUCKET_TIMER_HIGH", TypedValue::makeInt64(static_cast<int64_t>(CatalogManager::digestHistogramUpperBound(i)))},
                    {"COUNT_BUCKET", TypedValue::makeInt64(static_cast<int64_t>(count_bucket))},
                    {"COUNT_BUCKET_AND_LOWER", TypedValue::makeInt64(static_cast<int64_t>(cumulative))},
                    {"BUCKET_QUANTILE", TypedValue::makeFloat64(quantile)}
                };
                results.rows.push_back(std::move(row));
            }
        }

        return Status::OK;
    }

    Status queryEventsStatementsHistogramGlobal(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::array<uint64_t, CatalogManager::kDigestHistogramBuckets> counts{};
        catalog_manager_->getStatementDigestHistogramGlobal(counts, ctx);

        uint64_t total = 0;
        for (uint64_t count : counts) {
            total += count;
        }

        uint64_t cumulative = 0;
        for (size_t i = 0; i < counts.size(); ++i) {
            uint64_t count_bucket = counts[i];
            cumulative += count_bucket;
            double quantile = total == 0 ? 0.0
                : static_cast<double>(cumulative) / static_cast<double>(total);

            VirtualRow row;
            row.columns = {
                {"BUCKET_NUMBER", TypedValue::makeInt64(static_cast<int64_t>(i))},
                {"BUCKET_TIMER_LOW", TypedValue::makeInt64(static_cast<int64_t>(CatalogManager::digestHistogramLowerBound(i)))},
                {"BUCKET_TIMER_HIGH", TypedValue::makeInt64(static_cast<int64_t>(CatalogManager::digestHistogramUpperBound(i)))},
                {"COUNT_BUCKET", TypedValue::makeInt64(static_cast<int64_t>(count_bucket))},
                {"COUNT_BUCKET_AND_LOWER", TypedValue::makeInt64(static_cast<int64_t>(cumulative))},
                {"BUCKET_QUANTILE", TypedValue::makeFloat64(quantile)}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryEventsStatements(VirtualResultSet& results, ErrorContext* ctx, bool active_only) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<ProcessControlBlock> backends;
        core::ErrorContext proc_ctx;
        if (ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) != Status::OK) {
            return Status::OK;
        }

        auto now_micros = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        for (const auto& backend : backends) {
            if (active_only && backend.query_start_time == 0) {
                continue;
            }

            uint64_t start_time = backend.query_start_time != 0
                ? backend.query_start_time
                : backend.state_change_time;
            if (start_time == 0) {
                start_time = backend.start_time;
            }

            int64_t event_id = static_cast<int64_t>(
                backend.query_start_time != 0 ? backend.query_start_time : backend.proc_id);
            int64_t timer_wait = 0;
            if (start_time != 0 && now_micros >= start_time) {
                timer_wait = static_cast<int64_t>(now_micros - start_time);
            }

            TypedValue sql_text;
            if (backend.query_text[0] != '\0') {
                sql_text = TypedValue::makeText(backend.query_text);
            }

            VirtualRow row;
            row.columns = {
                {"THREAD_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id))},
                {"EVENT_ID", TypedValue::makeInt64(event_id)},
                {"END_EVENT_ID", TypedValue()},
                {"EVENT_NAME", TypedValue::makeVarchar("statement/sql/exec")},
                {"TIMER_START", start_time == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(start_time))},
                {"TIMER_END", TypedValue::makeInt64(static_cast<int64_t>(now_micros))},
                {"TIMER_WAIT", TypedValue::makeInt64(timer_wait)},
                {"LOCK_TIME", TypedValue()},
                {"SQL_TEXT", sql_text},
                {"CURRENT_SCHEMA", TypedValue::makeVarchar("scratchbird")},
                {"DIGEST", TypedValue()},
                {"DIGEST_TEXT", TypedValue()},
                {"MYSQL_ERRNO", TypedValue()},
                {"RETURNED_SQLSTATE", TypedValue()},
                {"MESSAGE_TEXT", TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryEventsTransactionsCurrent(VirtualResultSet& results, ErrorContext* ctx) {
        return queryEventsTransactions(results, ctx, true);
    }

    Status queryEventsTransactionsHistoryLong(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::TransactionHistoryEntry> history;
        catalog_manager_->listTransactionHistory(history, ctx);

        for (const auto& entry : history) {
            VirtualRow row;
            row.columns = {
                {"THREAD_ID", TypedValue::makeInt64(static_cast<int64_t>(entry.thread_id))},
                {"EVENT_ID", TypedValue::makeInt64(static_cast<int64_t>(entry.event_id))},
                {"END_EVENT_ID", entry.end_event_id == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.end_event_id))},
                {"EVENT_NAME", TypedValue::makeVarchar("transaction")},
                {"STATE", TypedValue::makeVarchar(entry.committed ? "COMMITTED" : "ROLLED BACK")},
                {"TRX_ID", entry.trx_id == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.trx_id))},
                {"SOURCE", TypedValue::makeVarchar("scratchbird")},
                {"TIMER_START", entry.timer_start == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.timer_start))},
                {"TIMER_END", entry.timer_end == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.timer_end))},
                {"TIMER_WAIT", entry.timer_wait == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.timer_wait))},
                {"ACCESS_MODE", TypedValue::makeVarchar(entry.read_only ? "READ ONLY" : "READ WRITE")},
                {"ISOLATION_LEVEL", TypedValue::makeVarchar(mysqlIsolationLevelName(entry.isolation_level))},
                {"AUTOCOMMIT", TypedValue::makeVarchar(entry.autocommit ? "YES" : "NO")}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryEventsTransactions(VirtualResultSet& results, ErrorContext* ctx, bool active_only) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        if (!active_only) {
            // History table mirrors the current snapshot until we track completed transactions.
        }

        std::vector<ProcessControlBlock> backends;
        core::ErrorContext proc_ctx;
        if (ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) != Status::OK) {
            return Status::OK;
        }

        auto now_micros = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        for (const auto& backend : backends) {
            if (backend.xid == 0) {
                continue;
            }

            uint64_t start_time = backend.xact_start_time != 0
                ? backend.xact_start_time
                : backend.start_time;
            int64_t event_id = static_cast<int64_t>(
                start_time != 0 ? start_time : backend.proc_id);
            int64_t timer_wait = 0;
            if (start_time != 0 && now_micros >= start_time) {
                timer_wait = static_cast<int64_t>(now_micros - start_time);
            }

            VirtualRow row;
            row.columns = {
                {"THREAD_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id))},
                {"EVENT_ID", TypedValue::makeInt64(event_id)},
                {"END_EVENT_ID", TypedValue()},
                {"EVENT_NAME", TypedValue::makeVarchar("transaction")},
                {"STATE", TypedValue::makeVarchar("ACTIVE")},
                {"TRX_ID", TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
                {"SOURCE", TypedValue::makeVarchar("scratchbird")},
                {"TIMER_START", start_time == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(start_time))},
                {"TIMER_END", TypedValue::makeInt64(static_cast<int64_t>(now_micros))},
                {"TIMER_WAIT", TypedValue::makeInt64(timer_wait)},
                {"ACCESS_MODE", TypedValue::makeVarchar(backend.is_read_only ? "READ ONLY" : "READ WRITE")},
                {"ISOLATION_LEVEL", TypedValue::makeVarchar(mysqlIsolationLevelName(backend.isolation_level))},
                {"AUTOCOMMIT", TypedValue::makeVarchar("YES")}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryMetadataLocks(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<LockSnapshot> locks;
        if (catalog_manager_->listLocks(locks, ctx) != Status::OK) {
            return Status::OK;
        }

        std::unordered_map<uint32_t, uint64_t> event_id_by_proc;
        std::vector<ProcessControlBlock> backends;
        core::ErrorContext proc_ctx;
        if (ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) == Status::OK) {
            for (const auto& backend : backends) {
                uint64_t event_id = backend.query_start_time != 0
                    ? backend.query_start_time
                    : backend.proc_id;
                event_id_by_proc[backend.proc_id] = event_id;
            }
        }

        std::unordered_map<ID, std::pair<std::string, std::string>, IDHash> table_lookup;

        for (const auto& lock : locks) {
            std::string object_schema;
            std::string object_name;

            if (lock.tag.target_type == LockTarget::LOCK_TARGET_DATABASE) {
                object_schema = "scratchbird";
            } else if (!isZeroId(lock.tag.object_uuid)) {
                auto it = table_lookup.find(lock.tag.object_uuid);
                if (it != table_lookup.end()) {
                    object_schema = it->second.first;
                    object_name = it->second.second;
                } else {
                    CatalogManager::TableInfo table_info;
                    if (catalog_manager_->getTable(lock.tag.object_uuid, table_info, ctx) == Status::OK) {
                        CatalogManager::SchemaInfo schema_info;
                        if (catalog_manager_->getSchema(table_info.schema_id, schema_info, ctx) == Status::OK) {
                            object_schema = schemaName(schema_info);
                        }
                        object_name = table_info.table_name;
                        table_lookup.emplace(lock.tag.object_uuid,
                                             std::pair<std::string, std::string>{object_schema, object_name});
                    }
                }
            }

            TypedValue object_schema_val = object_schema.empty()
                ? TypedValue()
                : TypedValue::makeVarchar(object_schema);
            TypedValue object_name_val = object_name.empty()
                ? TypedValue()
                : TypedValue::makeVarchar(object_name);

            TypedValue instance_val;
            if (lock.request_time != 0) {
                instance_val = TypedValue::makeInt64(static_cast<int64_t>(lock.request_time));
            }

            TypedValue owner_event_val;
            auto event_it = event_id_by_proc.find(lock.proc_id);
            if (event_it != event_id_by_proc.end()) {
                owner_event_val = TypedValue::makeInt64(static_cast<int64_t>(event_it->second));
            }

            VirtualRow row;
            row.columns = {
                {"OBJECT_TYPE", TypedValue::makeVarchar(mysqlMetadataObjectType(lock.tag.target_type))},
                {"OBJECT_SCHEMA", object_schema_val},
                {"OBJECT_NAME", object_name_val},
                {"COLUMN_NAME", TypedValue()},
                {"OBJECT_INSTANCE_BEGIN", instance_val},
                {"LOCK_TYPE", TypedValue::makeVarchar(mysqlLockType(lock.mode))},
                {"LOCK_DURATION", TypedValue::makeVarchar("TRANSACTION")},
                {"LOCK_STATUS", TypedValue::makeVarchar(lock.granted ? "GRANTED" : "PENDING")},
                {"SOURCE", TypedValue::makeVarchar("scratchbird")},
                {"OWNER_THREAD_ID", lock.proc_id == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(lock.proc_id))},
                {"OWNER_EVENT_ID", owner_event_val}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryEventsWaitsCurrent(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<LockSnapshot> locks;
        if (catalog_manager_->listLocks(locks, ctx) != Status::OK) {
            return Status::OK;
        }

        auto now_micros = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());

        for (const auto& lock : locks) {
            if (lock.granted) {
                continue;
            }

            uint64_t start_time = lock.request_time;
            int64_t event_id = static_cast<int64_t>(
                start_time != 0 ? start_time : lock.proc_id);
            int64_t timer_wait = 0;
            if (start_time != 0 && now_micros >= start_time) {
                timer_wait = static_cast<int64_t>(now_micros - start_time);
            }

            VirtualRow row;
            row.columns = {
                {"THREAD_ID", TypedValue::makeInt64(static_cast<int64_t>(lock.proc_id))},
                {"EVENT_ID", TypedValue::makeInt64(event_id)},
                {"END_EVENT_ID", TypedValue()},
                {"EVENT_NAME", TypedValue::makeVarchar("wait/lock/metadata")},
                {"SOURCE", TypedValue::makeVarchar("scratchbird")},
                {"TIMER_START", start_time == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(start_time))},
                {"TIMER_END", TypedValue::makeInt64(static_cast<int64_t>(now_micros))},
                {"TIMER_WAIT", TypedValue::makeInt64(timer_wait)},
                {"SPINS", TypedValue()},
                {"OBJECT_INSTANCE_BEGIN", start_time == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(start_time))},
                {"NESTING_EVENT_ID", TypedValue()},
                {"NESTING_EVENT_TYPE", TypedValue()},
                {"OPERATION", TypedValue()},
                {"NUMBER_OF_BYTES", TypedValue()},
                {"FLAGS", TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }

    Status queryEventsWaitsHistoryLong(VirtualResultSet& results, ErrorContext* ctx) {
        if (!catalog_manager_) {
            return Status::OK;
        }

        std::vector<CatalogManager::WaitHistoryEntry> history;
        catalog_manager_->listWaitHistory(history, ctx);

        for (const auto& entry : history) {
            VirtualRow row;
            row.columns = {
                {"THREAD_ID", TypedValue::makeInt64(static_cast<int64_t>(entry.thread_id))},
                {"EVENT_ID", TypedValue::makeInt64(static_cast<int64_t>(entry.event_id))},
                {"END_EVENT_ID", entry.timer_end == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.timer_end))},
                {"EVENT_NAME", TypedValue::makeVarchar("wait/lock/metadata")},
                {"SOURCE", TypedValue::makeVarchar("scratchbird")},
                {"TIMER_START", entry.timer_start == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.timer_start))},
                {"TIMER_END", entry.timer_end == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.timer_end))},
                {"TIMER_WAIT", entry.timer_wait == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.timer_wait))},
                {"SPINS", TypedValue()},
                {"OBJECT_INSTANCE_BEGIN", entry.object_instance_begin == 0 ? TypedValue() : TypedValue::makeInt64(static_cast<int64_t>(entry.object_instance_begin))},
                {"NESTING_EVENT_ID", TypedValue()},
                {"NESTING_EVENT_TYPE", TypedValue()},
                {"OPERATION", TypedValue()},
                {"NUMBER_OF_BYTES", TypedValue()},
                {"FLAGS", entry.timed_out ? TypedValue::makeVarchar("TIMEOUT") : TypedValue()}
            };
            results.rows.push_back(std::move(row));
        }

        return Status::OK;
    }
};

} // namespace scratchbird::catalog
