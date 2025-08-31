// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace scratchbird::engine
{

    /// Configuration for pool allocator system
    struct PoolAllocatorConfig {
        /// Size classes for fixed-size pools (in bytes)
        std::vector<std::size_t> size_classes{8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096};

        /// Initial number of objects per pool
        std::size_t initial_pool_capacity{1024};

        /// Pool growth factor when expanding
        double growth_factor{1.5};

        /// Maximum number of free objects to keep in pool
        std::size_t max_free_objects{10000};

        /// Large object threshold (objects larger than this use separate allocator)
        std::size_t large_object_threshold{8192};

        /// Thread-local cache size per size class
        std::size_t thread_cache_size{128};

        /// Enable detailed statistics collection
        bool enable_statistics{true};

        /// Memory alignment requirement
        std::size_t alignment{8};

        /// Enable NUMA-aware allocation
        bool numa_aware{true};
    };

    /// Statistics for pool allocator performance monitoring
    struct PoolAllocatorStats {
        /// Default constructor
        PoolAllocatorStats() = default;

        /// Copy constructor - copy the values (not atomic references)
        PoolAllocatorStats(const PoolAllocatorStats& other);

        /// Assignment operator
        PoolAllocatorStats& operator=(const PoolAllocatorStats& other);
        /// Per-size-class statistics
        struct SizeClassStats {
            std::size_t size_class{0};                   ///< Size class in bytes
            std::atomic<std::uint64_t> allocations{0};   ///< Total allocations
            std::atomic<std::uint64_t> deallocations{0}; ///< Total deallocations
            std::atomic<std::uint64_t> pool_hits{0};     ///< Allocations served from pool
            std::atomic<std::uint64_t> pool_misses{0};   ///< Allocations that required new memory
            std::atomic<std::uint64_t> thread_cache_hits{0}; ///< Thread-local cache hits
            std::atomic<std::uint64_t> objects_in_use{0};    ///< Currently allocated objects
            std::atomic<std::uint64_t> peak_objects{0};      ///< Peak number of objects
            std::atomic<std::uint64_t> memory_allocated{0};  ///< Total memory allocated (bytes)
            std::atomic<std::uint64_t> memory_peak{0};       ///< Peak memory usage (bytes)

            // Default constructor
            SizeClassStats() = default;

            // Copy constructor - copy the values (not atomic references)
            SizeClassStats(const SizeClassStats& other)
                : size_class(other.size_class), allocations(other.allocations.load()),
                  deallocations(other.deallocations.load()), pool_hits(other.pool_hits.load()),
                  pool_misses(other.pool_misses.load()),
                  thread_cache_hits(other.thread_cache_hits.load()),
                  objects_in_use(other.objects_in_use.load()),
                  peak_objects(other.peak_objects.load()),
                  memory_allocated(other.memory_allocated.load()),
                  memory_peak(other.memory_peak.load())
            {
            }

            // Assignment operator
            SizeClassStats& operator=(const SizeClassStats& other)
            {
                if (this != &other) {
                    size_class = other.size_class;
                    allocations.store(other.allocations.load());
                    deallocations.store(other.deallocations.load());
                    pool_hits.store(other.pool_hits.load());
                    pool_misses.store(other.pool_misses.load());
                    thread_cache_hits.store(other.thread_cache_hits.load());
                    objects_in_use.store(other.objects_in_use.load());
                    peak_objects.store(other.peak_objects.load());
                    memory_allocated.store(other.memory_allocated.load());
                    memory_peak.store(other.memory_peak.load());
                }
                return *this;
            }
        };

        std::vector<SizeClassStats> size_class_stats;

        /// Large object allocator statistics
        std::atomic<std::uint64_t> large_allocations{0};
        std::atomic<std::uint64_t> large_deallocations{0};
        std::atomic<std::uint64_t> large_memory_allocated{0};
        std::atomic<std::uint64_t> large_memory_peak{0};

        /// Overall performance metrics
        std::atomic<std::uint64_t> total_allocations{0};
        std::atomic<std::uint64_t> total_deallocations{0};
        std::atomic<std::chrono::nanoseconds::rep> total_allocation_time_ns{0};
        std::atomic<std::chrono::nanoseconds::rep> total_deallocation_time_ns{0};

        /// Fragmentation and efficiency metrics
        std::atomic<std::uint64_t> internal_fragmentation_bytes{
            0}; ///< Wasted space within allocations
        std::atomic<std::uint64_t> external_fragmentation_bytes{
            0};                                          ///< Wasted space between allocations
        std::atomic<double> pool_utilization_ratio{0.0}; ///< Pool memory utilization
    };

    /// Individual memory pool for a specific size class
    class MemoryPool
    {
      public:
        explicit MemoryPool(std::size_t object_size, std::size_t initial_capacity = 1024);
        ~MemoryPool();

        /// Non-copyable, non-moveable
        MemoryPool(const MemoryPool&) = delete;
        MemoryPool& operator=(const MemoryPool&) = delete;
        MemoryPool(MemoryPool&&) = delete;
        MemoryPool& operator=(MemoryPool&&) = delete;

        /// Allocate object from pool (lock-free fast path)
        void* allocate();

        /// Return object to pool (lock-free fast path)
        void deallocate(void* ptr);

        /// Get current pool statistics
        PoolAllocatorStats::SizeClassStats get_stats() const;

        /// Reset statistics
        void reset_stats();

        /// Check if pointer belongs to this pool
        bool owns(void* ptr) const;

        /// Get object size for this pool
        std::size_t get_object_size() const
        {
            return object_size_;
        }

        /// Get number of available objects
        std::size_t get_available_count() const;

        /// Pre-allocate objects to avoid allocation during runtime
        void prefill(std::size_t count);

      private:
        const std::size_t object_size_;
        const std::size_t initial_capacity_;

        /// Pool chunks (large blocks containing many objects)
        struct Chunk {
            std::unique_ptr<char[]> memory;
            std::size_t size;
            std::size_t object_count;
            std::atomic<bool> in_use{true};

            Chunk(std::size_t chunk_size, std::size_t obj_size);
            void* get_object(std::size_t index) const;
            std::size_t get_object_index(void* ptr) const;
            bool contains(void* ptr) const;
        };

        mutable std::mutex chunks_mutex_;
        std::vector<std::unique_ptr<Chunk>> chunks_;

        /// Free list (lock-free stack)
        struct FreeNode {
            std::atomic<FreeNode*> next;
        };
        std::atomic<FreeNode*> free_list_;

        /// Statistics
        mutable PoolAllocatorStats::SizeClassStats stats_;

        /// Helper methods
        void expand_pool();
        std::unique_ptr<Chunk> create_chunk(std::size_t object_count);
        void add_to_free_list(void* ptr);
        void* pop_from_free_list();
    };

    /// Thread-local cache for fast allocation
    class ThreadLocalCache
    {
      public:
        ThreadLocalCache(std::size_t cache_size, std::size_t object_size);
        ~ThreadLocalCache();

        /// Try to allocate from cache (returns nullptr if cache is empty)
        void* try_allocate();

        /// Return object to cache (returns false if cache is full)
        bool try_deallocate(void* ptr);

        /// Fill cache from central pool
        void refill_from_pool(MemoryPool& pool);

        /// Return objects to central pool
        void drain_to_pool(MemoryPool& pool);

        /// Get cache statistics
        struct CacheStats {
            std::size_t hits{0};
            std::size_t misses{0};
            std::size_t current_size{0};
            std::size_t peak_size{0};
        };

        CacheStats get_stats() const;

      private:
        const std::size_t cache_size_;
        const std::size_t object_size_;

        std::vector<void*> cached_objects_;
        std::size_t current_size_{0};

        /// Statistics (not atomic since thread-local)
        mutable CacheStats stats_;
    };

    /// Large object allocator for objects exceeding pool threshold
    class LargeObjectAllocator
    {
      public:
        LargeObjectAllocator();
        ~LargeObjectAllocator();

        /// Allocate large object
        void* allocate(std::size_t size, std::size_t alignment = 8);

        /// Deallocate large object
        void deallocate(void* ptr, std::size_t size);

        /// Check if pointer was allocated by this allocator
        bool owns(void* ptr) const;

        /// Get statistics
        struct LargeObjectStats {
            std::atomic<std::uint64_t> allocations{0};
            std::atomic<std::uint64_t> deallocations{0};
            std::atomic<std::uint64_t> bytes_allocated{0};
            std::atomic<std::uint64_t> bytes_deallocated{0};
            std::atomic<std::uint64_t> peak_memory{0};
            std::atomic<std::uint64_t> current_memory{0};

            // Default constructor
            LargeObjectStats() = default;

            // Copy constructor
            LargeObjectStats(const LargeObjectStats& other)
                : allocations(other.allocations.load()), deallocations(other.deallocations.load()),
                  bytes_allocated(other.bytes_allocated.load()),
                  bytes_deallocated(other.bytes_deallocated.load()),
                  peak_memory(other.peak_memory.load()), current_memory(other.current_memory.load())
            {
            }

            // Assignment operator
            LargeObjectStats& operator=(const LargeObjectStats& other)
            {
                if (this != &other) {
                    allocations.store(other.allocations.load());
                    deallocations.store(other.deallocations.load());
                    bytes_allocated.store(other.bytes_allocated.load());
                    bytes_deallocated.store(other.bytes_deallocated.load());
                    peak_memory.store(other.peak_memory.load());
                    current_memory.store(other.current_memory.load());
                }
                return *this;
            }
        };

        LargeObjectStats get_stats() const;
        void reset_stats();

      private:
        mutable std::mutex allocations_mutex_;
        std::unordered_map<void*, std::size_t> active_allocations_;

        mutable LargeObjectStats stats_;

        /// Helper methods
        void* allocate_aligned(std::size_t size, std::size_t alignment);
        void track_allocation(void* ptr, std::size_t size);
        void untrack_allocation(void* ptr);
    };

    /// Main pool allocator class
    class PoolAllocator
    {
      public:
        explicit PoolAllocator(const PoolAllocatorConfig& config = PoolAllocatorConfig{});
        ~PoolAllocator();

        /// Non-copyable, non-moveable
        PoolAllocator(const PoolAllocator&) = delete;
        PoolAllocator& operator=(const PoolAllocator&) = delete;
        PoolAllocator(PoolAllocator&&) = delete;
        PoolAllocator& operator=(PoolAllocator&&) = delete;

        /// Main allocation interface
        void* allocate(std::size_t size, std::size_t alignment = 8);
        void deallocate(void* ptr, std::size_t size);

        /// Templated allocation for type safety
        template <typename T> T* allocate()
        {
            return static_cast<T*>(allocate(sizeof(T), alignof(T)));
        }

        template <typename T> void deallocate(T* ptr)
        {
            deallocate(static_cast<void*>(ptr), sizeof(T));
        }

        /// Batch allocation for efficiency
        std::vector<void*> allocate_batch(std::size_t size, std::size_t count);
        void deallocate_batch(const std::vector<void*>& ptrs, std::size_t size);

        /// Configuration and monitoring
        const PoolAllocatorConfig& get_config() const
        {
            return config_;
        }
        PoolAllocatorStats get_statistics() const;
        void reset_statistics();

        /// Performance utilities
        void prefill_pools(); ///< Pre-allocate objects for better startup performance
        void trim_pools();    ///< Release unused memory back to system
        double get_fragmentation_ratio() const; ///< Calculate memory fragmentation

        /// NUMA awareness
        void set_numa_node(int node);
        int get_numa_node() const
        {
            return numa_node_;
        }

        /// Global instance management
        static PoolAllocator& get_global_instance();
        static void set_global_instance(std::unique_ptr<PoolAllocator> allocator);

      private:
        PoolAllocatorConfig config_;

        /// Memory pools for different size classes
        std::vector<std::unique_ptr<MemoryPool>> pools_;

        /// Large object allocator
        std::unique_ptr<LargeObjectAllocator> large_allocator_;

        /// Thread-local caches
        thread_local static std::vector<std::unique_ptr<ThreadLocalCache>> thread_caches_;

        /// NUMA node affinity
        std::atomic<int> numa_node_{-1};

        /// Statistics
        mutable PoolAllocatorStats stats_;

        /// Global instance
        static std::unique_ptr<PoolAllocator> global_instance_;
        static std::mutex global_instance_mutex_;

        /// Helper methods
        std::size_t find_size_class(std::size_t size) const;
        MemoryPool* get_pool_for_size(std::size_t size);
        ThreadLocalCache* get_thread_cache(std::size_t size_class_index);
        void initialize_thread_caches();
        void cleanup_thread_caches();

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

    /// RAII wrapper for pool-allocated objects
    template <typename T> class PoolPtr
    {
      public:
        explicit PoolPtr(PoolAllocator& allocator = PoolAllocator::get_global_instance())
            : allocator_(allocator), ptr_(allocator_.allocate<T>())
        {
            new (ptr_) T();
        }

        template <typename... Args>
        PoolPtr(PoolAllocator& allocator, Args&&... args)
            : allocator_(allocator), ptr_(allocator_.allocate<T>())
        {
            new (ptr_) T(std::forward<Args>(args)...);
        }

        ~PoolPtr()
        {
            if (ptr_) {
                ptr_->~T();
                allocator_.deallocate(ptr_);
            }
        }

        /// Non-copyable, moveable
        PoolPtr(const PoolPtr&) = delete;
        PoolPtr& operator=(const PoolPtr&) = delete;

        PoolPtr(PoolPtr&& other) noexcept : allocator_(other.allocator_), ptr_(other.ptr_)
        {
            other.ptr_ = nullptr;
        }

        PoolPtr& operator=(PoolPtr&& other) noexcept
        {
            if (this != &other) {
                reset();
                allocator_ = other.allocator_;
                ptr_ = other.ptr_;
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
                ptr_->~T();
                allocator_.deallocate(ptr_);
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
        PoolAllocator& allocator_;
        T* ptr_;
    };

    /// Convenience functions for pool allocation
    template <typename T, typename... Args> PoolPtr<T> make_pool_unique(Args&&... args)
    {
        return PoolPtr<T>(PoolAllocator::get_global_instance(), std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    PoolPtr<T> make_pool_unique(PoolAllocator& allocator, Args&&... args)
    {
        return PoolPtr<T>(allocator, std::forward<Args>(args)...);
    }

    /// STL-compatible allocator for pool allocation
    template <typename T> class pool_allocator
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
            using other = pool_allocator<U>;
        };

        pool_allocator() noexcept : pool_(PoolAllocator::get_global_instance()) {}
        explicit pool_allocator(PoolAllocator& pool) noexcept : pool_(pool) {}

        template <typename U>
        pool_allocator(const pool_allocator<U>& other) noexcept : pool_(other.pool_)
        {
        }

        pointer allocate(size_type n)
        {
            return static_cast<pointer>(pool_.allocate(n * sizeof(T), alignof(T)));
        }

        void deallocate(pointer p, size_type n)
        {
            pool_.deallocate(static_cast<void*>(p), n * sizeof(T));
        }

        template <typename U, typename... Args> void construct(U* p, Args&&... args)
        {
            new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
        }

        template <typename U> void destroy(U* p)
        {
            p->~U();
        }

        bool operator==(const pool_allocator& other) const noexcept
        {
            return &pool_ == &other.pool_;
        }

        bool operator!=(const pool_allocator& other) const noexcept
        {
            return !(*this == other);
        }

      private:
        PoolAllocator& pool_;

        template <typename U> friend class pool_allocator;
    };

    /// Utility functions
    namespace pool_utils
    {
        /// Get optimal pool configuration for current system
        PoolAllocatorConfig get_optimal_config();

        /// Benchmark allocator performance
        struct BenchmarkResults {
            double allocations_per_second{0.0};
            double deallocations_per_second{0.0};
            double average_allocation_time_ns{0.0};
            double memory_efficiency_ratio{0.0};
            double fragmentation_ratio{0.0};
        };

        BenchmarkResults benchmark_allocator(PoolAllocator& allocator, std::size_t object_size,
                                             std::size_t operation_count = 1000000);

        /// Memory usage analysis
        struct MemoryUsageReport {
            std::size_t total_allocated_bytes{0};
            std::size_t total_wasted_bytes{0};
            std::size_t internal_fragmentation{0};
            std::size_t external_fragmentation{0};
            double efficiency_ratio{0.0};
            std::vector<std::pair<std::size_t, std::size_t>> size_class_usage; // size -> bytes
        };

        MemoryUsageReport analyze_memory_usage(const PoolAllocator& allocator);

        /// System memory information
        struct SystemMemoryInfo {
            std::size_t total_physical_memory{0};
            std::size_t available_memory{0};
            std::size_t cache_line_size{64};
            std::size_t page_size{4096};
            int numa_nodes{1};
            bool has_numa_support{false};
        };

        SystemMemoryInfo get_system_memory_info();
    } // namespace pool_utils

} // namespace scratchbird::engine
