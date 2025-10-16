#pragma once

#include <cstdint>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/error_context.h"

namespace scratchbird::core
{

    // Forward declarations
    class Database;

    /**
     * Buffer Pool - Manages in-memory page cache
     *
     * Implements a fixed-size buffer pool with LRU eviction.
     * Single-threaded for Alpha (mutex for future multi-threading).
     */
    class BufferPool
    {
    public:
        // Buffer pool configuration
        struct Config
        {
            uint32_t pool_size = 32;    // Number of pages in pool
            uint32_t page_size = 16384; // Page size in bytes

            // Adaptive flushing configuration (Issue 2.20)
            bool enable_background_writer = true;   // Enable background writer thread
            uint32_t bgwriter_delay_ms = 200;       // Delay between background writer runs (milliseconds)
            uint32_t bgwriter_max_pages = 100;      // Maximum pages to write per background writer cycle
            double dirty_ratio_low = 0.25;          // Start flushing when dirty ratio exceeds this (25%)
            double dirty_ratio_high = 0.50;         // Aggressive flushing when dirty ratio exceeds this (50%)
            double dirty_ratio_checkpoint = 0.75;   // Emergency flushing to prevent checkpoint storm (75%)
        };

        BufferPool(Database *db, const Config &config);
        ~BufferPool();

        // Initialize buffer pool
        auto initialize(ErrorContext *ctx = nullptr) -> Status;

        // Shutdown and flush all dirty pages
        auto shutdown(ErrorContext *ctx = nullptr) -> Status;

        /**
         * Pin a page in the buffer pool
         * @param page_id Page to pin
         * @param buffer Returns pointer to page data
         * @param ctx Error context
         * @return Status code
         */
        auto pinPage(uint32_t page_id, void **buffer, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Unpin a page
         * @param page_id Page to unpin
         * @param is_dirty True if page was modified
         * @param ctx Error context
         * @return Status code
         */
        auto unpinPage(uint32_t page_id, bool is_dirty, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Allocate a new page and pin it
         * @param page_id_out Returns the allocated page ID
         * @param buffer Returns pointer to page data
         * @param ctx Error context
         * @return Status code
         */
        auto allocatePage(uint32_t *page_id_out, void **buffer, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Mark a page as dirty (without unpinning)
         * @param page_id Page to mark dirty
         * @param ctx Error context
         * @return Status code
         */
        auto markDirty(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Flush a specific page if dirty
         */
        auto flushPage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Flush all dirty pages
         */
        auto flushAll(ErrorContext *ctx = nullptr) -> Status;

        /**
         * Lock a page for exclusive access (must be pinned first)
         * Caller must call unlockPage() when done
         * @param page_id Page ID to lock
         * @param ctx Error context
         * @return Status code
         */
        auto lockPage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

        /**
         * Unlock a previously locked page
         * @param page_id Page ID to unlock
         * @param ctx Error context
         * @return Status code
         */
        auto unlockPage(uint32_t page_id, ErrorContext *ctx = nullptr) -> Status;

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

            return snapshot;
        }

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
            uint32_t page_id = INVALID_PAGE_ID;
            uint32_t pin_count = 0;
            bool is_dirty = false;
            uint32_t usage_count = 0; // Clock Sweep algorithm: usage counter for eviction
            std::unique_ptr<uint8_t[]> data = nullptr;
            std::unique_ptr<std::mutex>
                content_mutex; // Protects page content from concurrent modifications

            static constexpr uint32_t INVALID_PAGE_ID = 0xFFFFFFFF;
            static constexpr uint32_t MAX_USAGE_COUNT = 5; // Maximum usage count for Clock Sweep

            // Constructor to initialize mutex
            Frame() : content_mutex(std::make_unique<std::mutex>()) {}
        };

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

            // Note: dirty_ratio values are read/written only while holding mutex_, so they don't need atomics
            double dirty_ratio_current = 0.0;    // Current dirty page ratio (0.0-1.0)
            double dirty_ratio_max = 0.0;        // Maximum dirty ratio since last reset
        };

        Database *db_;                                      // Database instance
        Config config_;                                     // Configuration
        std::vector<Frame> frames_;                         // Buffer pool frames
        std::list<uint32_t> lru_list_;                      // LRU list (frame indices)
        std::unordered_map<uint32_t, uint32_t> page_table_; // page_id -> frame_index
        Stats stats_;                                       // Statistics (atomic counters)
        mutable std::mutex mutex_;                          // Thread safety (future)

        // Clock Sweep algorithm state
        uint32_t clock_hand_ = 0;                           // Current position of clock hand

        // Background writer state (Issue 2.20)
        std::unique_ptr<std::thread> bgwriter_thread_;      // Background writer thread
        std::atomic<bool> bgwriter_shutdown_{false};        // Shutdown flag for background writer
        std::condition_variable bgwriter_cv_;               // Condition variable for bgwriter wake-up
        std::mutex bgwriter_mutex_;                         // Mutex for background writer coordination

        // Helper methods
        auto evictPage(uint32_t &evicted_frame, ErrorContext *ctx) -> Status;
        auto readPageFromDisk(uint32_t page_id, uint8_t *buffer, ErrorContext *ctx) -> Status;
        auto writePageToDisk(uint32_t page_id, const uint8_t *buffer, ErrorContext *ctx) -> Status;
        void updateLru(uint32_t frame_index);

        // Background writer methods (Issue 2.20)
        void backgroundWriterMain();                        // Background writer thread main loop
        void backgroundWriterFlush(ErrorContext *ctx);      // Perform one cycle of adaptive flushing
        double calculateDirtyRatio() const;                 // Calculate current dirty page ratio
        uint32_t getDirtyPageCount() const;                 // Get count of dirty pages
        void startBackgroundWriter();                       // Start background writer thread
        void stopBackgroundWriter();                        // Stop background writer thread
    };

} // namespace scratchbird::core
