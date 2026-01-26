/**
 * ScratchBird sys.* Catalog Handler
 *
 * Provides sys.jobs, sys.job_runs, sys.job_dependencies virtual tables.
 */

#include "scratchbird/catalog/sys_catalog.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/uuidv7.h"
#include <chrono>
#include <unordered_map>

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

core::TypedValue timestampValueOrNull(uint64_t value) {
    if (value == 0) {
        return core::TypedValue::makeNull(core::DataType::TIMESTAMP);
    }
    return core::TypedValue::makeTimestamp(static_cast<int64_t>(value));
}

std::string isolationLevelToString(uint8_t isolation_level) {
    switch (static_cast<core::IsolationLevel>(isolation_level)) {
        case core::IsolationLevel::READ_COMMITTED:
        case core::IsolationLevel::READ_COMMITTED_READ_CONSISTENCY:
            return "read_committed";
        case core::IsolationLevel::SNAPSHOT:
            return "repeatable_read";
        case core::IsolationLevel::SNAPSHOT_TABLE_STABILITY:
            return "serializable";
        default:
            return "";
    }
}

std::string lockModeToString(core::LockMode mode) {
    switch (mode) {
        case core::LockMode::LOCK_ACCESS_SHARE: return "access_share";
        case core::LockMode::LOCK_ROW_SHARE: return "row_share";
        case core::LockMode::LOCK_ROW_EXCLUSIVE: return "row_exclusive";
        case core::LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE: return "share_update_exclusive";
        case core::LockMode::LOCK_SHARE: return "share";
        case core::LockMode::LOCK_SHARE_ROW_EXCLUSIVE: return "share_row_exclusive";
        case core::LockMode::LOCK_EXCLUSIVE: return "exclusive";
        case core::LockMode::LOCK_ACCESS_EXCLUSIVE: return "access_exclusive";
        default: return "";
    }
}

std::string lockTargetToString(core::LockTarget target) {
    switch (target) {
        case core::LockTarget::LOCK_TARGET_DATABASE: return "database";
        case core::LockTarget::LOCK_TARGET_TABLE: return "table";
        case core::LockTarget::LOCK_TARGET_PAGE: return "page";
        case core::LockTarget::LOCK_TARGET_TUPLE: return "tuple";
        default: return "";
    }
}

std::string baseNameFromPath(const std::string& path) {
    size_t last_slash = path.find_last_of("/\\");
    if (last_slash == std::string::npos) {
        return path;
    }
    return path.substr(last_slash + 1);
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
        "sessions",
        "transactions",
        "locks",
        "statements",
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

    static const ColumnDefs kSessionsColumns = {
        {"session_id", DataType::UUID, false},
        {"connection_id", DataType::INT64, false},
        {"user_name", DataType::TEXT, true},
        {"role_name", DataType::TEXT, true},
        {"database_name", DataType::TEXT, true},
        {"protocol", DataType::TEXT, true},
        {"client_addr", DataType::TEXT, true},
        {"client_port", DataType::INT32, true},
        {"state", DataType::TEXT, true},
        {"connected_at", DataType::TIMESTAMP, true},
        {"last_activity_at", DataType::TIMESTAMP, true},
        {"transaction_id", DataType::INT64, true},
        {"statement_id", DataType::INT64, true},
        {"current_query", DataType::TEXT, true},
        {"wait_event", DataType::TEXT, true},
        {"wait_resource", DataType::TEXT, true}
    };

    static const ColumnDefs kTransactionsColumns = {
        {"transaction_id", DataType::INT64, false},
        {"transaction_uuid", DataType::UUID, true},
        {"session_id", DataType::UUID, true},
        {"state", DataType::TEXT, true},
        {"isolation_level", DataType::TEXT, true},
        {"read_only", DataType::BOOLEAN, true},
        {"start_time", DataType::TIMESTAMP, true},
        {"duration_ms", DataType::INT64, true},
        {"current_query", DataType::TEXT, true},
        {"wait_event", DataType::TEXT, true},
        {"locks_held", DataType::INT32, true},
        {"pages_modified", DataType::INT32, true},
        {"distributed", DataType::BOOLEAN, true},
        {"coordinator_uuid", DataType::UUID, true}
    };

    static const ColumnDefs kLocksColumns = {
        {"lock_id", DataType::INT64, false},
        {"lock_type", DataType::TEXT, true},
        {"lock_mode", DataType::TEXT, true},
        {"granted", DataType::BOOLEAN, true},
        {"lock_state", DataType::TEXT, true},
        {"database_uuid", DataType::UUID, true},
        {"relation_uuid", DataType::UUID, true},
        {"relation_name", DataType::TEXT, true},
        {"page", DataType::INT64, true},
        {"tuple", DataType::INT64, true},
        {"transaction_id", DataType::INT64, true},
        {"session_id", DataType::UUID, true},
        {"virtual_xid", DataType::TEXT, true},
        {"grant_time", DataType::TIMESTAMP, true},
        {"wait_start", DataType::TIMESTAMP, true}
    };

    static const ColumnDefs kStatementsColumns = {
        {"statement_id", DataType::INT64, false},
        {"session_id", DataType::UUID, true},
        {"transaction_id", DataType::INT64, true},
        {"state", DataType::TEXT, true},
        {"sql_text", DataType::TEXT, true},
        {"start_time", DataType::TIMESTAMP, true},
        {"elapsed_ms", DataType::INT64, true},
        {"rows_processed", DataType::INT64, true},
        {"wait_event", DataType::TEXT, true},
        {"wait_resource", DataType::TEXT, true}
    };

    if (equalsCaseInsensitive(table_name, "sessions")) {
        return &kSessionsColumns;
    }
    if (equalsCaseInsensitive(table_name, "transactions")) {
        return &kTransactionsColumns;
    }
    if (equalsCaseInsensitive(table_name, "locks")) {
        return &kLocksColumns;
    }
    if (equalsCaseInsensitive(table_name, "statements")) {
        return &kStatementsColumns;
    }
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

    if (equalsCaseInsensitive(table_name, "sessions")) {
        return querySessions(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "transactions")) {
        return queryTransactions(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "locks")) {
        return queryLocks(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "statements")) {
        return queryStatements(results, ctx);
    }
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

Status SysCatalogHandler::querySessions(VirtualResultSet& results, ErrorContext* ctx) {
    std::unordered_map<core::ID, core::CatalogManager::SessionInfo, core::IDHash> sessions_by_id;
    if (catalog_manager_) {
        std::vector<core::CatalogManager::SessionInfo> sessions;
        Status status = catalog_manager_->listSessions(sessions, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }
        for (const auto& session : sessions) {
            sessions_by_id.emplace(session.session_id, session);
        }
    }

    std::vector<core::ProcessControlBlock> backends;
    core::ErrorContext proc_ctx;
    Status backend_status = core::ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx);
    if (backend_status != Status::OK) {
        return Status::OK;
    }

    core::ConnectionContext* conn_ctx = core::ConnectionContext::getCurrent();
    bool allow_all = !conn_ctx || conn_ctx->isSuperuser();
    core::ID current_session_id = conn_ctx ? conn_ctx->effectiveSessionId() : core::ID{};

    std::string database_name;
    if (catalog_manager_ && catalog_manager_->database()) {
        database_name = baseNameFromPath(catalog_manager_->database()->path());
    }

    for (const auto& backend : backends) {
        const core::ID& session_id = backend.session_id;
        if (isZeroId(session_id)) {
            continue;
        }
        if (!allow_all) {
            if (isZeroId(current_session_id) || session_id != current_session_id) {
                continue;
            }
        }

        const core::CatalogManager::SessionInfo* session_info = nullptr;
        auto it = sessions_by_id.find(session_id);
        if (it != sessions_by_id.end()) {
            session_info = &it->second;
        }

        std::string role_name;
        if (session_info && !session_info->effective_roles.empty()) {
            core::CatalogManager::RoleInfo role_info;
            if (catalog_manager_ &&
                catalog_manager_->getRole(session_info->effective_roles.front(),
                                          role_info, ctx) == Status::OK) {
                role_name = role_info.role_name;
            }
        }

        std::string query_text;
        if (backend.query_text[0] != '\0') {
            query_text = backend.query_text;
        }

        std::string state;
        if (backend.wait_lock_id != 0) {
            state = "waiting";
        } else if (backend.query_start_time != 0) {
            state = "active";
        } else if (backend.xid != 0) {
            state = "idle_in_txn";
        } else {
            state = "idle";
        }

        std::string protocol = "scratchbird";
        if (session_info && !session_info->emulation_mode.empty()) {
            protocol = session_info->emulation_mode;
        }

        uint64_t connected_at = session_info ? session_info->login_time : backend.start_time;
        if (connected_at == 0) {
            connected_at = backend.start_time;
        }
        uint64_t last_activity = session_info ? session_info->last_activity_time
                                              : backend.state_change_time;
        if (last_activity == 0) {
            last_activity = backend.state_change_time;
        }

        VirtualRow row;
        row.columns = {
            {"session_id", uuidValueOrNull(session_id)},
            {"connection_id", core::TypedValue::makeInt64(static_cast<int64_t>(backend.proc_id + 1))},
            {"user_name", session_info ? textValueOrNull(session_info->username, DataType::TEXT)
                                       : core::TypedValue::makeNull(DataType::TEXT)},
            {"role_name", textValueOrNull(role_name, DataType::TEXT)},
            {"database_name", textValueOrNull(database_name, DataType::TEXT)},
            {"protocol", textValueOrNull(protocol, DataType::TEXT)},
            {"client_addr", core::TypedValue::makeNull(DataType::TEXT)},
            {"client_port", core::TypedValue::makeNull(DataType::INT32)},
            {"state", textValueOrNull(state, DataType::TEXT)},
            {"connected_at", timestampValueOrNull(connected_at)},
            {"last_activity_at", timestampValueOrNull(last_activity)},
            {"transaction_id", backend.xid == 0
                                  ? core::TypedValue::makeNull(DataType::INT64)
                                  : core::TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
            {"statement_id", backend.query_start_time == 0
                                ? core::TypedValue::makeNull(DataType::INT64)
                                : core::TypedValue::makeInt64(
                                      static_cast<int64_t>(backend.query_start_time))},
            {"current_query", textValueOrNull(query_text, DataType::TEXT)},
            {"wait_event", backend.wait_lock_id == 0
                              ? core::TypedValue::makeNull(DataType::TEXT)
                              : core::TypedValue::makeText("lock")},
            {"wait_resource", backend.wait_lock_id == 0
                                 ? core::TypedValue::makeNull(DataType::TEXT)
                                 : core::TypedValue::makeText(
                                       std::to_string(backend.wait_lock_id))}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryTransactions(VirtualResultSet& results, ErrorContext* ctx) {
    (void)ctx;
    std::vector<core::ProcessControlBlock> backends;
    core::ErrorContext proc_ctx;
    Status backend_status = core::ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx);
    if (backend_status != Status::OK) {
        return Status::OK;
    }

    core::ConnectionContext* conn_ctx = core::ConnectionContext::getCurrent();
    bool allow_all = !conn_ctx || conn_ctx->isSuperuser();
    core::ID current_session_id = conn_ctx ? conn_ctx->effectiveSessionId() : core::ID{};

    std::unordered_map<uint32_t, uint32_t> lock_counts;
    if (catalog_manager_ && catalog_manager_->database() &&
        catalog_manager_->database()->lock_manager()) {
        std::vector<core::LockSnapshot> locks;
        if (catalog_manager_->database()->lock_manager()->listLocks(locks) == Status::OK) {
            for (const auto& lock : locks) {
                if (!lock.granted) {
                    continue;
                }
                lock_counts[lock.proc_id]++;
            }
        }
    }

    uint64_t now_micros = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    for (const auto& backend : backends) {
        if (backend.xid == 0) {
            continue;
        }
        if (!allow_all) {
            if (isZeroId(current_session_id) || backend.session_id != current_session_id) {
                continue;
            }
        }

        std::string query_text;
        if (backend.query_text[0] != '\0') {
            query_text = backend.query_text;
        }

        uint64_t duration_ms = 0;
        if (backend.xact_start_time != 0 && now_micros > backend.xact_start_time) {
            duration_ms = (now_micros - backend.xact_start_time) / 1000;
        }

        std::string state = backend.wait_lock_id != 0 ? "waiting" : "active";

        VirtualRow row;
        row.columns = {
            {"transaction_id", core::TypedValue::makeInt64(static_cast<int64_t>(backend.xid))},
            {"transaction_uuid", core::TypedValue::makeNull(DataType::UUID)},
            {"session_id", uuidValueOrNull(backend.session_id)},
            {"state", textValueOrNull(state, DataType::TEXT)},
            {"isolation_level", textValueOrNull(isolationLevelToString(backend.isolation_level),
                                                DataType::TEXT)},
            {"read_only", core::TypedValue::makeBool(backend.is_read_only)},
            {"start_time", timestampValueOrNull(backend.xact_start_time)},
            {"duration_ms", duration_ms == 0
                                ? core::TypedValue::makeNull(DataType::INT64)
                                : core::TypedValue::makeInt64(static_cast<int64_t>(duration_ms))},
            {"current_query", textValueOrNull(query_text, DataType::TEXT)},
            {"wait_event", backend.wait_lock_id == 0
                              ? core::TypedValue::makeNull(DataType::TEXT)
                              : core::TypedValue::makeText("lock")},
            {"locks_held", core::TypedValue::makeInt32(
                               static_cast<int32_t>(lock_counts[backend.proc_id]))},
            {"pages_modified", core::TypedValue::makeInt32(0)},
            {"distributed", core::TypedValue::makeBool(false)},
            {"coordinator_uuid", core::TypedValue::makeNull(DataType::UUID)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryLocks(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_ || !catalog_manager_->database() ||
        !catalog_manager_->database()->lock_manager()) {
        return Status::OK;
    }

    std::vector<core::LockSnapshot> locks;
    if (catalog_manager_->database()->lock_manager()->listLocks(locks) != Status::OK) {
        return Status::OK;
    }

    std::unordered_map<uint32_t, core::ProcessControlBlock> backends_by_proc;
    std::vector<core::ProcessControlBlock> backends;
    core::ErrorContext proc_ctx;
    if (core::ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx) == Status::OK) {
        for (const auto& backend : backends) {
            backends_by_proc.emplace(backend.proc_id, backend);
        }
    }

    core::ConnectionContext* conn_ctx = core::ConnectionContext::getCurrent();
    bool allow_all = !conn_ctx || conn_ctx->isSuperuser();
    core::ID current_session_id = conn_ctx ? conn_ctx->effectiveSessionId() : core::ID{};

    core::ID database_uuid{};
    if (catalog_manager_->database()) {
        database_uuid = catalog_manager_->database()->uuid();
    }

    int64_t lock_id_counter = 1;
    for (const auto& lock : locks) {
        core::ID session_id{};
        uint64_t transaction_id = 0;
        auto backend_it = backends_by_proc.find(lock.proc_id);
        if (backend_it != backends_by_proc.end()) {
            session_id = backend_it->second.session_id;
            transaction_id = backend_it->second.xid;
        }

        if (!allow_all) {
            if (isZeroId(current_session_id) || session_id != current_session_id) {
                continue;
            }
        }

        core::ID relation_uuid{};
        std::string relation_name;
        if (!isZeroId(lock.tag.object_uuid)) {
            relation_uuid = lock.tag.object_uuid;
            core::CatalogManager::TableInfo table_info;
            if (catalog_manager_->getTable(relation_uuid, table_info, ctx) == Status::OK) {
                relation_name = table_info.table_name;
            } else {
                core::CatalogManager::IndexInfo index_info;
                if (catalog_manager_->getIndex(relation_uuid, index_info, ctx) == Status::OK) {
                    relation_name = index_info.index_name;
                }
            }
        }

        VirtualRow row;
        row.columns = {
            {"lock_id", core::TypedValue::makeInt64(lock_id_counter++)},
            {"lock_type", textValueOrNull(lockTargetToString(lock.tag.target_type), DataType::TEXT)},
            {"lock_mode", textValueOrNull(lockModeToString(lock.mode), DataType::TEXT)},
            {"granted", core::TypedValue::makeBool(lock.granted)},
            {"lock_state", textValueOrNull(lock.granted ? "granted" : "waiting", DataType::TEXT)},
            {"database_uuid", uuidValueOrNull(database_uuid)},
            {"relation_uuid", uuidValueOrNull(relation_uuid)},
            {"relation_name", textValueOrNull(relation_name, DataType::TEXT)},
            {"page", lock.tag.page_num == 0
                        ? core::TypedValue::makeNull(DataType::INT64)
                        : core::TypedValue::makeInt64(static_cast<int64_t>(lock.tag.page_num))},
            {"tuple", lock.tag.offset_num == 0
                         ? core::TypedValue::makeNull(DataType::INT64)
                         : core::TypedValue::makeInt64(static_cast<int64_t>(lock.tag.offset_num))},
            {"transaction_id", transaction_id == 0
                                   ? core::TypedValue::makeNull(DataType::INT64)
                                   : core::TypedValue::makeInt64(
                                         static_cast<int64_t>(transaction_id))},
            {"session_id", uuidValueOrNull(session_id)},
            {"virtual_xid", core::TypedValue::makeNull(DataType::TEXT)},
            {"grant_time", lock.granted
                               ? core::TypedValue::makeNull(DataType::TIMESTAMP)
                               : core::TypedValue::makeNull(DataType::TIMESTAMP)},
            {"wait_start", lock.granted
                               ? core::TypedValue::makeNull(DataType::TIMESTAMP)
                               : timestampValueOrNull(lock.request_time)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryStatements(VirtualResultSet& results, ErrorContext* ctx) {
    (void)ctx;
    std::vector<core::ProcessControlBlock> backends;
    core::ErrorContext proc_ctx;
    Status backend_status = core::ProcArrayManager::getAllActiveBackends(&backends, &proc_ctx);
    if (backend_status != Status::OK) {
        return Status::OK;
    }

    core::ConnectionContext* conn_ctx = core::ConnectionContext::getCurrent();
    bool allow_all = !conn_ctx || conn_ctx->isSuperuser();
    core::ID current_session_id = conn_ctx ? conn_ctx->effectiveSessionId() : core::ID{};

    uint64_t now_micros = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    for (const auto& backend : backends) {
        if (backend.query_start_time == 0) {
            continue;
        }
        if (!allow_all) {
            if (isZeroId(current_session_id) || backend.session_id != current_session_id) {
                continue;
            }
        }

        std::string query_text;
        if (backend.query_text[0] != '\0') {
            query_text = backend.query_text;
        }

        uint64_t elapsed_ms = 0;
        if (now_micros > backend.query_start_time) {
            elapsed_ms = (now_micros - backend.query_start_time) / 1000;
        }

        std::string state = backend.wait_lock_id != 0 ? "waiting" : "running";

        VirtualRow row;
        row.columns = {
            {"statement_id", core::TypedValue::makeInt64(
                                 static_cast<int64_t>(backend.query_start_time))},
            {"session_id", uuidValueOrNull(backend.session_id)},
            {"transaction_id", backend.xid == 0
                                   ? core::TypedValue::makeNull(DataType::INT64)
                                   : core::TypedValue::makeInt64(
                                         static_cast<int64_t>(backend.xid))},
            {"state", textValueOrNull(state, DataType::TEXT)},
            {"sql_text", textValueOrNull(query_text, DataType::TEXT)},
            {"start_time", timestampValueOrNull(backend.query_start_time)},
            {"elapsed_ms", core::TypedValue::makeInt64(static_cast<int64_t>(elapsed_ms))},
            {"rows_processed", core::TypedValue::makeInt64(0)},
            {"wait_event", backend.wait_lock_id == 0
                              ? core::TypedValue::makeNull(DataType::TEXT)
                              : core::TypedValue::makeText("lock")},
            {"wait_resource", backend.wait_lock_id == 0
                                 ? core::TypedValue::makeNull(DataType::TEXT)
                                 : core::TypedValue::makeText(
                                       std::to_string(backend.wait_lock_id))}
        };
        results.rows.push_back(std::move(row));
    }

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

    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    std::string db_name;
    if (db) {
        db_name = baseNameFromPath(db->path());
    }
    const std::string metric_db = db_name.empty() ? "default" : db_name;

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
            {"database_name", db_name.empty()
                                  ? core::TypedValue::makeNull(DataType::TEXT)
                                  : core::TypedValue::makeText(db_name)},
            {"updated_at", updated_at}
        };
        results.rows.push_back(std::move(row));
    };

    const std::vector<std::string> default_database = {metric_db};
    const std::vector<std::string> query_types = {
        "select", "insert", "update", "delete", "ddl", "other", "merge", "copy"
    };

    if (db) {
        add_row("page_size_bytes", static_cast<double>(db->page_size()), "bytes");
        add_row("allocated_pages", static_cast<double>(db->total_pages()), "count");
        add_row("page_buffers",
                metrics.buffer_pool_pages_total
                    ? metrics.buffer_pool_pages_total->get()
                    : 0.0,
                "count");
        add_row("ods_major", 13.0, "count");
        add_row("ods_minor", 0.0, "count");

        auto* txn_mgr = db->transaction_manager();
        if (txn_mgr) {
            add_row("next_transaction",
                    static_cast<double>(txn_mgr->getCurrentXid()),
                    "count");
            add_row("oldest_transaction",
                    static_cast<double>(txn_mgr->getOldestXid()),
                    "count");
            add_row("oldest_active",
                    static_cast<double>(txn_mgr->getOldestActiveXid()),
                    "count");
            add_row("oldest_snapshot",
                    static_cast<double>(txn_mgr->getOldestSnapshot()),
                    "count");
        }

        static const uint64_t process_start_micros = now_micros();
        uint64_t uptime_micros = now_micros() - process_start_micros;
        add_row("uptime_seconds",
                static_cast<double>(uptime_micros) / 1000000.0,
                "seconds");
    }

    if (db && db->lock_manager()) {
        std::vector<core::LockSnapshot> locks;
        if (db->lock_manager()->listLocks(locks) == Status::OK) {
            uint64_t held = 0;
            for (const auto& lock : locks) {
                if (lock.granted) {
                    held++;
                }
            }
            add_row("locks_held", static_cast<double>(held), "count");
        }
    }

    if (metrics.connections_active) {
        add_row("connections_active", metrics.connections_active->get(), "count");
    }
    if (metrics.connections_idle) {
        add_row("connections_idle", metrics.connections_idle->get(), "count");
    }
    if (metrics.connections_total) {
        add_row("connections_total", metrics.connections_total->get(), "count");
    }
    if (metrics.query_currently_running) {
        add_row("query_currently_running",
                metrics.query_currently_running->get({metric_db}),
                "count");
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
        double total_count = 0.0;
        double total_sum = 0.0;
        for (const auto& type : query_types) {
            total_count += static_cast<double>(
                metrics.query_duration_seconds->count({type, metric_db}));
            total_sum += metrics.query_duration_seconds->sum({type, metric_db});
        }
        double avg_sec = total_count > 0.0 ? total_sum / total_count : 0.0;
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
    if (!db && metrics.locks_held) {
        add_row("locks_held", metrics.locks_held->get({"all"}), "count");
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
        for (const auto& type : query_types) {
            add_row("queries_total{type=" + type + "}",
                    metrics.queries_total->get({type, metric_db}),
                    "count");
        }
    }
    if (metrics.query_errors_total) {
        add_row("query_errors_total",
                metrics.query_errors_total->get({"error", metric_db}),
                "count");
    }
    if (metrics.query_rows_returned_total) {
        add_row("query_rows_returned_total",
                metrics.query_rows_returned_total->get({"select", metric_db}),
                "count");
    }
    if (metrics.query_rows_affected_total) {
        for (const auto& type : {"insert", "update", "delete"}) {
            add_row("query_rows_affected_total{type=" + std::string(type) + "}",
                    metrics.query_rows_affected_total->get({type, metric_db}),
                    "count");
        }
    }
    if (metrics.copy_rows_total) {
        add_row("copy_rows_total{direction=from}",
                metrics.copy_rows_total->get({"from"}),
                "count");
        add_row("copy_rows_total{direction=to}",
                metrics.copy_rows_total->get({"to"}),
                "count");
    }
    if (metrics.copy_bytes_total) {
        add_row("copy_bytes_total{direction=from}",
                metrics.copy_bytes_total->get({"from"}),
                "bytes");
        add_row("copy_bytes_total{direction=to}",
                metrics.copy_bytes_total->get({"to"}),
                "bytes");
    }
    if (metrics.copy_errors_total) {
        add_row("copy_errors_total", metrics.copy_errors_total->get(), "count");
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
