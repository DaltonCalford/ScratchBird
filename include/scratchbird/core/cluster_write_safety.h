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
#include <unordered_map>
#include <vector>

#include "scratchbird/core/error_context.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"

namespace scratchbird::core
{

    enum class WriteAdmissionReason : uint8_t
    {
        NONE = 0,
        SHARD_NOT_REGISTERED = 1,
        SHARD_WRITES_DISABLED = 2,
        NOT_CURRENT_LEADER = 3,
        FENCING_SHARD_MISMATCH = 4,
        STALE_FENCING_TOKEN = 5,
        ROUTING_EPOCH_MISMATCH = 6,
    };

    auto toString(WriteAdmissionReason reason) -> const char*;

    struct FencingToken
    {
        ID shard_id{};
        uint64_t leader_term = 0;
    };

    struct ShardLeaderState
    {
        ID shard_id{};
        ID leader_node_id{};
        uint64_t leader_term = 0;
        uint64_t routing_epoch = 0;
        bool write_enabled = true;
    };

    struct WriteAdmissionRequest
    {
        ID shard_id{};
        ID node_id{};
        FencingToken fencing_token{};
        bool has_routing_epoch = false;
        uint64_t routing_epoch = 0;
    };

    struct WriteAdmissionResult
    {
        bool allowed = false;
        Status status = Status::INVALID_TRANSACTION_STATE;
        WriteAdmissionReason reason = WriteAdmissionReason::SHARD_NOT_REGISTERED;
        uint64_t expected_leader_term = 0;
        uint64_t expected_routing_epoch = 0;
    };

    class ClusterWriteSafetyController
    {
    public:
        auto upsertShardLeaderState(const ShardLeaderState& state, ErrorContext* ctx = nullptr) -> Status;
        auto validateWrite(const WriteAdmissionRequest& request) const -> WriteAdmissionResult;
        auto getShardLeaderState(const ID& shard_id, ShardLeaderState& state_out) const -> bool;

    private:
        mutable std::mutex mutex_;
        std::unordered_map<ID, ShardLeaderState, IDHash> shard_states_;
    };

    enum class RoutingDecisionReason : uint8_t
    {
        NONE = 0,
        PLAN_NOT_FOUND = 1,
        PLAN_HAS_NO_TARGETS = 2,
        STALE_ROUTING_EPOCH = 3,
    };

    auto toString(RoutingDecisionReason reason) -> const char*;

    struct RoutingTarget
    {
        ID shard_id{};
        ID leader_node_id{};
        std::string leader_endpoint;
        uint32_t route_weight = 1;
    };

    struct RoutingPlan
    {
        ID table_id{};
        uint64_t routing_epoch = 0;
        std::vector<RoutingTarget> targets;
    };

    struct RoutingRequest
    {
        ID table_id{};
        std::string shard_key;
        bool has_expected_routing_epoch = false;
        uint64_t expected_routing_epoch = 0;
    };

    struct RoutingDecision
    {
        bool routed = false;
        Status status = Status::NOT_FOUND;
        RoutingDecisionReason reason = RoutingDecisionReason::PLAN_NOT_FOUND;
        uint64_t routing_epoch = 0;
        RoutingTarget target{};
    };

    class DeterministicShardRouter
    {
    public:
        auto upsertPlan(const RoutingPlan& plan, ErrorContext* ctx = nullptr) -> Status;
        auto route(const RoutingRequest& request) const -> RoutingDecision;
        auto getPlan(const ID& table_id, RoutingPlan& plan_out) const -> bool;

    private:
        static auto deterministicHash(std::string_view value) -> uint64_t;

        mutable std::mutex mutex_;
        std::unordered_map<ID, RoutingPlan, IDHash> plans_;
    };

    enum class SessionEpochMismatchPolicy : uint8_t
    {
        REPLAN = 0,
        REJECT = 1,
    };

    enum class SessionEpochReason : uint8_t
    {
        NONE = 0,
        CLUSTER_CONFIG_EPOCH_MISMATCH = 1,
        SCHEMA_EPOCH_MISMATCH = 2,
        SECURITY_EPOCH_MISMATCH = 3,
    };

    enum class SessionEpochAction : uint8_t
    {
        NONE = 0,
        REPLAN = 1,
        REJECT = 2,
    };

    auto toString(SessionEpochReason reason) -> const char*;

    struct SessionEpochPins
    {
        uint64_t cluster_config_epoch = 0;
        uint64_t schema_epoch = 0;
        uint64_t security_epoch = 0;
    };

    struct SessionEpochValidationResult
    {
        bool valid = true;
        Status status = Status::OK;
        SessionEpochReason reason = SessionEpochReason::NONE;
        SessionEpochAction action = SessionEpochAction::NONE;
    };

    auto validateSessionEpochPins(const SessionEpochPins& pinned,
                                  const SessionEpochPins& current,
                                  SessionEpochMismatchPolicy policy) -> SessionEpochValidationResult;

    enum class MultiShardGuardReason : uint8_t
    {
        NONE = 0,
        MULTI_SHARD_WRITE_REQUIRES_OVERRIDE = 1,
        MULTI_SHARD_WRITE_NOT_ALLOWED = 2,
    };

    auto toString(MultiShardGuardReason reason) -> const char*;

    struct MultiShardGuardPolicy
    {
        bool allow_cross_shard = false;
        bool require_explicit_override = true;
    };

    struct MultiShardGuardResult
    {
        bool allowed = false;
        Status status = Status::PERMISSION_DENIED;
        MultiShardGuardReason reason = MultiShardGuardReason::MULTI_SHARD_WRITE_NOT_ALLOWED;
        size_t unique_shard_count = 0;
    };

    auto evaluateMultiShardWrite(const std::vector<ID>& write_shards,
                                 const MultiShardGuardPolicy& policy,
                                 bool explicit_override) -> MultiShardGuardResult;

    struct GTXID
    {
        ID shard_id{};
        uint64_t local_txn_id = 0;

        auto operator==(const GTXID& other) const -> bool
        {
            return shard_id == other.shard_id && local_txn_id == other.local_txn_id;
        }
    };

    enum class TxnOrderingReason : uint8_t
    {
        NONE = 0,
        INVALID_GTXID = 1,
        STALE_OR_DUPLICATE = 2,
        OUT_OF_ORDER = 3,
    };

    auto toString(TxnOrderingReason reason) -> const char*;

    struct TxnOrderingResult
    {
        bool accepted = false;
        Status status = Status::INVALID_TRANSACTION_STATE;
        TxnOrderingReason reason = TxnOrderingReason::INVALID_GTXID;
        uint64_t expected_next_local_txn_id = 0;
    };

    class ShardTxnOrderBook
    {
    public:
        auto allocateNext(const ID& shard_id, GTXID& gtxid_out, ErrorContext* ctx = nullptr) -> Status;
        auto recordCommitted(const GTXID& gtxid) -> TxnOrderingResult;
        auto recordFollowerApply(const GTXID& gtxid) -> TxnOrderingResult;
        auto lastCommitted(const ID& shard_id) const -> uint64_t;
        auto lastApplied(const ID& shard_id) const -> uint64_t;

    private:
        struct ShardTxnState
        {
            uint64_t last_allocated = 0;
            uint64_t last_committed = 0;
            uint64_t last_applied = 0;
        };

        mutable std::mutex mutex_;
        std::unordered_map<ID, ShardTxnState, IDHash> shard_states_;
    };

    enum class ShardCommitLogAppendReason : uint8_t
    {
        NONE = 0,
        INVALID_ENTRY = 1,
        OUT_OF_ORDER_LOCAL_TXN_ID = 2,
        DURABILITY_WRITE_FAILED = 3,
    };

    auto toString(ShardCommitLogAppendReason reason) -> const char*;

    struct ShardCommitLogEntry
    {
        GTXID gtxid{};
        uint64_t commit_timestamp_ns = 0;
        std::string payload;
        std::string payload_format = "logical";
    };

    struct ShardCommitLogAppendResult
    {
        bool appended = false;
        Status status = Status::INVALID_TRANSACTION_STATE;
        ShardCommitLogAppendReason reason = ShardCommitLogAppendReason::INVALID_ENTRY;
        uint64_t expected_next_local_txn_id = 0;
        std::string durable_path;
    };

    class ShardCommitLog
    {
    public:
        explicit ShardCommitLog(std::string root_directory);

        auto append(const ShardCommitLogEntry& entry,
                    ShardCommitLogAppendResult* result_out = nullptr,
                    ErrorContext* ctx = nullptr) -> Status;

        auto readEntries(const ID& shard_id,
                         std::vector<ShardCommitLogEntry>& entries_out,
                         ErrorContext* ctx = nullptr) const -> Status;

        auto durablePathForShard(const ID& shard_id) const -> std::string;

    private:
        static auto hexEncode(const std::string& value) -> std::string;
        static auto hexDecode(const std::string& value, std::string& out) -> bool;
        auto appendDurableLine(const std::string& path,
                               const std::string& line,
                               ErrorContext* ctx) const -> Status;

        std::string root_directory_;
        mutable std::mutex mutex_;
        std::unordered_map<ID, uint64_t, IDHash> last_local_txn_id_by_shard_;
    };

    enum class FollowerApplyReason : uint8_t
    {
        NONE = 0,
        LOG_ENTRY_NOT_FOUND = 1,
        PAYLOAD_MISMATCH = 2,
        OUT_OF_ORDER = 3,
        ALREADY_APPLIED = 4,
    };

    auto toString(FollowerApplyReason reason) -> const char*;

    struct FollowerApplyResult
    {
        bool applied = false;
        bool replayed = false;
        Status status = Status::INVALID_TRANSACTION_STATE;
        FollowerApplyReason reason = FollowerApplyReason::OUT_OF_ORDER;
        uint64_t expected_next_local_txn_id = 0;
        uint64_t replication_watermark = 0;
    };

    class FollowerApplyPipeline
    {
    public:
        explicit FollowerApplyPipeline(ShardCommitLog* commit_log);

        auto apply(const ID& shard_id,
                   uint64_t local_txn_id,
                   const std::string& payload,
                   FollowerApplyResult* result_out = nullptr,
                   ErrorContext* ctx = nullptr) -> Status;

        auto replicationWatermark(const ID& shard_id) const -> uint64_t;

    private:
        ShardCommitLog* commit_log_ = nullptr;
        mutable std::mutex mutex_;
        std::unordered_map<ID, uint64_t, IDHash> last_applied_by_shard_;
    };

    struct SnapshotRegistryEntry
    {
        ID session_id{};
        ID shard_id{};
        uint64_t snapshot_boundary = 0;
        uint64_t start_time_ns = 0;
        uint64_t last_heartbeat_ns = 0;
    };

    class SnapshotRegistry
    {
    public:
        auto registerOrUpdate(const SnapshotRegistryEntry& entry, ErrorContext* ctx = nullptr) -> Status;
        auto remove(const ID& session_id, const ID& shard_id, ErrorContext* ctx = nullptr) -> Status;
        auto oldestSnapshotBoundary(const ID& shard_id) const -> uint64_t;
        auto listByShard(const ID& shard_id,
                         std::vector<SnapshotRegistryEntry>& entries_out,
                         ErrorContext* ctx = nullptr) const -> Status;

    private:
        mutable std::mutex mutex_;
        std::unordered_map<ID, std::unordered_map<ID, SnapshotRegistryEntry, IDHash>, IDHash> snapshots_by_shard_;
    };

    class CommittedWatermarkPublisher
    {
    public:
        auto publishCommitted(const GTXID& gtxid, ErrorContext* ctx = nullptr) -> Status;
        auto watermark(const ID& shard_id) const -> uint64_t;
        auto snapshotVector(const std::vector<ID>& shard_ids,
                            std::unordered_map<ID, uint64_t, IDHash>& out) const -> Status;

    private:
        mutable std::mutex mutex_;
        std::unordered_map<ID, uint64_t, IDHash> cwm_by_shard_;
    };

    struct GcSafeHorizonEvaluation
    {
        uint64_t oldest_snapshot_boundary = 0;
        uint64_t replication_watermark = 0;
        uint64_t gc_safe_horizon = 0;
    };

    class GcSafeHorizonCalculator
    {
    public:
        GcSafeHorizonCalculator(const SnapshotRegistry* snapshot_registry,
                                const FollowerApplyPipeline* follower_apply_pipeline);

        auto evaluate(const ID& shard_id,
                      GcSafeHorizonEvaluation& evaluation_out,
                      ErrorContext* ctx = nullptr) const -> Status;

        auto canReclaimVersion(const ID& shard_id,
                               uint64_t creator_local_txn_id,
                               bool& reclaimable_out,
                               GcSafeHorizonEvaluation* evaluation_out = nullptr,
                               ErrorContext* ctx = nullptr) const -> Status;

    private:
        const SnapshotRegistry* snapshot_registry_ = nullptr;
        const FollowerApplyPipeline* follower_apply_pipeline_ = nullptr;
    };

    enum class DomainControlPlaneEventType : uint8_t
    {
        CREATE = 0,
        ALTER = 1,
        DROP = 2,
    };

    auto toString(DomainControlPlaneEventType event_type) -> const char*;

    struct DomainControlPlaneEvent
    {
        uint64_t cluster_config_epoch = 0;
        uint64_t schema_epoch = 0;
        DomainControlPlaneEventType event_type = DomainControlPlaneEventType::CREATE;
        ID domain_id{};
        std::string definition_hash;
    };

    struct DomainJoinManifestEntry
    {
        ID domain_id{};
        std::string definition_hash;
    };

    struct DomainJoinValidationResult
    {
        bool valid = false;
        size_t mismatch_count = 0;
        size_t local_domain_count = 0;
        size_t remote_domain_count = 0;
        std::vector<std::string> mismatch_reasons;
    };

    class DomainControlPlaneReplicaCatalog
    {
    public:
        static auto computeDefinitionHash(std::string_view canonical_definition) -> std::string;

        auto appendEvent(const DomainControlPlaneEvent& event, ErrorContext* ctx = nullptr) -> Status;
        auto eventLog(std::vector<DomainControlPlaneEvent>& events_out) const -> Status;
        auto exportManifest(std::vector<DomainJoinManifestEntry>& manifest_out) const -> Status;
        auto validateJoinManifest(const std::vector<DomainJoinManifestEntry>& remote_manifest,
                                  DomainJoinValidationResult& result_out,
                                  ErrorContext* ctx = nullptr) const -> Status;

    private:
        mutable std::mutex mutex_;
        std::vector<DomainControlPlaneEvent> event_log_;
        std::unordered_map<ID, std::string, IDHash> domain_hashes_;
        uint64_t last_cluster_config_epoch_ = 0;
        uint64_t last_schema_epoch_ = 0;
    };

} // namespace scratchbird::core
