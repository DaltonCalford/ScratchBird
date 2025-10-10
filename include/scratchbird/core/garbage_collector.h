#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include <cstdint>
#include <atomic>
#include <thread>
#include <mutex>
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

        GCStatistics()
            : tuples_removed(0)
            , pages_cleaned(0)
            , cooperative_runs(0)
            , background_runs(0)
            , last_background_time(0)
            , last_background_duration_ms(0)
            , dirty_page_count(0)
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

        // Dirty page tracking
        mutable std::mutex dirty_pages_mutex_;
        std::unordered_set<uint32_t> dirty_pages_;

        // Statistics
        mutable std::mutex stats_mutex_;
        GCStatistics stats_;

        // Internal methods
        void backgroundGCLoop();
        uint64_t cleanPage(uint32_t page_id, ErrorContext* ctx);
        bool isTupleGarbage(uint64_t xmax, uint64_t oit);
        void readConfiguration();

        // Statistics helpers
        void updateCooperativeStats(uint64_t tuples_removed, uint64_t pages_cleaned);
        void updateBackgroundStats(uint64_t tuples_removed, uint64_t pages_cleaned, uint64_t duration_ms);
        void wakeBackgroundThread();

        // Rate limiting for cooperative GC
        bool shouldRunCooperativeGC();
    };

} // namespace scratchbird::core
