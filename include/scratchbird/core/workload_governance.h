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
    struct QueryDescriptor
    {
        ConnectionContext* connection = nullptr;
        std::string sql;
        std::string database_name;
        std::string schema_name;
        std::string client_app;
        std::string resource_tag;
    };

    struct AdmissionDecision
    {
        bool admitted = true;
        bool queued = false;
        Status status = Status::OK;
        std::string code;
        std::string detail;
        ID class_id{};
        ID policy_id{};
        std::string class_name;
        std::string policy_name;
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
        uint32_t proc_id_ = 0;
        ID class_id_{};
        ID policy_id_{};
        std::string class_name_;
        std::string policy_name_;
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

    explicit WorkloadGovernance(Database* db);

    auto acquire(const QueryDescriptor& descriptor,
                 AdmissionLease& lease_out,
                 ErrorContext* ctx = nullptr) -> AdmissionDecision;

    auto snapshotAdmissionStatus(std::vector<AdmissionStatusRow>& rows_out,
                                 ErrorContext* ctx = nullptr) const -> Status;
    auto snapshotRoutingPlan(std::vector<RoutingPlanRow>& rows_out,
                             ErrorContext* ctx = nullptr) const -> Status;

private:
    struct CounterState
    {
        uint32_t active_queries = 0;
        uint32_t queued_queries = 0;
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

    auto countActiveSessionsLocked(const BindingState& binding,
                                   const std::vector<uint32_t>& active_proc_ids,
                                   uint32_t current_proc_id,
                                   bool current_session_known) const -> uint32_t;
    void releaseLease(uint32_t proc_id, const ID& policy_id, const ID& class_id);

    Database* db_ = nullptr;
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    std::unordered_map<ID, CounterState, IDHash> policy_counters_;
    std::unordered_map<uint32_t, ID> session_class_map_;
    std::unordered_map<uint32_t, ID> session_policy_map_;
};

} // namespace scratchbird::core
