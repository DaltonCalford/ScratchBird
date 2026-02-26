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

#include <algorithm>
#include <cctype>
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
