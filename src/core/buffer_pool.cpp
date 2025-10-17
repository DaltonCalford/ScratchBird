#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/debug.h"
#include <cstring>
#include <algorithm>
#include <cassert>

namespace scratchbird::core
{

    BufferPool::BufferPool(Database *db, const Config &config) : db_(db), config_(config)
    {
        frames_.resize(config.pool_size);
    }

    BufferPool::~BufferPool()
    {
        shutdown();
    }

    auto BufferPool::initialize(ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Allocate memory for each frame
        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            try
            {
                frames_[i].data = std::make_unique<uint8_t[]>(config_.page_size);
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer pool memory");
                return Status::OOM;
            }

            // Initialize frame
            frames_[i].page_id = Frame::INVALID_PAGE_ID;
            // CRITICAL FIX (CRITICAL-1): Use atomic store for thread-safe write
            frames_[i].pin_count.store(0, std::memory_order_relaxed);
            frames_[i].is_dirty = false;

            // Add to LRU list (all frames start as free)
            lru_list_.push_back(i);
        }

        // Start background writer if enabled (Issue 2.20)
        if (config_.enable_background_writer)
        {
            startBackgroundWriter();
        }

        return Status::OK;
    }

    auto BufferPool::shutdown(ErrorContext *ctx) -> Status
    {
        // Stop background writer first (before acquiring mutex to avoid deadlock)
        stopBackgroundWriter();

        std::lock_guard<std::mutex> lock(mutex_);

        // Flush all dirty pages
        Status status = flushAll(ctx);

        // Memory is freed automatically by unique_ptr

        // Clear data structures
        lru_list_.clear();
        page_table_.clear();

        return status;
    }

    auto BufferPool::pinPage(uint32_t page_id, void **buffer, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null buffer pointer");
            return Status::INVALID_ARGUMENT;
        }

        // Check if page is already in buffer pool
        auto it = page_table_.find(page_id);
        if (it != page_table_.end())
        {
            // Cache hit
            uint32_t frame_index = it->second;

            // CRITICAL FIX (Issue 1.13): Check for pin count overflow BEFORE incrementing
            // If pin_count reaches UINT32_MAX and wraps to 0, the page could be evicted while in use
            // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
            if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) == UINT32_MAX)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Pin count overflow - page pinned too many times");
                return Status::INVALID_ARGUMENT;
            }

            // CRITICAL FIX (CRITICAL-1): Use atomic fetch_add for thread-safe increment
            frames_[frame_index].pin_count.fetch_add(1, std::memory_order_relaxed);

            // Clock Sweep: Increment usage count (capped at MAX_USAGE_COUNT)
            // This gives frequently accessed pages a longer stay in the buffer pool
            // CRITICAL FIX (CRITICAL-1): Use atomic operations for thread-safe read-modify-write
            uint32_t current_usage = frames_[frame_index].usage_count.load(std::memory_order_relaxed);
            if (current_usage < Frame::MAX_USAGE_COUNT)
            {
                frames_[frame_index].usage_count.fetch_add(1, std::memory_order_relaxed);
            }

            *buffer = frames_[frame_index].data.get();

            // Update LRU (still maintained for fallback)
            updateLru(frame_index);

            // MEDIUM-1 FIX: Use relaxed atomic increment for stats (consistency with other operations)
            stats_.hits.fetch_add(1, std::memory_order_relaxed);
            return Status::OK;
        }

        // Cache miss - need to load page
        // MEDIUM-1 FIX: Use relaxed atomic increment for stats
        stats_.misses.fetch_add(1, std::memory_order_relaxed);

        // Find a frame to use
        uint32_t frame_index;

        // First try to find an unpinned frame
        bool found_free = false;
        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            if (frames_[i].page_id == Frame::INVALID_PAGE_ID)
            {
                frame_index = i;
                found_free = true;
                break;
            }
        }

        if (!found_free)
        {
            // Need to evict a page
            Status status = evictPage(frame_index, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        // Read page from disk
        Status status = readPageFromDisk(page_id, frames_[frame_index].data.get(), ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // HIGH-1 FIX: Update page_table BEFORE frame metadata to ensure atomicity
        // This prevents the race where frame is updated but page_table is not,
        // which could cause evictPage() to fail to find the page in page_table
        // while the frame thinks it contains the page.
        //
        // Order of operations (CRITICAL for correctness):
        // 1. page_table_[page_id] = frame_index  (establish mapping first)
        // 2. frames_[frame_index].page_id = page_id  (then update frame)
        //
        // This way, if anything fails after step 1, the page_table entry exists
        // and evictPage() can clean it up properly. If we did it the other way,
        // we'd have an orphaned frame that thinks it contains a page but isn't
        // in the page_table, causing corruption.
        page_table_[page_id] = frame_index;

        // Update frame metadata (page_table already knows about this mapping)
        frames_[frame_index].page_id = page_id;
        // CRITICAL FIX (CRITICAL-1): Use atomic store for thread-safe write
        frames_[frame_index].pin_count.store(1, std::memory_order_relaxed);
        frames_[frame_index].is_dirty = false;

        // Clock Sweep: Initialize usage count for newly loaded page
        // Start with usage_count = 1 to give new pages a chance to stay
        // CRITICAL FIX (CRITICAL-1): Use atomic store for thread-safe write
        frames_[frame_index].usage_count.store(1, std::memory_order_relaxed);

        // Update LRU (still maintained for fallback)
        updateLru(frame_index);

        *buffer = frames_[frame_index].data.get();
        return Status::OK;
    }

    auto BufferPool::unpinPage(uint32_t page_id, bool is_dirty, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find the page in buffer pool
        auto it = page_table_.find(page_id);
        if (it == page_table_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page not in buffer pool");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t frame_index = it->second;

        // Check pin count
        // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
        if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page is not pinned");
            return Status::INVALID_ARGUMENT;
        }

        // Update dirty flag
        if (is_dirty)
        {
            frames_[frame_index].is_dirty = true;
        }

        // Decrement pin count
        // CRITICAL FIX (CRITICAL-1): Use atomic fetch_sub for thread-safe decrement
        frames_[frame_index].pin_count.fetch_sub(1, std::memory_order_relaxed);

        return Status::OK;
    }

    auto BufferPool::flushPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find the page in buffer pool
        auto it = page_table_.find(page_id);
        if (it == page_table_.end())
        {
            // Page not in buffer pool, nothing to flush
            return Status::OK;
        }

        uint32_t frame_index = it->second;

        // Check if dirty
        if (!frames_[frame_index].is_dirty)
        {
            // Not dirty, nothing to flush
            return Status::OK;
        }

        // Write to disk
        Status status = writePageToDisk(page_id, frames_[frame_index].data.get(), ctx);
        if (status == Status::OK)
        {
            frames_[frame_index].is_dirty = false;
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.flushes.fetch_add(1, std::memory_order_relaxed);
        }

        return status;
    }

    auto BufferPool::flushAll(ErrorContext *ctx) -> Status
    {
        // Note: caller must hold lock or this must be called from a method that holds lock

        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            if (frames_[i].page_id != Frame::INVALID_PAGE_ID && frames_[i].is_dirty)
            {
                Status status = writePageToDisk(frames_[i].page_id, frames_[i].data.get(), ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                frames_[i].is_dirty = false;
                // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                stats_.flushes.fetch_add(1, std::memory_order_relaxed);
            }
        }

        return Status::OK;
    }

    auto BufferPool::lockPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        uint32_t frame_index;

        // Find the frame index while holding buffer pool mutex
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = page_table_.find(page_id);
            if (it == page_table_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                  "Page not in buffer pool - must pin first");
                return Status::NOT_FOUND;
            }

            frame_index = it->second;

            // Page must be pinned before locking
            // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
            if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot lock unpinned page");
                return Status::INVALID_ARGUMENT;
            }
        }

        // Acquire the content mutex for this page (outside buffer pool mutex to avoid deadlock)
        frames_[frame_index].content_mutex->lock();

        return Status::OK;
    }

    auto BufferPool::unlockPage(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        uint32_t frame_index;

        // Find the frame index while holding buffer pool mutex
        {
            std::lock_guard<std::mutex> lock(mutex_);

            auto it = page_table_.find(page_id);
            if (it == page_table_.end())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Page not in buffer pool");
                return Status::NOT_FOUND;
            }

            frame_index = it->second;
        }

        // Release the content mutex for this page
        frames_[frame_index].content_mutex->unlock();

        return Status::OK;
    }

    auto BufferPool::evictPage(uint32_t &evicted_frame, ErrorContext *ctx) -> Status
    {
        // CLOCK SWEEP ALGORITHM (Issue 2.14)
        // This algorithm provides better eviction decisions than pure LRU by:
        // 1. Avoiding sequential scan pollution (frequently accessed pages stay in cache)
        // 2. Giving recently accessed pages a second chance (usage_count mechanism)
        // 3. Preferring clean pages over dirty pages for faster eviction
        //
        // Algorithm:
        // - Each frame has a usage_count (0-MAX_USAGE_COUNT)
        // - On access, usage_count is incremented (capped at MAX_USAGE_COUNT)
        // - Clock hand sweeps through frames circularly
        // - For each frame:
        //   * Skip if pinned (in use)
        //   * If usage_count == 0 and unpinned, evict it
        //   * Otherwise decrement usage_count (give it another chance)
        //
        // Spec: docs/specifications/STORAGE_ENGINE_BUFFER_POOL.md:402-465

        constexpr uint32_t MAX_PASSES = 2; // Maximum passes before forcing eviction

        uint32_t candidate_frame = UINT32_MAX;
        uint32_t passes = 0;
        uint32_t start_hand = clock_hand_;

        // Clock sweep: search for victim page
        while (passes < MAX_PASSES)
        {
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.clock_sweeps.fetch_add(1, std::memory_order_relaxed);

            // SAFETY: Bounds check for clock_hand_
            if (clock_hand_ >= config_.pool_size)
            {
                DEBUG_LOG_BP("Clock hand out of bounds: " << clock_hand_
                                                          << " >= pool_size: " << config_.pool_size);
                clock_hand_ = 0; // Reset to safe value
            }

            Frame &frame = frames_[clock_hand_];

            // Move clock hand forward (circular)
            uint32_t current_hand = clock_hand_;
            clock_hand_ = (clock_hand_ + 1) % config_.pool_size;

            // Track when we wrap around
            if (clock_hand_ == 0)
            {
                // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                stats_.clock_hand_resets.fetch_add(1, std::memory_order_relaxed);
            }

            // Skip pinned frames (in use)
            // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
            if (frame.pin_count.load(std::memory_order_relaxed) > 0)
            {
                continue;
            }

            // Skip empty frames (these should be allocated first, not evicted)
            if (frame.page_id == Frame::INVALID_PAGE_ID)
            {
                continue;
            }

            // Check usage count
            // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
            uint32_t current_usage_count = frame.usage_count.load(std::memory_order_relaxed);
            if (current_usage_count == 0)
            {
                // Found victim! This page hasn't been accessed recently
                // Prefer clean pages for faster eviction (READ ONLY optimization)
                if (!frame.is_dirty)
                {
                    // Clean page - evict immediately
                    candidate_frame = current_hand;
                    break;
                }
                else if (candidate_frame == UINT32_MAX)
                {
                    // Dirty page - remember as fallback, but keep looking for clean page
                    candidate_frame = current_hand;
                }
            }
            else
            {
                // Give page another chance - decrement usage count
                // CRITICAL FIX (CRITICAL-1): Use atomic fetch_sub for thread-safe decrement
                frame.usage_count.fetch_sub(1, std::memory_order_relaxed);
            }

            // Check if we've completed a full pass
            if (clock_hand_ == start_hand)
            {
                passes++;
                if (candidate_frame != UINT32_MAX)
                {
                    // We found a dirty page candidate, use it
                    break;
                }
                // Otherwise continue for another pass
            }
        }

        // Emergency fallback: force evict the least recently used dirty page
        // This should rarely happen - only if all pages have high usage counts
        if (candidate_frame == UINT32_MAX)
        {
            DEBUG_LOG_BP("Clock sweep failed after " << MAX_PASSES
                                                     << " passes, using LRU fallback");

            // Fallback to LRU for emergency eviction
            for (unsigned int frame_index : lru_list_)
            {
                // DEFENSIVE CHECK (Issue 3.2): Validate LRU list entries
                // This is NOT redundant - it validates data from lru_list_ which could be corrupted
                if (frame_index >= config_.pool_size)
                {
                    continue; // Skip invalid entries
                }

                // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
                if (frames_[frame_index].pin_count.load(std::memory_order_relaxed) == 0 &&
                    frames_[frame_index].page_id != Frame::INVALID_PAGE_ID)
                {
                    candidate_frame = frame_index;
                    break;
                }
            }
        }

        // Final check: did we find any evictable page?
        if (candidate_frame == UINT32_MAX)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Buffer pool full - all pages are pinned");
            return Status::INVALID_ARGUMENT;
        }

        // ALGORITHM OUTPUT VALIDATION (Issue 3.2): Final safety check
        // This is NOT redundant - it validates the algorithm's output (candidate_frame) which is
        // computed from clock sweep or LRU fallback logic. Different variable than internal checks.
        if (candidate_frame >= config_.pool_size)
        {
            DEBUG_LOG_BP("Invalid candidate_frame: " << candidate_frame
                                                     << " >= pool_size: " << config_.pool_size);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Invalid frame index selected for eviction");
            return Status::IO_ERROR;
        }

        evicted_frame = candidate_frame;

        // CRITICAL FIX (Issue 2.2): Consistency check - verify frame is unpinned
        // This MUST be fatal in ALL builds (not just debug) to prevent corruption
        // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
        uint32_t evicted_pin_count = frames_[evicted_frame].pin_count.load(std::memory_order_relaxed);
        if (evicted_pin_count != 0)
        {
            DEBUG_LOG_BP("CONSISTENCY ERROR: Attempting to evict pinned frame "
                         << evicted_frame
                         << " with pin_count=" << evicted_pin_count);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Buffer pool corruption: attempting to evict pinned page");
            return Status::IO_ERROR;
        }

        // Track whether this is a clean or dirty eviction
        bool was_dirty = frames_[evicted_frame].is_dirty;
        uint32_t evicted_page_id = frames_[evicted_frame].page_id;

        // SAFETY: Verify the page_id is valid before using it
        if (evicted_page_id == Frame::INVALID_PAGE_ID)
        {
            DEBUG_LOG_BP("Evicting frame with INVALID_PAGE_ID at index " << evicted_frame);
            // This is actually OK - might be a free frame, just skip the flush and erase
            evicted_frame = candidate_frame;
            return Status::OK;
        }

        // If dirty, flush first
        if (was_dirty)
        {
            Status status =
                writePageToDisk(evicted_page_id, frames_[evicted_frame].data.get(), ctx);
            if (status != Status::OK)
            {
                return status;
            }
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.flushes.fetch_add(1, std::memory_order_relaxed);
            stats_.evictions_dirty.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.evictions_clean.fetch_add(1, std::memory_order_relaxed);
        }

        // CRITICAL FIX (Issue 2.2): Verify page_id exists in page_table before erasing
        // This MUST be fatal in ALL builds (not just debug) to prevent corruption
        auto page_table_it = page_table_.find(evicted_page_id);
        if (page_table_it == page_table_.end())
        {
            DEBUG_LOG_BP("CONSISTENCY ERROR: page_id "
                         << evicted_page_id << " not found in page_table during eviction");
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Buffer pool corruption: evicting page not in page_table");
            return Status::IO_ERROR;
        }

        // CRITICAL FIX (Issue 2.2): Verify consistency - page_table points to correct frame
        // This MUST be fatal in ALL builds (not just debug) to prevent corruption
        if (page_table_it->second != evicted_frame)
        {
            DEBUG_LOG_BP("CONSISTENCY ERROR: page_table["
                         << evicted_page_id << "] = " << page_table_it->second
                         << " but evicting frame " << evicted_frame);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Buffer pool corruption: page_table frame_index mismatch");
            return Status::IO_ERROR;
        }

        // Remove from page table
        page_table_.erase(page_table_it);

        // Reset frame (including Clock Sweep usage_count)
        frames_[evicted_frame].page_id = Frame::INVALID_PAGE_ID;
        frames_[evicted_frame].is_dirty = false;
        // CRITICAL FIX (CRITICAL-1): Use atomic store for thread-safe write
        frames_[evicted_frame].usage_count.store(0, std::memory_order_relaxed); // Reset usage count for next page

        // MEDIUM-1 FIX: Use relaxed atomic increment for stats
        stats_.evictions.fetch_add(1, std::memory_order_relaxed);
        return Status::OK;
    }

    auto BufferPool::readPageFromDisk(uint32_t page_id, uint8_t *buffer, ErrorContext *ctx)
        -> Status
    {
        return db_->read_page(page_id, buffer, ctx);
    }

    auto BufferPool::writePageToDisk(uint32_t page_id, const uint8_t *buffer, ErrorContext *ctx)
        -> Status
    {
        return db_->write_page(page_id, buffer, ctx);
    }

    void BufferPool::updateLru(uint32_t frame_index)
    {
        // CRITICAL: This method MUST be called with mutex_ held
        // The LRU list is shared state and concurrent modification will cause corruption
        // We use assert() because this is an internal consistency requirement
        // NOTE: There's no portable way to assert a mutex is locked, so we document the requirement
        // and rely on correct usage patterns. All callers (pinPage) do hold the lock.

        // INTERNAL CONSISTENCY CHECK (Issue 3.2 consolidation):
        // This is an internal method - callers must provide valid frame_index
        // Use assertion instead of runtime check since this indicates a programming error
        assert(frame_index < config_.pool_size && "updateLru called with invalid frame_index");

        // Remove from current position in LRU list
        lru_list_.remove(frame_index);

        // Add to end of LRU list (most recently used)
        lru_list_.push_back(frame_index);
    }

    auto BufferPool::allocatePage(uint32_t *page_id_out, void **buffer, ErrorContext *ctx) -> Status
    {
        if (page_id_out == nullptr || buffer == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Null output parameters");
            return Status::INVALID_ARGUMENT;
        }

        // Allocate a new page ID from database
        uint32_t new_page_id;
        Status status = db_->allocate_page_id(&new_page_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Pin the new page (this will allocate a frame and initialize it)
        status = pinPage(new_page_id, buffer, ctx);
        if (status != Status::OK)
        {
            // Failed to pin - the page_id has been allocated but not used
            // For simplicity, we don't reclaim it (would require free list)
            return status;
        }

        // Mark the new page as dirty since it needs to be written
        status = markDirty(new_page_id, ctx);
        if (status != Status::OK)
        {
            // Unpin on failure
            unpinPage(new_page_id, false, ctx);
            return status;
        }

        *page_id_out = new_page_id;
        return Status::OK;
    }

    auto BufferPool::markDirty(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Find the page in buffer pool
        auto it = page_table_.find(page_id);
        if (it == page_table_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page not in buffer pool");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t frame_index = it->second;

        // Mark the frame as dirty
        frames_[frame_index].is_dirty = true;

        return Status::OK;
    }

    // ===========================================================================================
    // ISSUE 2.20: ADAPTIVE FLUSHING - BACKGROUND WRITER IMPLEMENTATION
    // ===========================================================================================
    //
    // This implementation addresses the audit finding:
    // "Buffer Pool - No Adaptive Flushing"
    // File: src/core/buffer_pool.cpp:438-449
    // Severity: MAJOR
    //
    // Problem:
    // - Flushing only occurs when evicting dirty pages
    // - Causes checkpoint storms (unpredictable I/O spikes)
    // - Long checkpoint times block transactions
    // - No proactive dirty page management
    //
    // Solution:
    // - Background writer thread with adaptive flushing
    // - Dirty ratio monitoring (percentage of dirty pages)
    // - Three-tier flushing strategy:
    //   * Low threshold (25%): Start gentle flushing
    //   * High threshold (50%): Aggressive flushing
    //   * Checkpoint threshold (75%): Emergency flushing
    // - Smooths I/O load over time
    // - Prevents checkpoint storms
    //
    // Benefits:
    // - Predictable I/O patterns
    // - Shorter checkpoint times (less dirty pages to flush)
    // - Better transaction throughput (fewer eviction stalls)
    // - Configurable flushing behavior
    //
    // Algorithm based on PostgreSQL's bgwriter and MySQL InnoDB's adaptive flushing
    // Spec: docs/specifications/STORAGE_ENGINE_BUFFER_POOL.md (background writer)

    void BufferPool::startBackgroundWriter()
    {
        // CRITICAL: This method is called while holding mutex_ in initialize()
        // Do NOT acquire mutex_ here to avoid deadlock

        bgwriter_shutdown_.store(false, std::memory_order_release);
        bgwriter_thread_ = std::make_unique<std::thread>(&BufferPool::backgroundWriterMain, this);
    }

    void BufferPool::stopBackgroundWriter()
    {
        // Signal shutdown (no mutex needed - atomic operation)
        bgwriter_shutdown_.store(true, std::memory_order_release);

        // Wake up background writer if sleeping
        {
            std::lock_guard<std::mutex> lock(bgwriter_mutex_);
            bgwriter_cv_.notify_one();
        }

        // Wait for thread to finish
        if (bgwriter_thread_ && bgwriter_thread_->joinable())
        {
            bgwriter_thread_->join();
        }
    }

    void BufferPool::backgroundWriterMain()
    {
        // Background writer thread main loop
        // This runs continuously until shutdown is requested

        ErrorContext ctx;

        while (!bgwriter_shutdown_.load(std::memory_order_acquire))
        {
            // Sleep for configured delay (using condition variable for interruptible sleep)
            {
                std::unique_lock<std::mutex> lock(bgwriter_mutex_);
                bgwriter_cv_.wait_for(lock,
                                       std::chrono::milliseconds(config_.bgwriter_delay_ms),
                                       [this] { return bgwriter_shutdown_.load(std::memory_order_acquire); });
            }

            // Check shutdown again after waking up
            if (bgwriter_shutdown_.load(std::memory_order_acquire))
            {
                break;
            }

            // Perform one cycle of adaptive flushing
            backgroundWriterFlush(&ctx);

            // Update statistics (dirty ratio tracking)
            {
                std::lock_guard<std::mutex> lock(mutex_);
                stats_.dirty_ratio_current = calculateDirtyRatio();
                if (stats_.dirty_ratio_current > stats_.dirty_ratio_max)
                {
                    stats_.dirty_ratio_max = stats_.dirty_ratio_current;
                }
            }
        }
    }

    void BufferPool::backgroundWriterFlush(ErrorContext *ctx)
    {
        // Perform one cycle of adaptive flushing
        // This implements the three-tier flushing strategy based on dirty ratio

        uint32_t pages_written = 0;
        uint32_t pages_to_write = 0;

        // Acquire mutex for the entire flushing cycle
        std::lock_guard<std::mutex> lock(mutex_);

        // Calculate current dirty ratio
        double dirty_ratio = calculateDirtyRatio();

        // Determine how many pages to write based on dirty ratio (adaptive algorithm)
        if (dirty_ratio >= config_.dirty_ratio_checkpoint)
        {
            // EMERGENCY: Checkpoint threshold exceeded
            // Write maximum pages to prevent checkpoint storm
            pages_to_write = config_.bgwriter_max_pages;
        }
        else if (dirty_ratio >= config_.dirty_ratio_high)
        {
            // AGGRESSIVE: High threshold exceeded
            // Write 75% of maximum pages
            pages_to_write = static_cast<uint32_t>(config_.bgwriter_max_pages * 0.75);
        }
        else if (dirty_ratio >= config_.dirty_ratio_low)
        {
            // GENTLE: Low threshold exceeded
            // Write scaled based on how far above low threshold
            // Scale linearly from 25% to 75% of max_pages
            double scale = (dirty_ratio - config_.dirty_ratio_low) /
                           (config_.dirty_ratio_high - config_.dirty_ratio_low);
            pages_to_write = static_cast<uint32_t>(config_.bgwriter_max_pages * (0.25 + scale * 0.50));
        }
        else
        {
            // Below low threshold - no flushing needed
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.bgwriter_runs.fetch_add(1, std::memory_order_relaxed);
            return;
        }

        // Ensure we write at least 1 page if dirty ratio triggered flushing
        if (pages_to_write == 0)
        {
            pages_to_write = 1;
        }

        // Flush dirty pages (up to pages_to_write limit)
        // Strategy: Iterate through frames and flush dirty, unpinned pages
        // Prefer pages with lower usage_count (clock sweep integration)
        for (uint32_t i = 0; i < config_.pool_size && pages_written < pages_to_write; i++)
        {
            Frame &frame = frames_[i];

            // Skip non-dirty pages
            if (!frame.is_dirty)
            {
                continue;
            }

            // Skip pinned pages (in use by transactions)
            // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
            if (frame.pin_count.load(std::memory_order_relaxed) > 0)
            {
                continue;
            }

            // Skip invalid pages
            if (frame.page_id == Frame::INVALID_PAGE_ID)
            {
                continue;
            }

            // Prefer pages with lower usage_count (cold pages)
            // This integrates with Clock Sweep eviction algorithm
            // CRITICAL FIX (CRITICAL-1): Use atomic load for thread-safe read
            if (frame.usage_count.load(std::memory_order_relaxed) > 2 && pages_written < pages_to_write / 2)
            {
                // Skip hot pages in first half of writes (only flush cold pages)
                continue;
            }

            // Flush this dirty page
            Status status = writePageToDisk(frame.page_id, frame.data.get(), ctx);
            if (status == Status::OK)
            {
                frame.is_dirty = false;
                pages_written++;
                // MEDIUM-1 FIX: Use relaxed atomic increment for stats
                stats_.bgwriter_pages_written.fetch_add(1, std::memory_order_relaxed);
            }
            else
            {
                // Log error but continue flushing other pages
                // Background writer should be resilient to transient I/O errors
                DEBUG_LOG_BP("Background writer failed to flush page "
                             << frame.page_id << ": " << static_cast<int>(status));
            }
        }

        // Update statistics
        // MEDIUM-1 FIX: Use relaxed atomic increment for stats
        stats_.bgwriter_runs.fetch_add(1, std::memory_order_relaxed);
        if (pages_written >= pages_to_write)
        {
            // MEDIUM-1 FIX: Use relaxed atomic increment for stats
            stats_.bgwriter_maxwritten.fetch_add(1, std::memory_order_relaxed);
        }
    }

    double BufferPool::calculateDirtyRatio() const
    {
        // CRITICAL: Caller must hold mutex_
        // Calculate the ratio of dirty pages to total pages

        uint32_t dirty_count = getDirtyPageCount();
        uint32_t total_pages = config_.pool_size;

        if (total_pages == 0)
        {
            return 0.0;
        }

        return static_cast<double>(dirty_count) / static_cast<double>(total_pages);
    }

    uint32_t BufferPool::getDirtyPageCount() const
    {
        // CRITICAL: Caller must hold mutex_
        // Count the number of dirty pages in the buffer pool

        uint32_t dirty_count = 0;

        for (uint32_t i = 0; i < config_.pool_size; i++)
        {
            if (frames_[i].is_dirty && frames_[i].page_id != Frame::INVALID_PAGE_ID)
            {
                dirty_count++;
            }
        }

        return dirty_count;
    }

} // namespace scratchbird::core
