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
#include <array>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <string>
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
     * Implements a fixed-size buffer pool with Clock Sweep eviction algorithm.
     * Clock Sweep provides better eviction decisions than pure LRU with O(1) complexity.
     * Thread-safe with mutex protection (despite comment saying single-threaded for Alpha).
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

        enum class PoolLayout
        {
            Single,
            HotCold,
            Tablespace
        };

        // Buffer pool configuration
        struct Config
        {
            uint32_t pool_size = 32;    // Number of pages in pool
            uint32_t page_size = 16384; // Page size in bytes
            PoolLayout layout = PoolLayout::Single;

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
                     AccessStrategy strategy = AccessStrategy::Normal) -> Status;

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
         * Example:
         *   GPID gpid = makeGPID(5, 1000);  // Tablespace 5, page 1000
         *   void *buffer;
         *   Status s = buffer_pool->pinPageGlobal(gpid, &buffer);
         */
        auto pinPageGlobal(GPID gpid, void **buffer, ErrorContext *ctx = nullptr,
                           AccessStrategy strategy = AccessStrategy::Normal) -> Status;

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
                           AccessStrategy strategy = AccessStrategy::Normal) -> Status;

        /**
         * P2-3: Prefetch multiple pages (GPID version)
         */
        auto prefetchPagesGlobal(const std::vector<GPID> &gpids, ErrorContext *ctx = nullptr,
                                 AccessStrategy strategy = AccessStrategy::Normal) -> Status;

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
            bool is_dirty = false;
            std::atomic<uint32_t> usage_count{0}; // Clock Sweep algorithm: usage counter for eviction
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
                  is_dirty(other.is_dirty),
                  usage_count(other.usage_count.load(std::memory_order_relaxed)),
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
                    is_dirty = other.is_dirty;
                    usage_count.store(other.usage_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    // data and content_mutex remain unchanged (unique per frame)
                }
                return *this;
            }

            // Move constructor
            Frame(Frame&& other) noexcept
                : gpid(other.gpid),
                  pin_count(other.pin_count.load(std::memory_order_relaxed)),
                  is_dirty(other.is_dirty),
                  usage_count(other.usage_count.load(std::memory_order_relaxed)),
                  data(std::move(other.data)),
                  content_mutex(std::move(other.content_mutex))
            {
                other.gpid = INVALID_GPID;
                other.pin_count.store(0, std::memory_order_relaxed);
                other.is_dirty = false;
                other.usage_count.store(0, std::memory_order_relaxed);
            }

            // Move assignment operator
            Frame& operator=(Frame&& other) noexcept {
                if (this != &other) {
                    gpid = other.gpid;
                    pin_count.store(other.pin_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    is_dirty = other.is_dirty;
                    usage_count.store(other.usage_count.load(std::memory_order_relaxed), std::memory_order_relaxed);
                    data = std::move(other.data);
                    content_mutex = std::move(other.content_mutex);

                    other.gpid = INVALID_GPID;
                    other.pin_count.store(0, std::memory_order_relaxed);
                    other.is_dirty = false;
                    other.usage_count.store(0, std::memory_order_relaxed);
                }
                return *this;
            }
        };

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

        // Hash function to map GPID to partition index
        static size_t getPartitionIndex(GPID gpid) {
            // Use simple modulo hashing - GPID is already well-distributed
            return static_cast<size_t>(gpid) % NUM_PAGE_TABLE_PARTITIONS;
        }

        Stats stats_;                                       // Statistics (atomic counters)
        mutable std::mutex mutex_;                          // Global mutex for frame allocation/eviction

        std::mutex stats_debug_mutex_;
        FILE *stats_debug_fp_ = nullptr;
        bool stats_debug_enabled_ = false;
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

        // P2-2: Atomic dirty page counter for O(1) getDirtyPageCount()
        // Updated atomically whenever is_dirty flag changes on any frame
        std::atomic<uint32_t> dirty_page_count_{0};

        // Background writer state (Issue 2.20)
        std::unique_ptr<std::thread> bgwriter_thread_;      // Background writer thread
        std::atomic<bool> bgwriter_shutdown_{false};        // Shutdown flag for background writer
        std::condition_variable bgwriter_cv_;               // Condition variable for bgwriter wake-up
        std::mutex bgwriter_mutex_;                         // Mutex for background writer coordination
        ScratchBirdMetrics *metrics_{nullptr};              // Telemetry wiring (optional)

        // Helper methods
        auto evictPage(uint32_t &evicted_frame, ErrorContext *ctx) -> Status;
        auto evictSpecificFrame(uint32_t frame_index, ErrorContext *ctx) -> Status;
        // PHASE 1, TASK 1.2.3: Changed page_id to gpid (GPID is 64-bit)
        auto readPageFromDisk(GPID gpid, uint8_t *buffer, ErrorContext *ctx) -> Status;
        auto writePageToDisk(GPID gpid, const uint8_t *buffer, ErrorContext *ctx) -> Status;
        void updateLru(uint32_t frame_index);
        void insertLruMidpoint(uint32_t frame_index);
        void initializeRingBuffers();
        RingBuffer* getRingBuffer(AccessStrategy strategy);
        uint32_t nextRingSlot(RingBuffer &ring);

        // Background writer methods (Issue 2.20)
        void backgroundWriterMain();                        // Background writer thread main loop
        void backgroundWriterFlush(ErrorContext *ctx);      // Perform one cycle of adaptive flushing
        double calculateDirtyRatio() const;                 // Calculate current dirty page ratio
        uint32_t getDirtyPageCount() const;                 // Get count of dirty pages
        void startBackgroundWriter();                       // Start background writer thread
        void stopBackgroundWriter();                        // Stop background writer thread
        void updateDirtyTelemetry();                        // Sync dirty page gauge
        void updatePoolTelemetry();                         // Sync pool size/total gauges
    };

} // namespace scratchbird::core
