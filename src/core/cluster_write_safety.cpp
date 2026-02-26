/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/cluster_write_safety.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace scratchbird::core
{

    namespace
    {

        auto isZeroId(const ID& value) -> bool
        {
            for (uint8_t b : value.bytes)
            {
                if (b != 0)
                {
                    return false;
                }
            }
            return true;
        }

        constexpr uint64_t kFnvOffset = 1469598103934665603ull;
        constexpr uint64_t kFnvPrime = 1099511628211ull;

        auto fnv1a64(std::string_view value) -> uint64_t
        {
            uint64_t hash = kFnvOffset;
            for (unsigned char c : value)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= kFnvPrime;
            }
            return hash;
        }

        auto splitTsvLine(const std::string& line,
                          std::string& first,
                          std::string& second,
                          std::string& third,
                          std::string& fourth) -> bool
        {
            const size_t p1 = line.find('\t');
            if (p1 == std::string::npos)
            {
                return false;
            }
            const size_t p2 = line.find('\t', p1 + 1);
            if (p2 == std::string::npos)
            {
                return false;
            }
            const size_t p3 = line.find('\t', p2 + 1);
            if (p3 == std::string::npos)
            {
                return false;
            }

            first = line.substr(0, p1);
            second = line.substr(p1 + 1, p2 - (p1 + 1));
            third = line.substr(p2 + 1, p3 - (p2 + 1));
            fourth = line.substr(p3 + 1);
            return true;
        }

    } // namespace

    auto toString(WriteAdmissionReason reason) -> const char*
    {
        switch (reason)
        {
            case WriteAdmissionReason::NONE:
                return "none";
            case WriteAdmissionReason::SHARD_NOT_REGISTERED:
                return "shard_not_registered";
            case WriteAdmissionReason::SHARD_WRITES_DISABLED:
                return "shard_writes_disabled";
            case WriteAdmissionReason::NOT_CURRENT_LEADER:
                return "not_current_leader";
            case WriteAdmissionReason::FENCING_SHARD_MISMATCH:
                return "fencing_shard_mismatch";
            case WriteAdmissionReason::STALE_FENCING_TOKEN:
                return "stale_fencing_token";
            case WriteAdmissionReason::ROUTING_EPOCH_MISMATCH:
                return "routing_epoch_mismatch";
        }
        return "unknown";
    }

    auto ClusterWriteSafetyController::upsertShardLeaderState(const ShardLeaderState& state, ErrorContext* ctx)
        -> Status
    {
        if (isZeroId(state.shard_id) || isZeroId(state.leader_node_id) || state.leader_term == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid shard leader state");
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        shard_states_[state.shard_id] = state;
        return Status::OK;
    }

    auto ClusterWriteSafetyController::validateWrite(const WriteAdmissionRequest& request) const
        -> WriteAdmissionResult
    {
        WriteAdmissionResult result{};
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = shard_states_.find(request.shard_id);
        if (it == shard_states_.end())
        {
            result.allowed = false;
            result.status = Status::NOT_FOUND;
            result.reason = WriteAdmissionReason::SHARD_NOT_REGISTERED;
            return result;
        }

        const ShardLeaderState& current = it->second;
        result.expected_leader_term = current.leader_term;
        result.expected_routing_epoch = current.routing_epoch;

        if (!current.write_enabled)
        {
            result.allowed = false;
            result.status = Status::PERMISSION_DENIED;
            result.reason = WriteAdmissionReason::SHARD_WRITES_DISABLED;
            return result;
        }

        if (request.node_id != current.leader_node_id)
        {
            result.allowed = false;
            result.status = Status::PERMISSION_DENIED;
            result.reason = WriteAdmissionReason::NOT_CURRENT_LEADER;
            return result;
        }

        if (request.fencing_token.shard_id != request.shard_id)
        {
            result.allowed = false;
            result.status = Status::INVALID_ARGUMENT;
            result.reason = WriteAdmissionReason::FENCING_SHARD_MISMATCH;
            return result;
        }

        if (request.fencing_token.leader_term != current.leader_term)
        {
            result.allowed = false;
            result.status = Status::INVALID_TRANSACTION_STATE;
            result.reason = WriteAdmissionReason::STALE_FENCING_TOKEN;
            return result;
        }

        if (request.has_routing_epoch && request.routing_epoch != current.routing_epoch)
        {
            result.allowed = false;
            result.status = Status::INVALID_TRANSACTION_STATE;
            result.reason = WriteAdmissionReason::ROUTING_EPOCH_MISMATCH;
            return result;
        }

        result.allowed = true;
        result.status = Status::OK;
        result.reason = WriteAdmissionReason::NONE;
        return result;
    }

    auto ClusterWriteSafetyController::getShardLeaderState(const ID& shard_id, ShardLeaderState& state_out) const
        -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = shard_states_.find(shard_id);
        if (it == shard_states_.end())
        {
            return false;
        }
        state_out = it->second;
        return true;
    }

    auto toString(RoutingDecisionReason reason) -> const char*
    {
        switch (reason)
        {
            case RoutingDecisionReason::NONE:
                return "none";
            case RoutingDecisionReason::PLAN_NOT_FOUND:
                return "plan_not_found";
            case RoutingDecisionReason::PLAN_HAS_NO_TARGETS:
                return "plan_has_no_targets";
            case RoutingDecisionReason::STALE_ROUTING_EPOCH:
                return "stale_routing_epoch";
        }
        return "unknown";
    }

    auto DeterministicShardRouter::upsertPlan(const RoutingPlan& plan, ErrorContext* ctx) -> Status
    {
        if (isZeroId(plan.table_id) || plan.targets.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid routing plan");
            return Status::INVALID_ARGUMENT;
        }

        for (const RoutingTarget& target : plan.targets)
        {
            if (isZeroId(target.shard_id) || isZeroId(target.leader_node_id) || target.route_weight == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid routing target");
                return Status::INVALID_ARGUMENT;
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        plans_[plan.table_id] = plan;
        return Status::OK;
    }

    auto DeterministicShardRouter::route(const RoutingRequest& request) const -> RoutingDecision
    {
        RoutingDecision decision{};
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = plans_.find(request.table_id);
        if (it == plans_.end())
        {
            decision.routed = false;
            decision.status = Status::NOT_FOUND;
            decision.reason = RoutingDecisionReason::PLAN_NOT_FOUND;
            return decision;
        }

        const RoutingPlan& plan = it->second;
        decision.routing_epoch = plan.routing_epoch;

        if (plan.targets.empty())
        {
            decision.routed = false;
            decision.status = Status::NOT_FOUND;
            decision.reason = RoutingDecisionReason::PLAN_HAS_NO_TARGETS;
            return decision;
        }

        if (request.has_expected_routing_epoch &&
            request.expected_routing_epoch != plan.routing_epoch)
        {
            decision.routed = false;
            decision.status = Status::INVALID_TRANSACTION_STATE;
            decision.reason = RoutingDecisionReason::STALE_ROUTING_EPOCH;
            return decision;
        }

        uint64_t total_weight = 0;
        for (const RoutingTarget& target : plan.targets)
        {
            total_weight += target.route_weight;
        }
        if (total_weight == 0)
        {
            decision.routed = false;
            decision.status = Status::INVALID_ARGUMENT;
            decision.reason = RoutingDecisionReason::PLAN_HAS_NO_TARGETS;
            return decision;
        }

        std::string hash_input;
        hash_input.reserve(request.shard_key.size() + 1 + 36);
        hash_input.append(request.shard_key);
        hash_input.push_back('|');
        hash_input.append(request.table_id.toString());

        uint64_t bucket = deterministicHash(hash_input) % total_weight;
        uint64_t cursor = 0;
        for (const RoutingTarget& target : plan.targets)
        {
            cursor += target.route_weight;
            if (bucket < cursor)
            {
                decision.routed = true;
                decision.status = Status::OK;
                decision.reason = RoutingDecisionReason::NONE;
                decision.target = target;
                return decision;
            }
        }

        decision.routed = false;
        decision.status = Status::INTERNAL_ERROR;
        decision.reason = RoutingDecisionReason::PLAN_HAS_NO_TARGETS;
        return decision;
    }

    auto DeterministicShardRouter::getPlan(const ID& table_id, RoutingPlan& plan_out) const -> bool
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = plans_.find(table_id);
        if (it == plans_.end())
        {
            return false;
        }
        plan_out = it->second;
        return true;
    }

    auto DeterministicShardRouter::deterministicHash(std::string_view value) -> uint64_t
    {
        return fnv1a64(value);
    }

    auto toString(SessionEpochReason reason) -> const char*
    {
        switch (reason)
        {
            case SessionEpochReason::NONE:
                return "none";
            case SessionEpochReason::CLUSTER_CONFIG_EPOCH_MISMATCH:
                return "cluster_config_epoch_mismatch";
            case SessionEpochReason::SCHEMA_EPOCH_MISMATCH:
                return "schema_epoch_mismatch";
            case SessionEpochReason::SECURITY_EPOCH_MISMATCH:
                return "security_epoch_mismatch";
        }
        return "unknown";
    }

    auto validateSessionEpochPins(const SessionEpochPins& pinned,
                                  const SessionEpochPins& current,
                                  SessionEpochMismatchPolicy policy) -> SessionEpochValidationResult
    {
        SessionEpochValidationResult result{};
        result.valid = true;
        result.status = Status::OK;
        result.reason = SessionEpochReason::NONE;
        result.action = SessionEpochAction::NONE;

        auto set_mismatch = [&](SessionEpochReason reason) {
            result.valid = false;
            result.reason = reason;
            if (policy == SessionEpochMismatchPolicy::REPLAN)
            {
                result.action = SessionEpochAction::REPLAN;
                result.status = Status::OK;
            }
            else
            {
                result.action = SessionEpochAction::REJECT;
                result.status = Status::INVALID_TRANSACTION_STATE;
            }
        };

        if (pinned.cluster_config_epoch != current.cluster_config_epoch)
        {
            set_mismatch(SessionEpochReason::CLUSTER_CONFIG_EPOCH_MISMATCH);
            return result;
        }
        if (pinned.schema_epoch != current.schema_epoch)
        {
            set_mismatch(SessionEpochReason::SCHEMA_EPOCH_MISMATCH);
            return result;
        }
        if (pinned.security_epoch != current.security_epoch)
        {
            set_mismatch(SessionEpochReason::SECURITY_EPOCH_MISMATCH);
            return result;
        }

        return result;
    }

    auto toString(MultiShardGuardReason reason) -> const char*
    {
        switch (reason)
        {
            case MultiShardGuardReason::NONE:
                return "none";
            case MultiShardGuardReason::MULTI_SHARD_WRITE_REQUIRES_OVERRIDE:
                return "multi_shard_write_requires_override";
            case MultiShardGuardReason::MULTI_SHARD_WRITE_NOT_ALLOWED:
                return "multi_shard_write_not_allowed";
        }
        return "unknown";
    }

    auto evaluateMultiShardWrite(const std::vector<ID>& write_shards,
                                 const MultiShardGuardPolicy& policy,
                                 bool explicit_override) -> MultiShardGuardResult
    {
        std::unordered_set<ID, IDHash> unique_shards;
        unique_shards.reserve(write_shards.size());
        for (const ID& shard_id : write_shards)
        {
            if (!isZeroId(shard_id))
            {
                unique_shards.insert(shard_id);
            }
        }

        MultiShardGuardResult result{};
        result.unique_shard_count = unique_shards.size();

        if (result.unique_shard_count <= 1)
        {
            result.allowed = true;
            result.status = Status::OK;
            result.reason = MultiShardGuardReason::NONE;
            return result;
        }

        if (!policy.allow_cross_shard)
        {
            result.allowed = false;
            result.status = Status::PERMISSION_DENIED;
            result.reason = MultiShardGuardReason::MULTI_SHARD_WRITE_NOT_ALLOWED;
            return result;
        }

        if (policy.require_explicit_override && !explicit_override)
        {
            result.allowed = false;
            result.status = Status::PERMISSION_DENIED;
            result.reason = MultiShardGuardReason::MULTI_SHARD_WRITE_REQUIRES_OVERRIDE;
            return result;
        }

        result.allowed = true;
        result.status = Status::OK;
        result.reason = MultiShardGuardReason::NONE;
        return result;
    }

    auto toString(TxnOrderingReason reason) -> const char*
    {
        switch (reason)
        {
            case TxnOrderingReason::NONE:
                return "none";
            case TxnOrderingReason::INVALID_GTXID:
                return "invalid_gtxid";
            case TxnOrderingReason::STALE_OR_DUPLICATE:
                return "stale_or_duplicate";
            case TxnOrderingReason::OUT_OF_ORDER:
                return "out_of_order";
        }
        return "unknown";
    }

    auto ShardTxnOrderBook::allocateNext(const ID& shard_id, GTXID& gtxid_out, ErrorContext* ctx) -> Status
    {
        if (isZeroId(shard_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Shard ID required");
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        ShardTxnState& state = shard_states_[shard_id];
        state.last_allocated += 1;

        gtxid_out.shard_id = shard_id;
        gtxid_out.local_txn_id = state.last_allocated;
        return Status::OK;
    }

    auto ShardTxnOrderBook::recordCommitted(const GTXID& gtxid) -> TxnOrderingResult
    {
        TxnOrderingResult result{};
        if (isZeroId(gtxid.shard_id) || gtxid.local_txn_id == 0)
        {
            result.accepted = false;
            result.status = Status::INVALID_ARGUMENT;
            result.reason = TxnOrderingReason::INVALID_GTXID;
            return result;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        ShardTxnState& state = shard_states_[gtxid.shard_id];
        result.expected_next_local_txn_id = state.last_committed + 1;

        if (gtxid.local_txn_id <= state.last_committed)
        {
            result.accepted = false;
            result.status = Status::INVALID_TRANSACTION_STATE;
            result.reason = TxnOrderingReason::STALE_OR_DUPLICATE;
            return result;
        }

        state.last_committed = gtxid.local_txn_id;
        if (state.last_allocated < gtxid.local_txn_id)
        {
            state.last_allocated = gtxid.local_txn_id;
        }

        result.accepted = true;
        result.status = Status::OK;
        result.reason = TxnOrderingReason::NONE;
        result.expected_next_local_txn_id = state.last_committed + 1;
        return result;
    }

    auto ShardTxnOrderBook::recordFollowerApply(const GTXID& gtxid) -> TxnOrderingResult
    {
        TxnOrderingResult result{};
        if (isZeroId(gtxid.shard_id) || gtxid.local_txn_id == 0)
        {
            result.accepted = false;
            result.status = Status::INVALID_ARGUMENT;
            result.reason = TxnOrderingReason::INVALID_GTXID;
            return result;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        ShardTxnState& state = shard_states_[gtxid.shard_id];
        const uint64_t expected = state.last_applied + 1;
        result.expected_next_local_txn_id = expected;

        if (gtxid.local_txn_id != expected)
        {
            result.accepted = false;
            result.status = Status::INVALID_TRANSACTION_STATE;
            result.reason = (gtxid.local_txn_id < expected)
                ? TxnOrderingReason::STALE_OR_DUPLICATE
                : TxnOrderingReason::OUT_OF_ORDER;
            return result;
        }

        state.last_applied = gtxid.local_txn_id;
        if (state.last_committed < gtxid.local_txn_id)
        {
            state.last_committed = gtxid.local_txn_id;
        }
        if (state.last_allocated < gtxid.local_txn_id)
        {
            state.last_allocated = gtxid.local_txn_id;
        }

        result.accepted = true;
        result.status = Status::OK;
        result.reason = TxnOrderingReason::NONE;
        result.expected_next_local_txn_id = state.last_applied + 1;
        return result;
    }

    auto ShardTxnOrderBook::lastCommitted(const ID& shard_id) const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = shard_states_.find(shard_id);
        if (it == shard_states_.end())
        {
            return 0;
        }
        return it->second.last_committed;
    }

    auto ShardTxnOrderBook::lastApplied(const ID& shard_id) const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = shard_states_.find(shard_id);
        if (it == shard_states_.end())
        {
            return 0;
        }
        return it->second.last_applied;
    }

    auto toString(ShardCommitLogAppendReason reason) -> const char*
    {
        switch (reason)
        {
            case ShardCommitLogAppendReason::NONE:
                return "none";
            case ShardCommitLogAppendReason::INVALID_ENTRY:
                return "invalid_entry";
            case ShardCommitLogAppendReason::OUT_OF_ORDER_LOCAL_TXN_ID:
                return "out_of_order_local_txn_id";
            case ShardCommitLogAppendReason::DURABILITY_WRITE_FAILED:
                return "durability_write_failed";
        }
        return "unknown";
    }

    ShardCommitLog::ShardCommitLog(std::string root_directory)
        : root_directory_(std::move(root_directory))
    {
    }

    auto ShardCommitLog::append(const ShardCommitLogEntry& entry,
                                ShardCommitLogAppendResult* result_out,
                                ErrorContext* ctx) -> Status
    {
        ShardCommitLogAppendResult local_result{};
        local_result.appended = false;
        local_result.status = Status::INVALID_TRANSACTION_STATE;
        local_result.reason = ShardCommitLogAppendReason::INVALID_ENTRY;

        if (isZeroId(entry.gtxid.shard_id) || entry.gtxid.local_txn_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid shard commit log entry");
            local_result.status = Status::INVALID_ARGUMENT;
            if (result_out != nullptr)
            {
                *result_out = local_result;
            }
            return Status::INVALID_ARGUMENT;
        }

        if (entry.payload_format.find('\t') != std::string::npos ||
            entry.payload_format.find('\n') != std::string::npos)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid payload format");
            local_result.status = Status::INVALID_ARGUMENT;
            if (result_out != nullptr)
            {
                *result_out = local_result;
            }
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        std::error_code ec;
        std::filesystem::create_directories(root_directory_, ec);
        if (ec)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to create shard commit log directory");
            local_result.status = Status::IO_ERROR;
            local_result.reason = ShardCommitLogAppendReason::DURABILITY_WRITE_FAILED;
            if (result_out != nullptr)
            {
                *result_out = local_result;
            }
            return Status::IO_ERROR;
        }

        const uint64_t expected_next = last_local_txn_id_by_shard_[entry.gtxid.shard_id] + 1;
        local_result.expected_next_local_txn_id = expected_next;
        local_result.durable_path = durablePathForShard(entry.gtxid.shard_id);
        if (entry.gtxid.local_txn_id != expected_next)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_TRANSACTION_STATE, "Out of order shard local_txn_id");
            local_result.status = Status::INVALID_TRANSACTION_STATE;
            local_result.reason = ShardCommitLogAppendReason::OUT_OF_ORDER_LOCAL_TXN_ID;
            if (result_out != nullptr)
            {
                *result_out = local_result;
            }
            return Status::INVALID_TRANSACTION_STATE;
        }

        std::ostringstream line;
        line << entry.gtxid.local_txn_id << '\t'
             << entry.commit_timestamp_ns << '\t'
             << entry.payload_format << '\t'
             << hexEncode(entry.payload) << '\n';

        Status status = appendDurableLine(local_result.durable_path, line.str(), ctx);
        if (status != Status::OK)
        {
            local_result.status = status;
            local_result.reason = ShardCommitLogAppendReason::DURABILITY_WRITE_FAILED;
            if (result_out != nullptr)
            {
                *result_out = local_result;
            }
            return status;
        }

        last_local_txn_id_by_shard_[entry.gtxid.shard_id] = entry.gtxid.local_txn_id;
        local_result.appended = true;
        local_result.status = Status::OK;
        local_result.reason = ShardCommitLogAppendReason::NONE;
        local_result.expected_next_local_txn_id = entry.gtxid.local_txn_id + 1;

        if (result_out != nullptr)
        {
            *result_out = local_result;
        }
        return Status::OK;
    }

    auto ShardCommitLog::readEntries(const ID& shard_id,
                                     std::vector<ShardCommitLogEntry>& entries_out,
                                     ErrorContext* ctx) const -> Status
    {
        entries_out.clear();
        if (isZeroId(shard_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid shard ID");
            return Status::INVALID_ARGUMENT;
        }

        const std::string path = durablePathForShard(shard_id);
        std::ifstream in(path);
        if (!in.is_open())
        {
            return Status::NOT_FOUND;
        }

        std::string line;
        uint64_t expected = 1;
        while (std::getline(in, line))
        {
            if (line.empty())
            {
                continue;
            }

            std::string local_txn_id_s;
            std::string commit_ts_s;
            std::string payload_format;
            std::string payload_hex;
            if (!splitTsvLine(line, local_txn_id_s, commit_ts_s, payload_format, payload_hex))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Malformed commit log line");
                return Status::DATA_CORRUPTED;
            }

            uint64_t local_txn_id = 0;
            uint64_t commit_ts = 0;
            try
            {
                local_txn_id = std::stoull(local_txn_id_s);
                commit_ts = std::stoull(commit_ts_s);
            }
            catch (const std::exception&)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Malformed commit log numeric fields");
                return Status::DATA_CORRUPTED;
            }

            if (local_txn_id != expected)
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Out of order commit log sequence");
                return Status::DATA_CORRUPTED;
            }

            std::string payload;
            if (!hexDecode(payload_hex, payload))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Malformed commit log payload encoding");
                return Status::DATA_CORRUPTED;
            }

            ShardCommitLogEntry entry{};
            entry.gtxid.shard_id = shard_id;
            entry.gtxid.local_txn_id = local_txn_id;
            entry.commit_timestamp_ns = commit_ts;
            entry.payload_format = payload_format;
            entry.payload = std::move(payload);
            entries_out.push_back(std::move(entry));
            expected += 1;
        }

        return Status::OK;
    }

    auto ShardCommitLog::durablePathForShard(const ID& shard_id) const -> std::string
    {
        std::filesystem::path base(root_directory_);
        base /= shard_id.toString() + ".scl";
        return base.string();
    }

    auto ShardCommitLog::hexEncode(const std::string& value) -> std::string
    {
        static constexpr char kHex[] = "0123456789abcdef";
        std::string out;
        out.reserve(value.size() * 2);
        for (unsigned char c : value)
        {
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0f]);
        }
        return out;
    }

    auto ShardCommitLog::hexDecode(const std::string& value, std::string& out) -> bool
    {
        if ((value.size() % 2) != 0)
        {
            return false;
        }

        auto decode_nibble = [](char c, uint8_t& nibble) -> bool {
            if (c >= '0' && c <= '9')
            {
                nibble = static_cast<uint8_t>(c - '0');
                return true;
            }
            if (c >= 'a' && c <= 'f')
            {
                nibble = static_cast<uint8_t>(10 + (c - 'a'));
                return true;
            }
            if (c >= 'A' && c <= 'F')
            {
                nibble = static_cast<uint8_t>(10 + (c - 'A'));
                return true;
            }
            return false;
        };

        out.clear();
        out.reserve(value.size() / 2);
        for (size_t i = 0; i < value.size(); i += 2)
        {
            uint8_t hi = 0;
            uint8_t lo = 0;
            if (!decode_nibble(value[i], hi) || !decode_nibble(value[i + 1], lo))
            {
                out.clear();
                return false;
            }
            out.push_back(static_cast<char>((hi << 4) | lo));
        }
        return true;
    }

    auto ShardCommitLog::appendDurableLine(const std::string& path,
                                           const std::string& line,
                                           ErrorContext* ctx) const -> Status
    {
        FILE* fp = std::fopen(path.c_str(), "ab");
        if (fp == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to open shard commit log file");
            return Status::IO_ERROR;
        }

        const size_t written = std::fwrite(line.data(), 1, line.size(), fp);
        if (written != line.size())
        {
            std::fclose(fp);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to write shard commit log entry");
            return Status::IO_ERROR;
        }

        if (std::fflush(fp) != 0)
        {
            std::fclose(fp);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to flush shard commit log file");
            return Status::IO_ERROR;
        }

#if defined(_WIN32)
        if (_commit(_fileno(fp)) != 0)
#else
        if (fsync(fileno(fp)) != 0)
#endif
        {
            std::fclose(fp);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to fsync shard commit log file");
            return Status::IO_ERROR;
        }

        if (std::fclose(fp) != 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to close shard commit log file");
            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    auto toString(FollowerApplyReason reason) -> const char*
    {
        switch (reason)
        {
            case FollowerApplyReason::NONE:
                return "none";
            case FollowerApplyReason::LOG_ENTRY_NOT_FOUND:
                return "log_entry_not_found";
            case FollowerApplyReason::PAYLOAD_MISMATCH:
                return "payload_mismatch";
            case FollowerApplyReason::OUT_OF_ORDER:
                return "out_of_order";
            case FollowerApplyReason::ALREADY_APPLIED:
                return "already_applied";
        }
        return "unknown";
    }

    FollowerApplyPipeline::FollowerApplyPipeline(ShardCommitLog* commit_log)
        : commit_log_(commit_log)
    {
    }

    auto FollowerApplyPipeline::apply(const ID& shard_id,
                                      uint64_t local_txn_id,
                                      const std::string& payload,
                                      FollowerApplyResult* result_out,
                                      ErrorContext* ctx) -> Status
    {
        FollowerApplyResult result{};
        result.status = Status::INVALID_TRANSACTION_STATE;
        result.reason = FollowerApplyReason::OUT_OF_ORDER;

        if (commit_log_ == nullptr || isZeroId(shard_id) || local_txn_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid follower apply request");
            result.status = Status::INVALID_ARGUMENT;
            if (result_out != nullptr)
            {
                *result_out = result;
            }
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        const uint64_t last_applied = last_applied_by_shard_[shard_id];
        const uint64_t expected_next = last_applied + 1;
        result.expected_next_local_txn_id = expected_next;
        result.replication_watermark = last_applied;

        if (local_txn_id <= last_applied)
        {
            result.applied = false;
            result.replayed = true;
            result.status = Status::OK;
            result.reason = FollowerApplyReason::ALREADY_APPLIED;
            if (result_out != nullptr)
            {
                *result_out = result;
            }
            return Status::OK;
        }

        if (local_txn_id != expected_next)
        {
            result.applied = false;
            result.replayed = false;
            result.status = Status::INVALID_TRANSACTION_STATE;
            result.reason = FollowerApplyReason::OUT_OF_ORDER;
            if (result_out != nullptr)
            {
                *result_out = result;
            }
            return Status::INVALID_TRANSACTION_STATE;
        }

        std::vector<ShardCommitLogEntry> entries;
        Status read_status = commit_log_->readEntries(shard_id, entries, ctx);
        if (read_status != Status::OK)
        {
            result.status = read_status;
            result.reason = FollowerApplyReason::LOG_ENTRY_NOT_FOUND;
            if (result_out != nullptr)
            {
                *result_out = result;
            }
            return read_status;
        }

        if (local_txn_id == 0 || local_txn_id > entries.size())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Commit log entry not found");
            result.status = Status::NOT_FOUND;
            result.reason = FollowerApplyReason::LOG_ENTRY_NOT_FOUND;
            if (result_out != nullptr)
            {
                *result_out = result;
            }
            return Status::NOT_FOUND;
        }

        const ShardCommitLogEntry& expected_entry = entries[local_txn_id - 1];
        if (expected_entry.gtxid.local_txn_id != local_txn_id || expected_entry.payload != payload)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Follower payload mismatch");
            result.status = Status::DATA_CORRUPTED;
            result.reason = FollowerApplyReason::PAYLOAD_MISMATCH;
            if (result_out != nullptr)
            {
                *result_out = result;
            }
            return Status::DATA_CORRUPTED;
        }

        last_applied_by_shard_[shard_id] = local_txn_id;
        result.applied = true;
        result.replayed = false;
        result.status = Status::OK;
        result.reason = FollowerApplyReason::NONE;
        result.expected_next_local_txn_id = local_txn_id + 1;
        result.replication_watermark = local_txn_id;
        if (result_out != nullptr)
        {
            *result_out = result;
        }
        return Status::OK;
    }

    auto FollowerApplyPipeline::replicationWatermark(const ID& shard_id) const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = last_applied_by_shard_.find(shard_id);
        if (it == last_applied_by_shard_.end())
        {
            return 0;
        }
        return it->second;
    }

    auto SnapshotRegistry::registerOrUpdate(const SnapshotRegistryEntry& entry, ErrorContext* ctx) -> Status
    {
        if (isZeroId(entry.session_id) || isZeroId(entry.shard_id) || entry.snapshot_boundary == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid snapshot registry entry");
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        snapshots_by_shard_[entry.shard_id][entry.session_id] = entry;
        return Status::OK;
    }

    auto SnapshotRegistry::remove(const ID& session_id, const ID& shard_id, ErrorContext* ctx) -> Status
    {
        if (isZeroId(session_id) || isZeroId(shard_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid snapshot registry key");
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto shard_it = snapshots_by_shard_.find(shard_id);
        if (shard_it == snapshots_by_shard_.end())
        {
            return Status::NOT_FOUND;
        }
        auto& per_session = shard_it->second;
        auto session_it = per_session.find(session_id);
        if (session_it == per_session.end())
        {
            return Status::NOT_FOUND;
        }

        per_session.erase(session_it);
        if (per_session.empty())
        {
            snapshots_by_shard_.erase(shard_it);
        }
        return Status::OK;
    }

    auto SnapshotRegistry::oldestSnapshotBoundary(const ID& shard_id) const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto shard_it = snapshots_by_shard_.find(shard_id);
        if (shard_it == snapshots_by_shard_.end() || shard_it->second.empty())
        {
            return 0;
        }

        uint64_t oldest = std::numeric_limits<uint64_t>::max();
        for (const auto& pair : shard_it->second)
        {
            const SnapshotRegistryEntry& entry = pair.second;
            if (entry.snapshot_boundary < oldest)
            {
                oldest = entry.snapshot_boundary;
            }
        }
        return oldest == std::numeric_limits<uint64_t>::max() ? 0 : oldest;
    }

    auto SnapshotRegistry::listByShard(const ID& shard_id,
                                       std::vector<SnapshotRegistryEntry>& entries_out,
                                       ErrorContext* ctx) const -> Status
    {
        if (isZeroId(shard_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid shard ID");
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        entries_out.clear();
        auto shard_it = snapshots_by_shard_.find(shard_id);
        if (shard_it == snapshots_by_shard_.end())
        {
            return Status::NOT_FOUND;
        }

        entries_out.reserve(shard_it->second.size());
        for (const auto& pair : shard_it->second)
        {
            entries_out.push_back(pair.second);
        }
        return Status::OK;
    }

    auto CommittedWatermarkPublisher::publishCommitted(const GTXID& gtxid, ErrorContext* ctx) -> Status
    {
        if (isZeroId(gtxid.shard_id) || gtxid.local_txn_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid GTXID");
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t& cwm = cwm_by_shard_[gtxid.shard_id];
        if (gtxid.local_txn_id < cwm)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_TRANSACTION_STATE, "CWM cannot move backwards");
            return Status::INVALID_TRANSACTION_STATE;
        }
        cwm = gtxid.local_txn_id;
        return Status::OK;
    }

    auto CommittedWatermarkPublisher::watermark(const ID& shard_id) const -> uint64_t
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cwm_by_shard_.find(shard_id);
        if (it == cwm_by_shard_.end())
        {
            return 0;
        }
        return it->second;
    }

    auto CommittedWatermarkPublisher::snapshotVector(
        const std::vector<ID>& shard_ids,
        std::unordered_map<ID, uint64_t, IDHash>& out) const -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        out.clear();
        out.reserve(shard_ids.size());
        for (const ID& shard_id : shard_ids)
        {
            auto it = cwm_by_shard_.find(shard_id);
            out[shard_id] = (it == cwm_by_shard_.end()) ? 0 : it->second;
        }
        return Status::OK;
    }

    GcSafeHorizonCalculator::GcSafeHorizonCalculator(
        const SnapshotRegistry* snapshot_registry,
        const FollowerApplyPipeline* follower_apply_pipeline)
        : snapshot_registry_(snapshot_registry), follower_apply_pipeline_(follower_apply_pipeline)
    {
    }

    auto GcSafeHorizonCalculator::evaluate(const ID& shard_id,
                                           GcSafeHorizonEvaluation& evaluation_out,
                                           ErrorContext* ctx) const -> Status
    {
        if (snapshot_registry_ == nullptr || follower_apply_pipeline_ == nullptr || isZeroId(shard_id))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid GC safe horizon calculator request");
            return Status::INVALID_ARGUMENT;
        }

        evaluation_out = GcSafeHorizonEvaluation{};
        evaluation_out.oldest_snapshot_boundary = snapshot_registry_->oldestSnapshotBoundary(shard_id);
        evaluation_out.replication_watermark = follower_apply_pipeline_->replicationWatermark(shard_id);

        if (evaluation_out.oldest_snapshot_boundary == 0 || evaluation_out.replication_watermark == 0)
        {
            evaluation_out.gc_safe_horizon = 0;
            return Status::OK;
        }

        evaluation_out.gc_safe_horizon = std::min(evaluation_out.oldest_snapshot_boundary,
                                                  evaluation_out.replication_watermark);
        return Status::OK;
    }

    auto GcSafeHorizonCalculator::canReclaimVersion(const ID& shard_id,
                                                    uint64_t creator_local_txn_id,
                                                    bool& reclaimable_out,
                                                    GcSafeHorizonEvaluation* evaluation_out,
                                                    ErrorContext* ctx) const -> Status
    {
        reclaimable_out = false;
        if (creator_local_txn_id == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid creator_local_txn_id");
            return Status::INVALID_ARGUMENT;
        }

        GcSafeHorizonEvaluation evaluation{};
        Status status = evaluate(shard_id, evaluation, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        reclaimable_out = (evaluation.gc_safe_horizon != 0) &&
            (creator_local_txn_id < evaluation.gc_safe_horizon);
        if (evaluation_out != nullptr)
        {
            *evaluation_out = evaluation;
        }
        return Status::OK;
    }

    auto toString(DomainControlPlaneEventType event_type) -> const char*
    {
        switch (event_type)
        {
            case DomainControlPlaneEventType::CREATE:
                return "create";
            case DomainControlPlaneEventType::ALTER:
                return "alter";
            case DomainControlPlaneEventType::DROP:
                return "drop";
        }
        return "unknown";
    }

    auto DomainControlPlaneReplicaCatalog::computeDefinitionHash(std::string_view canonical_definition)
        -> std::string
    {
        const uint64_t hash = fnv1a64(canonical_definition);
        std::ostringstream out;
        out << std::hex << hash;
        return out.str();
    }

    auto DomainControlPlaneReplicaCatalog::appendEvent(const DomainControlPlaneEvent& event,
                                                       ErrorContext* ctx) -> Status
    {
        if (isZeroId(event.domain_id) || event.cluster_config_epoch == 0 || event.schema_epoch == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid domain control-plane event");
            return Status::INVALID_ARGUMENT;
        }

        if (event.event_type != DomainControlPlaneEventType::DROP && event.definition_hash.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Domain definition hash required");
            return Status::INVALID_ARGUMENT;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        if (event.cluster_config_epoch < last_cluster_config_epoch_ ||
            event.schema_epoch < last_schema_epoch_)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_TRANSACTION_STATE, "Domain event epochs must be monotonic");
            return Status::INVALID_TRANSACTION_STATE;
        }

        last_cluster_config_epoch_ = event.cluster_config_epoch;
        last_schema_epoch_ = event.schema_epoch;

        switch (event.event_type)
        {
            case DomainControlPlaneEventType::CREATE:
            case DomainControlPlaneEventType::ALTER:
                domain_hashes_[event.domain_id] = event.definition_hash;
                break;
            case DomainControlPlaneEventType::DROP:
                domain_hashes_.erase(event.domain_id);
                break;
        }

        event_log_.push_back(event);
        return Status::OK;
    }

    auto DomainControlPlaneReplicaCatalog::eventLog(std::vector<DomainControlPlaneEvent>& events_out) const
        -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events_out = event_log_;
        return Status::OK;
    }

    auto DomainControlPlaneReplicaCatalog::exportManifest(
        std::vector<DomainJoinManifestEntry>& manifest_out) const -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);
        manifest_out.clear();
        manifest_out.reserve(domain_hashes_.size());
        for (const auto& pair : domain_hashes_)
        {
            DomainJoinManifestEntry entry{};
            entry.domain_id = pair.first;
            entry.definition_hash = pair.second;
            manifest_out.push_back(std::move(entry));
        }
        return Status::OK;
    }

    auto DomainControlPlaneReplicaCatalog::validateJoinManifest(
        const std::vector<DomainJoinManifestEntry>& remote_manifest,
        DomainJoinValidationResult& result_out,
        ErrorContext* ctx) const -> Status
    {
        (void)ctx;
        result_out = DomainJoinValidationResult{};

        std::unordered_map<ID, std::string, IDHash> remote_hashes;
        remote_hashes.reserve(remote_manifest.size());
        for (const DomainJoinManifestEntry& entry : remote_manifest)
        {
            if (isZeroId(entry.domain_id))
            {
                result_out.mismatch_reasons.push_back("remote_zero_domain_id");
                continue;
            }
            auto [it, inserted] = remote_hashes.emplace(entry.domain_id, entry.definition_hash);
            if (!inserted)
            {
                result_out.mismatch_reasons.push_back("remote_duplicate_domain_id:" + entry.domain_id.toString());
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        result_out.local_domain_count = domain_hashes_.size();
        result_out.remote_domain_count = remote_manifest.size();

        for (const auto& local_pair : domain_hashes_)
        {
            auto remote_it = remote_hashes.find(local_pair.first);
            if (remote_it == remote_hashes.end())
            {
                result_out.mismatch_reasons.push_back("missing_remote_domain:" + local_pair.first.toString());
                continue;
            }

            if (remote_it->second != local_pair.second)
            {
                result_out.mismatch_reasons.push_back("hash_mismatch:" + local_pair.first.toString());
            }
        }

        for (const auto& remote_pair : remote_hashes)
        {
            if (domain_hashes_.find(remote_pair.first) == domain_hashes_.end())
            {
                result_out.mismatch_reasons.push_back("unexpected_remote_domain:" + remote_pair.first.toString());
            }
        }

        result_out.mismatch_count = result_out.mismatch_reasons.size();
        result_out.valid = (result_out.mismatch_count == 0);
        return Status::OK;
    }

} // namespace scratchbird::core
