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

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/telemetry.h"

namespace scratchbird::core
{
    class Database;

    struct MetricPolicyViolation
    {
        std::string metric_name;
        std::string reason;
    };

    class MetricContractPolicy
    {
    public:
        static auto isCanonicalMetricName(std::string_view metric_name) -> bool;
        static auto isAllowedLabelName(std::string_view label_name) -> bool;
        static auto isForbiddenLabelName(std::string_view label_name) -> bool;

        static auto validateSample(const MetricSampleRow& sample,
                                   std::vector<std::string>& reasons_out) -> bool;

        static auto auditRegistry(const MetricsRegistry& registry,
                                  std::vector<MetricPolicyViolation>& violations_out) -> Status;

        static auto registerSbObsBaselineMetrics(MetricsRegistry& registry) -> Status;

        static auto buildLegacyNameMapping(
            std::vector<std::pair<std::string, std::string>>& mapping_out) -> Status;
    };

    enum class HealthComponentStatus : uint8_t
    {
        OK = 0,
        WARN = 1,
        FAIL = 2,
    };

    auto toString(HealthComponentStatus status) -> const char*;

    struct HealthComponentRow
    {
        std::string component;
        HealthComponentStatus status = HealthComponentStatus::FAIL;
        std::string message;
        uint64_t updated_at = 0;
    };

    struct SqlRuntimeMetricRow
    {
        std::string metric_name;
        std::string metric_type;
        double value = 0.0;
        std::string labels_json;
        uint64_t updated_at = 0;
    };

    struct SqlCheckpointStatusRow
    {
        uint64_t checkpoint_generation = 0;
        std::string checkpoint_state;
        uint64_t start_time = 0;
        uint64_t dirty_generation_low_watermark = 0;
        uint64_t dirty_generation_high_watermark = 0;
        uint64_t captured_flush_debt_pages = 0;
        uint64_t pages_target = 0;
        uint64_t pages_remaining = 0;
        uint64_t blocked_frame_count = 0;
        bool queue_rebuild_required = false;
        bool has_failure_reason = false;
        std::string failure_reason;
    };

    struct SqlCheckpointHistoryRow
    {
        std::string checkpoint_run_uuid;
        uint64_t checkpoint_generation = 0;
        std::string checkpoint_state;
        uint64_t start_time = 0;
        bool has_end_time = false;
        uint64_t end_time = 0;
        uint64_t dirty_generation_low_watermark = 0;
        uint64_t pages_target = 0;
        uint64_t pages_flushed = 0;
        bool has_failure_reason = false;
        std::string failure_reason;
    };

    struct SqlRecoveryStatusRow
    {
        uint64_t recovery_generation = 0;
        std::string classification;
        std::string startup_state;
        uint64_t normalized_transactions = 0;
        uint64_t repair_required_pages = 0;
        bool write_fenced = false;
        bool queue_rebuild_completed = false;
        std::string warmup_mode;
        bool has_start_time = false;
        uint64_t start_time = 0;
        bool has_end_time = false;
        uint64_t end_time = 0;
    };

    struct SqlRecoveryIncidentRow
    {
        std::string recovery_incident_uuid;
        uint64_t recovery_generation = 0;
        std::string classification;
        bool has_checkpoint_generation = false;
        uint64_t checkpoint_generation = 0;
        bool has_object_uuid = false;
        std::string object_uuid;
        bool has_details = false;
        std::string details_json;
        uint64_t created_time = 0;
    };

    struct SqlWritebackIncidentRow
    {
        std::string incident_uuid;
        bool has_filespace_uuid = false;
        std::string filespace_uuid;
        std::string queue_kind;
        std::string policy_domain;
        uint64_t page_class = 0;
        std::string failure_class;
        uint64_t first_seen_time = 0;
        uint64_t last_seen_time = 0;
        uint64_t retry_count = 0;
        std::string degraded_state;
        bool has_clearance_condition = false;
        std::string clearance_condition;
        bool is_open = false;
        int64_t last_error_status = 0;
    };

    struct SqlBufferWritebackDebtRow
    {
        std::string db_uuid;
        uint64_t checkpoint_generation = 0;
        std::string checkpoint_state;
        uint64_t dirty_pages = 0;
        uint64_t checkpoint_flush_debt_pages = 0;
        uint64_t checkpoint_pages_remaining = 0;
        uint64_t blocked_frame_count = 0;
        bool write_admission_fenced = false;
        bool incident_open = false;
        uint64_t retry_count = 0;
        bool reserve_exhaustion_risk = false;
    };

    struct SqlSweepResumeStatusRow
    {
        uint64_t sweep_generation = 0;
        std::string relation_uuid;
        std::string filespace_uuid;
        uint64_t page_id = 0;
        uint32_t slot_id = 0;
        uint64_t checkpoint_generation_seen = 0;
        uint64_t persist_time = 0;
        bool active = false;
        uint8_t stage = 0;
        uint16_t resume_lane_mask = 0;
        bool resume_strict_audit = true;
        uint64_t start_horizon = 0;
        uint64_t reclaimed_version_count = 0;
        uint64_t reclaimed_bytes = 0;
        uint64_t index_backlog_count = 0;
        uint32_t cursor_crc32c = 0;
        std::string resume_outcome;
    };

    struct SqlMgaActiveTransactionRow
    {
        std::string db_uuid;
        uint64_t txid = 0;
        std::string state;
        std::string isolation_mode;
        bool has_xmin = false;
        uint64_t xmin = 0;
        double age_seconds = 0.0;
        uint64_t retained_bytes = 0;
        uint64_t started_at_ms = 0;
    };

    struct SqlMgaCleanupDebtRow
    {
        std::string db_uuid;
        std::string relation_name;
        uint64_t cleanup_debt_bytes = 0;
        uint64_t retained_dead_bytes = 0;
        bool has_chain_scatter_bucket = false;
        std::string chain_scatter_bucket;
        bool rewrite_recommended = false;
        uint64_t sweep_generation = 0;
        uint64_t observed_at_ms = 0;
    };

    struct SqlMgaSnapshotBlockerRow
    {
        std::string db_uuid;
        uint64_t blocker_txid = 0;
        std::string blocker_identity;
        uint64_t retained_bytes = 0;
        double snapshot_age_seconds = 0.0;
        uint64_t ost_txid = 0;
        uint64_t observed_at_ms = 0;
    };

    struct SqlMgaTransactionHistoryRow
    {
        std::string db_uuid;
        uint64_t txid = 0;
        std::string state;
        bool has_start_oit = false;
        uint64_t start_oit = 0;
        bool has_end_oit = false;
        uint64_t end_oit = 0;
        bool has_start_oat = false;
        uint64_t start_oat = 0;
        bool has_end_oat = false;
        uint64_t end_oat = 0;
        bool has_start_ost = false;
        uint64_t start_ost = 0;
        bool has_end_ost = false;
        uint64_t end_ost = 0;
        uint64_t restart_count = 0;
        bool has_publication_fence_seconds = false;
        double publication_fence_seconds = 0.0;
        bool has_limbo_state = false;
        std::string limbo_state;
        uint64_t started_at_ms = 0;
        bool has_ended_at_ms = false;
        uint64_t ended_at_ms = 0;
    };

    struct SqlMgaFailpointEventRow
    {
        std::string event_id;
        std::string seed_id;
        std::string trigger_name;
        std::string outcome;
        bool has_db_uuid = false;
        ID db_uuid{};
        bool has_txid = false;
        uint64_t txid = 0;
        uint64_t occurred_at_ms = 0;
    };

    struct SqlMgaWaitHistoryRow
    {
        std::string db_uuid;
        std::string wait_event_id;
        std::string wait_mode;
        bool has_blocker_txid = false;
        uint64_t blocker_txid = 0;
        bool has_victim_txid = false;
        uint64_t victim_txid = 0;
        bool has_blocker_identity = false;
        std::string blocker_identity;
        bool has_victim_identity = false;
        std::string victim_identity;
        double wait_seconds = 0.0;
        std::string outcome;
        uint64_t observed_at_ms = 0;
    };

    struct SqlDormantTransactionPolicyRow
    {
        std::string db_uuid;
        std::string restart_reattach_policy;
        std::string cleanup_policy;
        uint64_t lease_seconds = 0;
        uint64_t terminal_retention_seconds = 0;
        uint64_t total_rows = 0;
        uint64_t dormant_rows = 0;
        uint64_t restart_stale_rows = 0;
        uint64_t expired_rows = 0;
        uint64_t terminal_rows = 0;
        uint64_t observed_at_ms = 0;
    };

    struct SqlDormantTransactionRow
    {
        std::string db_uuid;
        ID dormant_id{};
        ID attachment_id{};
        ID session_id{};
        ID user_id{};
        uint64_t txid = 0;
        std::string state;
        std::string isolation_mode;
        bool read_only = false;
        std::string wait_mode;
        uint64_t lock_timeout_seconds = 0;
        double dormant_age_seconds = 0.0;
        bool has_lease_expires_at_ms = false;
        uint64_t lease_expires_at_ms = 0;
        bool restart_stale = false;
        bool has_last_statement_time_ms = false;
        uint64_t last_statement_time_ms = 0;
        uint64_t last_statement_hash = 0;
        int64_t last_rows_affected = 0;
        bool has_last_error_code = false;
        uint32_t last_error_code = 0;
        bool has_last_sqlstate = false;
        std::string last_sqlstate;
        bool has_last_statement_text = false;
        std::string last_statement_text;
    };

    struct ClusterShardObservabilityInput
    {
        std::string db_uuid;
        std::string shard_id;
        std::string leader_node_id;
        uint64_t leader_term = 0;
        uint64_t lease_expires_at = 0;
        uint64_t cwm_txn = 0;
        uint64_t ost_txn = 0;
        uint64_t rwm_txn = 0;
        uint64_t gc_safe_txn = 0;
        uint64_t replication_lag_txn = 0;
        double replication_lag_seconds = 0.0;
    };

    struct SqlClusterShardMetricRow
    {
        std::string db_uuid;
        std::string shard_id;
        std::string leader_node_id;
        uint64_t leader_term = 0;
        uint64_t lease_expires_at = 0;
        uint64_t cwm_txn = 0;
        uint64_t ost_txn = 0;
        uint64_t rwm_txn = 0;
        uint64_t gc_safe_txn = 0;
        uint64_t replication_lag_txn = 0;
        double replication_lag_seconds = 0.0;
    };

    struct ClusterSnapshotObservabilityInput
    {
        std::string session_id;
        std::string db_uuid;
        std::string shard_id;
        uint64_t snapshot_boundary = 0;
        uint64_t start_time = 0;
        uint64_t last_heartbeat = 0;
    };

    struct SqlClusterSnapshotMetricRow
    {
        std::string session_id;
        std::string db_uuid;
        std::string shard_id;
        uint64_t snapshot_boundary = 0;
        uint64_t start_time = 0;
        uint64_t last_heartbeat = 0;
    };

    struct MetricSchemaDefinition
    {
        std::string metric_name;
        MetricType metric_type = MetricType::GAUGE;
        std::vector<std::string> label_names;
        std::string help;
        std::string unit;
    };

    struct SqlViewColumnDefinition
    {
        std::string column_name;
        std::string column_type;
        bool nullable = false;
    };

    struct SqlViewSchemaDefinition
    {
        std::string view_name;
        uint32_t schema_version = 0;
        std::string purpose;
        std::vector<SqlViewColumnDefinition> columns;
    };

    struct DashboardPanelDefinition
    {
        std::string panel_id;
        std::string source_view;
        std::vector<std::string> required_fields;
    };

    struct DashboardAlertDefinition
    {
        std::string alert_id;
        std::string predicate;
        std::string severity;
    };

    struct DashboardSchemaDefinition
    {
        std::string dashboard_id;
        uint32_t schema_version = 0;
        std::string title;
        std::vector<DashboardPanelDefinition> panels;
        std::vector<DashboardAlertDefinition> alerts;
    };

    class MgaObservabilityContract
    {
    public:
        static auto contract_id() -> const char*;
        static auto metric_schema_version() -> uint32_t;
        static auto sql_view_schema_version() -> uint32_t;
        static auto dashboard_schema_version() -> uint32_t;

        static auto appendMetricDefinitions(std::vector<MetricSchemaDefinition>& definitions_out) -> Status;
        static auto appendSqlViewDefinitions(std::vector<SqlViewSchemaDefinition>& definitions_out) -> Status;
        static auto appendDashboardDefinitions(
            std::vector<DashboardSchemaDefinition>& definitions_out) -> Status;

        static auto registerRequiredMetrics(MetricsRegistry& registry) -> Status;

        static auto verifyRegistryContainsRequiredMetrics(
            const MetricsRegistry& registry,
            std::vector<std::string>& missing_metrics_out) -> Status;
    };

    class SqlObservabilityViewBuilder
    {
    public:
        static auto buildRuntimeRows(const MetricsRegistry& registry,
                                     uint64_t updated_at_ms,
                                     std::vector<SqlRuntimeMetricRow>& rows_out) -> Status;

        static auto buildMgaRuntimeRows(const Database& db,
                                        const MetricsRegistry& registry,
                                        uint64_t updated_at_ms,
                                        std::vector<SqlRuntimeMetricRow>& rows_out) -> Status;

        static auto buildHealthRows(const std::vector<HealthComponentRow>& health_components,
                                    std::vector<HealthComponentRow>& rows_out) -> Status;

        static auto buildMgaActiveTransactionRows(const Database& db,
                                                  uint64_t observed_at_ms,
                                                  std::vector<SqlMgaActiveTransactionRow>& rows_out) -> Status;

        static auto buildMgaCleanupDebtRows(const Database& db,
                                            const MetricsRegistry& registry,
                                            uint64_t observed_at_ms,
                                            std::vector<SqlMgaCleanupDebtRow>& rows_out) -> Status;

        static auto buildMgaSnapshotBlockerRows(const Database& db,
                                                const MetricsRegistry& registry,
                                                uint64_t observed_at_ms,
                                                std::vector<SqlMgaSnapshotBlockerRow>& rows_out) -> Status;

        static auto buildMgaTransactionHistoryRows(
            const Database& db,
            std::vector<SqlMgaTransactionHistoryRow>& rows_out) -> Status;

        static auto buildCheckpointStatusRows(
            const Database& db,
            std::vector<SqlCheckpointStatusRow>& rows_out) -> Status;

        static auto buildCheckpointHistoryRows(
            const Database& db,
            std::vector<SqlCheckpointHistoryRow>& rows_out) -> Status;

        static auto buildRecoveryStatusRows(
            const Database& db,
            std::vector<SqlRecoveryStatusRow>& rows_out) -> Status;

        static auto buildRecoveryIncidentRows(
            const Database& db,
            std::vector<SqlRecoveryIncidentRow>& rows_out) -> Status;

        static auto buildWritebackIncidentRows(
            const Database& db,
            std::vector<SqlWritebackIncidentRow>& rows_out) -> Status;

        static auto buildBufferWritebackDebtRows(
            const Database& db,
            std::vector<SqlBufferWritebackDebtRow>& rows_out) -> Status;

        static auto buildSweepResumeStatusRows(
            const Database& db,
            std::vector<SqlSweepResumeStatusRow>& rows_out) -> Status;

        static auto buildMgaFailpointEventRows(
            const Database& db,
            std::vector<SqlMgaFailpointEventRow>& rows_out) -> Status;

        static auto buildMgaWaitHistoryRows(const Database& db,
                                            std::vector<SqlMgaWaitHistoryRow>& rows_out) -> Status;

        static auto buildDormantTransactionPolicyRows(
            const Database& db,
            uint64_t observed_at_ms,
            std::vector<SqlDormantTransactionPolicyRow>& rows_out) -> Status;

        static auto buildDormantTransactionRows(
            const Database& db,
            uint64_t observed_at_ms,
            std::vector<SqlDormantTransactionRow>& rows_out) -> Status;

        static auto buildClusterShardRows(
            const std::vector<ClusterShardObservabilityInput>& shards,
            std::vector<SqlClusterShardMetricRow>& rows_out) -> Status;

        static auto buildClusterSnapshotRows(
            const std::vector<ClusterSnapshotObservabilityInput>& snapshots,
            std::vector<SqlClusterSnapshotMetricRow>& rows_out) -> Status;
    };

    class HealthReadinessContract
    {
    public:
        auto setLivenessState(bool process_running, bool event_loop_responding) -> void;
        auto setReadinessState(bool database_open,
                               bool catalog_available,
                               bool cluster_epoch_loaded,
                               bool listener_pool_available,
                               bool control_plane_reachable,
                               bool leader_leases_valid,
                               bool shard_map_loaded) -> void;

        auto isLive() const -> bool;
        auto isReady() const -> bool;

        auto healthzJson(uint64_t now_ms) const -> std::string;
        auto readyzJson(uint64_t now_ms) const -> std::string;
        auto healthComponentRows(uint64_t now_ms, std::vector<HealthComponentRow>& rows_out) const -> Status;

    private:
        mutable std::mutex mutex_;
        bool process_running_ = true;
        bool event_loop_responding_ = true;
        bool database_open_ = false;
        bool catalog_available_ = false;
        bool cluster_epoch_loaded_ = false;
        bool listener_pool_available_ = false;
        bool control_plane_reachable_ = false;
        bool leader_leases_valid_ = false;
        bool shard_map_loaded_ = false;
    };

    enum class StructuredEventSeverity : uint8_t
    {
        INFO = 0,
        WARN = 1,
        ERROR = 2,
    };

    auto toString(StructuredEventSeverity severity) -> const char*;

    struct StructuredEpochContext
    {
        uint64_t cluster_config_epoch = 0;
        uint64_t schema_epoch = 0;
        uint64_t security_epoch = 0;
    };

    struct StructuredEventRecord
    {
        std::string event_type;
        StructuredEventSeverity severity = StructuredEventSeverity::INFO;
        uint64_t occurred_at_ms = 0;
        StructuredEpochContext epoch{};
        std::string db_uuid;
        std::string node_id;
        std::string shard_id;
        std::string message;
        std::string payload_json = "{}";
    };

    class StructuredEventStream
    {
    public:
        auto setMaxInMemory(size_t max_events) -> void;

        auto emit(const StructuredEventRecord& event,
                  std::string* event_id_out = nullptr,
                  ErrorContext* ctx = nullptr) -> Status;

        auto exportJsonLines(std::vector<std::string>& lines_out) const -> Status;
        auto schemaRegistry(std::vector<std::string>& event_types_out) const -> Status;

        static auto validate(const StructuredEventRecord& event, ErrorContext* ctx = nullptr) -> Status;

    private:
        struct StoredEvent
        {
            std::string event_id;
            StructuredEventRecord event;
            std::string serialized_json;
        };

        static auto serialize(const std::string& event_id, const StructuredEventRecord& event) -> std::string;

        mutable std::mutex mutex_;
        std::vector<StoredEvent> events_;
        std::vector<std::string> schema_event_types_;
        uint64_t next_sequence_ = 1;
        size_t max_events_ = 1024;
    };

} // namespace scratchbird::core
