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
#include <functional>
#include <memory>
#include <regex>
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

} // namespace

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
