/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird sys.* Catalog Handler
 *
 * Provides sys.jobs, sys.job_runs, sys.job_dependencies virtual tables.
 */

#include "scratchbird/catalog/sys_catalog.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/observability_contract.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/lsm_compression.h"
#include "scratchbird/protocol/wire_protocol.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace scratchbird::catalog {

namespace {

using json = nlohmann::json;

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

std::string joinNames(const std::vector<std::string>& values) {
    std::string joined;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            joined.append(",");
        }
        joined.append(values[i]);
    }
    return joined;
}

std::string extractPartitionParentName(const json& metadata) {
    if (!metadata.is_object() || !metadata.contains("partition_parent")) {
        return {};
    }
    const auto& parent = metadata["partition_parent"];
    if (parent.is_string()) {
        return parent.get<std::string>();
    }
    if (parent.is_object()) {
        auto it_name = parent.find("name");
        if (it_name != parent.end() && it_name->is_string()) {
            return it_name->get<std::string>();
        }
    }
    return {};
}

std::string indexStateToString(uint8_t state) {
    switch (static_cast<core::CatalogManager::IndexState>(state)) {
        case core::CatalogManager::IndexState::BUILDING: return "BUILDING";
        case core::CatalogManager::IndexState::ACTIVE: return "ACTIVE";
        case core::CatalogManager::IndexState::RETIRED: return "RETIRED";
        case core::CatalogManager::IndexState::FAILED: return "FAILED";
        case core::CatalogManager::IndexState::INACTIVE: return "INACTIVE";
        default: return "UNKNOWN";
    }
}

std::string constraintTypeToString(core::CatalogManager::ConstraintType type) {
    switch (type) {
        case core::CatalogManager::ConstraintType::PRIMARY_KEY: return "PRIMARY_KEY";
        case core::CatalogManager::ConstraintType::UNIQUE: return "UNIQUE";
        case core::CatalogManager::ConstraintType::CHECK: return "CHECK";
        case core::CatalogManager::ConstraintType::FOREIGN_KEY: return "FOREIGN_KEY";
        case core::CatalogManager::ConstraintType::NOT_NULL: return "NOT_NULL";
        case core::CatalogManager::ConstraintType::EXCLUSION: return "EXCLUSION";
        default: return "UNKNOWN";
    }
}

std::string domainTypeToString(core::DomainType type) {
    switch (type) {
        case core::DomainType::BASIC: return "BASIC";
        case core::DomainType::RECORD: return "RECORD";
        case core::DomainType::ENUM: return "ENUM";
        case core::DomainType::SET: return "SET";
        case core::DomainType::VARIANT: return "VARIANT";
        case core::DomainType::RANGE: return "RANGE";
        case core::DomainType::BASE: return "BASE";
        case core::DomainType::SHELL: return "SHELL";
        default: return "UNKNOWN";
    }
}

std::string fkActionToString(core::CatalogManager::FKAction action) {
    switch (action) {
        case core::CatalogManager::FKAction::NO_ACTION: return "NO_ACTION";
        case core::CatalogManager::FKAction::RESTRICT: return "RESTRICT";
        case core::CatalogManager::FKAction::CASCADE: return "CASCADE";
        case core::CatalogManager::FKAction::SET_NULL: return "SET_NULL";
        case core::CatalogManager::FKAction::SET_DEFAULT: return "SET_DEFAULT";
        default: return "UNKNOWN";
    }
}

std::string fkMatchToString(core::CatalogManager::FKMatchType match) {
    switch (match) {
        case core::CatalogManager::FKMatchType::SIMPLE: return "SIMPLE";
        case core::CatalogManager::FKMatchType::FULL: return "FULL";
        case core::CatalogManager::FKMatchType::PARTIAL: return "PARTIAL";
        default: return "UNKNOWN";
    }
}

std::string tableTypeToString(core::CatalogManager::TableType type) {
    switch (type) {
        case core::CatalogManager::TableType::HEAP: return "HEAP";
        case core::CatalogManager::TableType::INDEX: return "INDEX";
        case core::CatalogManager::TableType::TEMPORARY: return "TEMPORARY";
        case core::CatalogManager::TableType::EXTERNAL: return "EXTERNAL";
        case core::CatalogManager::TableType::MATERIALIZED_VIEW: return "MATERIALIZED_VIEW";
        case core::CatalogManager::TableType::TOAST: return "TOAST";
        default: return "UNKNOWN";
    }
}

bool loadTableMetadata(core::CatalogManager* catalog,
                       const core::CatalogManager::TableInfo& table_info,
                       json& metadata_out,
                       core::ErrorContext* ctx) {
    metadata_out = json::object();
    if (!catalog || isZeroId(table_info.storage_params_oid)) {
        return false;
    }
    std::string params;
    if (catalog->loadStringFromToast(table_info.storage_params_oid, 0, params, ctx) != core::Status::OK ||
        params.empty()) {
        return false;
    }
    try {
        metadata_out = json::parse(params);
    } catch (...) {
        metadata_out = json::object();
        return false;
    }
    return true;
}


std::string classifyStatementType(const std::string& sql) {
    size_t pos = 0;
    while (pos < sql.size() && std::isspace(static_cast<unsigned char>(sql[pos]))) {
        ++pos;
    }
    std::string keyword;
    while (pos < sql.size() && std::isalpha(static_cast<unsigned char>(sql[pos]))) {
        keyword.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(sql[pos]))));
        ++pos;
    }
    if (keyword.empty()) {
        return "UNKNOWN";
    }
    if (keyword == "SELECT" || keyword == "WITH") {
        return "SELECT";
    }
    if (keyword == "INSERT") {
        return "INSERT";
    }
    if (keyword == "UPDATE") {
        return "UPDATE";
    }
    if (keyword == "DELETE") {
        return "DELETE";
    }
    if (keyword == "CREATE" || keyword == "ALTER" || keyword == "DROP") {
        return "DDL";
    }
    return keyword;
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

core::TypedValue timestampValueOrNull(const std::chrono::system_clock::time_point& value) {
    if (value.time_since_epoch().count() == 0) {
        return core::TypedValue::makeNull(core::DataType::TIMESTAMP);
    }
    auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        value.time_since_epoch()).count();
    return core::TypedValue::makeTimestamp(static_cast<int64_t>(micros));
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

std::string remoteConnectorStateToString(core::CatalogManager::RemoteConnectorState state) {
    switch (state) {
        case core::CatalogManager::RemoteConnectorState::DISABLED: return "DISABLED";
        case core::CatalogManager::RemoteConnectorState::PROBING: return "PROBING";
        case core::CatalogManager::RemoteConnectorState::READY: return "READY";
        case core::CatalogManager::RemoteConnectorState::DEGRADED: return "DEGRADED";
        case core::CatalogManager::RemoteConnectorState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

std::string replicationDirectionToString(core::CatalogManager::ReplicationDirection direction) {
    switch (direction) {
        case core::CatalogManager::ReplicationDirection::ONE_WAY: return "ONE_WAY";
        case core::CatalogManager::ReplicationDirection::BIDIRECTIONAL: return "BIDIRECTIONAL";
        default: return "UNKNOWN";
    }
}

std::string replicationChannelStateToString(core::CatalogManager::ReplicationChannelState state) {
    switch (state) {
        case core::CatalogManager::ReplicationChannelState::INIT: return "INIT";
        case core::CatalogManager::ReplicationChannelState::SNAPSHOT: return "SNAPSHOT";
        case core::CatalogManager::ReplicationChannelState::CATCHUP: return "CATCHUP";
        case core::CatalogManager::ReplicationChannelState::STREAMING: return "STREAMING";
        case core::CatalogManager::ReplicationChannelState::PAUSED: return "PAUSED";
        case core::CatalogManager::ReplicationChannelState::DEGRADED: return "DEGRADED";
        case core::CatalogManager::ReplicationChannelState::FENCED: return "FENCED";
        case core::CatalogManager::ReplicationChannelState::STOPPED: return "STOPPED";
        case core::CatalogManager::ReplicationChannelState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

std::string replicationConflictKindToString(core::CatalogManager::ReplicationConflictKind kind) {
    switch (kind) {
        case core::CatalogManager::ReplicationConflictKind::UPDATE_UPDATE: return "UPDATE_UPDATE";
        case core::CatalogManager::ReplicationConflictKind::DELETE_UPDATE: return "DELETE_UPDATE";
        case core::CatalogManager::ReplicationConflictKind::UNIQUE_CONSTRAINT: return "UNIQUE_CONSTRAINT";
        case core::CatalogManager::ReplicationConflictKind::DDL_DML: return "DDL_DML";
        case core::CatalogManager::ReplicationConflictKind::DDL_DDL: return "DDL_DDL";
        case core::CatalogManager::ReplicationConflictKind::TYPE_MISMATCH: return "TYPE_MISMATCH";
        default: return "UNKNOWN";
    }
}

std::string replicationResolutionStateToString(
    core::CatalogManager::ReplicationResolutionState state) {
    switch (state) {
        case core::CatalogManager::ReplicationResolutionState::OPEN: return "OPEN";
        case core::CatalogManager::ReplicationResolutionState::AUTO_RESOLVED: return "AUTO_RESOLVED";
        case core::CatalogManager::ReplicationResolutionState::MANUAL_PENDING: return "MANUAL_PENDING";
        case core::CatalogManager::ReplicationResolutionState::MANUAL_RESOLVED: return "MANUAL_RESOLVED";
        case core::CatalogManager::ReplicationResolutionState::IGNORED: return "IGNORED";
        default: return "UNKNOWN";
    }
}

std::string replicationCursorStateToString(core::CatalogManager::ReplicationCursorState state) {
    switch (state) {
        case core::CatalogManager::ReplicationCursorState::ACTIVE: return "ACTIVE";
        case core::CatalogManager::ReplicationCursorState::STALLED: return "STALLED";
        case core::CatalogManager::ReplicationCursorState::ERROR: return "ERROR";
        case core::CatalogManager::ReplicationCursorState::CLOSED: return "CLOSED";
        default: return "UNKNOWN";
    }
}

std::string shardStateToString(core::CatalogManager::ShardState state) {
    switch (state) {
        case core::CatalogManager::ShardState::CREATING: return "CREATING";
        case core::CatalogManager::ShardState::ONLINE: return "ONLINE";
        case core::CatalogManager::ShardState::REBALANCING: return "REBALANCING";
        case core::CatalogManager::ShardState::DRAINING: return "DRAINING";
        case core::CatalogManager::ShardState::OFFLINE: return "OFFLINE";
        default: return "UNKNOWN";
    }
}

std::string shardKindToString(core::CatalogManager::ShardKind kind) {
    switch (kind) {
        case core::CatalogManager::ShardKind::ROW: return "ROW";
        case core::CatalogManager::ShardKind::COLUMN: return "COLUMN";
        case core::CatalogManager::ShardKind::VECTOR: return "VECTOR";
        case core::CatalogManager::ShardKind::DOCUMENT: return "DOCUMENT";
        default: return "UNKNOWN";
    }
}

std::string shardMigrationStateToString(core::CatalogManager::ShardMigrationState state) {
    switch (state) {
        case core::CatalogManager::ShardMigrationState::PLANNED: return "PLANNED";
        case core::CatalogManager::ShardMigrationState::RUNNING: return "RUNNING";
        case core::CatalogManager::ShardMigrationState::PAUSED: return "PAUSED";
        case core::CatalogManager::ShardMigrationState::COMPLETED: return "COMPLETED";
        case core::CatalogManager::ShardMigrationState::FAILED: return "FAILED";
        default: return "UNKNOWN";
    }
}

std::string throttleStateToString(core::CatalogManager::ThrottleState state) {
    switch (state) {
        case core::CatalogManager::ThrottleState::NONE: return "NONE";
        case core::CatalogManager::ThrottleState::LOW: return "LOW";
        case core::CatalogManager::ThrottleState::MEDIUM: return "MEDIUM";
        case core::CatalogManager::ThrottleState::HIGH: return "HIGH";
        default: return "UNKNOWN";
    }
}

}  // namespace

void SysCatalogHandler::initializeTableNames() {
    table_names_ = {
        "schemas",
        "tables",
        "columns",
        "indexes",
        "index_columns",
        "constraints",
        "foreign_keys",
        "primary_keys",
        "types",
        "domains",
        "sessions",
        "context_variables",
        "transactions",
        "locks",
        "statements",
        "io_stats",
        "cache_stats",
        "buffer_pool_stats",
        "statement_cache",
        "server_capabilities",
        "jobs",
        "job_runs",
        "job_dependencies",
        "performance",
        "migration_status",
        "migration_audit_summary",
        "replication_channel_status",
        "replication_conflict_queue",
        "replication_cursor_status",
        "shard_status",
        "shard_migrations",
        "plugin",
        "prepared_statement",
        "sb_mga_runtime_metrics",
        "sb_mga_active_transactions",
        "sb_mga_cleanup_debt",
        "sb_mga_snapshot_blockers",
        "sb_mga_transaction_history",
        "sb_mga_wait_history"
    };
}

const SysCatalogHandler::ColumnDefs* SysCatalogHandler::getTableDefinition(
    const std::string& table_name) const {
    static const ColumnDefs kSchemasColumns = {
        {"schema_id", DataType::UUID, false},
        {"schema_name", DataType::TEXT, false},
        {"owner_id", DataType::UUID, true},
        {"default_tablespace_id", DataType::UUID, true},
        {"default_charset", DataType::UUID, true},
        {"default_collation_id", DataType::UINT32, true},
        {"is_valid", DataType::BOOLEAN, true}
    };

    static const ColumnDefs kTablesColumns = {
        {"table_id", DataType::UUID, false},
        {"schema_id", DataType::UUID, false},
        {"table_name", DataType::TEXT, false},
        {"table_type", DataType::TEXT, true},
        {"owner_id", DataType::UUID, true},
        {"tablespace_id", DataType::UUID, true},
        {"row_count", DataType::INT64, true},
        {"has_toast", DataType::BOOLEAN, true},
        {"toast_table_id", DataType::UUID, true},
        {"is_valid", DataType::BOOLEAN, true},
        {"partition_strategy", DataType::TEXT, true},
        {"partition_columns", DataType::TEXT, true},
        {"partition_parent_name", DataType::TEXT, true},
        {"is_partition_child", DataType::BOOLEAN, true}
    };

    static const ColumnDefs kColumnsColumns = {
        {"column_id", DataType::UUID, false},
        {"table_id", DataType::UUID, false},
        {"column_name", DataType::TEXT, false},
        {"data_type_id", DataType::UINT16, true},
        {"data_type_name", DataType::TEXT, true},
        {"ordinal_position", DataType::INT32, true},
        {"is_nullable", DataType::BOOLEAN, true},
        {"default_value", DataType::TEXT, true},
        {"domain_id", DataType::UUID, true},
        {"collation_id", DataType::UINT32, true},
        {"charset_id", DataType::UUID, true},
        {"is_identity", DataType::BOOLEAN, true},
        {"is_generated", DataType::BOOLEAN, true},
        {"generation_expression", DataType::TEXT, true},
        {"is_valid", DataType::BOOLEAN, true}
    };

    static const ColumnDefs kIndexesColumns = {
        {"index_id", DataType::UUID, false},
        {"table_id", DataType::UUID, false},
        {"index_name", DataType::TEXT, false},
        {"index_type", DataType::TEXT, true},
        {"is_unique", DataType::BOOLEAN, true},
        {"is_expression", DataType::BOOLEAN, true},
        {"is_partial", DataType::BOOLEAN, true},
        {"expression_sql", DataType::TEXT, true},
        {"predicate_sql", DataType::TEXT, true},
        {"state", DataType::TEXT, true},
        {"tablespace_id", DataType::UUID, true},
        {"is_valid", DataType::BOOLEAN, true}
    };

    static const ColumnDefs kIndexColumnsColumns = {
        {"index_id", DataType::UUID, false},
        {"column_id", DataType::UUID, true},
        {"column_name", DataType::TEXT, true},
        {"ordinal_position", DataType::INT32, true},
        {"is_included", DataType::BOOLEAN, true}
    };

    static const ColumnDefs kConstraintsColumns = {
        {"constraint_id", DataType::UUID, false},
        {"table_id", DataType::UUID, false},
        {"constraint_name", DataType::TEXT, false},
        {"constraint_type", DataType::TEXT, true},
        {"is_deferrable", DataType::BOOLEAN, true},
        {"initially_deferred", DataType::BOOLEAN, true},
        {"is_enabled", DataType::BOOLEAN, true},
        {"is_validated", DataType::BOOLEAN, true},
        {"check_expression", DataType::TEXT, true}
    };

    static const ColumnDefs kForeignKeysColumns = {
        {"fk_id", DataType::UUID, false},
        {"fk_name", DataType::TEXT, false},
        {"child_table_id", DataType::UUID, false},
        {"parent_table_id", DataType::UUID, false},
        {"child_columns", DataType::TEXT, true},
        {"parent_columns", DataType::TEXT, true},
        {"on_delete", DataType::TEXT, true},
        {"on_update", DataType::TEXT, true},
        {"match_type", DataType::TEXT, true},
        {"is_enabled", DataType::BOOLEAN, true},
        {"is_deferrable", DataType::BOOLEAN, true},
        {"initially_deferred", DataType::BOOLEAN, true}
    };

    static const ColumnDefs kPrimaryKeysColumns = {
        {"constraint_id", DataType::UUID, false},
        {"table_id", DataType::UUID, false},
        {"constraint_name", DataType::TEXT, false},
        {"column_names", DataType::TEXT, true}
    };

    static const ColumnDefs kTypesColumns = {
        {"type_id", DataType::UINT32, false},
        {"type_name", DataType::TEXT, false},
        {"is_builtin", DataType::BOOLEAN, true}
    };

    static const ColumnDefs kDomainsColumns = {
        {"domain_id", DataType::UUID, false},
        {"schema_id", DataType::UUID, false},
        {"domain_name", DataType::TEXT, false},
        {"domain_type", DataType::TEXT, true},
        {"base_type_id", DataType::UINT16, true},
        {"base_type_name", DataType::TEXT, true},
        {"precision", DataType::UINT32, true},
        {"scale", DataType::UINT32, true},
        {"is_nullable", DataType::BOOLEAN, true},
        {"default_value", DataType::TEXT, true},
        {"parent_domain_id", DataType::UUID, true},
        {"is_enum", DataType::BOOLEAN, true},
        {"enum_labels", DataType::TEXT, true},
        {"collation_name", DataType::TEXT, true}
    };

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

    if (equalsCaseInsensitive(table_name, "schemas")) {
        return &kSchemasColumns;
    }
    if (equalsCaseInsensitive(table_name, "tables")) {
        return &kTablesColumns;
    }
    if (equalsCaseInsensitive(table_name, "columns")) {
        return &kColumnsColumns;
    }
    if (equalsCaseInsensitive(table_name, "indexes")) {
        return &kIndexesColumns;
    }
    if (equalsCaseInsensitive(table_name, "index_columns")) {
        return &kIndexColumnsColumns;
    }
    if (equalsCaseInsensitive(table_name, "constraints")) {
        return &kConstraintsColumns;
    }
    if (equalsCaseInsensitive(table_name, "foreign_keys")) {
        return &kForeignKeysColumns;
    }
    if (equalsCaseInsensitive(table_name, "primary_keys")) {
        return &kPrimaryKeysColumns;
    }
    if (equalsCaseInsensitive(table_name, "types")) {
        return &kTypesColumns;
    }
    if (equalsCaseInsensitive(table_name, "domains")) {
        return &kDomainsColumns;
    }
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

    static const ColumnDefs kCacheStatsColumns = {
        {"cache_type", DataType::TEXT, false},
        {"database_name", DataType::TEXT, true},
        {"hits_total", DataType::INT64, true},
        {"misses_total", DataType::INT64, true},
        {"evictions_total", DataType::INT64, true},
        {"entries", DataType::INT64, true},
        {"memory_bytes", DataType::INT64, true},
        {"hit_ratio", DataType::FLOAT64, true},
        {"updated_at", DataType::TIMESTAMP, true}
    };

    static const ColumnDefs kBufferPoolStatsColumns = {
        {"database_name", DataType::TEXT, true},
        {"pool_size_bytes", DataType::INT64, true},
        {"pages_total", DataType::INT64, true},
        {"pages_dirty", DataType::INT64, true},
        {"hits_total", DataType::INT64, true},
        {"misses_total", DataType::INT64, true},
        {"reads_total", DataType::INT64, true},
        {"writes_total", DataType::INT64, true},
        {"hit_ratio", DataType::FLOAT64, true},
        {"updated_at", DataType::TIMESTAMP, true}
    };

    static const ColumnDefs kStatementCacheColumns = {
        {"database_name", DataType::TEXT, true},
        {"sql_text", DataType::TEXT, true},
        {"fingerprint", DataType::TEXT, true},
        {"statement_type", DataType::TEXT, true},
        {"hit_count", DataType::INT64, true},
        {"miss_count", DataType::INT64, true},
        {"execution_count", DataType::INT64, true},
        {"error_count", DataType::INT64, true},
        {"created_at", DataType::TIMESTAMP, true},
        {"last_accessed", DataType::TIMESTAMP, true},
        {"last_executed", DataType::TIMESTAMP, true},
        {"avg_execution_time_ms", DataType::INT64, true},
        {"memory_bytes", DataType::INT64, true},
        {"plan_memory_bytes", DataType::INT64, true}
    };

    static const ColumnDefs kServerCapabilitiesColumns = {
        {"capability", DataType::TEXT, false},
        {"enabled", DataType::BOOLEAN, false}
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

    static const ColumnDefs kContextVariablesColumns = {
        {"attachment_id", DataType::INT64, true},
        {"transaction_id", DataType::INT64, true},
        {"variable_name", DataType::TEXT, false},
        {"variable_value", DataType::TEXT, true}
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

    static const ColumnDefs kIoStatsColumns = {
        {"stat_id", DataType::INT64, false},
        {"stat_group", DataType::INT16, false},
        {"session_id", DataType::UUID, true},
        {"transaction_id", DataType::INT64, true},
        {"statement_id", DataType::INT64, true},
        {"page_reads", DataType::INT64, true},
        {"page_writes", DataType::INT64, true},
        {"page_fetches", DataType::INT64, true},
        {"page_marks", DataType::INT64, true}
    };

    static const ColumnDefs kMigrationStatusColumns = {
        {"connector_id", DataType::UUID, false},
        {"connector_name", DataType::TEXT, false},
        {"engine_name", DataType::TEXT, false},
        {"state", DataType::TEXT, false},
        {"failure_count", DataType::INT32, false},
        {"last_probe_time", DataType::INT64, true},
        {"last_ready_time", DataType::INT64, true},
        {"open_error_count", DataType::INT64, false},
        {"snapshot_count", DataType::INT64, false},
        {"metadata_object_count", DataType::INT64, false},
        {"metadata_column_count", DataType::INT64, false}
    };

    static const ColumnDefs kMigrationAuditSummaryColumns = {
        {"connector_id", DataType::UUID, false},
        {"connector_name", DataType::TEXT, false},
        {"request_count", DataType::INT64, false},
        {"success_count", DataType::INT64, false},
        {"failed_count", DataType::INT64, false},
        {"avg_latency_ms", DataType::INT64, false},
        {"bytes_in_total", DataType::INT64, false},
        {"bytes_out_total", DataType::INT64, false},
        {"last_activity_time", DataType::INT64, true},
        {"open_error_count", DataType::INT64, false}
    };

    static const ColumnDefs kReplicationChannelStatusColumns = {
        {"channel_id", DataType::UUID, false},
        {"channel_name", DataType::TEXT, false},
        {"direction", DataType::TEXT, false},
        {"state", DataType::TEXT, false},
        {"mode_version", DataType::INT64, false},
        {"lag_ms", DataType::INT64, false},
        {"open_conflict_count", DataType::INT64, false},
        {"open_error_count", DataType::INT64, false},
        {"active_cursor_count", DataType::INT64, false},
        {"last_applied_commit_seq", DataType::INT64, false},
        {"last_applied_time", DataType::INT64, true}
    };

    static const ColumnDefs kReplicationConflictQueueColumns = {
        {"conflict_id", DataType::UUID, false},
        {"channel_id", DataType::UUID, false},
        {"channel_name", DataType::TEXT, false},
        {"batch_id", DataType::UUID, false},
        {"conflict_kind", DataType::TEXT, false},
        {"source_commit_seq", DataType::INT64, false},
        {"resolution_state", DataType::TEXT, false},
        {"source_payload", DataType::TEXT, true},
        {"target_payload", DataType::TEXT, true},
        {"resolved_time", DataType::INT64, true}
    };

    static const ColumnDefs kReplicationCursorStatusColumns = {
        {"cursor_id", DataType::UUID, false},
        {"channel_id", DataType::UUID, false},
        {"channel_name", DataType::TEXT, false},
        {"member_id", DataType::UUID, false},
        {"cursor_name", DataType::TEXT, false},
        {"cursor_state", DataType::TEXT, false},
        {"source_commit_seq", DataType::INT64, false},
        {"applied_commit_seq", DataType::INT64, false},
        {"lag_ms", DataType::INT64, false},
        {"heartbeat_time", DataType::INT64, true},
        {"last_error_id", DataType::UUID, true}
    };

    static const ColumnDefs kShardStatusColumns = {
        {"shard_id", DataType::UUID, false},
        {"shard_name", DataType::TEXT, false},
        {"cluster_id", DataType::UUID, false},
        {"state", DataType::TEXT, false},
        {"kind", DataType::TEXT, false},
        {"policy_id", DataType::UUID, false},
        {"replica_count", DataType::INT64, false},
        {"online_replica_count", DataType::INT64, false},
        {"migration_in_progress", DataType::BOOLEAN, false}
    };

    static const ColumnDefs kShardMigrationsColumns = {
        {"migration_id", DataType::UUID, false},
        {"shard_id", DataType::UUID, false},
        {"shard_name", DataType::TEXT, false},
        {"source_node_id", DataType::UUID, false},
        {"target_node_id", DataType::UUID, false},
        {"state", DataType::TEXT, false},
        {"bytes_total", DataType::INT64, false},
        {"bytes_copied", DataType::INT64, false},
        {"rows_total", DataType::INT64, false},
        {"rows_copied", DataType::INT64, false},
        {"throttle_state", DataType::TEXT, false},
        {"progress_pct", DataType::FLOAT64, false},
        {"started_time", DataType::INT64, false},
        {"updated_time", DataType::INT64, false},
        {"completed_time", DataType::INT64, true},
        {"error_code", DataType::TEXT, true},
        {"error_message", DataType::TEXT, true}
    };

    static const ColumnDefs kPluginColumns = {
        {"module_id", DataType::UUID, false},
        {"module_name", DataType::TEXT, false},
        {"engine_id", DataType::UUID, false},
        {"library_path", DataType::TEXT, true},
        {"checksum", DataType::TEXT, true},
        {"entry_point", DataType::TEXT, true},
        {"is_loaded", DataType::BOOLEAN, false},
        {"is_validated", DataType::BOOLEAN, false},
        {"loaded_count", DataType::INT64, false},
        {"last_modified_time", DataType::INT64, false}
    };

    static const ColumnDefs kPreparedStatementColumns = {
        {"remote_prepared_id", DataType::UUID, false},
        {"remote_connector_id", DataType::UUID, false},
        {"session_id", DataType::UUID, false},
        {"statement_name", DataType::TEXT, false},
        {"statement_fingerprint", DataType::INT64, false},
        {"remote_handle", DataType::TEXT, true},
        {"created_time", DataType::INT64, false},
        {"last_used_time", DataType::INT64, false},
        {"expires_time", DataType::INT64, true},
        {"is_valid", DataType::BOOLEAN, false}
    };

    static const ColumnDefs kMgaRuntimeMetricsColumns = {
        {"metric_name", DataType::TEXT, false},
        {"metric_type", DataType::TEXT, false},
        {"value", DataType::FLOAT64, false},
        {"labels_json", DataType::JSON, false},
        {"updated_at_ms", DataType::INT64, false}
    };

    static const ColumnDefs kMgaActiveTransactionsColumns = {
        {"db_uuid", DataType::UUID, false},
        {"txid", DataType::INT64, false},
        {"state", DataType::TEXT, false},
        {"isolation_mode", DataType::TEXT, false},
        {"xmin", DataType::INT64, true},
        {"age_seconds", DataType::FLOAT64, false},
        {"retained_bytes", DataType::INT64, false},
        {"started_at_ms", DataType::INT64, false}
    };

    static const ColumnDefs kMgaCleanupDebtColumns = {
        {"db_uuid", DataType::UUID, false},
        {"relation_name", DataType::TEXT, false},
        {"cleanup_debt_bytes", DataType::INT64, false},
        {"retained_dead_bytes", DataType::INT64, false},
        {"chain_scatter_bucket", DataType::TEXT, true},
        {"rewrite_recommended", DataType::BOOLEAN, false},
        {"sweep_generation", DataType::INT64, false},
        {"observed_at_ms", DataType::INT64, false}
    };

    static const ColumnDefs kMgaSnapshotBlockersColumns = {
        {"db_uuid", DataType::UUID, false},
        {"blocker_txid", DataType::INT64, false},
        {"blocker_identity", DataType::TEXT, false},
        {"retained_bytes", DataType::INT64, false},
        {"snapshot_age_seconds", DataType::FLOAT64, false},
        {"ost_txid", DataType::INT64, false},
        {"observed_at_ms", DataType::INT64, false}
    };

    static const ColumnDefs kMgaTransactionHistoryColumns = {
        {"db_uuid", DataType::UUID, false},
        {"txid", DataType::INT64, false},
        {"state", DataType::TEXT, false},
        {"start_oit", DataType::INT64, true},
        {"end_oit", DataType::INT64, true},
        {"start_oat", DataType::INT64, true},
        {"end_oat", DataType::INT64, true},
        {"start_ost", DataType::INT64, true},
        {"end_ost", DataType::INT64, true},
        {"restart_count", DataType::INT64, false},
        {"publication_fence_seconds", DataType::FLOAT64, true},
        {"limbo_state", DataType::TEXT, true},
        {"started_at_ms", DataType::INT64, false},
        {"ended_at_ms", DataType::INT64, true}
    };

    static const ColumnDefs kMgaWaitHistoryColumns = {
        {"db_uuid", DataType::UUID, false},
        {"wait_event_id", DataType::TEXT, false},
        {"wait_mode", DataType::TEXT, false},
        {"blocker_txid", DataType::INT64, true},
        {"victim_txid", DataType::INT64, true},
        {"blocker_identity", DataType::TEXT, true},
        {"victim_identity", DataType::TEXT, true},
        {"wait_seconds", DataType::FLOAT64, false},
        {"outcome", DataType::TEXT, false},
        {"observed_at_ms", DataType::INT64, false}
    };

    if (equalsCaseInsensitive(table_name, "sessions")) {
        return &kSessionsColumns;
    }
    if (equalsCaseInsensitive(table_name, "context_variables")) {
        return &kContextVariablesColumns;
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
    if (equalsCaseInsensitive(table_name, "io_stats")) {
        return &kIoStatsColumns;
    }
    if (equalsCaseInsensitive(table_name, "cache_stats")) {
        return &kCacheStatsColumns;
    }
    if (equalsCaseInsensitive(table_name, "buffer_pool_stats")) {
        return &kBufferPoolStatsColumns;
    }
    if (equalsCaseInsensitive(table_name, "statement_cache")) {
        return &kStatementCacheColumns;
    }
    if (equalsCaseInsensitive(table_name, "server_capabilities")) {
        return &kServerCapabilitiesColumns;
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
    if (equalsCaseInsensitive(table_name, "migration_status")) {
        return &kMigrationStatusColumns;
    }
    if (equalsCaseInsensitive(table_name, "migration_audit_summary")) {
        return &kMigrationAuditSummaryColumns;
    }
    if (equalsCaseInsensitive(table_name, "replication_channel_status")) {
        return &kReplicationChannelStatusColumns;
    }
    if (equalsCaseInsensitive(table_name, "replication_conflict_queue")) {
        return &kReplicationConflictQueueColumns;
    }
    if (equalsCaseInsensitive(table_name, "replication_cursor_status")) {
        return &kReplicationCursorStatusColumns;
    }
    if (equalsCaseInsensitive(table_name, "shard_status")) {
        return &kShardStatusColumns;
    }
    if (equalsCaseInsensitive(table_name, "shard_migrations")) {
        return &kShardMigrationsColumns;
    }
    if (equalsCaseInsensitive(table_name, "plugin")) {
        return &kPluginColumns;
    }
    if (equalsCaseInsensitive(table_name, "prepared_statement")) {
        return &kPreparedStatementColumns;
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_runtime_metrics")) {
        return &kMgaRuntimeMetricsColumns;
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_active_transactions")) {
        return &kMgaActiveTransactionsColumns;
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_cleanup_debt")) {
        return &kMgaCleanupDebtColumns;
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_snapshot_blockers")) {
        return &kMgaSnapshotBlockersColumns;
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_transaction_history")) {
        return &kMgaTransactionHistoryColumns;
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_wait_history")) {
        return &kMgaWaitHistoryColumns;
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

    if (equalsCaseInsensitive(table_name, "schemas")) {
        return querySchemas(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "tables")) {
        return queryTables(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "columns")) {
        return queryColumns(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "indexes")) {
        return queryIndexes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "index_columns")) {
        return queryIndexColumns(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "constraints")) {
        return queryConstraints(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "foreign_keys")) {
        return queryForeignKeys(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "primary_keys")) {
        return queryPrimaryKeys(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "types")) {
        return queryTypes(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "domains")) {
        return queryDomains(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "sessions")) {
        return querySessions(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "context_variables")) {
        return queryContextVariables(results, ctx);
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
    if (equalsCaseInsensitive(table_name, "io_stats")) {
        return queryIoStats(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "cache_stats")) {
        return queryCacheStats(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "buffer_pool_stats")) {
        return queryBufferPoolStats(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "statement_cache")) {
        return queryStatementCache(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "server_capabilities")) {
        return queryServerCapabilities(results, ctx);
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
    if (equalsCaseInsensitive(table_name, "migration_status")) {
        return queryMigrationStatus(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "migration_audit_summary")) {
        return queryMigrationAuditSummary(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "replication_channel_status")) {
        return queryReplicationChannelStatus(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "replication_conflict_queue")) {
        return queryReplicationConflictQueue(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "replication_cursor_status")) {
        return queryReplicationCursorStatus(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "shard_status")) {
        return queryShardStatus(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "shard_migrations")) {
        return queryShardMigrations(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "plugin")) {
        return queryPlugin(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "prepared_statement")) {
        return queryPreparedStatements(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_runtime_metrics")) {
        return queryMgaRuntimeMetrics(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_active_transactions")) {
        return queryMgaActiveTransactions(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_cleanup_debt")) {
        return queryMgaCleanupDebt(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_snapshot_blockers")) {
        return queryMgaSnapshotBlockers(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_transaction_history")) {
        return queryMgaTransactionHistory(results, ctx);
    }
    if (equalsCaseInsensitive(table_name, "sb_mga_wait_history")) {
        return queryMgaWaitHistory(results, ctx);
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

Status SysCatalogHandler::querySchemas(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        VirtualRow row;
        row.columns = {
            {"schema_id", uuidValueOrNull(schema.schema_id)},
            {"schema_name", textValueOrNull(schema.full_path.empty() ? schema.schema_name : schema.full_path,
                                            DataType::TEXT)},
            {"owner_id", uuidValueOrNull(schema.owner_id)},
            {"default_tablespace_id", uuidValueOrNull(schema.default_tablespace_uuid)},
            {"default_charset", uuidValueOrNull(schema.default_charset_uuid)},
            {"default_collation_id", schema.default_collation_id == 0
                                        ? core::TypedValue::makeNull(DataType::INT32)
                                        : core::TypedValue::makeInt32(
                                              static_cast<int32_t>(schema.default_collation_id))},
            {"is_valid", core::TypedValue::makeBoolean(true)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryTables(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<core::CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::string partition_strategy;
            std::string partition_columns;
            std::string partition_parent;
            bool is_partition_child = false;

            json meta;
            if (loadTableMetadata(catalog_manager_, table, meta, ctx)) {
                if (meta.contains("partition") && meta["partition"].is_object()) {
                    const auto& partition = meta["partition"];
                    if (partition.contains("strategy") && partition["strategy"].is_string()) {
                        partition_strategy = partition["strategy"].get<std::string>();
                    }
                    if (partition.contains("columns") && partition["columns"].is_array()) {
                        std::vector<std::string> cols;
                        for (const auto& col : partition["columns"]) {
                            if (col.is_string()) {
                                cols.push_back(col.get<std::string>());
                            }
                        }
                        partition_columns = joinNames(cols);
                    }
                }
                partition_parent = extractPartitionParentName(meta);
                is_partition_child = !partition_parent.empty();
            }

            VirtualRow row;
            row.columns = {
                {"table_id", uuidValueOrNull(table.table_id)},
                {"schema_id", uuidValueOrNull(table.schema_id)},
                {"table_name", textValueOrNull(table.table_name, DataType::TEXT)},
                {"table_type", textValueOrNull(tableTypeToString(table.table_type), DataType::TEXT)},
                {"owner_id", uuidValueOrNull(table.owner_id)},
                {"tablespace_id", uuidValueOrNull(table.tablespace_uuid)},
                {"row_count", core::TypedValue::makeInt64(static_cast<int64_t>(table.row_count))},
                {"has_toast", core::TypedValue::makeBoolean(table.has_toast)},
                {"toast_table_id", uuidValueOrNull(table.toast_table_id)},
                {"is_valid", core::TypedValue::makeBoolean(true)},
                {"partition_strategy", textValueOrNull(partition_strategy, DataType::TEXT)},
                {"partition_columns", textValueOrNull(partition_columns, DataType::TEXT)},
                {"partition_parent_name", textValueOrNull(partition_parent, DataType::TEXT)},
                {"is_partition_child", core::TypedValue::makeBoolean(is_partition_child)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryColumns(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<core::CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<core::CatalogManager::ColumnInfo> columns;
            if (catalog_manager_->getColumns(table.table_id, columns, ctx) != Status::OK) {
                continue;
            }
            for (const auto& col : columns) {
                std::string default_value = col.default_value;
                if (default_value.empty() && !isZeroId(col.default_value_oid)) {
                    catalog_manager_->loadStringFromToast(col.default_value_oid, 0, default_value, ctx);
                }
                std::string generation_expr = col.generation_expression;
                if (generation_expr.empty() && !isZeroId(col.generation_expr_oid)) {
                    catalog_manager_->loadStringFromToast(col.generation_expr_oid, 0, generation_expr, ctx);
                }

                VirtualRow row;
                row.columns = {
                    {"column_id", uuidValueOrNull(col.column_id)},
                    {"table_id", uuidValueOrNull(col.table_id)},
                    {"column_name", textValueOrNull(col.column_name, DataType::TEXT)},
                    {"data_type_id", core::TypedValue::makeInt32(static_cast<int32_t>(col.data_type))},
                    {"data_type_name", textValueOrNull(core::TypeSystem::getTypeName(
                                                            static_cast<core::DataType>(col.data_type)),
                                                       DataType::TEXT)},
                    {"ordinal_position", core::TypedValue::makeInt32(static_cast<int32_t>(col.ordinal))},
                    {"is_nullable", core::TypedValue::makeBoolean(col.nullable)},
                    {"default_value", textValueOrNull(default_value, DataType::TEXT)},
                    {"domain_id", uuidValueOrNull(col.domain_id)},
                    {"collation_id", col.collation_id == 0
                                         ? core::TypedValue::makeNull(DataType::INT32)
                                         : core::TypedValue::makeInt32(
                                               static_cast<int32_t>(col.collation_id))},
                    {"charset_id", uuidValueOrNull(col.charset_uuid)},
                    {"is_identity", core::TypedValue::makeBoolean(col.is_identity)},
                    {"is_generated", core::TypedValue::makeBoolean(col.is_generated)},
                    {"generation_expression", textValueOrNull(generation_expr, DataType::TEXT)},
                    {"is_valid", core::TypedValue::makeBoolean(true)}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryIndexes(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<core::CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<core::CatalogManager::IndexInfo> indexes;
            if (catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx) != Status::OK) {
                continue;
            }
            for (const auto& index : indexes) {
                std::string expression_sql;
                if (!index.expression_strings.empty()) {
                    expression_sql = joinNames(index.expression_strings);
                }
                VirtualRow row;
                row.columns = {
                    {"index_id", uuidValueOrNull(index.index_id)},
                    {"table_id", uuidValueOrNull(index.table_id)},
                    {"index_name", textValueOrNull(index.index_name, DataType::TEXT)},
                    {"index_type", textValueOrNull(core::indexTypeToString(index.index_type), DataType::TEXT)},
                    {"is_unique", core::TypedValue::makeBoolean(index.is_unique)},
                    {"is_expression", core::TypedValue::makeBoolean(index.is_expression_index)},
                    {"is_partial", core::TypedValue::makeBoolean(index.is_partial_index)},
                    {"expression_sql", textValueOrNull(expression_sql, DataType::TEXT)},
                    {"predicate_sql", textValueOrNull(index.predicate_string, DataType::TEXT)},
                    {"state", textValueOrNull(indexStateToString(index.state), DataType::TEXT)},
                    {"tablespace_id", uuidValueOrNull(index.tablespace_uuid)},
                    {"is_valid", core::TypedValue::makeBoolean(true)}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryIndexColumns(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<core::CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<core::CatalogManager::ColumnInfo> columns;
            if (catalog_manager_->getColumns(table.table_id, columns, ctx) != Status::OK) {
                continue;
            }
            std::unordered_map<core::ID, std::string, core::IDHash> column_names;
            for (const auto& col : columns) {
                column_names.emplace(col.column_id, col.column_name);
            }

            std::vector<core::CatalogManager::IndexInfo> indexes;
            if (catalog_manager_->listIndexesForTable(table.table_id, indexes, ctx) != Status::OK) {
                continue;
            }
            for (const auto& index : indexes) {
                int32_t ordinal = 1;
                for (const auto& col_id : index.column_ids) {
                    auto it = column_names.find(col_id);
                    VirtualRow row;
                    row.columns = {
                        {"index_id", uuidValueOrNull(index.index_id)},
                        {"column_id", uuidValueOrNull(col_id)},
                        {"column_name", it == column_names.end()
                                            ? core::TypedValue::makeNull(DataType::TEXT)
                                            : textValueOrNull(it->second, DataType::TEXT)},
                        {"ordinal_position", core::TypedValue::makeInt32(ordinal++)},
                        {"is_included", core::TypedValue::makeBoolean(false)}
                    };
                    results.rows.push_back(std::move(row));
                }
                for (const auto& col_id : index.include_column_ids) {
                    auto it = column_names.find(col_id);
                    VirtualRow row;
                    row.columns = {
                        {"index_id", uuidValueOrNull(index.index_id)},
                        {"column_id", uuidValueOrNull(col_id)},
                        {"column_name", it == column_names.end()
                                            ? core::TypedValue::makeNull(DataType::TEXT)
                                            : textValueOrNull(it->second, DataType::TEXT)},
                        {"ordinal_position", core::TypedValue::makeInt32(ordinal++)},
                        {"is_included", core::TypedValue::makeBoolean(true)}
                    };
                    results.rows.push_back(std::move(row));
                }
            }
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryConstraints(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<core::CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<core::CatalogManager::ConstraintInfo> constraints;
            if (catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx) != Status::OK) {
                continue;
            }
            for (const auto& constraint : constraints) {
                std::string check_expression = constraint.check_expression;
                if (check_expression.empty() && !isZeroId(constraint.check_expr_oid)) {
                    catalog_manager_->loadStringFromToast(constraint.check_expr_oid, 0,
                                                          check_expression, ctx);
                }
                VirtualRow row;
                row.columns = {
                    {"constraint_id", uuidValueOrNull(constraint.constraint_id)},
                    {"table_id", uuidValueOrNull(constraint.table_id)},
                    {"constraint_name", textValueOrNull(constraint.constraint_name, DataType::TEXT)},
                    {"constraint_type", textValueOrNull(constraintTypeToString(constraint.constraint_type),
                                                        DataType::TEXT)},
                    {"is_deferrable", core::TypedValue::makeBoolean(constraint.is_deferrable)},
                    {"initially_deferred", core::TypedValue::makeBoolean(constraint.initially_deferred)},
                    {"is_enabled", core::TypedValue::makeBoolean(constraint.is_enabled)},
                    {"is_validated", core::TypedValue::makeBoolean(constraint.is_validated)},
                    {"check_expression", textValueOrNull(check_expression, DataType::TEXT)}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryForeignKeys(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<core::CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<core::CatalogManager::ForeignKeyInfo> fks;
            if (catalog_manager_->getForeignKeysForTable(table.table_id, fks, ctx) != Status::OK) {
                continue;
            }
            for (const auto& fk : fks) {
                VirtualRow row;
                row.columns = {
                    {"fk_id", uuidValueOrNull(fk.fk_id)},
                    {"fk_name", textValueOrNull(fk.fk_name, DataType::TEXT)},
                    {"child_table_id", uuidValueOrNull(fk.child_table_id)},
                    {"parent_table_id", uuidValueOrNull(fk.parent_table_id)},
                    {"child_columns", textValueOrNull(joinNames(fk.child_columns), DataType::TEXT)},
                    {"parent_columns", textValueOrNull(joinNames(fk.parent_columns), DataType::TEXT)},
                    {"on_delete", textValueOrNull(fkActionToString(fk.on_delete), DataType::TEXT)},
                    {"on_update", textValueOrNull(fkActionToString(fk.on_update), DataType::TEXT)},
                    {"match_type", textValueOrNull(fkMatchToString(fk.match_type), DataType::TEXT)},
                    {"is_enabled", core::TypedValue::makeBoolean(fk.is_enabled)},
                    {"is_deferrable", core::TypedValue::makeBoolean(fk.is_deferrable)},
                    {"initially_deferred", core::TypedValue::makeBoolean(fk.initially_deferred)}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryPrimaryKeys(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::CatalogManager::SchemaInfo> schemas;
    Status status = catalog_manager_->listSchemas(schemas, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& schema : schemas) {
        std::vector<core::CatalogManager::TableInfo> tables;
        if (catalog_manager_->listTables(schema.schema_id, tables, ctx) != Status::OK) {
            continue;
        }
        for (const auto& table : tables) {
            std::vector<core::CatalogManager::ConstraintInfo> constraints;
            if (catalog_manager_->getConstraintsForTable(table.table_id, constraints, ctx) != Status::OK) {
                continue;
            }
            for (const auto& constraint : constraints) {
                if (constraint.constraint_type != core::CatalogManager::ConstraintType::PRIMARY_KEY) {
                    continue;
                }
                VirtualRow row;
                row.columns = {
                    {"constraint_id", uuidValueOrNull(constraint.constraint_id)},
                    {"table_id", uuidValueOrNull(constraint.table_id)},
                    {"constraint_name", textValueOrNull(constraint.constraint_name, DataType::TEXT)},
                    {"column_names", textValueOrNull(joinNames(constraint.column_names), DataType::TEXT)}
                };
                results.rows.push_back(std::move(row));
            }
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryTypes(VirtualResultSet& results, ErrorContext* /* ctx */) {
    static const core::DataType kTypes[] = {
        core::DataType::UNKNOWN,
        core::DataType::INT8,
        core::DataType::INT16,
        core::DataType::INT32,
        core::DataType::INT64,
        core::DataType::INT128,
        core::DataType::UINT8,
        core::DataType::UINT16,
        core::DataType::UINT32,
        core::DataType::UINT64,
        core::DataType::UINT128,
        core::DataType::FLOAT32,
        core::DataType::FLOAT64,
        core::DataType::DECIMAL,
        core::DataType::MONEY,
        core::DataType::DECFLOAT16,
        core::DataType::DECFLOAT34,
        core::DataType::CHAR,
        core::DataType::VARCHAR,
        core::DataType::TEXT,
        core::DataType::BINARY,
        core::DataType::VARBINARY,
        core::DataType::BLOB,
        core::DataType::BYTEA,
        core::DataType::DATE,
        core::DataType::TIME,
        core::DataType::TIMESTAMP,
        core::DataType::INTERVAL,
        core::DataType::BOOLEAN,
        core::DataType::UUID,
        core::DataType::JSON,
        core::DataType::JSONB,
        core::DataType::XML,
        core::DataType::VECTOR,
        core::DataType::POINT,
        core::DataType::LINESTRING,
        core::DataType::POLYGON,
        core::DataType::MULTIPOINT,
        core::DataType::MULTILINESTRING,
        core::DataType::MULTIPOLYGON,
        core::DataType::GEOMETRYCOLLECTION,
        core::DataType::ARRAY,
        core::DataType::COMPOSITE,
        core::DataType::TSVECTOR,
        core::DataType::TSQUERY,
        core::DataType::INT4RANGE,
        core::DataType::INT8RANGE,
        core::DataType::NUMRANGE,
        core::DataType::TSRANGE,
        core::DataType::TSTZRANGE,
        core::DataType::DATERANGE,
        core::DataType::INET,
        core::DataType::CIDR,
        core::DataType::MACADDR,
        core::DataType::MACADDR8,
        core::DataType::VARIANT,
        core::DataType::NULL_TYPE
    };

    for (const auto& type : kTypes) {
        VirtualRow row;
        row.columns = {
            {"type_id", core::TypedValue::makeInt32(static_cast<int32_t>(type))},
            {"type_name", textValueOrNull(core::TypeSystem::getTypeName(type), DataType::TEXT)},
            {"is_builtin", core::TypedValue::makeBoolean(true)}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status SysCatalogHandler::queryDomains(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }
    std::vector<core::DomainInfo> domains;
    Status status = catalog_manager_->listDomains(core::ID{}, domains, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& domain : domains) {
        std::vector<core::EnumValue> enum_values = domain.enum_values;
        std::sort(enum_values.begin(), enum_values.end(),
                  [](const core::EnumValue& a, const core::EnumValue& b) {
                      return a.position < b.position;
                  });
        std::vector<std::string> enum_labels;
        enum_labels.reserve(enum_values.size());
        for (const auto& val : enum_values) {
            enum_labels.push_back(val.label);
        }

        VirtualRow row;
        row.columns = {
            {"domain_id", uuidValueOrNull(domain.domain_id)},
            {"schema_id", uuidValueOrNull(domain.schema_id)},
            {"domain_name", textValueOrNull(domain.domain_name, DataType::TEXT)},
            {"domain_type", textValueOrNull(domainTypeToString(domain.domain_type), DataType::TEXT)},
            {"base_type_id", core::TypedValue::makeInt32(static_cast<int32_t>(domain.base_type))},
            {"base_type_name", textValueOrNull(core::TypeSystem::getTypeName(domain.base_type),
                                               DataType::TEXT)},
            {"precision", core::TypedValue::makeInt32(static_cast<int32_t>(domain.precision))},
            {"scale", core::TypedValue::makeInt32(static_cast<int32_t>(domain.scale))},
            {"is_nullable", core::TypedValue::makeBoolean(domain.nullable)},
            {"default_value", textValueOrNull(domain.default_value, DataType::TEXT)},
            {"parent_domain_id", uuidValueOrNull(domain.parent_domain_id)},
            {"is_enum", core::TypedValue::makeBoolean(domain.domain_type == core::DomainType::ENUM)},
            {"enum_labels", textValueOrNull(joinNames(enum_labels), DataType::TEXT)},
            {"collation_name", textValueOrNull(domain.collation_name, DataType::TEXT)}
        };
        results.rows.push_back(std::move(row));
    }

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

Status SysCatalogHandler::queryContextVariables(VirtualResultSet& results, ErrorContext* /* ctx */) {
    core::ConnectionContext* conn_ctx = core::ConnectionContext::getCurrent();
    if (!conn_ctx) {
        return Status::OK;
    }

    const int64_t attachment_id = static_cast<int64_t>(conn_ctx->getProcId());
    const auto variables = conn_ctx->listSessionVariables();
    for (const auto& entry : variables) {
        VirtualRow row;
        row.columns = {
            {"attachment_id", TypedValue::makeInt64(attachment_id)},
            {"transaction_id", TypedValue()},
            {"variable_name", TypedValue::makeText(entry.first)},
            {"variable_value", entry.second.empty() ? TypedValue() : TypedValue::makeText(entry.second)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryIoStats(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    if (!db) {
        return Status::OK;
    }

    auto snapshots = db->snapshotConnectionIoStats();
    if (snapshots.empty()) {
        return Status::OK;
    }

    uint64_t db_page_reads = 0;
    uint64_t db_page_writes = 0;
    uint64_t db_page_fetches = 0;
    uint64_t db_page_marks = 0;

    for (const auto& snap : snapshots) {
        db_page_reads += snap.connection_io.page_reads;
        db_page_writes += snap.connection_io.page_writes;
        db_page_fetches += snap.connection_io.page_fetches;
        db_page_marks += snap.connection_io.page_marks;
    }

    VirtualRow db_row;
    db_row.columns = {
        {"stat_id", TypedValue::makeInt64(0)},
        {"stat_group", TypedValue::makeInt64(0)},
        {"session_id", TypedValue::makeNull(DataType::UUID)},
        {"transaction_id", TypedValue()},
        {"statement_id", TypedValue()},
        {"page_reads", TypedValue::makeInt64(static_cast<int64_t>(db_page_reads))},
        {"page_writes", TypedValue::makeInt64(static_cast<int64_t>(db_page_writes))},
        {"page_fetches", TypedValue::makeInt64(static_cast<int64_t>(db_page_fetches))},
        {"page_marks", TypedValue::makeInt64(static_cast<int64_t>(db_page_marks))}
    };
    results.rows.push_back(std::move(db_row));

    for (const auto& snap : snapshots) {
        VirtualRow connection_row;
        connection_row.columns = {
            {"stat_id", TypedValue::makeInt64(static_cast<int64_t>(snap.proc_id))},
            {"stat_group", TypedValue::makeInt64(1)},
            {"session_id", uuidValueOrNull(snap.session_id)},
            {"transaction_id", TypedValue()},
            {"statement_id", TypedValue()},
            {"page_reads", TypedValue::makeInt64(static_cast<int64_t>(snap.connection_io.page_reads))},
            {"page_writes", TypedValue::makeInt64(static_cast<int64_t>(snap.connection_io.page_writes))},
            {"page_fetches", TypedValue::makeInt64(static_cast<int64_t>(snap.connection_io.page_fetches))},
            {"page_marks", TypedValue::makeInt64(static_cast<int64_t>(snap.connection_io.page_marks))}
        };
        results.rows.push_back(std::move(connection_row));

        if (snap.transaction_id != 0) {
            VirtualRow txn_row;
            txn_row.columns = {
                {"stat_id", TypedValue::makeInt64(static_cast<int64_t>(snap.transaction_id))},
                {"stat_group", TypedValue::makeInt64(2)},
                {"session_id", uuidValueOrNull(snap.session_id)},
                {"transaction_id", TypedValue::makeInt64(static_cast<int64_t>(snap.transaction_id))},
                {"statement_id", TypedValue()},
                {"page_reads", TypedValue::makeInt64(static_cast<int64_t>(snap.transaction_io.page_reads))},
                {"page_writes", TypedValue::makeInt64(static_cast<int64_t>(snap.transaction_io.page_writes))},
                {"page_fetches", TypedValue::makeInt64(static_cast<int64_t>(snap.transaction_io.page_fetches))},
                {"page_marks", TypedValue::makeInt64(static_cast<int64_t>(snap.transaction_io.page_marks))}
            };
            results.rows.push_back(std::move(txn_row));
        }

        if (snap.statement_id != 0) {
            VirtualRow stmt_row;
            stmt_row.columns = {
                {"stat_id", TypedValue::makeInt64(static_cast<int64_t>(snap.statement_id))},
                {"stat_group", TypedValue::makeInt64(3)},
                {"session_id", uuidValueOrNull(snap.session_id)},
                {"transaction_id", snap.transaction_id != 0
                    ? TypedValue::makeInt64(static_cast<int64_t>(snap.transaction_id))
                    : TypedValue()},
                {"statement_id", TypedValue::makeInt64(static_cast<int64_t>(snap.statement_id))},
                {"page_reads", TypedValue::makeInt64(static_cast<int64_t>(snap.statement_io.page_reads))},
                {"page_writes", TypedValue::makeInt64(static_cast<int64_t>(snap.statement_io.page_writes))},
                {"page_fetches", TypedValue::makeInt64(static_cast<int64_t>(snap.statement_io.page_fetches))},
                {"page_marks", TypedValue::makeInt64(static_cast<int64_t>(snap.statement_io.page_marks))}
            };
            results.rows.push_back(std::move(stmt_row));
        }
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
    if (metrics.query_progress_rows) {
        add_row("query_progress_rows",
                metrics.query_progress_rows->get({metric_db}),
                "count");
    }
    if (metrics.query_progress_bytes) {
        add_row("query_progress_bytes",
                metrics.query_progress_bytes->get({metric_db}),
                "bytes");
    }
    if (metrics.query_progress_last_update_micros) {
        add_row("query_progress_last_update_micros",
                metrics.query_progress_last_update_micros->get({metric_db}),
                "micros");
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
    double buffer_pool_pages = metrics.buffer_pool_pages_total
        ? metrics.buffer_pool_pages_total->get()
        : 0.0;
    double page_size_bytes = db ? static_cast<double>(db->page_size()) : 0.0;
    if (metrics.buffer_pool_size_bytes || (buffer_pool_pages > 0.0 && page_size_bytes > 0.0)) {
        double allocated_bytes = metrics.buffer_pool_size_bytes
            ? metrics.buffer_pool_size_bytes->get()
            : buffer_pool_pages * page_size_bytes;
        double used_bytes = (buffer_pool_pages > 0.0 && page_size_bytes > 0.0)
            ? buffer_pool_pages * page_size_bytes
            : allocated_bytes;
        add_row("memory_allocated_bytes", allocated_bytes, "bytes");
        add_row("memory_used_bytes", used_bytes, "bytes");
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

Status SysCatalogHandler::queryCacheStats(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto now_micros = []() -> int64_t {
        return static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    };

    std::string db_name;
    if (catalog_manager_ && catalog_manager_->database()) {
        db_name = baseNameFromPath(catalog_manager_->database()->path());
    }

    const core::TypedValue updated_at = core::TypedValue::makeTimestamp(now_micros());

    auto add_row = [&](const std::string& cache_type,
                       const std::string& database_name,
                       int64_t hits,
                       int64_t misses,
                       int64_t evictions,
                       int64_t entries,
                       int64_t memory_bytes,
                       double hit_ratio) {
        VirtualRow row;
        row.columns = {
            {"cache_type", core::TypedValue::makeText(cache_type)},
            {"database_name", database_name.empty()
                                  ? core::TypedValue::makeNull(DataType::TEXT)
                                  : core::TypedValue::makeText(database_name)},
            {"hits_total", core::TypedValue::makeInt64(hits)},
            {"misses_total", core::TypedValue::makeInt64(misses)},
            {"evictions_total", core::TypedValue::makeInt64(evictions)},
            {"entries", core::TypedValue::makeInt64(entries)},
            {"memory_bytes", core::TypedValue::makeInt64(memory_bytes)},
            {"hit_ratio", core::TypedValue::makeFloat64(hit_ratio)},
            {"updated_at", updated_at}
        };
        results.rows.push_back(std::move(row));
    };

    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    auto counter_value = [](core::Counter* counter) -> int64_t {
        return counter ? static_cast<int64_t>(counter->get()) : 0;
    };

    int64_t stmt_hits = counter_value(metrics.statement_cache_hits_total);
    int64_t stmt_misses = counter_value(metrics.statement_cache_misses_total);
    int64_t stmt_evictions = counter_value(metrics.statement_cache_evictions_total);
    double stmt_total = static_cast<double>(stmt_hits + stmt_misses);
    double stmt_hit_ratio = stmt_total > 0.0 ? stmt_hits / stmt_total : 0.0;
    add_row("statement_cache", db_name, stmt_hits, stmt_misses, stmt_evictions, 0, 0, stmt_hit_ratio);

    int64_t result_hits = counter_value(metrics.result_cache_hits_total);
    int64_t result_misses = counter_value(metrics.result_cache_misses_total);
    int64_t result_evictions = counter_value(metrics.result_cache_evictions_total);
    double result_total = static_cast<double>(result_hits + result_misses);
    double result_hit_ratio = result_total > 0.0 ? result_hits / result_total : 0.0;
    add_row("result_cache", db_name, result_hits, result_misses, result_evictions, 0, 0, result_hit_ratio);

    int64_t translation_hits = counter_value(metrics.translation_cache_hits_total);
    int64_t translation_misses = counter_value(metrics.translation_cache_misses_total);
    int64_t translation_evictions = counter_value(metrics.translation_cache_evictions_total);
    double translation_total = static_cast<double>(translation_hits + translation_misses);
    double translation_hit_ratio = translation_total > 0.0 ? translation_hits / translation_total : 0.0;
    add_row("translation_cache", "", translation_hits, translation_misses, translation_evictions, 0, 0,
            translation_hit_ratio);

    return Status::OK;
}

Status SysCatalogHandler::queryBufferPoolStats(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto now_micros = []() -> int64_t {
        return static_cast<int64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
    };

    std::string db_name;
    if (catalog_manager_ && catalog_manager_->database()) {
        db_name = baseNameFromPath(catalog_manager_->database()->path());
    }

    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    double hits = metrics.buffer_pool_hits_total ? metrics.buffer_pool_hits_total->get() : 0.0;
    double misses = metrics.buffer_pool_misses_total ? metrics.buffer_pool_misses_total->get() : 0.0;
    double total = hits + misses;
    double hit_ratio = total > 0.0 ? hits / total : 0.0;

    VirtualRow row;
    row.columns = {
        {"database_name", db_name.empty()
                              ? core::TypedValue::makeNull(DataType::TEXT)
                              : core::TypedValue::makeText(db_name)},
        {"pool_size_bytes", core::TypedValue::makeInt64(
                                metrics.buffer_pool_size_bytes
                                    ? static_cast<int64_t>(metrics.buffer_pool_size_bytes->get())
                                    : 0)},
        {"pages_total", core::TypedValue::makeInt64(
                            metrics.buffer_pool_pages_total
                                ? static_cast<int64_t>(metrics.buffer_pool_pages_total->get())
                                : 0)},
        {"pages_dirty", core::TypedValue::makeInt64(
                            metrics.buffer_pool_pages_dirty
                                ? static_cast<int64_t>(metrics.buffer_pool_pages_dirty->get())
                                : 0)},
        {"hits_total", core::TypedValue::makeInt64(static_cast<int64_t>(hits))},
        {"misses_total", core::TypedValue::makeInt64(static_cast<int64_t>(misses))},
        {"reads_total", core::TypedValue::makeInt64(
                            metrics.buffer_pool_reads_total
                                ? static_cast<int64_t>(metrics.buffer_pool_reads_total->get())
                                : 0)},
        {"writes_total", core::TypedValue::makeInt64(
                             metrics.buffer_pool_writes_total
                                 ? static_cast<int64_t>(metrics.buffer_pool_writes_total->get())
                                 : 0)},
        {"hit_ratio", core::TypedValue::makeFloat64(hit_ratio)},
        {"updated_at", core::TypedValue::makeTimestamp(now_micros())}
    };
    results.rows.push_back(std::move(row));

    return Status::OK;
}

Status SysCatalogHandler::queryStatementCache(VirtualResultSet& results, ErrorContext* /* ctx */) {
    std::string db_name;
    if (catalog_manager_ && catalog_manager_->database()) {
        db_name = baseNameFromPath(catalog_manager_->database()->path());
    }

    auto* conn_ctx = core::ConnectionContext::getCurrent();
    if (!conn_ctx) {
        return Status::OK;
    }

    std::vector<core::ConnectionContext::PreparedStatementInfo> statements;
    conn_ctx->listPreparedStatements(statements);

    for (const auto& stmt : statements) {
        VirtualRow row;
        row.columns = {
            {"database_name", textValueOrNull(db_name, DataType::TEXT)},
            {"sql_text", textValueOrNull(stmt.sql_text, DataType::TEXT)},
            {"fingerprint", textValueOrNull(stmt.name, DataType::TEXT)},
            {"statement_type", textValueOrNull(classifyStatementType(stmt.sql_text),
                                               DataType::TEXT)},
            {"hit_count", core::TypedValue::makeInt64(static_cast<int64_t>(stmt.execution_count))},
            {"miss_count", core::TypedValue::makeInt64(0)},
            {"execution_count", core::TypedValue::makeInt64(static_cast<int64_t>(stmt.execution_count))},
            {"error_count", core::TypedValue::makeInt64(0)},
            {"created_at", stmt.created_at_micros > 0
                               ? core::TypedValue::makeTimestamp(stmt.created_at_micros)
                               : core::TypedValue::makeNull(DataType::TIMESTAMP)},
            {"last_accessed", stmt.last_used_micros > 0
                                  ? core::TypedValue::makeTimestamp(stmt.last_used_micros)
                                  : core::TypedValue::makeNull(DataType::TIMESTAMP)},
            {"last_executed", stmt.last_used_micros > 0
                                  ? core::TypedValue::makeTimestamp(stmt.last_used_micros)
                                  : core::TypedValue::makeNull(DataType::TIMESTAMP)},
            {"avg_execution_time_ms", core::TypedValue::makeNull(DataType::INT64)},
            {"memory_bytes", core::TypedValue::makeInt64(static_cast<int64_t>(stmt.memory_bytes))},
            {"plan_memory_bytes", core::TypedValue::makeInt64(0)}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status SysCatalogHandler::queryServerCapabilities(VirtualResultSet& results, ErrorContext* /* ctx */) {
    uint16_t capabilities = protocol::CONNECT_FLAG_BASE_CAPABILITIES;
    if (core::isCompressionSupported(core::CompressionType::ZSTD)) {
        capabilities |= protocol::CONNECT_FLAG_ZSTD_COMPRESSION;
    }

    struct CapabilityDef {
        const char* name;
        uint16_t flag;
    };

    static const CapabilityDef kCapabilities[] = {
        {"compression", protocol::CONNECT_FLAG_ZSTD_COMPRESSION},
        {"copy", protocol::CONNECT_FLAG_COPY},
        {"lob_stream", protocol::CONNECT_FLAG_LOB_STREAM},
        {"portal_paging", protocol::CONNECT_FLAG_PORTAL_PAGING},
        {"notifications", protocol::CONNECT_FLAG_NOTIFICATIONS},
        {"progress", protocol::CONNECT_FLAG_PROGRESS}
    };

    for (const auto& capability : kCapabilities) {
        VirtualRow row;
        row.columns = {
            {"capability", core::TypedValue::makeText(capability.name)},
            {"enabled", core::TypedValue::makeBool((capabilities & capability.flag) != 0)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryMigrationStatus(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::RemoteConnectorCatalogInfo> connectors;
    Status status = catalog_manager_->listRemoteConnectorCatalogEntries(connectors, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& connector : connectors) {
        std::vector<core::CatalogManager::RemoteErrorCatalogInfo> errors;
        status = catalog_manager_->listRemoteErrorCatalogEntries(connector.remote_connector_id, errors, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        std::vector<core::CatalogManager::RemoteMetadataSnapshotCatalogInfo> snapshots;
        status = catalog_manager_->listRemoteMetadataSnapshotCatalogEntries(
            connector.remote_connector_id, snapshots, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        int64_t open_error_count = 0;
        for (const auto& error : errors) {
            if (error.is_open) {
                ++open_error_count;
            }
        }

        int64_t metadata_object_count = 0;
        int64_t metadata_column_count = 0;
        for (const auto& snapshot : snapshots) {
            std::vector<core::CatalogManager::RemoteMetadataObjectCatalogInfo> objects;
            status = catalog_manager_->listRemoteMetadataObjectCatalogEntries(snapshot.snapshot_id, objects, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND) {
                return status;
            }
            metadata_object_count += static_cast<int64_t>(objects.size());

            for (const auto& object : objects) {
                std::vector<core::CatalogManager::RemoteMetadataColumnCatalogInfo> columns;
                status = catalog_manager_->listRemoteMetadataColumnCatalogEntries(
                    object.remote_object_id, columns, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND) {
                    return status;
                }
                metadata_column_count += static_cast<int64_t>(columns.size());
            }
        }

        VirtualRow row;
        row.columns = {
            {"connector_id", uuidValueOrNull(connector.remote_connector_id)},
            {"connector_name", textValueOrNull(connector.connector_name, DataType::TEXT)},
            {"engine_name", textValueOrNull(connector.engine_name, DataType::TEXT)},
            {"state", textValueOrNull(remoteConnectorStateToString(connector.state), DataType::TEXT)},
            {"failure_count", core::TypedValue::makeInt32(static_cast<int32_t>(connector.failure_count))},
            {"last_probe_time", connector.has_last_probe_time
                                    ? core::TypedValue::makeInt64(static_cast<int64_t>(connector.last_probe_time))
                                    : core::TypedValue::makeNull(DataType::INT64)},
            {"last_ready_time", connector.has_last_ready_time
                                    ? core::TypedValue::makeInt64(static_cast<int64_t>(connector.last_ready_time))
                                    : core::TypedValue::makeNull(DataType::INT64)},
            {"open_error_count", core::TypedValue::makeInt64(open_error_count)},
            {"snapshot_count", core::TypedValue::makeInt64(static_cast<int64_t>(snapshots.size()))},
            {"metadata_object_count", core::TypedValue::makeInt64(metadata_object_count)},
            {"metadata_column_count", core::TypedValue::makeInt64(metadata_column_count)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryMigrationAuditSummary(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::RemoteConnectorCatalogInfo> connectors;
    Status status = catalog_manager_->listRemoteConnectorCatalogEntries(connectors, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& connector : connectors) {
        std::vector<core::CatalogManager::RemoteExecutionAuditCatalogInfo> audits;
        status = catalog_manager_->listRemoteExecutionAuditCatalogEntries(
            connector.remote_connector_id, audits, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        std::vector<core::CatalogManager::RemoteErrorCatalogInfo> errors;
        status = catalog_manager_->listRemoteErrorCatalogEntries(connector.remote_connector_id, errors, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        int64_t request_count = static_cast<int64_t>(audits.size());
        int64_t success_count = 0;
        int64_t failed_count = 0;
        int64_t total_latency_ms = 0;
        int64_t bytes_in_total = 0;
        int64_t bytes_out_total = 0;
        uint64_t last_activity_time = 0;

        for (const auto& audit : audits) {
            if (audit.exec_status == core::CatalogManager::RemoteExecStatus::SUCCESS) {
                ++success_count;
            } else {
                ++failed_count;
            }
            total_latency_ms += static_cast<int64_t>(audit.latency_ms);
            bytes_in_total += static_cast<int64_t>(audit.bytes_in);
            bytes_out_total += static_cast<int64_t>(audit.bytes_out);
            last_activity_time = std::max(last_activity_time, audit.finished_time);
        }

        int64_t open_error_count = 0;
        for (const auto& error : errors) {
            if (error.is_open) {
                ++open_error_count;
            }
        }

        VirtualRow row;
        row.columns = {
            {"connector_id", uuidValueOrNull(connector.remote_connector_id)},
            {"connector_name", textValueOrNull(connector.connector_name, DataType::TEXT)},
            {"request_count", core::TypedValue::makeInt64(request_count)},
            {"success_count", core::TypedValue::makeInt64(success_count)},
            {"failed_count", core::TypedValue::makeInt64(failed_count)},
            {"avg_latency_ms", core::TypedValue::makeInt64(
                                   request_count == 0 ? 0 : total_latency_ms / request_count)},
            {"bytes_in_total", core::TypedValue::makeInt64(bytes_in_total)},
            {"bytes_out_total", core::TypedValue::makeInt64(bytes_out_total)},
            {"last_activity_time", last_activity_time == 0
                                       ? core::TypedValue::makeNull(DataType::INT64)
                                       : core::TypedValue::makeInt64(static_cast<int64_t>(last_activity_time))},
            {"open_error_count", core::TypedValue::makeInt64(open_error_count)}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryReplicationChannelStatus(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::ReplicationChannelCatalogInfo> channels;
    Status status = catalog_manager_->listReplicationChannelCatalogEntries(channels, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& channel : channels) {
        std::vector<core::CatalogManager::ReplicationCursorCatalogInfo> cursors;
        status = catalog_manager_->listReplicationCursorCatalogEntries(
            channel.replication_channel_id, cursors, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        std::vector<core::CatalogManager::ReplicationConflictCatalogInfo> conflicts;
        status = catalog_manager_->listReplicationConflictCatalogEntries(
            channel.replication_channel_id, conflicts, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        std::vector<core::CatalogManager::ReplicationErrorCatalogInfo> errors;
        status = catalog_manager_->listReplicationErrorCatalogEntries(
            channel.replication_channel_id, errors, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        int64_t lag_ms = 0;
        int64_t active_cursor_count = 0;
        uint64_t last_applied_commit_seq = 0;
        uint64_t last_applied_time = 0;
        for (const auto& cursor : cursors) {
            lag_ms = std::max<int64_t>(lag_ms, static_cast<int64_t>(cursor.lag_ms));
            if (cursor.cursor_state == core::CatalogManager::ReplicationCursorState::ACTIVE) {
                ++active_cursor_count;
            }
            last_applied_commit_seq = std::max(last_applied_commit_seq, cursor.applied_commit_seq);
            if (cursor.has_applied_time) {
                last_applied_time = std::max(last_applied_time, cursor.applied_time);
            }
        }

        int64_t open_conflict_count = 0;
        for (const auto& conflict : conflicts) {
            if (conflict.resolution_state == core::CatalogManager::ReplicationResolutionState::OPEN ||
                conflict.resolution_state ==
                    core::CatalogManager::ReplicationResolutionState::MANUAL_PENDING) {
                ++open_conflict_count;
            }
        }

        int64_t open_error_count = 0;
        for (const auto& error : errors) {
            if (error.is_open) {
                ++open_error_count;
            }
        }

        VirtualRow row;
        row.columns = {
            {"channel_id", uuidValueOrNull(channel.replication_channel_id)},
            {"channel_name", textValueOrNull(channel.channel_name, DataType::TEXT)},
            {"direction", textValueOrNull(replicationDirectionToString(channel.direction), DataType::TEXT)},
            {"state", textValueOrNull(replicationChannelStateToString(channel.channel_state), DataType::TEXT)},
            {"mode_version", core::TypedValue::makeInt64(static_cast<int64_t>(channel.mode_version))},
            {"lag_ms", core::TypedValue::makeInt64(lag_ms)},
            {"open_conflict_count", core::TypedValue::makeInt64(open_conflict_count)},
            {"open_error_count", core::TypedValue::makeInt64(open_error_count)},
            {"active_cursor_count", core::TypedValue::makeInt64(active_cursor_count)},
            {"last_applied_commit_seq",
             core::TypedValue::makeInt64(static_cast<int64_t>(last_applied_commit_seq))},
            {"last_applied_time", last_applied_time == 0
                                      ? core::TypedValue::makeNull(DataType::INT64)
                                      : core::TypedValue::makeInt64(static_cast<int64_t>(last_applied_time))}
        };
        results.rows.push_back(std::move(row));
    }

    return Status::OK;
}

Status SysCatalogHandler::queryReplicationConflictQueue(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::ReplicationChannelCatalogInfo> channels;
    Status status = catalog_manager_->listReplicationChannelCatalogEntries(channels, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& channel : channels) {
        std::vector<core::CatalogManager::ReplicationConflictCatalogInfo> conflicts;
        status = catalog_manager_->listReplicationConflictCatalogEntries(
            channel.replication_channel_id, conflicts, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& conflict : conflicts) {
            if (conflict.resolution_state != core::CatalogManager::ReplicationResolutionState::OPEN &&
                conflict.resolution_state !=
                    core::CatalogManager::ReplicationResolutionState::MANUAL_PENDING) {
                continue;
            }

            VirtualRow row;
            row.columns = {
                {"conflict_id", uuidValueOrNull(conflict.replication_conflict_id)},
                {"channel_id", uuidValueOrNull(conflict.replication_channel_id)},
                {"channel_name", textValueOrNull(channel.channel_name, DataType::TEXT)},
                {"batch_id", uuidValueOrNull(conflict.replication_batch_id)},
                {"conflict_kind",
                 textValueOrNull(replicationConflictKindToString(conflict.conflict_kind), DataType::TEXT)},
                {"source_commit_seq",
                 core::TypedValue::makeInt64(static_cast<int64_t>(conflict.source_commit_seq))},
                {"resolution_state",
                 textValueOrNull(replicationResolutionStateToString(conflict.resolution_state), DataType::TEXT)},
                {"source_payload", textValueOrNull(conflict.source_payload, DataType::TEXT)},
                {"target_payload", conflict.has_target_payload
                                       ? textValueOrNull(conflict.target_payload, DataType::TEXT)
                                       : core::TypedValue::makeNull(DataType::TEXT)},
                {"resolved_time", conflict.has_resolved_time
                                      ? core::TypedValue::makeInt64(static_cast<int64_t>(conflict.resolved_time))
                                      : core::TypedValue::makeNull(DataType::INT64)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryReplicationCursorStatus(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::ReplicationChannelCatalogInfo> channels;
    Status status = catalog_manager_->listReplicationChannelCatalogEntries(channels, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& channel : channels) {
        std::vector<core::CatalogManager::ReplicationCursorCatalogInfo> cursors;
        status = catalog_manager_->listReplicationCursorCatalogEntries(
            channel.replication_channel_id, cursors, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& cursor : cursors) {
            VirtualRow row;
            row.columns = {
                {"cursor_id", uuidValueOrNull(cursor.replication_cursor_id)},
                {"channel_id", uuidValueOrNull(cursor.replication_channel_id)},
                {"channel_name", textValueOrNull(channel.channel_name, DataType::TEXT)},
                {"member_id", uuidValueOrNull(cursor.channel_member_id)},
                {"cursor_name", textValueOrNull(cursor.cursor_name, DataType::TEXT)},
                {"cursor_state", textValueOrNull(replicationCursorStateToString(cursor.cursor_state),
                                                 DataType::TEXT)},
                {"source_commit_seq",
                 core::TypedValue::makeInt64(static_cast<int64_t>(cursor.source_commit_seq))},
                {"applied_commit_seq",
                 core::TypedValue::makeInt64(static_cast<int64_t>(cursor.applied_commit_seq))},
                {"lag_ms", core::TypedValue::makeInt64(static_cast<int64_t>(cursor.lag_ms))},
                {"heartbeat_time", cursor.has_heartbeat_time
                                       ? core::TypedValue::makeInt64(static_cast<int64_t>(cursor.heartbeat_time))
                                       : core::TypedValue::makeNull(DataType::INT64)},
                {"last_error_id", cursor.has_last_error_id
                                      ? uuidValueOrNull(cursor.last_error_id)
                                      : core::TypedValue::makeNull(DataType::UUID)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryShardStatus(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::ClusterCatalogInfo> clusters;
    Status status = catalog_manager_->listClusterCatalogEntries(clusters, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& cluster : clusters) {
        std::vector<core::CatalogManager::ShardCatalogInfo> shards;
        status = catalog_manager_->listShardCatalogEntries(cluster.cluster_id, shards, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& shard : shards) {
            std::vector<core::CatalogManager::ShardReplicaCatalogInfo> replicas;
            status = catalog_manager_->listShardReplicaCatalogEntries(shard.shard_id, replicas, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND) {
                return status;
            }

            std::vector<core::CatalogManager::ShardMigrationCatalogInfo> migrations;
            status = catalog_manager_->listShardMigrationCatalogEntries(shard.shard_id, migrations, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND) {
                return status;
            }

            int64_t online_replica_count = 0;
            for (const auto& replica : replicas) {
                if (replica.replica_state == core::CatalogManager::ReplicaState::ONLINE) {
                    ++online_replica_count;
                }
            }

            bool migration_in_progress = false;
            for (const auto& migration : migrations) {
                if (migration.state == core::CatalogManager::ShardMigrationState::PLANNED ||
                    migration.state == core::CatalogManager::ShardMigrationState::RUNNING ||
                    migration.state == core::CatalogManager::ShardMigrationState::PAUSED) {
                    migration_in_progress = true;
                    break;
                }
            }

            VirtualRow row;
            row.columns = {
                {"shard_id", uuidValueOrNull(shard.shard_id)},
                {"shard_name", textValueOrNull(shard.shard_name, DataType::TEXT)},
                {"cluster_id", uuidValueOrNull(shard.cluster_id)},
                {"state", textValueOrNull(shardStateToString(shard.shard_state), DataType::TEXT)},
                {"kind", textValueOrNull(shardKindToString(shard.shard_kind), DataType::TEXT)},
                {"policy_id", uuidValueOrNull(shard.policy_id)},
                {"replica_count", core::TypedValue::makeInt64(static_cast<int64_t>(replicas.size()))},
                {"online_replica_count", core::TypedValue::makeInt64(online_replica_count)},
                {"migration_in_progress", core::TypedValue::makeBoolean(migration_in_progress)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryShardMigrations(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::unordered_map<core::ID, std::string, core::IDHash> shard_names;
    std::vector<core::CatalogManager::ClusterCatalogInfo> clusters;
    Status status = catalog_manager_->listClusterCatalogEntries(clusters, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& cluster : clusters) {
        std::vector<core::CatalogManager::ShardCatalogInfo> shards;
        status = catalog_manager_->listShardCatalogEntries(cluster.cluster_id, shards, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }
        for (const auto& shard : shards) {
            shard_names.emplace(shard.shard_id, shard.shard_name);
        }
    }

    for (const auto& [shard_id, shard_name] : shard_names) {
        std::vector<core::CatalogManager::ShardMigrationCatalogInfo> migrations;
        status = catalog_manager_->listShardMigrationCatalogEntries(shard_id, migrations, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }

        for (const auto& migration : migrations) {
            double progress_pct = 0.0;
            if (migration.bytes_total > 0) {
                progress_pct =
                    static_cast<double>(migration.bytes_copied) * 100.0 /
                    static_cast<double>(migration.bytes_total);
            } else if (migration.rows_total > 0) {
                progress_pct =
                    static_cast<double>(migration.rows_copied) * 100.0 /
                    static_cast<double>(migration.rows_total);
            }

            VirtualRow row;
            row.columns = {
                {"migration_id", uuidValueOrNull(migration.migration_id)},
                {"shard_id", uuidValueOrNull(migration.shard_id)},
                {"shard_name", textValueOrNull(shard_name, DataType::TEXT)},
                {"source_node_id", uuidValueOrNull(migration.source_node_id)},
                {"target_node_id", uuidValueOrNull(migration.target_node_id)},
                {"state", textValueOrNull(shardMigrationStateToString(migration.state), DataType::TEXT)},
                {"bytes_total", core::TypedValue::makeInt64(static_cast<int64_t>(migration.bytes_total))},
                {"bytes_copied", core::TypedValue::makeInt64(static_cast<int64_t>(migration.bytes_copied))},
                {"rows_total", core::TypedValue::makeInt64(static_cast<int64_t>(migration.rows_total))},
                {"rows_copied", core::TypedValue::makeInt64(static_cast<int64_t>(migration.rows_copied))},
                {"throttle_state",
                 textValueOrNull(throttleStateToString(migration.throttle_state), DataType::TEXT)},
                {"progress_pct", core::TypedValue::makeFloat64(progress_pct)},
                {"started_time", core::TypedValue::makeInt64(static_cast<int64_t>(migration.started_time))},
                {"updated_time", core::TypedValue::makeInt64(static_cast<int64_t>(migration.updated_time))},
                {"completed_time", migration.has_completed_time
                                       ? core::TypedValue::makeInt64(static_cast<int64_t>(
                                             migration.completed_time))
                                       : core::TypedValue::makeNull(DataType::INT64)},
                {"error_code", textValueOrNull(migration.error_code, DataType::TEXT)},
                {"error_message", textValueOrNull(migration.error_message, DataType::TEXT)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryPlugin(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::UDREngineInfo> engines;
    Status status = catalog_manager_->listUDREngines(engines, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& engine : engines) {
        std::vector<core::CatalogManager::UDRModuleInfo> modules;
        status = catalog_manager_->listUDRModules(engine.engine_id, modules, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }
        for (const auto& module : modules) {
            VirtualRow row;
            row.columns = {
                {"module_id", uuidValueOrNull(module.module_id)},
                {"module_name", textValueOrNull(module.module_name, DataType::TEXT)},
                {"engine_id", uuidValueOrNull(module.engine_id)},
                {"library_path", textValueOrNull(module.library_path, DataType::TEXT)},
                {"checksum", textValueOrNull(module.checksum, DataType::TEXT)},
                {"entry_point", textValueOrNull(module.entry_point, DataType::TEXT)},
                {"is_loaded", core::TypedValue::makeBoolean(module.is_loaded)},
                {"is_validated", core::TypedValue::makeBoolean(module.is_validated)},
                {"loaded_count", core::TypedValue::makeInt64(static_cast<int64_t>(module.loaded_count))},
                {"last_modified_time",
                 core::TypedValue::makeInt64(static_cast<int64_t>(module.last_modified_time))}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryPreparedStatements(VirtualResultSet& results, ErrorContext* ctx) {
    if (!catalog_manager_) {
        return Status::OK;
    }

    std::vector<core::CatalogManager::SessionInfo> sessions;
    Status status = catalog_manager_->listSessions(sessions, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND) {
        return status;
    }

    for (const auto& session : sessions) {
        std::vector<core::CatalogManager::RemotePreparedStatementCatalogInfo> prepared_rows;
        status = catalog_manager_->listRemotePreparedStatementCatalogEntries(
            session.session_id, prepared_rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND) {
            return status;
        }
        for (const auto& prepared : prepared_rows) {
            VirtualRow row;
            row.columns = {
                {"remote_prepared_id", uuidValueOrNull(prepared.remote_prepared_id)},
                {"remote_connector_id", uuidValueOrNull(prepared.remote_connector_id)},
                {"session_id", uuidValueOrNull(prepared.session_id)},
                {"statement_name", textValueOrNull(prepared.statement_name, DataType::TEXT)},
                {"statement_fingerprint",
                 core::TypedValue::makeInt64(static_cast<int64_t>(prepared.statement_fingerprint))},
                {"remote_handle", textValueOrNull(prepared.remote_handle, DataType::TEXT)},
                {"created_time", core::TypedValue::makeInt64(static_cast<int64_t>(prepared.created_time))},
                {"last_used_time", core::TypedValue::makeInt64(static_cast<int64_t>(prepared.last_used_time))},
                {"expires_time", prepared.has_expires_time
                                     ? core::TypedValue::makeInt64(static_cast<int64_t>(
                                           prepared.expires_time))
                                     : core::TypedValue::makeNull(DataType::INT64)},
                {"is_valid", core::TypedValue::makeBoolean(prepared.is_valid)}
            };
            results.rows.push_back(std::move(row));
        }
    }

    return Status::OK;
}

Status SysCatalogHandler::queryMgaRuntimeMetrics(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    if (!db) {
        return Status::OK;
    }

    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::vector<core::SqlRuntimeMetricRow> rows;
    Status status = core::SqlObservabilityViewBuilder::buildMgaRuntimeRows(
        *db, core::MetricsRegistry::getInstance(), now_ms, rows);
    if (status != Status::OK) {
        return status;
    }

    for (const auto& metric_row : rows) {
        VirtualRow row;
        row.columns = {
            {"metric_name", core::TypedValue::makeText(metric_row.metric_name)},
            {"metric_type", core::TypedValue::makeText(metric_row.metric_type)},
            {"value", core::TypedValue::makeFloat64(metric_row.value)},
            {"labels_json", core::TypedValue::makeJSON(metric_row.labels_json)},
            {"updated_at_ms", core::TypedValue::makeInt64(static_cast<int64_t>(metric_row.updated_at))}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status SysCatalogHandler::queryMgaActiveTransactions(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    if (!db) {
        return Status::OK;
    }

    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::vector<core::SqlMgaActiveTransactionRow> rows;
    Status status = core::SqlObservabilityViewBuilder::buildMgaActiveTransactionRows(*db, now_ms, rows);
    if (status != Status::OK) {
        return status;
    }

    const core::TypedValue db_uuid = uuidValueOrNull(db->uuid());
    for (const auto& active_row : rows) {
        VirtualRow row;
        row.columns = {
            {"db_uuid", db_uuid},
            {"txid", core::TypedValue::makeInt64(static_cast<int64_t>(active_row.txid))},
            {"state", core::TypedValue::makeText(active_row.state)},
            {"isolation_mode", core::TypedValue::makeText(active_row.isolation_mode)},
            {"xmin", active_row.has_xmin
                         ? core::TypedValue::makeInt64(static_cast<int64_t>(active_row.xmin))
                         : core::TypedValue::makeNull(DataType::INT64)},
            {"age_seconds", core::TypedValue::makeFloat64(active_row.age_seconds)},
            {"retained_bytes", core::TypedValue::makeInt64(static_cast<int64_t>(active_row.retained_bytes))},
            {"started_at_ms", core::TypedValue::makeInt64(static_cast<int64_t>(active_row.started_at_ms))}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status SysCatalogHandler::queryMgaCleanupDebt(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    if (!db) {
        return Status::OK;
    }

    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::vector<core::SqlMgaCleanupDebtRow> rows;
    Status status = core::SqlObservabilityViewBuilder::buildMgaCleanupDebtRows(
        *db, core::MetricsRegistry::getInstance(), now_ms, rows);
    if (status != Status::OK) {
        return status;
    }

    const core::TypedValue db_uuid = uuidValueOrNull(db->uuid());
    for (const auto& debt_row : rows) {
        VirtualRow row;
        row.columns = {
            {"db_uuid", db_uuid},
            {"relation_name", core::TypedValue::makeText(debt_row.relation_name)},
            {"cleanup_debt_bytes", core::TypedValue::makeInt64(static_cast<int64_t>(debt_row.cleanup_debt_bytes))},
            {"retained_dead_bytes", core::TypedValue::makeInt64(static_cast<int64_t>(debt_row.retained_dead_bytes))},
            {"chain_scatter_bucket", debt_row.has_chain_scatter_bucket
                                         ? core::TypedValue::makeText(debt_row.chain_scatter_bucket)
                                         : core::TypedValue::makeNull(DataType::TEXT)},
            {"rewrite_recommended", core::TypedValue::makeBoolean(debt_row.rewrite_recommended)},
            {"sweep_generation", core::TypedValue::makeInt64(static_cast<int64_t>(debt_row.sweep_generation))},
            {"observed_at_ms", core::TypedValue::makeInt64(static_cast<int64_t>(debt_row.observed_at_ms))}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status SysCatalogHandler::queryMgaSnapshotBlockers(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    if (!db) {
        return Status::OK;
    }

    auto& metrics = core::ScratchBirdMetrics::getInstance();
    metrics.initialize();

    const uint64_t now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());

    std::vector<core::SqlMgaSnapshotBlockerRow> rows;
    Status status = core::SqlObservabilityViewBuilder::buildMgaSnapshotBlockerRows(
        *db, core::MetricsRegistry::getInstance(), now_ms, rows);
    if (status != Status::OK) {
        return status;
    }

    const core::TypedValue db_uuid = uuidValueOrNull(db->uuid());
    for (const auto& blocker_row : rows) {
        VirtualRow row;
        row.columns = {
            {"db_uuid", db_uuid},
            {"blocker_txid", core::TypedValue::makeInt64(static_cast<int64_t>(blocker_row.blocker_txid))},
            {"blocker_identity", core::TypedValue::makeText(blocker_row.blocker_identity)},
            {"retained_bytes", core::TypedValue::makeInt64(static_cast<int64_t>(blocker_row.retained_bytes))},
            {"snapshot_age_seconds", core::TypedValue::makeFloat64(blocker_row.snapshot_age_seconds)},
            {"ost_txid", core::TypedValue::makeInt64(static_cast<int64_t>(blocker_row.ost_txid))},
            {"observed_at_ms", core::TypedValue::makeInt64(static_cast<int64_t>(blocker_row.observed_at_ms))}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status SysCatalogHandler::queryMgaTransactionHistory(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    if (!db) {
        return Status::OK;
    }

    std::vector<core::SqlMgaTransactionHistoryRow> rows;
    Status status = core::SqlObservabilityViewBuilder::buildMgaTransactionHistoryRows(*db, rows);
    if (status != Status::OK) {
        return status;
    }

    const core::TypedValue db_uuid = uuidValueOrNull(db->uuid());
    for (const auto& history_row : rows) {
        VirtualRow row;
        row.columns = {
            {"db_uuid", db_uuid},
            {"txid", core::TypedValue::makeInt64(static_cast<int64_t>(history_row.txid))},
            {"state", core::TypedValue::makeText(history_row.state)},
            {"start_oit", history_row.has_start_oit
                              ? core::TypedValue::makeInt64(static_cast<int64_t>(history_row.start_oit))
                              : core::TypedValue::makeNull(DataType::INT64)},
            {"end_oit", history_row.has_end_oit
                            ? core::TypedValue::makeInt64(static_cast<int64_t>(history_row.end_oit))
                            : core::TypedValue::makeNull(DataType::INT64)},
            {"start_oat", history_row.has_start_oat
                              ? core::TypedValue::makeInt64(static_cast<int64_t>(history_row.start_oat))
                              : core::TypedValue::makeNull(DataType::INT64)},
            {"end_oat", history_row.has_end_oat
                            ? core::TypedValue::makeInt64(static_cast<int64_t>(history_row.end_oat))
                            : core::TypedValue::makeNull(DataType::INT64)},
            {"start_ost", history_row.has_start_ost
                              ? core::TypedValue::makeInt64(static_cast<int64_t>(history_row.start_ost))
                              : core::TypedValue::makeNull(DataType::INT64)},
            {"end_ost", history_row.has_end_ost
                            ? core::TypedValue::makeInt64(static_cast<int64_t>(history_row.end_ost))
                            : core::TypedValue::makeNull(DataType::INT64)},
            {"restart_count", core::TypedValue::makeInt64(static_cast<int64_t>(history_row.restart_count))},
            {"publication_fence_seconds",
             history_row.has_publication_fence_seconds
                 ? core::TypedValue::makeFloat64(history_row.publication_fence_seconds)
                 : core::TypedValue::makeNull(DataType::FLOAT64)},
            {"limbo_state", history_row.has_limbo_state
                                ? core::TypedValue::makeText(history_row.limbo_state)
                                : core::TypedValue::makeNull(DataType::TEXT)},
            {"started_at_ms", core::TypedValue::makeInt64(static_cast<int64_t>(history_row.started_at_ms))},
            {"ended_at_ms", history_row.has_ended_at_ms
                                ? core::TypedValue::makeInt64(static_cast<int64_t>(history_row.ended_at_ms))
                                : core::TypedValue::makeNull(DataType::INT64)}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

Status SysCatalogHandler::queryMgaWaitHistory(VirtualResultSet& results, ErrorContext* /* ctx */) {
    auto* db = catalog_manager_ ? catalog_manager_->database() : nullptr;
    if (!db) {
        return Status::OK;
    }

    std::vector<core::SqlMgaWaitHistoryRow> rows;
    Status status = core::SqlObservabilityViewBuilder::buildMgaWaitHistoryRows(*db, rows);
    if (status != Status::OK) {
        return status;
    }

    const core::TypedValue db_uuid = uuidValueOrNull(db->uuid());
    for (const auto& wait_row : rows) {
        VirtualRow row;
        row.columns = {
            {"db_uuid", db_uuid},
            {"wait_event_id", core::TypedValue::makeText(wait_row.wait_event_id)},
            {"wait_mode", core::TypedValue::makeText(wait_row.wait_mode)},
            {"blocker_txid", wait_row.has_blocker_txid
                                 ? core::TypedValue::makeInt64(static_cast<int64_t>(wait_row.blocker_txid))
                                 : core::TypedValue::makeNull(DataType::INT64)},
            {"victim_txid", wait_row.has_victim_txid
                                ? core::TypedValue::makeInt64(static_cast<int64_t>(wait_row.victim_txid))
                                : core::TypedValue::makeNull(DataType::INT64)},
            {"blocker_identity", wait_row.has_blocker_identity
                                     ? core::TypedValue::makeText(wait_row.blocker_identity)
                                     : core::TypedValue::makeNull(DataType::TEXT)},
            {"victim_identity", wait_row.has_victim_identity
                                    ? core::TypedValue::makeText(wait_row.victim_identity)
                                    : core::TypedValue::makeNull(DataType::TEXT)},
            {"wait_seconds", core::TypedValue::makeFloat64(wait_row.wait_seconds)},
            {"outcome", core::TypedValue::makeText(wait_row.outcome)},
            {"observed_at_ms", core::TypedValue::makeInt64(static_cast<int64_t>(wait_row.observed_at_ms))}
        };
        results.rows.push_back(std::move(row));
    }
    return Status::OK;
}

} // namespace scratchbird::catalog
