#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <unordered_set>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    class TransactionManager;
    class StorageEngine;

    // Garbage collection policy
    enum class GCPolicy
    {
        COOPERATIVE,  // Only cooperative GC during page reads
        BACKGROUND,   // Only background GC thread
        COMBINED      // Both cooperative and background (default)
    };

    // GC statistics
    struct GCStatistics
    {
        uint64_t tuples_removed;               // Total tuples removed
        uint64_t pages_cleaned;                // Pages cleaned
        uint64_t cooperative_runs;             // Cooperative GC executions
        uint64_t background_runs;              // Background GC passes
        uint64_t last_background_time;         // Timestamp of last background run (microseconds)
        uint64_t last_background_duration_ms;  // Duration of last background run
        uint64_t dirty_page_count;             // Current dirty pages
        uint64_t space_reclaimed_bytes;        // Total bytes reclaimed

        // Enhanced metrics - Duration histogram (background GC runs)
        uint64_t duration_0_10ms;              // Runs that took 0-10ms
        uint64_t duration_10_50ms;             // Runs that took 10-50ms
        uint64_t duration_50_100ms;            // Runs that took 50-100ms
        uint64_t duration_100_500ms;           // Runs that took 100-500ms
        uint64_t duration_500_1000ms;          // Runs that took 500-1000ms
        uint64_t duration_1000ms_plus;         // Runs that took 1000ms+

        // Enhanced metrics - Page efficiency
        uint64_t pages_with_no_garbage;        // Pages scanned with no garbage found
        uint64_t max_space_reclaimed_single_page;  // Max bytes reclaimed from one page

        // Enhanced metrics - Garbage accumulation
        uint64_t total_dirty_pages_marked;     // Total pages marked dirty (all time)

        GCStatistics()
            : tuples_removed(0)
            , pages_cleaned(0)
            , cooperative_runs(0)
            , background_runs(0)
            , last_background_time(0)
            , last_background_duration_ms(0)
            , dirty_page_count(0)
            , space_reclaimed_bytes(0)
            , duration_0_10ms(0)
            , duration_10_50ms(0)
            , duration_50_100ms(0)
            , duration_100_500ms(0)
            , duration_500_1000ms(0)
            , duration_1000ms_plus(0)
            , pages_with_no_garbage(0)
            , max_space_reclaimed_single_page(0)
            , total_dirty_pages_marked(0)
        {
        }
    };

    // Garbage Collector
    // Manages both cooperative and background garbage collection
    class GarbageCollector
    {
    public:
        // Constructor - does not take ownership of Database
        explicit GarbageCollector(Database* db);

        // Destructor - stops background thread if running
        ~GarbageCollector();

        // Initialize garbage collector
        // Must be called after Database is fully initialized
        Status initialize(ErrorContext* ctx = nullptr);

        // Cooperative GC - called during page reads
        // Opportunistically cleans dead tuples on accessed pages
        void processPageCooperative(uint32_t page_id, ErrorContext* ctx = nullptr);

        // Background GC control
        Status startBackgroundGC(ErrorContext* ctx = nullptr);
        Status stopBackgroundGC(ErrorContext* ctx = nullptr);
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

        // Integration with sweep
        void notifySweepComplete(uint64_t old_oit, uint64_t new_oit);

    private:
        Database* db_;
        TransactionManager* txn_manager_;
        StorageEngine* storage_engine_;

        // GC policy and enabled state
        GCPolicy policy_;
        std::atomic<bool> enabled_;

        // Configuration parameters
        uint64_t background_interval_ms_;  // Sleep interval for background GC (default: 5000ms)
        uint32_t cooperative_rate_;         // Cooperative GC rate: 1 in N page reads (default: 100)

        // Background GC thread
        std::thread background_thread_;
        std::atomic<bool> background_running_;
        std::atomic<bool> shutdown_requested_;

        // Background GC wake mechanism
        std::mutex bg_wake_mutex_;
        std::condition_variable bg_wake_cv_;

        // Dirty page tracking
        mutable std::mutex dirty_pages_mutex_;
        std::unordered_set<uint32_t> dirty_pages_;

        // Statistics
        mutable std::mutex stats_mutex_;
        GCStatistics stats_;

        // Internal methods
        void backgroundGCLoop();
        uint64_t cleanPage(uint32_t page_id, uint64_t* space_reclaimed_out, ErrorContext* ctx);
        bool isTupleGarbage(uint64_t xmax, uint64_t oit);
        void readConfiguration();

        // Statistics helpers
        void updateCooperativeStats(uint64_t tuples_removed, uint64_t pages_cleaned, uint64_t space_reclaimed);
        void updateBackgroundStats(uint64_t tuples_removed, uint64_t pages_cleaned, uint64_t space_reclaimed, uint64_t duration_ms);
        void wakeBackgroundThread();

        // Rate limiting for cooperative GC
        bool shouldRunCooperativeGC();
    };

} // namespace scratchbird::core
