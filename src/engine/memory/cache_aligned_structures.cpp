// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstdlib>
#include <mutex>
#include <scratchbird/engine/cache_aligned_structures.h>
#include <stdexcept>

#ifdef __linux__
#include <cpuid.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

namespace scratchbird::engine::cache_utils
{

    namespace detail
    {
        /// Runtime cache line size detection
        std::size_t detect_cache_line_size() noexcept
        {
#ifdef __linux__
            // Try to get cache line size from sysconf
            long cache_line_size = sysconf(_SC_LEVEL1_DCACHE_LINESIZE);
            if (cache_line_size > 0) {
                return static_cast<std::size_t>(cache_line_size);
            }

            // Fallback: try CPUID on x86/x64
#if defined(__x86_64__) || defined(__i386__)
            unsigned int eax, ebx, ecx, edx;
            if (__get_cpuid(0x80000006, &eax, &ebx, &ecx, &edx)) {
                unsigned int cache_line = ecx & 0xFF;
                if (cache_line > 0) {
                    return cache_line;
                }
            }
#endif
#endif

            // Conservative default
            return 64;
        }

        /// Get runtime cache line size (cached after first call)
        std::size_t get_runtime_cache_line_size() noexcept
        {
            static const std::size_t cache_line_size = detect_cache_line_size();
            return cache_line_size;
        }
    } // namespace detail

    std::size_t get_runtime_cache_line_size() noexcept
    {
        return detail::get_runtime_cache_line_size();
    }

    bool is_power_of_2(std::size_t n) noexcept
    {
        return n > 0 && (n & (n - 1)) == 0;
    }

    std::size_t next_power_of_2(std::size_t n) noexcept
    {
        if (n <= 1)
            return 1;

        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        n |= n >> 32;
        return n + 1;
    }

    std::size_t align_to_boundary(std::size_t size, std::size_t alignment) noexcept
    {
        if (alignment == 0 || !is_power_of_2(alignment)) {
            return size;
        }
        return (size + alignment - 1) & ~(alignment - 1);
    }

    void prefetch_cache_line(const void* addr, int locality) noexcept
    {
#ifdef __builtin_prefetch
        __builtin_prefetch(addr, 0, locality);
#else
        (void)addr; // Suppress unused parameter warning
        (void)locality;
#endif
    }

    void prefetch_range(const void* start, std::size_t size, int locality) noexcept
    {
        const std::size_t cache_line_size = get_runtime_cache_line_size();
        const char* current = static_cast<const char*>(start);
        const char* end = current + size;

        while (current < end) {
            prefetch_cache_line(current, locality);
            current += cache_line_size;
        }
    }

    cache_utils::CacheInfo get_cache_info() noexcept
    {
        cache_utils::CacheInfo info{};
        info.l1_cache_line_size = get_runtime_cache_line_size();
        info.l2_cache_line_size = info.l1_cache_line_size; // Usually the same
        info.l3_cache_line_size = info.l1_cache_line_size; // Usually the same

#ifdef __linux__
        // Try to get cache sizes from sysconf
        long l1_cache_size = sysconf(_SC_LEVEL1_DCACHE_SIZE);
        long l2_cache_size = sysconf(_SC_LEVEL2_CACHE_SIZE);
        long l3_cache_size = sysconf(_SC_LEVEL3_CACHE_SIZE);

        if (l1_cache_size > 0)
            info.l1_cache_size = static_cast<std::size_t>(l1_cache_size);
        if (l2_cache_size > 0)
            info.l2_cache_size = static_cast<std::size_t>(l2_cache_size);
        if (l3_cache_size > 0)
            info.l3_cache_size = static_cast<std::size_t>(l3_cache_size);

        // Get number of processors
        int nprocs = get_nprocs();
        if (nprocs > 0)
            info.logical_processors = static_cast<std::size_t>(nprocs);
#endif

        // Set reasonable defaults if detection failed
        if (info.l1_cache_size == 0)
            info.l1_cache_size = 32 * 1024; // 32KB
        if (info.l2_cache_size == 0)
            info.l2_cache_size = 256 * 1024; // 256KB
        if (info.l3_cache_size == 0)
            info.l3_cache_size = 8 * 1024 * 1024; // 8MB
        if (info.logical_processors == 0)
            info.logical_processors = 1;

        return info;
    }

    SystemMemoryInfo get_system_memory_info()
    {
        SystemMemoryInfo info{};
        info.cache_line_size = get_runtime_cache_line_size();

#ifdef __linux__
        info.page_size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));

        // Get total and available memory
        struct sysinfo si;
        if (sysinfo(&si) == 0) {
            info.total_physical_memory = si.totalram * si.mem_unit;
            info.available_memory = si.freeram * si.mem_unit;
        }

        // Get number of processors
        int nprocs = get_nprocs();
        if (nprocs > 0)
            info.logical_processors = static_cast<std::size_t>(nprocs);
#endif

        return info;
    }

} // namespace scratchbird::engine::cache_utils

namespace scratchbird::engine
{

    //==============================================================================
    // Cache-aligned memory utilities implementation
    //==============================================================================

    namespace
    {
        /// Memory allocation with cache-line alignment and optional huge pages
        void* allocate_cache_aligned_impl(std::size_t size, std::size_t alignment,
                                          bool use_huge_pages)
        {
            if (size == 0)
                return nullptr;

            // Ensure alignment is at least cache line size
            const std::size_t cache_line_size = cache_utils::get_runtime_cache_line_size();
            alignment = std::max(alignment, cache_line_size);

            // Align size to cache line boundary
            size = cache_utils::align_to_boundary(size, cache_line_size);

            void* ptr = nullptr;

            // Try huge pages if requested and size is large enough
            if (use_huge_pages && size >= 2 * 1024 * 1024) {
#ifdef __linux__
                ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
                if (ptr == MAP_FAILED) {
                    ptr = nullptr;
                }
#endif
            }

            // Fallback to regular aligned allocation
            if (!ptr) {
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606L
                ptr = std::aligned_alloc(alignment, size);
#else
                // Fallback for older C++ standards
                if (posix_memalign(&ptr, alignment, size) != 0) {
                    ptr = nullptr;
                }
#endif
            }

            return ptr;
        }

        /// Free memory allocated by allocate_cache_aligned_impl
        void free_cache_aligned_impl(void* ptr, std::size_t size, bool was_huge_pages) noexcept
        {
            if (!ptr)
                return;

            if (was_huge_pages && size >= 2 * 1024 * 1024) {
#ifdef __linux__
                munmap(ptr, size);
                return;
#endif
            }

            std::free(ptr);
        }
    } // anonymous namespace

    //==============================================================================
    // Cache-aligned memory pool implementation
    //==============================================================================

    class CacheAlignedMemoryPool::Impl
    {
      public:
        explicit Impl(std::size_t object_size, std::size_t pool_size, bool use_huge_pages)
            : object_size_(
                  cache_utils::align_to_boundary(object_size, cache_utils::get_cache_line_size())),
              pool_size_(pool_size), use_huge_pages_(use_huge_pages), free_list_(nullptr),
              allocated_objects_(0), total_allocations_(0), total_deallocations_(0)
        {

            if (object_size_ == 0) {
                throw std::invalid_argument("Object size must be greater than 0");
            }

            allocate_pool();
        }

        ~Impl()
        {
            for (const auto& pool : pools_) {
                free_cache_aligned_impl(pool.memory, pool.size, use_huge_pages_);
            }
        }

        void* allocate()
        {
            // Try to get from free list first
            if (FreeNode* node = free_list_.load()) {
                if (free_list_.compare_exchange_weak(node, node->next)) {
                    allocated_objects_.fetch_add(1, std::memory_order_relaxed);
                    total_allocations_.fetch_add(1, std::memory_order_relaxed);
                    return node;
                }
            }

            // Allocate new pool if needed
            if (need_new_pool()) {
                allocate_pool();
            }

            // Try free list again
            if (FreeNode* node = free_list_.load()) {
                if (free_list_.compare_exchange_weak(node, node->next)) {
                    allocated_objects_.fetch_add(1, std::memory_order_relaxed);
                    total_allocations_.fetch_add(1, std::memory_order_relaxed);
                    return node;
                }
            }

            // Fallback to regular allocation
            void* ptr = allocate_cache_aligned_impl(object_size_,
                                                    cache_utils::get_cache_line_size(), false);
            if (ptr) {
                allocated_objects_.fetch_add(1, std::memory_order_relaxed);
                total_allocations_.fetch_add(1, std::memory_order_relaxed);
            }
            return ptr;
        }

        void deallocate(void* ptr) noexcept
        {
            if (!ptr)
                return;

            // Check if pointer belongs to our pools
            if (!owns_pointer(ptr)) {
                free_cache_aligned_impl(ptr, object_size_, false);
            } else {
                // Add to free list
                auto* node = static_cast<FreeNode*>(ptr);
                FreeNode* old_head = free_list_.load();
                do {
                    node->next = old_head;
                } while (!free_list_.compare_exchange_weak(old_head, node));
            }

            allocated_objects_.fetch_sub(1, std::memory_order_relaxed);
            total_deallocations_.fetch_add(1, std::memory_order_relaxed);
        }

        std::size_t get_allocated_count() const noexcept
        {
            return allocated_objects_.load(std::memory_order_relaxed);
        }

        std::size_t get_total_allocations() const noexcept
        {
            return total_allocations_.load(std::memory_order_relaxed);
        }

        std::size_t get_total_deallocations() const noexcept
        {
            return total_deallocations_.load(std::memory_order_relaxed);
        }

        std::size_t get_object_size() const noexcept
        {
            return object_size_;
        }

      private:
        struct Pool {
            void* memory;
            std::size_t size;
            std::size_t object_count;
        };

        struct FreeNode {
            std::atomic<FreeNode*> next;
        };

        const std::size_t object_size_;
        const std::size_t pool_size_;
        const bool use_huge_pages_;

        std::vector<Pool> pools_;
        std::mutex pools_mutex_;

        std::atomic<FreeNode*> free_list_;
        std::atomic<std::size_t> allocated_objects_;
        std::atomic<std::size_t> total_allocations_;
        std::atomic<std::size_t> total_deallocations_;

        bool need_new_pool() const
        {
            return free_list_.load() == nullptr && !pools_.empty();
        }

        void allocate_pool()
        {
            std::lock_guard<std::mutex> lock(pools_mutex_);

            const std::size_t objects_per_pool = pool_size_ / object_size_;
            const std::size_t actual_pool_size = objects_per_pool * object_size_;

            void* memory = allocate_cache_aligned_impl(
                actual_pool_size, cache_utils::get_cache_line_size(), use_huge_pages_);
            if (!memory) {
                throw std::bad_alloc();
            }

            // Add objects to free list
            char* current = static_cast<char*>(memory);
            FreeNode* head = nullptr;

            for (std::size_t i = 0; i < objects_per_pool; ++i) {
                auto* node = reinterpret_cast<FreeNode*>(current);
                node->next.store(head, std::memory_order_relaxed);
                head = node;
                current += object_size_;
            }

            // Update free list
            FreeNode* old_head = free_list_.load();
            do {
                head->next.store(old_head, std::memory_order_relaxed);
            } while (!free_list_.compare_exchange_weak(old_head, head));

            pools_.emplace_back(Pool{memory, actual_pool_size, objects_per_pool});
        }

        bool owns_pointer(void* ptr) const
        {
            for (const auto& pool : pools_) {
                char* start = static_cast<char*>(pool.memory);
                char* end = start + pool.size;
                if (ptr >= start && ptr < end) {
                    // Check if aligned to object boundary
                    std::ptrdiff_t offset = static_cast<char*>(ptr) - start;
                    return (offset % object_size_) == 0;
                }
            }
            return false;
        }
    };

    //==============================================================================
    // CacheAlignedMemoryPool public interface
    //==============================================================================

    CacheAlignedMemoryPool::CacheAlignedMemoryPool(std::size_t object_size, std::size_t pool_size,
                                                   bool use_huge_pages)
        : impl_(std::make_unique<Impl>(object_size, pool_size, use_huge_pages))
    {
    }

    CacheAlignedMemoryPool::~CacheAlignedMemoryPool() = default;

    void* CacheAlignedMemoryPool::allocate()
    {
        return impl_->allocate();
    }

    void CacheAlignedMemoryPool::deallocate(void* ptr) noexcept
    {
        impl_->deallocate(ptr);
    }

    std::size_t CacheAlignedMemoryPool::get_allocated_count() const noexcept
    {
        return impl_->get_allocated_count();
    }

    std::size_t CacheAlignedMemoryPool::get_total_allocations() const noexcept
    {
        return impl_->get_total_allocations();
    }

    std::size_t CacheAlignedMemoryPool::get_total_deallocations() const noexcept
    {
        return impl_->get_total_deallocations();
    }

    std::size_t CacheAlignedMemoryPool::get_object_size() const noexcept
    {
        return impl_->get_object_size();
    }

} // namespace scratchbird::engine
