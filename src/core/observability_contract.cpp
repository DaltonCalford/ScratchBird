/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/observability_contract.h"

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/garbage_collector.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/mga_failpoint_manager.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/sweep_manager.h"
#include "scratchbird/core/transaction_manager.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace scratchbird::core
{

    namespace
    {

        auto toLower(std::string_view value) -> std::string
        {
            std::string out;
            out.reserve(value.size());
            for (char c : value)
            {
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
            return out;
        }

        auto splitByUnderscore(std::string_view value) -> std::vector<std::string>
        {
            std::vector<std::string> out;
            std::string current;
            for (char c : value)
            {
                if (c == '_')
                {
                    out.push_back(current);
                    current.clear();
                }
                else
                {
                    current.push_back(c);
                }
            }
            out.push_back(current);
            return out;
        }

        auto typeToString(MetricType type) -> const char*
        {
            switch (type)
            {
                case MetricType::COUNTER:
                    return "counter";
                case MetricType::GAUGE:
                    return "gauge";
                case MetricType::HISTOGRAM:
                    return "histogram";
                case MetricType::SUMMARY:
                    return "summary";
            }
            return "unknown";
        }

        auto hasSuffix(std::string_view value, std::string_view suffix) -> bool
        {
            return value.size() >= suffix.size() &&
                value.substr(value.size() - suffix.size()) == suffix;
        }

        auto inferTypeFromSampleName(std::string_view metric_name) -> const char*
        {
            if (hasSuffix(metric_name, "_bucket") || hasSuffix(metric_name, "_sum") ||
                hasSuffix(metric_name, "_count"))
            {
                return "histogram";
            }
            if (hasSuffix(metric_name, "_total"))
            {
                return "counter";
            }
            return "gauge";
        }

        auto isZeroId(const ID& id) -> bool
        {
            for (uint8_t byte : id.bytes)
            {
                if (byte != 0)
                {
                    return false;
                }
            }
            return true;
        }

        auto microsToMillis(uint64_t micros) -> uint64_t
        {
            return micros / 1000;
        }

        auto microsToSeconds(uint64_t micros) -> double
        {
            return static_cast<double>(micros) / 1000000.0;
        }

        auto ageSecondsFromMicros(uint64_t now_ms, uint64_t started_at_micros) -> double
        {
            const uint64_t now_micros = now_ms * 1000;
            if (started_at_micros == 0 || now_micros <= started_at_micros)
            {
                return 0.0;
            }
            return microsToSeconds(now_micros - started_at_micros);
        }

        auto dbUuidString(const Database& db) -> std::string
        {
            return db.uuid().toString();
        }

        auto relationNameForTableId(CatalogManager* catalog, const ID& table_id) -> std::string
        {
            if (catalog == nullptr || isZeroId(table_id))
            {
                return {};
            }
            CatalogManager::TableInfo table{};
            if (catalog->getTable(table_id, table, nullptr) != Status::OK)
            {
                return {};
            }
            return table.table_name;
        }

        auto isolationModeName(uint8_t isolation_level) -> std::string
        {
            switch (static_cast<IsolationLevel>(isolation_level))
            {
                case IsolationLevel::READ_COMMITTED:
                case IsolationLevel::READ_COMMITTED_READ_CONSISTENCY:
                    return "read_committed";
                case IsolationLevel::SNAPSHOT:
                    return "snapshot";
                case IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                    return "snapshot_table_stability";
            }
            return "unknown";
        }

        auto runtimeTransactionStateName(CatalogManager::RuntimeTransactionState state) -> const char*
        {
            switch (state)
            {
                case CatalogManager::RuntimeTransactionState::IN_PROGRESS:
                    return "IN_PROGRESS";
                case CatalogManager::RuntimeTransactionState::COMMITTED:
                    return "COMMITTED";
                case CatalogManager::RuntimeTransactionState::ABORTED:
                    return "ABORTED";
                case CatalogManager::RuntimeTransactionState::PREPARED:
                    return "PREPARED";
            }
            return "UNKNOWN";
        }

        auto lockModeNameFromByte(uint8_t mode) -> std::string
        {
            switch (static_cast<LockMode>(mode))
            {
                case LockMode::LOCK_ACCESS_SHARE:
                    return "ACCESS_SHARE";
                case LockMode::LOCK_ROW_SHARE:
                    return "ROW_SHARE";
                case LockMode::LOCK_ROW_EXCLUSIVE:
                    return "ROW_EXCLUSIVE";
                case LockMode::LOCK_SHARE_UPDATE_EXCLUSIVE:
                    return "SHARE_UPDATE_EXCLUSIVE";
                case LockMode::LOCK_SHARE:
                    return "SHARE";
                case LockMode::LOCK_SHARE_ROW_EXCLUSIVE:
                    return "SHARE_ROW_EXCLUSIVE";
                case LockMode::LOCK_EXCLUSIVE:
                    return "EXCLUSIVE";
                case LockMode::LOCK_ACCESS_EXCLUSIVE:
                    return "ACCESS_EXCLUSIVE";
            }
            return "UNKNOWN";
        }

        auto chainScatterBucketForPages(size_t page_count) -> std::string
        {
            if (page_count <= 1)
            {
                return "same_page";
            }
            if (page_count <= 4)
            {
                return "local";
            }
            if (page_count <= 16)
            {
                return "scattered";
            }
            return "wide";
        }

        auto chainDepthBucketForDepth(uint16_t depth_hint) -> std::string
        {
            if (depth_hint <= 1)
            {
                return "depth_1";
            }
            if (depth_hint <= 3)
            {
                return "depth_2_3";
            }
            if (depth_hint <= 7)
            {
                return "depth_4_7";
            }
            return "depth_8_plus";
        }

        auto checkpointStateName(CheckpointLifecycleState state) -> const char*
        {
            switch (state)
            {
                case CheckpointLifecycleState::IDLE:
                    return "IDLE";
                case CheckpointLifecycleState::CAPTURING_HORIZONS:
                    return "CAPTURING_HORIZONS";
                case CheckpointLifecycleState::DRAINING_DIRTY_SET:
                    return "DRAINING_DIRTY_SET";
                case CheckpointLifecycleState::PERSISTING_CHECKPOINT_MARKER:
                    return "PERSISTING_CHECKPOINT_MARKER";
                case CheckpointLifecycleState::CLEAN_SHUTDOWN_ARMED:
                    return "CLEAN_SHUTDOWN_ARMED";
                case CheckpointLifecycleState::COMPLETE:
                    return "COMPLETE";
                case CheckpointLifecycleState::FAILED:
                    return "FAILED";
            }
            return "UNKNOWN";
        }

        auto startupClassificationName(Database::StartupRecoveryClassification classification)
            -> const char*
        {
            switch (classification)
            {
                case Database::StartupRecoveryClassification::NOT_CLASSIFIED:
                    return "not_classified";
                case Database::StartupRecoveryClassification::CLEAN_SHUTDOWN_FAST_PATH:
                    return "clean_shutdown_fast_path";
                case Database::StartupRecoveryClassification::DIRTY_SHUTDOWN_NORMALIZATION_REQUIRED:
                    return "dirty_shutdown_normalization_required";
                case Database::StartupRecoveryClassification::REPAIRABLE_PAGE_DAMAGE:
                    return "repairable_page_damage";
                case Database::StartupRecoveryClassification::WRITEBACK_FAILURE_RESUME:
                    return "writeback_failure_resume";
                case Database::StartupRecoveryClassification::CATALOG_OR_CONTROL_DAMAGE_FATAL:
                    return "catalog_or_control_damage_fatal";
            }
            return "unknown";
        }

        auto startupOutcomeName(Database::StartupReconciliationOutcome outcome) -> const char*
        {
            switch (outcome)
            {
                case Database::StartupReconciliationOutcome::NOT_RUN:
                    return "not_run";
                case Database::StartupReconciliationOutcome::CLEAN:
                    return "clean";
                case Database::StartupReconciliationOutcome::CLEAN_WITH_FINDINGS:
                    return "clean_with_findings";
                case Database::StartupReconciliationOutcome::RECOVERY_WITH_FINDINGS:
                    return "recovery_with_findings";
                case Database::StartupReconciliationOutcome::FAILED_PAGE_SCAN:
                    return "failed_page_scan";
                case Database::StartupReconciliationOutcome::FAILED_TXN_RECONCILIATION:
                    return "failed_txn_reconciliation";
                case Database::StartupReconciliationOutcome::FAILED_CORRUPTION_POLICY:
                    return "failed_corruption_policy";
            }
            return "unknown";
        }

        auto startupServiceStateName(Database::StartupServiceState state) -> const char*
        {
            switch (state)
            {
                case Database::StartupServiceState::NORMAL:
                    return "normal";
                case Database::StartupServiceState::DEGRADED_READ_WRITE:
                    return "degraded_read_write";
                case Database::StartupServiceState::WRITE_FENCED:
                    return "write_fenced";
                case Database::StartupServiceState::FATAL:
                    return "fatal";
            }
            return "unknown";
        }

        auto startupWarmupModeName(const Database::StartupReconciliationState& state) -> const char*
        {
            if (state.classification ==
                Database::StartupRecoveryClassification::CLEAN_SHUTDOWN_FAST_PATH)
            {
                return "fast_path";
            }
            if (state.service_state == Database::StartupServiceState::FATAL)
            {
                return "fatal_hold";
            }
            if (state.quarantine_active)
            {
                return "quarantine";
            }
            return "recovery";
        }

        auto writebackQueueKindName(WritebackQueueKind kind) -> const char*
        {
            switch (kind)
            {
                case WritebackQueueKind::UNKNOWN:
                    return "unknown";
                case WritebackQueueKind::FOREGROUND_HELP:
                    return "foreground_help";
                case WritebackQueueKind::BACKGROUND_AGE:
                    return "background_age";
                case WritebackQueueKind::CHECKPOINT:
                    return "checkpoint";
                case WritebackQueueKind::METADATA_PRIORITY:
                    return "metadata_priority";
                case WritebackQueueKind::WRITE_COMBINE:
                    return "write_combine";
                case WritebackQueueKind::REPAIR_RETRY:
                    return "repair_retry";
            }
            return "unknown";
        }

        auto writebackPolicyDomainName(WritebackPolicyDomain domain) -> const char*
        {
            switch (domain)
            {
                case WritebackPolicyDomain::UNKNOWN:
                    return "unknown";
                case WritebackPolicyDomain::TRANSACTION:
                    return "transaction";
                case WritebackPolicyDomain::CHECKPOINT:
                    return "checkpoint";
                case WritebackPolicyDomain::ALLOCATOR:
                    return "allocator";
                case WritebackPolicyDomain::CATALOG:
                    return "catalog";
                case WritebackPolicyDomain::SYSTEM_STATE:
                    return "system_state";
            }
            return "unknown";
        }

        auto writebackFailureClassName(WritebackFailureClass failure_class) -> const char*
        {
            switch (failure_class)
            {
                case WritebackFailureClass::NONE:
                    return "none";
                case WritebackFailureClass::RETRYABLE_WRITEBACK_IO:
                    return "retryable_writeback_io";
                case WritebackFailureClass::RETRYABLE_FSYNC_IO:
                    return "retryable_fsync_io";
                case WritebackFailureClass::DISK_FULL:
                    return "disk_full";
                case WritebackFailureClass::RESERVE_SPACE_EXHAUSTED:
                    return "reserve_space_exhausted";
                case WritebackFailureClass::CORRUPT_TARGET_PAGE:
                    return "corrupt_target_page";
                case WritebackFailureClass::FILESYSTEM_OFFLINE:
                    return "filesystem_offline";
                case WritebackFailureClass::WRITEBACK_TIMEOUT:
                    return "writeback_timeout";
            }
            return "unknown";
        }

        auto writebackDegradedStateName(WritebackDegradedState degraded_state) -> const char*
        {
            switch (degraded_state)
            {
                case WritebackDegradedState::NORMAL:
                    return "normal";
                case WritebackDegradedState::DEGRADED_READ_WRITE:
                    return "degraded_read_write";
                case WritebackDegradedState::WRITE_FENCED:
                    return "write_fenced";
                case WritebackDegradedState::FATAL:
                    return "fatal";
            }
            return "unknown";
        }

        auto statusName(Status status) -> std::string
        {
            switch (status)
            {
                case Status::OK:
                    return "OK";
                case Status::IO_ERROR:
                    return "IO_ERROR";
                case Status::PAGE_CORRUPT:
                    return "PAGE_CORRUPT";
                case Status::DATA_CORRUPTED:
                    return "DATA_CORRUPTED";
                case Status::INVALID_ARGUMENT:
                    return "INVALID_ARGUMENT";
                case Status::DISK_FULL:
                    return "DISK_FULL";
                case Status::INTERNAL_ERROR:
                    return "INTERNAL_ERROR";
                default:
                    return std::to_string(static_cast<int>(status));
            }
        }

        struct CheckpointTelemetryState
        {
            uint64_t checkpoint_generation = 0;
            CheckpointLifecycleState checkpoint_state = CheckpointLifecycleState::IDLE;
            uint64_t checkpoint_start_time = 0;
            uint64_t dirty_generation_low_watermark = 0;
            uint64_t dirty_generation_high_watermark = 0;
            uint64_t captured_flush_debt_pages = 0;
            bool queue_rebuild_required = false;
            Status checkpoint_failure_reason = Status::OK;
        };

        struct WritebackTelemetryState
        {
            bool incident_open = false;
            uint64_t retry_count = 0;
            WritebackDegradedState degraded_state = WritebackDegradedState::NORMAL;
            Status last_error_status = Status::OK;
        };

        auto readSystemStatePageLocal(const Database& db,
                                      BootstrapSystemStatePage* state_page_out) -> Status
        {
            if (state_page_out == nullptr)
            {
                return Status::INVALID_ARGUMENT;
            }

            std::vector<uint8_t> page_buffer(db.page_size(), 0);
            ErrorContext ctx;
            const Status status =
                db.read_page(BOOTSTRAP_PAGE_SYSTEM_STATE, page_buffer.data(), &ctx);
            if (status != Status::OK)
            {
                return status;
            }

            const auto* state_page =
                reinterpret_cast<const BootstrapSystemStatePage*>(page_buffer.data());
            if (state_page->page_header.page_type != PAGE_TYPE_SYSTEM_STATE ||
                state_page->page_header.page_size != db.page_size())
            {
                return Status::PAGE_CORRUPT;
            }

            *state_page_out = *state_page;
            return Status::OK;
        }

        void loadCheckpointTelemetryState(const BootstrapSystemStatePage& state_page,
                                          CheckpointTelemetryState* state_out)
        {
            if (state_out == nullptr)
            {
                return;
            }

            CheckpointTelemetryState state{};
            const uint64_t version =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_VERSION_SLOT];
            if (version == 0)
            {
                *state_out = state;
                return;
            }

            state.checkpoint_generation =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_GENERATION_SLOT];
            state.checkpoint_state = static_cast<CheckpointLifecycleState>(
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_STATE_SLOT]);
            state.checkpoint_start_time =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_START_TIME_SLOT];
            state.dirty_generation_low_watermark =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_DIRTY_LOW_SLOT];
            state.dirty_generation_high_watermark =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_DIRTY_HIGH_SLOT];
            state.captured_flush_debt_pages =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_FLUSH_DEBT_SLOT];
            state.queue_rebuild_required =
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_QUEUE_REBUILD_SLOT] != 0;
            state.checkpoint_failure_reason = static_cast<Status>(
                state_page.reserved[SYSTEM_STATE_CHECKPOINT_FAILURE_REASON_SLOT]);
            *state_out = state;
        }

        void loadWritebackTelemetryState(const BootstrapSystemStatePage& state_page,
                                         WritebackTelemetryState* state_out)
        {
            if (state_out == nullptr)
            {
                return;
            }

            WritebackTelemetryState state{};
            const uint64_t version =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_VERSION_SLOT];
            if (version == 0)
            {
                *state_out = state;
                return;
            }

            state.incident_open =
                (state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_FLAGS_SLOT] &
                 SYSTEM_STATE_WRITEBACK_INCIDENT_FLAG_OPEN) != 0;
            state.retry_count =
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_RETRY_COUNT_SLOT];
            state.degraded_state = static_cast<WritebackDegradedState>(
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_DEGRADED_STATE_SLOT]);
            state.last_error_status = static_cast<Status>(
                state_page.reserved[SYSTEM_STATE_WRITEBACK_INCIDENT_LAST_ERROR_STATUS_SLOT]);
            *state_out = state;
        }

        auto makeLabelsJson(const std::vector<MetricLabel>& labels) -> std::string
        {
            nlohmann::ordered_json doc = nlohmann::ordered_json::object();
            for (const MetricLabel& label : labels)
            {
                doc[label.name] = label.value;
            }
            return doc.dump();
        }

        auto makeMetricDefinition(std::string metric_name,
                                  MetricType metric_type,
                                  std::vector<std::string> label_names,
                                  std::string help,
                                  std::string unit) -> MetricSchemaDefinition
        {
            MetricSchemaDefinition definition{};
            definition.metric_name = std::move(metric_name);
            definition.metric_type = metric_type;
            definition.label_names = std::move(label_names);
            definition.help = std::move(help);
            definition.unit = std::move(unit);
            return definition;
        }

        auto makeColumn(std::string column_name, std::string column_type, bool nullable)
            -> SqlViewColumnDefinition
        {
            SqlViewColumnDefinition column{};
            column.column_name = std::move(column_name);
            column.column_type = std::move(column_type);
            column.nullable = nullable;
            return column;
        }

        auto makePanel(std::string panel_id,
                       std::string source_view,
                       std::vector<std::string> required_fields) -> DashboardPanelDefinition
        {
            DashboardPanelDefinition panel{};
            panel.panel_id = std::move(panel_id);
            panel.source_view = std::move(source_view);
            panel.required_fields = std::move(required_fields);
            return panel;
        }

        auto makeAlert(std::string alert_id, std::string predicate, std::string severity)
            -> DashboardAlertDefinition
        {
            DashboardAlertDefinition alert{};
            alert.alert_id = std::move(alert_id);
            alert.predicate = std::move(predicate);
            alert.severity = std::move(severity);
            return alert;
        }

        auto metricDefinitions() -> const std::vector<MetricSchemaDefinition>&
        {
            static const std::vector<MetricSchemaDefinition> kDefinitions = [] {
                std::vector<MetricSchemaDefinition> definitions{
                    makeMetricDefinition("sb_buf_commit_fence_backlog",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Pending commit-fence publication backlog.",
                                         "count"),
                    makeMetricDefinition("sb_checkpoint_dirty_boundary_pages",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Dirty pages captured at the checkpoint boundary.",
                                         "pages"),
                    makeMetricDefinition("sb_checkpoint_duration_seconds",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Elapsed duration of the current or latest checkpoint.",
                                         "seconds"),
                    makeMetricDefinition("sb_checkpoint_failed_total",
                                         MetricType::COUNTER,
                                         {"db", "reason"},
                                         "Checkpoint failures by reason.",
                                         "events"),
                    makeMetricDefinition("sb_checkpoint_flush_debt_pages",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Checkpoint flush debt pages captured for the current or latest checkpoint.",
                                         "pages"),
                    makeMetricDefinition("sb_checkpoint_generation_current",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Current or latest checkpoint generation.",
                                         "generation"),
                    makeMetricDefinition("sb_checkpoint_state",
                                         MetricType::GAUGE,
                                         {"db", "state"},
                                         "Current or latest checkpoint lifecycle state.",
                                         "state"),
                    makeMetricDefinition("sb_buf_evictions_by_class_total",
                                         MetricType::COUNTER,
                                         {"db", "class", "reason"},
                                         "Buffer evictions by MGA page class.",
                                         "frames"),
                    makeMetricDefinition("sb_buf_frames_by_class",
                                         MetricType::GAUGE,
                                         {"db", "class"},
                                         "Resident buffer frames by MGA page class.",
                                         "frames"),
                    makeMetricDefinition("sb_buf_gc_candidate_queue",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Queued GC candidate pages.",
                                         "pages"),
                    makeMetricDefinition("sb_buf_scan_probation_churn_total",
                                         MetricType::COUNTER,
                                         {"db", "class"},
                                         "Scan-resistance probation churn events.",
                                         "events"),
                    makeMetricDefinition("sb_gc_background_reclaim_bytes_total",
                                         MetricType::COUNTER,
                                         {"db", "relation"},
                                         "Bytes reclaimed by background GC.",
                                         "bytes"),
                    makeMetricDefinition("sb_gc_cleanup_debt_bytes",
                                         MetricType::GAUGE,
                                         {"db", "relation"},
                                         "Cleanup debt still retained on disk.",
                                         "bytes"),
                    makeMetricDefinition("sb_gc_cooperative_reclaim_bytes_total",
                                         MetricType::COUNTER,
                                         {"db", "relation"},
                                         "Bytes reclaimed by cooperative GC.",
                                         "bytes"),
                    makeMetricDefinition("sb_gc_index_backlog_entries",
                                         MetricType::GAUGE,
                                         {"db", "relation"},
                                         "Pending dead index entries awaiting cleanup.",
                                         "entries"),
                    makeMetricDefinition("sb_gc_sweep_generation",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Current sweep generation.",
                                         "generation"),
                    makeMetricDefinition("sb_gc_sweep_resumes_total",
                                         MetricType::COUNTER,
                                         {"db", "reason"},
                                         "Sweep resume events after interruption.",
                                         "events"),
                    makeMetricDefinition("sb_lock_blockers",
                                         MetricType::GAUGE,
                                         {"db", "wait_mode"},
                                         "Active blockers observed by wait mode.",
                                         "count"),
                    makeMetricDefinition("sb_lock_deadlocks_total",
                                         MetricType::COUNTER,
                                         {"db", "reason"},
                                         "Detected deadlocks.",
                                         "events"),
                    makeMetricDefinition("sb_lock_read_consistency_restarts_total",
                                         MetricType::COUNTER,
                                         {"db", "reason"},
                                         "Read-consistency restart outcomes.",
                                         "events"),
                    makeMetricDefinition("sb_lock_unique_conflicts_total",
                                         MetricType::COUNTER,
                                         {"db", "relation"},
                                         "Unique-key conflict outcomes.",
                                         "events"),
                    makeMetricDefinition("sb_lock_wait_seconds_total",
                                         MetricType::COUNTER,
                                         {"db", "wait_mode"},
                                         "Accumulated lock wait time.",
                                         "seconds"),
                    makeMetricDefinition("sb_mga_chain_depth_bucket",
                                         MetricType::GAUGE,
                                         {"db", "relation", "bucket"},
                                         "Version-chain depth bucket counts.",
                                         "count"),
                    makeMetricDefinition("sb_mga_chain_scatter_bucket",
                                         MetricType::GAUGE,
                                         {"db", "relation", "bucket"},
                                         "Version-chain scatter bucket counts.",
                                         "count"),
                    makeMetricDefinition("sb_mga_dead_space_bytes",
                                         MetricType::GAUGE,
                                         {"db", "relation"},
                                         "Dead space retained on relation pages.",
                                         "bytes"),
                    makeMetricDefinition("sb_mga_long_snapshot_count",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Long-lived active snapshots.",
                                         "count"),
                    makeMetricDefinition("sb_mga_oat",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Oldest active transaction boundary.",
                                         "txid"),
                    makeMetricDefinition("sb_mga_oit",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Oldest interesting transaction boundary.",
                                         "txid"),
                    makeMetricDefinition("sb_mga_ost",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Oldest snapshot transaction boundary.",
                                         "txid"),
                    makeMetricDefinition("sb_mga_retained_dead_bytes",
                                         MetricType::GAUGE,
                                         {"db", "relation"},
                                         "Dead bytes retained by old snapshots.",
                                         "bytes"),
                    makeMetricDefinition("sb_mga_rewrite_recommendations_total",
                                         MetricType::COUNTER,
                                         {"db", "relation", "reason"},
                                         "Rewrite recommendations issued by fragmentation policy.",
                                         "events"),
                    makeMetricDefinition("sb_mga_same_page_update_ratio",
                                         MetricType::GAUGE,
                                         {"db", "relation"},
                                         "Ratio of updates that remain on the same page.",
                                         "ratio"),
                    makeMetricDefinition("sb_mga_statement_restarts_total",
                                         MetricType::COUNTER,
                                         {"db", "reason"},
                                         "Statement restarts triggered by MGA visibility or conflicts.",
                                         "events"),
                    makeMetricDefinition("sb_recovery_classification_total",
                                         MetricType::COUNTER,
                                         {"db", "classification"},
                                         "Startup recovery classifications by occurrence.",
                                         "events"),
                    makeMetricDefinition("sb_recovery_generation_current",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Current recovery generation.",
                                         "generation"),
                    makeMetricDefinition("sb_recovery_repair_required_pages",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Repair-required pages found during startup reconciliation.",
                                         "pages"),
                    makeMetricDefinition("sb_recovery_startup_seconds",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Elapsed startup recovery duration.",
                                         "seconds"),
                    makeMetricDefinition("sb_tx_aborted_total",
                                         MetricType::COUNTER,
                                         {"db"},
                                         "Aborted transactions.",
                                         "transactions"),
                    makeMetricDefinition("sb_tx_active",
                                         MetricType::GAUGE,
                                         {"db"},
                                         "Currently active transactions.",
                                         "transactions"),
                    makeMetricDefinition("sb_tx_commit_fence_flush_seconds",
                                         MetricType::HISTOGRAM,
                                         {"db", "result"},
                                         "Commit-fence flush latency.",
                                         "seconds"),
                    makeMetricDefinition("sb_tx_committed_total",
                                         MetricType::COUNTER,
                                         {"db"},
                                         "Committed transactions.",
                                         "transactions"),
                    makeMetricDefinition("sb_tx_limbo",
                                         MetricType::GAUGE,
                                         {"db", "limbo_state"},
                                         "Transactions retained in limbo/prepared state.",
                                         "transactions"),
                    makeMetricDefinition("sb_tx_restart_normalized_total",
                                         MetricType::COUNTER,
                                         {"db", "reason"},
                                         "Transactions normalized during restart reconciliation.",
                                         "events"),
                    makeMetricDefinition("sb_writeback_incident_age_seconds",
                                         MetricType::GAUGE,
                                         {"db", "degraded_state"},
                                         "Age of the oldest open writeback incident by degraded state.",
                                         "seconds"),
                    makeMetricDefinition("sb_writeback_incidents_open",
                                         MetricType::GAUGE,
                                         {"db", "degraded_state"},
                                         "Open writeback incidents by degraded state.",
                                         "incidents"),
                };

                std::sort(definitions.begin(),
                          definitions.end(),
                          [](const MetricSchemaDefinition& lhs, const MetricSchemaDefinition& rhs) {
                              return lhs.metric_name < rhs.metric_name;
                          });
                return definitions;
            }();
            return kDefinitions;
        }

        auto sqlViewDefinitions() -> const std::vector<SqlViewSchemaDefinition>&
        {
            static const std::vector<SqlViewSchemaDefinition> kDefinitions = [] {
                std::vector<SqlViewSchemaDefinition> definitions;

                SqlViewSchemaDefinition active_transactions{};
                active_transactions.view_name = "sb_mga_active_transactions";
                active_transactions.schema_version = 1;
                active_transactions.purpose = "Active transaction inventory with retained-byte attribution.";
                active_transactions.columns = {
                    makeColumn("db_uuid", "UUID", false),
                    makeColumn("txid", "BIGINT", false),
                    makeColumn("state", "VARCHAR", false),
                    makeColumn("isolation_mode", "VARCHAR", false),
                    makeColumn("xmin", "BIGINT", true),
                    makeColumn("age_seconds", "DOUBLE", false),
                    makeColumn("retained_bytes", "BIGINT", false),
                    makeColumn("started_at_ms", "BIGINT", false),
                };
                definitions.push_back(std::move(active_transactions));

                SqlViewSchemaDefinition cleanup_debt{};
                cleanup_debt.view_name = "sb_mga_cleanup_debt";
                cleanup_debt.schema_version = 1;
                cleanup_debt.purpose = "Cleanup debt and rewrite recommendation summary by relation.";
                cleanup_debt.columns = {
                    makeColumn("db_uuid", "UUID", false),
                    makeColumn("relation_name", "VARCHAR", false),
                    makeColumn("cleanup_debt_bytes", "BIGINT", false),
                    makeColumn("retained_dead_bytes", "BIGINT", false),
                    makeColumn("chain_scatter_bucket", "VARCHAR", true),
                    makeColumn("rewrite_recommended", "BOOLEAN", false),
                    makeColumn("sweep_generation", "BIGINT", false),
                    makeColumn("observed_at_ms", "BIGINT", false),
                };
                definitions.push_back(std::move(cleanup_debt));

                SqlViewSchemaDefinition buffer_writeback_debt{};
                buffer_writeback_debt.view_name = "sb_buffer_writeback_debt";
                buffer_writeback_debt.schema_version = 1;
                buffer_writeback_debt.purpose =
                    "Checkpoint debt, writeback fence, and reserve-pressure summary.";
                buffer_writeback_debt.columns = {
                    makeColumn("db_uuid", "UUID", false),
                    makeColumn("checkpoint_generation", "BIGINT", false),
                    makeColumn("checkpoint_state", "VARCHAR", false),
                    makeColumn("dirty_pages", "BIGINT", false),
                    makeColumn("checkpoint_flush_debt_pages", "BIGINT", false),
                    makeColumn("checkpoint_pages_remaining", "BIGINT", false),
                    makeColumn("blocked_frame_count", "BIGINT", false),
                    makeColumn("write_admission_fenced", "BOOLEAN", false),
                    makeColumn("incident_open", "BOOLEAN", false),
                    makeColumn("retry_count", "BIGINT", false),
                    makeColumn("reserve_exhaustion_risk", "BOOLEAN", false),
                };
                definitions.push_back(std::move(buffer_writeback_debt));

                SqlViewSchemaDefinition checkpoint_history{};
                checkpoint_history.view_name = "sb_checkpoint_history";
                checkpoint_history.schema_version = 1;
                checkpoint_history.purpose =
                    "Checkpoint run history with dirty-boundary and failure metadata.";
                checkpoint_history.columns = {
                    makeColumn("checkpoint_run_uuid", "UUID", false),
                    makeColumn("checkpoint_generation", "BIGINT", false),
                    makeColumn("checkpoint_state", "VARCHAR", false),
                    makeColumn("start_time", "BIGINT", false),
                    makeColumn("end_time", "BIGINT", true),
                    makeColumn("dirty_generation_low_watermark", "BIGINT", false),
                    makeColumn("pages_target", "BIGINT", false),
                    makeColumn("pages_flushed", "BIGINT", false),
                    makeColumn("failure_reason", "VARCHAR", true),
                };
                definitions.push_back(std::move(checkpoint_history));

                SqlViewSchemaDefinition checkpoint_status{};
                checkpoint_status.view_name = "sb_checkpoint_status";
                checkpoint_status.schema_version = 1;
                checkpoint_status.purpose =
                    "Current or latest checkpoint control and progress surface.";
                checkpoint_status.columns = {
                    makeColumn("checkpoint_generation", "BIGINT", false),
                    makeColumn("checkpoint_state", "VARCHAR", false),
                    makeColumn("start_time", "BIGINT", false),
                    makeColumn("dirty_generation_low_watermark", "BIGINT", false),
                    makeColumn("dirty_generation_high_watermark", "BIGINT", false),
                    makeColumn("captured_flush_debt_pages", "BIGINT", false),
                    makeColumn("pages_target", "BIGINT", false),
                    makeColumn("pages_remaining", "BIGINT", false),
                    makeColumn("blocked_frame_count", "BIGINT", false),
                    makeColumn("queue_rebuild_required", "BOOLEAN", false),
                    makeColumn("failure_reason", "VARCHAR", true),
                };
                definitions.push_back(std::move(checkpoint_status));

                SqlViewSchemaDefinition failpoint_events{};
                failpoint_events.view_name = "sb_mga_failpoint_events";
                failpoint_events.schema_version = 1;
                failpoint_events.purpose = "Injected failpoint events and replay outcomes.";
                failpoint_events.columns = {
                    makeColumn("event_id", "VARCHAR", false),
                    makeColumn("seed_id", "VARCHAR", false),
                    makeColumn("trigger_name", "VARCHAR", false),
                    makeColumn("outcome", "VARCHAR", false),
                    makeColumn("db_uuid", "UUID", true),
                    makeColumn("txid", "BIGINT", true),
                    makeColumn("occurred_at_ms", "BIGINT", false),
                };
                definitions.push_back(std::move(failpoint_events));

                SqlViewSchemaDefinition runtime_metrics{};
                runtime_metrics.view_name = "sb_mga_runtime_metrics";
                runtime_metrics.schema_version = 1;
                runtime_metrics.purpose = "Canonical sb_* MGA metric samples.";
                runtime_metrics.columns = {
                    makeColumn("metric_name", "VARCHAR", false),
                    makeColumn("metric_type", "VARCHAR", false),
                    makeColumn("value", "DOUBLE", false),
                    makeColumn("labels_json", "JSON", false),
                    makeColumn("updated_at_ms", "BIGINT", false),
                };
                definitions.push_back(std::move(runtime_metrics));

                SqlViewSchemaDefinition recovery_incidents{};
                recovery_incidents.view_name = "sb_recovery_incidents";
                recovery_incidents.schema_version = 1;
                recovery_incidents.purpose =
                    "Persisted recovery incidents with checkpoint and object attribution.";
                recovery_incidents.columns = {
                    makeColumn("recovery_incident_uuid", "UUID", false),
                    makeColumn("recovery_generation", "BIGINT", false),
                    makeColumn("classification", "VARCHAR", false),
                    makeColumn("checkpoint_generation", "BIGINT", true),
                    makeColumn("object_uuid", "UUID", true),
                    makeColumn("details_json", "JSON", true),
                    makeColumn("created_time", "BIGINT", false),
                };
                definitions.push_back(std::move(recovery_incidents));

                SqlViewSchemaDefinition recovery_status{};
                recovery_status.view_name = "sb_recovery_status";
                recovery_status.schema_version = 1;
                recovery_status.purpose =
                    "Current startup recovery and degraded-mode status surface.";
                recovery_status.columns = {
                    makeColumn("recovery_generation", "BIGINT", false),
                    makeColumn("classification", "VARCHAR", false),
                    makeColumn("startup_state", "VARCHAR", false),
                    makeColumn("normalized_transactions", "BIGINT", false),
                    makeColumn("repair_required_pages", "BIGINT", false),
                    makeColumn("write_fenced", "BOOLEAN", false),
                    makeColumn("queue_rebuild_completed", "BOOLEAN", false),
                    makeColumn("warmup_mode", "VARCHAR", false),
                    makeColumn("start_time", "BIGINT", true),
                    makeColumn("end_time", "BIGINT", true),
                };
                definitions.push_back(std::move(recovery_status));

                SqlViewSchemaDefinition snapshot_blockers{};
                snapshot_blockers.view_name = "sb_mga_snapshot_blockers";
                snapshot_blockers.schema_version = 1;
                snapshot_blockers.purpose = "Long-snapshot blockers retaining OST and dead bytes.";
                snapshot_blockers.columns = {
                    makeColumn("db_uuid", "UUID", false),
                    makeColumn("blocker_txid", "BIGINT", false),
                    makeColumn("blocker_identity", "VARCHAR", false),
                    makeColumn("retained_bytes", "BIGINT", false),
                    makeColumn("snapshot_age_seconds", "DOUBLE", false),
                    makeColumn("ost_txid", "BIGINT", false),
                    makeColumn("observed_at_ms", "BIGINT", false),
                };
                definitions.push_back(std::move(snapshot_blockers));

                SqlViewSchemaDefinition transaction_history{};
                transaction_history.view_name = "sb_mga_transaction_history";
                transaction_history.schema_version = 1;
                transaction_history.purpose = "Transaction lifecycle history with horizon and fence latency.";
                transaction_history.columns = {
                    makeColumn("db_uuid", "UUID", false),
                    makeColumn("txid", "BIGINT", false),
                    makeColumn("state", "VARCHAR", false),
                    makeColumn("start_oit", "BIGINT", true),
                    makeColumn("end_oit", "BIGINT", true),
                    makeColumn("start_oat", "BIGINT", true),
                    makeColumn("end_oat", "BIGINT", true),
                    makeColumn("start_ost", "BIGINT", true),
                    makeColumn("end_ost", "BIGINT", true),
                    makeColumn("restart_count", "BIGINT", false),
                    makeColumn("publication_fence_seconds", "DOUBLE", true),
                    makeColumn("limbo_state", "VARCHAR", true),
                    makeColumn("started_at_ms", "BIGINT", false),
                    makeColumn("ended_at_ms", "BIGINT", true),
                };
                definitions.push_back(std::move(transaction_history));

                SqlViewSchemaDefinition sweep_resume_status{};
                sweep_resume_status.view_name = "sb_sweep_resume_status";
                sweep_resume_status.schema_version = 1;
                sweep_resume_status.purpose =
                    "Current or latest sweep resume cursor and rewind decision surface.";
                sweep_resume_status.columns = {
                    makeColumn("sweep_generation", "BIGINT", false),
                    makeColumn("relation_uuid", "UUID", false),
                    makeColumn("filespace_uuid", "UUID", false),
                    makeColumn("page_id", "BIGINT", false),
                    makeColumn("slot_id", "INTEGER", false),
                    makeColumn("checkpoint_generation_seen", "BIGINT", false),
                    makeColumn("persist_time", "BIGINT", false),
                    makeColumn("active", "BOOLEAN", false),
                    makeColumn("stage", "INTEGER", false),
                    makeColumn("resume_lane_mask", "INTEGER", false),
                    makeColumn("resume_strict_audit", "BOOLEAN", false),
                    makeColumn("start_horizon", "BIGINT", false),
                    makeColumn("reclaimed_version_count", "BIGINT", false),
                    makeColumn("reclaimed_bytes", "BIGINT", false),
                    makeColumn("index_backlog_count", "BIGINT", false),
                    makeColumn("cursor_crc32c", "BIGINT", false),
                    makeColumn("resume_outcome", "VARCHAR", false),
                };
                definitions.push_back(std::move(sweep_resume_status));

                SqlViewSchemaDefinition wait_history{};
                wait_history.view_name = "sb_mga_wait_history";
                wait_history.schema_version = 1;
                wait_history.purpose = "Blocker/victim wait history for MGA conflicts and deadlocks.";
                wait_history.columns = {
                    makeColumn("db_uuid", "UUID", false),
                    makeColumn("wait_event_id", "VARCHAR", false),
                    makeColumn("wait_mode", "VARCHAR", false),
                    makeColumn("blocker_txid", "BIGINT", true),
                    makeColumn("victim_txid", "BIGINT", true),
                    makeColumn("blocker_identity", "VARCHAR", true),
                    makeColumn("victim_identity", "VARCHAR", true),
                    makeColumn("wait_seconds", "DOUBLE", false),
                    makeColumn("outcome", "VARCHAR", false),
                    makeColumn("observed_at_ms", "BIGINT", false),
                };
                definitions.push_back(std::move(wait_history));

                SqlViewSchemaDefinition writeback_incidents{};
                writeback_incidents.view_name = "sb_writeback_incidents";
                writeback_incidents.schema_version = 1;
                writeback_incidents.purpose =
                    "Writeback incident history with queue, domain, and degraded-state attribution.";
                writeback_incidents.columns = {
                    makeColumn("incident_uuid", "UUID", false),
                    makeColumn("filespace_uuid", "UUID", true),
                    makeColumn("queue_kind", "VARCHAR", false),
                    makeColumn("policy_domain", "VARCHAR", false),
                    makeColumn("page_class", "BIGINT", false),
                    makeColumn("failure_class", "VARCHAR", false),
                    makeColumn("first_seen_time", "BIGINT", false),
                    makeColumn("last_seen_time", "BIGINT", false),
                    makeColumn("retry_count", "BIGINT", false),
                    makeColumn("degraded_state", "VARCHAR", false),
                    makeColumn("clearance_condition", "UUID", true),
                    makeColumn("is_open", "BOOLEAN", false),
                    makeColumn("last_error_status", "BIGINT", false),
                };
                definitions.push_back(std::move(writeback_incidents));

                std::sort(definitions.begin(),
                          definitions.end(),
                          [](const SqlViewSchemaDefinition& lhs, const SqlViewSchemaDefinition& rhs) {
                              return lhs.view_name < rhs.view_name;
                          });
                return definitions;
            }();
            return kDefinitions;
        }

        auto dashboardDefinitions() -> const std::vector<DashboardSchemaDefinition>&
        {
            static const std::vector<DashboardSchemaDefinition> kDefinitions = [] {
                std::vector<DashboardSchemaDefinition> definitions;

                DashboardSchemaDefinition chain_locality{};
                chain_locality.dashboard_id = "sb_mga_chain_locality_fragmentation";
                chain_locality.schema_version = 1;
                chain_locality.title = "Chain locality and fragmentation";
                chain_locality.panels = {
                    makePanel("chain_depth_distribution",
                              "sb_mga_runtime_metrics",
                              {"metric_name", "labels_json", "value"}),
                    makePanel("dead_space_by_relation",
                              "sb_mga_cleanup_debt",
                              {"relation_name", "cleanup_debt_bytes", "retained_dead_bytes"}),
                };
                chain_locality.alerts = {
                    makeAlert("cleanup_debt_ratio",
                              "cleanup_debt_bytes > 10% object_size for 2 sweep intervals",
                              "WARN"),
                };
                definitions.push_back(std::move(chain_locality));

                DashboardSchemaDefinition cleanup_debt{};
                cleanup_debt.dashboard_id = "sb_mga_cleanup_debt_and_sweep_debt";
                cleanup_debt.schema_version = 1;
                cleanup_debt.title = "Cleanup debt and sweep debt";
                cleanup_debt.panels = {
                    makePanel("cleanup_debt_summary",
                              "sb_mga_cleanup_debt",
                              {"relation_name", "cleanup_debt_bytes", "sweep_generation"}),
                    makePanel("runtime_cleanup_metric",
                              "sb_mga_runtime_metrics",
                              {"metric_name", "value", "updated_at_ms"}),
                };
                cleanup_debt.alerts = {
                    makeAlert("cleanup_debt_growth",
                              "cleanup_debt_bytes > 10% object_size for 2 sweep intervals",
                              "WARN"),
                };
                definitions.push_back(std::move(cleanup_debt));

                DashboardSchemaDefinition conflict_storm{};
                conflict_storm.dashboard_id = "sb_mga_conflict_storm_deadlock_heatmap";
                conflict_storm.schema_version = 1;
                conflict_storm.title = "Conflict storm and deadlock heatmap";
                conflict_storm.panels = {
                    makePanel("wait_history_heatmap",
                              "sb_mga_wait_history",
                              {"wait_mode", "wait_seconds", "outcome"}),
                    makePanel("blocker_counts",
                              "sb_mga_runtime_metrics",
                              {"metric_name", "labels_json", "value"}),
                };
                conflict_storm.alerts = {
                    makeAlert("deadlock_rate_baseline",
                              "deadlock rate exceeds configured baseline multiplier",
                              "WARN"),
                };
                definitions.push_back(std::move(conflict_storm));

                DashboardSchemaDefinition long_snapshot{};
                long_snapshot.dashboard_id = "sb_mga_long_snapshot_blockers";
                long_snapshot.schema_version = 1;
                long_snapshot.title = "Long-snapshot blocker list";
                long_snapshot.panels = {
                    makePanel("blocker_list",
                              "sb_mga_snapshot_blockers",
                              {"blocker_txid", "blocker_identity", "retained_bytes", "snapshot_age_seconds"}),
                    makePanel("active_transaction_inventory",
                              "sb_mga_active_transactions",
                              {"txid", "state", "retained_bytes", "age_seconds"}),
                };
                long_snapshot.alerts = {
                    makeAlert("long_snapshot_age",
                              "snapshot_age_seconds > 300",
                              "WARN"),
                    makeAlert("long_snapshot_retained_bytes",
                              "retained_bytes > 1073741824",
                              "WARN"),
                };
                definitions.push_back(std::move(long_snapshot));

                DashboardSchemaDefinition restart_anomalies{};
                restart_anomalies.dashboard_id = "sb_mga_restart_crash_window_anomalies";
                restart_anomalies.schema_version = 1;
                restart_anomalies.title = "Restart and crash-window anomaly summary";
                restart_anomalies.panels = {
                    makePanel("restart_normalization",
                              "sb_mga_runtime_metrics",
                              {"metric_name", "labels_json", "value"}),
                    makePanel("failpoint_event_log",
                              "sb_mga_failpoint_events",
                              {"seed_id", "trigger_name", "outcome", "occurred_at_ms"}),
                    makePanel("transaction_history",
                              "sb_mga_transaction_history",
                              {"txid", "state", "restart_count", "publication_fence_seconds"}),
                };
                restart_anomalies.alerts = {
                    makeAlert("commit_fence_backlog_age",
                              "commit fence backlog older than 2 s",
                              "WARN"),
                };
                definitions.push_back(std::move(restart_anomalies));

                std::sort(definitions.begin(),
                          definitions.end(),
                          [](const DashboardSchemaDefinition& lhs, const DashboardSchemaDefinition& rhs) {
                              return lhs.dashboard_id < rhs.dashboard_id;
                          });
                return definitions;
            }();
            return kDefinitions;
        }

    } // namespace

    auto MetricContractPolicy::isCanonicalMetricName(std::string_view metric_name) -> bool
    {
        static const std::unordered_set<std::string> kAllowedSubsystems = {
            "engine",
            "cluster",
            "auth",
            "storage",
            "driver",
            "migration",
            "udr",
            "net",
            "cache",
            "planner",
            "exec",
            "tx",
            "mga",
            "gc",
            "buf",
            "lock",
            "checkpoint",
            "recovery",
            "writeback",
        };

        if (metric_name.size() < 6 || metric_name.substr(0, 3) != "sb_")
        {
            return false;
        }

        for (char c : metric_name)
        {
            const bool valid = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
            if (!valid)
            {
                return false;
            }
        }

        const auto tokens = splitByUnderscore(metric_name);
        if (tokens.size() < 3 || tokens[0] != "sb")
        {
            return false;
        }

        return kAllowedSubsystems.find(tokens[1]) != kAllowedSubsystems.end();
    }

    auto MetricContractPolicy::isAllowedLabelName(std::string_view label_name) -> bool
    {
        static const std::unordered_set<std::string> kAllowedLabels = {
            "db",
            "shard",
            "node",
            "protocol",
            "driver",
            "result",
            "reason",
            "relation",
            "class",
            "classification",
            "bucket",
            "state",
            "degraded_state",
            "wait_mode",
            "limbo_state",
            "le",
        };
        return kAllowedLabels.find(toLower(label_name)) != kAllowedLabels.end();
    }

    auto MetricContractPolicy::isForbiddenLabelName(std::string_view label_name) -> bool
    {
        static const std::unordered_set<std::string> kForbiddenLabels = {
            "raw_sql",
            "sql",
            "session_id",
            "user_id",
            "client_ip",
            "table_id",
            "index_id",
            "object_uuid",
            "object_id",
        };
        return kForbiddenLabels.find(toLower(label_name)) != kForbiddenLabels.end();
    }

    auto MetricContractPolicy::validateSample(const MetricSampleRow& sample,
                                              std::vector<std::string>& reasons_out) -> bool
    {
        reasons_out.clear();
        if (!isCanonicalMetricName(sample.metric_name))
        {
            reasons_out.push_back("metric_name_non_canonical");
        }

        for (const MetricLabel& label : sample.labels)
        {
            if (isForbiddenLabelName(label.name))
            {
                reasons_out.push_back("forbidden_label:" + label.name);
            }
            else if (!isAllowedLabelName(label.name))
            {
                reasons_out.push_back("label_not_in_allowlist:" + label.name);
            }
        }
        return reasons_out.empty();
    }

    auto MetricContractPolicy::auditRegistry(const MetricsRegistry& registry,
                                             std::vector<MetricPolicyViolation>& violations_out) -> Status
    {
        violations_out.clear();
        const std::vector<MetricSampleRow> rows = registry.snapshotSamples();
        std::vector<std::string> reasons;
        for (const MetricSampleRow& row : rows)
        {
            if (validateSample(row, reasons))
            {
                continue;
            }
            for (const std::string& reason : reasons)
            {
                violations_out.push_back(MetricPolicyViolation{row.metric_name, reason});
            }
        }

        std::sort(violations_out.begin(), violations_out.end(),
                  [](const MetricPolicyViolation& lhs, const MetricPolicyViolation& rhs) {
                      if (lhs.metric_name != rhs.metric_name)
                      {
                          return lhs.metric_name < rhs.metric_name;
                      }
                      return lhs.reason < rhs.reason;
                  });
        return Status::OK;
    }

    auto MetricContractPolicy::registerSbObsBaselineMetrics(MetricsRegistry& registry) -> Status
    {
        registry.registerGauge("sb_engine_connections_active",
                               "Active engine connections",
                               {"db"});
        registry.registerCounter("sb_engine_connections_total",
                                 "Total engine connections",
                                 {"db", "result"});
        registry.registerGauge("sb_engine_sessions_active",
                               "Active engine sessions",
                               {"db"});
        registry.registerCounter("sb_engine_sessions_total",
                                 "Total engine sessions",
                                 {"db", "result"});
        registry.registerCounter("sb_engine_queries_total",
                                 "Total engine queries",
                                 {"db", "result"});
        registry.registerHistogram("sb_engine_query_duration_seconds",
                                   "Engine query latency",
                                   Histogram::DEFAULT_LATENCY_BUCKETS,
                                   {"db", "result"});

        registry.registerGauge("sb_cluster_leader_term",
                               "Current leader term by shard",
                               {"db", "shard", "node"});
        registry.registerGauge("sb_cluster_lease_seconds_remaining",
                               "Leader lease remaining by shard",
                               {"db", "shard", "node"});
        registry.registerCounter("sb_cluster_fencing_rejections_total",
                                 "Write rejections due to fencing checks",
                                 {"db", "shard", "reason"});
        registry.registerCounter("sb_cluster_routing_requests_total",
                                 "Cluster routing requests",
                                 {"db", "protocol", "result"});
        registry.registerGauge("sb_cluster_routing_epoch",
                               "Cluster routing epoch",
                               {"db"});
        registry.registerGauge("sb_cluster_replication_lag_txn",
                               "Replication lag in txns",
                               {"db", "shard"});
        registry.registerGauge("sb_cluster_replication_lag_seconds",
                               "Replication lag in seconds",
                               {"db", "shard"});
        registry.registerCounter("sb_cluster_replication_apply_total",
                                 "Follower apply attempts",
                                 {"db", "shard", "result"});
        registry.registerHistogram("sb_cluster_replication_apply_seconds",
                                   "Follower apply latency",
                                   Histogram::DEFAULT_LATENCY_BUCKETS,
                                   {"db", "shard"});
        registry.registerGauge("sb_cluster_cwm_txn",
                               "Committed watermark by shard",
                               {"db", "shard"});
        registry.registerGauge("sb_cluster_ost_txn",
                               "Oldest snapshot transaction boundary by shard",
                               {"db", "shard"});
        registry.registerGauge("sb_cluster_rwm_txn",
                               "Replication watermark by shard",
                               {"db", "shard"});
        registry.registerGauge("sb_cluster_gc_safe_horizon_txn",
                               "GC safe horizon by shard",
                               {"db", "shard"});
        registry.registerGauge("sb_cluster_snapshots_active",
                               "Active snapshots by shard",
                               {"db", "shard"});
        registry.registerCounter("sb_cluster_snapshot_heartbeats_total",
                                 "Snapshot heartbeat updates by shard",
                                 {"db", "shard"});
        return Status::OK;
    }

    auto MetricContractPolicy::buildLegacyNameMapping(
        std::vector<std::pair<std::string, std::string>>& mapping_out) -> Status
    {
        mapping_out.clear();
        mapping_out = {
            {"scratchbird_queries_total", "sb_engine_queries_total"},
            {"scratchbird_query_duration_seconds", "sb_engine_query_duration_seconds"},
            {"scratchbird_connections_active", "sb_engine_connections_active"},
            {"scratchbird_connections_total", "sb_engine_connections_total"},
            {"scratchbird_transactions_active", "sb_engine_sessions_active"},
            {"scratchbird_transactions_total", "sb_engine_sessions_total"},
            {"scratchbird_lock_wait_seconds", "sb_storage_io_seconds{op=\"lock_wait\"}"},
            {"scratchbird_disk_read_bytes_total", "sb_storage_page_reads_total"},
            {"scratchbird_disk_write_bytes_total", "sb_storage_page_writes_total"},
            {"scratchbird_buffer_pool_hits_total", "sb_storage_buffer_pool_hit_total"},
            {"scratchbird_buffer_pool_misses_total", "sb_storage_buffer_pool_miss_total"},
            {"scratchbird_translation_cache_hits_total", "sb_cache_translation_hits_total"},
            {"scratchbird_translation_cache_misses_total", "sb_cache_translation_misses_total"},
            {"scratchbird_statement_cache_hits_total", "sb_cache_statement_hits_total"},
            {"scratchbird_statement_cache_misses_total", "sb_cache_statement_misses_total"},
        };

        std::sort(mapping_out.begin(), mapping_out.end());
        return Status::OK;
    }

    auto MgaObservabilityContract::contract_id() -> const char*
    {
        return "sb_mga_observability/v1";
    }

    auto MgaObservabilityContract::metric_schema_version() -> uint32_t
    {
        return 1;
    }

    auto MgaObservabilityContract::sql_view_schema_version() -> uint32_t
    {
        return 1;
    }

    auto MgaObservabilityContract::dashboard_schema_version() -> uint32_t
    {
        return 1;
    }

    auto MgaObservabilityContract::appendMetricDefinitions(
        std::vector<MetricSchemaDefinition>& definitions_out) -> Status
    {
        definitions_out = metricDefinitions();
        return Status::OK;
    }

    auto MgaObservabilityContract::appendSqlViewDefinitions(
        std::vector<SqlViewSchemaDefinition>& definitions_out) -> Status
    {
        definitions_out = sqlViewDefinitions();
        return Status::OK;
    }

    auto MgaObservabilityContract::appendDashboardDefinitions(
        std::vector<DashboardSchemaDefinition>& definitions_out) -> Status
    {
        definitions_out = dashboardDefinitions();
        return Status::OK;
    }

    auto MgaObservabilityContract::registerRequiredMetrics(MetricsRegistry& registry) -> Status
    {
        const auto& definitions = metricDefinitions();
        for (const MetricSchemaDefinition& definition : definitions)
        {
            Metric* existing = registry.get(definition.metric_name);
            if (existing != nullptr)
            {
                if (existing->type() != definition.metric_type)
                {
                    return Status::INVALID_ARGUMENT;
                }
                continue;
            }
            switch (definition.metric_type)
            {
                case MetricType::COUNTER:
                    registry.registerCounter(
                        definition.metric_name, definition.help, definition.label_names);
                    break;
                case MetricType::GAUGE:
                    registry.registerGauge(
                        definition.metric_name, definition.help, definition.label_names);
                    break;
                case MetricType::HISTOGRAM:
                    registry.registerHistogram(
                        definition.metric_name,
                        definition.help,
                        Histogram::DEFAULT_LATENCY_BUCKETS,
                        definition.label_names);
                    break;
                case MetricType::SUMMARY:
                    return Status::INVALID_ARGUMENT;
            }
        }
        return Status::OK;
    }

    auto MgaObservabilityContract::verifyRegistryContainsRequiredMetrics(
        const MetricsRegistry& registry,
        std::vector<std::string>& missing_metrics_out) -> Status
    {
        missing_metrics_out.clear();
        for (const MetricSchemaDefinition& definition : metricDefinitions())
        {
            Metric* metric =
                const_cast<MetricsRegistry&>(registry).get(definition.metric_name);
            if (metric == nullptr)
            {
                missing_metrics_out.push_back(definition.metric_name);
                continue;
            }
            if (metric->type() != definition.metric_type)
            {
                missing_metrics_out.push_back(definition.metric_name + ":type_mismatch");
            }
        }
        return Status::OK;
    }

    auto toString(HealthComponentStatus status) -> const char*
    {
        switch (status)
        {
            case HealthComponentStatus::OK:
                return "OK";
            case HealthComponentStatus::WARN:
                return "WARN";
            case HealthComponentStatus::FAIL:
                return "FAIL";
        }
        return "UNKNOWN";
    }

    auto SqlObservabilityViewBuilder::buildRuntimeRows(const MetricsRegistry& registry,
                                                       uint64_t updated_at_ms,
                                                       std::vector<SqlRuntimeMetricRow>& rows_out) -> Status
    {
        rows_out.clear();
        const std::vector<MetricSampleRow> samples = registry.snapshotSamples();
        rows_out.reserve(samples.size());

        for (const MetricSampleRow& sample : samples)
        {
            nlohmann::ordered_json labels = nlohmann::ordered_json::object();
            for (const MetricLabel& label : sample.labels)
            {
                labels[label.name] = label.value;
            }

            SqlRuntimeMetricRow row{};
            row.metric_name = sample.metric_name;
            Metric* metric = const_cast<MetricsRegistry&>(registry).get(sample.metric_name);
            row.metric_type = metric ? typeToString(metric->type()) : inferTypeFromSampleName(sample.metric_name);
            row.value = sample.value;
            row.labels_json = labels.dump();
            row.updated_at = updated_at_ms;
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(), rows_out.end(),
                  [](const SqlRuntimeMetricRow& lhs, const SqlRuntimeMetricRow& rhs) {
                      if (lhs.metric_name != rhs.metric_name)
                      {
                          return lhs.metric_name < rhs.metric_name;
                      }
                      if (lhs.labels_json != rhs.labels_json)
                      {
                          return lhs.labels_json < rhs.labels_json;
                      }
                      return lhs.value < rhs.value;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildMgaRuntimeRows(const Database& db,
                                                          const MetricsRegistry& registry,
                                                          uint64_t updated_at_ms,
                                                          std::vector<SqlRuntimeMetricRow>& rows_out) -> Status
    {
        rows_out.clear();

        std::unordered_set<std::string> required_metric_names;
        for (const MetricSchemaDefinition& definition : metricDefinitions())
        {
            required_metric_names.insert(definition.metric_name);
        }

        std::map<std::string, SqlRuntimeMetricRow> row_map;
        auto add_row = [&row_map](SqlRuntimeMetricRow row) {
            row_map[row.metric_name + "\n" + row.labels_json] = std::move(row);
        };

        const std::vector<MetricSampleRow> samples = registry.snapshotSamples();
        for (const MetricSampleRow& sample : samples)
        {
            if (required_metric_names.find(sample.metric_name) == required_metric_names.end())
            {
                continue;
            }

            SqlRuntimeMetricRow row{};
            row.metric_name = sample.metric_name;
            Metric* metric = const_cast<MetricsRegistry&>(registry).get(sample.metric_name);
            row.metric_type = metric ? typeToString(metric->type()) : inferTypeFromSampleName(sample.metric_name);
            row.value = sample.value;
            row.labels_json = makeLabelsJson(sample.labels);
            row.updated_at = updated_at_ms;
            add_row(std::move(row));
        }

        const std::string db_uuid = dbUuidString(db);
        auto db_labels_json = [&db_uuid]() {
            return makeLabelsJson({{"db", db_uuid}});
        };
        auto db_wait_labels_json = [&db_uuid](const std::string& wait_mode) {
            return makeLabelsJson({{"db", db_uuid}, {"wait_mode", wait_mode}});
        };
        auto db_relation_labels_json = [&db_uuid](const std::string& relation) {
            return makeLabelsJson({{"db", db_uuid}, {"relation", relation}});
        };

        std::vector<SqlMgaActiveTransactionRow> active_rows;
        Status status = buildMgaActiveTransactionRows(db, updated_at_ms, active_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlMgaCleanupDebtRow> cleanup_rows;
        status = buildMgaCleanupDebtRows(db, registry, updated_at_ms, cleanup_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlMgaSnapshotBlockerRow> blocker_rows;
        status = buildMgaSnapshotBlockerRows(db, registry, updated_at_ms, blocker_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlMgaTransactionHistoryRow> history_rows;
        status = buildMgaTransactionHistoryRows(db, history_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlCheckpointStatusRow> checkpoint_status_rows;
        status = buildCheckpointStatusRows(db, checkpoint_status_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlCheckpointHistoryRow> checkpoint_history_rows;
        status = buildCheckpointHistoryRows(db, checkpoint_history_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlRecoveryStatusRow> recovery_status_rows;
        status = buildRecoveryStatusRows(db, recovery_status_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlRecoveryIncidentRow> recovery_incident_rows;
        status = buildRecoveryIncidentRows(db, recovery_incident_rows);
        if (status != Status::OK)
        {
            return status;
        }
        (void)recovery_incident_rows;

        std::vector<SqlWritebackIncidentRow> writeback_incident_rows;
        status = buildWritebackIncidentRows(db, writeback_incident_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlBufferWritebackDebtRow> writeback_debt_rows;
        status = buildBufferWritebackDebtRows(db, writeback_debt_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlSweepResumeStatusRow> sweep_resume_rows;
        status = buildSweepResumeStatusRows(db, sweep_resume_rows);
        if (status != Status::OK)
        {
            return status;
        }
        (void)sweep_resume_rows;

        std::vector<SqlMgaWaitHistoryRow> wait_rows;
        status = buildMgaWaitHistoryRows(db, wait_rows);
        if (status != Status::OK)
        {
            return status;
        }

        if (const auto* buffer_pool = db.buffer_pool())
        {
            const auto stats = buffer_pool->getStats();
            add_row(SqlRuntimeMetricRow{
                "sb_buf_commit_fence_backlog",
                "gauge",
                static_cast<double>(stats.mga_commit_fence_backlog),
                db_labels_json(),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_buf_gc_candidate_queue",
                "gauge",
                static_cast<double>(stats.mga_frames_gc_candidate),
                db_labels_json(),
                updated_at_ms});

            const std::array<std::pair<const char*, uint64_t>, 6> frame_counts{{
                {"tx_state", stats.mga_frames_tx_state},
                {"version_root", stats.mga_frames_version_root},
                {"chain_heavy", stats.mga_frames_chain_heavy},
                {"gc_candidate", stats.mga_frames_gc_candidate},
                {"scan_probation", stats.mga_frames_scan_probation},
                {"index_churn", stats.mga_frames_index_churn},
            }};
            for (const auto& [klass, count] : frame_counts)
            {
                add_row(SqlRuntimeMetricRow{
                    "sb_buf_frames_by_class",
                    "gauge",
                    static_cast<double>(count),
                    makeLabelsJson({{"db", db_uuid}, {"class", klass}}),
                    updated_at_ms});
            }

            const std::array<std::pair<const char*, uint64_t>, 6> eviction_counts{{
                {"tx_state", stats.mga_evictions_tx_state},
                {"version_root", stats.mga_evictions_version_root},
                {"chain_heavy", stats.mga_evictions_chain_heavy},
                {"gc_candidate", stats.mga_evictions_gc_candidate},
                {"scan_probation", stats.mga_evictions_scan_probation},
                {"index_churn", stats.mga_evictions_index_churn},
            }};
            for (const auto& [klass, count] : eviction_counts)
            {
                add_row(SqlRuntimeMetricRow{
                    "sb_buf_evictions_by_class_total",
                    "counter",
                    static_cast<double>(count),
                    makeLabelsJson(
                        {{"db", db_uuid}, {"class", klass}, {"reason", "capacity"}}),
                    updated_at_ms});
            }

            add_row(SqlRuntimeMetricRow{
                "sb_buf_scan_probation_churn_total",
                "counter",
                static_cast<double>(stats.mga_scan_probation_churn),
                makeLabelsJson({{"db", db_uuid}, {"class", "scan_probation"}}),
                updated_at_ms});
        }

        if (const auto* gc = db.garbage_collector())
        {
            const auto gc_stats = gc->getStatistics();
            const std::string db_total_relation = "__database__";
            add_row(SqlRuntimeMetricRow{
                "sb_gc_cooperative_reclaim_bytes_total",
                "counter",
                static_cast<double>(gc_stats.cooperative_reclaimed_bytes),
                db_relation_labels_json(db_total_relation),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_gc_background_reclaim_bytes_total",
                "counter",
                static_cast<double>(gc_stats.background_reclaimed_bytes),
                db_relation_labels_json(db_total_relation),
                updated_at_ms});
        }

        if (const auto* txn_mgr = db.transaction_manager())
        {
            add_row(SqlRuntimeMetricRow{
                "sb_mga_oit", "gauge", static_cast<double>(txn_mgr->getOldestXid()),
                db_labels_json(), updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_mga_oat", "gauge", static_cast<double>(txn_mgr->getOldestActiveXid()),
                db_labels_json(), updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_mga_ost", "gauge", static_cast<double>(txn_mgr->getOldestSnapshot()),
                db_labels_json(), updated_at_ms});
        }

        size_t active_count = 0;
        size_t limbo_count = 0;
        for (const SqlMgaActiveTransactionRow& row : active_rows)
        {
            if (row.state == "PREPARED")
            {
                ++limbo_count;
            }
            else if (row.state == "IN_PROGRESS")
            {
                ++active_count;
            }
        }

        add_row(SqlRuntimeMetricRow{
            "sb_tx_active", "gauge", static_cast<double>(active_count),
            db_labels_json(), updated_at_ms});
        add_row(SqlRuntimeMetricRow{
            "sb_tx_limbo", "gauge", static_cast<double>(limbo_count),
            makeLabelsJson({{"db", db_uuid}, {"limbo_state", "prepared"}}), updated_at_ms});
        add_row(SqlRuntimeMetricRow{
            "sb_mga_long_snapshot_count", "gauge", static_cast<double>(blocker_rows.size()),
            db_labels_json(), updated_at_ms});

        uint64_t committed_total = 0;
        uint64_t aborted_total = 0;
        for (const SqlMgaTransactionHistoryRow& row : history_rows)
        {
            if (row.state == "COMMITTED")
            {
                ++committed_total;
            }
            else if (row.state == "ABORTED")
            {
                ++aborted_total;
            }
        }
        add_row(SqlRuntimeMetricRow{
            "sb_tx_committed_total", "counter", static_cast<double>(committed_total),
            db_labels_json(), updated_at_ms});
        add_row(SqlRuntimeMetricRow{
            "sb_tx_aborted_total", "counter", static_cast<double>(aborted_total),
            db_labels_json(), updated_at_ms});

        if (!checkpoint_status_rows.empty())
        {
            const auto& checkpoint = checkpoint_status_rows.front();
            add_row(SqlRuntimeMetricRow{
                "sb_checkpoint_generation_current",
                "gauge",
                static_cast<double>(checkpoint.checkpoint_generation),
                db_labels_json(),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_checkpoint_state",
                "gauge",
                static_cast<double>(checkpoint.checkpoint_generation),
                makeLabelsJson({{"db", db_uuid}, {"state", checkpoint.checkpoint_state}}),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_checkpoint_dirty_boundary_pages",
                "gauge",
                static_cast<double>(checkpoint.pages_target),
                db_labels_json(),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_checkpoint_flush_debt_pages",
                "gauge",
                static_cast<double>(checkpoint.captured_flush_debt_pages),
                db_labels_json(),
                updated_at_ms});

            double checkpoint_duration_seconds = 0.0;
            if (!checkpoint_history_rows.empty())
            {
                const auto& latest = checkpoint_history_rows.front();
                const uint64_t end_time =
                    latest.has_end_time ? latest.end_time : updated_at_ms * 1000;
                if (end_time > latest.start_time)
                {
                    checkpoint_duration_seconds =
                        microsToSeconds(end_time - latest.start_time);
                }
            }
            add_row(SqlRuntimeMetricRow{
                "sb_checkpoint_duration_seconds",
                "gauge",
                checkpoint_duration_seconds,
                db_labels_json(),
                updated_at_ms});
        }

        std::unordered_map<std::string, double> checkpoint_failures_by_reason;
        for (const auto& row : checkpoint_history_rows)
        {
            if (row.has_failure_reason)
            {
                checkpoint_failures_by_reason[row.failure_reason] += 1.0;
            }
        }
        for (const auto& [reason, count] : checkpoint_failures_by_reason)
        {
            add_row(SqlRuntimeMetricRow{
                "sb_checkpoint_failed_total",
                "counter",
                count,
                makeLabelsJson({{"db", db_uuid}, {"reason", reason}}),
                updated_at_ms});
        }

        if (!recovery_status_rows.empty())
        {
            const auto& recovery = recovery_status_rows.front();
            add_row(SqlRuntimeMetricRow{
                "sb_recovery_generation_current",
                "gauge",
                static_cast<double>(recovery.recovery_generation),
                db_labels_json(),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_recovery_repair_required_pages",
                "gauge",
                static_cast<double>(recovery.repair_required_pages),
                db_labels_json(),
                updated_at_ms});

            double recovery_duration_seconds = 0.0;
            if (recovery.has_start_time)
            {
                const uint64_t end_time =
                    recovery.has_end_time ? recovery.end_time : updated_at_ms * 1000;
                if (end_time > recovery.start_time)
                {
                    recovery_duration_seconds =
                        microsToSeconds(end_time - recovery.start_time);
                }
            }
            add_row(SqlRuntimeMetricRow{
                "sb_recovery_startup_seconds",
                "gauge",
                recovery_duration_seconds,
                db_labels_json(),
                updated_at_ms});
        }

        std::unordered_map<std::string, double> recovery_classifications;
        if (CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager()))
        {
            std::vector<CatalogManager::RecoveryRunCatalogInfo> recovery_runs;
            const Status recovery_status =
                catalog->listRecoveryRunCatalogEntries(recovery_runs, nullptr);
            if (recovery_status != Status::OK && recovery_status != Status::NOT_FOUND)
            {
                return recovery_status;
            }
            for (const auto& run : recovery_runs)
            {
                recovery_classifications[startupClassificationName(run.classification)] += 1.0;
            }
        }
        for (const auto& [classification, count] : recovery_classifications)
        {
            add_row(SqlRuntimeMetricRow{
                "sb_recovery_classification_total",
                "counter",
                count,
                makeLabelsJson({{"db", db_uuid}, {"classification", classification}}),
                updated_at_ms});
        }

        std::unordered_map<std::string, double> open_writeback_incidents;
        std::unordered_map<std::string, double> writeback_age_seconds;
        for (const auto& incident : writeback_incident_rows)
        {
            if (!incident.is_open)
            {
                continue;
            }
            open_writeback_incidents[incident.degraded_state] += 1.0;
            if (incident.first_seen_time != 0 && updated_at_ms * 1000 > incident.first_seen_time)
            {
                const double age =
                    microsToSeconds(updated_at_ms * 1000 - incident.first_seen_time);
                auto& oldest = writeback_age_seconds[incident.degraded_state];
                oldest = std::max(oldest, age);
            }
        }
        for (const auto& [degraded_state, count] : open_writeback_incidents)
        {
            add_row(SqlRuntimeMetricRow{
                "sb_writeback_incidents_open",
                "gauge",
                count,
                makeLabelsJson({{"db", db_uuid}, {"degraded_state", degraded_state}}),
                updated_at_ms});
        }
        for (const auto& [degraded_state, age] : writeback_age_seconds)
        {
            add_row(SqlRuntimeMetricRow{
                "sb_writeback_incident_age_seconds",
                "gauge",
                age,
                makeLabelsJson({{"db", db_uuid}, {"degraded_state", degraded_state}}),
                updated_at_ms});
        }

        double statement_restart_total = 0.0;
        for (const SqlMgaTransactionHistoryRow& row : history_rows)
        {
            statement_restart_total += static_cast<double>(row.restart_count);
        }
        add_row(SqlRuntimeMetricRow{
            "sb_mga_statement_restarts_total",
            "counter",
            statement_restart_total,
            makeLabelsJson({{"db", db_uuid}, {"reason", "statement_restart"}}),
            updated_at_ms});

        std::unordered_map<std::string, double> wait_seconds_by_mode;
        std::unordered_map<std::string, double> deadlocks_by_reason;
        for (const SqlMgaWaitHistoryRow& row : wait_rows)
        {
            wait_seconds_by_mode[row.wait_mode] += row.wait_seconds;
            if (row.outcome == "DEADLOCK_DETECTED")
            {
                deadlocks_by_reason["youngest_xid"] += 1.0;
            }
        }
        for (const auto& [wait_mode, total_seconds] : wait_seconds_by_mode)
        {
            add_row(SqlRuntimeMetricRow{
                "sb_lock_wait_seconds_total",
                "counter",
                total_seconds,
                db_wait_labels_json(wait_mode),
                updated_at_ms});
        }
        for (const auto& [reason, count] : deadlocks_by_reason)
        {
            add_row(SqlRuntimeMetricRow{
                "sb_lock_deadlocks_total",
                "counter",
                count,
                makeLabelsJson({{"db", db_uuid}, {"reason", reason}}),
                updated_at_ms});
        }

        if (const auto* lock_mgr = db.lock_manager())
        {
            LockStats lock_stats{};
            lock_mgr->getStatistics(&lock_stats);
            add_row(SqlRuntimeMetricRow{
                "sb_lock_read_consistency_restarts_total",
                "counter",
                static_cast<double>(lock_stats.no_wait_rejections),
                makeLabelsJson({{"db", db_uuid}, {"reason", "no_wait_conflict"}}),
                updated_at_ms});
        }

        const uint64_t sweep_generation =
            db.sweep_manager() ? db.sweep_manager()->getStatistics().sweep_count : 0;
        add_row(SqlRuntimeMetricRow{
            "sb_gc_sweep_generation", "gauge", static_cast<double>(sweep_generation),
            db_labels_json(), updated_at_ms});

        if (!writeback_debt_rows.empty())
        {
            const auto& debt = writeback_debt_rows.front();
            add_row(SqlRuntimeMetricRow{
                "sb_buf_commit_fence_backlog",
                "gauge",
                static_cast<double>(debt.blocked_frame_count),
                db_labels_json(),
                updated_at_ms});
        }

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        StorageEngine* storage = const_cast<StorageEngine*>(db.storage_engine());
        if (catalog != nullptr && storage != nullptr)
        {
            struct FragmentationAggregate
            {
                double index_backlog_entries = 0.0;
                double same_page_ratio_sum = 0.0;
                double same_page_ratio_weight = 0.0;
                std::unordered_map<std::string, double> chain_depth_buckets;
            };

            std::vector<StorageEngine::FragmentationAdvisorySnapshot> advisory_rows;
            status = storage->listFragmentationAdvisories(advisory_rows);
            if (status != Status::OK)
            {
                return status;
            }

            std::unordered_map<ID, FragmentationAggregate, IDHash> advisory_aggregates;
            for (const auto& advisory_row : advisory_rows)
            {
                auto& aggregate = advisory_aggregates[advisory_row.table_id];
                aggregate.index_backlog_entries +=
                    static_cast<double>(advisory_row.advisory.deleted_slots);
                if (advisory_row.advisory.chain_depth_hint != 0)
                {
                    aggregate.chain_depth_buckets[chainDepthBucketForDepth(
                        advisory_row.advisory.chain_depth_hint)] += 1.0;
                    aggregate.same_page_ratio_sum +=
                        advisory_row.advisory.same_page_update_ratio;
                    aggregate.same_page_ratio_weight += 1.0;
                }
            }

            for (const auto& [table_id, aggregate] : advisory_aggregates)
            {
                const std::string relation_name = relationNameForTableId(catalog, table_id);
                const std::string relation_label =
                    relation_name.empty() ? table_id.toString() : relation_name;
                add_row(SqlRuntimeMetricRow{
                    "sb_gc_index_backlog_entries",
                    "gauge",
                    aggregate.index_backlog_entries,
                    db_relation_labels_json(relation_label),
                    updated_at_ms});
                for (const auto& [bucket, count] : aggregate.chain_depth_buckets)
                {
                    add_row(SqlRuntimeMetricRow{
                        "sb_mga_chain_depth_bucket",
                        "gauge",
                        count,
                        makeLabelsJson(
                            {{"db", db_uuid}, {"relation", relation_label}, {"bucket", bucket}}),
                        updated_at_ms});
                }
                if (aggregate.same_page_ratio_weight > 0.0)
                {
                    add_row(SqlRuntimeMetricRow{
                        "sb_mga_same_page_update_ratio",
                        "gauge",
                        aggregate.same_page_ratio_sum / aggregate.same_page_ratio_weight,
                        db_relation_labels_json(relation_label),
                        updated_at_ms});
                }
            }
        }

        for (const SqlMgaCleanupDebtRow& row : cleanup_rows)
        {
            add_row(SqlRuntimeMetricRow{
                "sb_gc_cleanup_debt_bytes",
                "gauge",
                static_cast<double>(row.cleanup_debt_bytes),
                db_relation_labels_json(row.relation_name),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_mga_dead_space_bytes",
                "gauge",
                static_cast<double>(row.cleanup_debt_bytes),
                db_relation_labels_json(row.relation_name),
                updated_at_ms});
            add_row(SqlRuntimeMetricRow{
                "sb_mga_retained_dead_bytes",
                "gauge",
                static_cast<double>(row.retained_dead_bytes),
                db_relation_labels_json(row.relation_name),
                updated_at_ms});
            if (row.has_chain_scatter_bucket)
            {
                add_row(SqlRuntimeMetricRow{
                    "sb_mga_chain_scatter_bucket",
                    "gauge",
                    1.0,
                    makeLabelsJson(
                        {{"db", db_uuid}, {"relation", row.relation_name}, {"bucket", row.chain_scatter_bucket}}),
                    updated_at_ms});
            }
            if (row.rewrite_recommended)
            {
                add_row(SqlRuntimeMetricRow{
                    "sb_mga_rewrite_recommendations_total",
                    "counter",
                    1.0,
                    makeLabelsJson(
                        {{"db", db_uuid}, {"relation", row.relation_name}, {"reason", "fragmentation"}}),
                    updated_at_ms});
            }
        }

        if (const auto* lock_mgr = db.lock_manager())
        {
            std::vector<LockSnapshot> locks;
            if (lock_mgr->listLocks(locks) == Status::OK)
            {
                std::unordered_map<std::string, double> blockers_by_mode;
                for (const LockSnapshot& snapshot : locks)
                {
                    if (!snapshot.granted)
                    {
                        blockers_by_mode[lockModeNameFromByte(static_cast<uint8_t>(snapshot.mode))] += 1.0;
                    }
                }
                for (const auto& [wait_mode, blocker_count] : blockers_by_mode)
                {
                    add_row(SqlRuntimeMetricRow{
                        "sb_lock_blockers",
                        "gauge",
                        blocker_count,
                        db_wait_labels_json(wait_mode),
                        updated_at_ms});
                }
            }
        }

        rows_out.clear();
        rows_out.reserve(row_map.size());
        for (auto& [key, row] : row_map)
        {
            (void)key;
            rows_out.push_back(std::move(row));
        }
        std::sort(rows_out.begin(), rows_out.end(),
                  [](const SqlRuntimeMetricRow& lhs, const SqlRuntimeMetricRow& rhs) {
                      if (lhs.metric_name != rhs.metric_name)
                      {
                          return lhs.metric_name < rhs.metric_name;
                      }
                      if (lhs.labels_json != rhs.labels_json)
                      {
                          return lhs.labels_json < rhs.labels_json;
                      }
                      return lhs.value < rhs.value;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildMgaActiveTransactionRows(
        const Database& db,
        uint64_t observed_at_ms,
        std::vector<SqlMgaActiveTransactionRow>& rows_out) -> Status
    {
        rows_out.clear();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::RuntimeTransactionCatalogInfo> runtime_rows;
        Status status = catalog->listRuntimeTransactionCatalogEntries(db.uuid(), runtime_rows, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        const std::string db_uuid = dbUuidString(db);
        const uint64_t ost = db.transaction_manager() ? db.transaction_manager()->getOldestSnapshot() : 0;
        uint64_t total_retained = 0;
        std::vector<StorageEngine::FragmentationAdvisorySnapshot> advisories;
        if (db.storage_engine() && db.storage_engine()->listFragmentationAdvisories(advisories) == Status::OK)
        {
            for (const StorageEngine::FragmentationAdvisorySnapshot& advisory : advisories)
            {
                total_retained += advisory.advisory.reclaimable_bytes;
            }
        }

        rows_out.reserve(runtime_rows.size());
        for (const CatalogManager::RuntimeTransactionCatalogInfo& info : runtime_rows)
        {
            if (info.state != CatalogManager::RuntimeTransactionState::IN_PROGRESS &&
                info.state != CatalogManager::RuntimeTransactionState::PREPARED)
            {
                continue;
            }

            SqlMgaActiveTransactionRow row{};
            row.db_uuid = db_uuid;
            row.txid = info.txid;
            row.state = runtimeTransactionStateName(info.state);
            row.isolation_mode = isolationModeName(info.isolation_level);
            if (info.txid != 0)
            {
                row.has_xmin = true;
                row.xmin = info.txid;
            }
            row.age_seconds = ageSecondsFromMicros(observed_at_ms, info.start_time);
            row.retained_bytes = (ost != 0 && info.txid != 0 && info.txid <= ost) ? total_retained : 0;
            row.started_at_ms = microsToMillis(info.start_time);
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlMgaActiveTransactionRow& lhs, const SqlMgaActiveTransactionRow& rhs) {
                      if (lhs.started_at_ms != rhs.started_at_ms)
                      {
                          return lhs.started_at_ms < rhs.started_at_ms;
                      }
                      return lhs.txid < rhs.txid;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildMgaCleanupDebtRows(
        const Database& db,
        const MetricsRegistry& registry,
        uint64_t observed_at_ms,
        std::vector<SqlMgaCleanupDebtRow>& rows_out) -> Status
    {
        (void)registry;
        rows_out.clear();

        StorageEngine* storage = const_cast<StorageEngine*>(db.storage_engine());
        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (storage == nullptr || catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<StorageEngine::FragmentationAdvisorySnapshot> advisories;
        Status status = storage->listFragmentationAdvisories(advisories);
        if (status != Status::OK)
        {
            return status;
        }

        struct Aggregate
        {
            uint64_t cleanup_debt_bytes = 0;
            uint64_t retained_dead_bytes = 0;
            bool rewrite_recommended = false;
            std::unordered_set<uint32_t> pages;
        };

        std::unordered_map<ID, Aggregate, IDHash> aggregates;
        for (const StorageEngine::FragmentationAdvisorySnapshot& snapshot : advisories)
        {
            Aggregate& agg = aggregates[snapshot.table_id];
            agg.cleanup_debt_bytes += snapshot.advisory.reclaimable_bytes;
            agg.retained_dead_bytes += snapshot.advisory.reclaimable_bytes;
            agg.rewrite_recommended = agg.rewrite_recommended || snapshot.advisory.rewrite_recommended;
            agg.pages.insert(snapshot.advisory.page_id);
        }

        const std::string db_uuid = dbUuidString(db);
        const uint64_t sweep_generation =
            db.sweep_manager() ? db.sweep_manager()->getStatistics().sweep_count : 0;
        rows_out.reserve(aggregates.size());
        for (const auto& [table_id, agg] : aggregates)
        {
            SqlMgaCleanupDebtRow row{};
            row.db_uuid = db_uuid;
            row.relation_name = relationNameForTableId(catalog, table_id);
            if (row.relation_name.empty())
            {
                row.relation_name = table_id.toString();
            }
            row.cleanup_debt_bytes = agg.cleanup_debt_bytes;
            row.retained_dead_bytes = agg.retained_dead_bytes;
            row.has_chain_scatter_bucket = true;
            row.chain_scatter_bucket = chainScatterBucketForPages(agg.pages.size());
            row.rewrite_recommended = agg.rewrite_recommended;
            row.sweep_generation = sweep_generation;
            row.observed_at_ms = observed_at_ms;
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlMgaCleanupDebtRow& lhs, const SqlMgaCleanupDebtRow& rhs) {
                      return lhs.relation_name < rhs.relation_name;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildMgaSnapshotBlockerRows(
        const Database& db,
        const MetricsRegistry& registry,
        uint64_t observed_at_ms,
        std::vector<SqlMgaSnapshotBlockerRow>& rows_out) -> Status
    {
        (void)registry;
        rows_out.clear();

        const TransactionManager* txn_mgr = db.transaction_manager();
        if (txn_mgr == nullptr)
        {
            return Status::OK;
        }

        const uint64_t ost = txn_mgr->getOldestSnapshot();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::RuntimeTransactionCatalogInfo> runtime_rows;
        Status status = catalog->listRuntimeTransactionCatalogEntries(db.uuid(), runtime_rows, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        uint64_t total_retained = 0;
        std::vector<StorageEngine::FragmentationAdvisorySnapshot> advisories;
        if (db.storage_engine() && db.storage_engine()->listFragmentationAdvisories(advisories) == Status::OK)
        {
            for (const StorageEngine::FragmentationAdvisorySnapshot& advisory : advisories)
            {
                total_retained += advisory.advisory.reclaimable_bytes;
            }
        }

        const std::string db_uuid = dbUuidString(db);
        for (const CatalogManager::RuntimeTransactionCatalogInfo& info : runtime_rows)
        {
            if (info.state != CatalogManager::RuntimeTransactionState::IN_PROGRESS &&
                info.state != CatalogManager::RuntimeTransactionState::PREPARED)
            {
                continue;
            }
            if (info.txid == 0)
            {
                continue;
            }
            const bool ost_matches = (ost != 0 && info.txid <= ost);
            if (!ost_matches && !rows_out.empty())
            {
                continue;
            }
            if (!ost_matches && ost != 0)
            {
                continue;
            }

            SqlMgaSnapshotBlockerRow row{};
            row.db_uuid = db_uuid;
            row.blocker_txid = info.txid;
            row.blocker_identity = isZeroId(info.session_id) ? info.tx_uuid.toString() : info.session_id.toString();
            row.retained_bytes = total_retained;
            row.snapshot_age_seconds = ageSecondsFromMicros(observed_at_ms, info.start_time);
            row.ost_txid = ost != 0 ? ost : info.txid;
            row.observed_at_ms = observed_at_ms;
            rows_out.push_back(std::move(row));
        }

        if (rows_out.empty())
        {
            const auto oldest_it = std::min_element(
                runtime_rows.begin(),
                runtime_rows.end(),
                [](const CatalogManager::RuntimeTransactionCatalogInfo& lhs,
                   const CatalogManager::RuntimeTransactionCatalogInfo& rhs) {
                    if (lhs.start_time != rhs.start_time)
                    {
                        return lhs.start_time < rhs.start_time;
                    }
                    return lhs.txid < rhs.txid;
                });
            if (oldest_it != runtime_rows.end() && oldest_it->txid != 0 &&
                (oldest_it->state == CatalogManager::RuntimeTransactionState::IN_PROGRESS ||
                 oldest_it->state == CatalogManager::RuntimeTransactionState::PREPARED))
            {
                SqlMgaSnapshotBlockerRow row{};
                row.db_uuid = db_uuid;
                row.blocker_txid = oldest_it->txid;
                row.blocker_identity = isZeroId(oldest_it->session_id)
                    ? oldest_it->tx_uuid.toString()
                    : oldest_it->session_id.toString();
                row.retained_bytes = total_retained;
                row.snapshot_age_seconds = ageSecondsFromMicros(observed_at_ms, oldest_it->start_time);
                row.ost_txid = ost != 0 ? ost : oldest_it->txid;
                row.observed_at_ms = observed_at_ms;
                rows_out.push_back(std::move(row));
            }
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlMgaSnapshotBlockerRow& lhs, const SqlMgaSnapshotBlockerRow& rhs) {
                      if (lhs.snapshot_age_seconds != rhs.snapshot_age_seconds)
                      {
                          return lhs.snapshot_age_seconds > rhs.snapshot_age_seconds;
                      }
                      return lhs.blocker_txid < rhs.blocker_txid;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildMgaTransactionHistoryRows(
        const Database& db,
        std::vector<SqlMgaTransactionHistoryRow>& rows_out) -> Status
    {
        rows_out.clear();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::TransactionHistoryEntry> entries;
        Status status = catalog->listTransactionHistory(entries, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        const std::string db_uuid = dbUuidString(db);
        rows_out.reserve(entries.size());
        for (const CatalogManager::TransactionHistoryEntry& entry : entries)
        {
            SqlMgaTransactionHistoryRow row{};
            row.db_uuid = db_uuid;
            row.txid = entry.trx_id;
            row.state = entry.committed ? "COMMITTED" : "ABORTED";
            row.has_start_oit = entry.start_oit != 0;
            row.start_oit = entry.start_oit;
            row.has_end_oit = entry.end_oit != 0;
            row.end_oit = entry.end_oit;
            row.has_start_oat = entry.start_oat != 0;
            row.start_oat = entry.start_oat;
            row.has_end_oat = entry.end_oat != 0;
            row.end_oat = entry.end_oat;
            row.has_start_ost = entry.start_ost != 0;
            row.start_ost = entry.start_ost;
            row.has_end_ost = entry.end_ost != 0;
            row.end_ost = entry.end_ost;
            row.restart_count = entry.restart_count;
            row.has_publication_fence_seconds = entry.has_publication_fence_us;
            row.publication_fence_seconds = microsToSeconds(entry.publication_fence_us);
            row.has_limbo_state = !entry.limbo_state.empty();
            row.limbo_state = entry.limbo_state;
            row.started_at_ms = microsToMillis(entry.timer_start);
            row.has_ended_at_ms = entry.timer_end != 0;
            row.ended_at_ms = microsToMillis(entry.timer_end);
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlMgaTransactionHistoryRow& lhs,
                     const SqlMgaTransactionHistoryRow& rhs) {
                      if (lhs.started_at_ms != rhs.started_at_ms)
                      {
                          return lhs.started_at_ms < rhs.started_at_ms;
                      }
                      return lhs.txid < rhs.txid;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildCheckpointStatusRows(
        const Database& db,
        std::vector<SqlCheckpointStatusRow>& rows_out) -> Status
    {
        rows_out.clear();

        BootstrapSystemStatePage state_page{};
        Status status = readSystemStatePageLocal(db, &state_page);
        if (status != Status::OK)
        {
            return status;
        }

        CheckpointTelemetryState checkpoint{};
        loadCheckpointTelemetryState(state_page, &checkpoint);

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        CatalogManager::CheckpointRunCatalogInfo latest{};
        const bool has_latest = catalog != nullptr &&
            catalog->getLatestCheckpointRunCatalogEntry(latest, nullptr) == Status::OK;

        if (checkpoint.checkpoint_generation == 0 && !has_latest)
        {
            return Status::OK;
        }

        const bool prefer_history =
            has_latest &&
            (checkpoint.checkpoint_state == CheckpointLifecycleState::IDLE ||
             latest.checkpoint_generation > checkpoint.checkpoint_generation);

        SqlCheckpointStatusRow row{};
        row.checkpoint_generation = prefer_history
            ? latest.checkpoint_generation
            : (checkpoint.checkpoint_generation != 0
                   ? checkpoint.checkpoint_generation
                   : latest.checkpoint_generation);
        row.checkpoint_state = prefer_history
            ? checkpointStateName(latest.checkpoint_state)
            : checkpointStateName(checkpoint.checkpoint_state);
        row.start_time = prefer_history
            ? latest.start_time
            : (checkpoint.checkpoint_start_time != 0
                   ? checkpoint.checkpoint_start_time
                   : (has_latest ? latest.start_time : 0));
        row.dirty_generation_low_watermark =
            prefer_history
                ? latest.dirty_generation_low_watermark
                : checkpoint.dirty_generation_low_watermark != 0
                ? checkpoint.dirty_generation_low_watermark
                : (has_latest ? latest.dirty_generation_low_watermark : 0);
        row.dirty_generation_high_watermark =
            prefer_history
                ? row.dirty_generation_low_watermark
                : checkpoint.dirty_generation_high_watermark != 0
                ? checkpoint.dirty_generation_high_watermark
                : row.dirty_generation_low_watermark;
        row.pages_target = has_latest ? latest.pages_target : 0;
        row.pages_remaining =
            has_latest && latest.pages_target > latest.pages_flushed
                ? latest.pages_target - latest.pages_flushed
                : 0;
        row.captured_flush_debt_pages =
            !prefer_history && checkpoint.captured_flush_debt_pages != 0
                ? checkpoint.captured_flush_debt_pages
                : row.pages_remaining;
        if (const auto* buffer_pool = db.buffer_pool())
        {
            const auto stats = buffer_pool->getStats();
            row.blocked_frame_count = stats.mga_commit_fence_backlog;
            if (row.pages_remaining == 0 &&
                checkpoint.checkpoint_state != CheckpointLifecycleState::IDLE)
            {
                row.pages_remaining = buffer_pool->currentDirtyPageCount();
            }
        }
        row.queue_rebuild_required = checkpoint.queue_rebuild_required;
        const Status failure_reason =
            !prefer_history && checkpoint.checkpoint_failure_reason != Status::OK
                ? checkpoint.checkpoint_failure_reason
                : (has_latest && latest.has_failure_reason ? latest.failure_reason : Status::OK);
        row.has_failure_reason = failure_reason != Status::OK;
        if (row.has_failure_reason)
        {
            row.failure_reason = statusName(failure_reason);
        }

        rows_out.push_back(std::move(row));
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildCheckpointHistoryRows(
        const Database& db,
        std::vector<SqlCheckpointHistoryRow>& rows_out) -> Status
    {
        rows_out.clear();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::CheckpointRunCatalogInfo> history_rows;
        const Status status =
            catalog->listCheckpointRunCatalogEntries(history_rows, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        rows_out.reserve(history_rows.size());
        for (const auto& history : history_rows)
        {
            SqlCheckpointHistoryRow row{};
            row.checkpoint_run_uuid = history.checkpoint_run_uuid.toString();
            row.checkpoint_generation = history.checkpoint_generation;
            row.checkpoint_state = checkpointStateName(history.checkpoint_state);
            row.start_time = history.start_time;
            row.has_end_time = history.has_end_time;
            row.end_time = history.end_time;
            row.dirty_generation_low_watermark = history.dirty_generation_low_watermark;
            row.pages_target = history.pages_target;
            row.pages_flushed = history.pages_flushed;
            row.has_failure_reason = history.has_failure_reason;
            if (history.has_failure_reason)
            {
                row.failure_reason = statusName(history.failure_reason);
            }
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlCheckpointHistoryRow& lhs,
                     const SqlCheckpointHistoryRow& rhs) {
                      if (lhs.checkpoint_generation != rhs.checkpoint_generation)
                      {
                          return lhs.checkpoint_generation > rhs.checkpoint_generation;
                      }
                      if (lhs.start_time != rhs.start_time)
                      {
                          return lhs.start_time > rhs.start_time;
                      }
                      return lhs.checkpoint_run_uuid < rhs.checkpoint_run_uuid;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildRecoveryStatusRows(
        const Database& db,
        std::vector<SqlRecoveryStatusRow>& rows_out) -> Status
    {
        rows_out.clear();

        const auto& startup = db.last_startup_reconciliation();
        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        CatalogManager::RecoveryRunCatalogInfo latest{};
        const bool has_latest = catalog != nullptr &&
            catalog->getLatestRecoveryRunCatalogEntry(latest, nullptr) == Status::OK;

        BootstrapSystemStatePage state_page{};
        Status status = readSystemStatePageLocal(db, &state_page);
        if (status != Status::OK)
        {
            return status;
        }

        CheckpointTelemetryState checkpoint{};
        loadCheckpointTelemetryState(state_page, &checkpoint);

        SqlRecoveryStatusRow row{};
        row.recovery_generation = has_latest ? latest.recovery_generation : db.startup_generation();
        row.classification = startupClassificationName(
            has_latest ? latest.classification : startup.classification);
        row.startup_state = startupServiceStateName(
            has_latest ? latest.degraded_state : startup.service_state);
        row.normalized_transactions = has_latest
            ? latest.normalized_transactions
            : startup.tip_active_to_aborted + startup.tip_active_to_prepared +
                  startup.stale_prepared_records_removed;
        row.repair_required_pages = has_latest
            ? latest.repair_required_pages
            : static_cast<uint64_t>(startup.relinkable_chain_pages) +
                  static_cast<uint64_t>(startup.cleanup_blocked_chain_pages) +
                  static_cast<uint64_t>(startup.quarantinable_chain_pages) +
                  static_cast<uint64_t>(startup.unrecoverable_chain_pages);
        row.write_fenced =
            db.write_admission_fenced() ||
            startup.service_state == Database::StartupServiceState::WRITE_FENCED ||
            (has_latest &&
             latest.degraded_state == Database::StartupServiceState::WRITE_FENCED);
        row.queue_rebuild_completed = !checkpoint.queue_rebuild_required;
        row.warmup_mode = startupWarmupModeName(startup);
        row.has_start_time = has_latest && latest.start_time != 0;
        row.start_time = has_latest ? latest.start_time : 0;
        row.has_end_time = has_latest && latest.has_end_time;
        row.end_time = has_latest ? latest.end_time : 0;

        rows_out.push_back(std::move(row));
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildRecoveryIncidentRows(
        const Database& db,
        std::vector<SqlRecoveryIncidentRow>& rows_out) -> Status
    {
        rows_out.clear();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::RecoveryIncidentCatalogInfo> incidents;
        const Status status =
            catalog->listRecoveryIncidentCatalogEntries(incidents, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        rows_out.reserve(incidents.size());
        for (const auto& incident : incidents)
        {
            SqlRecoveryIncidentRow row{};
            row.recovery_incident_uuid = incident.recovery_incident_uuid.toString();
            row.recovery_generation = incident.recovery_generation;
            row.classification = startupClassificationName(incident.classification);
            row.has_checkpoint_generation = incident.has_checkpoint_generation;
            row.checkpoint_generation = incident.checkpoint_generation;
            row.has_object_uuid = incident.has_object_uuid;
            row.object_uuid =
                incident.has_object_uuid ? incident.object_uuid.toString() : std::string{};
            row.has_details = incident.has_details;
            row.details_json = incident.details_json;
            row.created_time = incident.created_time;
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlRecoveryIncidentRow& lhs,
                     const SqlRecoveryIncidentRow& rhs) {
                      if (lhs.created_time != rhs.created_time)
                      {
                          return lhs.created_time > rhs.created_time;
                      }
                      return lhs.recovery_incident_uuid < rhs.recovery_incident_uuid;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildWritebackIncidentRows(
        const Database& db,
        std::vector<SqlWritebackIncidentRow>& rows_out) -> Status
    {
        rows_out.clear();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::WritebackIncidentCatalogInfo> incidents;
        const Status status =
            catalog->listWritebackIncidentCatalogEntries(incidents, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        rows_out.reserve(incidents.size());
        for (const auto& incident : incidents)
        {
            SqlWritebackIncidentRow row{};
            row.incident_uuid = incident.writeback_incident_uuid.toString();
            row.has_filespace_uuid = incident.has_filespace_uuid;
            row.filespace_uuid =
                incident.has_filespace_uuid ? incident.filespace_uuid.toString() : std::string{};
            row.queue_kind = writebackQueueKindName(incident.queue_kind);
            row.policy_domain = writebackPolicyDomainName(incident.policy_domain);
            row.page_class = incident.page_class;
            row.failure_class = writebackFailureClassName(incident.failure_class);
            row.first_seen_time = incident.first_seen_time;
            row.last_seen_time = incident.last_seen_time;
            row.retry_count = incident.retry_count;
            row.degraded_state = writebackDegradedStateName(incident.degraded_state);
            row.has_clearance_condition = incident.has_clearance_condition_uuid;
            row.clearance_condition = incident.has_clearance_condition_uuid
                ? incident.clearance_condition_uuid.toString()
                : std::string{};
            row.is_open = incident.is_open;
            row.last_error_status = static_cast<int64_t>(incident.last_error_status);
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlWritebackIncidentRow& lhs,
                     const SqlWritebackIncidentRow& rhs) {
                      if (lhs.is_open != rhs.is_open)
                      {
                          return lhs.is_open && !rhs.is_open;
                      }
                      if (lhs.first_seen_time != rhs.first_seen_time)
                      {
                          return lhs.first_seen_time > rhs.first_seen_time;
                      }
                      return lhs.incident_uuid < rhs.incident_uuid;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildBufferWritebackDebtRows(
        const Database& db,
        std::vector<SqlBufferWritebackDebtRow>& rows_out) -> Status
    {
        rows_out.clear();

        std::vector<SqlCheckpointStatusRow> checkpoint_rows;
        Status status = buildCheckpointStatusRows(db, checkpoint_rows);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<SqlWritebackIncidentRow> incident_rows;
        status = buildWritebackIncidentRows(db, incident_rows);
        if (status != Status::OK)
        {
            return status;
        }

        BootstrapSystemStatePage state_page{};
        status = readSystemStatePageLocal(db, &state_page);
        if (status != Status::OK)
        {
            return status;
        }

        WritebackTelemetryState writeback{};
        loadWritebackTelemetryState(state_page, &writeback);

        SqlBufferWritebackDebtRow row{};
        row.db_uuid = dbUuidString(db);
        if (!checkpoint_rows.empty())
        {
            row.checkpoint_generation = checkpoint_rows.front().checkpoint_generation;
            row.checkpoint_state = checkpoint_rows.front().checkpoint_state;
            row.checkpoint_flush_debt_pages =
                checkpoint_rows.front().captured_flush_debt_pages;
            row.checkpoint_pages_remaining = checkpoint_rows.front().pages_remaining;
            row.blocked_frame_count = checkpoint_rows.front().blocked_frame_count;
        }
        row.dirty_pages =
            db.buffer_pool() ? db.buffer_pool()->currentDirtyPageCount() : 0;
        row.write_admission_fenced = db.write_admission_fenced();
        row.incident_open = writeback.incident_open;
        row.retry_count = writeback.retry_count;
        if (!incident_rows.empty() && incident_rows.front().is_open)
        {
            row.incident_open = true;
            row.retry_count = incident_rows.front().retry_count;
        }
        row.reserve_exhaustion_risk = row.write_admission_fenced || row.incident_open;

        rows_out.push_back(std::move(row));
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildSweepResumeStatusRows(
        const Database& db,
        std::vector<SqlSweepResumeStatusRow>& rows_out) -> Status
    {
        rows_out.clear();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::SweepCursorStateCatalogInfo> sweep_rows;
        const Status status =
            catalog->listSweepCursorStateCatalogEntries(sweep_rows, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        if (sweep_rows.empty())
        {
            return Status::OK;
        }

        std::sort(sweep_rows.begin(),
                  sweep_rows.end(),
                  [](const CatalogManager::SweepCursorStateCatalogInfo& lhs,
                     const CatalogManager::SweepCursorStateCatalogInfo& rhs) {
                      if (lhs.persist_time != rhs.persist_time)
                      {
                          return lhs.persist_time > rhs.persist_time;
                      }
                      return lhs.sweep_generation > rhs.sweep_generation;
                  });

        BootstrapSystemStatePage state_page{};
        Status read_status = readSystemStatePageLocal(db, &state_page);
        if (read_status != Status::OK)
        {
            return read_status;
        }

        CheckpointTelemetryState checkpoint{};
        loadCheckpointTelemetryState(state_page, &checkpoint);

        const auto& latest = sweep_rows.front();
        const auto& startup = db.last_startup_reconciliation();
        const bool checkpoint_resume_safe =
            (checkpoint.checkpoint_state == CheckpointLifecycleState::IDLE ||
             checkpoint.checkpoint_state == CheckpointLifecycleState::COMPLETE) &&
            !checkpoint.queue_rebuild_required &&
            checkpoint.checkpoint_failure_reason == Status::OK &&
            (checkpoint.checkpoint_generation == 0 ||
             checkpoint.checkpoint_generation >= latest.checkpoint_generation_seen);
        const bool recovery_resume_safe =
            db.last_shutdown_was_clean() &&
            startup.classification !=
                Database::StartupRecoveryClassification::WRITEBACK_FAILURE_RESUME &&
            startup.classification !=
                Database::StartupRecoveryClassification::CATALOG_OR_CONTROL_DAMAGE_FATAL;

        SqlSweepResumeStatusRow row{};
        row.sweep_generation = latest.sweep_generation;
        row.relation_uuid = latest.relation_uuid.toString();
        row.filespace_uuid = latest.filespace_uuid.toString();
        row.page_id = latest.page_id;
        row.slot_id = latest.slot_id;
        row.checkpoint_generation_seen = latest.checkpoint_generation_seen;
        row.persist_time = latest.persist_time;
        row.active = latest.active;
        row.stage = latest.stage;
        row.resume_lane_mask = latest.resume_lane_mask;
        row.resume_strict_audit = latest.resume_strict_audit;
        row.start_horizon = latest.start_horizon;
        row.reclaimed_version_count = latest.reclaimed_version_count;
        row.reclaimed_bytes = latest.reclaimed_bytes;
        row.index_backlog_count = latest.index_backlog_count;
        row.cursor_crc32c = latest.cursor_crc32c;
        if (!latest.active && latest.page_id == 0)
        {
            row.resume_outcome = "complete";
        }
        else
        {
            row.resume_outcome =
                checkpoint_resume_safe && recovery_resume_safe
                    ? "resume_possible"
                    : "rewind_required";
        }

        rows_out.push_back(std::move(row));
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildMgaFailpointEventRows(
        const Database& db,
        std::vector<SqlMgaFailpointEventRow>& rows_out) -> Status
    {
        rows_out.clear();
        const MgaFailpointManager* failpoints = db.mga_failpoint_manager();
        if (failpoints == nullptr)
        {
            return Status::OK;
        }

        std::vector<MgaFailpointEvent> events;
        Status status = failpoints->listEvents(events, nullptr);
        if (status != Status::OK)
        {
            return status;
        }

        rows_out.reserve(events.size());
        for (const MgaFailpointEvent& event : events)
        {
            SqlMgaFailpointEventRow row{};
            row.event_id = event.event_id;
            row.seed_id = event.seed_id;
            row.trigger_name = event.trigger_name;
            row.outcome = event.outcome;
            row.has_db_uuid = event.has_db_uuid;
            row.db_uuid = event.db_uuid;
            row.has_txid = event.has_txid;
            row.txid = event.txid;
            row.occurred_at_ms = event.occurred_at_ms;
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlMgaFailpointEventRow& lhs, const SqlMgaFailpointEventRow& rhs) {
                      if (lhs.occurred_at_ms != rhs.occurred_at_ms)
                      {
                          return lhs.occurred_at_ms < rhs.occurred_at_ms;
                      }
                      return lhs.event_id < rhs.event_id;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildMgaWaitHistoryRows(
        const Database& db,
        std::vector<SqlMgaWaitHistoryRow>& rows_out) -> Status
    {
        rows_out.clear();

        CatalogManager* catalog = const_cast<CatalogManager*>(db.catalog_manager());
        if (catalog == nullptr)
        {
            return Status::OK;
        }

        std::vector<CatalogManager::WaitHistoryEntry> entries;
        Status status = catalog->listWaitHistory(entries, nullptr);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }

        const std::string db_uuid = dbUuidString(db);
        rows_out.reserve(entries.size());
        for (const CatalogManager::WaitHistoryEntry& entry : entries)
        {
            SqlMgaWaitHistoryRow row{};
            row.db_uuid = db_uuid;
            row.wait_event_id = std::to_string(entry.event_id);
            row.wait_mode = lockModeNameFromByte(entry.requested_mode);
            row.has_blocker_txid = entry.has_blocker_txid;
            row.blocker_txid = entry.blocker_txid;
            row.has_victim_txid = entry.has_victim_txid;
            row.victim_txid = entry.victim_txid;
            row.has_blocker_identity = !entry.blocker_identity.empty();
            row.blocker_identity = entry.blocker_identity;
            row.has_victim_identity = !entry.victim_identity.empty();
            row.victim_identity = entry.victim_identity;
            row.wait_seconds = microsToSeconds(entry.timer_wait);
            row.outcome = entry.outcome_code;
            row.observed_at_ms = microsToMillis(entry.timer_end != 0 ? entry.timer_end : entry.timer_start);
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(),
                  rows_out.end(),
                  [](const SqlMgaWaitHistoryRow& lhs, const SqlMgaWaitHistoryRow& rhs) {
                      if (lhs.observed_at_ms != rhs.observed_at_ms)
                      {
                          return lhs.observed_at_ms < rhs.observed_at_ms;
                      }
                      return lhs.wait_event_id < rhs.wait_event_id;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildHealthRows(const std::vector<HealthComponentRow>& health_components,
                                                      std::vector<HealthComponentRow>& rows_out) -> Status
    {
        rows_out = health_components;
        std::sort(rows_out.begin(), rows_out.end(),
                  [](const HealthComponentRow& lhs, const HealthComponentRow& rhs) {
                      return lhs.component < rhs.component;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildClusterShardRows(
        const std::vector<ClusterShardObservabilityInput>& shards,
        std::vector<SqlClusterShardMetricRow>& rows_out) -> Status
    {
        rows_out.clear();
        rows_out.reserve(shards.size());
        for (const ClusterShardObservabilityInput& in : shards)
        {
            SqlClusterShardMetricRow row{};
            row.db_uuid = in.db_uuid;
            row.shard_id = in.shard_id;
            row.leader_node_id = in.leader_node_id;
            row.leader_term = in.leader_term;
            row.lease_expires_at = in.lease_expires_at;
            row.cwm_txn = in.cwm_txn;
            row.ost_txn = in.ost_txn;
            row.rwm_txn = in.rwm_txn;
            row.gc_safe_txn = in.gc_safe_txn;
            row.replication_lag_txn = in.replication_lag_txn;
            row.replication_lag_seconds = in.replication_lag_seconds;
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(), rows_out.end(),
                  [](const SqlClusterShardMetricRow& lhs, const SqlClusterShardMetricRow& rhs) {
                      if (lhs.db_uuid != rhs.db_uuid)
                      {
                          return lhs.db_uuid < rhs.db_uuid;
                      }
                      return lhs.shard_id < rhs.shard_id;
                  });
        return Status::OK;
    }

    auto SqlObservabilityViewBuilder::buildClusterSnapshotRows(
        const std::vector<ClusterSnapshotObservabilityInput>& snapshots,
        std::vector<SqlClusterSnapshotMetricRow>& rows_out) -> Status
    {
        rows_out.clear();
        rows_out.reserve(snapshots.size());
        for (const ClusterSnapshotObservabilityInput& in : snapshots)
        {
            SqlClusterSnapshotMetricRow row{};
            row.session_id = in.session_id;
            row.db_uuid = in.db_uuid;
            row.shard_id = in.shard_id;
            row.snapshot_boundary = in.snapshot_boundary;
            row.start_time = in.start_time;
            row.last_heartbeat = in.last_heartbeat;
            rows_out.push_back(std::move(row));
        }

        std::sort(rows_out.begin(), rows_out.end(),
                  [](const SqlClusterSnapshotMetricRow& lhs, const SqlClusterSnapshotMetricRow& rhs) {
                      if (lhs.db_uuid != rhs.db_uuid)
                      {
                          return lhs.db_uuid < rhs.db_uuid;
                      }
                      if (lhs.shard_id != rhs.shard_id)
                      {
                          return lhs.shard_id < rhs.shard_id;
                      }
                      return lhs.session_id < rhs.session_id;
                  });
        return Status::OK;
    }

    auto HealthReadinessContract::setLivenessState(bool process_running, bool event_loop_responding) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        process_running_ = process_running;
        event_loop_responding_ = event_loop_responding;
    }

    auto HealthReadinessContract::setReadinessState(bool database_open,
                                                    bool catalog_available,
                                                    bool cluster_epoch_loaded,
                                                    bool listener_pool_available,
                                                    bool control_plane_reachable,
                                                    bool leader_leases_valid,
                                                    bool shard_map_loaded) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        database_open_ = database_open;
        catalog_available_ = catalog_available;
        cluster_epoch_loaded_ = cluster_epoch_loaded;
        listener_pool_available_ = listener_pool_available;
        control_plane_reachable_ = control_plane_reachable;
        leader_leases_valid_ = leader_leases_valid;
        shard_map_loaded_ = shard_map_loaded;
    }

    auto HealthReadinessContract::isLive() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return process_running_ && event_loop_responding_;
    }

    auto HealthReadinessContract::isReady() const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return process_running_ && event_loop_responding_ && database_open_ && catalog_available_ &&
            cluster_epoch_loaded_ && listener_pool_available_ && control_plane_reachable_ &&
            leader_leases_valid_ && shard_map_loaded_;
    }

    auto HealthReadinessContract::healthComponentRows(uint64_t now_ms,
                                                      std::vector<HealthComponentRow>& rows_out) const -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        rows_out.clear();
        rows_out = {
            {"process_running",
             process_running_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             process_running_ ? "process is running" : "process is not running",
             now_ms},
            {"event_loop_responding",
             event_loop_responding_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             event_loop_responding_ ? "event loop responding" : "event loop unresponsive",
             now_ms},
            {"database_open",
             database_open_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             database_open_ ? "database opened" : "database not opened",
             now_ms},
            {"catalog_available",
             catalog_available_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             catalog_available_ ? "catalog available" : "catalog unavailable",
             now_ms},
            {"cluster_epoch_loaded",
             cluster_epoch_loaded_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             cluster_epoch_loaded_ ? "cluster epoch loaded" : "cluster epoch not loaded",
             now_ms},
            {"listener_pool_available",
             listener_pool_available_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             listener_pool_available_ ? "listener/parser pool available" : "listener/parser pool unavailable",
             now_ms},
            {"control_plane_reachable",
             control_plane_reachable_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             control_plane_reachable_ ? "control plane reachable" : "control plane unreachable",
             now_ms},
            {"leader_leases_valid",
             leader_leases_valid_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             leader_leases_valid_ ? "leader leases valid" : "leader lease invalid",
             now_ms},
            {"shard_map_loaded",
             shard_map_loaded_ ? HealthComponentStatus::OK : HealthComponentStatus::FAIL,
             shard_map_loaded_ ? "shard map loaded" : "shard map unavailable",
             now_ms},
        };
        return Status::OK;
    }

    auto HealthReadinessContract::healthzJson(uint64_t now_ms) const -> std::string
    {
        std::vector<HealthComponentRow> rows;
        (void)healthComponentRows(now_ms, rows);
        const bool live = isLive();

        nlohmann::ordered_json doc = nlohmann::ordered_json::object();
        doc["schema"] = "ScratchBirdHealthV1";
        doc["path"] = "/healthz";
        doc["checked_at_ms"] = now_ms;
        doc["live"] = live;
        doc["ready"] = isReady();
        doc["status"] = live ? "OK" : "FAIL";
        doc["components"] = nlohmann::ordered_json::array();
        for (const HealthComponentRow& row : rows)
        {
            nlohmann::ordered_json item = nlohmann::ordered_json::object();
            item["component"] = row.component;
            item["status"] = toString(row.status);
            item["message"] = row.message;
            item["updated_at"] = row.updated_at;
            doc["components"].push_back(std::move(item));
        }
        return doc.dump();
    }

    auto HealthReadinessContract::readyzJson(uint64_t now_ms) const -> std::string
    {
        std::vector<HealthComponentRow> rows;
        (void)healthComponentRows(now_ms, rows);
        const bool ready = isReady();

        nlohmann::ordered_json doc = nlohmann::ordered_json::object();
        doc["schema"] = "ScratchBirdReadinessV1";
        doc["path"] = "/readyz";
        doc["checked_at_ms"] = now_ms;
        doc["ready"] = ready;
        doc["status"] = ready ? "READY" : "NOT_READY";
        doc["components"] = nlohmann::ordered_json::array();
        for (const HealthComponentRow& row : rows)
        {
            nlohmann::ordered_json item = nlohmann::ordered_json::object();
            item["component"] = row.component;
            item["status"] = toString(row.status);
            item["message"] = row.message;
            item["updated_at"] = row.updated_at;
            doc["components"].push_back(std::move(item));
        }
        return doc.dump();
    }

    auto toString(StructuredEventSeverity severity) -> const char*
    {
        switch (severity)
        {
            case StructuredEventSeverity::INFO:
                return "INFO";
            case StructuredEventSeverity::WARN:
                return "WARN";
            case StructuredEventSeverity::ERROR:
                return "ERROR";
        }
        return "UNKNOWN";
    }

    auto StructuredEventStream::setMaxInMemory(size_t max_events) -> void
    {
        std::lock_guard<std::mutex> lock(mutex_);
        max_events_ = std::max<size_t>(1, max_events);
        if (events_.size() > max_events_)
        {
            events_.erase(events_.begin(), events_.begin() + (events_.size() - max_events_));
        }
    }

    auto StructuredEventStream::validate(const StructuredEventRecord& event, ErrorContext* ctx) -> Status
    {
        if (event.event_type.empty() || event.message.empty() || event.occurred_at_ms == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "event_type, message, and occurred_at_ms are required");
            return Status::INVALID_ARGUMENT;
        }

        if (event.epoch.cluster_config_epoch == 0 || event.epoch.schema_epoch == 0 ||
            event.epoch.security_epoch == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "epoch context is required");
            return Status::INVALID_ARGUMENT;
        }

        if (event.db_uuid.empty() || event.node_id.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "db_uuid and node_id are required");
            return Status::INVALID_ARGUMENT;
        }

        try
        {
            const nlohmann::json payload = nlohmann::json::parse(event.payload_json);
            if (!payload.is_object())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "payload_json must be a JSON object");
                return Status::INVALID_ARGUMENT;
            }
        }
        catch (const std::exception&)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "payload_json is not valid JSON");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    auto StructuredEventStream::serialize(const std::string& event_id, const StructuredEventRecord& event)
        -> std::string
    {
        nlohmann::ordered_json doc = nlohmann::ordered_json::object();
        doc["event_id"] = event_id;
        doc["event_type"] = event.event_type;
        doc["severity"] = toString(event.severity);
        doc["occurred_at_ms"] = event.occurred_at_ms;
        doc["cluster_config_epoch"] = event.epoch.cluster_config_epoch;
        doc["schema_epoch"] = event.epoch.schema_epoch;
        doc["security_epoch"] = event.epoch.security_epoch;
        doc["db_uuid"] = event.db_uuid;
        doc["node_id"] = event.node_id;
        doc["shard_id"] = event.shard_id;
        doc["message"] = event.message;
        doc["payload"] = nlohmann::json::parse(event.payload_json);
        return doc.dump();
    }

    auto StructuredEventStream::emit(const StructuredEventRecord& event,
                                     std::string* event_id_out,
                                     ErrorContext* ctx) -> Status
    {
        Status status = validate(event, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const std::string event_id = "evt-" + std::to_string(next_sequence_++);
        StoredEvent stored{};
        stored.event_id = event_id;
        stored.event = event;
        stored.serialized_json = serialize(event_id, event);
        events_.push_back(std::move(stored));
        if (events_.size() > max_events_)
        {
            events_.erase(events_.begin(), events_.begin() + (events_.size() - max_events_));
        }

        if (std::find(schema_event_types_.begin(), schema_event_types_.end(), event.event_type) ==
            schema_event_types_.end())
        {
            schema_event_types_.push_back(event.event_type);
            std::sort(schema_event_types_.begin(), schema_event_types_.end());
        }

        if (event_id_out != nullptr)
        {
            *event_id_out = event_id;
        }
        return Status::OK;
    }

    auto StructuredEventStream::exportJsonLines(std::vector<std::string>& lines_out) const -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lines_out.clear();
        lines_out.reserve(events_.size());
        for (const StoredEvent& event : events_)
        {
            lines_out.push_back(event.serialized_json);
        }
        return Status::OK;
    }

    auto StructuredEventStream::schemaRegistry(std::vector<std::string>& event_types_out) const -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        event_types_out = schema_event_types_;
        return Status::OK;
    }

} // namespace scratchbird::core
