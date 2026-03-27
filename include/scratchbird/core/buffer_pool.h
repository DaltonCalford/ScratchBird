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
#include <vector>
#include <list>
#include <deque>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <string>
#include <cmath>
#include <cstdio>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/gpid.h"

namespace scratchbird::core
{

    // Forward declarations
    class Database;
    class ScratchBirdMetrics;

    /**
     * Buffer Pool - Manages in-memory page cache
     *
     * AUDIT CONTRACT:
     * - Alpha recovery truth remains MGA/TIP/CLOG state; buffer policy is not recovery authority.
     * - `PoolLayout::Segmented` means logical policy domains and budget accounting over the current
     *   shared frame array. The first-wave tier model now exposes `Probationary`, `Protected`,
     *   `RingOnly`, and `PinBiased` residency plus bounded ghost history over that shared array.
     * - Domain accounting and frame state must stay observable in code even before later tickets
     *   finish domain-aware victim search, queue-driven writeback, and restart queue rebuild.
     * - Admission, promotion, demotion, and ghost reuse are advisory policy only. They may not
     *   redefine MGA visibility, checkpoint truth, or commit-fence durability.
     *
     * Implements a fixed-size buffer pool with Clock Sweep eviction algorithm.
     * Clock Sweep provides better eviction decisions than pure LRU with O(1) complexity.
     * Thread-safe with mutex protection.
     */
    class BufferPool
    {
    public:
        enum class AccessStrategy
        {
            Normal,
            Sequential,
            Vacuum,
            BulkWrite
        };

        enum class BufferProfile : uint8_t
        {
            Dev = 0,
            Oltp = 1,
            Mixed = 2,
            Analytics = 3,
            MaintenanceRecovery = 4
        };

        enum class PoolLayout
        {
            Single,
            Segmented,
            HotCold,
            Tablespace
        };

        enum class PolicyDomain : uint8_t
        {
            CriticalSystem = 0,
            HotOltp = 1,
            ReadMostly = 2,
            ScanBulkRing = 3,
            VersionUndo = 4,
            TemporaryWork = 5,
            Count = 6
        };

        enum class WorkloadClass : uint8_t
        {
            Unspecified = 0,
            PointLookup = 1,
            IndexProbe = 2,
            RangeScan = 3,
            SequentialScan = 4,
            NestedLoopReread = 5,
            SweepGc = 6,
            BulkWrite = 7,
            CheckpointCleaner = 8,
            RecoveryReplay = 9,
            PrefetchSpeculative = 10,
            TemporaryWork = 11
        };

        enum class MgaPageClass : uint8_t
        {
            Generic = 0,
            TX_STATE = 1,
            SYSTEM_META = 2,
            INDEX_ROOT_INTERNAL = 3,
            VERSION_ROOT = 4,
            CHAIN_HEAVY = 5,
            GC_CANDIDATE = 6,
            SCAN_PROBATION = 7,
            INDEX_CHURN = 8,
            TEMP_WORK = 9
        };

        // AUDIT CONTRACT:
        // Queue kind is a derived scheduling reason for resident dirty pages. It
        // exists to make cleaner ordering, writeback-debt accounting, and
        // failure attribution readable in code. Queue kind is not recovery
        // truth, and queue contents may be rebuilt from page state.
        enum class WritebackQueueState : uint8_t
        {
            NONE = 0,
            FOREGROUND_HELP = 1,
            BACKGROUND_AGE = 2,
            CHECKPOINT = 3,
            METADATA_PRIORITY = 4,
            WRITE_COMBINE = 5,
            REPAIR_RETRY = 6
        };

        enum class ResidencyTier : uint8_t
        {
            LegacyShared = 0,
            RingOnly = 1,
            Probationary = 2,
            Protected = 3,
            PinBiased = 4
        };

        enum class LifecycleState : uint8_t
        {
            Free = 0,
            Loading = 1,
            Valid = 2,
            Flushing = 3,
            Evicting = 4,
            Error = 5
        };

        // AUDIT CONTRACT:
        // Resident dirty-state tracks page publication progress inside the MGA
        // buffer/cache model. `DirtyFlushedPendingFsync` means the canonical page
        // image has been written but the engine-wide forced-write fence has not
        // finished yet. This is durability staging, not WAL/redo authority.
        enum class DirtyState : uint8_t
        {
            Clean = 0,
            DirtyUnscheduled = 1,
            DirtyQueued = 2,
            DirtyInFlight = 3,
            DirtyFlushedPendingFsync = 4,
            DirtyFailed = 5
        };

        // AUDIT CONTRACT:
        // Thrash state is an observable policy posture only. It explains why
        // speculative prefetch was clamped or cancelled; it is not durability
        // or visibility truth.
        enum class ThrashDetectorState : uint8_t
        {
            None = 0,
            GlobalDebtCap = 1,
            SessionBudgetCap = 2,
            ScanPressure = 3,
            UsefulnessCollapse = 4
        };

        struct MgaFrameHints
        {
            MgaPageClass page_class = MgaPageClass::Generic;
            WorkloadClass workload_class = WorkloadClass::Unspecified;
            uint64_t oldest_interesting_txid = 0;
            uint64_t prune_safe_horizon_hint = 0;
            uint32_t dead_version_bytes = 0;
            uint16_t chain_depth_hint = 0;
            uint64_t last_gc_touch_generation = 0;
            uint64_t scan_probation_generation = 0;
            bool commit_fence_member = false;
        };

        struct MgaFrameSnapshot
        {
            bool resident = false;
            GPID gpid = INVALID_GPID;
            uint16_t owner_partition = 0;
            uint16_t home_partition = 0;
            uint32_t object_id = 0;
            MgaPageClass page_class = MgaPageClass::Generic;
            PolicyDomain policy_domain = PolicyDomain::HotOltp;
            WorkloadClass workload_class = WorkloadClass::Unspecified;
            ResidencyTier residency_tier = ResidencyTier::LegacyShared;
            LifecycleState lifecycle_state = LifecycleState::Free;
            DirtyState dirty_state = DirtyState::Clean;
            WritebackQueueState writeback_queue_state = WritebackQueueState::NONE;
            uint32_t pin_count = 0;
            bool is_dirty = false;
            uint64_t state_generation = 0;
            uint64_t io_generation = 0;
            uint64_t dirty_generation = 0;
            uint64_t last_flush_generation = 0;
            uint64_t checkpoint_target_generation = 0;
            uint64_t admission_generation = 0;
            uint64_t last_touch_generation = 0;
            uint64_t temperature_generation = 0;
            uint64_t oldest_interesting_txid = 0;
            uint64_t prune_safe_horizon_hint = 0;
            uint32_t dead_version_bytes = 0;
            uint16_t chain_depth_hint = 0;
            uint64_t prefetch_session_key = 0;
            uint64_t last_gc_touch_generation = 0;
            uint64_t scan_probation_generation = 0;
            bool speculative_prefetch = false;
            bool prefetch_consumed = false;
            bool commit_fence_member = false;
        };

        struct DomainBudgetConfig
        {
            double min_pct = 0.0;
            double target_pct = 0.0;
            double max_pct = 0.0;
            uint32_t min_frames = 0;
            uint32_t target_frames = 0;
            uint32_t max_frames = 0;
        };

        // Buffer pool configuration
        struct Config
        {
            uint32_t pool_size = 64;    // Number of pages in pool
            uint32_t page_size = 16384; // Page size in bytes
            BufferProfile profile = BufferProfile::Mixed;
            PoolLayout layout = PoolLayout::Segmented;
            std::array<DomainBudgetConfig, static_cast<size_t>(PolicyDomain::Count)> domain_budgets{};
            uint32_t replacement_protected_pct = 35;
            uint32_t replacement_ghost_history_pct = 20;
            uint32_t admission_second_touch_generations = 2;
            bool admission_direct_protect_roots = true;
            bool prefetch_enabled = true;
            uint32_t prefetch_workers = 1;
            uint32_t prefetch_scan_window_pages = 16;
            uint32_t prefetch_index_window_pages = 8;
            uint32_t prefetch_chain_window_pages = 8;
            uint32_t prefetch_max_debt_pages = 256;
            uint32_t prefetch_usefulness_floor_pct = 50;
            uint32_t thrash_session_budget_pct = 15;
            uint32_t thrash_object_budget_pct = 25;
            uint32_t thrash_prefetch_pressure_pct = 80;

            // Adaptive flushing configuration (Issue 2.20)
            bool enable_background_writer = true;   // Enable background writer thread
            uint32_t bgwriter_delay_ms = 200;       // Delay between background writer runs (milliseconds)
            uint32_t bgwriter_max_pages = 100;      // Maximum pages to write per background writer cycle
            double dirty_ratio_low = 0.25;          // Start flushing when dirty ratio exceeds this (25%)
            double dirty_ratio_high = 0.50;         // Aggressive flushing when dirty ratio exceeds this (50%)
            double dirty_ratio_checkpoint = 0.75;   // Emergency flushing to prevent checkpoint storm (75%)

            Config()
            {
                applyProfileDefaults(profile);
            }

            static auto pctToFrames(double pct, uint32_t pool_size) -> uint32_t
            {
                if (pct <= 0.0 || pool_size == 0)
                {
                    return 0;
                }
                const auto frames = static_cast<uint32_t>(
                    std::ceil((pct * static_cast<double>(pool_size)) / 100.0));
                return std::max<uint32_t>(1, frames);
            }

            auto domainBudget(PolicyDomain domain) -> DomainBudgetConfig&
            {
                return domain_budgets[static_cast<size_t>(domain)];
            }

            auto domainBudget(PolicyDomain domain) const -> const DomainBudgetConfig&
            {
                return domain_budgets[static_cast<size_t>(domain)];
            }

            void recomputeDomainFrames()
            {
                for (auto &budget : domain_budgets)
                {
                    budget.min_frames = pctToFrames(budget.min_pct, pool_size);
                    budget.target_frames = pctToFrames(budget.target_pct, pool_size);
                    budget.max_frames = pctToFrames(budget.max_pct, pool_size);
                }
            }

            void applyProfileDefaults(BufferProfile new_profile)
            {
                profile = new_profile;
                domain_budgets.fill(DomainBudgetConfig{});

                switch (profile)
                {
                    case BufferProfile::Dev:
                        domainBudget(PolicyDomain::CriticalSystem).min_pct = 8.0;
                        domainBudget(PolicyDomain::HotOltp).target_pct = 28.0;
                        domainBudget(PolicyDomain::ReadMostly).target_pct = 20.0;
                        domainBudget(PolicyDomain::ScanBulkRing).max_pct = 12.0;
                        domainBudget(PolicyDomain::VersionUndo).min_pct = 12.0;
                        domainBudget(PolicyDomain::TemporaryWork).max_pct = 10.0;
                        replacement_protected_pct = 30;
                        replacement_ghost_history_pct = 15;
                        admission_second_touch_generations = 2;
                        prefetch_max_debt_pages = 64;
                        prefetch_usefulness_floor_pct = 40;
                        break;
                    case BufferProfile::Oltp:
                        domainBudget(PolicyDomain::CriticalSystem).min_pct = 10.0;
                        domainBudget(PolicyDomain::HotOltp).target_pct = 40.0;
                        domainBudget(PolicyDomain::ReadMostly).target_pct = 12.0;
                        domainBudget(PolicyDomain::ScanBulkRing).max_pct = 8.0;
                        domainBudget(PolicyDomain::VersionUndo).min_pct = 18.0;
                        domainBudget(PolicyDomain::TemporaryWork).max_pct = 6.0;
                        replacement_protected_pct = 40;
                        replacement_ghost_history_pct = 15;
                        admission_second_touch_generations = 2;
                        prefetch_max_debt_pages = 128;
                        prefetch_usefulness_floor_pct = 60;
                        break;
                    case BufferProfile::Mixed:
                        domainBudget(PolicyDomain::CriticalSystem).min_pct = 8.0;
                        domainBudget(PolicyDomain::HotOltp).target_pct = 34.0;
                        domainBudget(PolicyDomain::ReadMostly).target_pct = 20.0;
                        domainBudget(PolicyDomain::ScanBulkRing).max_pct = 12.0;
                        domainBudget(PolicyDomain::VersionUndo).min_pct = 16.0;
                        domainBudget(PolicyDomain::TemporaryWork).max_pct = 10.0;
                        replacement_protected_pct = 35;
                        replacement_ghost_history_pct = 20;
                        admission_second_touch_generations = 2;
                        prefetch_max_debt_pages = 256;
                        prefetch_usefulness_floor_pct = 50;
                        break;
                    case BufferProfile::Analytics:
                        domainBudget(PolicyDomain::CriticalSystem).min_pct = 8.0;
                        domainBudget(PolicyDomain::HotOltp).target_pct = 22.0;
                        domainBudget(PolicyDomain::ReadMostly).target_pct = 32.0;
                        domainBudget(PolicyDomain::ScanBulkRing).max_pct = 12.0;
                        domainBudget(PolicyDomain::VersionUndo).min_pct = 14.0;
                        domainBudget(PolicyDomain::TemporaryWork).max_pct = 10.0;
                        replacement_protected_pct = 25;
                        replacement_ghost_history_pct = 25;
                        admission_second_touch_generations = 3;
                        prefetch_max_debt_pages = 512;
                        prefetch_usefulness_floor_pct = 35;
                        break;
                    case BufferProfile::MaintenanceRecovery:
                        domainBudget(PolicyDomain::CriticalSystem).min_pct = 12.0;
                        domainBudget(PolicyDomain::HotOltp).target_pct = 18.0;
                        domainBudget(PolicyDomain::ReadMostly).target_pct = 16.0;
                        domainBudget(PolicyDomain::ScanBulkRing).max_pct = 8.0;
                        domainBudget(PolicyDomain::VersionUndo).min_pct = 24.0;
                        domainBudget(PolicyDomain::TemporaryWork).max_pct = 8.0;
                        replacement_protected_pct = 30;
                        replacement_ghost_history_pct = 15;
                        admission_second_touch_generations = 1;
                        prefetch_max_debt_pages = 64;
                        prefetch_usefulness_floor_pct = 70;
                        break;
                }

                recomputeDomainFrames();
            }
        };

        BufferPool(Database *db, const Config &config);
        ~BufferPool();

        // Initialize buffer pool
        auto initialize(ErrorContext *ctx = nullptr) -> Status;

        // Shutdown and flush all dirty pages
        auto shutdown(ErrorContext *ctx = nullptr) -> Status;

        // === LEGACY API: 32-bit page_id (tablespace 0 only) ===

        /**
         * Pin a page in the buffer pool (LEGACY API - tablespace 0 only)
         * @param page_id Page to pin (32-bit, primary tablespace only)
         * @param buffer Returns pointer to page data
         * @param ctx Error context
         * @return Status code
         *
         * Note: For new code, use pinPageGlobal(GPID) instead.
         */
        auto pinPage(uint32_t page_id, void **buffer, ErrorContext *ctx = nullptr,
                     AccessStrategy strategy = AccessStrategy::Normal,
                     WorkloadClass workload_class = WorkloadClass::Unspecified) -> Status;

        /**
         * Unpin a page (LEGACY API - tablespace 0 only)
         * @param page_id Page to unpin (32-bit, primary tablespace only)
         * @param is_dirty True if page was modified
         * @param ctx Error context
         * @return Status code
         *
         * Note: For new code, use unpinPageGlobal(GPID) instead.
         */
        auto unpinPage(uint32_t page_id, bool is_dirty, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Allocate a new page and pin it (LEGACY API - tablespace 0 only)
         * @param page_id_out Returns the allocated page ID (32-bit, primary tablespace only)
         * @param buffer Returns pointer to page data
         * @param ctx Error context
         * @return Status code
         *
         * Note: For new code, use allocatePageGlobal(GPID) instead.
         */
        auto allocatePage(uint32_t *page_id_out, void **buffer, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Mark a page as dirty (LEGACY API - tablespace 0 only)
         * @param page_id Page to mark dirty (32-bit, primary tablespace only)
         * @param ctx Error context
         * @return Status code
         *
         * Note: For new code, use markDirtyGlobal(GPID) instead.
         */
        auto markDirty(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Flush a specific page if dirty (LEGACY API - tablespace 0 only)
         * @param page_id Page to flush (32-bit, primary tablespace only)
         * @param ctx Error context
         * @return Status code
         *
         * Note: For new code, use flushPageGlobal(GPID) instead.
         */
        auto flushPage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

        // === NEW: GPID-based API (Phase 1, Task 1.2.3) ===

        /**
         * pinPageGlobal - Pin a page in the buffer pool (GPID version)
         *
         * @param gpid Global Page ID of page to pin
         * @param buffer Returns pointer to page data
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Purpose: Multi-tablespace support. Pins a page identified by GPID.
         *
         * AUDIT CONTRACT:
         * - `workload_class` is advisory policy intent only.
         * - Page-role safety still outranks workload hints.
         * - This API exists so auditors can see workload intent enter the
         *   residency policy in code instead of inferring it from downstream
         *   heuristics.
         *
         * Example:
         *   GPID gpid = makeGPID(5, 1000);  // Tablespace 5, page 1000
         *   void *buffer;
         *   Status s = buffer_pool->pinPageGlobal(gpid, &buffer);
         */
        auto pinPageGlobal(GPID gpid, void **buffer, ErrorContext *ctx = nullptr,
                           AccessStrategy strategy = AccessStrategy::Normal,
                           WorkloadClass workload_class = WorkloadClass::Unspecified) -> Status;

        /**
         * unpinPageGlobal - Unpin a page (GPID version)
         *
         * @param gpid Global Page ID of page to unpin
         * @param is_dirty True if page was modified
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        auto unpinPageGlobal(GPID gpid, bool is_dirty, ErrorContext *ctx = nullptr) -> Status;

        /**
         * allocatePageGlobal - Allocate a new page in a specific tablespace and pin it
         *
         * @param tablespace_id Tablespace ID (0 = primary, 1-65535 = custom)
         * @param gpid_out Returns the allocated GPID
         * @param buffer Returns pointer to page data
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Note: For Phase 1, only tablespace_id=0 (primary) is supported.
         */
        auto allocatePageGlobal(uint16_t tablespace_id, GPID *gpid_out, void **buffer,
                               ErrorContext *ctx = nullptr) -> Status;

        /**
         * markDirtyGlobal - Mark a page as dirty (GPID version)
         *
         * @param gpid Global Page ID of page to mark dirty
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        auto markDirtyGlobal(GPID gpid, ErrorContext *ctx = nullptr) -> Status;

        /**
         * flushPageGlobal - Flush a specific page if dirty (GPID version)
         *
         * @param gpid Global Page ID of page to flush
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        auto flushPageGlobal(GPID gpid, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Flush all dirty pages
         */
        auto flushAll(ErrorContext *ctx = nullptr) -> Status;

        /**
         * P2-3: Prefetch multiple pages into buffer pool
         *
         * @param page_ids Vector of page IDs to prefetch
         * @param ctx Error context
         * @return Status::OK on success (partial success still returns OK)
         *
         * Purpose: TOAST chunk prefetching optimization. Batches multiple page reads
         *          to reduce random I/O and improve cache locality.
         *
         * Algorithm:
         *   1. Filter out pages already in buffer pool
         *   2. Pin remaining pages (reads from disk into cache)
         *   3. Immediately unpin (but pages stay in cache for subsequent access)
         *
         * Thread-safety: Uses partition locks for page table lookups, global lock
         *                only when frame allocation is needed.
         */
        auto prefetchPages(const std::vector<uint32_t> &page_ids, ErrorContext *ctx = nullptr,
                           AccessStrategy strategy = AccessStrategy::Normal,
                           WorkloadClass workload_class = WorkloadClass::Unspecified) -> Status;

        /**
         * P2-3: Prefetch multiple pages (GPID version)
         */
        auto prefetchPagesGlobal(const std::vector<GPID> &gpids, ErrorContext *ctx = nullptr,
                                 AccessStrategy strategy = AccessStrategy::Normal,
                                 WorkloadClass workload_class = WorkloadClass::Unspecified)
            -> Status;

        /**
         * flushTablespace - Flush all dirty pages for a specific tablespace
         *
         * @param tablespace_id Tablespace ID to flush (0 = primary, 1-65535 = custom)
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         *
         * Purpose: Phase 6 detach support. Ensures all dirty pages for a tablespace
         *          are written to disk before the tablespace is detached.
         *
         * Algorithm:
         *   1. Iterate through all frames in the buffer pool
         *   2. For each frame with matching tablespace_id:
         *      - If dirty, call flushPageGlobal(gpid)
         *   3. Return Status::OK when all pages flushed
         */
        auto flushTablespace(uint16_t tablespace_id, ErrorContext *ctx = nullptr) -> Status;

        auto publishMgaFrameHintsGlobal(GPID gpid,
                                        const MgaFrameHints &hints,
                                        ErrorContext *ctx = nullptr) -> Status;

        auto getMgaFrameSnapshotGlobal(GPID gpid,
                                       MgaFrameSnapshot *snapshot_out,
                                       ErrorContext *ctx = nullptr) const -> Status;
        uint64_t currentDirtyGeneration() const
        {
            return dirty_generation_clock_.load(std::memory_order_relaxed);
        }
        uint32_t currentDirtyPageCount() const
        {
            return dirty_page_count_.load(std::memory_order_relaxed);
        }
        auto flushDirtyCheckpointBoundary(uint64_t dirty_generation_boundary,
                                          ErrorContext *ctx = nullptr) -> Status;

        void beginCommitFence();
        void endCommitFence();
        // Called by Database::sync() once the engine-wide forced-write fence
        // succeeds so resident frames can advance from
        // DirtyFlushedPendingFsync to Clean.
        void completeFsyncFence();
        // AUDIT CONTRACT:
        // Startup recovery may reseed checkpoint-marker debt from published page
        // headers and checkpoint control state. This restores restart-time
        // checkpoint catch-up without treating the buffer pool as recovery
        // authority or replaying WAL.
        void restoreCheckpointQueueState(const std::vector<GPID> &checkpoint_marker_gpids,
                                         uint64_t dirty_generation_floor);
        auto checkpointDebtCandidateCount() -> uint64_t;

        /**
         * Lock a page for exclusive access (must be pinned first)
         * Caller must call unlockPage() when done
         * @param page_id Page ID to lock
         * @param ctx Error context
         * @return Status code
         */
        auto lockPage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;
        auto lockPageGlobal(GPID gpid, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Unlock a previously locked page
         * @param page_id Page ID to unlock
         * @param ctx Error context
         * @return Status code
         */
        auto unlockPage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;
        auto unlockPageGlobal(GPID gpid, ErrorContext *ctx = nullptr) -> Status;

        // Runtime config snapshot for tests and diagnostics.
        auto getConfigSnapshot() const -> Config
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return config_;
        }

        struct DomainAccountingSnapshot
        {
            PolicyDomain domain = PolicyDomain::CriticalSystem;
            DomainBudgetConfig budget{};
            uint64_t resident_pages = 0;
            uint64_t protected_pages = 0;
            uint64_t probationary_pages = 0;
            uint64_t ring_only_pages = 0;
            uint64_t pin_biased_pages = 0;
            uint64_t dirty_pages = 0;
            uint64_t dirty_bytes = 0;
            uint64_t commit_fence_pages = 0;
            uint64_t borrowed_pages = 0;
            uint64_t reservation_breach_count = 0;
            uint64_t emergency_breach_count = 0;
        };

        auto getDomainAccountingSnapshot() const
            -> std::array<DomainAccountingSnapshot, static_cast<size_t>(PolicyDomain::Count)>
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::array<DomainAccountingSnapshot, static_cast<size_t>(PolicyDomain::Count)> snapshot{};

            for (size_t i = 0; i < snapshot.size(); ++i)
            {
                snapshot[i].domain = static_cast<PolicyDomain>(i);
                snapshot[i].budget = config_.domain_budgets[i];
            }

            const auto resident_counts = collectDomainResidentCountsLocked();

            for (const auto &frame : frames_)
            {
                const auto lifecycle = static_cast<LifecycleState>(
                    frame.lifecycle_state.load(std::memory_order_relaxed));
                if (lifecycle == LifecycleState::Free)
                {
                    continue;
                }

                const auto domain = static_cast<PolicyDomain>(
                    frame.policy_domain.load(std::memory_order_relaxed));
                auto &entry = snapshot[static_cast<size_t>(domain)];
                entry.resident_pages++;
                const auto residency_tier = static_cast<ResidencyTier>(
                    frame.residency_tier.load(std::memory_order_relaxed));
                switch (residency_tier)
                {
                    case ResidencyTier::Protected:
                        entry.protected_pages++;
                        break;
                    case ResidencyTier::Probationary:
                        entry.probationary_pages++;
                        break;
                    case ResidencyTier::RingOnly:
                        entry.ring_only_pages++;
                        break;
                    case ResidencyTier::PinBiased:
                        entry.pin_biased_pages++;
                        break;
                    case ResidencyTier::LegacyShared:
                        break;
                }
                if (frame.is_dirty.load(std::memory_order_relaxed))
                {
                    entry.dirty_pages++;
                    entry.dirty_bytes += config_.page_size;
                }
                if (frame.commit_fence_member.load(std::memory_order_relaxed))
                {
                    entry.commit_fence_pages++;
                }
            }

            for (size_t i = 0; i < snapshot.size(); ++i)
            {
                const auto domain = static_cast<PolicyDomain>(i);
                snapshot[i].borrowed_pages = domainBorrowedPagesLocked(domain, resident_counts);
                snapshot[i].reservation_breach_count =
                    domain_reservation_breach_counts_[i].load(std::memory_order_relaxed);
                snapshot[i].emergency_breach_count =
                    domain_emergency_breach_counts_[i].load(std::memory_order_relaxed);
            }

            return snapshot;
        }

        // Statistics snapshot (non-atomic for return values)
        struct StatsSnapshot
        {
            uint64_t hits = 0;      // Cache hits
            uint64_t misses = 0;    // Cache misses
            uint64_t evictions = 0; // Pages evicted
            uint64_t flushes = 0;   // Pages flushed

            // READ ONLY transaction optimizations (Phase 3)
            uint64_t evictions_clean = 0; // Clean pages evicted (read-only benefit)
            uint64_t evictions_dirty = 0; // Dirty pages evicted (requires flush)

            // Corruption detection (MED-005)
            uint64_t page_size_mismatches = 0; // Page size mismatches corrected

            // Clock Sweep algorithm statistics (Issue 2.14)
            uint64_t clock_sweeps = 0;      // Total clock sweeps performed
            uint64_t clock_hand_resets = 0; // Times clock hand wrapped around

            // Background writer statistics (Issue 2.20)
            uint64_t bgwriter_runs = 0;          // Background writer cycles executed
            uint64_t bgwriter_pages_written = 0; // Total pages written by background writer
            uint64_t bgwriter_maxwritten = 0;    // Times bgwriter hit max_pages limit
            uint64_t checkpoint_flushes = 0;     // Pages flushed during checkpoints
            double dirty_ratio_current = 0.0;    // Current dirty page ratio (0.0-1.0)
            double dirty_ratio_max = 0.0;        // Maximum dirty ratio since last reset
            uint64_t dirty_generation_low_watermark = 0;
            uint64_t dirty_generation_high_watermark = 0;
            uint64_t checkpoint_bound_dirty_pages = 0;
            uint64_t foreground_help_backlog_pages = 0;
            uint64_t queue_depth_foreground_help = 0;
            uint64_t queue_depth_background_age = 0;
            uint64_t queue_depth_checkpoint = 0;
            uint64_t queue_depth_metadata_priority = 0;
            uint64_t queue_depth_write_combine = 0;
            uint64_t queue_depth_repair_retry = 0;
            uint64_t prefetch_pages_total = 0;
            uint64_t prefetch_pages_useful = 0;
            uint64_t prefetch_pages_unused_evicted = 0;
            uint64_t prefetch_cancelled_pages = 0;
            uint64_t prefetch_debt_pages = 0;
            uint64_t prefetch_scan_debt_pages = 0;
            uint64_t fairness_session_budget_breaches = 0;
            uint64_t fairness_object_budget_breaches = 0;
            uint64_t thrash_policy_shift_count = 0;
            double prefetch_usefulness_pct = 100.0;
            ThrashDetectorState thrash_detector_state = ThrashDetectorState::None;

            uint64_t mga_frames_tx_state = 0;
            uint64_t mga_frames_system_meta = 0;
            uint64_t mga_frames_index_root_internal = 0;
            uint64_t mga_frames_version_root = 0;
            uint64_t mga_frames_chain_heavy = 0;
            uint64_t mga_frames_gc_candidate = 0;
            uint64_t mga_frames_scan_probation = 0;
            uint64_t mga_frames_index_churn = 0;
            uint64_t mga_frames_temp_work = 0;

            uint64_t mga_evictions_tx_state = 0;
            uint64_t mga_evictions_system_meta = 0;
            uint64_t mga_evictions_index_root_internal = 0;
            uint64_t mga_evictions_version_root = 0;
            uint64_t mga_evictions_chain_heavy = 0;
            uint64_t mga_evictions_gc_candidate = 0;
            uint64_t mga_evictions_scan_probation = 0;
            uint64_t mga_evictions_index_churn = 0;
            uint64_t mga_evictions_temp_work = 0;

            uint64_t mga_commit_fence_backlog = 0;
            uint64_t mga_gc_handoff_offers = 0;
            uint64_t mga_scan_probation_churn = 0;
            uint64_t mga_chain_heavy_hits = 0;
            uint64_t mga_protected_set_collapse_events = 0;
            uint64_t mga_admission_promotions = 0;
            uint64_t mga_residency_demotions = 0;
            uint64_t mga_ghost_history_hits = 0;
            uint64_t mga_ghost_history_entries = 0;
        };

        auto getStats() const -> StatsSnapshot
        {
            std::lock_guard<std::mutex> lock(mutex_);

            // ISSUE 3.10 FIX: Read atomic stats with memory_order_relaxed
            // Relaxed ordering is sufficient since we're just gathering statistics
            StatsSnapshot snapshot;
            snapshot.hits = stats_.hits.load(std::memory_order_relaxed);
            snapshot.misses = stats_.misses.load(std::memory_order_relaxed);
            snapshot.evictions = stats_.evictions.load(std::memory_order_relaxed);
            snapshot.flushes = stats_.flushes.load(std::memory_order_relaxed);
            snapshot.evictions_clean = stats_.evictions_clean.load(std::memory_order_relaxed);
            snapshot.evictions_dirty = stats_.evictions_dirty.load(std::memory_order_relaxed);
            snapshot.page_size_mismatches = stats_.page_size_mismatches.load(std::memory_order_relaxed);
            snapshot.clock_sweeps = stats_.clock_sweeps.load(std::memory_order_relaxed);
            snapshot.clock_hand_resets = stats_.clock_hand_resets.load(std::memory_order_relaxed);
            snapshot.bgwriter_runs = stats_.bgwriter_runs.load(std::memory_order_relaxed);
            snapshot.bgwriter_pages_written = stats_.bgwriter_pages_written.load(std::memory_order_relaxed);
            snapshot.bgwriter_maxwritten = stats_.bgwriter_maxwritten.load(std::memory_order_relaxed);
            snapshot.checkpoint_flushes = stats_.checkpoint_flushes.load(std::memory_order_relaxed);
            snapshot.dirty_ratio_current = stats_.dirty_ratio_current;
            snapshot.dirty_ratio_max = stats_.dirty_ratio_max;
            snapshot.prefetch_pages_total =
                stats_.prefetch_pages_total.load(std::memory_order_relaxed);
            snapshot.prefetch_pages_useful =
                stats_.prefetch_pages_useful.load(std::memory_order_relaxed);
            snapshot.prefetch_pages_unused_evicted =
                stats_.prefetch_pages_unused_evicted.load(std::memory_order_relaxed);
            snapshot.prefetch_cancelled_pages =
                stats_.prefetch_cancelled_pages.load(std::memory_order_relaxed);
            snapshot.fairness_session_budget_breaches =
                stats_.fairness_session_budget_breaches.load(std::memory_order_relaxed);
            snapshot.fairness_object_budget_breaches =
                stats_.fairness_object_budget_breaches.load(std::memory_order_relaxed);
            snapshot.thrash_policy_shift_count =
                stats_.thrash_policy_shift_count.load(std::memory_order_relaxed);
            snapshot.thrash_detector_state = static_cast<ThrashDetectorState>(
                thrash_detector_state_.load(std::memory_order_relaxed));
            const uint64_t usefulness_denominator = snapshot.prefetch_pages_useful +
                                                   snapshot.prefetch_pages_unused_evicted;
            if (usefulness_denominator != 0)
            {
                snapshot.prefetch_usefulness_pct =
                    (static_cast<double>(snapshot.prefetch_pages_useful) * 100.0) /
                    static_cast<double>(usefulness_denominator);
            }

            snapshot.mga_evictions_tx_state =
                stats_.mga_evictions_tx_state.load(std::memory_order_relaxed);
            snapshot.mga_evictions_system_meta =
                stats_.mga_evictions_system_meta.load(std::memory_order_relaxed);
            snapshot.mga_evictions_index_root_internal =
                stats_.mga_evictions_index_root_internal.load(std::memory_order_relaxed);
            snapshot.mga_evictions_version_root =
                stats_.mga_evictions_version_root.load(std::memory_order_relaxed);
            snapshot.mga_evictions_chain_heavy =
                stats_.mga_evictions_chain_heavy.load(std::memory_order_relaxed);
            snapshot.mga_evictions_gc_candidate =
                stats_.mga_evictions_gc_candidate.load(std::memory_order_relaxed);
            snapshot.mga_evictions_scan_probation =
                stats_.mga_evictions_scan_probation.load(std::memory_order_relaxed);
            snapshot.mga_evictions_index_churn =
                stats_.mga_evictions_index_churn.load(std::memory_order_relaxed);
            snapshot.mga_evictions_temp_work =
                stats_.mga_evictions_temp_work.load(std::memory_order_relaxed);
            snapshot.mga_gc_handoff_offers =
                stats_.mga_gc_handoff_offers.load(std::memory_order_relaxed);
            snapshot.mga_scan_probation_churn =
                stats_.mga_scan_probation_churn.load(std::memory_order_relaxed);
            snapshot.mga_chain_heavy_hits =
                stats_.mga_chain_heavy_hits.load(std::memory_order_relaxed);
            snapshot.mga_protected_set_collapse_events =
                stats_.mga_protected_set_collapse_events.load(std::memory_order_relaxed);
            snapshot.mga_admission_promotions =
                stats_.mga_admission_promotions.load(std::memory_order_relaxed);
            snapshot.mga_residency_demotions =
                stats_.mga_residency_demotions.load(std::memory_order_relaxed);
            snapshot.mga_ghost_history_hits =
                stats_.mga_ghost_history_hits.load(std::memory_order_relaxed);
            snapshot.mga_ghost_history_entries = ghost_history_.size();
            snapshot.mga_commit_fence_backlog =
                commit_fence_backlog_.load(std::memory_order_relaxed);
            snapshot.foreground_help_backlog_pages = snapshot.mga_commit_fence_backlog;

            for (const auto &frame : frames_)
            {
                if (frame.gpid == INVALID_GPID)
                {
                    continue;
                }

                if (frame.is_dirty.load(std::memory_order_relaxed))
                {
                    const uint64_t dirty_generation =
                        frame.dirty_generation.load(std::memory_order_relaxed);
                    if (dirty_generation != 0)
                    {
                        if (snapshot.dirty_generation_low_watermark == 0 ||
                            dirty_generation < snapshot.dirty_generation_low_watermark)
                        {
                            snapshot.dirty_generation_low_watermark = dirty_generation;
                        }
                        if (dirty_generation > snapshot.dirty_generation_high_watermark)
                        {
                            snapshot.dirty_generation_high_watermark = dirty_generation;
                        }
                    }

                    if (frame.checkpoint_target_generation.load(std::memory_order_relaxed) != 0)
                    {
                        snapshot.checkpoint_bound_dirty_pages++;
                    }

                    switch (static_cast<WritebackQueueState>(
                        frame.writeback_queue_state.load(std::memory_order_relaxed)))
                    {
                        case WritebackQueueState::FOREGROUND_HELP:
                            snapshot.queue_depth_foreground_help++;
                            break;
                        case WritebackQueueState::BACKGROUND_AGE:
                            snapshot.queue_depth_background_age++;
                            break;
                        case WritebackQueueState::CHECKPOINT:
                            snapshot.queue_depth_checkpoint++;
                            break;
                        case WritebackQueueState::METADATA_PRIORITY:
                            snapshot.queue_depth_metadata_priority++;
                            break;
                        case WritebackQueueState::WRITE_COMBINE:
                            snapshot.queue_depth_write_combine++;
                            break;
                        case WritebackQueueState::REPAIR_RETRY:
                            snapshot.queue_depth_repair_retry++;
                            break;
                        case WritebackQueueState::NONE:
                        default:
                            break;
                    }
                }

                if (frame.speculative_prefetch.load(std::memory_order_relaxed))
                {
                    snapshot.prefetch_debt_pages++;
                    if (static_cast<PolicyDomain>(
                            frame.policy_domain.load(std::memory_order_relaxed)) ==
                        PolicyDomain::ScanBulkRing)
                    {
                        snapshot.prefetch_scan_debt_pages++;
                    }
                }

                switch (static_cast<MgaPageClass>(
                    frame.mga_page_class.load(std::memory_order_relaxed)))
                {
                    case MgaPageClass::TX_STATE:
                        snapshot.mga_frames_tx_state++;
                        break;
                    case MgaPageClass::SYSTEM_META:
                        snapshot.mga_frames_system_meta++;
                        break;
                    case MgaPageClass::INDEX_ROOT_INTERNAL:
                        snapshot.mga_frames_index_root_internal++;
                        break;
                    case MgaPageClass::VERSION_ROOT:
                        snapshot.mga_frames_version_root++;
                        break;
                    case MgaPageClass::CHAIN_HEAVY:
                        snapshot.mga_frames_chain_heavy++;
                        break;
                    case MgaPageClass::GC_CANDIDATE:
                        snapshot.mga_frames_gc_candidate++;
                        break;
                    case MgaPageClass::SCAN_PROBATION:
                        snapshot.mga_frames_scan_probation++;
                        break;
                    case MgaPageClass::INDEX_CHURN:
                        snapshot.mga_frames_index_churn++;
                        break;
                    case MgaPageClass::TEMP_WORK:
                        snapshot.mga_frames_temp_work++;
                        break;
                    case MgaPageClass::Generic:
                    default:
                        break;
                }
            }

            return snapshot;
        }

        // Stats debug logging (tests/diagnostics only)
        auto enableStatsDebug(const std::string &path, ErrorContext *ctx = nullptr) -> bool;
        void disableStatsDebug();

        // Increment page size mismatch counter (called by HeapPage when corruption detected)
        void incrementPageSizeMismatchCount()
        {
            // ISSUE 3.10 FIX: Use atomic increment (no lock needed)
            stats_.page_size_mismatches.fetch_add(1, std::memory_order_relaxed);
        }

    private:
        // Frame metadata
        struct Frame
        {
            // PHASE 1, TASK 1.2.3: Changed from uint32_t to GPID (64-bit)
            GPID gpid = INVALID_GPID;
            // CRITICAL FIX (CRITICAL-1): Make pin_count and usage_count atomic to prevent race conditions
            // Even though operations occur under mutex_, atomics provide memory ordering guarantees
            // and prevent torn reads/writes on all architectures
            std::atomic<uint32_t> pin_count{0};
            std::atomic<bool> is_dirty{false};
            std::atomic<uint32_t> usage_count{0}; // Clock Sweep algorithm: usage counter for eviction
            std::atomic<uint16_t> owner_partition{0};
            std::atomic<uint16_t> home_partition{0};
            std::atomic<uint32_t> object_id{0};
            std::atomic<uint8_t> mga_page_class{
                static_cast<uint8_t>(MgaPageClass::Generic)};
            std::atomic<uint8_t> policy_domain{
                static_cast<uint8_t>(PolicyDomain::HotOltp)};
            std::atomic<uint8_t> workload_class{
                static_cast<uint8_t>(WorkloadClass::Unspecified)};
            std::atomic<uint8_t> residency_tier{
                static_cast<uint8_t>(ResidencyTier::LegacyShared)};
            std::atomic<uint8_t> lifecycle_state{
                static_cast<uint8_t>(LifecycleState::Free)};
            std::atomic<uint8_t> dirty_state{
                static_cast<uint8_t>(DirtyState::Clean)};
            std::atomic<uint64_t> oldest_interesting_txid{0};
            std::atomic<uint64_t> prune_safe_horizon_hint{0};
            std::atomic<uint32_t> dead_version_bytes{0};
            std::atomic<uint16_t> chain_depth_hint{0};
            std::atomic<uint64_t> prefetch_session_key{0};
            std::atomic<uint64_t> last_gc_touch_generation{0};
            std::atomic<uint64_t> scan_probation_generation{0};
            std::atomic<uint64_t> state_generation{0};
            std::atomic<uint64_t> io_generation{0};
            std::atomic<uint64_t> dirty_generation{0};
            std::atomic<uint64_t> last_flush_generation{0};
            std::atomic<uint64_t> checkpoint_target_generation{0};
            std::atomic<uint64_t> admission_generation{0};
            std::atomic<uint64_t> last_touch_generation{0};
            std::atomic<uint64_t> temperature_generation{0};
            std::atomic<uint8_t> writeback_queue_state{
                static_cast<uint8_t>(WritebackQueueState::NONE)};
            std::atomic<bool> speculative_prefetch{false};
            std::atomic<bool> prefetch_consumed{false};
            std::atomic<bool> commit_fence_member{false};
            std::unique_ptr<uint8_t[]> data = nullptr;
            std::unique_ptr<std::mutex>
                content_mutex; // Protects page content from concurrent modifications

            static constexpr uint32_t MAX_USAGE_COUNT = 5; // Maximum usage count for Clock Sweep

            // Constructor to initialize mutex
            Frame() : content_mutex(std::make_unique<std::mutex>()) {}

            // CRITICAL FIX (CRITICAL-1): std::atomic is not copyable, so we need custom copy/move
            // Copy constructor: atomic values are copied with load/store
            Frame(const Frame& other)
                : gpid(other.gpid),
                  pin_count(other.pin_count.load(std::memory_order_relaxed)),
                  is_dirty(other.is_dirty.load(std::memory_order_relaxed)),
                  usage_count(other.usage_count.load(std::memory_order_relaxed)),
                  owner_partition(other.owner_partition.load(std::memory_order_relaxed)),
                  home_partition(other.home_partition.load(std::memory_order_relaxed)),
                  object_id(other.object_id.load(std::memory_order_relaxed)),
                  mga_page_class(other.mga_page_class.load(std::memory_order_relaxed)),
                  policy_domain(other.policy_domain.load(std::memory_order_relaxed)),
                  workload_class(other.workload_class.load(std::memory_order_relaxed)),
                  residency_tier(other.residency_tier.load(std::memory_order_relaxed)),
                  lifecycle_state(other.lifecycle_state.load(std::memory_order_relaxed)),
                  dirty_state(other.dirty_state.load(std::memory_order_relaxed)),
                  oldest_interesting_txid(
                      other.oldest_interesting_txid.load(std::memory_order_relaxed)),
                  prune_safe_horizon_hint(
                      other.prune_safe_horizon_hint.load(std::memory_order_relaxed)),
                  dead_version_bytes(
                      other.dead_version_bytes.load(std::memory_order_relaxed)),
                  chain_depth_hint(other.chain_depth_hint.load(std::memory_order_relaxed)),
                  prefetch_session_key(
                      other.prefetch_session_key.load(std::memory_order_relaxed)),
                  last_gc_touch_generation(
                      other.last_gc_touch_generation.load(std::memory_order_relaxed)),
                  scan_probation_generation(
                      other.scan_probation_generation.load(std::memory_order_relaxed)),
                  state_generation(
                      other.state_generation.load(std::memory_order_relaxed)),
                  io_generation(
                      other.io_generation.load(std::memory_order_relaxed)),
                  dirty_generation(
                      other.dirty_generation.load(std::memory_order_relaxed)),
                  last_flush_generation(
                      other.last_flush_generation.load(std::memory_order_relaxed)),
                  checkpoint_target_generation(
                      other.checkpoint_target_generation.load(std::memory_order_relaxed)),
                  admission_generation(
                      other.admission_generation.load(std::memory_order_relaxed)),
                  last_touch_generation(
                      other.last_touch_generation.load(std::memory_order_relaxed)),
                  temperature_generation(
                      other.temperature_generation.load(std::memory_order_relaxed)),
                  writeback_queue_state(
                      other.writeback_queue_state.load(std::memory_order_relaxed)),
                  speculative_prefetch(
                      other.speculative_prefetch.load(std::memory_order_relaxed)),
                  prefetch_consumed(
                      other.prefetch_consumed.load(std::memory_order_relaxed)),
                  commit_fence_member(
                      other.commit_fence_member.load(std::memory_order_relaxed)),
                  data(nullptr),
                  content_mutex(std::make_unique<std::mutex>())
            {
                // Note: data is not copied (unique_ptr), each frame gets its own data allocation
                // content_mutex is always a new mutex (unique_ptr)
            }

            // Copy assignment operator
            Frame& operator=(const Frame& other) {
                if (this != &other) {
                    gpid = other.gpid;
                    pin_count.store(other.pin_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    is_dirty.store(other.is_dirty.load(std::memory_order_relaxed),
                                   std::memory_order_relaxed);
                    usage_count.store(other.usage_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    owner_partition.store(
                        other.owner_partition.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    home_partition.store(
                        other.home_partition.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    object_id.store(other.object_id.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
                    mga_page_class.store(other.mga_page_class.load(std::memory_order_relaxed),
                                         std::memory_order_relaxed);
                    policy_domain.store(other.policy_domain.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
                    workload_class.store(other.workload_class.load(std::memory_order_relaxed),
                                         std::memory_order_relaxed);
                    residency_tier.store(other.residency_tier.load(std::memory_order_relaxed),
                                         std::memory_order_relaxed);
                    lifecycle_state.store(other.lifecycle_state.load(std::memory_order_relaxed),
                                          std::memory_order_relaxed);
                    dirty_state.store(other.dirty_state.load(std::memory_order_relaxed),
                                      std::memory_order_relaxed);
                    oldest_interesting_txid.store(
                        other.oldest_interesting_txid.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    prune_safe_horizon_hint.store(
                        other.prune_safe_horizon_hint.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    dead_version_bytes.store(
                        other.dead_version_bytes.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    chain_depth_hint.store(
                        other.chain_depth_hint.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    prefetch_session_key.store(
                        other.prefetch_session_key.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    last_gc_touch_generation.store(
                        other.last_gc_touch_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    scan_probation_generation.store(
                        other.scan_probation_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    state_generation.store(
                        other.state_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    io_generation.store(
                        other.io_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    dirty_generation.store(
                        other.dirty_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    last_flush_generation.store(
                        other.last_flush_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    checkpoint_target_generation.store(
                        other.checkpoint_target_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    admission_generation.store(
                        other.admission_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    last_touch_generation.store(
                        other.last_touch_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    temperature_generation.store(
                        other.temperature_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    writeback_queue_state.store(
                        other.writeback_queue_state.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    speculative_prefetch.store(
                        other.speculative_prefetch.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    prefetch_consumed.store(
                        other.prefetch_consumed.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    commit_fence_member.store(
                        other.commit_fence_member.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    // data and content_mutex remain unchanged (unique per frame)
                }
                return *this;
            }

            // Move constructor
            Frame(Frame&& other) noexcept
                : gpid(other.gpid),
                  pin_count(other.pin_count.load(std::memory_order_relaxed)),
                  is_dirty(other.is_dirty.load(std::memory_order_relaxed)),
                  usage_count(other.usage_count.load(std::memory_order_relaxed)),
                  owner_partition(other.owner_partition.load(std::memory_order_relaxed)),
                  home_partition(other.home_partition.load(std::memory_order_relaxed)),
                  object_id(other.object_id.load(std::memory_order_relaxed)),
                  mga_page_class(other.mga_page_class.load(std::memory_order_relaxed)),
                  policy_domain(other.policy_domain.load(std::memory_order_relaxed)),
                  workload_class(other.workload_class.load(std::memory_order_relaxed)),
                  residency_tier(other.residency_tier.load(std::memory_order_relaxed)),
                  lifecycle_state(other.lifecycle_state.load(std::memory_order_relaxed)),
                  dirty_state(other.dirty_state.load(std::memory_order_relaxed)),
                  oldest_interesting_txid(
                      other.oldest_interesting_txid.load(std::memory_order_relaxed)),
                  prune_safe_horizon_hint(
                      other.prune_safe_horizon_hint.load(std::memory_order_relaxed)),
                  dead_version_bytes(
                      other.dead_version_bytes.load(std::memory_order_relaxed)),
                  chain_depth_hint(other.chain_depth_hint.load(std::memory_order_relaxed)),
                  prefetch_session_key(
                      other.prefetch_session_key.load(std::memory_order_relaxed)),
                  last_gc_touch_generation(
                      other.last_gc_touch_generation.load(std::memory_order_relaxed)),
                  scan_probation_generation(
                      other.scan_probation_generation.load(std::memory_order_relaxed)),
                  state_generation(
                      other.state_generation.load(std::memory_order_relaxed)),
                  io_generation(
                      other.io_generation.load(std::memory_order_relaxed)),
                  dirty_generation(
                      other.dirty_generation.load(std::memory_order_relaxed)),
                  last_flush_generation(
                      other.last_flush_generation.load(std::memory_order_relaxed)),
                  checkpoint_target_generation(
                      other.checkpoint_target_generation.load(std::memory_order_relaxed)),
                  admission_generation(
                      other.admission_generation.load(std::memory_order_relaxed)),
                  last_touch_generation(
                      other.last_touch_generation.load(std::memory_order_relaxed)),
                  temperature_generation(
                      other.temperature_generation.load(std::memory_order_relaxed)),
                  writeback_queue_state(
                      other.writeback_queue_state.load(std::memory_order_relaxed)),
                  speculative_prefetch(
                      other.speculative_prefetch.load(std::memory_order_relaxed)),
                  prefetch_consumed(
                      other.prefetch_consumed.load(std::memory_order_relaxed)),
                  commit_fence_member(
                      other.commit_fence_member.load(std::memory_order_relaxed)),
                  data(std::move(other.data)),
                  content_mutex(std::move(other.content_mutex))
            {
                other.gpid = INVALID_GPID;
                other.pin_count.store(0, std::memory_order_relaxed);
                other.is_dirty.store(false, std::memory_order_relaxed);
                other.usage_count.store(0, std::memory_order_relaxed);
                other.owner_partition.store(0, std::memory_order_relaxed);
                other.home_partition.store(0, std::memory_order_relaxed);
                other.object_id.store(0, std::memory_order_relaxed);
                other.mga_page_class.store(static_cast<uint8_t>(MgaPageClass::Generic),
                                           std::memory_order_relaxed);
                other.policy_domain.store(static_cast<uint8_t>(PolicyDomain::HotOltp),
                                          std::memory_order_relaxed);
                other.workload_class.store(static_cast<uint8_t>(WorkloadClass::Unspecified),
                                           std::memory_order_relaxed);
                other.residency_tier.store(static_cast<uint8_t>(ResidencyTier::LegacyShared),
                                           std::memory_order_relaxed);
                other.lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Free),
                                            std::memory_order_relaxed);
                other.dirty_state.store(static_cast<uint8_t>(DirtyState::Clean),
                                        std::memory_order_relaxed);
                other.oldest_interesting_txid.store(0, std::memory_order_relaxed);
                other.prune_safe_horizon_hint.store(0, std::memory_order_relaxed);
                other.dead_version_bytes.store(0, std::memory_order_relaxed);
                other.chain_depth_hint.store(0, std::memory_order_relaxed);
                other.prefetch_session_key.store(0, std::memory_order_relaxed);
                other.last_gc_touch_generation.store(0, std::memory_order_relaxed);
                other.scan_probation_generation.store(0, std::memory_order_relaxed);
                other.state_generation.store(0, std::memory_order_relaxed);
                other.io_generation.store(0, std::memory_order_relaxed);
                other.dirty_generation.store(0, std::memory_order_relaxed);
                other.last_flush_generation.store(0, std::memory_order_relaxed);
                other.checkpoint_target_generation.store(0, std::memory_order_relaxed);
                other.admission_generation.store(0, std::memory_order_relaxed);
                other.last_touch_generation.store(0, std::memory_order_relaxed);
                other.temperature_generation.store(0, std::memory_order_relaxed);
                other.writeback_queue_state.store(
                    static_cast<uint8_t>(WritebackQueueState::NONE),
                    std::memory_order_relaxed);
                other.speculative_prefetch.store(false, std::memory_order_relaxed);
                other.prefetch_consumed.store(false, std::memory_order_relaxed);
                other.commit_fence_member.store(false, std::memory_order_relaxed);
            }

            // Move assignment operator
            Frame& operator=(Frame&& other) noexcept {
                if (this != &other) {
                    gpid = other.gpid;
                    pin_count.store(other.pin_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    is_dirty.store(other.is_dirty.load(std::memory_order_relaxed),
                                   std::memory_order_relaxed);
                    usage_count.store(other.usage_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    owner_partition.store(
                        other.owner_partition.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    home_partition.store(
                        other.home_partition.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    object_id.store(other.object_id.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
                    mga_page_class.store(other.mga_page_class.load(std::memory_order_relaxed),
                                         std::memory_order_relaxed);
                    policy_domain.store(other.policy_domain.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
                    workload_class.store(other.workload_class.load(std::memory_order_relaxed),
                                         std::memory_order_relaxed);
                    residency_tier.store(other.residency_tier.load(std::memory_order_relaxed),
                                         std::memory_order_relaxed);
                    lifecycle_state.store(other.lifecycle_state.load(std::memory_order_relaxed),
                                          std::memory_order_relaxed);
                    dirty_state.store(other.dirty_state.load(std::memory_order_relaxed),
                                      std::memory_order_relaxed);
                    oldest_interesting_txid.store(
                        other.oldest_interesting_txid.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    prune_safe_horizon_hint.store(
                        other.prune_safe_horizon_hint.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    dead_version_bytes.store(
                        other.dead_version_bytes.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    chain_depth_hint.store(
                        other.chain_depth_hint.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    prefetch_session_key.store(
                        other.prefetch_session_key.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    last_gc_touch_generation.store(
                        other.last_gc_touch_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    scan_probation_generation.store(
                        other.scan_probation_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    state_generation.store(
                        other.state_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    io_generation.store(
                        other.io_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    dirty_generation.store(
                        other.dirty_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    last_flush_generation.store(
                        other.last_flush_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    checkpoint_target_generation.store(
                        other.checkpoint_target_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    admission_generation.store(
                        other.admission_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    last_touch_generation.store(
                        other.last_touch_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    temperature_generation.store(
                        other.temperature_generation.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    writeback_queue_state.store(
                        other.writeback_queue_state.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    speculative_prefetch.store(
                        other.speculative_prefetch.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    prefetch_consumed.store(
                        other.prefetch_consumed.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    commit_fence_member.store(
                        other.commit_fence_member.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
                    data = std::move(other.data);
                    content_mutex = std::move(other.content_mutex);

                    other.gpid = INVALID_GPID;
                    other.pin_count.store(0, std::memory_order_relaxed);
                    other.is_dirty.store(false, std::memory_order_relaxed);
                    other.usage_count.store(0, std::memory_order_relaxed);
                    other.owner_partition.store(0, std::memory_order_relaxed);
                    other.home_partition.store(0, std::memory_order_relaxed);
                    other.object_id.store(0, std::memory_order_relaxed);
                    other.mga_page_class.store(static_cast<uint8_t>(MgaPageClass::Generic),
                                               std::memory_order_relaxed);
                    other.policy_domain.store(static_cast<uint8_t>(PolicyDomain::HotOltp),
                                              std::memory_order_relaxed);
                    other.workload_class.store(
                        static_cast<uint8_t>(WorkloadClass::Unspecified),
                        std::memory_order_relaxed);
                    other.residency_tier.store(
                        static_cast<uint8_t>(ResidencyTier::LegacyShared),
                        std::memory_order_relaxed);
                    other.lifecycle_state.store(static_cast<uint8_t>(LifecycleState::Free),
                                                std::memory_order_relaxed);
                    other.dirty_state.store(static_cast<uint8_t>(DirtyState::Clean),
                                            std::memory_order_relaxed);
                    other.oldest_interesting_txid.store(0, std::memory_order_relaxed);
                    other.prune_safe_horizon_hint.store(0, std::memory_order_relaxed);
                    other.dead_version_bytes.store(0, std::memory_order_relaxed);
                    other.chain_depth_hint.store(0, std::memory_order_relaxed);
                    other.prefetch_session_key.store(0, std::memory_order_relaxed);
                    other.last_gc_touch_generation.store(0, std::memory_order_relaxed);
                    other.scan_probation_generation.store(0, std::memory_order_relaxed);
                    other.state_generation.store(0, std::memory_order_relaxed);
                    other.io_generation.store(0, std::memory_order_relaxed);
                    other.dirty_generation.store(0, std::memory_order_relaxed);
                    other.last_flush_generation.store(0, std::memory_order_relaxed);
                    other.checkpoint_target_generation.store(0, std::memory_order_relaxed);
                    other.admission_generation.store(0, std::memory_order_relaxed);
                    other.last_touch_generation.store(0, std::memory_order_relaxed);
                    other.temperature_generation.store(0, std::memory_order_relaxed);
                    other.writeback_queue_state.store(
                        static_cast<uint8_t>(WritebackQueueState::NONE),
                        std::memory_order_relaxed);
                    other.speculative_prefetch.store(false, std::memory_order_relaxed);
                    other.prefetch_consumed.store(false, std::memory_order_relaxed);
                    other.commit_fence_member.store(false, std::memory_order_relaxed);
                }
                return *this;
            }
        };

        void resetFrameScaffolding(Frame &frame);

        void logStatsEvent(const char *event, GPID gpid);

        // ISSUE 3.10 FIX: Internal Stats structure with atomic types for thread-safe updates
        struct Stats
        {
            std::atomic<uint64_t> hits{0};      // Cache hits
            std::atomic<uint64_t> misses{0};    // Cache misses
            std::atomic<uint64_t> evictions{0}; // Pages evicted
            std::atomic<uint64_t> flushes{0};   // Pages flushed

            // READ ONLY transaction optimizations (Phase 3)
            std::atomic<uint64_t> evictions_clean{0}; // Clean pages evicted (read-only benefit)
            std::atomic<uint64_t> evictions_dirty{0}; // Dirty pages evicted (requires flush)

            // Corruption detection (MED-005)
            std::atomic<uint64_t> page_size_mismatches{0}; // Page size mismatches corrected

            // Clock Sweep algorithm statistics (Issue 2.14)
            std::atomic<uint64_t> clock_sweeps{0};      // Total clock sweeps performed
            std::atomic<uint64_t> clock_hand_resets{0}; // Times clock hand wrapped around

            // Background writer statistics (Issue 2.20)
            std::atomic<uint64_t> bgwriter_runs{0};          // Background writer cycles executed
            std::atomic<uint64_t> bgwriter_pages_written{0}; // Total pages written by background writer
            std::atomic<uint64_t> bgwriter_maxwritten{0};    // Times bgwriter hit max_pages limit
            std::atomic<uint64_t> checkpoint_flushes{0};     // Pages flushed during checkpoints
            std::atomic<uint64_t> prefetch_pages_total{0};
            std::atomic<uint64_t> prefetch_pages_useful{0};
            std::atomic<uint64_t> prefetch_pages_unused_evicted{0};
            std::atomic<uint64_t> prefetch_cancelled_pages{0};
            std::atomic<uint64_t> fairness_session_budget_breaches{0};
            std::atomic<uint64_t> fairness_object_budget_breaches{0};
            std::atomic<uint64_t> thrash_policy_shift_count{0};
            std::atomic<uint64_t> mga_evictions_tx_state{0};
            std::atomic<uint64_t> mga_evictions_system_meta{0};
            std::atomic<uint64_t> mga_evictions_index_root_internal{0};
            std::atomic<uint64_t> mga_evictions_version_root{0};
            std::atomic<uint64_t> mga_evictions_chain_heavy{0};
            std::atomic<uint64_t> mga_evictions_gc_candidate{0};
            std::atomic<uint64_t> mga_evictions_scan_probation{0};
            std::atomic<uint64_t> mga_evictions_index_churn{0};
            std::atomic<uint64_t> mga_evictions_temp_work{0};
            std::atomic<uint64_t> mga_gc_handoff_offers{0};
            std::atomic<uint64_t> mga_scan_probation_churn{0};
            std::atomic<uint64_t> mga_chain_heavy_hits{0};
            std::atomic<uint64_t> mga_protected_set_collapse_events{0};
            std::atomic<uint64_t> mga_admission_promotions{0};
            std::atomic<uint64_t> mga_residency_demotions{0};
            std::atomic<uint64_t> mga_ghost_history_hits{0};

            // Note: dirty_ratio values are read/written only while holding mutex_, so they don't need atomics
            double dirty_ratio_current = 0.0;    // Current dirty page ratio (0.0-1.0)
            double dirty_ratio_max = 0.0;        // Maximum dirty ratio since last reset
        };

        Database *db_;                                      // Database instance
        Config config_;                                     // Configuration
        std::vector<Frame> frames_;                         // Buffer pool frames
        std::list<uint32_t> lru_list_;                      // LRU list (frame indices)

        // P2-1: Page Table Lock Partitioning
        // Split page table into NUM_PAGE_TABLE_PARTITIONS buckets with separate locks
        // This reduces contention when multiple threads access different pages
        static constexpr size_t NUM_PAGE_TABLE_PARTITIONS = 64;

        struct PageTablePartition {
            std::unordered_map<GPID, uint32_t> table;       // gpid -> frame_index
            mutable std::mutex mutex;                        // Per-partition lock
        };
        std::array<PageTablePartition, NUM_PAGE_TABLE_PARTITIONS> page_table_partitions_;

        struct OwnershipPartition
        {
            std::vector<uint32_t> owned_frames;
            std::deque<uint32_t> free_frames;
            uint32_t victim_cursor = 0;
            mutable std::mutex mutex;
        };
        std::array<OwnershipPartition, NUM_PAGE_TABLE_PARTITIONS> ownership_partitions_;

        // Hash function to map GPID to partition index
        static size_t getPartitionIndex(GPID gpid) {
            // Use simple modulo hashing - GPID is already well-distributed
            return static_cast<size_t>(gpid) % NUM_PAGE_TABLE_PARTITIONS;
        }
        static size_t getOwnershipHomePartition(uint32_t frame_index)
        {
            return static_cast<size_t>(frame_index) % NUM_PAGE_TABLE_PARTITIONS;
        }

        Stats stats_;                                       // Statistics (atomic counters)
        mutable std::mutex mutex_;                          // Global mutex for frame allocation/eviction

        std::mutex stats_debug_mutex_;
        FILE *stats_debug_fp_ = nullptr;
        std::atomic<bool> stats_debug_enabled_{false};
        uint64_t stats_debug_seq_ = 0;

        // Clock Sweep algorithm state
        uint32_t clock_hand_ = 0;                           // Current position of clock hand

        struct RingBuffer
        {
            std::vector<uint32_t> frames;
            uint32_t next = 0;

            void reset(uint32_t size)
            {
                frames.assign(size, UINT32_MAX);
                next = 0;
            }
        };

        RingBuffer seq_ring_;
        RingBuffer vacuum_ring_;
        RingBuffer bulk_write_ring_;

        enum class GhostEvictionReason : uint8_t
        {
            RingChurn = 0,
            ProbationaryAging = 1,
            ProtectedDemotion = 2,
            DirtyForeground = 3,
            EmergencyCollapse = 4
        };

        struct GhostEntry
        {
            GPID gpid = INVALID_GPID;
            PolicyDomain domain = PolicyDomain::HotOltp;
            MgaPageClass page_class = MgaPageClass::Generic;
            ResidencyTier residency_tier = ResidencyTier::Probationary;
            uint64_t eviction_generation = 0;
            GhostEvictionReason reason = GhostEvictionReason::ProbationaryAging;
        };

        using DomainCounterArray =
            std::array<uint32_t, static_cast<size_t>(PolicyDomain::Count)>;

        // P2-2: Atomic dirty page counter for O(1) getDirtyPageCount()
        // Updated atomically whenever is_dirty flag changes on any frame
        std::atomic<uint32_t> dirty_page_count_{0};
        std::mutex dirty_tracking_mutex_;
        std::unordered_map<GPID, uint64_t> dirty_checkpoint_candidates_;
        std::unordered_set<GPID> checkpoint_marker_candidates_;
        std::deque<GhostEntry> ghost_history_;

        // Background writer state (Issue 2.20)
        std::unique_ptr<std::thread> bgwriter_thread_;      // Background writer thread
        bool bgwriter_shutdown_ = false;                    // Shutdown flag for background writer
        std::condition_variable bgwriter_cv_;               // Condition variable for bgwriter wake-up
        std::mutex bgwriter_mutex_;                         // Mutex for background writer coordination
        ScratchBirdMetrics *metrics_{nullptr};              // Telemetry wiring (optional)
        std::atomic<uint64_t> mga_scan_generation_{0};
        std::atomic<uint64_t> mga_gc_touch_generation_{0};
        std::atomic<uint64_t> residency_generation_clock_{0};
        std::atomic<uint64_t> dirty_generation_clock_{0};
        std::atomic<uint64_t> commit_fence_backlog_{0};
        std::atomic<uint32_t> commit_fence_depth_{0};
        std::atomic<uint8_t> thrash_detector_state_{
            static_cast<uint8_t>(ThrashDetectorState::None)};
        std::array<std::atomic<uint64_t>, static_cast<size_t>(PolicyDomain::Count)>
            domain_reservation_breach_counts_{};
        std::array<std::atomic<uint64_t>, static_cast<size_t>(PolicyDomain::Count)>
            domain_emergency_breach_counts_{};

        // Helper methods
        auto evictPage(uint32_t &evicted_frame, ErrorContext *ctx) -> Status;
        auto evictSpecificFrame(uint32_t frame_index, ErrorContext *ctx) -> Status;
        // PHASE 1, TASK 1.2.3: Changed page_id to gpid (GPID is 64-bit)
        auto readPageFromDisk(GPID gpid, uint8_t *buffer, ErrorContext *ctx) -> Status;
        auto writePageToDisk(uint32_t frame_index,
                             ErrorContext *ctx,
                             WritebackQueueState queue_state,
                             bool checkpoint_flush = false) -> Status;
        void updateLru(uint32_t frame_index);
        void insertLruMidpoint(uint32_t frame_index);
        void initializeRingBuffers();
        RingBuffer* getRingBuffer(AccessStrategy strategy);
        uint32_t nextRingSlot(RingBuffer &ring);
        static auto classifyPageType(const uint8_t *buffer,
                                     uint32_t page_size,
                                     WorkloadClass workload_class) -> MgaPageClass;
        static auto defaultWorkloadClass(AccessStrategy strategy,
                                         bool prefetch_request = false) -> WorkloadClass;
        static auto classifyPolicyDomain(const uint8_t *buffer,
                                         uint32_t page_size,
                                         MgaPageClass page_class,
                                         WorkloadClass workload_class) -> PolicyDomain;
        static auto classifyResidencyTier(WorkloadClass workload_class,
                                          MgaPageClass page_class) -> ResidencyTier;
        static auto evictionRankForClass(MgaPageClass page_class) -> uint32_t;
        static auto evictionRankForTier(ResidencyTier tier) -> uint32_t;
        static auto isVersionUndoClass(MgaPageClass page_class) -> bool;
        static auto isHardProtectedClass(MgaPageClass page_class) -> bool;
        static auto isHardReservedDomain(PolicyDomain domain) -> bool;
        static auto isDirectProtectClass(MgaPageClass page_class) -> bool;
        static auto isDirectProtectSystemMetaPageType(uint16_t page_type) -> bool;
        static auto isSystemMetaPageType(uint16_t page_type) -> bool;
        static auto isIndexRootInternalPageType(uint16_t page_type) -> bool;
        static auto isIndexLikePageType(uint16_t page_type) -> bool;
        static auto classifyWritebackQueueState(const Frame &frame) -> WritebackQueueState;
        static auto objectIdForBuffer(const uint8_t *buffer, uint32_t page_size) -> uint32_t;
        auto protectedFrameBudget() const -> uint32_t;
        auto objectProtectionBudget() const -> uint32_t;
        auto sessionPrefetchBudget() const -> uint32_t;
        auto prefetchPressureBudget() const -> uint32_t;
        auto currentProtectedFrameCount() const -> uint32_t;
        auto currentProtectedFrameCountForObject(uint32_t object_id,
                                                uint32_t excluding_frame) const -> uint32_t;
        auto collectDomainResidentCountsLocked() const -> DomainCounterArray;
        auto domainBorrowedPagesLocked(PolicyDomain domain,
                                       const DomainCounterArray &resident_counts) const
            -> uint32_t;
        auto canEvictFromDomainLocked(PolicyDomain domain,
                                      const DomainCounterArray &resident_counts) const -> bool;
        auto domainEvictionPriorityLocked(PolicyDomain domain,
                                         const DomainCounterArray &resident_counts) const
            -> uint32_t;
        auto consumeGhostHistory(GPID gpid) -> bool;
        void recordGhostHistory(GPID gpid,
                                PolicyDomain domain,
                                MgaPageClass page_class,
                                ResidencyTier residency_tier,
                                uint64_t eviction_generation,
                                GhostEvictionReason reason);
        void initializeOwnershipPartitionsLocked();
        void clearOwnershipPartitionsLocked();
        void resetFrameIdentity(Frame &frame, size_t owner_partition, bool keep_home_partition);
        void stageFrameForOwnership(Frame &frame, size_t owner_partition, LifecycleState lifecycle_state);
        void releaseFrameToOwnershipFreeListLocked(uint32_t frame_index, size_t owner_partition);
        void transferFrameOwnershipLocked(uint32_t frame_index,
                                          size_t from_partition,
                                          size_t to_partition);
        auto tryClaimFreeFrameLocked(size_t owner_partition, uint32_t &frame_index) -> bool;
        auto tryStealFreeFrameLocked(size_t owner_partition, uint32_t &frame_index) -> bool;
        auto selectOwnedVictimFrameLocked(size_t owner_partition,
                                          uint32_t &candidate_frame) -> bool;
        auto tryClaimOwnedVictimFrameLocked(size_t owner_partition,
                                            uint32_t &frame_index,
                                            ErrorContext *ctx) -> Status;
        auto tryStealOwnedVictimFrameLocked(size_t owner_partition,
                                            uint32_t &frame_index,
                                            ErrorContext *ctx) -> Status;
        auto claimFrameForSegmentedMiss(size_t owner_partition,
                                        uint32_t &frame_index,
                                        ErrorContext *ctx) -> Status;
        void applyAutomaticMgaClassification(uint32_t frame_index,
                                            WorkloadClass workload_class);
        void refreshFrameObjectId(uint32_t frame_index);
        void markFramePrefetched(uint32_t frame_index, uint64_t session_key);
        void consumePrefetchDebt(uint32_t frame_index, WorkloadClass workload_class);
        void recordUnusedPrefetchEviction(const Frame &frame);
        auto currentOutstandingPrefetchDebtPages(uint64_t session_key = 0) const -> uint32_t;
        auto currentOutstandingPrefetchDebtPages(PolicyDomain domain) const -> uint32_t;
        auto evaluateThrashState(uint64_t session_key) const -> ThrashDetectorState;
        void publishThrashState(ThrashDetectorState state);
        auto canPromoteFrameToProtected(uint32_t frame_index) const -> bool;
        void noteFrameAccess(uint32_t frame_index,
                             WorkloadClass workload_class,
                             bool ghost_hit = false);
        void beginFrameWriteback(uint32_t frame_index,
                                 WritebackQueueState queue_state,
                                 uint64_t checkpoint_target_generation = 0);
        void markFrameWritebackFailure(uint32_t frame_index,
                                       WritebackQueueState queue_state);
        void recordMgaEviction(MgaPageClass page_class);
        void offerGcCandidate(uint32_t frame_index);
        void restoreCheckpointDebtForFrame(uint32_t frame_index);

        // Background writer methods (Issue 2.20)
        void backgroundWriterMain();                        // Background writer thread main loop
        void backgroundWriterFlush(ErrorContext *ctx);      // Perform one cycle of adaptive flushing
        double calculateDirtyRatio() const;                 // Calculate current dirty page ratio
        uint32_t getDirtyPageCount() const;                 // Get count of dirty pages
        void startBackgroundWriter();                       // Start background writer thread
        void stopBackgroundWriter();                        // Stop background writer thread
        void updateDirtyTelemetry();                        // Sync dirty page gauge
        void updatePoolTelemetry();                         // Sync pool size/total gauges
        bool tryMarkFrameDirty(uint32_t frame_index);       // Dirty transition false->true
        bool tryClearFrameDirty(uint32_t frame_index);      // Dirty transition true->false
        bool finishFrameWriteback(uint32_t frame_index,
                                  uint64_t flushed_generation,
                                  WritebackQueueState dirty_queue_state);
        uint64_t publishDirtyGeneration(uint32_t frame_index);
    };

} // namespace scratchbird::core
