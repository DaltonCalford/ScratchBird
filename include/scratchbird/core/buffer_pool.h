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

namespace scratchbird {
namespace core {

// Forward declarations
class Database;

/**
 * Buffer Pool - Manages in-memory page cache
 * 
 * Implements a fixed-size buffer pool with LRU eviction.
 * Single-threaded for Alpha (mutex for future multi-threading).
 */
class BufferPool {
public:
    // Buffer pool configuration
    struct Config {
        uint32_t pool_size = 32;      // Number of pages in pool
        uint32_t page_size = 16384;   // Page size in bytes
    };
    
    BufferPool(Database* db, const Config& config);
    ~BufferPool();
    
    // Initialize buffer pool
    Status initialize(ErrorContext* ctx = nullptr);
    
    // Shutdown and flush all dirty pages
    Status shutdown(ErrorContext* ctx = nullptr);
    
    /**
     * Pin a page in the buffer pool
     * @param page_id Page to pin
     * @param buffer Returns pointer to page data
     * @param ctx Error context
     * @return Status code
     */
    Status pin_page(uint32_t page_id, void** buffer, ErrorContext* ctx = nullptr);
    
    /**
     * Unpin a page
     * @param page_id Page to unpin
     * @param is_dirty True if page was modified
     * @param ctx Error context
     * @return Status code
     */
    Status unpin_page(uint32_t page_id, bool is_dirty, ErrorContext* ctx = nullptr);
    
    /**
     * Flush a specific page if dirty
     */
    Status flush_page(uint32_t page_id, ErrorContext* ctx = nullptr);
    
    /**
     * Flush all dirty pages
     */
    Status flush_all(ErrorContext* ctx = nullptr);
    
    // Statistics
    struct Stats {
        uint64_t hits = 0;          // Cache hits
        uint64_t misses = 0;        // Cache misses
        uint64_t evictions = 0;     // Pages evicted
        uint64_t flushes = 0;       // Pages flushed
    };
    
    Stats get_stats() const { return stats_; }
    
private:
    // Frame metadata
    struct Frame {
        uint32_t page_id = INVALID_PAGE_ID;
        uint32_t pin_count = 0;
        bool is_dirty = false;
        uint8_t* data = nullptr;
        
        static constexpr uint32_t INVALID_PAGE_ID = 0xFFFFFFFF;
    };
    
    Database* db_;                              // Database instance
    Config config_;                             // Configuration
    std::vector<Frame> frames_;                 // Buffer pool frames
    std::list<uint32_t> lru_list_;             // LRU list (frame indices)
    std::unordered_map<uint32_t, uint32_t> page_table_;  // page_id -> frame_index
    Stats stats_;                               // Statistics
    mutable std::mutex mutex_;                  // Thread safety (future)
    
    // Helper methods
    Status evict_page(uint32_t& evicted_frame, ErrorContext* ctx);
    Status read_page_from_disk(uint32_t page_id, uint8_t* buffer, ErrorContext* ctx);
    Status write_page_to_disk(uint32_t page_id, const uint8_t* buffer, ErrorContext* ctx);
    void update_lru(uint32_t frame_index);
};

} // namespace core
} // namespace scratchbird