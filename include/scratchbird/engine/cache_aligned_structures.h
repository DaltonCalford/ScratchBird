// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace scratchbird::engine
{

    /// Cache line size detection and alignment utilities
    namespace cache_utils
    {
        /// Get cache line size for the current architecture
        constexpr std::size_t get_cache_line_size() noexcept
        {
#ifdef __cpp_lib_hardware_interference_size
            return std::hardware_destructive_interference_size;
#else
            return 64; // Common cache line size for x86/x64
#endif
        }

        /// Get constructive interference size (for grouping related data)
        constexpr std::size_t get_constructive_interference_size() noexcept
        {
#ifdef __cpp_lib_hardware_interference_size
            return std::hardware_constructive_interference_size;
#else
            return 64;
#endif
        }

        /// Align size to cache line boundary
        constexpr std::size_t align_to_cache_line(std::size_t size) noexcept
        {
            const std::size_t cache_line = get_cache_line_size();
            return (size + cache_line - 1) & ~(cache_line - 1);
        }

        /// Check if pointer is cache-aligned
        inline bool is_cache_aligned(const void* ptr) noexcept
        {
            return reinterpret_cast<std::uintptr_t>(ptr) % get_cache_line_size() == 0;
        }
    } // namespace cache_utils

    /// Cache-aligned atomic counter with padding to prevent false sharing
    template <typename T = std::uint64_t>
    class alignas(cache_utils::get_cache_line_size()) cache_aligned_atomic
    {
        static_assert(std::is_arithmetic_v<T>, "T must be arithmetic type");

      public:
        cache_aligned_atomic() noexcept : value_(T{}) {}
        explicit cache_aligned_atomic(T initial) noexcept : value_(initial) {}

        // Non-copyable, non-moveable
        cache_aligned_atomic(const cache_aligned_atomic&) = delete;
        cache_aligned_atomic& operator=(const cache_aligned_atomic&) = delete;
        cache_aligned_atomic(cache_aligned_atomic&&) = delete;
        cache_aligned_atomic& operator=(cache_aligned_atomic&&) = delete;

        T load(std::memory_order order = std::memory_order_seq_cst) const noexcept
        {
            return value_.load(order);
        }

        void store(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            value_.store(desired, order);
        }

        T exchange(T desired, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            return value_.exchange(desired, order);
        }

        bool compare_exchange_weak(T& expected, T desired,
                                   std::memory_order success = std::memory_order_seq_cst,
                                   std::memory_order failure = std::memory_order_seq_cst) noexcept
        {
            return value_.compare_exchange_weak(expected, desired, success, failure);
        }

        bool compare_exchange_strong(T& expected, T desired,
                                     std::memory_order success = std::memory_order_seq_cst,
                                     std::memory_order failure = std::memory_order_seq_cst) noexcept
        {
            return value_.compare_exchange_strong(expected, desired, success, failure);
        }

        T fetch_add(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            return value_.fetch_add(arg, order);
        }

        T fetch_sub(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            return value_.fetch_sub(arg, order);
        }

        T fetch_and(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            return value_.fetch_and(arg, order);
        }

        T fetch_or(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            return value_.fetch_or(arg, order);
        }

        T fetch_xor(T arg, std::memory_order order = std::memory_order_seq_cst) noexcept
        {
            return value_.fetch_xor(arg, order);
        }

        // Atomic arithmetic operators
        T operator++() noexcept
        {
            return fetch_add(1) + 1;
        }
        T operator++(int) noexcept
        {
            return fetch_add(1);
        }
        T operator--() noexcept
        {
            return fetch_sub(1) - 1;
        }
        T operator--(int) noexcept
        {
            return fetch_sub(1);
        }
        T operator+=(T arg) noexcept
        {
            return fetch_add(arg) + arg;
        }
        T operator-=(T arg) noexcept
        {
            return fetch_sub(arg) - arg;
        }
        T operator&=(T arg) noexcept
        {
            return fetch_and(arg) & arg;
        }
        T operator|=(T arg) noexcept
        {
            return fetch_or(arg) | arg;
        }
        T operator^=(T arg) noexcept
        {
            return fetch_xor(arg) ^ arg;
        }

        // Implicit conversion to T for read operations
        operator T() const noexcept
        {
            return load();
        }

      private:
        std::atomic<T> value_;

        // Padding to ensure the atomic occupies exactly one cache line
        static constexpr std::size_t padding_size =
            cache_utils::get_cache_line_size() - sizeof(std::atomic<T>);
        [[maybe_unused]] char padding_[padding_size > 0 ? padding_size : 1];
    };

    /// Cache-aligned data structure with automatic padding
    template <typename T> class alignas(cache_utils::get_cache_line_size()) cache_aligned
    {
      public:
        template <typename... Args>
        explicit cache_aligned(Args&&... args) : data_(std::forward<Args>(args)...)
        {
        }

        T& get() noexcept
        {
            return data_;
        }
        const T& get() const noexcept
        {
            return data_;
        }

        T& operator*() noexcept
        {
            return data_;
        }
        const T& operator*() const noexcept
        {
            return data_;
        }

        T* operator->() noexcept
        {
            return &data_;
        }
        const T* operator->() const noexcept
        {
            return &data_;
        }

      private:
        T data_;

        // Padding to fill the cache line
        static constexpr std::size_t padding_size =
            cache_utils::align_to_cache_line(sizeof(T)) - sizeof(T);
        [[maybe_unused]] char padding_[padding_size > 0 ? padding_size : 1];
    };

    /// Lock-free stack with cache-aligned nodes to reduce contention
    template <typename T> class cache_aligned_stack
    {
      private:
        struct alignas(cache_utils::get_cache_line_size()) Node {
            T data;
            std::atomic<Node*> next;

            template <typename... Args>
            Node(Args&&... args) : data(std::forward<Args>(args)...), next(nullptr)
            {
            }
        };

      public:
        cache_aligned_stack() : head_(nullptr), size_(0) {}

        ~cache_aligned_stack()
        {
            clear();
        }

        // Non-copyable, non-moveable
        cache_aligned_stack(const cache_aligned_stack&) = delete;
        cache_aligned_stack& operator=(const cache_aligned_stack&) = delete;
        cache_aligned_stack(cache_aligned_stack&&) = delete;
        cache_aligned_stack& operator=(cache_aligned_stack&&) = delete;

        void push(const T& item)
        {
            auto new_node = std::make_unique<Node>(item);
            Node* old_head = head_.load();

            do {
                new_node->next.store(old_head);
            } while (!head_.compare_exchange_weak(old_head, new_node.get()));

            new_node.release(); // Transfer ownership to the stack
            size_.fetch_add(1, std::memory_order_relaxed);
        }

        void push(T&& item)
        {
            auto new_node = std::make_unique<Node>(std::move(item));
            Node* old_head = head_.load();

            do {
                new_node->next.store(old_head);
            } while (!head_.compare_exchange_weak(old_head, new_node.get()));

            new_node.release();
            size_.fetch_add(1, std::memory_order_relaxed);
        }

        template <typename... Args> void emplace(Args&&... args)
        {
            auto new_node = std::make_unique<Node>(std::forward<Args>(args)...);
            Node* old_head = head_.load();

            do {
                new_node->next.store(old_head);
            } while (!head_.compare_exchange_weak(old_head, new_node.get()));

            new_node.release();
            size_.fetch_add(1, std::memory_order_relaxed);
        }

        bool pop(T& result)
        {
            Node* old_head = head_.load();

            while (old_head) {
                if (head_.compare_exchange_weak(old_head, old_head->next.load())) {
                    result = std::move(old_head->data);
                    delete old_head;
                    size_.fetch_sub(1, std::memory_order_relaxed);
                    return true;
                }
            }

            return false;
        }

        bool empty() const noexcept
        {
            return head_.load() == nullptr;
        }

        std::size_t size() const noexcept
        {
            return size_.load(std::memory_order_relaxed);
        }

        void clear()
        {
            while (Node* old_head = head_.load()) {
                head_.store(old_head->next);
                delete old_head;
            }
            size_.store(0, std::memory_order_relaxed);
        }

      private:
        cache_aligned_atomic<Node*> head_;
        cache_aligned_atomic<std::size_t> size_;
    };

    /// Cache-optimized hash table with aligned buckets
    template <typename Key, typename Value, typename Hash = std::hash<Key>>
    class cache_aligned_hash_table
    {
      private:
        struct alignas(cache_utils::get_cache_line_size()) Bucket {
            struct Entry {
                Key key;
                Value value;
                std::atomic<Entry*> next;

                template <typename K, typename V>
                Entry(K&& k, V&& v)
                    : key(std::forward<K>(k)), value(std::forward<V>(v)), next(nullptr)
                {
                }
            };

            std::atomic<Entry*> head;
            mutable std::atomic<std::uint64_t> access_count;

            Bucket() : head(nullptr), access_count(0) {}
            ~Bucket()
            {
                clear();
            }

            void clear()
            {
                Entry* current = head.load();
                while (current) {
                    Entry* next = current->next.load();
                    delete current;
                    current = next;
                }
                head.store(nullptr);
            }
        };

      public:
        explicit cache_aligned_hash_table(std::size_t bucket_count = 1024)
            : buckets_(bucket_count), bucket_mask_(bucket_count - 1)
        {
            // Ensure bucket count is power of 2 for efficient modulo
            if ((bucket_count & (bucket_count - 1)) != 0) {
                throw std::invalid_argument("Bucket count must be power of 2");
            }
        }

        ~cache_aligned_hash_table()
        {
            clear();
        }

        // Non-copyable, non-moveable
        cache_aligned_hash_table(const cache_aligned_hash_table&) = delete;
        cache_aligned_hash_table& operator=(const cache_aligned_hash_table&) = delete;
        cache_aligned_hash_table(cache_aligned_hash_table&&) = delete;
        cache_aligned_hash_table& operator=(cache_aligned_hash_table&&) = delete;

        template <typename K, typename V> bool insert(K&& key, V&& value)
        {
            std::size_t bucket_index = hash_(key) & bucket_mask_;
            Bucket& bucket = buckets_[bucket_index];

            auto new_entry = std::make_unique<typename Bucket::Entry>(std::forward<K>(key),
                                                                      std::forward<V>(value));

            // Check if key already exists
            typename Bucket::Entry* current = bucket.head.load();
            while (current) {
                if (current->key == key) {
                    return false; // Key already exists
                }
                current = current->next.load();
            }

            // Insert at head of bucket
            typename Bucket::Entry* old_head = bucket.head.load();
            do {
                new_entry->next.store(old_head);
            } while (!bucket.head.compare_exchange_weak(old_head, new_entry.get()));

            new_entry.release();
            bucket.access_count.fetch_add(1, std::memory_order_relaxed);
            return true;
        }

        bool find(const Key& key, Value& result) const
        {
            std::size_t bucket_index = hash_(key) & bucket_mask_;
            const Bucket& bucket = buckets_[bucket_index];

            typename Bucket::Entry* current = bucket.head.load();
            while (current) {
                if (current->key == key) {
                    result = current->value;
                    bucket.access_count.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                current = current->next.load();
            }

            return false;
        }

        bool contains(const Key& key) const
        {
            std::size_t bucket_index = hash_(key) & bucket_mask_;
            const Bucket& bucket = buckets_[bucket_index];

            typename Bucket::Entry* current = bucket.head.load();
            while (current) {
                if (current->key == key) {
                    bucket.access_count.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                current = current->next.load();
            }

            return false;
        }

        bool erase(const Key& key)
        {
            std::size_t bucket_index = hash_(key) & bucket_mask_;
            Bucket& bucket = buckets_[bucket_index];

            typename Bucket::Entry* current = bucket.head.load();
            typename Bucket::Entry* prev = nullptr;

            while (current) {
                if (current->key == key) {
                    if (prev) {
                        prev->next.store(current->next.load());
                    } else {
                        bucket.head.store(current->next.load());
                    }
                    delete current;
                    return true;
                }
                prev = current;
                current = current->next.load();
            }

            return false;
        }

        void clear()
        {
            for (auto& bucket : buckets_) {
                bucket.clear();
            }
        }

        std::size_t bucket_count() const noexcept
        {
            return buckets_.size();
        }

        // Performance monitoring
        struct Stats {
            std::size_t total_entries{0};
            std::size_t max_bucket_size{0};
            double average_bucket_size{0.0};
            double load_factor{0.0};
            std::uint64_t total_accesses{0};
        };

        Stats get_stats() const
        {
            Stats stats;
            std::size_t total_entries = 0;
            std::uint64_t total_accesses = 0;
            std::size_t max_bucket_size = 0;

            for (const auto& bucket : buckets_) {
                std::size_t bucket_size = 0;
                typename Bucket::Entry* current = bucket.head.load();

                while (current) {
                    bucket_size++;
                    current = current->next.load();
                }

                total_entries += bucket_size;
                max_bucket_size = std::max(max_bucket_size, bucket_size);
                total_accesses += bucket.access_count.load(std::memory_order_relaxed);
            }

            stats.total_entries = total_entries;
            stats.max_bucket_size = max_bucket_size;
            stats.average_bucket_size = static_cast<double>(total_entries) / buckets_.size();
            stats.load_factor = stats.average_bucket_size;
            stats.total_accesses = total_accesses;

            return stats;
        }

      private:
        std::vector<Bucket> buckets_;
        std::size_t bucket_mask_;
        Hash hash_;
    };

    /// Cache-friendly circular buffer with padding to prevent false sharing
    template <typename T> class cache_aligned_ring_buffer
    {
      public:
        explicit cache_aligned_ring_buffer(std::size_t capacity)
            : capacity_(next_power_of_2(capacity)), mask_(capacity_ - 1),
              buffer_(std::make_unique<Element[]>(capacity_)), head_(0), tail_(0)
        {
        }

        // Non-copyable, non-moveable
        cache_aligned_ring_buffer(const cache_aligned_ring_buffer&) = delete;
        cache_aligned_ring_buffer& operator=(const cache_aligned_ring_buffer&) = delete;
        cache_aligned_ring_buffer(cache_aligned_ring_buffer&&) = delete;
        cache_aligned_ring_buffer& operator=(cache_aligned_ring_buffer&&) = delete;

        template <typename U> bool push(U&& item)
        {
            const std::size_t current_tail = tail_.load(std::memory_order_relaxed);
            const std::size_t next_tail = (current_tail + 1) & mask_;

            if (next_tail == head_.load(std::memory_order_acquire)) {
                return false; // Buffer full
            }

            buffer_[current_tail].data = std::forward<U>(item);
            tail_.store(next_tail, std::memory_order_release);
            return true;
        }

        bool pop(T& result)
        {
            const std::size_t current_head = head_.load(std::memory_order_relaxed);

            if (current_head == tail_.load(std::memory_order_acquire)) {
                return false; // Buffer empty
            }

            result = std::move(buffer_[current_head].data);
            head_.store((current_head + 1) & mask_, std::memory_order_release);
            return true;
        }

        bool empty() const noexcept
        {
            return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
        }

        bool full() const noexcept
        {
            return ((tail_.load(std::memory_order_acquire) + 1) & mask_) ==
                   head_.load(std::memory_order_acquire);
        }

        std::size_t size() const noexcept
        {
            return (tail_.load(std::memory_order_acquire) - head_.load(std::memory_order_acquire)) &
                   mask_;
        }

        std::size_t capacity() const noexcept
        {
            return capacity_ - 1; // One slot reserved for full/empty distinction
        }

      private:
        struct alignas(cache_utils::get_cache_line_size()) Element {
            T data;
        };

        static std::size_t next_power_of_2(std::size_t n)
        {
            n--;
            n |= n >> 1;
            n |= n >> 2;
            n |= n >> 4;
            n |= n >> 8;
            n |= n >> 16;
            n |= n >> 32;
            return n + 1;
        }

        const std::size_t capacity_;
        const std::size_t mask_;
        std::unique_ptr<Element[]> buffer_;

        // Separate head and tail to different cache lines to reduce contention
        alignas(cache_utils::get_cache_line_size()) cache_aligned_atomic<std::size_t> head_;
        alignas(cache_utils::get_cache_line_size()) cache_aligned_atomic<std::size_t> tail_;
    };

    /// Memory allocator that ensures cache-line alignment
    template <typename T> class cache_aligned_allocator
    {
      public:
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using const_pointer = const T*;
        using reference = T&;
        using const_reference = const T&;

        template <typename U> struct rebind {
            using other = cache_aligned_allocator<U>;
        };

        cache_aligned_allocator() noexcept = default;

        template <typename U> cache_aligned_allocator(const cache_aligned_allocator<U>&) noexcept {}

        pointer allocate(size_type n)
        {
            if (n == 0)
                return nullptr;

            const std::size_t alignment = cache_utils::get_cache_line_size();
            const std::size_t size = n * sizeof(T);
            const std::size_t aligned_size = cache_utils::align_to_cache_line(size);

            void* ptr = nullptr;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606L
            ptr = std::aligned_alloc(alignment, aligned_size);
#else
            // Fallback for older C++ standards
            if (posix_memalign(&ptr, alignment, aligned_size) != 0) {
                ptr = nullptr;
            }
#endif
            if (!ptr) {
                throw std::bad_alloc();
            }

            return static_cast<pointer>(ptr);
        }

        void deallocate(pointer p, size_type) noexcept
        {
            free(p);
        }

        template <typename U, typename... Args> void construct(U* p, Args&&... args)
        {
            new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
        }

        template <typename U> void destroy(U* p)
        {
            p->~U();
        }

        bool operator==(const cache_aligned_allocator&) const noexcept
        {
            return true;
        }
        bool operator!=(const cache_aligned_allocator&) const noexcept
        {
            return false;
        }
    };

    /// Convenience type aliases for common cache-aligned containers
    template <typename T> using cache_aligned_vector = std::vector<T, cache_aligned_allocator<T>>;

    /// RAII wrapper for cache-aligned memory allocation
    template <typename T> class cache_aligned_ptr
    {
      public:
        explicit cache_aligned_ptr(std::size_t count = 1) : count_(count)
        {
            if (count > 0) {
                const std::size_t alignment = cache_utils::get_cache_line_size();
                const std::size_t size = count * sizeof(T);
                const std::size_t aligned_size = cache_utils::align_to_cache_line(size);

                void* raw_ptr = nullptr;
#if defined(__cpp_aligned_new) && __cpp_aligned_new >= 201606L
                raw_ptr = std::aligned_alloc(alignment, aligned_size);
#else
                // Fallback for older C++ standards
                if (posix_memalign(&raw_ptr, alignment, aligned_size) != 0) {
                    raw_ptr = nullptr;
                }
#endif
                ptr_ = static_cast<T*>(raw_ptr);
                if (!ptr_) {
                    throw std::bad_alloc();
                }
            }
        }

        ~cache_aligned_ptr()
        {
            if (ptr_) {
                // Call destructors for constructed objects
                for (std::size_t i = 0; i < count_; ++i) {
                    ptr_[i].~T();
                }
                std::free(ptr_);
            }
        }

        // Non-copyable, moveable
        cache_aligned_ptr(const cache_aligned_ptr&) = delete;
        cache_aligned_ptr& operator=(const cache_aligned_ptr&) = delete;

        cache_aligned_ptr(cache_aligned_ptr&& other) noexcept
            : ptr_(other.ptr_), count_(other.count_)
        {
            other.ptr_ = nullptr;
            other.count_ = 0;
        }

        cache_aligned_ptr& operator=(cache_aligned_ptr&& other) noexcept
        {
            if (this != &other) {
                if (ptr_) {
                    for (std::size_t i = 0; i < count_; ++i) {
                        ptr_[i].~T();
                    }
                    free(ptr_);
                }
                ptr_ = other.ptr_;
                count_ = other.count_;
                other.ptr_ = nullptr;
                other.count_ = 0;
            }
            return *this;
        }

        T* get() const noexcept
        {
            return ptr_;
        }
        T& operator[](std::size_t index) const noexcept
        {
            return ptr_[index];
        }
        T& operator*() const noexcept
        {
            return *ptr_;
        }
        T* operator->() const noexcept
        {
            return ptr_;
        }

        explicit operator bool() const noexcept
        {
            return ptr_ != nullptr;
        }

        std::size_t size() const noexcept
        {
            return count_;
        }

        template <typename... Args> void construct_at(std::size_t index, Args&&... args)
        {
            if (index < count_) {
                new (ptr_ + index) T(std::forward<Args>(args)...);
            }
        }

      private:
        T* ptr_{nullptr};
        std::size_t count_{0};
    };

    /// Factory function for creating cache-aligned objects
    template <typename T, typename... Args>
    cache_aligned_ptr<T> make_cache_aligned(std::size_t count, Args&&... args)
    {
        cache_aligned_ptr<T> ptr(count);
        for (std::size_t i = 0; i < count; ++i) {
            ptr.construct_at(i, std::forward<Args>(args)...);
        }
        return ptr;
    }

    template <typename T, typename... Args> cache_aligned_ptr<T> make_cache_aligned(Args&&... args)
    {
        cache_aligned_ptr<T> ptr(1);
        ptr.construct_at(0, std::forward<Args>(args)...);
        return ptr;
    }

    /// Cache hierarchy information
    namespace cache_utils
    {
        struct CacheInfo {
            std::size_t l1_cache_size{32 * 1024};       // 32KB
            std::size_t l2_cache_size{256 * 1024};      // 256KB
            std::size_t l3_cache_size{8 * 1024 * 1024}; // 8MB
            std::size_t l1_cache_line_size{64};
            std::size_t l2_cache_line_size{64};
            std::size_t l3_cache_line_size{64};
            std::size_t logical_processors{1};
        };

        /// System memory information
        struct SystemMemoryInfo {
            std::size_t total_physical_memory{0};
            std::size_t available_memory{0};
            std::size_t cache_line_size{64};
            std::size_t page_size{4096};
            std::size_t logical_processors{1};
            int numa_nodes{1};
            bool has_numa_support{false};
        };

        // Additional utility functions
        std::size_t get_runtime_cache_line_size() noexcept;
        bool is_power_of_2(std::size_t n) noexcept;
        std::size_t next_power_of_2(std::size_t n) noexcept;
        std::size_t align_to_boundary(std::size_t size, std::size_t alignment) noexcept;
        void prefetch_cache_line(const void* addr, int locality = 3) noexcept;
        void prefetch_range(const void* start, std::size_t size, int locality = 3) noexcept;
        CacheInfo get_cache_info() noexcept;
        SystemMemoryInfo get_system_memory_info();
    } // namespace cache_utils

    /// Cache-aligned memory pool for fixed-size objects
    class CacheAlignedMemoryPool
    {
      public:
        CacheAlignedMemoryPool(std::size_t object_size, std::size_t pool_size = 1024 * 1024,
                               bool use_huge_pages = false);
        ~CacheAlignedMemoryPool();

        /// Non-copyable, non-moveable
        CacheAlignedMemoryPool(const CacheAlignedMemoryPool&) = delete;
        CacheAlignedMemoryPool& operator=(const CacheAlignedMemoryPool&) = delete;
        CacheAlignedMemoryPool(CacheAlignedMemoryPool&&) = delete;
        CacheAlignedMemoryPool& operator=(CacheAlignedMemoryPool&&) = delete;

        void* allocate();
        void deallocate(void* ptr) noexcept;

        std::size_t get_allocated_count() const noexcept;
        std::size_t get_total_allocations() const noexcept;
        std::size_t get_total_deallocations() const noexcept;
        std::size_t get_object_size() const noexcept;

      private:
        class Impl;
        std::unique_ptr<Impl> impl_;
    };

} // namespace scratchbird::engine
