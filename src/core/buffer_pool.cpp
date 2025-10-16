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
            frames_[i].pin_count = 0;
            frames_[i].is_dirty = false;

            // Add to LRU list (all frames start as free)
            lru_list_.push_back(i);
        }

        return Status::OK;
    }

    auto BufferPool::shutdown(ErrorContext *ctx) -> Status
    {
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
            if (frames_[frame_index].pin_count == UINT32_MAX)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Pin count overflow - page pinned too many times");
                return Status::INVALID_ARGUMENT;
            }

            frames_[frame_index].pin_count++;

            // Clock Sweep: Increment usage count (capped at MAX_USAGE_COUNT)
            // This gives frequently accessed pages a longer stay in the buffer pool
            if (frames_[frame_index].usage_count < Frame::MAX_USAGE_COUNT)
            {
                frames_[frame_index].usage_count++;
            }

            *buffer = frames_[frame_index].data.get();

            // Update LRU (still maintained for fallback)
            updateLru(frame_index);

            stats_.hits++;
            return Status::OK;
        }

        // Cache miss - need to load page
        stats_.misses++;

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

        // Update frame metadata
        frames_[frame_index].page_id = page_id;
        frames_[frame_index].pin_count = 1;
        frames_[frame_index].is_dirty = false;

        // Clock Sweep: Initialize usage count for newly loaded page
        // Start with usage_count = 1 to give new pages a chance to stay
        frames_[frame_index].usage_count = 1;

        // Update page table
        page_table_[page_id] = frame_index;

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
        if (frames_[frame_index].pin_count == 0)
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
        frames_[frame_index].pin_count--;

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
            stats_.flushes++;
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
                stats_.flushes++;
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
            if (frames_[frame_index].pin_count == 0)
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
            stats_.clock_sweeps++;

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
                stats_.clock_hand_resets++;
            }

            // Skip pinned frames (in use)
            if (frame.pin_count > 0)
            {
                continue;
            }

            // Skip empty frames (these should be allocated first, not evicted)
            if (frame.page_id == Frame::INVALID_PAGE_ID)
            {
                continue;
            }

            // Check usage count
            if (frame.usage_count == 0)
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
                frame.usage_count--;
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
                if (frame_index >= config_.pool_size)
                {
                    continue; // Skip invalid entries
                }

                if (frames_[frame_index].pin_count == 0 &&
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

        // SAFETY: Final bounds check before using candidate_frame
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
        if (frames_[evicted_frame].pin_count != 0)
        {
            DEBUG_LOG_BP("CONSISTENCY ERROR: Attempting to evict pinned frame "
                         << evicted_frame
                         << " with pin_count=" << frames_[evicted_frame].pin_count);
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
            stats_.flushes++;
            stats_.evictions_dirty++;
        }
        else
        {
            stats_.evictions_clean++;
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
        frames_[evicted_frame].usage_count = 0; // Reset usage count for next page

        stats_.evictions++;
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

        // SAFETY: Bounds check before accessing LRU list
        if (frame_index >= config_.pool_size)
        {
            // This should never happen if callers are correct
            return; // Silently fail in release, assert in debug
        }

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

} // namespace scratchbird::core
