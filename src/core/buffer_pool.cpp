#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/database.h"
#include <cstring>
#include <algorithm>


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
                frames_[frame_index].pin_count++;
                *buffer = frames_[frame_index].data.get();

                // Update LRU
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

            // Update page table
            page_table_[page_id] = frame_index;

            // Update LRU
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
                    Status status =
                        writePageToDisk(frames_[i].page_id, frames_[i].data.get(), ctx);
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

        auto BufferPool::evictPage(uint32_t &evicted_frame, ErrorContext *ctx) -> Status
        {
            // Find a page to evict using LRU
            for (unsigned int frame_index : lru_list_)
            {
                // Can only evict unpinned pages
                if (frames_[frame_index].pin_count == 0)
                {
                    // Found a candidate
                    evicted_frame = frame_index;

                    // If dirty, flush first
                    if (frames_[frame_index].is_dirty)
                    {
                        Status status = writePageToDisk(frames_[frame_index].page_id,
                                                           frames_[frame_index].data.get(), ctx);
                        if (status != Status::OK)
                        {
                            return status;
                        }
                        stats_.flushes++;
                    }

                    // Remove from page table
                    page_table_.erase(frames_[frame_index].page_id);

                    // Reset frame
                    frames_[frame_index].page_id = Frame::INVALID_PAGE_ID;
                    frames_[frame_index].is_dirty = false;

                    stats_.evictions++;
                    return Status::OK;
                }
            }

            // No unpinned pages found
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Buffer pool full - all pages are pinned");
            return Status::INVALID_ARGUMENT;
        }

        auto BufferPool::readPageFromDisk(uint32_t page_id, uint8_t *buffer, ErrorContext *ctx) -> Status
        {
            return db_->read_page(page_id, buffer, ctx);
        }

        auto BufferPool::writePageToDisk(uint32_t page_id, const uint8_t *buffer,
                                              ErrorContext *ctx) -> Status
        {
            return db_->write_page(page_id, buffer, ctx);
        }

        void BufferPool::updateLru(uint32_t frame_index)
        {
            // Remove from current position
            lru_list_.remove(frame_index);

            // Add to end (most recently used)
            lru_list_.push_back(frame_index);
        }

    } // namespace scratchbird::core
