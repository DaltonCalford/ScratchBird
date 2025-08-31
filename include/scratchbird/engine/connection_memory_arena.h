// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "scratchbird/engine/pool_allocator.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace scratchbird::engine
{

    /// Configuration for connection memory arenas
    struct ConnectionArenaConfig {
        /// Initial arena size per connection (bytes)
        std::size_t initial_arena_size{1024 * 1024}; // 1MB

        /// Maximum arena size per connection (bytes)
        std::size_t max_arena_size{64 * 1024 * 1024}; // 64MB

        /// Arena growth factor when expanding
        double growth_factor{2.0};

        /// Size threshold for temporary vs persistent allocations
        std::size_t temporary_threshold{4096};

        /// Maximum number of persistent allocations to track
        std::size_t max_persistent_allocations{10000};

        /// Reset frequency for temporary arena (queries processed)
        std::size_t temp_reset_frequency{100};

        /// Memory pressure threshold (triggers cleanup)
        double memory_pressure_threshold{0.8};

        /// Enable detailed per-connection statistics
        bool enable_detailed_stats{true};

        /// Memory alignment for arena allocations
        std::size_t alignment{8};

        /// Enable automatic memory defragmentation
        bool enable_defragmentation{true};
    };

    /// Statistics for connection memory arena
    struct ConnectionArenaStats {
        /// Connection identification
        std::string connection_id;
        std::chrono::steady_clock::time_point creation_time;
        std::chrono::steady_clock::time_point last_activity;

        /// Memory usage statistics
        std::atomic<std::uint64_t> total_allocated_bytes{0};
        std::atomic<std::uint64_t> total_deallocated_bytes{0};
        std::atomic<std::uint64_t> current_usage_bytes{0};
        std::atomic<std::uint64_t> peak_usage_bytes{0};

        /// Allocation statistics
        std::atomic<std::uint64_t> total_allocations{0};
        std::atomic<std::uint64_t> total_deallocations{0};
        std::atomic<std::uint64_t> temporary_allocations{0};
        std::atomic<std::uint64_t> persistent_allocations{0};

        /// Performance metrics
        std::atomic<std::uint64_t> queries_processed{0};
        std::atomic<std::uint64_t> arena_resets{0};
        std::atomic<std::uint64_t> defragmentation_runs{0};

        /// Efficiency metrics
        std::atomic<std::uint64_t> wasted_bytes{0};   // Internal fragmentation
        std::atomic<double> fragmentation_ratio{0.0}; // Fragmentation percentage
        std::atomic<double> utilization_ratio{0.0};   // Memory utilization

        /// Timing statistics
        std::atomic<std::chrono::nanoseconds::rep> total_allocation_time_ns{0};
        std::atomic<std::chrono::nanoseconds::rep> total_deallocation_time_ns{0};
        std::atomic<std::chrono::nanoseconds::rep> total_reset_time_ns{0};
    };

    /// Memory block within an arena
    struct ArenaBlock {
        void* ptr{nullptr};                                    ///< Pointer to allocated memory
        std::size_t size{0};                                   ///< Size of allocation
        std::size_t aligned_size{0};                           ///< Size including alignment padding
        bool is_free{true};                                    ///< Whether block is available
        bool is_temporary{false};                              ///< Whether block is temporary
        std::chrono::steady_clock::time_point allocation_time; ///< When allocated

        ArenaBlock() = default;
        ArenaBlock(void* p, std::size_t sz, std::size_t aligned_sz, bool temp = false)
            : ptr(p), size(sz), aligned_size(aligned_sz), is_free(false), is_temporary(temp),
              allocation_time(std::chrono::steady_clock::now())
        {
        }
    };

    /// Single memory arena for efficient allocation
    class MemoryArena
    {
      public:
        explicit MemoryArena(std::size_t initial_size, std::size_t alignment = 8);
        ~MemoryArena();

        /// Non-copyable, moveable
        MemoryArena(const MemoryArena&) = delete;
        MemoryArena& operator=(const MemoryArena&) = delete;
        MemoryArena(MemoryArena&& other) noexcept;
        MemoryArena& operator=(MemoryArena&& other) noexcept;

        /// Allocate memory from arena
        void* allocate(std::size_t size, bool temporary = false);

        /// Deallocate specific memory block
        void deallocate(void* ptr);

        /// Reset all temporary allocations
        void reset_temporary();

        /// Reset entire arena
        void reset_all();

        /// Defragment arena memory
        void defragment();

        /// Check if pointer belongs to this arena
        bool owns(void* ptr) const;

        /// Get current usage statistics
        struct ArenaUsage {
            std::size_t total_size{0};
            std::size_t used_bytes{0};
            std::size_t free_bytes{0};
            std::size_t wasted_bytes{0};
            std::size_t largest_free_block{0};
            std::size_t active_blocks{0};
            double fragmentation_ratio{0.0};
            double utilization_ratio{0.0};
        };

        ArenaUsage get_usage() const;

        /// Get detailed block information (for debugging)
        std::vector<ArenaBlock> get_block_info() const;

      private:
        std::size_t alignment_;
        std::size_t total_size_;
        std::unique_ptr<char[]> memory_;
        char* current_pos_;
        char* end_pos_;

        mutable std::mutex blocks_mutex_;
        std::vector<ArenaBlock> blocks_;

        /// Helper methods
        std::size_t align_size(std::size_t size) const;
        void* allocate_from_free_list(std::size_t aligned_size, bool temporary);
        void* allocate_sequential(std::size_t aligned_size, bool temporary);
        void expand_arena(std::size_t min_additional_size);
        void coalesce_free_blocks();
        void update_pointers_after_defrag(const std::unordered_map<void*, void*>& relocation_map);
    };

    /// Per-connection memory arena manager
    class ConnectionMemoryArena
    {
      public:
        explicit ConnectionMemoryArena(
            const std::string& connection_id,
            const ConnectionArenaConfig& config = ConnectionArenaConfig{});
        ~ConnectionMemoryArena();

        /// Non-copyable, non-moveable
        ConnectionMemoryArena(const ConnectionMemoryArena&) = delete;
        ConnectionMemoryArena& operator=(const ConnectionMemoryArena&) = delete;
        ConnectionMemoryArena(ConnectionMemoryArena&&) = delete;
        ConnectionMemoryArena& operator=(ConnectionMemoryArena&&) = delete;

        /// Allocation interface
        void* allocate(std::size_t size, bool temporary = true);
        void deallocate(void* ptr);

        /// Templated allocation for type safety
        template <typename T, typename... Args>
        T* allocate_object(bool temporary = true, Args&&... args)
        {
            T* ptr = static_cast<T*>(allocate(sizeof(T), temporary));
            if (ptr) {
                new (ptr) T(std::forward<Args>(args)...);
            }
            return ptr;
        }

        template <typename T> void deallocate_object(T* ptr)
        {
            if (ptr) {
                ptr->~T();
                deallocate(static_cast<void*>(ptr));
            }
        }

        /// Batch allocation for arrays
        template <typename T> T* allocate_array(std::size_t count, bool temporary = true)
        {
            return static_cast<T*>(allocate(sizeof(T) * count, temporary));
        }

        template <typename T> void deallocate_array(T* ptr, std::size_t count)
        {
            if (ptr) {
                for (std::size_t i = 0; i < count; ++i) {
                    ptr[i].~T();
                }
                deallocate(static_cast<void*>(ptr));
            }
        }

        /// Query lifecycle management
        void begin_query();
        void end_query();
        void reset_temporary_allocations();

        /// Memory management
        void trigger_cleanup(bool aggressive = false);
        void defragment();

        /// Configuration and monitoring
        const std::string& get_connection_id() const
        {
            return connection_id_;
        }
        const ConnectionArenaConfig& get_config() const
        {
            return config_;
        }
        ConnectionArenaStats get_statistics() const;
        void reset_statistics();

        /// Memory pressure handling
        bool is_under_memory_pressure() const;
        void handle_memory_pressure();

        /// Debugging and diagnostics
        std::string generate_usage_report() const;
        std::vector<std::pair<std::size_t, std::string>> get_allocation_breakdown() const;
        void dump_allocation_state(const std::string& filename) const;

      private:
        std::string connection_id_;
        ConnectionArenaConfig config_;

        /// Memory arenas
        std::unique_ptr<MemoryArena> temporary_arena_;
        std::unique_ptr<MemoryArena> persistent_arena_;

        /// Allocation tracking
        mutable std::mutex allocation_mutex_;
        std::unordered_map<void*, std::size_t> active_allocations_;
        std::unordered_set<void*> temporary_allocations_;

        /// Query state
        std::atomic<std::uint64_t> current_query_id_{0};
        std::atomic<std::uint64_t> queries_processed_{0};
        std::atomic<bool> in_query_{false};

        /// Statistics
        mutable ConnectionArenaStats stats_;
        mutable std::mutex stats_mutex_;

        /// Helper methods
        void track_allocation(void* ptr, std::size_t size, bool temporary);
        void untrack_allocation(void* ptr);
        void update_usage_stats();
        void check_reset_frequency();
        MemoryArena* get_appropriate_arena(std::size_t size, bool temporary);

        /// Performance timing
        struct AllocationTimer {
            std::chrono::high_resolution_clock::time_point start;
            AllocationTimer() : start(std::chrono::high_resolution_clock::now()) {}
            std::chrono::nanoseconds elapsed() const
            {
                return std::chrono::high_resolution_clock::now() - start;
            }
        };
    };

    /// Global connection arena manager
    class ConnectionArenaManager
    {
      public:
        explicit ConnectionArenaManager(
            const ConnectionArenaConfig& default_config = ConnectionArenaConfig{});
        ~ConnectionArenaManager();

        /// Non-copyable, non-moveable
        ConnectionArenaManager(const ConnectionArenaManager&) = delete;
        ConnectionArenaManager& operator=(const ConnectionArenaManager&) = delete;
        ConnectionArenaManager(ConnectionArenaManager&&) = delete;
        ConnectionArenaManager& operator=(ConnectionArenaManager&&) = delete;

        /// Connection lifecycle
        std::shared_ptr<ConnectionMemoryArena>
        create_connection_arena(const std::string& connection_id);
        std::shared_ptr<ConnectionMemoryArena>
        get_connection_arena(const std::string& connection_id);
        void remove_connection_arena(const std::string& connection_id);

        /// Bulk operations
        void
        cleanup_idle_connections(std::chrono::seconds idle_threshold = std::chrono::seconds(300));
        void trigger_global_cleanup(bool aggressive = false);
        void defragment_all_arenas();

        /// Configuration management
        void update_default_config(const ConnectionArenaConfig& config);
        const ConnectionArenaConfig& get_default_config() const
        {
            return default_config_;
        }

        /// Global statistics
        struct GlobalArenaStats {
            std::size_t active_connections{0};
            std::size_t total_connections_created{0};
            std::uint64_t total_memory_allocated{0};
            std::uint64_t total_memory_in_use{0};
            std::uint64_t peak_memory_usage{0};
            double average_memory_per_connection{0.0};
            double global_fragmentation_ratio{0.0};
            std::chrono::steady_clock::time_point last_cleanup;
        };

        GlobalArenaStats get_global_statistics() const;
        std::vector<ConnectionArenaStats> get_all_connection_stats() const;

        /// Memory pressure management
        void set_global_memory_limit(std::size_t limit_bytes);
        std::size_t get_global_memory_limit() const
        {
            return global_memory_limit_.load();
        }
        bool is_global_memory_pressure() const;
        void handle_global_memory_pressure();

        /// Monitoring and reporting
        std::string generate_global_report() const;
        void export_statistics(const std::string& filename) const;

        /// Global instance management
        static ConnectionArenaManager& get_global_instance();
        static void set_global_instance(std::unique_ptr<ConnectionArenaManager> manager);

      private:
        ConnectionArenaConfig default_config_;

        mutable std::mutex arenas_mutex_;
        std::unordered_map<std::string, std::shared_ptr<ConnectionMemoryArena>> connection_arenas_;

        /// Global statistics tracking
        mutable std::mutex stats_mutex_;
        std::atomic<std::size_t> total_connections_created_{0};
        std::atomic<std::uint64_t> peak_memory_usage_{0};
        std::atomic<std::size_t> global_memory_limit_{SIZE_MAX};

        /// Background maintenance
        std::atomic<bool> maintenance_enabled_{true};
        std::thread maintenance_thread_;
        std::mutex maintenance_mutex_;
        std::condition_variable maintenance_cv_;

        /// Global instance
        static std::unique_ptr<ConnectionArenaManager> global_instance_;
        static std::mutex global_instance_mutex_;

        /// Helper methods
        void start_maintenance_thread();
        void stop_maintenance_thread();
        void maintenance_loop();
        void cleanup_expired_connections();
        std::uint64_t calculate_total_memory_usage() const;
        void update_peak_memory_usage(std::uint64_t current_usage);
    };

    /// RAII helper for connection arena allocation
    template <typename T> class ArenaPtr
    {
      public:
        ArenaPtr(ConnectionMemoryArena& arena, bool temporary = true)
            : arena_(arena), ptr_(arena_.allocate_object<T>(temporary)), temporary_(temporary)
        {
        }

        template <typename... Args>
        ArenaPtr(ConnectionMemoryArena& arena, bool temporary, Args&&... args)
            : arena_(arena),
              ptr_(arena_.allocate_object<T>(temporary, std::forward<Args>(args)...)),
              temporary_(temporary)
        {
        }

        ~ArenaPtr()
        {
            if (ptr_) {
                arena_.deallocate_object(ptr_);
            }
        }

        /// Non-copyable, moveable
        ArenaPtr(const ArenaPtr&) = delete;
        ArenaPtr& operator=(const ArenaPtr&) = delete;

        ArenaPtr(ArenaPtr&& other) noexcept
            : arena_(other.arena_), ptr_(other.ptr_), temporary_(other.temporary_)
        {
            other.ptr_ = nullptr;
        }

        ArenaPtr& operator=(ArenaPtr&& other) noexcept
        {
            if (this != &other) {
                reset();
                arena_ = other.arena_;
                ptr_ = other.ptr_;
                temporary_ = other.temporary_;
                other.ptr_ = nullptr;
            }
            return *this;
        }

        T& operator*() const
        {
            return *ptr_;
        }
        T* operator->() const
        {
            return ptr_;
        }
        T* get() const
        {
            return ptr_;
        }

        explicit operator bool() const
        {
            return ptr_ != nullptr;
        }

        void reset()
        {
            if (ptr_) {
                arena_.deallocate_object(ptr_);
                ptr_ = nullptr;
            }
        }

        T* release()
        {
            T* result = ptr_;
            ptr_ = nullptr;
            return result;
        }

      private:
        ConnectionMemoryArena& arena_;
        T* ptr_;
        bool temporary_;
    };

    /// Convenience functions
    template <typename T, typename... Args>
    ArenaPtr<T> make_arena_unique(ConnectionMemoryArena& arena, bool temporary = true,
                                  Args&&... args)
    {
        return ArenaPtr<T>(arena, temporary, std::forward<Args>(args)...);
    }

    /// STL-compatible allocator for connection arena
    template <typename T> class connection_arena_allocator
    {
      public:
        using value_type = T;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        template <typename U> struct rebind {
            using other = connection_arena_allocator<U>;
        };

        explicit connection_arena_allocator(ConnectionMemoryArena& arena,
                                            bool temporary = true) noexcept
            : arena_(arena), temporary_(temporary)
        {
        }

        template <typename U>
        connection_arena_allocator(const connection_arena_allocator<U>& other) noexcept
            : arena_(other.arena_), temporary_(other.temporary_)
        {
        }

        pointer allocate(size_type n)
        {
            return static_cast<pointer>(arena_.allocate(n * sizeof(T), temporary_));
        }

        void deallocate(pointer p, size_type n)
        {
            arena_.deallocate(static_cast<void*>(p));
        }

        template <typename U, typename... Args> void construct(U* p, Args&&... args)
        {
            new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
        }

        template <typename U> void destroy(U* p)
        {
            p->~U();
        }

        bool operator==(const connection_arena_allocator& other) const noexcept
        {
            return &arena_ == &other.arena_ && temporary_ == other.temporary_;
        }

        bool operator!=(const connection_arena_allocator& other) const noexcept
        {
            return !(*this == other);
        }

      private:
        ConnectionMemoryArena& arena_;
        bool temporary_;

        template <typename U> friend class connection_arena_allocator;
    };

    /// Connection arena utilities
    namespace arena_utils
    {
        /// Get optimal arena configuration based on connection type
        ConnectionArenaConfig get_config_for_connection_type(const std::string& connection_type);

        /// Analyze memory usage patterns
        struct MemoryPattern {
            std::vector<std::size_t> allocation_sizes;
            std::vector<std::chrono::milliseconds> allocation_lifetimes;
            double temporary_ratio{0.0};
            double peak_usage_ratio{0.0};
            std::chrono::milliseconds average_query_duration{0};
        };

        MemoryPattern analyze_connection_patterns(const ConnectionMemoryArena& arena);

        /// Optimize arena configuration based on patterns
        ConnectionArenaConfig optimize_config(const MemoryPattern& pattern,
                                              const ConnectionArenaConfig& base_config);

        /// Memory leak detection
        struct LeakReport {
            std::size_t leaked_allocations{0};
            std::size_t leaked_bytes{0};
            std::vector<std::pair<void*, std::size_t>> leak_details;
            std::chrono::steady_clock::time_point oldest_leak;
        };

        LeakReport detect_memory_leaks(const ConnectionMemoryArena& arena);
    } // namespace arena_utils

} // namespace scratchbird::engine
