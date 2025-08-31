// Copyright (c) ScratchBird Project
// SPDX-License-Identifier: Apache-2.0

#include <algorithm>
#include <cstring>
#include <scratchbird/engine/connection_memory_arena.h>
#include <scratchbird/engine/pool_allocator.h>
#include <stdexcept>

#ifdef __linux__
#include <numa.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace scratchbird::engine
{

    //==============================================================================
    // MemoryArena Implementation
    //==============================================================================

    MemoryArena::MemoryArena(const ArenaConfig& config)
        : config_(config), current_block_(nullptr), current_offset_(0)
    {
        allocate_new_block();
    }

    MemoryArena::~MemoryArena()
    {
        reset();
    }

    void* MemoryArena::allocate(std::size_t size, std::size_t alignment)
    {
        if (size == 0)
            return nullptr;

        // Record allocation timing if statistics are enabled
        AllocationTimer timer;

        // Handle large allocations separately
        if (size > config_.large_allocation_threshold) {
            void* ptr = allocate_large_object(size, alignment);
            if (config_.enable_statistics) {
                stats_.large_allocations.fetch_add(1);
                stats_.large_bytes_allocated.fetch_add(size);
                stats_.total_allocation_time_ns.fetch_add(timer.elapsed().count());
            }
            return ptr;
        }

        // Align size to requested alignment
        size = align_size(size, alignment);

        // Check if current block has enough space
        if (!current_block_ || current_offset_ + size > current_block_->size) {
            if (!allocate_new_block()) {
                throw std::bad_alloc();
            }
        }

        void* ptr = static_cast<char*>(current_block_->memory.get()) + current_offset_;
        current_offset_ += size;

        // Update statistics
        if (config_.enable_statistics) {
            stats_.allocations.fetch_add(1);
            stats_.bytes_allocated.fetch_add(size);
            stats_.current_memory_usage.fetch_add(size);
            std::size_t current = stats_.current_memory_usage.load();
            std::size_t peak = stats_.peak_memory_usage.load();
            while (current > peak &&
                   !stats_.peak_memory_usage.compare_exchange_weak(peak, current)) {
                peak = stats_.peak_memory_usage.load();
            }
            stats_.total_allocation_time_ns.fetch_add(timer.elapsed().count());
        }

        return ptr;
    }

    void MemoryArena::reset()
    {
        // Free all blocks except the first one (keep it for reuse)
        if (!blocks_.empty()) {
            auto first_block = std::move(blocks_[0]);
            blocks_.clear();

            if (first_block && first_block->size >= config_.initial_block_size) {
                blocks_.push_back(std::move(first_block));
                current_block_ = blocks_[0].get();
            } else {
                current_block_ = nullptr;
                allocate_new_block();
            }
        }

        // Free large objects
        for (auto& large_obj : large_objects_) {
            free_large_object(large_obj.ptr, large_obj.size);
        }
        large_objects_.clear();

        current_offset_ = 0;

        // Update statistics
        if (config_.enable_statistics) {
            stats_.resets.fetch_add(1);
            stats_.current_memory_usage.store(0);
        }
    }

    ArenaStats MemoryArena::get_stats() const
    {
        return stats_;
    }

    void MemoryArena::reset_stats()
    {
        stats_ = ArenaStats{};
    }

    std::size_t MemoryArena::get_memory_usage() const
    {
        std::size_t total = 0;

        // Sum up block usage
        for (const auto& block : blocks_) {
            if (block.get() == current_block_) {
                total += current_offset_;
            } else {
                total += block->size;
            }
        }

        // Add large object usage
        for (const auto& large_obj : large_objects_) {
            total += large_obj.size;
        }

        return total;
    }

    void MemoryArena::prefault_memory()
    {
        if (current_block_) {
            // Touch every page to prefault memory
            const std::size_t page_size = get_page_size();
            char* memory = static_cast<char*>(current_block_->memory.get());

            for (std::size_t offset = 0; offset < current_block_->size; offset += page_size) {
                volatile char dummy = memory[offset];
                (void)dummy; // Suppress unused variable warning
            }
        }
    }

    bool MemoryArena::allocate_new_block()
    {
        std::size_t block_size = blocks_.empty()
                                     ? config_.initial_block_size
                                     : std::min(blocks_.back()->size * 2, config_.max_block_size);

        auto block = create_block(block_size);
        if (!block) {
            return false;
        }

        current_block_ = block.get();
        current_offset_ = 0;
        blocks_.push_back(std::move(block));

        if (config_.enable_statistics) {
            stats_.blocks_allocated.fetch_add(1);
            stats_.block_bytes_allocated.fetch_add(block_size);
        }

        return true;
    }

    std::unique_ptr<MemoryArena::Block> MemoryArena::create_block(std::size_t size)
    {
        auto block = std::make_unique<Block>();
        block->size = size;

        if (config_.use_huge_pages) {
            block->memory = allocate_huge_pages(size);
        }

        if (!block->memory) {
            block->memory = std::unique_ptr<void, void (*)(void*)>(
                std::aligned_alloc(config_.alignment, size), [](void* ptr) { std::free(ptr); });
        }

        if (!block->memory) {
            return nullptr;
        }

        // Initialize memory if debugging is enabled
        if (config_.debug_fill_memory) {
            std::memset(block->memory.get(), 0xAA, size);
        }

        return block;
    }

    void* MemoryArena::allocate_large_object(std::size_t size, std::size_t alignment)
    {
        void* ptr = nullptr;

        if (config_.use_huge_pages && size >= get_huge_page_size()) {
            ptr = allocate_huge_pages(size).release();
        }

        if (!ptr) {
            ptr = std::aligned_alloc(alignment, size);
        }

        if (ptr) {
            large_objects_.emplace_back(LargeObject{ptr, size});

            if (config_.debug_fill_memory) {
                std::memset(ptr, 0xBB, size);
            }
        }

        return ptr;
    }

    void MemoryArena::free_large_object(void* ptr, std::size_t size)
    {
        if (config_.use_huge_pages && size >= get_huge_page_size()) {
            munmap(ptr, size);
        } else {
            std::free(ptr);
        }
    }

    std::unique_ptr<void, void (*)(void*)> MemoryArena::allocate_huge_pages(std::size_t size)
    {
#ifdef __linux__
        if (!config_.use_huge_pages) {
            return {nullptr, [](void*) {}};
        }

        // Align size to huge page boundary
        const std::size_t huge_page_size = get_huge_page_size();
        size = (size + huge_page_size - 1) & ~(huge_page_size - 1);

        void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);

        if (ptr != MAP_FAILED) {
            return {ptr, [size](void* p) { munmap(p, size); }};
        }
#endif

        return {nullptr, [](void*) {}};
    }

    std::size_t MemoryArena::align_size(std::size_t size, std::size_t alignment) const
    {
        if (alignment <= 1) {
            alignment = config_.alignment;
        }
        return (size + alignment - 1) & ~(alignment - 1);
    }

    std::size_t MemoryArena::get_page_size() const
    {
#ifdef __linux__
        static const std::size_t page_size = ::sysconf(_SC_PAGESIZE);
        return page_size;
#else
        return 4096; // Default page size
#endif
    }

    std::size_t MemoryArena::get_huge_page_size() const
    {
#ifdef __linux__
        static const std::size_t huge_page_size = 2 * 1024 * 1024; // 2MB
        return huge_page_size;
#else
        return 2 * 1024 * 1024;
#endif
    }

    //==============================================================================
    // ConnectionMemoryArena Implementation
    //==============================================================================

    ConnectionMemoryArena::ConnectionMemoryArena(std::uint64_t connection_id,
                                                 const ConnectionArenaConfig& config)
        : connection_id_(connection_id), config_(config),
          temporary_arena_(std::make_unique<MemoryArena>(config.temporary_arena_config)),
          persistent_arena_(std::make_unique<MemoryArena>(config.persistent_arena_config)),
          query_depth_(0)
    {
        if (config_.enable_statistics) {
            stats_.creation_time = std::chrono::high_resolution_clock::now();
        }
    }

    ConnectionMemoryArena::~ConnectionMemoryArena()
    {
        if (config_.enable_statistics) {
            stats_.destruction_time = std::chrono::high_resolution_clock::now();
            stats_.lifetime_duration = stats_.destruction_time - stats_.creation_time;
        }
    }

    void* ConnectionMemoryArena::allocate(std::size_t size, bool temporary, std::size_t alignment)
    {
        if (size == 0)
            return nullptr;

        AllocationTimer timer;
        void* ptr = nullptr;

        if (temporary) {
            ptr = temporary_arena_->allocate(size, alignment);
            if (config_.enable_statistics) {
                stats_.temporary_allocations.fetch_add(1);
                stats_.temporary_bytes_allocated.fetch_add(size);
            }
        } else {
            ptr = persistent_arena_->allocate(size, alignment);
            if (config_.enable_statistics) {
                stats_.persistent_allocations.fetch_add(1);
                stats_.persistent_bytes_allocated.fetch_add(size);
            }
        }

        if (config_.enable_statistics) {
            stats_.total_allocations.fetch_add(1);
            stats_.total_allocation_time_ns.fetch_add(timer.elapsed().count());
        }

        return ptr;
    }

    void ConnectionMemoryArena::deallocate(void* ptr)
    {
        // Memory arenas don't support individual deallocation
        // Memory is freed when arenas are reset or destroyed
        (void)ptr; // Suppress unused parameter warning
    }

    void ConnectionMemoryArena::begin_query()
    {
        std::lock_guard<std::mutex> lock(query_mutex_);
        query_depth_++;

        if (config_.enable_statistics) {
            stats_.queries_started.fetch_add(1);
        }
    }

    void ConnectionMemoryArena::end_query()
    {
        std::lock_guard<std::mutex> lock(query_mutex_);
        if (query_depth_ > 0) {
            query_depth_--;
        }

        if (config_.enable_statistics) {
            stats_.queries_completed.fetch_add(1);
        }
    }

    void ConnectionMemoryArena::reset_temporary_allocations()
    {
        std::lock_guard<std::mutex> lock(query_mutex_);

        if (query_depth_ == 0) {
            temporary_arena_->reset();

            if (config_.enable_statistics) {
                stats_.temporary_resets.fetch_add(1);
            }
        }
    }

    void ConnectionMemoryArena::reset_all_allocations()
    {
        std::lock_guard<std::mutex> lock(query_mutex_);

        temporary_arena_->reset();
        persistent_arena_->reset();
        query_depth_ = 0;

        if (config_.enable_statistics) {
            stats_.full_resets.fetch_add(1);
        }
    }

    ConnectionArenaStats ConnectionMemoryArena::get_stats() const
    {
        ConnectionArenaStats result = stats_;

        // Add arena-specific stats
        auto temp_stats = temporary_arena_->get_stats();
        auto persist_stats = persistent_arena_->get_stats();

        result.temporary_arena_memory = temp_stats.current_memory_usage.load();
        result.persistent_arena_memory = persist_stats.current_memory_usage.load();
        result.peak_temporary_memory = temp_stats.peak_memory_usage.load();
        result.peak_persistent_memory = persist_stats.peak_memory_usage.load();

        return result;
    }

    void ConnectionMemoryArena::reset_stats()
    {
        stats_ = ConnectionArenaStats{};
        temporary_arena_->reset_stats();
        persistent_arena_->reset_stats();
    }

    std::size_t ConnectionMemoryArena::get_total_memory_usage() const
    {
        return temporary_arena_->get_memory_usage() + persistent_arena_->get_memory_usage();
    }

    double ConnectionMemoryArena::get_fragmentation_ratio() const
    {
        // Calculate fragmentation based on allocated vs used memory
        std::size_t allocated = 0;
        std::size_t used = 0;

        auto temp_stats = temporary_arena_->get_stats();
        auto persist_stats = persistent_arena_->get_stats();

        allocated =
            temp_stats.block_bytes_allocated.load() + persist_stats.block_bytes_allocated.load();
        used = temp_stats.bytes_allocated.load() + persist_stats.bytes_allocated.load();

        if (allocated == 0)
            return 0.0;
        return 1.0 - (static_cast<double>(used) / allocated);
    }

    void ConnectionMemoryArena::prefault_memory()
    {
        temporary_arena_->prefault_memory();
        persistent_arena_->prefault_memory();
    }

    //==============================================================================
    // ConnectionArenaManager Implementation
    //==============================================================================

    ConnectionArenaManager::ConnectionArenaManager(const ManagerConfig& config)
        : config_(config), next_cleanup_(std::chrono::steady_clock::now() + config.cleanup_interval)
    {
        if (config_.enable_background_cleanup) {
            start_cleanup_thread();
        }
    }

    ConnectionArenaManager::~ConnectionArenaManager()
    {
        stop_cleanup_thread();
    }

    std::shared_ptr<ConnectionMemoryArena>
    ConnectionArenaManager::get_arena(std::uint64_t connection_id)
    {
        std::shared_lock<std::shared_mutex> lock(arenas_mutex_);

        auto it = arenas_.find(connection_id);
        if (it != arenas_.end()) {
            auto arena = it->second.lock();
            if (arena) {
                if (config_.enable_statistics) {
                    stats_.arena_hits.fetch_add(1);
                }
                return arena;
            } else {
                // Weak pointer expired, remove it
                lock.unlock();
                std::unique_lock<std::shared_mutex> write_lock(arenas_mutex_);
                arenas_.erase(it);
            }
        }

        if (config_.enable_statistics) {
            stats_.arena_misses.fetch_add(1);
        }

        return create_arena(connection_id);
    }

    void ConnectionArenaManager::remove_arena(std::uint64_t connection_id)
    {
        std::unique_lock<std::shared_mutex> lock(arenas_mutex_);
        auto it = arenas_.find(connection_id);
        if (it != arenas_.end()) {
            arenas_.erase(it);
            if (config_.enable_statistics) {
                stats_.arenas_removed.fetch_add(1);
            }
        }
    }

    void ConnectionArenaManager::cleanup_expired_arenas()
    {
        std::unique_lock<std::shared_mutex> lock(arenas_mutex_);

        auto it = arenas_.begin();
        std::size_t removed_count = 0;

        while (it != arenas_.end()) {
            if (it->second.expired()) {
                it = arenas_.erase(it);
                removed_count++;
            } else {
                ++it;
            }
        }

        if (config_.enable_statistics && removed_count > 0) {
            stats_.arenas_expired.fetch_add(removed_count);
        }
    }

    void ConnectionArenaManager::reset_all_temporary_allocations()
    {
        std::shared_lock<std::shared_mutex> lock(arenas_mutex_);

        for (const auto& pair : arenas_) {
            auto arena = pair.second.lock();
            if (arena) {
                arena->reset_temporary_allocations();
            }
        }

        if (config_.enable_statistics) {
            stats_.global_temp_resets.fetch_add(1);
        }
    }

    ManagerStats ConnectionArenaManager::get_stats() const
    {
        ManagerStats result = stats_;

        std::shared_lock<std::shared_mutex> lock(arenas_mutex_);
        result.active_arenas = 0;
        result.total_memory_usage = 0;

        for (const auto& pair : arenas_) {
            auto arena = pair.second.lock();
            if (arena) {
                result.active_arenas++;
                result.total_memory_usage += arena->get_total_memory_usage();
            }
        }

        return result;
    }

    void ConnectionArenaManager::reset_stats()
    {
        stats_ = ManagerStats{};
    }

    std::shared_ptr<ConnectionMemoryArena>
    ConnectionArenaManager::create_arena(std::uint64_t connection_id)
    {
        // Create new arena outside of lock to minimize contention
        auto arena = std::make_shared<ConnectionMemoryArena>(connection_id, config_.arena_config);

        std::unique_lock<std::shared_mutex> lock(arenas_mutex_);

        // Double-check that arena wasn't created while we were creating one
        auto it = arenas_.find(connection_id);
        if (it != arenas_.end()) {
            auto existing = it->second.lock();
            if (existing) {
                return existing;
            }
        }

        // Store weak reference to arena
        arenas_[connection_id] = std::weak_ptr<ConnectionMemoryArena>(arena);

        if (config_.enable_statistics) {
            stats_.arenas_created.fetch_add(1);
        }

        return arena;
    }

    void ConnectionArenaManager::start_cleanup_thread()
    {
        cleanup_stop_flag_ = false;
        cleanup_thread_ = std::thread([this]() {
            while (!cleanup_stop_flag_) {
                auto now = std::chrono::steady_clock::now();
                if (now >= next_cleanup_) {
                    cleanup_expired_arenas();
                    next_cleanup_ = now + config_.cleanup_interval;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        });
    }

    void ConnectionArenaManager::stop_cleanup_thread()
    {
        cleanup_stop_flag_ = true;
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
    }

} // namespace scratchbird::engine
