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
#include "scratchbird/core/tid.h"
#include "scratchbird/core/storage_engine.h"  // For ID type (UuidV7Bytes)
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <map>
#include <vector>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class TransactionManager;

    // Garbage collection policy
    enum class GCPolicy
    {
        COOPERATIVE, // Only cooperative GC during page reads
        BACKGROUND,  // Only background GC thread
        COMBINED     // Both cooperative and background (default)
    };

    // GC statistics
    struct GCStatistics
    {
        uint64_t tuples_removed;              // Total tuples removed
        uint64_t pages_cleaned;               // Pages cleaned
        uint64_t cooperative_runs;            // Cooperative GC executions
        uint64_t background_runs;             // Background GC passes
        uint64_t last_background_time;        // Timestamp of last background run (microseconds)
        uint64_t last_background_duration_ms; // Duration of last background run
        uint64_t dirty_page_count;            // Current dirty pages
        uint64_t space_reclaimed_bytes;       // Total bytes reclaimed

        // Enhanced metrics - Duration histogram (background GC runs)
        uint64_t duration_0_10ms;      // Runs that took 0-10ms
        uint64_t duration_10_50ms;     // Runs that took 10-50ms
        uint64_t duration_50_100ms;    // Runs that took 50-100ms
        uint64_t duration_100_500ms;   // Runs that took 100-500ms
        uint64_t duration_500_1000ms;  // Runs that took 500-1000ms
        uint64_t duration_1000ms_plus; // Runs that took 1000ms+

        // Enhanced metrics - Page efficiency
        uint64_t pages_with_no_garbage;           // Pages scanned with no garbage found
        uint64_t max_space_reclaimed_single_page; // Max bytes reclaimed from one page

        // Enhanced metrics - Garbage accumulation
        uint64_t total_dirty_pages_marked; // Total pages marked dirty (all time)

        // Current tuning parameters (for monitoring)
        uint32_t current_cooperative_rate;       // Current cooperative GC rate
        uint64_t current_background_interval_ms; // Current background interval

        GCStatistics()
            : tuples_removed(0), pages_cleaned(0), cooperative_runs(0), background_runs(0),
              last_background_time(0), last_background_duration_ms(0), dirty_page_count(0),
              space_reclaimed_bytes(0), duration_0_10ms(0), duration_10_50ms(0),
              duration_50_100ms(0), duration_100_500ms(0), duration_500_1000ms(0),
              duration_1000ms_plus(0), pages_with_no_garbage(0), max_space_reclaimed_single_page(0),
              total_dirty_pages_marked(0), current_cooperative_rate(100),
              current_background_interval_ms(5000)
        {
        }
    };

    // Dirty page information for priority-based cleaning
    struct DirtyPageInfo
    {
        uint32_t page_id;
        double priority;           // Higher = more urgent (estimated garbage density)
        uint64_t marked_timestamp; // When page was marked dirty (microseconds)
        uint32_t mark_count;       // Number of times marked dirty (indicates churn)

        DirtyPageInfo() : page_id(0), priority(0.0), marked_timestamp(0), mark_count(0) {}

        DirtyPageInfo(uint32_t page_id, double priority, uint64_t timestamp)
            : page_id(page_id), priority(priority), marked_timestamp(timestamp), mark_count(1)
        {
        }

        // Comparator for priority queue (higher priority first)
        bool operator<(const DirtyPageInfo &other) const
        {
            // First compare priority (higher is better)
            if (priority != other.priority)
            {
                return priority > other.priority; // Reverse for max-heap
            }
            // If priority equal, older pages first
            return marked_timestamp < other.marked_timestamp;
        }
    };

    // Garbage Collector
    // Manages both cooperative and background garbage collection
    class GarbageCollector
    {
    public:
        // Constructor - does not take ownership of Database
        explicit GarbageCollector(Database *db);

        // Destructor - stops background thread if running
        ~GarbageCollector();

        // Initialize garbage collector
        // Must be called after Database is fully initialized
        Status initialize(ErrorContext *ctx = nullptr);

        // Cooperative GC - called during page reads
        // Opportunistically cleans dead tuples on accessed pages
        void processPageCooperative(uint32_t page_id, ErrorContext *ctx = nullptr);

        // Background GC control
        Status startBackgroundGC(ErrorContext *ctx = nullptr);
        Status stopBackgroundGC(ErrorContext *ctx = nullptr);
        bool isBackgroundGCRunning() const;

        // Dirty page tracking
        void markPageDirty(uint32_t page_id);
        size_t getDirtyPageCount() const;

        // Policy management
        void setPolicy(GCPolicy policy);
        GCPolicy getPolicy() const;

        // Enable/disable GC
        void enable();
        void disable();
        bool isEnabled() const;

        // Statistics
        GCStatistics getStatistics() const;

        // Adaptive tuning
        void setAdaptiveTuning(bool enabled);
        bool isAdaptiveTuningEnabled() const;

        // Integration with sweep
        void notifySweepComplete(uint64_t old_oit, uint64_t new_oit);

        // Phase 4: TOAST Garbage Collection
        // Detect orphaned TOAST chunks (referenced by no heap tuples)
        Status detectOrphanedToastChunks(const ID& toast_table_id,
                                         std::unordered_set<uint32_t>* orphaned_value_ids,
                                         ErrorContext* ctx = nullptr);

        // Clean orphaned TOAST chunks
        Status cleanOrphanedToastChunks(const ID& toast_table_id,
                                        const std::unordered_set<uint32_t>& orphaned_value_ids,
                                        uint64_t* chunks_deleted,
                                        ErrorContext* ctx = nullptr);

        // TIP-based TOAST garbage collection
        // Cleans chunks where xmax is committed (via TIP lookup)
        Status cleanToastChunksByTIP(const ID& toast_table_id,
                                     uint64_t* chunks_deleted,
                                     ErrorContext* ctx = nullptr);

    private:
        Database *db_;
        TransactionManager *txn_manager_;
        StorageEngine *storage_engine_;

        // GC policy and enabled state
        GCPolicy policy_;
        std::atomic<bool> enabled_;

        // Configuration parameters
        uint64_t background_interval_ms_; // Sleep interval for background GC (default: 5000ms)
        uint32_t cooperative_rate_;       // Cooperative GC rate: 1 in N page reads (default: 100)

        // Adaptive tuning configuration
        std::atomic<bool>
            adaptive_tuning_enabled_;    // Enable/disable adaptive tuning (default: true)
        uint32_t tuning_check_interval_; // Check every N background runs (default: 10)

        // Adaptive tuning bounds
        static constexpr uint32_t MIN_COOPERATIVE_RATE = 10;          // 1 in 10 page reads
        static constexpr uint32_t MAX_COOPERATIVE_RATE = 1000;        // 1 in 1000 page reads
        static constexpr uint64_t MIN_BACKGROUND_INTERVAL_MS = 1000;  // 1 second
        static constexpr uint64_t MAX_BACKGROUND_INTERVAL_MS = 60000; // 1 minute

        // Adaptive tuning thresholds
        static constexpr double HIGH_WASTE_THRESHOLD = 0.30;   // 30% wasted effort
        static constexpr double LOW_WASTE_THRESHOLD = 0.10;    // 10% wasted effort
        static constexpr uint64_t HIGH_DIRTY_THRESHOLD = 1000; // Many dirty pages
        static constexpr uint64_t LOW_DIRTY_THRESHOLD = 100;   // Few dirty pages

        // Background GC thread
        std::thread background_thread_;
        std::atomic<bool> background_running_;
        std::atomic<bool> shutdown_requested_;

        // Background GC wake mechanism
        std::mutex bg_wake_mutex_;
        std::condition_variable bg_wake_cv_;

        // Dirty page tracking (priority-based)
        mutable std::mutex dirty_pages_mutex_;
        std::map<uint32_t, DirtyPageInfo> dirty_pages_; // page_id -> info

        // Statistics
        mutable std::mutex stats_mutex_;
        GCStatistics stats_;

        // Internal methods
        void backgroundGCLoop();
        uint64_t cleanPage(uint32_t page_id, uint64_t *space_reclaimed_out, ErrorContext *ctx);
        bool isTupleGarbage(uint64_t xmax, uint64_t oit) const;
        void readConfiguration();

        // PHASE 2 TASK 2.6: Clean indexes for dead tuples
        // PHASE 1.5 TASK 1.5.3: Migrated to TID struct API
        // Called after prunePage and unpinPage (post-heap cleanup)
        // Returns number of index entries removed
        uint64_t cleanIndexes(uint32_t page_id, const ID &table_id,
                              const std::vector<TID> &dead_tids, ErrorContext *ctx);

        // Statistics helpers
        void updateCooperativeStats(uint64_t tuples_removed, uint64_t pages_cleaned,
                                    uint64_t space_reclaimed);
        void updateBackgroundStats(uint64_t tuples_removed, uint64_t pages_cleaned,
                                   uint64_t space_reclaimed, uint64_t duration_ms);
        void wakeBackgroundThread();

        // Rate limiting for cooperative GC
        bool shouldRunCooperativeGC() const;

        // Adaptive tuning
        void performAdaptiveTuning();

        // Priority calculation for dirty pages
        double calculatePagePriority(uint32_t mark_count, uint64_t age_microseconds) const;
    };

} // namespace scratchbird::core
