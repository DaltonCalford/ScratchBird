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

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/storage_engine.h"
#include <cstdint>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class TransactionManager;
    class BufferPool;

    // Sweep statistics for monitoring
    struct SweepStatistics
    {
        uint64_t sweep_count = 0;              // Total sweeps executed
        uint64_t last_sweep_time = 0;          // Timestamp of last sweep (microseconds since epoch)
        uint64_t last_sweep_duration_ms = 0;   // Duration in milliseconds
        uint64_t last_oit_before = 0;          // OIT before last sweep
        uint64_t last_oit_after = 0;           // OIT after last sweep
        uint64_t total_transactions_swept = 0; // Cumulative count of transactions swept
        bool sweep_in_progress = false;        // Is sweep currently running?
        uint64_t total_evidence_items_emitted = 0;
        uint64_t last_evidence_items_emitted = 0;
        uint64_t evidence_persist_failures = 0;
        uint64_t total_wal_after_segments_emitted = 0;
        uint64_t last_wal_after_segments_emitted = 0;
        uint64_t wal_after_export_failures = 0;
        uint64_t wal_after_backlog_depth = 0;
        uint64_t total_page_audit_findings_emitted = 0;
        uint64_t last_page_audit_findings_emitted = 0;
        uint64_t page_audit_persist_failures = 0;
        uint64_t page_audit_mode_downgrades = 0;
        uint64_t total_shadow_capture_manifests_emitted = 0;
        uint64_t last_shadow_capture_manifests_emitted = 0;
        uint64_t shadow_capture_failures = 0;
        bool prune_blocked = false;
    };

    enum class SweepPolicyLane : uint8_t
    {
        NORMAL = 0,
        LINEAGE_RETENTION = 1,
        OBJECT_TOUCH_AUDIT = 2,
        SCHEMA_CHANGE_AUDIT = 3,
        WAL_AFTER_EXPORT = 4,
        PAGE_SPOT_AUDIT = 5,
        SHADOW_CAPTURE = 6,
        COMPOSITE = 7
    };

    enum class SweepScopeKind : uint8_t
    {
        DATABASE = 0,
        SCHEMA = 1,
        TABLE = 2,
        OBJECT_FAMILY = 3
    };

    enum class SweepProgressStage : uint8_t
    {
        NONE = 0,
        LOCAL_EVIDENCE_PENDING = 1,
        PAGE_AUDIT_PENDING = 2,
        SHADOW_CAPTURE_PENDING = 3,
        WAL_AFTER_PENDING = 4,
        RECLAIM_PENDING = 5
    };

    struct SweepPolicyScope
    {
        SweepScopeKind scope_kind = SweepScopeKind::DATABASE;
        ID scope_id{};
    };

    struct SweepPolicyBinding
    {
        ID binding_id{};
        SweepScopeKind scope_kind = SweepScopeKind::DATABASE;
        ID scope_id{};
        std::vector<SweepPolicyLane> lanes;
        ID sink_profile_id{};
        bool strict_audit = true;
        uint64_t created_time = 0;
    };

    struct SweepEvidenceWorkItem
    {
        ID work_item_id{};
        ID tx_uuid{};
        uint64_t txid = 0;
        ID sink_profile_id{};
        uint64_t sweep_oit_before = 0;
        uint64_t sweep_oit_after = 0;
        uint64_t segment_seq = 0;
        uint64_t created_time = 0;
        std::string evidence_class;
        std::string delivery_state;
        std::string spool_path;
        std::string lanes_csv;
    };

    struct SweepWalAfterLogSegment
    {
        ID segment_id{};
        ID sink_profile_id{};
        ID source_work_item_id{};
        ID source_sink_profile_id{};
        ID tx_uuid{};
        uint64_t txid = 0;
        uint64_t stream_seq = 0;
        uint64_t commit_time = 0;
        uint64_t created_time = 0;
        std::string shipping_mode;
        std::string statement_hashes_csv;
        std::string segment_path;
    };

    struct SweepPageAuditFinding
    {
        ID finding_id{};
        uint64_t finding_time = 0;
        std::string scan_mode;
        std::string trigger_source;
        ID filespace_uuid{};
        uint64_t page_id = 0;
        std::string page_type;
        std::string error_code;
        std::string severity;
        ID related_tx_uuid{};
        ID related_capsule_uuid{};
        std::string details_json;
        bool is_valid = true;
    };

    struct SweepShadowCaptureManifest
    {
        ID manifest_id{};
        ID tx_uuid{};
        ID object_uuid{};
        ID sink_profile_id{};
        uint64_t created_time = 0;
        bool has_retention_deadline_time = false;
        uint64_t retention_deadline_time = 0;
        std::string capture_scope;
        std::string capture_format;
        std::string payload_manifest;
        bool is_valid = true;
    };

    // SweepManager - Manages database sweep operations
    //
    // Sweep advances the Oldest Interesting Transaction (OIT) marker by:
    // 1. Scanning Transaction Inventory Pages (TIP) to find committed/aborted transactions
    // 2. Finding the first uncommitted transaction (becomes new OIT)
    // 3. Updating the database header with new OIT
    // 4. Optionally reclaiming space from old tuple versions (foreground mode)
    //
    // Sweep is triggered when: (OST - OIT) > sweep_interval
    // Where OST = Oldest Snapshot Transaction, OIT = Oldest Interesting Transaction
    class SweepManager
    {
    public:
        SweepManager(Database *db);
        ~SweepManager();

        // Explicitly delete copy operations
        SweepManager(const SweepManager &) = delete;
        SweepManager &operator=(const SweepManager &) = delete;

        // Initialize sweep manager
        Status initialize(ErrorContext *ctx = nullptr);

        // Check if sweep should be triggered based on transaction gap
        // Called after transaction commit
        // Returns true if sweep was triggered
        bool checkSweepTrigger(ErrorContext *ctx = nullptr);

        // Execute sweep process
        // foreground: true = full sweep with space reclamation, false = background (OIT advancement
        // only) Returns Status::OK on success
        Status executeSweep(bool foreground, ErrorContext *ctx = nullptr);

        // Configure deterministic sweep policy bindings. The current PH4 lane
        // supports explicit runtime bindings and database-scope execution.
        Status setPolicyBindings(const std::vector<SweepPolicyBinding>& bindings,
                                 ErrorContext* ctx = nullptr);

        // Resolve the effective policy binding across a supplied scope chain.
        // The first exact match in the chain wins; database-scope NORMAL is
        // returned when no binding exists.
        Status resolvePolicyBinding(const std::vector<SweepPolicyScope>& scope_chain,
                                    SweepPolicyBinding& binding_out,
                                    ErrorContext* ctx = nullptr) const;

        // List persisted local sweep evidence work items reconstructed from the
        // checksum-linked export-segment catalog for the built-in sweep sink.
        Status listEvidenceWorkItems(std::vector<SweepEvidenceWorkItem>& rows_out,
                                     ErrorContext* ctx = nullptr) const;

        // List persisted derivative wal_after_log segments reconstructed from
        // the append-only export-segment catalog.
        Status listWalAfterLogSegments(std::vector<SweepWalAfterLogSegment>& rows_out,
                                       ErrorContext* ctx = nullptr) const;

        // List persisted page spot-audit findings emitted by the sweep worker.
        Status listPageAuditFindings(std::vector<SweepPageAuditFinding>& rows_out,
                                     ErrorContext* ctx = nullptr) const;

        // List persisted logical shadow-capture manifests emitted by the sweep worker.
        Status listShadowCaptureManifests(std::vector<SweepShadowCaptureManifest>& rows_out,
                                          ErrorContext* ctx = nullptr) const;

        // Get current sweep statistics
        SweepStatistics getStatistics() const;

        // Check if sweep is currently running
        bool isSweepInProgress() const
        {
            return sweep_in_progress_.load(std::memory_order_acquire);
        }

    private:
        struct SweepProgressState
        {
            uint64_t sweep_generation = 0;
            bool active = false;
            uint64_t start_horizon = 0;
            uint64_t captured_oit = 0;
            uint64_t captured_oat = 0;
            uint64_t captured_ost = 0;
            uint64_t checkpoint_generation_seen = 0;
            uint16_t resume_lane_mask = 0;
            bool resume_strict_audit = true;
            ID relation_uuid{};
            ID filespace_uuid{};
            uint64_t page_id = 0;
            uint32_t slot_id = 0;
            uint32_t cursor_crc32c = 0;
            uint64_t persist_time = 0;
            uint64_t reclaimed_version_count = 0;
            uint64_t reclaimed_bytes = 0;
            uint64_t index_backlog_count = 0;
            SweepProgressStage stage = SweepProgressStage::NONE;
            bool cursor_checksum_valid = false;
        };

        Database *db_;
        TransactionManager *txn_manager_;
        BufferPool *buffer_pool_;

        // Sweep statistics (protected by mutex)
        mutable std::mutex stats_mutex_;
        SweepStatistics stats_;

        mutable std::mutex policy_mutex_;
        std::vector<SweepPolicyBinding> policy_bindings_;

        // Sweep in progress flag (atomic for lock-free check)
        std::atomic<bool> sweep_in_progress_{false};

        // Helper methods

        // Scan TIP pages to find first uncommitted transaction
        // Returns new OIT (or 0 if no change needed)
        uint64_t findFirstUncommittedTransaction(ErrorContext *ctx) const;

        // Reclaim space from old tuple versions (foreground sweep only)
        // Removes versions with xmax < new_oit
        Status reclaimSpace(uint64_t new_oit, SweepProgressState *progress, ErrorContext *ctx);

        Status loadSweepProgressState(SweepProgressState *state_out, ErrorContext *ctx) const;
        Status persistSweepProgressState(const SweepProgressState &state, ErrorContext *ctx);
        Status validateSweepResumeState(SweepProgressState *state,
                                        bool *resume_compatible_out,
                                        uint64_t *rewind_page_cursor_out,
                                        ErrorContext *ctx) const;

        // Update sweep statistics
        void updateStatistics(uint64_t oit_before, uint64_t oit_after, uint64_t duration_ms,
                              uint64_t evidence_items_emitted,
                              uint64_t wal_after_segments_emitted,
                              uint64_t wal_after_backlog_depth,
                              uint64_t page_audit_findings_emitted,
                              uint64_t shadow_capture_manifests_emitted,
                              bool prune_blocked,
                              bool evidence_failure,
                              bool page_audit_failure,
                              bool page_audit_mode_downgraded,
                              bool shadow_capture_failure,
                              bool wal_after_failure);

        Status emitLocalEvidenceForSweep(uint64_t oit_before, uint64_t oit_after,
                                         uint16_t lane_mask,
                                         bool strict_audit,
                                         uint64_t* evidence_items_emitted,
                                         bool* prune_blocked_out,
                                         ErrorContext* ctx);
        Status emitPageSpotAuditFindings(bool foreground,
                                         uint16_t lane_mask,
                                         uint64_t* findings_emitted,
                                         bool* mode_downgraded,
                                         ErrorContext* ctx);
        Status emitShadowCaptureManifests(uint64_t* manifests_emitted,
                                          uint16_t lane_mask,
                                          bool strict_audit,
                                          ErrorContext* ctx);
        Status emitDerivativeWalAfterLog(uint64_t* segments_emitted,
                                         uint64_t* backlog_depth,
                                         uint16_t lane_mask,
                                         ErrorContext* ctx);
    };

} // namespace scratchbird::core
