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

#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace scratchbird::core
{

class ConnectionContext;
class Database;

class WorkloadGovernance
{
public:
    enum class AcceleratorPosture : uint8_t
    {
        NONE = 0,
        CPU_ONLY = 1,
        CPU_PREFERRED = 2,
        ACCELERATOR_PREFERRED = 3,
        ACCELERATOR_REQUIRED = 4
    };

    enum class AcceleratorOperation : uint8_t
    {
        NONE = 0,
        SEARCH = 1,
        BUILD = 2
    };

    struct QueryDescriptor
    {
        ConnectionContext* connection = nullptr;
        std::string sql;
        std::string database_name;
        std::string schema_name;
        std::string client_app;
        std::string resource_tag;
        AcceleratorPosture accelerator_posture = AcceleratorPosture::NONE;
        AcceleratorOperation accelerator_operation = AcceleratorOperation::NONE;
        std::string accelerator_profile_name;
        std::string accelerator_device_class;
        std::string accelerator_device_id;
        std::string accelerator_device_pool_id;
        uint64_t accelerator_memory_request_bytes = 0;
        uint64_t accelerator_pinned_residency_bytes = 0;
    };

    struct AdmissionDecision
    {
        bool admitted = true;
        bool queued = false;
        bool accelerator_requested = false;
        bool accelerator_admitted = false;
        bool accelerator_fallback_used = false;
        Status status = Status::OK;
        std::string code;
        std::string detail;
        ID class_id{};
        ID policy_id{};
        std::string class_name;
        std::string policy_name;
        std::string accelerator_profile_name;
        std::string accelerator_device_class;
        std::string accelerator_device_id;
        std::string accelerator_device_pool_id;
    };

    class AdmissionLease
    {
    public:
        AdmissionLease() = default;
        ~AdmissionLease();

        AdmissionLease(const AdmissionLease&) = delete;
        AdmissionLease& operator=(const AdmissionLease&) = delete;

        AdmissionLease(AdmissionLease&& other) noexcept;
        auto operator=(AdmissionLease&& other) noexcept -> AdmissionLease&;

        bool active() const { return active_; }
        const ID& workloadClassId() const { return class_id_; }
        const ID& policyId() const { return policy_id_; }
        const std::string& workloadClassName() const { return class_name_; }
        const std::string& policyName() const { return policy_name_; }

        void release();

    private:
        friend class WorkloadGovernance;

        void moveFrom(AdmissionLease&& other) noexcept;

        WorkloadGovernance* owner_ = nullptr;
        uint32_t proc_id_ = UINT32_MAX;
        ID class_id_{};
        ID policy_id_{};
        std::string class_name_;
        std::string policy_name_;
        uint64_t accelerator_reserved_memory_bytes_ = 0;
        bool accelerator_search_active_ = false;
        bool accelerator_build_active_ = false;
        bool active_ = false;
    };

    struct AdmissionStatusRow
    {
        std::string scope;
        std::string class_name;
        std::string policy_name;
        std::string reject_mode;
        uint32_t binding_priority = 0;
        uint32_t class_priority = 0;
        uint32_t max_concurrent_sessions = 0;
        uint32_t max_concurrent_queries = 0;
        uint32_t max_queue_depth = 0;
        uint32_t queue_timeout_ms = 0;
        uint32_t active_sessions = 0;
        uint32_t active_queries = 0;
        uint32_t queued_queries = 0;
        std::string accelerator_profile_name;
        std::string accelerator_device_class;
        std::string accelerator_device_id;
        std::string accelerator_device_pool_id;
        std::string accelerator_prewarm_policy;
        std::string accelerator_fallback_policy;
        std::string accelerator_degraded_state_override;
        uint32_t accelerator_concurrent_build_limit = 0;
        uint32_t accelerator_concurrent_search_limit = 0;
        uint32_t accelerator_active_builds = 0;
        uint32_t accelerator_active_searches = 0;
        uint64_t accelerator_memory_budget_bytes = 0;
        uint64_t accelerator_pinned_residency_target_bytes = 0;
        uint64_t accelerator_reserved_memory_bytes = 0;
        uint64_t accelerator_forced_fallbacks = 0;
        bool class_enabled = false;
        bool policy_enabled = false;
        bool binding_enabled = false;
    };

    struct RoutingPlanRow
    {
        std::string class_name;
        uint32_t class_priority = 0;
        std::string route_name;
        std::string target_kind;
        std::string target_label;
        std::string role;
        std::string service_type;
        std::string transport;
        uint32_t route_weight = 0;
        std::string fallback_route_name;
        bool class_enabled = false;
        bool route_enabled = false;
    };

    struct SloTelemetrySample
    {
        ID node_id{};
        CatalogManager::ClusterNodeRole role = CatalogManager::ClusterNodeRole::OLTP_DATA;
        uint64_t sample_time = 0;
        uint8_t cpu_utilization_pct = 0;
        uint8_t queue_pressure_pct = 0;
        uint16_t current_node_count = 0;
        bool is_valid = true;
    };

    struct SloStatusRow
    {
        ID node_id{};
        std::string node_name;
        std::string role;
        std::string profile_name;
        uint64_t evaluation_time = 0;
        uint64_t window_start_time = 0;
        uint64_t window_end_time = 0;
        uint64_t request_count = 0;
        uint64_t success_count = 0;
        uint64_t error_count = 0;
        double availability_target_pct = 0.0;
        double availability_sli_pct = 0.0;
        uint32_t latency_p95_target_ms = 0;
        uint32_t latency_p95_ms = 0;
        uint32_t latency_p99_target_ms = 0;
        uint32_t latency_p99_ms = 0;
        double error_rate_target_pct = 0.0;
        double error_rate_sli_pct = 0.0;
        double short_burn_rate = 0.0;
        double long_burn_rate = 0.0;
        std::string burn_severity;
        std::string action_plan;
        bool binding_present = false;
        bool metrics_present = false;
    };

    struct ErrorBudgetStatusRow
    {
        ID node_id{};
        std::string node_name;
        std::string role;
        std::string profile_name;
        uint64_t evaluation_time = 0;
        uint64_t window_start_time = 0;
        uint64_t window_end_time = 0;
        double allowed_bad_requests = 0.0;
        double observed_bad_requests = 0.0;
        double remaining_bad_requests = 0.0;
        double remaining_budget_pct = 0.0;
        double short_burn_rate = 0.0;
        double long_burn_rate = 0.0;
        std::string burn_severity;
        bool binding_present = false;
        bool metrics_present = false;
    };

    explicit WorkloadGovernance(Database* db);

    auto acquire(const QueryDescriptor& descriptor,
                 AdmissionLease& lease_out,
                 ErrorContext* ctx = nullptr) -> AdmissionDecision;

    auto recordSloTelemetrySample(const SloTelemetrySample& sample,
                                  ErrorContext* ctx = nullptr) -> Status;
    auto evaluateSloPolicies(uint64_t evaluation_time = 0,
                             ErrorContext* ctx = nullptr) -> Status;

    auto snapshotAdmissionStatus(std::vector<AdmissionStatusRow>& rows_out,
                                 ErrorContext* ctx = nullptr) const -> Status;
    auto snapshotRoutingPlan(std::vector<RoutingPlanRow>& rows_out,
                             ErrorContext* ctx = nullptr) const -> Status;
    auto snapshotSloStatus(std::vector<SloStatusRow>& rows_out,
                           uint64_t evaluation_time = 0,
                           ErrorContext* ctx = nullptr) const -> Status;
    auto snapshotErrorBudgetStatus(std::vector<ErrorBudgetStatusRow>& rows_out,
                                   uint64_t evaluation_time = 0,
                                   ErrorContext* ctx = nullptr) const -> Status;

private:
    struct SloEvaluationRow;

    struct CounterState
    {
        uint32_t active_queries = 0;
        uint32_t queued_queries = 0;
        uint32_t active_accelerator_searches = 0;
        uint32_t active_accelerator_builds = 0;
        uint64_t active_accelerator_reserved_memory_bytes = 0;
        uint64_t forced_fallbacks = 0;
    };

    struct MatchState
    {
        bool matched = false;
        CatalogManager::WorkloadClassCatalogInfo klass{};
    };

    struct BindingState
    {
        bool matched = false;
        CatalogManager::AdmissionPolicyCatalogInfo policy{};
        CatalogManager::AdmissionBindingCatalogInfo binding{};
    };

    auto resolveWorkloadClass(const QueryDescriptor& descriptor,
                              MatchState& state_out,
                              ErrorContext* ctx) const -> Status;
    auto resolveBinding(const MatchState& match,
                        BindingState& state_out,
                        ErrorContext* ctx) const -> Status;
    auto collectSloEvaluations(std::vector<SloEvaluationRow>& rows_out,
                               uint64_t evaluation_time,
                               ErrorContext* ctx) const -> Status;

    auto countActiveSessionsLocked(const BindingState& binding,
                                   const std::vector<uint32_t>& active_proc_ids,
                                   uint32_t current_proc_id,
                                   bool current_session_known) const -> uint32_t;
    void releaseLease(uint32_t proc_id,
                      const ID& policy_id,
                      const ID& class_id,
                      uint64_t accelerator_reserved_memory_bytes,
                      bool accelerator_search_active,
                      bool accelerator_build_active);

    Database* db_ = nullptr;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::unordered_map<ID, CounterState, IDHash> policy_counters_;
    std::unordered_map<uint32_t, ID> session_class_map_;
    std::unordered_map<uint32_t, ID> session_policy_map_;
    std::unordered_map<std::string, std::vector<SloTelemetrySample>> slo_telemetry_history_;
};

} // namespace scratchbird::core
