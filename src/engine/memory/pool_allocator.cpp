// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include "scratchbird/engine/pool_allocator.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>
#include <stdexcept>

#ifdef __linux__
#include <numa.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace scratchbird::engine
{

    //==============================================================================
    // PoolAllocatorStats Implementation
    //==============================================================================

    PoolAllocatorStats::PoolAllocatorStats(const PoolAllocatorStats& other)
    {
        // Copy size class stats
        size_class_stats.resize(other.size_class_stats.size());
        for (std::size_t i = 0; i < other.size_class_stats.size(); ++i) {
            size_class_stats[i] = other.size_class_stats[i];
        }

        // Copy atomic values
        large_allocations.store(other.large_allocations.load());
        large_deallocations.store(other.large_deallocations.load());
        large_memory_allocated.store(other.large_memory_allocated.load());
        large_memory_peak.store(other.large_memory_peak.load());
        total_allocations.store(other.total_allocations.load());
        total_deallocations.store(other.total_deallocations.load());
        total_allocation_time_ns.store(other.total_allocation_time_ns.load());
        total_deallocation_time_ns.store(other.total_deallocation_time_ns.load());
        internal_fragmentation_bytes.store(other.internal_fragmentation_bytes.load());
        external_fragmentation_bytes.store(other.external_fragmentation_bytes.load());
        pool_utilization_ratio.store(other.pool_utilization_ratio.load());
    }

    PoolAllocatorStats& PoolAllocatorStats::operator=(const PoolAllocatorStats& other)
    {
        if (this != &other) {
            // Copy size class stats
            size_class_stats.resize(other.size_class_stats.size());
            for (std::size_t i = 0; i < other.size_class_stats.size(); ++i) {
                size_class_stats[i] = other.size_class_stats[i];
            }

            // Copy atomic values
            large_allocations.store(other.large_allocations.load());
            large_deallocations.store(other.large_deallocations.load());
            large_memory_allocated.store(other.large_memory_allocated.load());
            large_memory_peak.store(other.large_memory_peak.load());
            total_allocations.store(other.total_allocations.load());
            total_deallocations.store(other.total_deallocations.load());
            total_allocation_time_ns.store(other.total_allocation_time_ns.load());
            total_deallocation_time_ns.store(other.total_deallocation_time_ns.load());
            internal_fragmentation_bytes.store(other.internal_fragmentation_bytes.load());
            external_fragmentation_bytes.store(other.external_fragmentation_bytes.load());
            pool_utilization_ratio.store(other.pool_utilization_ratio.load());
        }
        return *this;
    }

    // MemoryPool::Chunk implementation

    MemoryPool::Chunk::Chunk(std::size_t chunk_size, std::size_t obj_size)
        : size(chunk_size), object_count(chunk_size / obj_size)
    {
        memory = std::make_unique<char[]>(chunk_size);
        if (!memory) {
            throw std::bad_alloc();
        }

        // Initialize all objects in the chunk
        std::memset(memory.get(), 0, chunk_size);
    }

    void* MemoryPool::Chunk::get_object(std::size_t index) const
    {
        if (index >= object_count) {
            return nullptr;
        }
        return memory.get() + (index * (size / object_count));
    }

    std::size_t MemoryPool::Chunk::get_object_index(void* ptr) const
    {
        if (!contains(ptr)) {
            return SIZE_MAX;
        }

        char* char_ptr = static_cast<char*>(ptr);
        std::size_t offset = char_ptr - memory.get();
        std::size_t obj_size = size / object_count;
        return offset / obj_size;
    }

    bool MemoryPool::Chunk::contains(void* ptr) const
    {
        char* char_ptr = static_cast<char*>(ptr);
        return char_ptr >= memory.get() && char_ptr < memory.get() + size;
    }

    // MemoryPool implementation

    MemoryPool::MemoryPool(std::size_t object_size, std::size_t initial_capacity)
        : object_size_(std::max({object_size, sizeof(void*), sizeof(FreeNode)})),
          initial_capacity_(initial_capacity), free_list_(nullptr)
    {
        if (object_size == 0) {
            throw std::invalid_argument("Object size must be greater than 0");
        }

        // Initialize statistics
        stats_.size_class = object_size_;

        // Create initial chunk
        expand_pool();
    }

    MemoryPool::~MemoryPool()
    {
        // All chunks are automatically cleaned up by unique_ptr
    }

    void* MemoryPool::allocate()
    {
        // Fast path: try to pop from free list (lock-free)
        void* ptr = pop_from_free_list();
        if (ptr) {
            stats_.allocations.fetch_add(1);
            stats_.pool_hits.fetch_add(1);
            stats_.objects_in_use.fetch_add(1);

            // Update peak usage
            std::uint64_t current_usage = stats_.objects_in_use.load();
            std::uint64_t peak = stats_.peak_objects.load();
            while (current_usage > peak &&
                   !stats_.peak_objects.compare_exchange_weak(peak, current_usage)) {
                // Retry if another thread updated peak
            }

            return ptr;
        }

        // Slow path: need to expand pool
        {
            std::lock_guard<std::mutex> lock(chunks_mutex_);

            // Double-check after acquiring lock
            ptr = pop_from_free_list();
            if (ptr) {
                stats_.allocations.fetch_add(1);
                stats_.pool_hits.fetch_add(1);
                stats_.objects_in_use.fetch_add(1);
                return ptr;
            }

            // Expand pool and try again
            expand_pool();
            ptr = pop_from_free_list();

            if (ptr) {
                stats_.allocations.fetch_add(1);
                stats_.pool_misses.fetch_add(1);
                stats_.objects_in_use.fetch_add(1);
            }

            return ptr;
        }
    }

    void MemoryPool::deallocate(void* ptr)
    {
        if (!ptr) {
            return;
        }

        // Verify pointer belongs to this pool (debug check)
        if (!owns(ptr)) {
            // In a production system, this would be a critical error
            return;
        }

        // Return to free list (lock-free)
        add_to_free_list(ptr);

        stats_.deallocations.fetch_add(1);
        stats_.objects_in_use.fetch_sub(1);
    }

    bool MemoryPool::owns(void* ptr) const
    {
        std::lock_guard<std::mutex> lock(chunks_mutex_);

        for (const auto& chunk : chunks_) {
            if (chunk->contains(ptr)) {
                return true;
            }
        }
        return false;
    }

    std::size_t MemoryPool::get_available_count() const
    {
        // Count free list nodes (approximate since it's lock-free)
        std::size_t count = 0;
        FreeNode* current = free_list_.load();

        while (current && count < 10000) { // Prevent infinite loops
            current = current->next.load();
            count++;
        }

        return count;
    }

    void MemoryPool::prefill(std::size_t count)
    {
        std::vector<void*> temp_ptrs;
        temp_ptrs.reserve(count);

        // Allocate requested number of objects
        for (std::size_t i = 0; i < count; ++i) {
            void* ptr = allocate();
            if (ptr) {
                temp_ptrs.push_back(ptr);
            } else {
                break;
            }
        }

        // Return them all to free list
        for (void* ptr : temp_ptrs) {
            deallocate(ptr);
        }
    }

    PoolAllocatorStats::SizeClassStats MemoryPool::get_stats() const
    {
        return stats_;
    }

    void MemoryPool::reset_stats()
    {
        stats_.allocations.store(0);
        stats_.deallocations.store(0);
        stats_.pool_hits.store(0);
        stats_.pool_misses.store(0);
        stats_.thread_cache_hits.store(0);
        stats_.objects_in_use.store(0);
        stats_.peak_objects.store(0);
        stats_.memory_allocated.store(0);
        stats_.memory_peak.store(0);
    }

    void MemoryPool::expand_pool()
    {
        // Calculate new chunk size (exponential growth)
        std::size_t new_chunk_objects = initial_capacity_;
        if (!chunks_.empty()) {
            new_chunk_objects = static_cast<std::size_t>(chunks_.back()->object_count * 1.5);
        }

        std::size_t chunk_size = new_chunk_objects * object_size_;

        // Create new chunk
        auto new_chunk = create_chunk(new_chunk_objects);
        if (!new_chunk) {
            throw std::bad_alloc();
        }

        // Add all objects from the new chunk to the free list
        for (std::size_t i = 0; i < new_chunk->object_count; ++i) {
            void* obj = new_chunk->get_object(i);
            add_to_free_list(obj);
        }

        // Update statistics
        stats_.memory_allocated.fetch_add(chunk_size);
        std::uint64_t current_memory = stats_.memory_allocated.load();
        std::uint64_t peak_memory = stats_.memory_peak.load();
        while (current_memory > peak_memory &&
               !stats_.memory_peak.compare_exchange_weak(peak_memory, current_memory)) {
            // Retry
        }

        chunks_.push_back(std::move(new_chunk));
    }

    std::unique_ptr<MemoryPool::Chunk> MemoryPool::create_chunk(std::size_t object_count)
    {
        std::size_t chunk_size = object_count * object_size_;
        return std::make_unique<Chunk>(chunk_size, object_size_);
    }

    void MemoryPool::add_to_free_list(void* ptr)
    {
        FreeNode* node = static_cast<FreeNode*>(ptr);
        FreeNode* current_head = free_list_.load();

        do {
            node->next.store(current_head);
        } while (!free_list_.compare_exchange_weak(current_head, node));
    }

    void* MemoryPool::pop_from_free_list()
    {
        FreeNode* head = free_list_.load();

        while (head != nullptr) {
            FreeNode* next = head->next.load();
            if (free_list_.compare_exchange_weak(head, next)) {
                return static_cast<void*>(head);
            }
            // head was updated by compare_exchange_weak on failure
        }

        return nullptr;
    }

    // ThreadLocalCache implementation

    ThreadLocalCache::ThreadLocalCache(std::size_t cache_size, std::size_t object_size)
        : cache_size_(cache_size), object_size_(object_size)
    {
        cached_objects_.reserve(cache_size_);
    }

    ThreadLocalCache::~ThreadLocalCache()
    {
        // Objects will be returned to pool during cleanup
    }

    void* ThreadLocalCache::try_allocate()
    {
        if (current_size_ > 0) {
            void* ptr = cached_objects_[--current_size_];
            stats_.hits++;
            return ptr;
        }

        stats_.misses++;
        return nullptr;
    }

    bool ThreadLocalCache::try_deallocate(void* ptr)
    {
        if (current_size_ < cache_size_) {
            cached_objects_[current_size_++] = ptr;
            if (current_size_ > stats_.peak_size) {
                stats_.peak_size = current_size_;
            }
            return true;
        }
        return false;
    }

    void ThreadLocalCache::refill_from_pool(MemoryPool& pool)
    {
        std::size_t refill_count = cache_size_ / 2;

        for (std::size_t i = 0; i < refill_count && current_size_ < cache_size_; ++i) {
            void* ptr = pool.allocate();
            if (ptr) {
                cached_objects_[current_size_++] = ptr;
            } else {
                break;
            }
        }
    }

    void ThreadLocalCache::drain_to_pool(MemoryPool& pool)
    {
        while (current_size_ > 0) {
            pool.deallocate(cached_objects_[--current_size_]);
        }
    }

    ThreadLocalCache::CacheStats ThreadLocalCache::get_stats() const
    {
        CacheStats result = stats_;
        result.current_size = current_size_;
        return result;
    }

    // LargeObjectAllocator implementation

    LargeObjectAllocator::LargeObjectAllocator() {}

    LargeObjectAllocator::~LargeObjectAllocator()
    {
        // Check for leaks in debug builds
#ifdef DEBUG
        std::lock_guard<std::mutex> lock(allocations_mutex_);
        if (!active_allocations_.empty()) {
            // Log warning about leaked allocations
        }
#endif
    }

    void* LargeObjectAllocator::allocate(std::size_t size, std::size_t alignment)
    {
        void* ptr = allocate_aligned(size, alignment);
        if (ptr) {
            track_allocation(ptr, size);
            stats_.allocations.fetch_add(1);
            stats_.bytes_allocated.fetch_add(size);

            // Update peak memory
            std::uint64_t current = stats_.current_memory.fetch_add(size) + size;
            std::uint64_t peak = stats_.peak_memory.load();
            while (current > peak && !stats_.peak_memory.compare_exchange_weak(peak, current)) {
                // Retry
            }
        }
        return ptr;
    }

    void LargeObjectAllocator::deallocate(void* ptr, std::size_t size)
    {
        if (!ptr) {
            return;
        }

        untrack_allocation(ptr);

        stats_.deallocations.fetch_add(1);
        stats_.bytes_deallocated.fetch_add(size);
        stats_.current_memory.fetch_sub(size);

        std::free(ptr);
    }

    bool LargeObjectAllocator::owns(void* ptr) const
    {
        std::lock_guard<std::mutex> lock(allocations_mutex_);
        return active_allocations_.find(ptr) != active_allocations_.end();
    }

    LargeObjectAllocator::LargeObjectStats LargeObjectAllocator::get_stats() const
    {
        return stats_;
    }

    void LargeObjectAllocator::reset_stats()
    {
        stats_.allocations.store(0);
        stats_.deallocations.store(0);
        stats_.bytes_allocated.store(0);
        stats_.bytes_deallocated.store(0);
        stats_.peak_memory.store(0);
        stats_.current_memory.store(0);
    }

    void* LargeObjectAllocator::allocate_aligned(std::size_t size, std::size_t alignment)
    {
        if (alignment <= alignof(std::max_align_t)) {
            return std::malloc(size);
        }

#ifdef _WIN32
        return _aligned_malloc(size, alignment);
#else
        void* ptr = nullptr;
        if (posix_memalign(&ptr, alignment, size) != 0) {
            return nullptr;
        }
        return ptr;
#endif
    }

    void LargeObjectAllocator::track_allocation(void* ptr, std::size_t size)
    {
        std::lock_guard<std::mutex> lock(allocations_mutex_);
        active_allocations_[ptr] = size;
    }

    void LargeObjectAllocator::untrack_allocation(void* ptr)
    {
        std::lock_guard<std::mutex> lock(allocations_mutex_);
        active_allocations_.erase(ptr);
    }

    // PoolAllocator implementation

    // Static member definitions
    std::unique_ptr<PoolAllocator> PoolAllocator::global_instance_;
    std::mutex PoolAllocator::global_instance_mutex_;
    thread_local std::vector<std::unique_ptr<ThreadLocalCache>> PoolAllocator::thread_caches_;

    PoolAllocator::PoolAllocator(const PoolAllocatorConfig& config) : config_(config)
    {
        // Create pools for each size class
        pools_.reserve(config_.size_classes.size());
        for (std::size_t size_class : config_.size_classes) {
            pools_.push_back(
                std::make_unique<MemoryPool>(size_class, config_.initial_pool_capacity));
        }

        // Create large object allocator
        large_allocator_ = std::make_unique<LargeObjectAllocator>();

        // Initialize thread caches
        initialize_thread_caches();
    }

    PoolAllocator::~PoolAllocator()
    {
        cleanup_thread_caches();
    }

    void* PoolAllocator::allocate(std::size_t size, std::size_t alignment)
    {
        AllocationTimer timer;

        if (size == 0) {
            size = 1;
        }

        // Handle large objects
        if (size > config_.large_object_threshold) {
            void* ptr = large_allocator_->allocate(size, alignment);

            auto elapsed = timer.elapsed();
            auto current_total =
                stats_.total_allocation_time_ns.fetch_add(elapsed.count()) + elapsed.count();
            stats_.total_allocations.fetch_add(1);

            return ptr;
        }

        // Find appropriate size class
        std::size_t size_class_index = find_size_class(size);
        if (size_class_index >= pools_.size()) {
            // Fallback to large object allocator
            return large_allocator_->allocate(size, alignment);
        }

        // Try thread-local cache first
        ThreadLocalCache* cache = get_thread_cache(size_class_index);
        if (cache) {
            void* ptr = cache->try_allocate();
            if (ptr) {
                auto elapsed = timer.elapsed();
                stats_.total_allocation_time_ns.fetch_add(elapsed.count());
                stats_.total_allocations.fetch_add(1);
                return ptr;
            }
        }

        // Get from central pool
        MemoryPool* pool = pools_[size_class_index].get();
        void* ptr = pool->allocate();

        // Try to refill thread cache
        if (cache && ptr) {
            cache->refill_from_pool(*pool);
        }

        auto elapsed = timer.elapsed();
        stats_.total_allocation_time_ns.fetch_add(elapsed.count());
        stats_.total_allocations.fetch_add(1);

        return ptr;
    }

    void PoolAllocator::deallocate(void* ptr, std::size_t size)
    {
        if (!ptr) {
            return;
        }

        AllocationTimer timer;

        // Handle large objects
        if (size > config_.large_object_threshold) {
            large_allocator_->deallocate(ptr, size);

            auto elapsed = timer.elapsed();
            stats_.total_deallocation_time_ns.fetch_add(elapsed.count());
            stats_.total_deallocations.fetch_add(1);
            return;
        }

        // Find appropriate size class
        std::size_t size_class_index = find_size_class(size);
        if (size_class_index >= pools_.size()) {
            large_allocator_->deallocate(ptr, size);
            return;
        }

        // Try thread-local cache first
        ThreadLocalCache* cache = get_thread_cache(size_class_index);
        if (cache && cache->try_deallocate(ptr)) {
            auto elapsed = timer.elapsed();
            stats_.total_deallocation_time_ns.fetch_add(elapsed.count());
            stats_.total_deallocations.fetch_add(1);
            return;
        }

        // Return to central pool
        MemoryPool* pool = pools_[size_class_index].get();
        pool->deallocate(ptr);

        auto elapsed = timer.elapsed();
        stats_.total_deallocation_time_ns.fetch_add(elapsed.count());
        stats_.total_deallocations.fetch_add(1);
    }

    std::vector<void*> PoolAllocator::allocate_batch(std::size_t size, std::size_t count)
    {
        std::vector<void*> ptrs;
        ptrs.reserve(count);

        for (std::size_t i = 0; i < count; ++i) {
            void* ptr = allocate(size);
            if (ptr) {
                ptrs.push_back(ptr);
            } else {
                // Clean up partial allocation
                for (void* allocated_ptr : ptrs) {
                    deallocate(allocated_ptr, size);
                }
                return {};
            }
        }

        return ptrs;
    }

    void PoolAllocator::deallocate_batch(const std::vector<void*>& ptrs, std::size_t size)
    {
        for (void* ptr : ptrs) {
            deallocate(ptr, size);
        }
    }

    PoolAllocatorStats PoolAllocator::get_statistics() const
    {
        PoolAllocatorStats stats;

        // Collect size class statistics
        stats.size_class_stats.reserve(pools_.size());
        for (const auto& pool : pools_) {
            stats.size_class_stats.push_back(pool->get_stats());
        }

        // Large object statistics
        auto large_stats = large_allocator_->get_stats();
        stats.large_allocations.store(large_stats.allocations.load());
        stats.large_deallocations.store(large_stats.deallocations.load());
        stats.large_memory_allocated.store(large_stats.bytes_allocated.load());
        stats.large_memory_peak.store(large_stats.peak_memory.load());

        // Overall statistics
        stats.total_allocations = stats_.total_allocations.load();
        stats.total_deallocations = stats_.total_deallocations.load();
        stats.total_allocation_time_ns = stats_.total_allocation_time_ns.load();
        stats.total_deallocation_time_ns = stats_.total_deallocation_time_ns.load();

        return stats;
    }

    void PoolAllocator::reset_statistics()
    {
        for (auto& pool : pools_) {
            pool->reset_stats();
        }
        large_allocator_->reset_stats();

        stats_.total_allocations.store(0);
        stats_.total_deallocations.store(0);
        stats_.total_allocation_time_ns.store(0);
        stats_.total_deallocation_time_ns.store(0);
    }

    void PoolAllocator::prefill_pools()
    {
        for (auto& pool : pools_) {
            pool->prefill(config_.initial_pool_capacity);
        }
    }

    void PoolAllocator::trim_pools()
    {
        // Implementation would return unused memory to the system
        // For now, this is a placeholder
    }

    double PoolAllocator::get_fragmentation_ratio() const
    {
        // Calculate fragmentation based on pool utilization
        std::uint64_t total_allocated = 0;
        std::uint64_t total_wasted = 0;

        auto stats = get_statistics();
        for (const auto& size_stats : stats.size_class_stats) {
            std::uint64_t objects_in_use = size_stats.objects_in_use.load();
            std::uint64_t memory_allocated = size_stats.memory_allocated.load();
            std::uint64_t actual_usage = objects_in_use * size_stats.size_class;

            total_allocated += memory_allocated;
            if (memory_allocated > actual_usage) {
                total_wasted += (memory_allocated - actual_usage);
            }
        }

        if (total_allocated == 0) {
            return 0.0;
        }

        return static_cast<double>(total_wasted) / static_cast<double>(total_allocated);
    }

    void PoolAllocator::set_numa_node(int node)
    {
        numa_node_.store(node);
    }

    PoolAllocator& PoolAllocator::get_global_instance()
    {
        std::lock_guard<std::mutex> lock(global_instance_mutex_);
        if (!global_instance_) {
            global_instance_ = std::make_unique<PoolAllocator>();
        }
        return *global_instance_;
    }

    void PoolAllocator::set_global_instance(std::unique_ptr<PoolAllocator> allocator)
    {
        std::lock_guard<std::mutex> lock(global_instance_mutex_);
        global_instance_ = std::move(allocator);
    }

    std::size_t PoolAllocator::find_size_class(std::size_t size) const
    {
        // Find smallest size class that can fit the requested size
        for (std::size_t i = 0; i < config_.size_classes.size(); ++i) {
            if (size <= config_.size_classes[i]) {
                return i;
            }
        }
        return SIZE_MAX; // Not found
    }

    MemoryPool* PoolAllocator::get_pool_for_size(std::size_t size)
    {
        std::size_t index = find_size_class(size);
        if (index < pools_.size()) {
            return pools_[index].get();
        }
        return nullptr;
    }

    ThreadLocalCache* PoolAllocator::get_thread_cache(std::size_t size_class_index)
    {
        if (size_class_index >= thread_caches_.size()) {
            thread_caches_.resize(size_class_index + 1);
        }

        if (!thread_caches_[size_class_index]) {
            std::size_t size_class = config_.size_classes[size_class_index];
            thread_caches_[size_class_index] =
                std::make_unique<ThreadLocalCache>(config_.thread_cache_size, size_class);
        }

        return thread_caches_[size_class_index].get();
    }

    void PoolAllocator::initialize_thread_caches()
    {
        thread_caches_.clear();
        thread_caches_.resize(config_.size_classes.size());
    }

    void PoolAllocator::cleanup_thread_caches()
    {
        // Return all cached objects to pools
        for (std::size_t i = 0; i < thread_caches_.size(); ++i) {
            if (thread_caches_[i] && i < pools_.size()) {
                thread_caches_[i]->drain_to_pool(*pools_[i]);
            }
        }
        thread_caches_.clear();
    }

    // Utility functions implementation

    namespace pool_utils
    {

        PoolAllocatorConfig get_optimal_config()
        {
            PoolAllocatorConfig config;

            // Get system information
            SystemMemoryInfo sys_info = get_system_memory_info();

            // Adjust configuration based on available memory
            if (sys_info.available_memory > 8ULL * 1024 * 1024 * 1024) { // 8GB+
                config.initial_pool_capacity = 2048;
                config.thread_cache_size = 256;
            } else if (sys_info.available_memory > 2ULL * 1024 * 1024 * 1024) { // 2GB+
                config.initial_pool_capacity = 1024;
                config.thread_cache_size = 128;
            } else {
                config.initial_pool_capacity = 512;
                config.thread_cache_size = 64;
            }

            // Enable NUMA if available
            config.numa_aware = sys_info.has_numa_support;

            // Set alignment based on cache line size
            config.alignment = sys_info.cache_line_size;

            return config;
        }

        BenchmarkResults benchmark_allocator(PoolAllocator& allocator, std::size_t object_size,
                                             std::size_t operation_count)
        {
            BenchmarkResults results;

            // Allocation benchmark
            std::vector<void*> ptrs;
            ptrs.reserve(operation_count);

            auto start = std::chrono::high_resolution_clock::now();
            for (std::size_t i = 0; i < operation_count; ++i) {
                void* ptr = allocator.allocate(object_size);
                if (ptr) {
                    ptrs.push_back(ptr);
                }
            }
            auto end = std::chrono::high_resolution_clock::now();

            auto allocation_duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            results.allocations_per_second =
                static_cast<double>(operation_count) * 1e9 / allocation_duration.count();
            results.average_allocation_time_ns =
                static_cast<double>(allocation_duration.count()) / operation_count;

            // Deallocation benchmark
            start = std::chrono::high_resolution_clock::now();
            for (void* ptr : ptrs) {
                allocator.deallocate(ptr, object_size);
            }
            end = std::chrono::high_resolution_clock::now();

            auto deallocation_duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            results.deallocations_per_second =
                static_cast<double>(ptrs.size()) * 1e9 / deallocation_duration.count();

            // Calculate efficiency metrics
            auto stats = allocator.get_statistics();
            results.fragmentation_ratio = allocator.get_fragmentation_ratio();

            std::uint64_t total_allocations = stats.total_allocations.load();
            std::uint64_t total_pool_hits = 0;
            for (const auto& size_stats : stats.size_class_stats) {
                total_pool_hits += size_stats.pool_hits.load();
            }

            if (total_allocations > 0) {
                results.memory_efficiency_ratio =
                    static_cast<double>(total_pool_hits) / total_allocations;
            }

            return results;
        }

        MemoryUsageReport analyze_memory_usage(const PoolAllocator& allocator)
        {
            MemoryUsageReport report;
            auto stats = allocator.get_statistics();

            for (const auto& size_stats : stats.size_class_stats) {
                std::uint64_t allocated_bytes = size_stats.memory_allocated.load();
                std::uint64_t used_bytes = size_stats.objects_in_use.load() * size_stats.size_class;

                report.total_allocated_bytes += allocated_bytes;
                if (allocated_bytes > used_bytes) {
                    report.internal_fragmentation += (allocated_bytes - used_bytes);
                }

                report.size_class_usage.emplace_back(size_stats.size_class, allocated_bytes);
            }

            report.total_wasted_bytes =
                report.internal_fragmentation + report.external_fragmentation;

            if (report.total_allocated_bytes > 0) {
                report.efficiency_ratio = 1.0 - (static_cast<double>(report.total_wasted_bytes) /
                                                 static_cast<double>(report.total_allocated_bytes));
            }

            return report;
        }

        SystemMemoryInfo get_system_memory_info()
        {
            SystemMemoryInfo info;

#ifdef __linux__
            // Get physical memory
            long pages = sysconf(_SC_PHYS_PAGES);
            long page_size = sysconf(_SC_PAGE_SIZE);
            if (pages > 0 && page_size > 0) {
                info.total_physical_memory = static_cast<std::size_t>(pages * page_size);
                info.page_size = static_cast<std::size_t>(page_size);
            }

            // Get available memory
            long avail_pages = sysconf(_SC_AVPHYS_PAGES);
            if (avail_pages > 0) {
                info.available_memory = static_cast<std::size_t>(avail_pages * page_size);
            }

            // Check for NUMA
            if (numa_available() >= 0) {
                info.has_numa_support = true;
                info.numa_nodes = numa_num_configured_nodes();
            }

            // Cache line size
            long cache_line = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
            if (cache_line > 0) {
                info.cache_line_size = static_cast<std::size_t>(cache_line);
            }
#endif

            return info;
        }

    } // namespace pool_utils

} // namespace scratchbird::engine
