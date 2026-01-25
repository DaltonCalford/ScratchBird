/**
 * ScratchBird sys.* Catalog Handler
 *
 * Provides sys.jobs, sys.job_runs, sys.job_dependencies virtual tables.
 */

#include "scratchbird/catalog/sys_catalog.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/uuidv7.h"
#include <chrono>

namespace scratchbird::catalog {

namespace {

bool isZeroId(const core::ID& id) {
    for (uint8_t byte : id.bytes) {
        if (byte != 0) {
            return false;
        }
    }
    return true;
}

core::TypedValue uuidValueOrNull(const core::ID& id) {
    if (isZeroId(id)) {
        return core::TypedValue::makeNull(core::DataType::UUID);
    }
    std::vector<uint8_t> bytes(id.bytes.begin(), id.bytes.end());
    return core::TypedValue::makeUUID(bytes);
}

core::TypedValue textValueOrNull(const std::string& value, core::DataType type) {
    if (value.empty()) {
        return core::TypedValue::makeNull(type);
    }
    if (type == core::DataType::TEXT) {
        return core::TypedValue::makeText(value);
    }
    return core::TypedValue::makeVarchar(value);
}

core::TypedValue timeValueOrNull(uint64_t value) {
    if (value == 0) {
        return core::TypedValue::makeNull(core::DataType::INT64);
    }
    return core::TypedValue::makeInt64(static_cast<int64_t>(value));
}

std::string jobClassToString(core::CatalogManager::JobClass job_class) {
    switch (job_class) {
        case core::CatalogManager::JobClass::LOCAL_SAFE: return "LOCAL_SAFE";
        case core::CatalogManager::JobClass::LEADER_ONLY: return "LEADER_ONLY";
        case core::CatalogManager::JobClass::QUORUM_REQUIRED: return "QUORUM_REQUIRED";
        default: return "";
    }
}

std::string jobTypeToString(core::CatalogManager::JobType job_type) {
    switch (job_type) {
        case core::CatalogManager::JobType::SQL: return "SQL";
        case core::CatalogManager::JobType::PROCEDURE: return "PROCEDURE";
        case core::CatalogManager::JobType::EXTERNAL: return "EXTERNAL";
        default: return "";
    }
}

std::string scheduleKindToString(core::CatalogManager::ScheduleKind kind) {
    switch (kind) {
        case core::CatalogManager::ScheduleKind::CRON: return "CRON";
        case core::CatalogManager::ScheduleKind::AT: return "AT";
        case core::CatalogManager::ScheduleKind::EVERY: return "EVERY";
        default: return "";
    }
}

std::string onCompletionToString(core::CatalogManager::JobOnCompletion completion) {
    switch (completion) {
        case core::CatalogManager::JobOnCompletion::PRESERVE: return "PRESERVE";
        case core::CatalogManager::JobOnCompletion::DROP: return "DROP";
        default: return "";
    }
}

std::string jobStateToString(core::CatalogManager::JobState state) {
    switch (state) {
        case core::CatalogManager::JobState::ENABLED: return "ENABLED";
        case core::CatalogManager::JobState::DISABLED: return "DISABLED";
        case core::CatalogManager::JobState::PAUSED: return "PAUSED";
        default: return "";
    }
}

std::string jobRunStateToString(core::CatalogManager::JobRunState state) {
    switch (state) {
        case core::CatalogManager::JobRunState::PENDING: return "PENDING";
        case core::CatalogManager::JobRunState::RUNNING: return "RUNNING";
        case core::CatalogManager::JobRunState::COMPLETED: return "COMPLETED";
        case core::CatalogManager::JobRunState::FAILED: return "FAILED";
        case core::CatalogManager::JobRunState::CANCELLED: return "CANCELLED";
        default: return "";
    }
}

}  // namespace

void SysCatalogHandler::initializeTableNames() {
    table_names_ = {
        "jobs",
        "job_runs",
        "job_dependencies",
        "performance"
    };
}

const SysCatalogHandler::ColumnDefs* SysCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const ColumnDefs kJobsColumns = {
        {"job_uuid", DataType::UUID, false},
        {"job_name", DataType::VARCHAR, false},
        {"description", DataType::TEXT, true},
        {"job_class", DataType::VARCHAR, true},
        {"job_type", DataType::VARCHAR, false},
        {"job_sql", DataType::TEXT, true},
        {"procedure_uuid", DataType::UUID, true},
        {"external_command", DataType::TEXT, true},
        {"schedule_kind", DataType::VARCHAR, false},
        {"cron_expression", DataType::TEXT, true},
        {"interval_seconds", DataType::INT64, true},
        {"starts_at", DataType::INT64, true},
        {"ends_at", DataType::INT64, true},
        {"schedule_tz", DataType::VARCHAR, true},
        {"next_run_time", DataType::INT64, true},
        {"on_completion", DataType::VARCHAR, true},
        {"partition_strategy", DataType::VARCHAR, true},
        {"partition_shard_uuid", DataType::UUID, true},
        {"partition_expression", DataType::TEXT, true},
        {"max_retries", DataType::INT32, true},
        {"retry_backoff_seconds", DataType::INT32, true},
        {"timeout_seconds", DataType::INT32, true},
        {"created_by_user_uuid", DataType::UUID, true},
        {"run_as_role_uuid", DataType::UUID, true},
        {"created_at", DataType::INT64, true},
        {"state", DataType::VARCHAR, true}
    };

    static const ColumnDefs kJobRunsColumns = {
        {"job_run_uuid", DataType::UUID, false},
        {"job_uuid", DataType::UUID, false},
        {"assigned_node_uuid", DataType::UUID, true},
        {"shard_uuid", DataType::UUID, true},
        {"scheduled_time", DataType::INT64, true},
        {"started_at", DataType::INT64, true},
        {"completed_at", DataType::INT64, true},
        {"state", DataType::VARCHAR, true},
        {"retry_count", DataType::INT32, true},
        {"result_message", DataType::TEXT, true},
        {"rows_affected", DataType::INT64, true},
        {"result_data", DataType::BYTEA, true},
        {"error_code", DataType::INT32, true}
    };

    static const ColumnDefs kJobDependenciesColumns = {
        {"job_uuid", DataType::UUID, false},
        {"depends_on_job_uuid", DataType::UUID, false}
    };

    static const ColumnDefs kPerformanceColumns = {
        {"metric", DataType::TEXT, false},
        {"value", DataType::FLOAT64, false},
        {"unit", DataType::TEXT, true},
        {"scope", DataType::TEXT, true},
        {"database_name", DataType::TEXT, true},
        {"updated_at", DataType::TIMESTAMP, true}
    };

    if (equalsCaseInsensitive(table_name, "jobs")) {
        return &kJobsColumns;
    }
    if (equalsCaseInsensitive(table_name, "job_runs")) {
        return &kJobRunsColumns;
    }
    if (equalsCaseInsensitive(table_name, "job_dependencies")) {
        return &kJobDependenciesColumns;
    }
    if (equalsCaseInsensitive(table_name, "performance")) {
        return &kPerformanceColumns;
    }

    return nullptr;
}

void SysCatalogHandler::setResultColumns(const ColumnDefs& def, VirtualResultSet& results) {
    results.column_names.clear();
    results.column_types.clear();
    results.column_names.reserve(def.size());
    results.column_types.reserve(def.size());
    for (const auto& col : def) {
        results.column_names.push_back(col.name);
        results.column_types.push_back(col.type);
    }
}

void SysCatalogHandler::setColumnInfo(const ColumnDefs& def,
                                      std::vector<CatalogManager::ColumnInfo>& columns) {
    columns.clear();
    columns.reserve(def.size());
    uint16_t ordinal = 1;
    for (const auto& col : def) {
        CatalogManager::ColumnInfo info;
        info.column_name = col.name;
        info.data_type = static_cast<uint16_t>(col.type);
        info.nullable = col.nullable;
        info.ordinal = ordinal++;
        columns.push_back(std::move(info));
    }
}

Status SysCatalogHandler::queryTable(const std::string& schema_name,
                                     const std::string& table_name,
                                     const std::string& /* where_clause */,
                                     VirtualResultSet& results,
                                     ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }

    if (!ownsTable(schema_name, table_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table not found: sys." + table_name).c_str());
        return Status::NOT_FOUND;
    }

    const ColumnDefs* def = getTableDefinition(table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table definition not found: sys." + table_name).c_str());
        return Status::NOT_FOUND;
    }
    setResultColumns(*def, results);

    if (equalsCaseInsensitive(table_name, "jobs")) {
        return queryJobs(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "job_runs")) {
        return queryJobRuns(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "job_dependencies")) {
        return queryJobDependencies(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "performance")) {
        return queryPerformance(results, ctx);
    }

    return Status::OK;
}

Status SysCatalogHandler::getTableColumns(const std::string& schema_name,
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
                          ("Table not found: sys." + table_name).c_str());
        return Status::NOT_FOUND;
    }

    const ColumnDefs* def = getTableDefinition(table_name);
    if (!def) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Table definition not found: sys." + table_name).c_str());
        return Status::NOT_FOUND;
    }

    setColumnInfo(*def, columns);
    return Status::OK;
}

Status SysCatalogHandler::listTables(const std::string& schema_name,
                                     std::vector<std::string>& table_names,
                                     ErrorContext* ctx) {
    if (!ownsSchema(schema_name)) {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                          ("Schema not found: " + schema_name).c_str());
        return Status::NOT_FOUND;
    }

    table_names = table_names_;
    return Status::OK;
}

Status SysCatalogHandler::listSchemas(std::vector<std::string>& schema_names,
                                      ErrorContext* /* ctx */) {
    schema_names.clear();
    schema_names.push_back("sys");
    return Status::OK;
}

Status SysCatalogHandler::queryJobs(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::JobInfo> jobs;
    Status status = catalog_manager_->listJobs(jobs, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& job : jobs) {
        VirtualRow row;
        row.columns = {
            {"job_uuid", uuidValueOrNull(job.job_id)},
            {"job_name", core::TypedValue::makeVarchar(job.job_name)},
            {"description", textValueOrNull(job.description, DataType::TEXT)},
            {"job_class", textValueOrNull(jobClassToString(job.job_class), DataType::VARCHAR)},
            {"job_type", textValueOrNull(jobTypeToString(job.job_type), DataType::VARCHAR)},
            {"job_sql", textValueOrNull(job.job_sql, DataType::TEXT)},
            {"procedure_uuid", uuidValueOrNull(job.procedure_uuid)},
            {"external_command", textValueOrNull(job.external_command, DataType::TEXT)},
            {"schedule_kind", textValueOrNull(scheduleKindToString(job.schedule_kind), DataType::VARCHAR)},
            {"cron_expression", textValueOrNull(job.cron_expression, DataType::TEXT)},
            {"interval_seconds", job.interval_seconds == 0
                ? core::TypedValue::makeNull(DataType::INT64)
                : core::TypedValue::makeInt64(job.interval_seconds)},
            {"starts_at", timeValueOrNull(job.starts_at)},
            {"ends_at", timeValueOrNull(job.ends_at)},
            {"schedule_tz", textValueOrNull(job.schedule_tz, DataType::VARCHAR)},
            {"next_run_time", timeValueOrNull(job.next_run_time)},
            {"on_completion", textValueOrNull(onCompletionToString(job.on_completion), DataType::VARCHAR)},
            {"partition_strategy", textValueOrNull(job.partition_strategy, DataType::VARCHAR)},
            {"partition_shard_uuid", uuidValueOrNull(job.partition_shard_uuid)},
            {"partition_expression", textValueOrNull(job.partition_expression, DataType::TEXT)},
            {"max_retries", core::TypedValue::makeInt32(static_cast<int32_t>(job.max_retries))},
            {"retry_backoff_seconds", core::TypedValue::makeInt32(static_cast<int32_t>(job.retry_backoff_seconds))},
            {"timeout_seconds", core::TypedValue::makeInt32(static_cast<int32_t>(job.timeout_seconds))},
            {"created_by_user_uuid", uuidValueOrNull(job.created_by_user_uuid)},
            {"run_as_role_uuid", uuidValueOrNull(job.run_as_role_uuid)},
            {"created_at", timeValueOrNull(job.created_at)},
            {"state", textValueOrNull(jobStateToString(job.state), DataType::VARCHAR)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryJobRuns(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::JobRunInfo> runs;
    Status status = catalog_manager_->listJobRuns(runs, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& run : runs) {
        VirtualRow row;
        row.columns = {
            {"job_run_uuid", uuidValueOrNull(run.job_run_id)},
            {"job_uuid", uuidValueOrNull(run.job_id)},
            {"assigned_node_uuid", uuidValueOrNull(run.assigned_node_uuid)},
            {"shard_uuid", uuidValueOrNull(run.shard_uuid)},
            {"scheduled_time", timeValueOrNull(run.scheduled_time)},
            {"started_at", timeValueOrNull(run.started_at)},
            {"completed_at", timeValueOrNull(run.completed_at)},
            {"state", textValueOrNull(jobRunStateToString(run.state), DataType::VARCHAR)},
            {"retry_count", core::TypedValue::makeInt32(static_cast<int32_t>(run.retry_count))},
            {"result_message", textValueOrNull(run.result_message, DataType::TEXT)},
            {"rows_affected", core::TypedValue::makeInt64(run.rows_affected)},
            {"result_data", run.result_data.empty()
                ? core::TypedValue::makeNull(DataType::BYTEA)
                : core::TypedValue::makeBytea(run.result_data)},
            {"error_code", core::TypedValue::makeInt32(run.error_code)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryJobDependencies(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::JobInfo> jobs;
    Status status = catalog_manager_->listJobs(jobs, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& job : jobs) {
        std::vector<core::CatalogManager::JobDependencyInfo> deps;
        Status dep_status = catalog_manager_->listJobDependencies(job.job_id, deps, ctx);
        if (dep_status != Status::OK && dep_status != Status::NOT_FOUND) {
            return dep_status;
        }
        for (const auto& dep : deps) {
            VirtualRow row;
            row.columns = {
                {"job_uuid", uuidValueOrNull(dep.job_id)},
                {"depends_on_job_uuid", uuidValueOrNull(dep.depends_on_job_id)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryPerformance(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto now_micros = []() -> int64_t {
        return static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    };

    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    const std::string scope = "engine";
    const core::TypedValue updated_at = core::TypedValue::makeTimestamp(now_micros());

    auto add_row = [&](const std::string& metric,
                       double value,
                       const std::string& unit) {
        VirtualRow row;
        row.columns = {
            {"metric", core::TypedValue::makeText(metric)},
            {"value", core::TypedValue::makeFloat64(value)},
            {"unit", textValueOrNull(unit, DataType::TEXT)},
            {"scope", core::TypedValue::makeText(scope)},
            {"database_name", core::TypedValue::makeNull(DataType::TEXT)},
            {"updated_at", updated_at}
        };
        results.rows.push_back(std::move(row));
    };

    const std::vector<std::string> default_database = {"default"};

    if (metrics.connections_active) {
        add_row("connections_active", metrics.connections_active->get(), "count");
    }
    if (metrics.connections_idle) {
        add_row("connections_idle", metrics.connections_idle->get(), "count");
    }
    if (metrics.connections_total) {
        add_row("connections_total", metrics.connections_total->get(), "count");
    }
    if (metrics.transactions_total) {
        add_row("transactions_total", metrics.transactions_total->get(default_database), "count");
    }
    if (metrics.transactions_active) {
        add_row("transactions_active", metrics.transactions_active->get(default_database), "count");
    }
    if (metrics.transactions_committed) {
        add_row("transactions_committed_total", metrics.transactions_committed->get(default_database), "count");
    }
    if (metrics.transactions_rolled_back) {
        add_row("transactions_rolled_back_total", metrics.transactions_rolled_back->get(default_database), "count");
    }
    if (metrics.query_duration_seconds) {
        double count = static_cast<double>(metrics.query_duration_seconds->count());
        double avg_sec = count > 0.0 ? metrics.query_duration_seconds->sum() / count : 0.0;
        add_row("query_latency_avg_ms", avg_sec * 1000.0, "ms");
    }
    if (metrics.lock_wait_seconds) {
        add_row("lock_waits_total",
                static_cast<double>(metrics.lock_wait_seconds->count()),
                "count");
        double count = static_cast<double>(metrics.lock_wait_seconds->count());
        double avg_sec = count > 0.0 ? metrics.lock_wait_seconds->sum() / count : 0.0;
        add_row("lock_wait_latency_avg_ms", avg_sec * 1000.0, "ms");
    }
    if (metrics.lock_deadlocks_total) {
        add_row("deadlocks_total", metrics.lock_deadlocks_total->get(), "count");
    }
    if (metrics.locks_held) {
        add_row("locks_held", metrics.locks_held->get(), "count");
    }
    if (metrics.buffer_pool_size_bytes) {
        add_row("buffer_pool_size_bytes", metrics.buffer_pool_size_bytes->get(), "bytes");
    }
    if (metrics.buffer_pool_pages_total) {
        add_row("buffer_pool_pages_total", metrics.buffer_pool_pages_total->get(), "count");
    }
    if (metrics.buffer_pool_pages_dirty) {
        add_row("buffer_pool_pages_dirty", metrics.buffer_pool_pages_dirty->get(), "count");
    }
    if (metrics.buffer_pool_hits_total) {
        add_row("cache_hits_total", metrics.buffer_pool_hits_total->get(), "count");
        add_row("buffer_pool_reads_total{source=cache}", metrics.buffer_pool_hits_total->get(), "count");
    }
    if (metrics.buffer_pool_misses_total) {
        add_row("cache_misses_total", metrics.buffer_pool_misses_total->get(), "count");
    }
    if (metrics.buffer_pool_hits_total && metrics.buffer_pool_misses_total) {
        double hits = metrics.buffer_pool_hits_total->get();
        double misses = metrics.buffer_pool_misses_total->get();
        double ratio = (hits + misses) > 0.0 ? hits / (hits + misses) : 0.0;
        add_row("buffer_pool_hit_ratio", ratio, "ratio");
    }
    if (metrics.buffer_pool_reads_total) {
        add_row("buffer_pool_reads_total{source=disk}", metrics.buffer_pool_reads_total->get(), "count");
        add_row("buffer_pool_disk_reads_total", metrics.buffer_pool_reads_total->get(), "count");
    }
    if (metrics.buffer_pool_writes_total) {
        add_row("buffer_pool_writes_total", metrics.buffer_pool_writes_total->get(), "count");
    }
    if (metrics.queries_total) {
        add_row("queries_total", metrics.queries_total->get(), "count");
    }
    if (metrics.query_errors_total) {
        add_row("query_errors_total", metrics.query_errors_total->get(), "count");
    }
    if (metrics.index_scans_total) {
        add_row("index_scans_total", metrics.index_scans_total->get(), "count");
    }
    if (metrics.seq_scans_total) {
        add_row("seq_scans_total", metrics.seq_scans_total->get(), "count");
    }
    if (metrics.index_scan_duration_seconds) {
        double count = static_cast<double>(metrics.index_scan_duration_seconds->count());
        double avg_sec = count > 0.0 ? metrics.index_scan_duration_seconds->sum() / count : 0.0;
        add_row("index_scan_latency_avg_ms", avg_sec * 1000.0, "ms");
    }
    if (metrics.disk_read_bytes_total) {
        add_row("disk_read_bytes_total", metrics.disk_read_bytes_total->get(), "bytes");
    }
    if (metrics.disk_write_bytes_total) {
        add_row("disk_write_bytes_total", metrics.disk_write_bytes_total->get(), "bytes");
    }
    if (metrics.disk_read_latency_seconds) {
        double count = static_cast<double>(metrics.disk_read_latency_seconds->count());
        double avg_sec = count > 0.0 ? metrics.disk_read_latency_seconds->sum() / count : 0.0;
        add_row("disk_read_latency_avg_ms", avg_sec * 1000.0, "ms");
    }
    if (metrics.disk_write_latency_seconds) {
        double count = static_cast<double>(metrics.disk_write_latency_seconds->count());
        double avg_sec = count > 0.0 ? metrics.disk_write_latency_seconds->sum() / count : 0.0;
        add_row("disk_write_latency_avg_ms", avg_sec * 1000.0, "ms");
    }
    if (metrics.tables_count) {
        add_row("tables_count", metrics.tables_count->get(default_database), "count");
    }
    if (metrics.indexes_count) {
        add_row("indexes_count", metrics.indexes_count->get(default_database), "count");
    }
    if (metrics.toast_reads_total) {
        add_row("toast_reads_total", metrics.toast_reads_total->get(), "count");
    }
    if (metrics.toast_writes_total) {
        add_row("toast_writes_total", metrics.toast_writes_total->get(), "count");
    }
    if (metrics.scheduler_queue_depth) {
        add_row("scheduler_queue_depth", metrics.scheduler_queue_depth->get(), "count");
    }
    if (metrics.scheduler_jobs_running) {
        add_row("scheduler_jobs_running", metrics.scheduler_jobs_running->get(), "count");
    }
    if (metrics.scheduler_jobs_failed_total) {
        add_row("scheduler_jobs_failed_total", metrics.scheduler_jobs_failed_total->get(), "count");
    }
    if (metrics.scheduler_job_run_latency_seconds) {
        double count = static_cast<double>(metrics.scheduler_job_run_latency_seconds->count());
        double avg_sec = count > 0.0 ? metrics.scheduler_job_run_latency_seconds->sum() / count : 0.0;
        add_row("scheduler_job_run_latency_avg_ms", avg_sec * 1000.0, "ms");
    }

    return Status::OK;
}

} // namespace scratchbird::catalog
