#pragma once

#include <cstdint>
#include <vector>
#include <list>
#include <unordered_map>
#include <mutex>
#include <memory>
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

        // Statistics
        struct Stats
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
        };

        auto getStats() const -> Stats
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return stats_;
        }

        // Increment page size mismatch counter (called by HeapPage when corruption detected)
        void incrementPageSizeMismatchCount()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.page_size_mismatches++;
        }

    private:
        // Frame metadata
        struct Frame
        {
            uint32_t page_id = INVALID_PAGE_ID;
            uint32_t pin_count = 0;
            bool is_dirty = false;
            std::unique_ptr<uint8_t[]> data = nullptr;
            std::unique_ptr<std::mutex>
                content_mutex; // Protects page content from concurrent modifications

            static constexpr uint32_t INVALID_PAGE_ID = 0xFFFFFFFF;

            // Constructor to initialize mutex
            Frame() : content_mutex(std::make_unique<std::mutex>()) {}
        };

        Database *db_;                                      // Database instance
        Config config_;                                     // Configuration
        std::vector<Frame> frames_;                         // Buffer pool frames
        std::list<uint32_t> lru_list_;                      // LRU list (frame indices)
        std::unordered_map<uint32_t, uint32_t> page_table_; // page_id -> frame_index
        Stats stats_;                                       // Statistics
        mutable std::mutex mutex_;                          // Thread safety (future)

        // Helper methods
        auto evictPage(uint32_t &evicted_frame, ErrorContext *ctx) -> Status;
        auto readPageFromDisk(uint32_t page_id, uint8_t *buffer, ErrorContext *ctx) -> Status;
        auto writePageToDisk(uint32_t page_id, const uint8_t *buffer, ErrorContext *ctx) -> Status;
        void updateLru(uint32_t frame_index);
    };

} // namespace scratchbird::core
