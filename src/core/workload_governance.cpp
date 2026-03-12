/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/workload_governance.h"

#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace scratchbird::core
{

namespace
{

auto trimAsciiCopy(const std::string& value) -> std::string
{
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0)
    {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }
    return value.substr(begin, end - begin);
}

auto toUpperAscii(std::string value) -> std::string
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return value;
}

auto toLowerAscii(std::string value) -> std::string
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

auto normalizeRuleValue(const std::string& raw) -> std::string
{
    std::string value = trimAsciiCopy(raw);
    const size_t eq = value.find('=');
    if (eq != std::string::npos)
    {
        value = trimAsciiCopy(value.substr(eq + 1));
    }
    if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')
    {
        value = value.substr(1, value.size() - 2);
    }
    return trimAsciiCopy(value);
}

auto baseNameFromPath(const std::string& path) -> std::string
{
    const size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos)
    {
        return path;
    }
    return path.substr(slash + 1);
}

auto classifyQueryType(const std::string& sql) -> std::string
{
    std::string trimmed = trimAsciiCopy(sql);
    std::string keyword;
    for (unsigned char ch : trimmed)
    {
        if (std::isspace(ch) != 0)
        {
            break;
        }
        keyword.push_back(static_cast<char>(std::toupper(ch)));
    }

    if (keyword == "SELECT" || keyword == "WITH")
    {
        return "select";
    }
    if (keyword == "INSERT")
    {
        return "insert";
    }
    if (keyword == "UPDATE")
    {
        return "update";
    }
    if (keyword == "DELETE")
    {
        return "delete";
    }
    if (keyword == "MERGE")
    {
        return "merge";
    }
    if (keyword == "COPY")
    {
        return "copy";
    }
    if (keyword == "CREATE" || keyword == "ALTER" || keyword == "DROP" ||
        keyword == "TRUNCATE" || keyword == "COMMENT" || keyword == "GRANT" ||
        keyword == "REVOKE" || keyword == "RENAME")
    {
        return "ddl";
    }
    if (!keyword.empty())
    {
        return "other";
    }
    return "unknown";
}

auto statementTagForSql(const std::string& sql) -> std::string
{
    std::string trimmed = trimAsciiCopy(sql);
    std::string tag;
    for (unsigned char ch : trimmed)
    {
        if (std::isspace(ch) != 0)
        {
            break;
        }
        tag.push_back(static_cast<char>(std::toupper(ch)));
    }
    return tag;
}

auto rejectModeToString(CatalogManager::AdmissionRejectMode mode) -> std::string
{
    switch (mode)
    {
        case CatalogManager::AdmissionRejectMode::REJECT:
            return "REJECT";
        case CatalogManager::AdmissionRejectMode::QUEUE:
            return "QUEUE";
        case CatalogManager::AdmissionRejectMode::SHED_LOW_PRIORITY:
            return "SHED_LOW_PRIORITY";
    }
    return "UNKNOWN";
}

auto routeTargetKindToString(CatalogManager::RouteTargetKind kind) -> std::string
{
    switch (kind)
    {
        case CatalogManager::RouteTargetKind::NODE:
            return "NODE";
        case CatalogManager::RouteTargetKind::SERVICE:
            return "SERVICE";
        case CatalogManager::RouteTargetKind::ROLE:
            return "ROLE";
        case CatalogManager::RouteTargetKind::SHARD:
            return "SHARD";
        case CatalogManager::RouteTargetKind::TIER:
            return "TIER";
    }
    return "UNKNOWN";
}

auto transportToString(CatalogManager::ConnectionTransport transport) -> std::string
{
    switch (transport)
    {
        case CatalogManager::ConnectionTransport::LOCAL:
            return "LOCAL";
        case CatalogManager::ConnectionTransport::IPC:
            return "IPC";
        case CatalogManager::ConnectionTransport::INET:
            return "INET";
    }
    return "UNKNOWN";
}

auto roleToString(CatalogManager::ClusterNodeRole role) -> std::string
{
    switch (role)
    {
        case CatalogManager::ClusterNodeRole::METADATA:
            return "METADATA";
        case CatalogManager::ClusterNodeRole::OLTP_DATA:
            return "OLTP_DATA";
        case CatalogManager::ClusterNodeRole::ROUTER:
            return "ROUTER";
        case CatalogManager::ClusterNodeRole::PARSER:
            return "PARSER";
        case CatalogManager::ClusterNodeRole::LISTENER:
            return "LISTENER";
        case CatalogManager::ClusterNodeRole::BACKUP:
            return "BACKUP";
        case CatalogManager::ClusterNodeRole::SCHEDULER:
            return "SCHEDULER";
        case CatalogManager::ClusterNodeRole::METRICS:
            return "METRICS";
        case CatalogManager::ClusterNodeRole::OLAP_INGEST:
            return "OLAP_INGEST";
        case CatalogManager::ClusterNodeRole::OLAP_STORAGE:
            return "OLAP_STORAGE";
        case CatalogManager::ClusterNodeRole::OLAP_COMPUTE:
            return "OLAP_COMPUTE";
        case CatalogManager::ClusterNodeRole::VECTOR_INDEX:
            return "VECTOR_INDEX";
        case CatalogManager::ClusterNodeRole::SEARCH_INDEX:
            return "SEARCH_INDEX";
        case CatalogManager::ClusterNodeRole::GRAPH_COMPUTE:
            return "GRAPH_COMPUTE";
        case CatalogManager::ClusterNodeRole::CACHE:
            return "CACHE";
    }
    return "UNKNOWN";
}

auto serviceTypeToString(CatalogManager::ClusterServiceType type) -> std::string
{
    switch (type)
    {
        case CatalogManager::ClusterServiceType::OLTP_RPC:
            return "OLTP_RPC";
        case CatalogManager::ClusterServiceType::OLAP_INGEST:
            return "OLAP_INGEST";
        case CatalogManager::ClusterServiceType::OLAP_QUERY:
            return "OLAP_QUERY";
        case CatalogManager::ClusterServiceType::VECTOR_QUERY:
            return "VECTOR_QUERY";
        case CatalogManager::ClusterServiceType::TEXT_SEARCH:
            return "TEXT_SEARCH";
        case CatalogManager::ClusterServiceType::GRAPH_QUERY:
            return "GRAPH_QUERY";
        case CatalogManager::ClusterServiceType::BACKUP:
            return "BACKUP";
        case CatalogManager::ClusterServiceType::METRICS:
            return "METRICS";
        case CatalogManager::ClusterServiceType::ADMIN:
            return "ADMIN";
    }
    return "UNKNOWN";
}

auto isZeroUuidLocal(const ID& id) -> bool
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

auto sloTelemetryKey(const ID& node_id, CatalogManager::ClusterNodeRole role) -> std::string
{
    return node_id.toString() + "|" + roleToString(role);
}

auto burnSeverityToString(CatalogManager::SloBurnSeverity severity) -> std::string
{
    switch (severity)
    {
        case CatalogManager::SloBurnSeverity::NONE:
            return "NONE";
        case CatalogManager::SloBurnSeverity::MODERATE:
            return "MODERATE";
        case CatalogManager::SloBurnSeverity::HIGH:
            return "HIGH";
        case CatalogManager::SloBurnSeverity::CRITICAL:
            return "CRITICAL";
    }
    return "UNKNOWN";
}

auto actionPlanToString(CatalogManager::SloActionPlan action_plan) -> std::string
{
    switch (action_plan)
    {
        case CatalogManager::SloActionPlan::NONE:
            return "NONE";
        case CatalogManager::SloActionPlan::ADMISSION_TIGHTEN:
            return "ADMISSION_TIGHTEN";
        case CatalogManager::SloActionPlan::SCALE_OUT:
            return "SCALE_OUT";
        case CatalogManager::SloActionPlan::SCALE_OUT_AND_TIGHTEN:
            return "SCALE_OUT_AND_TIGHTEN";
        case CatalogManager::SloActionPlan::INCIDENT_PAGE:
            return "INCIDENT_PAGE";
    }
    return "UNKNOWN";
}

auto windowLowerBound(uint64_t evaluation_time, uint32_t minutes) -> uint64_t
{
    const uint64_t duration_ms = static_cast<uint64_t>(minutes) * 60ULL * 1000ULL;
    return evaluation_time > duration_ms ? (evaluation_time - duration_ms) : 0;
}

auto computeAvailabilitySliPct(uint64_t request_count, uint64_t success_count) -> double
{
    if (request_count == 0)
    {
        return 100.0;
    }
    return (static_cast<double>(success_count) / static_cast<double>(request_count)) * 100.0;
}

auto computeErrorRateSliPct(uint64_t request_count, uint64_t error_count) -> double
{
    if (request_count == 0)
    {
        return 0.0;
    }
    return (static_cast<double>(error_count) / static_cast<double>(request_count)) * 100.0;
}

auto computeObservedBadRequests(uint64_t request_count,
                                uint64_t success_count,
                                uint64_t error_count) -> double
{
    const uint64_t availability_bad =
        request_count > success_count ? (request_count - success_count) : 0;
    return static_cast<double>(std::max(availability_bad, error_count));
}

auto computeAllowedBadRequests(const CatalogManager::SloProfileCatalogInfo& profile,
                               uint64_t request_count) -> double
{
    const double availability_budget =
        ((100.0 - profile.availability_target_pct) / 100.0) * static_cast<double>(request_count);
    const double error_budget =
        (profile.error_rate_target_pct / 100.0) * static_cast<double>(request_count);
    return std::max(availability_budget, error_budget);
}

auto computeBurnRate(double observed_bad_requests, double allowed_bad_requests) -> double
{
    if (allowed_bad_requests <= 0.0)
    {
        return observed_bad_requests <= 0.0
            ? 0.0
            : std::numeric_limits<double>::infinity();
    }
    return observed_bad_requests / allowed_bad_requests;
}

auto classifyBurnSeverity(double short_burn_rate,
                          double long_burn_rate,
                          const CatalogManager::SloProfileCatalogInfo& profile)
    -> CatalogManager::SloBurnSeverity
{
    if (short_burn_rate >= profile.critical_burn_threshold &&
        long_burn_rate >= (profile.critical_burn_threshold / 2.0))
    {
        return CatalogManager::SloBurnSeverity::CRITICAL;
    }
    if (short_burn_rate >= profile.high_burn_threshold &&
        long_burn_rate >= (profile.high_burn_threshold / 2.0))
    {
        return CatalogManager::SloBurnSeverity::HIGH;
    }
    if (short_burn_rate >= profile.moderate_burn_threshold &&
        long_burn_rate >= (profile.moderate_burn_threshold / 2.0))
    {
        return CatalogManager::SloBurnSeverity::MODERATE;
    }
    return CatalogManager::SloBurnSeverity::NONE;
}

} // namespace

struct WorkloadGovernance::SloEvaluationRow
{
    ID node_id{};
    std::string node_name;
    CatalogManager::ClusterNodeRole role = CatalogManager::ClusterNodeRole::OLTP_DATA;
    CatalogManager::SloProfileCatalogInfo profile{};
    bool binding_present = false;
    bool metrics_present = false;
    uint64_t evaluation_time = 0;
    uint64_t window_start_time = 0;
    uint64_t window_end_time = 0;
    uint64_t request_count = 0;
    uint64_t success_count = 0;
    uint64_t error_count = 0;
    uint32_t latency_p95_ms = 0;
    uint32_t latency_p99_ms = 0;
    double availability_sli_pct = 0.0;
    double error_rate_sli_pct = 0.0;
    double allowed_bad_requests = 0.0;
    double observed_bad_requests = 0.0;
    double remaining_bad_requests = 0.0;
    double remaining_budget_pct = 0.0;
    double short_burn_rate = 0.0;
    double long_burn_rate = 0.0;
    CatalogManager::SloBurnSeverity burn_severity = CatalogManager::SloBurnSeverity::NONE;
    CatalogManager::SloActionPlan action_plan = CatalogManager::SloActionPlan::NONE;
    uint8_t latest_cpu_utilization_pct = 0;
    uint8_t latest_queue_pressure_pct = 0;
    uint16_t current_node_count = 0;
    bool scale_out_candidate = false;
    bool scale_in_candidate = false;
};

WorkloadGovernance::AdmissionLease::~AdmissionLease()
{
    release();
}

WorkloadGovernance::AdmissionLease::AdmissionLease(AdmissionLease&& other) noexcept
{
    moveFrom(std::move(other));
}

auto WorkloadGovernance::AdmissionLease::operator=(AdmissionLease&& other) noexcept
    -> AdmissionLease&
{
    if (this != &other)
    {
        release();
        moveFrom(std::move(other));
    }
    return *this;
}

void WorkloadGovernance::AdmissionLease::release()
{
    if (owner_ != nullptr && active_)
    {
        owner_->releaseLease(proc_id_, policy_id_, class_id_);
    }
    owner_ = nullptr;
    proc_id_ = UINT32_MAX;
    class_id_ = ID{};
    policy_id_ = ID{};
    class_name_.clear();
    policy_name_.clear();
    active_ = false;
}

void WorkloadGovernance::AdmissionLease::moveFrom(AdmissionLease&& other) noexcept
{
    owner_ = other.owner_;
    proc_id_ = other.proc_id_;
    class_id_ = other.class_id_;
    policy_id_ = other.policy_id_;
    class_name_ = std::move(other.class_name_);
    policy_name_ = std::move(other.policy_name_);
    active_ = other.active_;

    other.owner_ = nullptr;
    other.proc_id_ = UINT32_MAX;
    other.class_id_ = ID{};
    other.policy_id_ = ID{};
    other.active_ = false;
}

WorkloadGovernance::WorkloadGovernance(Database* db)
    : db_(db)
{
}

auto WorkloadGovernance::recordSloTelemetrySample(const SloTelemetrySample& sample,
                                                  ErrorContext* ctx) -> Status
{
    if (isZeroUuidLocal(sample.node_id) ||
        !sample.is_valid ||
        sample.sample_time == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid SLO telemetry sample");
        return Status::INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    auto& history = slo_telemetry_history_[sloTelemetryKey(sample.node_id, sample.role)];
    history.push_back(sample);
    std::sort(history.begin(), history.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.sample_time != rhs.sample_time)
        {
            return lhs.sample_time < rhs.sample_time;
        }
        if (lhs.cpu_utilization_pct != rhs.cpu_utilization_pct)
        {
            return lhs.cpu_utilization_pct < rhs.cpu_utilization_pct;
        }
        return lhs.queue_pressure_pct < rhs.queue_pressure_pct;
    });
    if (history.size() > 64)
    {
        history.erase(history.begin(), history.end() - 64);
    }
    return Status::OK;
}

auto WorkloadGovernance::collectSloEvaluations(std::vector<SloEvaluationRow>& rows_out,
                                               uint64_t evaluation_time,
                                               ErrorContext* ctx) const -> Status
{
    rows_out.clear();
    auto* catalog = db_ ? db_->catalog_manager() : nullptr;
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Catalog manager not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<CatalogManager::NodeCatalogInfo> nodes;
    Status status = catalog->listNodeCatalogEntries(ID{}, nodes, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    if (status == Status::NOT_FOUND)
    {
        nodes.clear();
    }

    std::vector<CatalogManager::SloProfileCatalogInfo> profiles;
    status = catalog->listSloProfileCatalogEntries(profiles, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    if (status == Status::NOT_FOUND)
    {
        profiles.clear();
    }

    std::vector<CatalogManager::SloBindingCatalogInfo> bindings;
    status = catalog->listSloBindingCatalogEntries(ID{}, bindings, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    if (status == Status::NOT_FOUND)
    {
        bindings.clear();
    }

    std::vector<CatalogManager::SloWindowCatalogInfo> windows;
    status = catalog->listSloWindowCatalogEntries(ID{}, windows, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    if (status == Status::NOT_FOUND)
    {
        windows.clear();
    }

    std::vector<CatalogManager::AutoscalePolicyCatalogInfo> autoscale_policies;
    status = catalog->listAutoscalePolicyCatalogEntries(autoscale_policies, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    if (status == Status::NOT_FOUND)
    {
        autoscale_policies.clear();
    }

    struct NodeKey
    {
        ID node_id{};
        CatalogManager::ClusterNodeRole role = CatalogManager::ClusterNodeRole::OLTP_DATA;
        std::string node_name;
    };

    std::vector<NodeKey> candidates;
    auto append_candidate =
        [&](const ID& node_id,
            CatalogManager::ClusterNodeRole role,
            const std::string& node_name) {
            auto exists = std::find_if(candidates.begin(), candidates.end(), [&](const auto& candidate) {
                return candidate.node_id == node_id && candidate.role == role;
            });
            if (exists == candidates.end())
            {
                candidates.push_back(NodeKey{node_id, role, node_name});
            }
        };

    for (const auto& node : nodes)
    {
        if (!node.is_valid || isZeroUuidLocal(node.node_id))
        {
            continue;
        }
        append_candidate(node.node_id, node.node_role, node.node_name);
    }
    for (const auto& window : windows)
    {
        if (!window.is_valid || isZeroUuidLocal(window.node_id))
        {
            continue;
        }
        append_candidate(window.node_id, window.role, window.node_id.toString());
    }

    std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.role != rhs.role)
        {
            return static_cast<uint8_t>(lhs.role) < static_cast<uint8_t>(rhs.role);
        }
        if (lhs.node_name != rhs.node_name)
        {
            return lhs.node_name < rhs.node_name;
        }
        return lhs.node_id.bytes < rhs.node_id.bytes;
    });

    auto find_profile = [&](const ID& profile_id) -> const CatalogManager::SloProfileCatalogInfo* {
        for (const auto& profile : profiles)
        {
            if (profile.is_valid &&
                profile.is_active &&
                profile.slo_profile_id == profile_id)
            {
                return &profile;
            }
        }
        return nullptr;
    };

    auto find_autoscale_policy =
        [&](CatalogManager::ClusterNodeRole role) -> const CatalogManager::AutoscalePolicyCatalogInfo* {
            const CatalogManager::AutoscalePolicyCatalogInfo* best = nullptr;
            for (const auto& policy : autoscale_policies)
            {
                if (!policy.is_valid || policy.role != role)
                {
                    continue;
                }
                if (best == nullptr || policy.version_u64 > best->version_u64)
                {
                    best = &policy;
                }
            }
            return best;
        };

    for (const auto& candidate : candidates)
    {
        SloEvaluationRow row;
        row.node_id = candidate.node_id;
        row.node_name = candidate.node_name.empty() ? candidate.node_id.toString() : candidate.node_name;
        row.role = candidate.role;

        std::vector<CatalogManager::SloWindowCatalogInfo> node_windows;
        for (const auto& window : windows)
        {
            if (!window.is_valid ||
                window.node_id != candidate.node_id ||
                window.role != candidate.role)
            {
                continue;
            }
            node_windows.push_back(window);
        }
        std::sort(node_windows.begin(), node_windows.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.window_end_time != rhs.window_end_time)
            {
                return lhs.window_end_time < rhs.window_end_time;
            }
            return lhs.window_start_time < rhs.window_start_time;
        });

        uint64_t candidate_eval_time = evaluation_time;
        if (candidate_eval_time == 0)
        {
            if (!node_windows.empty())
            {
                candidate_eval_time = node_windows.back().window_end_time;
            }
            else
            {
                for (const auto& binding : bindings)
                {
                    if (!binding.is_valid || binding.role != candidate.role)
                    {
                        continue;
                    }
                    if (binding.has_node_id && binding.node_id != candidate.node_id)
                    {
                        continue;
                    }
                    candidate_eval_time = std::max(candidate_eval_time, binding.effective_from_time);
                }
            }
        }
        row.evaluation_time = candidate_eval_time;

        const CatalogManager::SloBindingCatalogInfo* best_binding = nullptr;
        const CatalogManager::SloProfileCatalogInfo* best_profile = nullptr;
        bool best_node_specific = false;
        for (const auto& binding : bindings)
        {
            if (!binding.is_valid || binding.role != candidate.role)
            {
                continue;
            }
            if (candidate_eval_time != 0 && binding.effective_from_time > candidate_eval_time)
            {
                continue;
            }
            if (binding.has_effective_to_time &&
                candidate_eval_time != 0 &&
                candidate_eval_time >= binding.effective_to_time)
            {
                continue;
            }
            const bool node_specific = binding.has_node_id && binding.node_id == candidate.node_id;
            if (binding.has_node_id && !node_specific)
            {
                continue;
            }

            const auto* profile = find_profile(binding.slo_profile_id);
            if (profile == nullptr)
            {
                continue;
            }

            if (best_binding == nullptr)
            {
                best_binding = &binding;
                best_profile = profile;
                best_node_specific = node_specific;
                continue;
            }

            if (node_specific != best_node_specific)
            {
                if (node_specific)
                {
                    best_binding = &binding;
                    best_profile = profile;
                    best_node_specific = true;
                }
                continue;
            }

            if (binding.priority_rank != best_binding->priority_rank)
            {
                if (binding.priority_rank < best_binding->priority_rank)
                {
                    best_binding = &binding;
                    best_profile = profile;
                    best_node_specific = node_specific;
                }
                continue;
            }

            if (binding.effective_from_time != best_binding->effective_from_time)
            {
                if (binding.effective_from_time > best_binding->effective_from_time)
                {
                    best_binding = &binding;
                    best_profile = profile;
                    best_node_specific = node_specific;
                }
                continue;
            }

            if (profile->profile_name < best_profile->profile_name)
            {
                best_binding = &binding;
                best_profile = profile;
                best_node_specific = node_specific;
            }
        }

        if (best_profile != nullptr)
        {
            row.binding_present = true;
            row.profile = *best_profile;
        }

        auto aggregate_window =
            [&](uint64_t lower_bound,
                uint64_t upper_bound,
                uint64_t& request_count_out,
                uint64_t& success_count_out,
                uint64_t& error_count_out,
                uint32_t& latency_p95_out,
                uint32_t& latency_p99_out,
                uint64_t& window_start_out,
                uint64_t& window_end_out) -> bool {
                request_count_out = 0;
                success_count_out = 0;
                error_count_out = 0;
                latency_p95_out = 0;
                latency_p99_out = 0;
                window_start_out = 0;
                window_end_out = 0;
                bool seen = false;

                for (const auto& window : node_windows)
                {
                    if (window.window_end_time < lower_bound ||
                        window.window_start_time > upper_bound)
                    {
                        continue;
                    }

                    request_count_out += window.request_count;
                    success_count_out += window.success_count;
                    error_count_out += window.error_count;
                    latency_p95_out = std::max(latency_p95_out, window.latency_p95_ms);
                    latency_p99_out = std::max(latency_p99_out, window.latency_p99_ms);
                    if (!seen)
                    {
                        window_start_out = window.window_start_time;
                        window_end_out = window.window_end_time;
                        seen = true;
                    }
                    else
                    {
                        window_start_out = std::min(window_start_out, window.window_start_time);
                        window_end_out = std::max(window_end_out, window.window_end_time);
                    }
                }

                return seen;
            };

        uint64_t latest_request_count = 0;
        uint64_t latest_success_count = 0;
        uint64_t latest_error_count = 0;
        uint32_t latest_latency_p95 = 0;
        uint32_t latest_latency_p99 = 0;
        uint64_t latest_window_start = 0;
        uint64_t latest_window_end = 0;
        if (!node_windows.empty())
        {
            const auto& latest = node_windows.back();
            latest_request_count = latest.request_count;
            latest_success_count = latest.success_count;
            latest_error_count = latest.error_count;
            latest_latency_p95 = latest.latency_p95_ms;
            latest_latency_p99 = latest.latency_p99_ms;
            latest_window_start = latest.window_start_time;
            latest_window_end = latest.window_end_time;
        }

        if (row.binding_present && candidate_eval_time != 0)
        {
            uint64_t short_request_count = 0;
            uint64_t short_success_count = 0;
            uint64_t short_error_count = 0;
            uint32_t short_latency_p95 = 0;
            uint32_t short_latency_p99 = 0;
            uint64_t short_window_start = 0;
            uint64_t short_window_end = 0;
            const bool short_seen = aggregate_window(
                windowLowerBound(candidate_eval_time, row.profile.short_burn_window_minutes),
                candidate_eval_time,
                short_request_count,
                short_success_count,
                short_error_count,
                short_latency_p95,
                short_latency_p99,
                short_window_start,
                short_window_end);

            uint64_t long_request_count = 0;
            uint64_t long_success_count = 0;
            uint64_t long_error_count = 0;
            uint32_t long_latency_p95 = 0;
            uint32_t long_latency_p99 = 0;
            uint64_t long_window_start = 0;
            uint64_t long_window_end = 0;
            const bool long_seen = aggregate_window(
                windowLowerBound(candidate_eval_time, row.profile.long_burn_window_minutes),
                candidate_eval_time,
                long_request_count,
                long_success_count,
                long_error_count,
                long_latency_p95,
                long_latency_p99,
                long_window_start,
                long_window_end);

            if (long_seen)
            {
                row.metrics_present = true;
                row.window_start_time = long_window_start;
                row.window_end_time = long_window_end;
                row.request_count = long_request_count;
                row.success_count = long_success_count;
                row.error_count = long_error_count;
                row.latency_p95_ms = long_latency_p95;
                row.latency_p99_ms = long_latency_p99;
                row.availability_sli_pct =
                    computeAvailabilitySliPct(long_request_count, long_success_count);
                row.error_rate_sli_pct =
                    computeErrorRateSliPct(long_request_count, long_error_count);
                row.allowed_bad_requests =
                    computeAllowedBadRequests(row.profile, long_request_count);
                row.observed_bad_requests =
                    computeObservedBadRequests(long_request_count, long_success_count, long_error_count);
                row.remaining_bad_requests =
                    std::max(0.0, row.allowed_bad_requests - row.observed_bad_requests);
                row.remaining_budget_pct = row.allowed_bad_requests > 0.0
                    ? (row.remaining_bad_requests / row.allowed_bad_requests) * 100.0
                    : 100.0;

                if (short_seen)
                {
                    row.short_burn_rate = computeBurnRate(
                        computeObservedBadRequests(
                            short_request_count,
                            short_success_count,
                            short_error_count),
                        computeAllowedBadRequests(row.profile, short_request_count));
                }
                row.long_burn_rate =
                    computeBurnRate(row.observed_bad_requests, row.allowed_bad_requests);
                row.burn_severity =
                    classifyBurnSeverity(row.short_burn_rate, row.long_burn_rate, row.profile);
            }
        }

        if (!row.metrics_present && latest_request_count > 0)
        {
            row.metrics_present = true;
            row.window_start_time = latest_window_start;
            row.window_end_time = latest_window_end;
            row.request_count = latest_request_count;
            row.success_count = latest_success_count;
            row.error_count = latest_error_count;
            row.latency_p95_ms = latest_latency_p95;
            row.latency_p99_ms = latest_latency_p99;
            row.availability_sli_pct =
                computeAvailabilitySliPct(latest_request_count, latest_success_count);
            row.error_rate_sli_pct =
                computeErrorRateSliPct(latest_request_count, latest_error_count);
        }

        std::vector<SloTelemetrySample> telemetry_history;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto history_it = slo_telemetry_history_.find(sloTelemetryKey(candidate.node_id, candidate.role));
            if (history_it != slo_telemetry_history_.end())
            {
                telemetry_history = history_it->second;
            }
        }
        std::sort(telemetry_history.begin(), telemetry_history.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.sample_time < rhs.sample_time;
        });

        std::vector<SloTelemetrySample> relevant_samples;
        for (const auto& sample : telemetry_history)
        {
            if (!sample.is_valid)
            {
                continue;
            }
            if (candidate_eval_time != 0 && sample.sample_time > candidate_eval_time)
            {
                continue;
            }
            relevant_samples.push_back(sample);
        }

        uint16_t role_node_count = 0;
        for (const auto& node : nodes)
        {
            if (node.is_valid && node.node_role == candidate.role)
            {
                ++role_node_count;
            }
        }
        row.current_node_count = role_node_count > 0 ? role_node_count : 1;
        if (!relevant_samples.empty())
        {
            const auto& latest_sample = relevant_samples.back();
            row.latest_cpu_utilization_pct = latest_sample.cpu_utilization_pct;
            row.latest_queue_pressure_pct = latest_sample.queue_pressure_pct;
            if (latest_sample.current_node_count > 0)
            {
                row.current_node_count = latest_sample.current_node_count;
            }
        }

        const auto last_n_samples_match =
            [&](size_t required_count, auto&& predicate) -> bool {
                if (relevant_samples.size() < required_count)
                {
                    return false;
                }
                size_t matched = 0;
                for (auto it = relevant_samples.rbegin();
                     it != relevant_samples.rend() && matched < required_count;
                     ++it)
                {
                    if (!predicate(*it))
                    {
                        return false;
                    }
                    ++matched;
                }
                return matched == required_count;
            };

        const auto recent_windows_below_recovery_threshold = [&]() -> bool {
            if (!row.binding_present || node_windows.size() < 24)
            {
                return false;
            }

            size_t validated = 0;
            for (auto it = node_windows.rbegin();
                 it != node_windows.rend() && validated < 24;
                 ++it)
            {
                if (!it->is_valid)
                {
                    return false;
                }
                const double observed_bad_requests = computeObservedBadRequests(
                    it->request_count,
                    it->success_count,
                    it->error_count);
                const double allowed_bad_requests =
                    computeAllowedBadRequests(row.profile, it->request_count);
                const double burn_rate =
                    computeBurnRate(observed_bad_requests, allowed_bad_requests);
                if (classifyBurnSeverity(burn_rate, burn_rate, row.profile) !=
                    CatalogManager::SloBurnSeverity::NONE)
                {
                    return false;
                }
                ++validated;
            }
            return validated == 24;
        };

        if (row.binding_present)
        {
            if (row.burn_severity == CatalogManager::SloBurnSeverity::CRITICAL)
            {
                row.action_plan = CatalogManager::SloActionPlan::INCIDENT_PAGE;
            }
            else if (row.burn_severity == CatalogManager::SloBurnSeverity::HIGH)
            {
                row.action_plan = CatalogManager::SloActionPlan::ADMISSION_TIGHTEN;
            }

            if (const auto* autoscale_policy = find_autoscale_policy(candidate.role);
                autoscale_policy != nullptr &&
                row.metrics_present)
            {
                const bool sustained_scale_out = last_n_samples_match(
                    5,
                    [&](const auto& sample) {
                        return sample.cpu_utilization_pct >= autoscale_policy->cpu_scale_out_pct &&
                               sample.queue_pressure_pct >= autoscale_policy->queue_scale_out_pct;
                    });
                if ((row.burn_severity == CatalogManager::SloBurnSeverity::HIGH ||
                     row.burn_severity == CatalogManager::SloBurnSeverity::CRITICAL) &&
                    row.long_burn_rate >= autoscale_policy->slo_burn_scale_out_threshold &&
                    row.current_node_count < autoscale_policy->max_nodes &&
                    sustained_scale_out)
                {
                    row.scale_out_candidate = true;
                    if (row.burn_severity == CatalogManager::SloBurnSeverity::HIGH)
                    {
                        row.action_plan = CatalogManager::SloActionPlan::SCALE_OUT_AND_TIGHTEN;
                    }
                }

                const bool sustained_recovery = last_n_samples_match(
                    24,
                    [](const auto& sample) {
                        return sample.cpu_utilization_pct < 35 &&
                               sample.queue_pressure_pct < 25;
                    });
                if (row.burn_severity == CatalogManager::SloBurnSeverity::NONE &&
                    row.long_burn_rate <= autoscale_policy->slo_recovery_scale_in_threshold &&
                    row.current_node_count > autoscale_policy->min_nodes &&
                    sustained_recovery &&
                    recent_windows_below_recovery_threshold())
                {
                    row.scale_in_candidate = true;
                }
            }
        }

        rows_out.push_back(std::move(row));
    }

    return Status::OK;
}

auto WorkloadGovernance::evaluateSloPolicies(uint64_t evaluation_time,
                                             ErrorContext* ctx) -> Status
{
    auto* catalog = db_ ? db_->catalog_manager() : nullptr;
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Catalog manager not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<SloEvaluationRow> rows;
    Status status = collectSloEvaluations(rows, evaluation_time, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (const auto& row : rows)
    {
        if (!row.binding_present || !row.metrics_present)
        {
            continue;
        }

        CatalogManager::SloBurnEventCatalogInfo burn_event{};
        burn_event.slo_burn_event_id = generateUuidV7();
        burn_event.node_id = row.node_id;
        burn_event.role = row.role;
        burn_event.slo_profile_id = row.profile.slo_profile_id;
        burn_event.short_burn_rate = row.short_burn_rate;
        burn_event.long_burn_rate = row.long_burn_rate;
        burn_event.burn_severity = row.burn_severity;
        burn_event.action_plan = row.action_plan;
        burn_event.event_time = row.evaluation_time != 0 ? row.evaluation_time : row.window_end_time;
        status = catalog->upsertSloBurnEventCatalogEntry(burn_event, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    std::vector<CatalogManager::AdmissionPolicyCatalogInfo> admission_policies;
    status = catalog->listAdmissionPolicyCatalogEntries(admission_policies, ctx);
    if (status != Status::OK && status != Status::NOT_FOUND)
    {
        return status;
    }
    if (status == Status::NOT_FOUND)
    {
        admission_policies.clear();
    }

    std::unordered_map<std::string, const SloEvaluationRow*> strongest_rows;
    for (const auto& row : rows)
    {
        if (!row.binding_present || !row.metrics_present)
        {
            continue;
        }
        const std::string role_key = roleToString(row.role);
        auto it = strongest_rows.find(role_key);
        if (it == strongest_rows.end())
        {
            strongest_rows.emplace(role_key, &row);
            continue;
        }

        const auto* current = it->second;
        if (static_cast<uint8_t>(row.burn_severity) > static_cast<uint8_t>(current->burn_severity) ||
            (row.burn_severity == current->burn_severity &&
             row.long_burn_rate > current->long_burn_rate))
        {
            it->second = &row;
        }
    }

    for (const auto& [role_key, row_ptr] : strongest_rows)
    {
        (void)role_key;
        const auto& row = *row_ptr;

        if (row.burn_severity == CatalogManager::SloBurnSeverity::HIGH ||
            row.burn_severity == CatalogManager::SloBurnSeverity::CRITICAL)
        {
            const double factor =
                row.burn_severity == CatalogManager::SloBurnSeverity::CRITICAL ? 0.80 : 0.90;

            for (auto policy : admission_policies)
            {
                if (!policy.is_valid || !policy.is_enabled)
                {
                    continue;
                }

                CatalogManager::AdmissionPolicyCatalogInfo updated = policy;
                const uint32_t scaled_queries = std::max<uint32_t>(
                    1u,
                    static_cast<uint32_t>(std::floor(
                        static_cast<double>(policy.max_concurrent_queries) * factor)));
                updated.max_concurrent_queries = std::min(policy.max_concurrent_queries, scaled_queries);

                if (policy.max_queue_depth > 0)
                {
                    const uint32_t scaled_depth = static_cast<uint32_t>(std::floor(
                        static_cast<double>(policy.max_queue_depth) * factor));
                    updated.max_queue_depth =
                        std::min(policy.max_queue_depth, std::max<uint32_t>(100u, scaled_depth));
                }

                if (policy.queue_timeout_ms > 0)
                {
                    const uint32_t scaled_timeout = static_cast<uint32_t>(std::floor(
                        static_cast<double>(policy.queue_timeout_ms) * factor));
                    updated.queue_timeout_ms =
                        std::min(policy.queue_timeout_ms, std::max<uint32_t>(250u, scaled_timeout));
                }

                if (updated.max_concurrent_queries == policy.max_concurrent_queries &&
                    updated.max_queue_depth == policy.max_queue_depth &&
                    updated.queue_timeout_ms == policy.queue_timeout_ms)
                {
                    continue;
                }

                status = catalog->upsertAdmissionPolicyCatalogEntry(updated, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                CatalogManager::AdmissionTuningEventCatalogInfo tuning{};
                tuning.admission_tuning_event_id = generateUuidV7();
                tuning.role = row.role;
                tuning.old_max_concurrent_queries = policy.max_concurrent_queries;
                tuning.new_max_concurrent_queries = updated.max_concurrent_queries;
                tuning.old_max_queue_depth = policy.max_queue_depth;
                tuning.new_max_queue_depth = updated.max_queue_depth;
                tuning.old_queue_timeout_ms = policy.queue_timeout_ms;
                tuning.new_queue_timeout_ms = updated.queue_timeout_ms;
                tuning.reason = "slo burn " + burnSeverityToString(row.burn_severity);
                tuning.policy_version_u64 =
                    updated.last_modified_time != 0
                        ? updated.last_modified_time
                        : (row.evaluation_time != 0 ? row.evaluation_time : row.window_end_time);
                tuning.event_time = row.evaluation_time != 0 ? row.evaluation_time : row.window_end_time;
                status = catalog->upsertAdmissionTuningEventCatalogEntry(tuning, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }

        std::vector<CatalogManager::AutoscalePolicyCatalogInfo> autoscale_policies;
        status = catalog->listAutoscalePolicyCatalogEntries(autoscale_policies, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        if (status == Status::NOT_FOUND)
        {
            autoscale_policies.clear();
        }

        const CatalogManager::AutoscalePolicyCatalogInfo* autoscale_policy = nullptr;
        for (const auto& policy : autoscale_policies)
        {
            if (!policy.is_valid || policy.role != row.role)
            {
                continue;
            }
            if (autoscale_policy == nullptr || policy.version_u64 > autoscale_policy->version_u64)
            {
                autoscale_policy = &policy;
            }
        }
        if (autoscale_policy == nullptr)
        {
            continue;
        }

        std::vector<CatalogManager::AutoscaleActionCatalogInfo> action_rows;
        status = catalog->listAutoscaleActionCatalogEntries(row.role, action_rows, ctx);
        if (status != Status::OK && status != Status::NOT_FOUND)
        {
            return status;
        }
        if (status == Status::NOT_FOUND)
        {
            action_rows.clear();
        }

        const auto cooldown_expired =
            [&](CatalogManager::AutoscaleActionKind kind, uint32_t cooldown_ms) {
                uint64_t latest_time = 0;
                for (const auto& action : action_rows)
                {
                    if (!action.is_valid || action.action_kind != kind)
                    {
                        continue;
                    }
                    latest_time = std::max(
                        latest_time,
                        action.has_completed_time ? action.completed_time : action.action_time);
                }
                const uint64_t anchor_time =
                    row.evaluation_time != 0 ? row.evaluation_time : row.window_end_time;
                return latest_time == 0 || anchor_time >= latest_time + cooldown_ms;
            };

        const uint64_t action_time = row.evaluation_time != 0 ? row.evaluation_time : row.window_end_time;
        if (row.scale_out_candidate &&
            cooldown_expired(CatalogManager::AutoscaleActionKind::SCALE_OUT,
                             autoscale_policy->scale_out_cooldown_ms))
        {
            const int16_t requested_delta = static_cast<int16_t>(autoscale_policy->scale_out_step);
            const uint16_t available = autoscale_policy->max_nodes > row.current_node_count
                ? static_cast<uint16_t>(autoscale_policy->max_nodes - row.current_node_count)
                : 0;
            const int16_t applied_delta = static_cast<int16_t>(
                std::min<uint16_t>(autoscale_policy->scale_out_step, available));
            if (applied_delta > 0)
            {
                CatalogManager::AutoscaleActionCatalogInfo action{};
                action.autoscale_action_id = generateUuidV7();
                action.role = row.role;
                action.action_kind = CatalogManager::AutoscaleActionKind::SCALE_OUT;
                action.requested_count_delta = requested_delta;
                action.applied_count_delta = applied_delta;
                action.trigger_reason = "slo burn " + burnSeverityToString(row.burn_severity);
                action.trigger_burn_rate = std::max(row.short_burn_rate, row.long_burn_rate);
                action.policy_version_u64 = autoscale_policy->version_u64;
                action.action_time = action_time;
                action.has_completed_time = true;
                action.completed_time = action_time;
                action.action_state = CatalogManager::AutoscaleActionState::APPLIED;
                status = catalog->upsertAutoscaleActionCatalogEntry(action, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }
        else if (row.scale_in_candidate &&
                 cooldown_expired(CatalogManager::AutoscaleActionKind::SCALE_IN,
                                  autoscale_policy->scale_in_cooldown_ms))
        {
            const uint16_t removable = row.current_node_count > autoscale_policy->min_nodes
                ? static_cast<uint16_t>(row.current_node_count - autoscale_policy->min_nodes)
                : 0;
            const int16_t applied_delta = static_cast<int16_t>(
                std::min<uint16_t>(autoscale_policy->scale_in_step, removable));
            if (applied_delta > 0)
            {
                CatalogManager::AutoscaleActionCatalogInfo action{};
                action.autoscale_action_id = generateUuidV7();
                action.role = row.role;
                action.action_kind = CatalogManager::AutoscaleActionKind::SCALE_IN;
                action.requested_count_delta = -static_cast<int16_t>(autoscale_policy->scale_in_step);
                action.applied_count_delta = -applied_delta;
                action.trigger_reason = "slo recovery";
                action.trigger_burn_rate = row.long_burn_rate;
                action.policy_version_u64 = autoscale_policy->version_u64;
                action.action_time = action_time;
                action.has_completed_time = true;
                action.completed_time = action_time;
                action.action_state = CatalogManager::AutoscaleActionState::APPLIED;
                status = catalog->upsertAutoscaleActionCatalogEntry(action, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }
    }

    return Status::OK;
}

auto WorkloadGovernance::snapshotSloStatus(std::vector<SloStatusRow>& rows_out,
                                           uint64_t evaluation_time,
                                           ErrorContext* ctx) const -> Status
{
    rows_out.clear();
    std::vector<SloEvaluationRow> rows;
    Status status = collectSloEvaluations(rows, evaluation_time, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (const auto& row : rows)
    {
        SloStatusRow public_row;
        public_row.node_id = row.node_id;
        public_row.node_name = row.node_name;
        public_row.role = roleToString(row.role);
        public_row.profile_name = row.binding_present ? row.profile.profile_name : std::string();
        public_row.evaluation_time = row.evaluation_time;
        public_row.window_start_time = row.window_start_time;
        public_row.window_end_time = row.window_end_time;
        public_row.request_count = row.request_count;
        public_row.success_count = row.success_count;
        public_row.error_count = row.error_count;
        public_row.availability_target_pct = row.binding_present ? row.profile.availability_target_pct : 0.0;
        public_row.availability_sli_pct = row.availability_sli_pct;
        public_row.latency_p95_target_ms = row.binding_present ? row.profile.latency_p95_target_ms : 0;
        public_row.latency_p95_ms = row.latency_p95_ms;
        public_row.latency_p99_target_ms = row.binding_present ? row.profile.latency_p99_target_ms : 0;
        public_row.latency_p99_ms = row.latency_p99_ms;
        public_row.error_rate_target_pct = row.binding_present ? row.profile.error_rate_target_pct : 0.0;
        public_row.error_rate_sli_pct = row.error_rate_sli_pct;
        public_row.short_burn_rate = row.short_burn_rate;
        public_row.long_burn_rate = row.long_burn_rate;
        public_row.burn_severity = burnSeverityToString(row.burn_severity);
        public_row.action_plan = actionPlanToString(row.action_plan);
        public_row.binding_present = row.binding_present;
        public_row.metrics_present = row.metrics_present;
        rows_out.push_back(std::move(public_row));
    }

    return Status::OK;
}

auto WorkloadGovernance::snapshotErrorBudgetStatus(
    std::vector<ErrorBudgetStatusRow>& rows_out,
    uint64_t evaluation_time,
    ErrorContext* ctx) const -> Status
{
    rows_out.clear();
    std::vector<SloEvaluationRow> rows;
    Status status = collectSloEvaluations(rows, evaluation_time, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (const auto& row : rows)
    {
        ErrorBudgetStatusRow public_row;
        public_row.node_id = row.node_id;
        public_row.node_name = row.node_name;
        public_row.role = roleToString(row.role);
        public_row.profile_name = row.binding_present ? row.profile.profile_name : std::string();
        public_row.evaluation_time = row.evaluation_time;
        public_row.window_start_time = row.window_start_time;
        public_row.window_end_time = row.window_end_time;
        public_row.allowed_bad_requests = row.allowed_bad_requests;
        public_row.observed_bad_requests = row.observed_bad_requests;
        public_row.remaining_bad_requests = row.remaining_bad_requests;
        public_row.remaining_budget_pct = row.remaining_budget_pct;
        public_row.short_burn_rate = row.short_burn_rate;
        public_row.long_burn_rate = row.long_burn_rate;
        public_row.burn_severity = burnSeverityToString(row.burn_severity);
        public_row.binding_present = row.binding_present;
        public_row.metrics_present = row.metrics_present;
        rows_out.push_back(std::move(public_row));
    }

    return Status::OK;
}

auto WorkloadGovernance::resolveWorkloadClass(const QueryDescriptor& descriptor,
                                              MatchState& state_out,
                                              ErrorContext* ctx) const -> Status
{
    state_out = MatchState{};
    auto* catalog = db_ ? db_->catalog_manager() : nullptr;
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Catalog manager not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<CatalogManager::WorkloadClassCatalogInfo> classes;
    Status status = catalog->listWorkloadClassCatalogEntries(classes, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::sort(classes.begin(), classes.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.priority != rhs.priority)
                  {
                      return lhs.priority > rhs.priority;
                  }
                  return lhs.class_name < rhs.class_name;
              });

    std::string user_name;
    std::string role_name;
    std::string current_schema = descriptor.schema_name;
    std::string client_app = descriptor.client_app;
    std::string resource_tag = descriptor.resource_tag;
    const std::string database_name = descriptor.database_name.empty()
        ? (db_ ? baseNameFromPath(db_->path()) : std::string())
        : descriptor.database_name;
    const std::string query_type = classifyQueryType(descriptor.sql);
    const std::string statement_tag = statementTagForSql(descriptor.sql);

    if (descriptor.connection != nullptr)
    {
        if (current_schema.empty())
        {
            current_schema = descriptor.connection->current_schema();
            if (current_schema.empty())
            {
                const auto& search_path = descriptor.connection->search_path();
                if (!search_path.empty())
                {
                    current_schema = search_path.front();
                }
            }
        }

        if (client_app.empty())
        {
            (void)descriptor.connection->getSessionVariable("APPLICATION_NAME", client_app);
            if (client_app.empty())
            {
                (void)descriptor.connection->getSessionVariable("SB$APPLICATION_NAME", client_app);
            }
        }

        if (resource_tag.empty())
        {
            (void)descriptor.connection->getSessionVariable("RESOURCE_TAG", resource_tag);
            if (resource_tag.empty())
            {
                (void)descriptor.connection->getSessionVariable("SB$RESOURCE_TAG", resource_tag);
            }
        }

        auto user_id = descriptor.connection->getCurrentUserId();
        if (!isZeroUuidLocal(user_id))
        {
            CatalogManager::UserInfo user_info;
            ErrorContext local_ctx;
            if (catalog->getUser(user_id, user_info, &local_ctx) == Status::OK)
            {
                user_name = user_info.username;
            }
        }

        auto role_id = descriptor.connection->getActiveRoleId();
        if (!isZeroUuidLocal(role_id))
        {
            CatalogManager::RoleInfo role_info;
            ErrorContext local_ctx;
            if (catalog->getRole(role_id, role_info, &local_ctx) == Status::OK)
            {
                role_name = role_info.role_name;
            }
        }
    }

    const std::string sql_upper = toUpperAscii(descriptor.sql);
    for (const auto& klass : classes)
    {
        if (!klass.is_valid || !klass.is_enabled)
        {
            continue;
        }

        const std::string rule = normalizeRuleValue(klass.match_text);
        if (rule.empty())
        {
            continue;
        }

        bool matched = false;
        switch (klass.match_kind)
        {
            case CatalogManager::WorkloadMatchKind::ROLE:
                matched = !role_name.empty() &&
                          toUpperAscii(role_name) == toUpperAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::USER:
                matched = !user_name.empty() &&
                          toUpperAscii(user_name) == toUpperAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::DATABASE:
                matched = !database_name.empty() &&
                          toUpperAscii(database_name) == toUpperAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::SCHEMA:
                matched = !current_schema.empty() &&
                          toUpperAscii(current_schema) == toUpperAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::CLIENT_APP:
                matched = !client_app.empty() &&
                          toUpperAscii(client_app) == toUpperAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::STATEMENT_TAG:
                matched = !statement_tag.empty() &&
                          toUpperAscii(statement_tag) == toUpperAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::QUERY_TYPE:
                matched = !query_type.empty() &&
                          toLowerAscii(query_type) == toLowerAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::REGEX:
                try
                {
                    matched = std::regex_search(descriptor.sql, std::regex(rule, std::regex::icase));
                }
                catch (const std::regex_error&)
                {
                    matched = false;
                }
                break;
            case CatalogManager::WorkloadMatchKind::RESOURCE_TAG:
                matched = !resource_tag.empty() &&
                          toUpperAscii(resource_tag) == toUpperAscii(rule);
                break;
            case CatalogManager::WorkloadMatchKind::CUSTOM:
                try
                {
                    matched = std::regex_search(descriptor.sql, std::regex(rule, std::regex::icase));
                }
                catch (const std::regex_error&)
                {
                    matched = sql_upper.find(toUpperAscii(rule)) != std::string::npos;
                }
                break;
        }

        if (matched)
        {
            state_out.matched = true;
            state_out.klass = klass;
            return Status::OK;
        }
    }

    return Status::OK;
}

auto WorkloadGovernance::resolveBinding(const MatchState& match,
                                        BindingState& state_out,
                                        ErrorContext* ctx) const -> Status
{
    state_out = BindingState{};
    auto* catalog = db_ ? db_->catalog_manager() : nullptr;
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Catalog manager not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<CatalogManager::AdmissionPolicyCatalogInfo> policies;
    Status status = catalog->listAdmissionPolicyCatalogEntries(policies, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    struct Candidate
    {
        CatalogManager::AdmissionPolicyCatalogInfo policy;
        CatalogManager::AdmissionBindingCatalogInfo binding;
    };
    std::vector<Candidate> candidates;

    for (const auto& policy : policies)
    {
        if (!policy.is_valid || !policy.is_enabled)
        {
            continue;
        }

        std::vector<CatalogManager::AdmissionBindingCatalogInfo> bindings;
        status = catalog->listAdmissionBindingCatalogEntries(policy.policy_id, bindings, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& binding : bindings)
        {
            if (!binding.is_valid || !binding.is_enabled)
            {
                continue;
            }
            if (binding.target_kind == CatalogManager::AdmissionTargetKind::CLUSTER)
            {
                candidates.push_back({policy, binding});
                continue;
            }
            if (binding.target_kind == CatalogManager::AdmissionTargetKind::WORKLOAD_CLASS &&
                match.matched &&
                binding.class_id == match.klass.class_id)
            {
                candidates.push_back({policy, binding});
            }
        }
    }

    if (candidates.empty())
    {
        return Status::OK;
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& lhs, const Candidate& rhs) {
                  if (lhs.binding.priority != rhs.binding.priority)
                  {
                      return lhs.binding.priority < rhs.binding.priority;
                  }
                  return lhs.policy.policy_name < rhs.policy.policy_name;
              });

    state_out.matched = true;
    state_out.policy = candidates.front().policy;
    state_out.binding = candidates.front().binding;
    return Status::OK;
}

auto WorkloadGovernance::countActiveSessionsLocked(const BindingState& binding,
                                                   const std::vector<uint32_t>& active_proc_ids,
                                                   uint32_t current_proc_id,
                                                   bool current_session_known) const -> uint32_t
{
    if (!binding.matched)
    {
        return 0;
    }

    std::unordered_set<uint32_t> active(active_proc_ids.begin(), active_proc_ids.end());
    uint32_t count = 0;
    for (uint32_t proc_id : active_proc_ids)
    {
        if (binding.binding.target_kind == CatalogManager::AdmissionTargetKind::CLUSTER)
        {
            ++count;
            continue;
        }

        auto it_policy = session_policy_map_.find(proc_id);
        if (it_policy != session_policy_map_.end() && it_policy->second == binding.policy.policy_id)
        {
            ++count;
            continue;
        }

        auto it_class = session_class_map_.find(proc_id);
        if (it_class != session_class_map_.end() && it_class->second == binding.binding.class_id)
        {
            ++count;
        }
    }

    if (!current_session_known &&
        current_proc_id != UINT32_MAX &&
        active.find(current_proc_id) == active.end())
    {
        ++count;
    }
    return count;
}

auto WorkloadGovernance::acquire(const QueryDescriptor& descriptor,
                                 AdmissionLease& lease_out,
                                 ErrorContext* ctx) -> AdmissionDecision
{
    lease_out.release();

    AdmissionDecision decision;
    if (db_ == nullptr || db_->catalog_manager() == nullptr)
    {
        decision.admitted = false;
        decision.status = Status::INTERNAL_ERROR;
        decision.code = "GOV_1500";
        decision.detail = "Workload governance requires an active catalog manager";
        SET_ERROR_CONTEXT(ctx, decision.status, decision.detail.c_str());
        return decision;
    }

    MatchState match;
    Status status = resolveWorkloadClass(descriptor, match, ctx);
    if (status != Status::OK)
    {
        decision.admitted = false;
        decision.status = status;
        decision.code = "GOV_1500";
        decision.detail = ctx && !ctx->message.empty()
            ? ctx->message
            : "Failed to resolve workload class";
        return decision;
    }

    if (match.matched)
    {
        decision.class_id = match.klass.class_id;
        decision.class_name = match.klass.class_name;
    }

    BindingState binding;
    status = resolveBinding(match, binding, ctx);
    if (status != Status::OK)
    {
        decision.admitted = false;
        decision.status = status;
        decision.code = "GOV_1500";
        decision.detail = ctx && !ctx->message.empty()
            ? ctx->message
            : "Failed to resolve admission binding";
        return decision;
    }

    if (!binding.matched)
    {
        return decision;
    }

    decision.policy_id = binding.policy.policy_id;
    decision.policy_name = binding.policy.policy_name;

    uint32_t proc_id = UINT32_MAX;
    if (descriptor.connection != nullptr)
    {
        proc_id = descriptor.connection->getProcId();
    }

    std::vector<uint32_t> active_proc_ids;
    for (const auto& snap : db_->snapshotConnectionSecurityStacks())
    {
        active_proc_ids.push_back(snap.proc_id);
    }
    std::sort(active_proc_ids.begin(), active_proc_ids.end());
    active_proc_ids.erase(std::unique(active_proc_ids.begin(), active_proc_ids.end()),
                          active_proc_ids.end());

    std::unique_lock<std::mutex> lock(mutex_);
    std::unordered_set<uint32_t> active_set(active_proc_ids.begin(), active_proc_ids.end());
    for (auto it = session_class_map_.begin(); it != session_class_map_.end();)
    {
        if (active_set.find(it->first) == active_set.end())
        {
            it = session_class_map_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    for (auto it = session_policy_map_.begin(); it != session_policy_map_.end();)
    {
        if (active_set.find(it->first) == active_set.end())
        {
            it = session_policy_map_.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (proc_id != UINT32_MAX && match.matched)
    {
        session_class_map_[proc_id] = match.klass.class_id;
    }

    const bool current_session_known =
        (proc_id != UINT32_MAX && session_policy_map_.find(proc_id) != session_policy_map_.end()) ||
        (proc_id != UINT32_MAX && session_class_map_.find(proc_id) != session_class_map_.end());

    if (binding.policy.max_concurrent_sessions > 0)
    {
        const uint32_t active_sessions = countActiveSessionsLocked(
            binding, active_proc_ids, proc_id, current_session_known);
        if (active_sessions > binding.policy.max_concurrent_sessions)
        {
            decision.admitted = false;
            decision.status = Status::TOO_MANY_CONNECTIONS;
            decision.code = "GOV_1501";
            decision.detail = "Admission rejected by max_concurrent_sessions";
            SET_ERROR_CONTEXT(ctx, decision.status, decision.detail.c_str());
            return decision;
        }
    }

    CounterState& counter = policy_counters_[binding.policy.policy_id];
    auto can_run_now = [&]() {
        return binding.policy.max_concurrent_queries == 0 ||
               counter.active_queries < binding.policy.max_concurrent_queries;
    };

    if (!can_run_now())
    {
        const auto reject_now = [&](Status reject_status,
                                    const char* code,
                                    const char* detail) -> AdmissionDecision {
            AdmissionDecision rejected = decision;
            rejected.admitted = false;
            rejected.status = reject_status;
            rejected.code = code;
            rejected.detail = detail;
            SET_ERROR_CONTEXT(ctx, reject_status, detail);
            return rejected;
        };

        if (binding.policy.reject_mode == CatalogManager::AdmissionRejectMode::REJECT)
        {
            return reject_now(Status::CONFIGURATION_LIMIT_EXCEEDED,
                              "GOV_1502",
                              "Admission rejected by max_concurrent_queries");
        }
        if (binding.policy.reject_mode == CatalogManager::AdmissionRejectMode::SHED_LOW_PRIORITY)
        {
            return reject_now(Status::CONFIGURATION_LIMIT_EXCEEDED,
                              "GOV_1503",
                              "Admission shed low-priority query at concurrency limit");
        }

        if (binding.policy.max_queue_depth > 0 &&
            counter.queued_queries >= binding.policy.max_queue_depth)
        {
            return reject_now(Status::CONFIGURATION_LIMIT_EXCEEDED,
                              "GOV_1504",
                              "Admission queue depth exceeded");
        }

        ++counter.queued_queries;
        const auto queue_guard = std::unique_ptr<void, std::function<void(void*)>>(
            reinterpret_cast<void*>(1),
            [&](void*) {
                CounterState& guarded = policy_counters_[binding.policy.policy_id];
                if (guarded.queued_queries > 0)
                {
                    --guarded.queued_queries;
                }
            });

        const auto deadline =
            std::chrono::steady_clock::now() +
            std::chrono::milliseconds(binding.policy.queue_timeout_ms);
        while (!can_run_now())
        {
            if (cv_.wait_until(lock, deadline) == std::cv_status::timeout)
            {
                return reject_now(Status::LOCK_TIMEOUT,
                                  "GOV_1505",
                                  "Admission queue wait timed out");
            }
        }
        decision.queued = true;
    }

    ++counter.active_queries;
    if (proc_id != UINT32_MAX)
    {
        session_policy_map_[proc_id] = binding.policy.policy_id;
    }

    lease_out.owner_ = this;
    lease_out.proc_id_ = proc_id;
    lease_out.class_id_ = match.matched ? match.klass.class_id : ID{};
    lease_out.policy_id_ = binding.policy.policy_id;
    lease_out.class_name_ = match.matched ? match.klass.class_name : std::string();
    lease_out.policy_name_ = binding.policy.policy_name;
    lease_out.active_ = true;
    return decision;
}

auto WorkloadGovernance::snapshotAdmissionStatus(std::vector<AdmissionStatusRow>& rows_out,
                                                 ErrorContext* ctx) const -> Status
{
    rows_out.clear();
    auto* catalog = db_ ? db_->catalog_manager() : nullptr;
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Catalog manager not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<CatalogManager::WorkloadClassCatalogInfo> classes;
    std::vector<CatalogManager::AdmissionPolicyCatalogInfo> policies;
    Status status = catalog->listWorkloadClassCatalogEntries(classes, ctx);
    if (status != Status::OK)
    {
        return status;
    }
    status = catalog->listAdmissionPolicyCatalogEntries(policies, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::vector<uint32_t> active_proc_ids;
    for (const auto& snap : db_->snapshotConnectionSecurityStacks())
    {
        active_proc_ids.push_back(snap.proc_id);
    }
    std::sort(active_proc_ids.begin(), active_proc_ids.end());
    active_proc_ids.erase(std::unique(active_proc_ids.begin(), active_proc_ids.end()),
                          active_proc_ids.end());

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& policy : policies)
    {
        if (!policy.is_valid)
        {
            continue;
        }

        std::vector<CatalogManager::AdmissionBindingCatalogInfo> bindings;
        status = catalog->listAdmissionBindingCatalogEntries(policy.policy_id, bindings, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& binding : bindings)
        {
            if (!binding.is_valid)
            {
                continue;
            }

            AdmissionStatusRow row;
            row.policy_name = policy.policy_name;
            row.reject_mode = rejectModeToString(policy.reject_mode);
            row.binding_priority = binding.priority;
            row.max_concurrent_sessions = policy.max_concurrent_sessions;
            row.max_concurrent_queries = policy.max_concurrent_queries;
            row.max_queue_depth = policy.max_queue_depth;
            row.queue_timeout_ms = policy.queue_timeout_ms;
            row.policy_enabled = policy.is_enabled;
            row.binding_enabled = binding.is_enabled;

            if (binding.target_kind == CatalogManager::AdmissionTargetKind::WORKLOAD_CLASS)
            {
                row.scope = "WORKLOAD_CLASS";
                for (const auto& klass : classes)
                {
                    if (klass.class_id == binding.class_id)
                    {
                        row.class_name = klass.class_name;
                        row.class_priority = klass.priority;
                        row.class_enabled = klass.is_enabled;
                        break;
                    }
                }
            }
            else
            {
                row.scope = "CLUSTER";
                row.class_enabled = true;
            }

            auto it_counter = policy_counters_.find(policy.policy_id);
            if (it_counter != policy_counters_.end())
            {
                row.active_queries = it_counter->second.active_queries;
                row.queued_queries = it_counter->second.queued_queries;
            }
            BindingState binding_state;
            binding_state.matched = true;
            binding_state.policy = policy;
            binding_state.binding = binding;
            row.active_sessions = countActiveSessionsLocked(binding_state,
                                                            active_proc_ids,
                                                            UINT32_MAX,
                                                            true);
            rows_out.push_back(std::move(row));
        }
    }

    std::sort(rows_out.begin(), rows_out.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.binding_priority != rhs.binding_priority)
                  {
                      return lhs.binding_priority < rhs.binding_priority;
                  }
                  if (lhs.policy_name != rhs.policy_name)
                  {
                      return lhs.policy_name < rhs.policy_name;
                  }
                  return lhs.class_name < rhs.class_name;
              });
    return Status::OK;
}

auto WorkloadGovernance::snapshotRoutingPlan(std::vector<RoutingPlanRow>& rows_out,
                                             ErrorContext* ctx) const -> Status
{
    rows_out.clear();
    auto* catalog = db_ ? db_->catalog_manager() : nullptr;
    if (catalog == nullptr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Catalog manager not available");
        return Status::INTERNAL_ERROR;
    }

    std::vector<CatalogManager::WorkloadClassCatalogInfo> classes;
    Status status = catalog->listWorkloadClassCatalogEntries(classes, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    for (const auto& klass : classes)
    {
        if (!klass.is_valid)
        {
            continue;
        }

        std::vector<CatalogManager::WorkloadRouteCatalogInfo> routes;
        status = catalog->listWorkloadRouteCatalogEntries(klass.class_id, routes, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& route : routes)
        {
            if (!route.is_valid)
            {
                continue;
            }

            RoutingPlanRow row;
            row.class_name = klass.class_name;
            row.class_priority = klass.priority;
            row.route_name = route.route_name;
            row.target_kind = routeTargetKindToString(route.target_kind);
            row.target_label = route.target_label;
            row.role = route.has_role ? roleToString(route.role) : std::string();
            row.service_type = route.has_service_type
                ? serviceTypeToString(route.service_type)
                : std::string();
            row.transport = transportToString(route.transport);
            row.route_weight = route.route_weight;
            row.class_enabled = klass.is_enabled;
            row.route_enabled = route.is_enabled;

            if (!isZeroUuidLocal(route.fallback_route_id))
            {
                CatalogManager::WorkloadRouteCatalogInfo fallback;
                ErrorContext fallback_ctx;
                if (catalog->getWorkloadRouteCatalogEntry(route.fallback_route_id,
                                                          fallback,
                                                          &fallback_ctx) == Status::OK)
                {
                    row.fallback_route_name = fallback.route_name;
                }
            }
            rows_out.push_back(std::move(row));
        }
    }

    std::sort(rows_out.begin(), rows_out.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.class_priority != rhs.class_priority)
                  {
                      return lhs.class_priority > rhs.class_priority;
                  }
                  if (lhs.class_name != rhs.class_name)
                  {
                      return lhs.class_name < rhs.class_name;
                  }
                  if (lhs.route_weight != rhs.route_weight)
                  {
                      return lhs.route_weight > rhs.route_weight;
                  }
                  return lhs.route_name < rhs.route_name;
              });
    return Status::OK;
}

void WorkloadGovernance::releaseLease(uint32_t proc_id,
                                      const ID& policy_id,
                                      const ID& class_id)
{
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = policy_counters_.find(policy_id);
    if (it != policy_counters_.end())
    {
        if (it->second.active_queries > 0)
        {
            --it->second.active_queries;
        }
        if (it->second.active_queries == 0 && it->second.queued_queries == 0)
        {
            policy_counters_.erase(it);
        }
    }
    if (proc_id != UINT32_MAX)
    {
        if (!isZeroUuidLocal(class_id))
        {
            session_class_map_[proc_id] = class_id;
        }
        session_policy_map_[proc_id] = policy_id;
    }
    cv_.notify_one();
}

} // namespace scratchbird::core
