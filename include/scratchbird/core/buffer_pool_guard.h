/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/error_context.h"
#include <stdexcept>

namespace scratchbird::core
{
    /**
     * RAII guard for buffer pool page pinning/unpinning
     *
     * Automatically unpins a buffer pool page when the guard goes out of scope,
     * ensuring that pinned pages are always properly released even in the presence
     * of exceptions.
     *
     * Usage:
     *   {
     *       void* buffer = nullptr;
     *       BufferPoolGuard guard(buffer_pool, page_id, &buffer, ctx);
     *
     *       // Use buffer...
     *
     *       guard.markDirty();  // Mark as dirty if modified
     *   } // Automatically unpins on scope exit
     */
    class BufferPoolGuard
    {
    private:
        BufferPool* pool_;
        uint32_t page_id_;
        bool dirty_;
        bool released_;

    public:
        /**
         * Constructor - pins the specified page
         *
         * @param pool Buffer pool instance
         * @param page_id Page ID to pin
         * @param buffer Output parameter for page buffer pointer
         * @param ctx Error context
         * @throws std::runtime_error if pinPage fails
         */
        BufferPoolGuard(BufferPool* pool, uint32_t page_id, void** buffer, ErrorContext* ctx)
            : pool_(pool), page_id_(page_id), dirty_(false), released_(false)
        {
            if (!pool_)
            {
                throw std::runtime_error("BufferPoolGuard: null pool");
            }

            Status status = pool_->pinPage(page_id_, buffer, ctx);
            if (status != Status::OK)
            {
                throw std::runtime_error("BufferPoolGuard: failed to pin page");
            }
        }

        /**
         * Destructor - automatically unpins the page if not already released
         */
        ~BufferPoolGuard()
        {
            if (!released_ && pool_)
            {
                pool_->unpinPage(page_id_, dirty_, nullptr);
            }
        }

        /**
         * Mark the page as dirty (modified)
         * Call this before the guard goes out of scope if the page was modified
         */
        void markDirty()
        {
            dirty_ = true;
        }

        /**
         * Manually release the page (unpin it)
         * After calling this, the destructor will not unpin the page
         */
        void release()
        {
            if (!released_ && pool_)
            {
                pool_->unpinPage(page_id_, dirty_, nullptr);
                released_ = true;
            }
        }

        /**
         * Get the page ID
         */
        uint32_t pageId() const { return page_id_; }

        /**
         * Check if the page has been marked dirty
         */
        bool isDirty() const { return dirty_; }

        /**
         * Check if the page has been released
         */
        bool isReleased() const { return released_; }

        // Prevent copying
        BufferPoolGuard(const BufferPoolGuard&) = delete;
        BufferPoolGuard& operator=(const BufferPoolGuard&) = delete;

        // Allow moving
        BufferPoolGuard(BufferPoolGuard&& other) noexcept
            : pool_(other.pool_)
            , page_id_(other.page_id_)
            , dirty_(other.dirty_)
            , released_(other.released_)
        {
            other.released_ = true;  // Prevent double-unpin
        }

        BufferPoolGuard& operator=(BufferPoolGuard&& other) noexcept
        {
            if (this != &other)
            {
                // Release current page if any
                if (!released_ && pool_)
                {
                    pool_->unpinPage(page_id_, dirty_, nullptr);
                }

                // Move from other
                pool_ = other.pool_;
                page_id_ = other.page_id_;
                dirty_ = other.dirty_;
                released_ = other.released_;

                other.released_ = true;  // Prevent double-unpin
            }
            return *this;
        }
    };

} // namespace scratchbird::core
