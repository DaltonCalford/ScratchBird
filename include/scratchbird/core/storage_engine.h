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

#include "scratchbird/core/gc_publication.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/savepoint_backout.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/tid.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <memory>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <limits>

namespace scratchbird::sblr
{
    class Executor;
}

namespace scratchbird::core
{

    using ID = UuidV7Bytes;

    // Forward declarations
    class Database;
    class BufferPool;
    class PageManager;
    class CatalogManager;
    class HeapPage;
    class StorageEngine;
    class MgaBackoutEngine;
    class ToastManager;
    struct ErrorContext;
    struct TableInfo;

    // Tuple data structure
    // PHASE 1.5: Migrated to TID struct
    struct Tuple
    {
        const uint8_t *data; // Pointer to tuple data
        uint32_t data_size;  // Size of tuple data
        TID tid;             // Tuple ID (GPID + slot)
    };

    // Iterator for sequential scan
    class HeapScanIterator
    {
    public:
        HeapScanIterator(Database *db, StorageEngine *engine, const ID &table_id,
                         uint32_t start_page,
                         uint32_t end_page_exclusive,
                         bool ignore_visibility);
        ~HeapScanIterator();

        // Move to next tuple
        auto next(Tuple *tuple_out, ErrorContext *ctx = nullptr) -> Status;

        // Check if scan is complete
        [[nodiscard]] auto isDone() const -> bool
        {
            return done_;
        }

    private:
        Database *db_;
        StorageEngine *engine_;
        ID table_id_;
        uint16_t tablespace_id_ = PRIMARY_TABLESPACE_ID;
        uint32_t current_page_;
        uint16_t current_item_;
        uint32_t last_page_;
        uint32_t end_page_exclusive_ = std::numeric_limits<uint32_t>::max();
        size_t current_page_index_ = 0;
        std::vector<GPID> allocated_pages_;
        GPID current_gpid_ = INVALID_GPID;
        bool done_;
        bool filter_session_ = false;
        ID session_id_{};
        bool ignore_visibility_ = false;
        uint32_t ra_current_pages_ = 0;
        uint32_t ra_seq_count_ = 0;
        uint32_t ra_last_page_ = UINT32_MAX;
        size_t ra_last_index_ = std::numeric_limits<size_t>::max();
        std::vector<uint8_t> visible_tuple_buffer_;

        // Current page data
        uint8_t *page_data_ = nullptr;

        // Load next page
        auto loadPage(uint32_t page_id, ErrorContext *ctx) -> Status;
        void maybeReadAheadPrimary(uint32_t page_id, ErrorContext *ctx);
        void maybeReadAheadTablespace(size_t page_index, ErrorContext *ctx);
    };

    // Iterator for index scan
    class IndexScanIterator
    {
    public:
        IndexScanIterator(Database *db, StorageEngine *engine, const ID &index_id,
                          const ID &table_id);
        ~IndexScanIterator();

        // Move to the first entry >= key
        auto seek(const std::vector<uint8_t> &key, ErrorContext *ctx = nullptr) -> Status;

        // Move to the next entry
        auto next(Tuple *tuple_out, ErrorContext *ctx = nullptr) -> Status;

        // Check if scan is complete
        [[nodiscard]] auto isDone() const -> bool
        {
            return done_;
        }

    private:
        Database *db_;
        StorageEngine *engine_;
        ID index_id_;
        ID table_id_;
        bool done_;

        // B-tree traversal state
        // PHASE 1.5: Migrated to TID struct
        std::vector<TID> current_tuple_ids_;      // Tuple IDs from current key
        size_t current_tuple_index_;              // Index within current_tuple_ids_
        std::vector<uint8_t> current_key_;        // Current key being scanned
        bool initialized_;                        // Whether seek() has been called
    };

    // Storage engine for heap storage
    class StorageEngine
    {
    public:
        struct CommitGroupMaintenanceStats
        {
            uint64_t batches_applied = 0;
            uint64_t transactions_applied = 0;
            uint64_t deltas_applied = 0;
            uint64_t locality_groups_applied = 0;
            uint64_t apply_failures = 0;
        };

        struct DeferredExactSecondaryMergeStats
        {
            uint64_t indexes_considered = 0;
            uint64_t indexes_merged = 0;
            uint64_t deltas_merged = 0;
        };

        struct FragmentationAdvisory
        {
            uint32_t page_id = 0;
            uint32_t live_tuple_bytes = 0;
            uint32_t reclaimable_bytes = 0;
            uint32_t free_bytes = 0;
            uint16_t live_slots = 0;
            uint16_t deleted_slots = 0;
            uint16_t unused_slots = 0;
            uint16_t chain_depth_hint = 0;
            uint16_t same_page_back_versions = 0;
            double same_page_update_ratio = 1.0;
            double dead_space_ratio = 0.0;
            bool warn_threshold = false;
            bool compact_threshold = false;
            bool rewrite_recommended = false;
            bool compaction_applied = false;
        };

        struct FragmentationAdvisorySnapshot
        {
            ID table_id{};
            FragmentationAdvisory advisory{};
        };

        struct UniquePreflightTraceStats
        {
            bool metadata_cache_hit = false;
            uint32_t unique_index_count = 0;
            double metadata_ms = 0.0;
            double layout_ms = 0.0;
            double key_extract_ms = 0.0;
            double exact_lookup_ms = 0.0;
            double visibility_ms = 0.0;
        };

        struct BulkInsertMaintenancePlanState;

        struct BulkInsertHandle
        {
            ID table_id{};
            uint16_t source_tablespace = PRIMARY_TABLESPACE_ID;
            uint16_t target_tablespace = PRIMARY_TABLESPACE_ID;
            uint64_t current_xid = 0;
            ToastManager *toast_mgr = nullptr;
            GPID pinned_gpid = INVALID_GPID;
            uint32_t pinned_page_id = 0;
            void *pinned_page_buffer = nullptr;
            bool pinned_page_dirty = false;
            bool migration_in_progress = false;
            ID migration_id{};
            bool temp_scope = false;
            ID temp_session_id{};
            uint32_t resume_scan_page = 0;
            bool have_resume_scan_page = false;
            uint32_t reservation_target_pages = 0;
            uint32_t reserved_page_budget = 0;
            uint32_t total_reserved_pages = 0;
            uint32_t consumed_reserved_pages = 0;
            uint32_t reservation_events = 0;
            bool reservation_failed = false;
            std::shared_ptr<BulkInsertMaintenancePlanState> maintenance_plan_state;
            uint32_t maintenance_plan_index_count = 0;
            uint32_t maintenance_plan_exact_index_count = 0;
            uint32_t maintenance_plan_unique_exact_index_count = 0;
            uint32_t maintenance_plan_active_maintenance_count = 0;
            uint32_t maintenance_plan_deferred_exact_index_count = 0;
            uint32_t maintenance_plan_grouped_exact_index_count = 0;
            uint32_t maintenance_plan_buffered_empty_unique_index_count = 0;
            uint64_t unique_preflight_bypass_rows = 0;
            uint64_t timing_begin_us = 0;
            uint64_t timing_unique_preflight_us = 0;
            uint64_t timing_buffered_unique_preflight_us = 0;
            uint64_t timing_reserve_growth_us = 0;
            uint64_t timing_find_free_page_us = 0;
            uint64_t timing_pin_page_us = 0;
            uint64_t timing_heap_insert_us = 0;
            uint64_t timing_post_insert_maintenance_us = 0;
            uint64_t timing_post_insert_track_mutation_us = 0;
            uint64_t timing_post_insert_migration_dirty_us = 0;
            uint64_t timing_post_insert_plan_build_us = 0;
            uint64_t timing_post_insert_layout_us = 0;
            uint64_t timing_post_insert_index_lookup_us = 0;
            uint64_t timing_post_insert_preflight_lookup_us = 0;
            uint64_t timing_post_insert_key_extract_us = 0;
            uint64_t timing_post_insert_scalar_key_build_us = 0;
            uint64_t timing_post_insert_buffer_enqueue_us = 0;
            uint64_t timing_post_insert_defer_check_us = 0;
            uint64_t timing_post_insert_columnstore_insert_us = 0;
            uint64_t timing_post_insert_buffered_flush_unique_us = 0;
            uint64_t timing_post_insert_grouped_flush_us = 0;
            uint64_t timing_post_insert_deferred_flush_us = 0;
            uint64_t timing_post_insert_direct_index_insert_us = 0;
            uint64_t timing_post_insert_online_delta_capture_us = 0;
            uint64_t timing_post_insert_extractor_clear_us = 0;
            uint64_t timing_post_insert_table_dml_delta_us = 0;
            uint64_t timing_end_flush_unique_us = 0;
            uint64_t timing_end_flush_unique_prepare_entries_us = 0;
            uint64_t timing_end_flush_unique_bulkload_sort_us = 0;
            uint64_t timing_end_flush_unique_bulkload_leaf_build_us = 0;
            uint64_t timing_end_flush_unique_bulkload_internal_build_us = 0;
            uint64_t timing_end_flush_unique_bulkload_root_finalize_us = 0;
            uint64_t timing_end_flush_unique_bulkload_total_us = 0;
            uint64_t timing_end_flush_grouped_us = 0;
            uint64_t timing_end_flush_deferred_us = 0;
            uint64_t timing_rows_inserted = 0;
            uint64_t timing_find_free_page_calls = 0;
            uint64_t timing_pinned_page_reuse_hits = 0;
            uint64_t timing_new_page_allocations = 0;
            uint64_t timing_page_full_retries = 0;
            uint64_t timing_post_insert_track_mutation_calls = 0;
            uint64_t timing_post_insert_migration_dirty_calls = 0;
            uint64_t timing_post_insert_plan_build_calls = 0;
            uint64_t timing_post_insert_layout_calls = 0;
            uint64_t timing_post_insert_index_lookup_calls = 0;
            uint64_t timing_post_insert_preflight_lookup_calls = 0;
            uint64_t timing_post_insert_key_extract_calls = 0;
            uint64_t timing_post_insert_scalar_key_build_calls = 0;
            uint64_t timing_post_insert_buffer_enqueue_rows = 0;
            uint64_t timing_post_insert_defer_check_calls = 0;
            uint64_t timing_post_insert_columnstore_insert_calls = 0;
            uint64_t timing_post_insert_buffered_flush_unique_calls = 0;
            uint64_t timing_post_insert_grouped_flush_calls = 0;
            uint64_t timing_post_insert_deferred_flush_calls = 0;
            uint64_t timing_post_insert_direct_index_insert_calls = 0;
            uint64_t timing_post_insert_online_delta_capture_calls = 0;
            uint64_t timing_post_insert_extractor_clear_calls = 0;
            uint64_t timing_post_insert_table_dml_delta_calls = 0;
            uint64_t timing_buffered_unique_fast_scalar_rows = 0;
            bool timing_end_flush_unique_bulkload_input_sorted = false;
            bool maintenance_plan_built = false;
            bool initialized = false;
        };

        struct UnchangedKeyUpdateMaintenanceTarget
        {
            ID index_id{};
            ID maintenance_id{};
        };

        struct UnchangedKeyUpdatePlan
        {
            std::vector<UnchangedKeyUpdateMaintenanceTarget> exact_indexes;
        };

        struct BulkInsertBufferedUniquePreflightKey
        {
            size_t target_index = 0;
            bool scalar_fast = false;
            uint64_t scalar_order_key = 0;
            uint8_t scalar_key_width = 0;
            std::vector<uint8_t> key;
        };

        explicit StorageEngine(Database *db);
        ~StorageEngine();

        // Insert a tuple into a table
        // Returns the tuple ID (page_id, item_id) on success
        auto insertTuple(const ID &table_id, const uint8_t *tuple_data, uint32_t tuple_size,
                         uint32_t *page_id_out, uint16_t *item_id_out, ErrorContext *ctx = nullptr)
            -> Status;

        auto beginBulkInsert(const ID &table_id, BulkInsertHandle *handle,
                             ErrorContext *ctx = nullptr) -> Status;
        auto insertTupleWithHandle(BulkInsertHandle *handle, const uint8_t *tuple_data,
                                   uint32_t tuple_size, uint32_t *page_id_out,
                                   uint16_t *item_id_out, ErrorContext *ctx = nullptr) -> Status;
        void endBulkInsert(BulkInsertHandle *handle, ErrorContext *ctx = nullptr);

        // Delete all tuples for a session from a temporary table
        auto deleteTuplesForSession(const ID &table_id, const ID &session_id,
                                    ErrorContext *ctx = nullptr) -> Status;

        // Get a specific tuple by ID
        auto getTuple(uint32_t page_id, uint16_t item_id, Tuple *tuple_out,
                      ErrorContext *ctx = nullptr) -> Status;

        // Get a specific tuple by TID with dual-source visibility (Sprint 4 Task 5.4.2)
        // Resolves which tablespace to read from during ONLINE migration
        auto getTuple(const ID &table_id, const TID &tid, Tuple *tuple_out,
                      ErrorContext *ctx = nullptr) -> Status;

        // Delete a tuple (mark as deleted)
        auto deleteTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                         uint16_t tablespace_id_override = UINT16_MAX,
                         ErrorContext *ctx = nullptr) -> Status;

        // Delete a tuple by TID
        auto deleteTuple(const ID &table_id, uint64_t tid, uint64_t xmax,
                         ErrorContext *ctx = nullptr) -> Status;
        auto deleteTuple(const ID &table_id, const TID &tid,
                         ErrorContext *ctx = nullptr) -> Status;

        // Update a tuple (MGA Phase 3: Version Chains)
        // Creates a new version and links it to the old version
        auto updateTuple(const ID &table_id, uint32_t page_id, uint16_t item_id,
                         const uint8_t *new_tuple_data, uint32_t new_tuple_size,
                         uint32_t *new_page_id_out, uint16_t *new_item_id_out,
                         ErrorContext *ctx = nullptr,
                         bool indexed_keys_unchanged = false,
                         const UnchangedKeyUpdatePlan *unchanged_key_update_plan = nullptr)
            -> Status;

        // Create a sequential scan iterator
        auto createScan(const ID &table_id, ErrorContext *ctx = nullptr)
            -> std::unique_ptr<HeapScanIterator>;
        auto createScanRange(const ID &table_id,
                             uint32_t start_page,
                             uint32_t end_page_exclusive,
                             ErrorContext *ctx = nullptr)
            -> std::unique_ptr<HeapScanIterator>;
        auto createScanAll(const ID &table_id, ErrorContext *ctx = nullptr)
            -> std::unique_ptr<HeapScanIterator>;

        // Create an index scan iterator
        auto createIndexScan(const ID &index_id, ErrorContext *ctx = nullptr)
            -> std::unique_ptr<IndexScanIterator>;

        // Create a sequential scan iterator with visibility
        auto sequentialScan(const ID &table_id, const std::vector<uint32_t> &columns, uint64_t xmin,
                            ErrorContext *ctx = nullptr) -> std::unique_ptr<HeapScanIterator>;

        // Check if a tuple is visible (basic visibility for single connection)
        auto isVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const -> bool;

        // Get current transaction ID from TransactionManager
        [[nodiscard]] auto getCurrentXid() const -> uint64_t;

        // TASK-DML-2: Public helper for removing from any index type (for executor)
        // Note: index_type_value is uint8_t to avoid circular include dependency
        // It corresponds to CatalogManager::IndexType enum value
        auto removeFromIndexHelper(uint8_t index_type_value,
                                    void *index_ptr,
                                    const std::vector<uint8_t> &key,
                                    const TID &tid,
                                    uint64_t xid,
                                    ErrorContext *ctx) -> Status;

        void publishFragmentationAdvisory(const ID &table_id, uint32_t page_id,
                                          const FragmentationAdvisory &advisory);
        void clearFragmentationAdvisory(const ID &table_id, uint32_t page_id);
        [[nodiscard]] auto getFragmentationAdvisory(const ID &table_id, uint32_t page_id,
                                                    FragmentationAdvisory *advisory_out) const
            -> bool;
        auto listFragmentationAdvisories(std::vector<FragmentationAdvisorySnapshot>& advisories_out) const
            -> Status;
        void publishIndexCleanupPublication(const IndexCleanupPublicationRecord& publication);
        auto listIndexCleanupPublications(
            std::vector<IndexCleanupPublicationRecord>& publications_out) const -> Status;
        auto getCommitGroupMaintenanceStats() const -> CommitGroupMaintenanceStats;
        auto drainDeferredExactSecondaryPageDeltas(
            size_t max_indexes_per_pass,
            DeferredExactSecondaryMergeStats* stats_out = nullptr,
            ErrorContext* ctx = nullptr) -> Status;
        auto prepareCommitGroupMaintenanceDeltas(const std::vector<uint64_t>& ordered_committed_xids,
                                                 std::vector<uint64_t>& prepared_xids_out,
                                                 std::vector<ID>& inserted_delta_ids_out,
                                                 uint64_t* locality_groups_out,
                                                 uint64_t* delta_count_out,
                                                 ErrorContext* ctx) -> Status;
        void finalizePreparedCommitGroupMaintenanceDeltas(
            const std::vector<uint64_t>& prepared_xids,
            uint64_t locality_group_count,
            uint64_t delta_count);
        void abortPreparedCommitGroupMaintenanceDeltas(
            const std::vector<ID>& inserted_delta_ids);
        void discardPendingCommitGroupMaintenanceDeltas(uint64_t xid);
        void discardPendingCommitGroupMaintenanceDeltas(
            const std::vector<uint64_t>& xids);

    private:
        friend class IndexScanIterator;
        friend class MgaBackoutEngine;
        friend class scratchbird::sblr::Executor;

        struct PendingOnlineMaintenanceDelta
        {
            ID logical_index_id{};
            ID maintenance_id{};
            uint8_t delta_op_value = 0;
            uint64_t tid_gpid = 0;
            uint16_t tid_slot = 0;
            uint64_t commit_txid = 0;
            uint64_t locality_key = 0;
        };

        Database *db_;
        BufferPool *buffer_pool_;
        PageManager *page_manager_;
        CatalogManager *catalog_manager_;

        // ToastManager cache (per-table)
        std::unordered_map<ID, std::unique_ptr<ToastManager>> toast_managers_;
        std::mutex toast_mutex_; // Protects toast_managers_ map
        std::unordered_map<ID, std::unordered_map<uint16_t, uint32_t>> relation_write_hints_;
        mutable std::mutex relation_write_hint_mutex_;
        struct UniquePreflightIndexPlan;
        struct UniquePreflightTablePlan;
        struct UniquePreflightCacheState;
        std::unique_ptr<UniquePreflightCacheState> unique_preflight_cache_state_;
        std::unordered_map<ID, std::unordered_map<uint32_t, FragmentationAdvisory>>
            fragmentation_advisories_;
        mutable std::mutex fragmentation_advisory_mutex_;
        std::unordered_map<ID, std::unordered_map<uint32_t, IndexCleanupPublicationRecord>>
            cleanup_publications_;
        mutable std::mutex cleanup_publication_mutex_;
        std::unordered_map<uint64_t, std::vector<PendingOnlineMaintenanceDelta>>
            pending_commit_group_maintenance_deltas_;
        mutable std::mutex pending_commit_group_maintenance_delta_mutex_;
        std::atomic<uint64_t> commit_group_maintenance_batches_applied_{0};
        std::atomic<uint64_t> commit_group_maintenance_transactions_applied_{0};
        std::atomic<uint64_t> commit_group_maintenance_deltas_applied_{0};
        std::atomic<uint64_t> commit_group_maintenance_locality_groups_applied_{0};
        std::atomic<uint64_t> commit_group_maintenance_apply_failures_{0};
        std::unordered_set<ID, IDHash> deferred_exact_secondary_indexes_in_merge_;
        mutable std::mutex deferred_exact_secondary_merge_state_mutex_;
        mutable std::condition_variable deferred_exact_secondary_merge_cv_;
        void refreshIndexCleanupDebtLedger(const ID& index_id);
        auto captureQueuedOrImmediateOnlineMaintenanceDelta(const ID& logical_index_id,
                                                           const ID& maintenance_id,
                                                           uint8_t delta_op_value,
                                                           const TID& tid,
                                                           uint64_t commit_txid) -> bool;
        auto captureImmediateOnlineMaintenanceDelta(const ID& logical_index_id,
                                                   const ID& maintenance_id,
                                                   uint8_t delta_op_value,
                                                   const TID& tid,
                                                   uint64_t commit_txid) -> bool;
        auto maybeDeferColdExactSecondaryInsert(const ID& index_id,
                                                GPID root_gpid,
                                                bool is_unique,
                                                bool is_expression_index,
                                                bool is_partial_index,
                                                uint8_t actual_index_type_value,
                                                const std::vector<uint8_t>& key,
                                                const TID& tid,
                                                uint64_t xid,
                                                bool* deferred_exact_mode,
                                                ErrorContext* ctx) -> bool;
        auto persistDeferredExactSecondaryInsert(const ID& index_id,
                                                uint8_t actual_index_type_value,
                                                const std::vector<uint8_t>& key,
                                                const TID& tid,
                                                uint64_t xid,
                                                ErrorContext* ctx) -> Status;
        void publishDeferredExactCleanupDebtSnapshot(
            const ID& index_id,
            IndexCleanupPublicationState preferred_state,
            uint64_t entries_removed,
            bool repair_required);
        auto mergeDeferredExactSecondaryPageDeltas(
            const ID& index_id,
            bool is_unique,
            bool is_expression_index,
            bool is_partial_index,
            uint8_t actual_index_type_value,
            void* index_ptr,
            uint64_t current_xid,
            ErrorContext* ctx) -> Status;

        // Find a page with free space for a tuple
        auto findFreePage(const ID &table_id, uint32_t tuple_size, uint32_t *page_id_out,
                          uint16_t tablespace_id, ErrorContext *ctx,
                          uint32_t *pages_scanned_out = nullptr,
                          uint32_t *heap_pages_examined_out = nullptr,
                          bool *allocated_new_out = nullptr,
                          const uint32_t *resume_from_page_hint = nullptr) -> Status;
        auto reserveBulkInsertGrowthWindow(BulkInsertHandle *handle,
                                           uint32_t tuple_size,
                                           ErrorContext *ctx) -> Status;
        auto buildBulkInsertMaintenancePlan(
            const ID &table_id,
            std::shared_ptr<BulkInsertMaintenancePlanState> *plan_out,
            ErrorContext *ctx) -> Status;

        // Find a locality-preserving page for a back version created during MGA update.
        auto findBackVersionPlacementPage(const ID &table_id, uint32_t tuple_size,
                                          uint32_t primary_page_id, uint16_t tablespace_id,
                                          uint32_t *page_id_out, ErrorContext *ctx) -> Status;

        // Allocate a new heap page for a table
        auto allocateHeapPage(const ID &table_id, uint16_t tablespace_id, uint32_t *page_id_out,
                              ErrorContext *ctx) -> Status;
        [[nodiscard]] auto getRelationWriteHint(const ID &table_id,
                                                uint16_t tablespace_id,
                                                uint32_t *page_id_out) const -> bool;
        void rememberRelationWriteHint(const ID &table_id,
                                       uint16_t tablespace_id,
                                       uint32_t page_id);
        void invalidateRelationWriteHint(const ID &table_id,
                                         uint16_t tablespace_id,
                                         uint32_t page_id);

        // Get or create ToastManager for a table
        auto getOrCreateToastManager(const ID &table_id, ErrorContext *ctx) -> ToastManager *;
        auto extractStoredIndexKey(const ID &table_id,
                                   const std::vector<ID> &indexed_column_ids,
                                   const Tuple &tuple,
                                   std::vector<uint8_t> *key_out,
                                   ErrorContext *ctx) -> Status;
        auto getVisibleTupleForStableTid(const ID &table_id,
                                         const TID &stable_tid,
                                         Tuple *tuple_out,
                                         ErrorContext *ctx) -> Status;
        auto getUniquePreflightTablePlan(const ID &table_id,
                                         UniquePreflightTablePlan *plan_out,
                                         bool *cache_hit_out,
                                         ErrorContext *ctx) -> Status;
        void invalidateUniquePreflightTablePlan(const ID &table_id);
        auto filterIndexCandidatesByVisibleHeap(const ID &table_id,
                                                const std::vector<ID> &indexed_column_ids,
                                                bool enforce_key_semantics,
                                                const std::vector<uint8_t> &search_key,
                                                const std::vector<TID> &candidate_tids,
                                                const TID *exclude_tid,
                                                std::vector<TID> *visible_tids,
                                                ErrorContext *ctx) -> Status;
        auto preflightUniqueInsert(const ID &table_id,
                                   const uint8_t *tuple_data,
                                   uint32_t tuple_size,
                                   uint64_t current_xid,
                                   UniquePreflightTraceStats *trace_stats,
                                   const std::unordered_set<ID, IDHash> *skip_unique_index_ids,
                                   bool *all_unique_indexes_skipped_out,
                                   ErrorContext *ctx) -> Status;
        auto preflightUniqueInsertWithBulkHandle(BulkInsertHandle *handle,
                                                 const uint8_t *tuple_data,
                                                 uint32_t tuple_size,
                                                 std::vector<BulkInsertBufferedUniquePreflightKey> *buffered_unique_keys_out,
                                                 ErrorContext *ctx) -> Status;
        auto preflightUniqueUpdate(const ID &table_id,
                                   const uint8_t *old_tuple_data,
                                   uint32_t old_tuple_size,
                                   const uint8_t *new_tuple_data,
                                   uint32_t new_tuple_size,
                                   const TID &stable_tid,
                                   uint64_t current_xid,
                                   UniquePreflightTraceStats *trace_stats,
                                   ErrorContext *ctx) -> Status;
        auto performPostInsertMaintenance(const ID &table_id,
                                          uint16_t source_tablespace,
                                          bool migration_in_progress,
                                          const ID &migration_id,
                                          uint16_t target_tablespace,
                                          const uint8_t *tuple_data,
                                          uint32_t tuple_size,
                                          uint32_t page_id,
                                          uint16_t item_id,
                                          uint64_t current_xid,
                                          ToastManager *toast_mgr,
                                          BulkInsertHandle *timing_handle,
                                          BulkInsertMaintenancePlanState *maintenance_plan,
                                          const std::vector<BulkInsertBufferedUniquePreflightKey> *buffered_unique_preflight_keys,
                                          ErrorContext *ctx) -> Status;
        auto updateStableTidIndexesForMutation(const ID &table_id,
                                               uint16_t tablespace_id,
                                               uint32_t stable_page_id,
                                               uint16_t stable_item_id,
                                               const uint8_t *old_tuple_data,
                                               uint32_t old_tuple_size,
                                               const uint8_t *new_tuple_data,
                                               uint32_t new_tuple_size,
                                               uint64_t current_xid,
                                               ErrorContext *ctx) -> Status;

        // Lock management helpers
        auto acquireTupleLock(const ID &table_id, uint32_t page_id, uint16_t item_id,
                              uint32_t proc_id, bool wait, ErrorContext *ctx) -> Status;
        auto releaseTupleLock(const ID &table_id, uint32_t page_id, uint16_t item_id,
                              uint32_t proc_id, ErrorContext *ctx) -> Status;

        // Index update helper for cross-page relocations
        auto updateIndexesForRelocation(const ID &table_id, uint32_t old_page_id,
                                        uint16_t old_item_id, uint32_t new_page_id,
                                        uint16_t new_item_id, const uint8_t *tuple_data,
                                        uint32_t tuple_size, ErrorContext *ctx) -> Status;
        // Physical row-backout leaves used by MgaBackoutEngine. Semantic
        // ownership of savepoint rollback lives outside StorageEngine.
        auto applyStableHeadBackout(const SavepointBackoutAction &action,
                                    uint64_t rollback_xid,
                                    ErrorContext *ctx) -> Status;
        auto applyStableTidIndexBackout(const ID &table_id,
                                        uint16_t tablespace_id,
                                        uint32_t stable_page_id,
                                        uint16_t stable_item_id,
                                        const uint8_t *current_tuple_data,
                                        uint32_t current_tuple_size,
                                        const uint8_t *prior_tuple_data,
                                        uint32_t prior_tuple_size,
                                        const std::vector<std::vector<uint8_t>> &transient_tuple_images,
                                        bool prior_row_present,
                                        uint64_t current_xid,
                                        ErrorContext *ctx) -> Status;
        auto backoutRemoveStableHeadRow(const SavepointBackoutAction &action,
                                        uint64_t rollback_xid,
                                        ErrorContext *ctx) -> Status;
        auto backoutRestoreStableHeadRow(const SavepointBackoutAction &action,
                                         uint64_t rollback_xid,
                                         ErrorContext *ctx) -> Status;
        auto rewriteStableTidIndexesForRollback(const ID &table_id,
                                                uint16_t tablespace_id,
                                                uint32_t stable_page_id,
                                                uint16_t stable_item_id,
                                                const uint8_t *current_tuple_data,
                                                uint32_t current_tuple_size,
                                                const uint8_t *restored_tuple_data,
                                                uint32_t restored_tuple_size,
                                                const std::vector<std::vector<uint8_t>> &transient_tuple_images,
                                                bool restored_row_present,
                                                uint64_t current_xid,
                                                ErrorContext *ctx) -> Status;
    };

} // namespace scratchbird::core
