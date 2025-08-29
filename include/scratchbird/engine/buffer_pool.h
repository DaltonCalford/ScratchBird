#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace scratchbird::engine
{

    // Unified type definitions combining both approaches
    using RelationOid = std::uint64_t; // Use 64-bit for compatibility
    using BlockNumber = std::uint64_t; // Use 64-bit for compatibility
    using LSN = std::uint64_t;

    enum ForkNumber : uint8_t { MAIN_FORKNUM = 0, FSM_FORKNUM = 1, VISIBILITY_MAP_FORKNUM = 2 };

    constexpr RelationOid INVALID_OID = 0;
    constexpr BlockNumber INVALID_BLOCK_NUMBER = 0xFFFFFFFFFFFFFFFFULL;

    /**
     * Buffer tag uniquely identifies a buffer page - unified approach
     */
    struct BufferTag {
        RelationOid relation_oid = INVALID_OID;          // Relation (table/index) identifier
        ForkNumber fork_number = MAIN_FORKNUM;           // Fork type (main, FSM, VM)
        BlockNumber block_number = INVALID_BLOCK_NUMBER; // Block number within relation

        // Legacy file_id/page_no compatibility
        std::uint64_t file_id() const
        {
            return relation_oid;
        }
        std::uint64_t page_no() const
        {
            return block_number;
        }

        BufferTag() = default;
        BufferTag(RelationOid rel_oid, ForkNumber fork_num, BlockNumber block_num)
            : relation_oid(rel_oid), fork_number(fork_num), block_number(block_num)
        {
        }
        // Legacy constructor
        BufferTag(std::uint64_t file_id, std::uint64_t page_no)
            : relation_oid(file_id), fork_number(MAIN_FORKNUM), block_number(page_no)
        {
        }

        bool operator==(const BufferTag& other) const
        {
            return relation_oid == other.relation_oid && fork_number == other.fork_number &&
                   block_number == other.block_number;
        }

        bool operator!=(const BufferTag& other) const
        {
            return !(*this == other);
        }

        bool is_valid() const
        {
            return relation_oid != INVALID_OID && block_number != INVALID_BLOCK_NUMBER;
        }
    };

    /**
     * Hash function for BufferTag - unified approach with better distribution
     */
    struct BufferTagHash {
        std::size_t operator()(const BufferTag& tag) const noexcept
        {
            // Use improved hash from remote version
            return static_cast<std::size_t>(
                (tag.relation_oid * 1315423911ull) ^ (tag.block_number * 2654435761ull) ^
                (static_cast<std::uint64_t>(tag.fork_number) * 1103515245ull));
        }
    };

    /**
     * Buffer frame states - enhanced from comprehensive version
     */
    enum class BufferState : uint8_t {
        INVALID = 0, // Buffer frame is not in use
        READING = 1, // Buffer is being read from disk
        VALID = 2,   // Buffer contains valid data
        DIRTY = 3,   // Buffer has been modified
        WRITING = 4, // Buffer is being written to disk
        PINNED = 5   // Buffer is pinned in memory
    };

    /**
     * Unified Buffer Frame combining best of both approaches
     */
    struct BufferFrame {
        BufferTag tag{};
        std::vector<std::uint8_t> data;
        std::atomic<bool> dirty{false};
        std::atomic<int> refcount{0};
        std::atomic<bool> valid{false};
        std::atomic<LSN> page_lsn{0};
        std::uint8_t clock{1}; // Clock-sweep usage bit

        // Statistics from comprehensive version
        std::atomic<std::chrono::steady_clock::time_point> last_access_time;
        std::atomic<uint64_t> access_count{0};

        BufferFrame()
        {
            last_access_time.store(std::chrono::steady_clock::now());
        }

        void update_access_time()
        {
            last_access_time.store(std::chrono::steady_clock::now());
            access_count.fetch_add(1, std::memory_order_relaxed);
            clock = 1; // Mark as recently used
        }
    };

    /**
     * Buffer pool configuration parameters - from comprehensive version
     */
    struct BufferPoolConfig {
        // Pool size configuration
        size_t num_buffers = 1024; // Number of buffer frames
        size_t buffer_size = 8192; // Size of each buffer frame (8KB default)

        // Performance tuning
        double dirty_page_threshold = 0.7; // Threshold for background writer activation
        std::chrono::milliseconds background_write_interval{100}; // Background writer interval

        // Monitoring and diagnostics
        bool enable_statistics = true;                  // Enable performance statistics collection
        bool enable_background_writer = false;          // Disable by default for testing
        std::chrono::seconds stats_report_interval{60}; // Statistics reporting interval

        // Memory management
        bool use_huge_pages = false; // Use huge pages for buffer pool memory
        bool enable_prefetch = true; // Enable buffer prefetching
    };

    // Forward declaration
    class BufferPool;

    /**
     * BufferHandle - RAII wrapper from elegant remote version
     * Retains a reference on a frame while in scope.
     * It is move-only and releases the reference on destruction.
     */
    class BufferHandle
    {
      public:
        BufferHandle() = default;
        BufferHandle(BufferPool* pool, int index) : pool_(pool), index_(index) {}
        BufferHandle(BufferHandle&& other) noexcept
        {
            swap(other);
        }
        BufferHandle& operator=(BufferHandle&& other) noexcept
        {
            if (this != &other) {
                release();
                swap(other);
            }
            return *this;
        }
        BufferHandle(const BufferHandle&) = delete;
        BufferHandle& operator=(const BufferHandle&) = delete;
        ~BufferHandle()
        {
            release();
        }

        bool valid() const
        {
            return pool_ != nullptr && index_ >= 0;
        }
        int index() const
        {
            return index_;
        }
        BufferFrame* frame();
        const BufferFrame* frame() const;
        void mark_dirty();

      private:
        void release();
        void swap(BufferHandle& other) noexcept
        {
            std::swap(pool_, other.pool_);
            std::swap(index_, other.index_);
        }

        BufferPool* pool_{nullptr};
        int index_{-1};
    };

    /**
     * Buffer pool statistics - comprehensive monitoring from detailed version
     */
    struct BufferPoolStats {
        // Hit ratio statistics
        uint64_t buffer_hits{0};
        uint64_t buffer_misses{0};
        uint64_t buffer_reads{0};
        uint64_t buffer_writes{0};

        // Buffer utilization
        uint64_t buffers_used{0};
        uint64_t buffers_dirty{0};
        uint64_t buffers_pinned{0};

        // Eviction and replacement statistics
        uint64_t evictions_clean{0};
        uint64_t evictions_dirty{0};
        uint64_t clock_sweeps{0};

        // Background writer statistics
        uint64_t background_writes{0};

        std::chrono::steady_clock::time_point last_reset_time{std::chrono::steady_clock::now()};

        // Legacy compatibility with simple stats
        uint64_t hits() const
        {
            return buffer_hits;
        }
        uint64_t misses() const
        {
            return buffer_misses;
        }
        uint64_t evictions() const
        {
            return evictions_clean + evictions_dirty;
        }
        uint64_t flushes() const
        {
            return background_writes;
        }

        // Calculated metrics
        double get_hit_ratio() const
        {
            uint64_t total = buffer_hits + buffer_misses;
            return total > 0 ? static_cast<double>(buffer_hits) / total : 0.0;
        }

        double get_dirty_ratio() const
        {
            return buffers_used > 0 ? static_cast<double>(buffers_dirty) / buffers_used : 0.0;
        }

        void reset()
        {
            buffer_hits = 0;
            buffer_misses = 0;
            buffer_reads = 0;
            buffer_writes = 0;
            buffers_used = 0;
            buffers_dirty = 0;
            buffers_pinned = 0;
            evictions_clean = 0;
            evictions_dirty = 0;
            clock_sweeps = 0;
            background_writes = 0;
            last_reset_time = std::chrono::steady_clock::now();
        }
    };

    /**
     * Unified BufferPool combining elegant RAII design with comprehensive features
     * Features:
     * - BufferHandle RAII pattern from remote version
     * - Clock-sweep LRU replacement algorithm
     * - Comprehensive statistics and monitoring
     * - Optional background writer support
     * - Thread-safe operations with fine-grained locking
     */
    class BufferPool
    {
      public:
        using FlushCallback = std::function<void(const BufferFrame&)>;

        explicit BufferPool(std::size_t capacity_pages = 1024, std::size_t page_size = 8192)
            : config_{capacity_pages, page_size}, capacity_(capacity_pages), page_size_(page_size)
        {
            frames_.reserve(capacity_);
            for (std::size_t i = 0; i < capacity_; ++i) {
                auto f = std::make_unique<BufferFrame>();
                f->data.resize(page_size_);
                frames_.push_back(std::move(f));
            }
        }

        explicit BufferPool(const BufferPoolConfig& config)
            : config_(config), capacity_(config.num_buffers), page_size_(config.buffer_size)
        {
            frames_.reserve(capacity_);
            for (std::size_t i = 0; i < capacity_; ++i) {
                auto f = std::make_unique<BufferFrame>();
                f->data.resize(page_size_);
                frames_.push_back(std::move(f));
            }
        }

        ~BufferPool()
        {
            shutdown();
        }

        // Non-copyable, non-moveable
        BufferPool(const BufferPool&) = delete;
        BufferPool& operator=(const BufferPool&) = delete;
        BufferPool(BufferPool&&) = delete;
        BufferPool& operator=(BufferPool&&) = delete;

        /**
         * Initialize buffer pool - comprehensive version API
         */
        std::error_code initialize()
        {
            if (config_.enable_background_writer) {
                start_background_writer();
            }
            return {};
        }

        /**
         * Shutdown buffer pool and flush all dirty buffers
         */
        void shutdown()
        {
            if (config_.enable_background_writer) {
                stop_background_writer();
            }
            flush_dirty_batch(capacity_);
        }

        // Core API - RAII BufferHandle approach (elegant remote version)
        BufferHandle get(const BufferTag& tag)
        {
            std::lock_guard<std::mutex> lock(mu_);
            int index = find_or_create_index_locked(tag);
            if (index >= 0) {
                inc_ref(index);
                frames_[index]->update_access_time();
                atomic_stats_.buffer_hits.fetch_add(1, std::memory_order_relaxed);
                return BufferHandle(this, index);
            }
            atomic_stats_.buffer_misses.fetch_add(1, std::memory_order_relaxed);
            return BufferHandle();
        }

        // Legacy API compatibility - comprehensive version interface
        int get_buffer(const BufferTag& tag, bool& found)
        {
            auto handle = get(tag);
            found = handle.valid();
            return found ? handle.index() : -1;
        }

        void set_flush_callback(FlushCallback cb)
        {
            std::lock_guard<std::mutex> lg(mu_);
            flush_cb_ = std::move(cb);
        }

        std::size_t capacity() const
        {
            return capacity_;
        }
        std::size_t page_size() const
        {
            return page_size_;
        }

        // Enhanced flush operations
        std::size_t flush_dirty_batch(std::size_t max_pages)
        {
            std::lock_guard<std::mutex> lock(mu_);
            std::size_t flushed = 0;

            for (std::size_t i = 0; i < capacity_ && flushed < max_pages; ++i) {
                if (frames_[i]->dirty.load() && frames_[i]->refcount.load() == 0) {
                    if (flush_cb_) {
                        flush_cb_(*frames_[i]);
                    }
                    frames_[i]->dirty.store(false);
                    flushed++;
                    atomic_stats_.background_writes.fetch_add(1, std::memory_order_relaxed);
                }
            }

            return flushed;
        }

        // Statistics - unified interface
        BufferPoolStats get_stats() const
        {
            BufferPoolStats result;
            result.buffer_hits = atomic_stats_.buffer_hits.load();
            result.buffer_misses = atomic_stats_.buffer_misses.load();
            result.buffer_reads = atomic_stats_.buffer_reads.load();
            result.buffer_writes = atomic_stats_.buffer_writes.load();
            result.buffers_used = atomic_stats_.buffers_used.load();
            result.buffers_dirty = atomic_stats_.buffers_dirty.load();
            result.buffers_pinned = atomic_stats_.buffers_pinned.load();
            result.evictions_clean = atomic_stats_.evictions_clean.load();
            result.evictions_dirty = atomic_stats_.evictions_dirty.load();
            result.clock_sweeps = atomic_stats_.clock_sweeps.load();
            result.background_writes = atomic_stats_.background_writes.load();
            result.last_reset_time = atomic_stats_.last_reset_time.load();
            return result;
        }

        void reset_stats()
        {
            atomic_stats_.buffer_hits.store(0);
            atomic_stats_.buffer_misses.store(0);
            atomic_stats_.buffer_reads.store(0);
            atomic_stats_.buffer_writes.store(0);
            atomic_stats_.buffers_used.store(0);
            atomic_stats_.buffers_dirty.store(0);
            atomic_stats_.buffers_pinned.store(0);
            atomic_stats_.evictions_clean.store(0);
            atomic_stats_.evictions_dirty.store(0);
            atomic_stats_.clock_sweeps.store(0);
            atomic_stats_.background_writes.store(0);
            atomic_stats_.last_reset_time.store(std::chrono::steady_clock::now());
        }

        // Configuration access
        const BufferPoolConfig& get_config() const
        {
            return config_;
        }

      private:
        friend class BufferHandle;

        // Internal atomic statistics for thread-safe updates
        struct AtomicBufferPoolStats {
            std::atomic<uint64_t> buffer_hits{0};
            std::atomic<uint64_t> buffer_misses{0};
            std::atomic<uint64_t> buffer_reads{0};
            std::atomic<uint64_t> buffer_writes{0};
            std::atomic<uint64_t> buffers_used{0};
            std::atomic<uint64_t> buffers_dirty{0};
            std::atomic<uint64_t> buffers_pinned{0};
            std::atomic<uint64_t> evictions_clean{0};
            std::atomic<uint64_t> evictions_dirty{0};
            std::atomic<uint64_t> clock_sweeps{0};
            std::atomic<uint64_t> background_writes{0};
            std::atomic<std::chrono::steady_clock::time_point> last_reset_time{std::chrono::steady_clock::now()};
        };

        BufferPoolConfig config_;
        std::size_t capacity_;
        std::size_t page_size_;
        std::vector<std::unique_ptr<BufferFrame>> frames_;
        std::unordered_map<BufferTag, int, BufferTagHash> tag_to_index_;
        mutable std::mutex mu_;
        FlushCallback flush_cb_{};
        mutable AtomicBufferPoolStats atomic_stats_;
        int clock_hand_{0};

        // Background writer support
        std::thread background_writer_thread_;
        std::atomic<bool> shutdown_requested_{false};
        std::condition_variable background_writer_cv_;
        mutable std::mutex background_writer_mutex_;

        int find_or_create_index_locked(const BufferTag& tag)
        {
            auto it = tag_to_index_.find(tag);
            if (it != tag_to_index_.end()) {
                return it->second;
            }

            // Need to find a victim
            int victim_index = choose_victim_locked();
            if (victim_index >= 0) {
                // Evict old tag if present
                if (frames_[victim_index]->tag.is_valid()) {
                    tag_to_index_.erase(frames_[victim_index]->tag);
                    if (frames_[victim_index]->dirty.load()) {
                        atomic_stats_.evictions_dirty.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        atomic_stats_.evictions_clean.fetch_add(1, std::memory_order_relaxed);
                    }
                }

                // Install new tag
                frames_[victim_index]->tag = tag;
                frames_[victim_index]->valid.store(true);
                frames_[victim_index]->dirty.store(false);
                tag_to_index_[tag] = victim_index;

                return victim_index;
            }

            return -1; // No victim available
        }

        int choose_victim_locked()
        {
            atomic_stats_.clock_sweeps.fetch_add(1, std::memory_order_relaxed);

            // Clock-sweep algorithm
            for (std::size_t attempts = 0; attempts < capacity_; ++attempts) {
                int index = clock_hand_;
                clock_hand_ = (clock_hand_ + 1) % static_cast<int>(capacity_);

                if (frames_[index]->refcount.load() == 0) {
                    if (frames_[index]->clock == 0) {
                        return index; // Found victim
                    } else {
                        frames_[index]->clock = 0; // Give second chance
                    }
                }
            }

            return -1; // No victim found
        }

        void inc_ref(int index)
        {
            frames_[index]->refcount.fetch_add(1, std::memory_order_acq_rel);
            frames_[index]->clock = 1;
        }

        void dec_ref(int index)
        {
            frames_[index]->refcount.fetch_sub(1, std::memory_order_acq_rel);
        }

        void mark_dirty(int index)
        {
            frames_[index]->dirty.store(true, std::memory_order_release);
        }

        void start_background_writer()
        {
            background_writer_thread_ = std::thread([this]() { background_writer_loop(); });
        }

        void stop_background_writer()
        {
            if (background_writer_thread_.joinable()) {
                shutdown_requested_.store(true);
                background_writer_cv_.notify_all();
                background_writer_thread_.join();
            }
        }

        void background_writer_loop()
        {
            std::unique_lock<std::mutex> lock(background_writer_mutex_);

            while (!shutdown_requested_.load()) {
                background_writer_cv_.wait_for(lock, config_.background_write_interval);

                if (shutdown_requested_.load())
                    break;

                // Check if we need to write dirty pages
                size_t dirty_count = atomic_stats_.buffers_dirty.load();
                size_t total_used = atomic_stats_.buffers_used.load();

                if (total_used > 0 &&
                    static_cast<double>(dirty_count) / total_used > config_.dirty_page_threshold) {

                    lock.unlock();
                    flush_dirty_batch(static_cast<size_t>(capacity_ * 0.1)); // Flush 10% of pages
                    lock.lock();
                }
            }
        }
    };

    // Buffer ID constants
    constexpr int INVALID_BUFFER_ID = -1;

} // namespace scratchbird::engine
