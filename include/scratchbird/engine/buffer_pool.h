#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <system_error>

// Basic type definitions for buffer pool
namespace ScratchBird {
    using RelationOid = uint32_t;
    using BlockNumber = uint32_t;
    using LSN = uint64_t;
    
    enum ForkNumber : uint8_t {
        MAIN_FORKNUM = 0,
        FSM_FORKNUM = 1,
        VISIBILITY_MAP_FORKNUM = 2
    };
    
    constexpr RelationOid INVALID_OID = 0;
    constexpr BlockNumber INVALID_BLOCK_NUMBER = 0xFFFFFFFF;
}

namespace ScratchBird
{
    // Forward declarations
    class WALManager;
    
    /**
     * Buffer pool configuration parameters
     */
    struct BufferPoolConfig {
        // Pool size configuration
        size_t num_buffers = 1024;                     // Number of buffer frames
        size_t buffer_size = 8192;                     // Size of each buffer frame (8KB default)
        
        // Performance tuning
        size_t hash_table_size = 2048;                 // Hash table size for buffer lookup
        double dirty_page_threshold = 0.7;             // Threshold for background writer activation
        std::chrono::milliseconds background_write_interval{100}; // Background writer interval
        
        // Monitoring and diagnostics
        bool enable_statistics = true;                 // Enable performance statistics collection
        bool enable_background_writer = true;          // Enable background writer thread
        std::chrono::seconds stats_report_interval{60}; // Statistics reporting interval
        
        // Replacement policy parameters
        size_t clock_hand_advance_size = 8;            // Number of buffers to check per clock sweep
        bool use_adaptive_replacement = true;          // Enable adaptive replacement policy
        
        // Memory management
        bool use_huge_pages = false;                   // Use huge pages for buffer pool memory
        bool enable_prefetch = true;                   // Enable buffer prefetching
    };

    /**
     * Buffer tag uniquely identifies a buffer page
     */
    struct BufferTag {
        RelationOid relation_oid = INVALID_OID;        // Relation (table/index) identifier  
        ForkNumber fork_number = MAIN_FORKNUM;         // Fork type (main, FSM, VM)
        BlockNumber block_number = INVALID_BLOCK_NUMBER; // Block number within relation
        
        BufferTag() = default;
        BufferTag(RelationOid rel_oid, ForkNumber fork_num, BlockNumber block_num)
            : relation_oid(rel_oid), fork_number(fork_num), block_number(block_num) {}
            
        bool operator==(const BufferTag& other) const {
            return relation_oid == other.relation_oid && 
                   fork_number == other.fork_number && 
                   block_number == other.block_number;
        }
        
        bool operator!=(const BufferTag& other) const {
            return !(*this == other);
        }
        
        bool is_valid() const {
            return relation_oid != INVALID_OID && block_number != INVALID_BLOCK_NUMBER;
        }
    };
    
    /**
     * Hash function for BufferTag
     */
    struct BufferTagHash {
        std::size_t operator()(const BufferTag& tag) const {
            std::size_t h1 = std::hash<uint32_t>{}(tag.relation_oid);
            std::size_t h2 = std::hash<uint8_t>{}(static_cast<uint8_t>(tag.fork_number));
            std::size_t h3 = std::hash<uint32_t>{}(tag.block_number);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    /**
     * Buffer frame states
     */
    enum class BufferState : uint8_t {
        INVALID = 0,        // Buffer frame is not in use
        READING = 1,        // Buffer is being read from disk
        VALID = 2,          // Buffer contains valid data
        DIRTY = 3,          // Buffer has been modified
        WRITING = 4,        // Buffer is being written to disk
        PINNED = 5         // Buffer is pinned in memory
    };

    /**
     * Buffer frame descriptor - metadata for each buffer frame
     */
    class BufferDescriptor {
    public:
        BufferDescriptor();
        ~BufferDescriptor() = default;
        
        // Non-copyable, non-moveable (managed by buffer pool)
        BufferDescriptor(const BufferDescriptor&) = delete;
        BufferDescriptor& operator=(const BufferDescriptor&) = delete;
        BufferDescriptor(BufferDescriptor&&) = delete;
        BufferDescriptor& operator=(BufferDescriptor&&) = delete;
        
        // Buffer identification
        BufferTag get_tag() const;
        void set_tag(const BufferTag& tag);
        bool matches_tag(const BufferTag& tag) const;
        
        // Buffer state management
        BufferState get_state() const;
        void set_state(BufferState state);
        bool is_dirty() const;
        bool is_valid() const;
        bool is_pinned() const;
        
        // Reference counting and pinning
        int get_pin_count() const;
        void pin();
        void unpin();
        bool try_pin();
        
        // Clock-sweep algorithm support
        bool get_usage_bit() const;
        void set_usage_bit(bool used);
        bool clear_usage_bit(); // Returns previous value
        
        // LSN tracking for WAL
        LSN get_lsn() const;
        void set_lsn(LSN lsn);
        
        // Statistics
        std::chrono::steady_clock::time_point get_last_access_time() const;
        void update_access_time();
        uint64_t get_access_count() const;
        
    private:
        mutable std::shared_mutex mutex_;
        
        BufferTag tag_;
        std::atomic<BufferState> state_{BufferState::INVALID};
        std::atomic<int> pin_count_{0};
        std::atomic<bool> usage_bit_{false};
        std::atomic<LSN> page_lsn_{0};
        
        std::atomic<std::chrono::steady_clock::time_point> last_access_time_;
        std::atomic<uint64_t> access_count_{0};
    };

    /**
     * Buffer frame - actual data storage
     */
    class BufferFrame {
    public:
        BufferFrame(size_t buffer_size);
        ~BufferFrame();
        
        // Non-copyable, non-moveable (managed by buffer pool)
        BufferFrame(const BufferFrame&) = delete;
        BufferFrame& operator=(const BufferFrame&) = delete;
        BufferFrame(BufferFrame&&) = delete;
        BufferFrame& operator=(BufferFrame&&) = delete;
        
        // Data access
        char* get_data() { return data_; }
        const char* get_data() const { return data_; }
        size_t get_size() const { return size_; }
        
        // Data operations
        void zero_fill();
        void copy_from(const char* source, size_t length);
        void copy_to(char* destination, size_t length) const;
        
    private:
        char* data_;
        size_t size_;
    };

    /**
     * Buffer pool statistics for monitoring and optimization
     */
    class BufferPoolStats {
    public:
        BufferPoolStats() = default;
        
        // Non-copyable due to atomic members, but provide copy functionality
        BufferPoolStats(const BufferPoolStats& other) {
            copy_from(other);
        }
        
        BufferPoolStats& operator=(const BufferPoolStats& other) {
            if (this != &other) {
                copy_from(other);
            }
            return *this;
        }
        
        // Hit ratio statistics
        std::atomic<uint64_t> buffer_hits{0};
        std::atomic<uint64_t> buffer_misses{0};
        std::atomic<uint64_t> buffer_reads{0};
        std::atomic<uint64_t> buffer_writes{0};
        
        // Buffer utilization
        std::atomic<uint64_t> buffers_used{0};
        std::atomic<uint64_t> buffers_dirty{0};
        std::atomic<uint64_t> buffers_pinned{0};
        
        // Eviction and replacement statistics
        std::atomic<uint64_t> evictions_clean{0};
        std::atomic<uint64_t> evictions_dirty{0};
        std::atomic<uint64_t> clock_sweeps{0};
        
        // Contention and performance
        std::atomic<uint64_t> lock_waits{0};
        std::atomic<uint64_t> lock_timeouts{0};
        std::atomic<uint64_t> avg_lookup_time_ns{0}; // Changed from chrono::nanoseconds
        
        // Background writer statistics
        std::atomic<uint64_t> background_writes{0};
        std::atomic<uint64_t> checkpoint_writes{0};
        
        std::chrono::steady_clock::time_point last_reset_time;
        
        // Calculated metrics
        double get_hit_ratio() const {
            uint64_t hits = buffer_hits.load();
            uint64_t total = hits + buffer_misses.load();
            return total > 0 ? static_cast<double>(hits) / total : 0.0;
        }
        
        double get_dirty_ratio() const {
            uint64_t used = buffers_used.load();
            return used > 0 ? static_cast<double>(buffers_dirty.load()) / used : 0.0;
        }
        
        void reset();
        
    private:
        void copy_from(const BufferPoolStats& other) {
            buffer_hits.store(other.buffer_hits.load());
            buffer_misses.store(other.buffer_misses.load());
            buffer_reads.store(other.buffer_reads.load());
            buffer_writes.store(other.buffer_writes.load());
            buffers_used.store(other.buffers_used.load());
            buffers_dirty.store(other.buffers_dirty.load());
            buffers_pinned.store(other.buffers_pinned.load());
            evictions_clean.store(other.evictions_clean.load());
            evictions_dirty.store(other.evictions_dirty.load());
            clock_sweeps.store(other.clock_sweeps.load());
            lock_waits.store(other.lock_waits.load());
            lock_timeouts.store(other.lock_timeouts.load());
            avg_lookup_time_ns.store(other.avg_lookup_time_ns.load());
            background_writes.store(other.background_writes.load());
            checkpoint_writes.store(other.checkpoint_writes.load());
            last_reset_time = other.last_reset_time;
        }
    };

    /**
     * Shared Buffer Pool - Main buffer management system
     * 
     * The buffer pool manages a fixed number of buffer frames, each capable of
     * holding one database page. It provides efficient buffer lookup, replacement,
     * and synchronization for concurrent access.
     * 
     * Features:
     * - Clock-sweep LRU replacement algorithm
     * - Hash table for O(1) buffer lookup
     * - Fine-grained locking for high concurrency
     * - Integrated WAL (Write-Ahead Logging) support
     * - Comprehensive statistics and monitoring
     */
    class BufferPool {
    public:
        explicit BufferPool(const BufferPoolConfig& config = {});
        ~BufferPool();
        
        // Non-copyable, non-moveable (singleton-like resource manager)
        BufferPool(const BufferPool&) = delete;
        BufferPool& operator=(const BufferPool&) = delete;
        BufferPool(BufferPool&&) = delete;
        BufferPool& operator=(BufferPool&&) = delete;
        
        /**
         * Initialize buffer pool and allocate memory
         * @return Error code, empty on success
         */
        std::error_code initialize();
        
        /**
         * Shutdown buffer pool and flush all dirty buffers
         */
        void shutdown();
        
        /**
         * Get a buffer for the specified page
         * @param tag Buffer tag identifying the page
         * @param found Set to true if buffer was found in pool
         * @return Buffer ID, or INVALID_BUFFER_ID on error
         */
        int get_buffer(const BufferTag& tag, bool& found);
        
        /**
         * Release a buffer (decrease pin count)
         * @param buffer_id Buffer ID to release
         * @param mark_dirty Mark buffer as dirty if true
         */
        void release_buffer(int buffer_id, bool mark_dirty = false);
        
        /**
         * Pin a buffer in memory (increase pin count)
         * @param buffer_id Buffer ID to pin
         * @return True if successfully pinned
         */
        bool pin_buffer(int buffer_id);
        
        /**
         * Unpin a buffer (decrease pin count)
         * @param buffer_id Buffer ID to unpin
         */
        void unpin_buffer(int buffer_id);
        
        /**
         * Flush a specific buffer to disk
         * @param buffer_id Buffer ID to flush
         * @return Error code, empty on success
         */
        std::error_code flush_buffer(int buffer_id);
        
        /**
         * Flush all dirty buffers to disk
         * @return Number of buffers flushed
         */
        size_t flush_all_buffers();
        
        /**
         * Get buffer frame data
         * @param buffer_id Buffer ID
         * @return Pointer to buffer data, or nullptr if invalid
         */
        char* get_buffer_data(int buffer_id);
        const char* get_buffer_data(int buffer_id) const;
        
        /**
         * Get buffer descriptor
         * @param buffer_id Buffer ID
         * @return Pointer to buffer descriptor, or nullptr if invalid
         */
        BufferDescriptor* get_buffer_descriptor(int buffer_id);
        const BufferDescriptor* get_buffer_descriptor(int buffer_id) const;
        
        /**
         * Invalidate buffers for a specific relation
         * @param relation_oid Relation OID to invalidate
         * @return Number of buffers invalidated
         */
        size_t invalidate_relation_buffers(RelationOid relation_oid);
        
        /**
         * Get buffer pool statistics
         * @return Current buffer pool statistics
         */
        BufferPoolStats get_stats() const;
        
        /**
         * Reset buffer pool statistics
         */
        void reset_stats();
        
        /**
         * Get buffer pool configuration
         */
        const BufferPoolConfig& get_config() const { return config_; }
        
        /**
         * Update buffer pool configuration
         * @param new_config New configuration
         * @return Error code, empty on success
         */
        std::error_code update_config(const BufferPoolConfig& new_config);
        
        /**
         * Validate buffer pool configuration
         * @param config Configuration to validate
         * @return Error message, empty if valid
         */
        static std::string validate_config(const BufferPoolConfig& config);
        
        /**
         * Get buffer pool usage information
         */
        struct UsageInfo {
            size_t total_buffers;
            size_t used_buffers;
            size_t dirty_buffers;
            size_t pinned_buffers;
            double usage_percentage;
            double hit_ratio;
        };
        UsageInfo get_usage_info() const;
        
    private:
        BufferPoolConfig config_;
        
        // Buffer storage
        std::vector<std::unique_ptr<BufferFrame>> buffer_frames_;
        std::vector<std::unique_ptr<BufferDescriptor>> buffer_descriptors_;
        
        // Hash table for buffer lookup
        std::unordered_map<BufferTag, int, BufferTagHash> buffer_hash_table_;
        mutable std::shared_mutex hash_table_mutex_;
        
        // Clock-sweep replacement algorithm
        std::atomic<size_t> clock_hand_{0};
        mutable std::mutex clock_mutex_;
        
        // Free buffer list
        std::vector<int> free_buffers_;
        mutable std::mutex free_list_mutex_;
        
        // Statistics
        mutable BufferPoolStats stats_;
        
        // Background writer
        std::thread background_writer_thread_;
        std::atomic<bool> shutdown_requested_{false};
        std::condition_variable background_writer_cv_;
        mutable std::mutex background_writer_mutex_;
        
        // WAL integration
        WALManager* wal_manager_ = nullptr;
        
        /**
         * Find a victim buffer for replacement using clock-sweep algorithm
         * @return Buffer ID of victim, or -1 if none available
         */
        int find_victim_buffer();
        
        /**
         * Allocate a new buffer from free list
         * @return Buffer ID, or -1 if no free buffers
         */
        int allocate_buffer();
        
        /**
         * Evict a buffer (write to disk if dirty)
         * @param buffer_id Buffer ID to evict
         * @return Error code, empty on success
         */
        std::error_code evict_buffer(int buffer_id);
        
        /**
         * Background writer loop
         */
        void background_writer_loop();
        
        /**
         * Write a dirty buffer to disk
         * @param buffer_id Buffer ID to write
         * @return Error code, empty on success
         */
        std::error_code write_buffer(int buffer_id);
        
        /**
         * Update buffer pool statistics
         */
        void update_stats(bool hit, bool read, bool write);
        
        /**
         * Validate buffer ID
         * @param buffer_id Buffer ID to validate
         * @return True if valid
         */
        bool is_valid_buffer_id(int buffer_id) const {
            return buffer_id >= 0 && static_cast<size_t>(buffer_id) < config_.num_buffers;
        }
    };
    
    // Buffer ID constants
    constexpr int INVALID_BUFFER_ID = -1;
    
} // namespace ScratchBird