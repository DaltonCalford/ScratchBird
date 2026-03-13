/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * BRIN (Block Range Index) Implementation
 * Complete implementation for space-efficient block range indexing
 *
 * BRIN indexes store min/max summaries for ranges of blocks, providing
 * 90%+ space savings vs B-Tree while maintaining acceptable performance
 * for naturally ordered data (time-series, logs, append-only tables).
 */

#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/brin_minmax_ops.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <set>
#include <algorithm>
#include <limits>

namespace scratchbird::core
{

// Forward declaration - Static helper for visibility checking
static bool isRangeVisible(uint64_t xmin, uint64_t xmax,
                           uint64_t current_xid,
                           TransactionManager *txn_mgr);

// =============================================================================
// BrinIndex Implementation
// =============================================================================

BrinIndex::BrinIndex(Database *db, SBBrinIndex index_info)
    : db_(db), index_info_(std::move(index_info))
{
}

BrinIndex::~BrinIndex()
{
}

Status BrinIndex::create(Database *db,
                        const UuidV7Bytes &index_uuid,
                        const UuidV7Bytes &table_uuid,
                        const std::vector<UuidV7Bytes> &column_uuids,
                        uint8_t value_type,
                        uint16_t range_size,
                        GPID root_gpid,
                        ErrorContext *ctx)
{
    if (!db || root_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments");
        return Status::INVALID_ARGUMENT;
    }

    PageManager *page_mgr = db->page_manager();
    BufferPool *buffer_pool = db->buffer_pool();
    TransactionManager *txn_mgr = db->transaction_manager();

    if (!page_mgr || !buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing database components");
        return Status::INVALID_ARGUMENT;
    }

    uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));

    // Pin and initialize root page
    void *page_buffer = nullptr;
    Status status = buffer_pool->pinPageGlobal(root_gpid, &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN root page");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinPage *root = reinterpret_cast<SBBrinPage*>(page_data);
    std::memset(root, 0, sizeof(SBBrinPage));

    // Initialize page header (no initPageHeader available, manual init)
    root->brin_index_uuid = index_uuid;
    root->brin_table_uuid = table_uuid;
    root->brin_flags = static_cast<uint16_t>(BrinFlags::ROOT);
    root->brin_count = 0;
    root->brin_free_space = db->page_size() - sizeof(SBBrinPage);
    root->brin_range_size = range_size;
    root->brin_first_block = 0;
    root->brin_last_block = 0;
    uint64_t create_xid = ConnectionContext::getCurrentTransactionId();
    if (create_xid == 0)
    {
        create_xid = txn_mgr->getCurrentXid();
    }
    root->brin_xmin = create_xid;
    root->brin_xmax = 0;
    root->brin_ranges_total = 0;
    root->brin_ranges_deleted = 0;

    buffer_pool->unpinPageGlobal(root_gpid, true, ctx); // Mark dirty

    LOG_INFO(GENERAL, "Created BRIN index with root page %u, range size %u",
             root_page, range_size);

    return Status::OK;
}

Status BrinIndex::create(Database *db,
                        const UuidV7Bytes &index_uuid,
                        const UuidV7Bytes &table_uuid,
                        const std::vector<UuidV7Bytes> &column_uuids,
                        uint8_t value_type,
                        uint16_t range_size,
                        uint16_t tablespace_id,
                        uint32_t *root_page_out,
                        ErrorContext *ctx)
{
    if (!db || !root_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments");
        return Status::INVALID_ARGUMENT;
    }

    PageManager *page_mgr = db->page_manager();
    if (!page_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing page manager");
        return Status::INVALID_ARGUMENT;
    }

    GPID root_gpid = 0;
    Status status = page_mgr->allocatePageInTablespace(tablespace_id, &root_gpid, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    *root_page_out = static_cast<uint32_t>(getPageNumber(root_gpid));
    return create(db, index_uuid, table_uuid, column_uuids, value_type, range_size, root_gpid, ctx);
}

Status BrinIndex::create(Database *db,
                        const UuidV7Bytes &index_uuid,
                        const UuidV7Bytes &table_uuid,
                        const std::vector<UuidV7Bytes> &column_uuids,
                        uint8_t value_type,
                        uint16_t range_size,
                        uint32_t *root_page_out,
                        ErrorContext *ctx)
{
    return create(db, index_uuid, table_uuid, column_uuids, value_type,
                  range_size, PRIMARY_TABLESPACE_ID, root_page_out, ctx);
}

std::unique_ptr<BrinIndex> BrinIndex::open(Database *db,
                                          const UuidV7Bytes &index_uuid,
                                          GPID root_gpid,
                                          ErrorContext *ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
        return nullptr;
    }

    uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));
    SBBrinIndex index_info;
    std::memcpy(index_info.idx_uuid.bytes.data(), index_uuid.bytes.data(), 16);
    index_info.idx_root_page = root_page;
    index_info.idx_tablespace_id = getTablespaceID(root_gpid);
    index_info.idx_range_size = 128;

    // Read range size from root page (authoritative)
    uint8_t *page_data = nullptr;
    if (db->buffer_pool()->pinPageGlobal(root_gpid, reinterpret_cast<void **>(&page_data), ctx) == Status::OK &&
        page_data != nullptr)
    {
        auto *root = reinterpret_cast<SBBrinPage *>(page_data);
        if (root->brin_range_size != 0)
        {
            index_info.idx_range_size = root->brin_range_size;
        }
        db->buffer_pool()->unpinPageGlobal(root_gpid, false, ctx);
    }

    auto index = std::make_unique<BrinIndex>(db, index_info);

    // Build revmap for O(1) lookups
    Status status = index->build_revmap(ctx);
    if (status != Status::OK)
    {
        LOG_WARNING(GENERAL, "BRIN: Failed to build revmap, will use linear scan");
        // Don't fail - revmap is optional optimization
    }

    return index;
}

std::unique_ptr<BrinIndex> BrinIndex::open(Database *db,
                                           const UuidV7Bytes &index_uuid,
                                           uint32_t root_page,
                                           ErrorContext *ctx)
{
    GPID root_gpid = makeGPID(PRIMARY_TABLESPACE_ID, root_page);
    return open(db, index_uuid, root_gpid, ctx);
}

GPID BrinIndex::indexGPID(uint64_t page_num) const
{
    return makeGPID(index_info_.idx_tablespace_id, page_num);
}

Status BrinIndex::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx,
                               BufferPool::AccessStrategy strategy)
{
    return db_->buffer_pool()->pinPageGlobal(indexGPID(page_num), buffer, ctx, strategy);
}

Status BrinIndex::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx)
{
    return db_->buffer_pool()->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
}

Status BrinIndex::insert(const std::vector<uint8_t> &value,
                        uint32_t block_number,
                        ErrorContext *ctx)
{
    if (!db_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No database");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
    if (current_xid == 0)
    {
        current_xid = txn_mgr->getCurrentXid();
    }
    if (current_xid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction");
        return Status::INVALID_ARGUMENT;
    }

    // Calculate which range this block belongs to
    uint32_t range_index = block_number / index_info_.idx_range_size;
    uint32_t range_start = range_index * index_info_.idx_range_size;
    uint32_t range_end = range_start + index_info_.idx_range_size - 1;

    // Try O(1) revmap lookup first
    uint32_t current_page_num = revmap_lookup(range_start);
    void *page_buffer = nullptr;
    Status status;

    if (current_page_num != 0)
    {
        // Revmap hit - pin the page directly
        status = pinIndexPage(current_page_num, &page_buffer, ctx);
        if (status != Status::OK || !page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page from revmap");
            return Status::IO_ERROR;
        }
    }
    else
    {
        // Revmap miss - fall back to linear scan
        current_page_num = index_info_.idx_root_page;

        // Traverse pages to find the right one
        while (current_page_num != 0)
        {
            status = pinIndexPage(current_page_num, &page_buffer, ctx);
            if (status != Status::OK || !page_buffer)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
                return Status::IO_ERROR;
            }

            uint8_t *temp_page_data = static_cast<uint8_t*>(page_buffer);
            SBBrinPage *temp_page = reinterpret_cast<SBBrinPage*>(temp_page_data);

            // Check if this page covers the target range
            if (temp_page->brin_count == 0 ||
                (range_start >= temp_page->brin_first_block && range_start <= temp_page->brin_last_block))
            {
                // This page should contain the range (or is empty)
                break;
            }

            // Check if we need to go to the next page
            uint64_t next_page = temp_page->brin_right_sibling;
            unpinIndexPage(current_page_num, false, ctx);

            if (next_page == 0)
            {
                // No more pages - use the last page
                status = pinIndexPage(current_page_num, &page_buffer, ctx);
                break;
            }

            current_page_num = static_cast<uint32_t>(next_page);
        }
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

    // Find existing range or add new one
    uint8_t *range_ptr = page_data + sizeof(SBBrinPage);
    bool found = false;
    bool updated = false;

    for (uint16_t i = 0; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);

        if (range->brn_start_block == range_start)
        {
            // Found the range, update min/max
            found = true;

            uint8_t *min_value_ptr = range_ptr + sizeof(SBBrinRange);
            uint8_t *max_value_ptr = min_value_ptr + range->brn_min_len;

            std::vector<uint8_t> current_min(min_value_ptr, min_value_ptr + range->brn_min_len);
            std::vector<uint8_t> current_max(max_value_ptr, max_value_ptr + range->brn_max_len);

            // Update min if needed
            if (BrinMinmaxOps::compare(value, current_min) < 0)
            {
                std::memcpy(min_value_ptr, value.data(),
                           std::min(value.size(), static_cast<size_t>(range->brn_min_len)));
                updated = true;
            }

            // Update max if needed
            if (BrinMinmaxOps::compare(value, current_max) > 0)
            {
                std::memcpy(max_value_ptr, value.data(),
                           std::min(value.size(), static_cast<size_t>(range->brn_max_len)));
                updated = true;
            }

            break;
        }

        // Move to next range
        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        range_ptr += range_size;
    }

    if (!found)
    {
        // Add new range
        uint16_t value_len = std::min(static_cast<size_t>(value.size()), static_cast<size_t>(256));
        size_t new_range_size = sizeof(SBBrinRange) + value_len * 2;

        if (page->brin_free_space < new_range_size)
        {
            // Page is full - split it
            unpinIndexPage(current_page_num, false, ctx);

            LOG_DEBUG(GENERAL, "BRIN: Page %u full, splitting before insert", current_page_num);
            status = split_page(current_page_num, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to split BRIN page");
                return status;
            }

            // Retry insertion after split (recursive call)
            return insert(value, block_number, ctx);
        }

        // Create new range at end
        SBBrinRange *new_range = reinterpret_cast<SBBrinRange*>(range_ptr);
        new_range->brn_start_block = range_start;
        new_range->brn_end_block = range_end;
        new_range->brn_flags = 0;
        new_range->brn_min_len = value_len;
        new_range->brn_max_len = value_len;
        new_range->brn_xmin = current_xid;
        new_range->brn_xmax = 0;

        // Copy min and max (initially same)
        uint8_t *min_ptr = range_ptr + sizeof(SBBrinRange);
        uint8_t *max_ptr = min_ptr + value_len;
        std::memcpy(min_ptr, value.data(), value_len);
        std::memcpy(max_ptr, value.data(), value_len);

        page->brin_count++;
        page->brin_free_space -= new_range_size;
        page->brin_ranges_total++;

        // Add to revmap
        revmap_add(range_start, current_page_num);

        updated = true;

        LOG_DEBUG(GENERAL, "BRIN: Added new range [%u-%u] to page %u",
                 range_start, range_end, current_page_num);
    }

    unpinIndexPage(current_page_num, updated, ctx);

    return Status::OK;
}

Status BrinIndex::scan(const std::vector<uint8_t> *min_value,
                      const std::vector<uint8_t> *max_value,
                      uint64_t current_xid,
                      std::vector<uint32_t> *block_numbers_out,
                      ErrorContext *ctx)
{
    if (!db_ || !block_numbers_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments");
        return Status::INVALID_ARGUMENT;
    }

    block_numbers_out->clear();

    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    // Traverse all pages in the sibling chain
    uint32_t current_page_num = index_info_.idx_root_page;
    uint64_t total_ranges = 0;

    while (current_page_num != 0)
    {
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page_num, &page_buffer, ctx,
                                     BufferPool::AccessStrategy::Sequential);
        if (status != Status::OK || !page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
            return Status::IO_ERROR;
        }

        uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
        SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

        // Scan all ranges on this page
        uint8_t *range_ptr = page_data + sizeof(SBBrinPage);

        auto decode_uint64_be = [](const std::vector<uint8_t> &bytes, uint64_t *out) -> bool
        {
            if (bytes.size() != 8 || !out)
                return false;

            uint64_t value = 0;
            for (size_t i = 0; i < 8; ++i)
            {
                value = (value << 8) | bytes[i];
            }
            *out = value;
            return true;
        };

        for (uint16_t i = 0; i < page->brin_count; ++i)
        {
            SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);

            // Check MGA visibility
            if (!isRangeVisible(range->brn_xmin, range->brn_xmax, current_xid, txn_mgr))
            {
                // Skip invisible range
                size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
                range_ptr += range_size;
                continue;
            }

            // Extract min/max values
            uint8_t *min_ptr = range_ptr + sizeof(SBBrinRange);
            uint8_t *max_ptr = min_ptr + range->brn_min_len;

            std::vector<uint8_t> range_min(min_ptr, min_ptr + range->brn_min_len);
            std::vector<uint8_t> range_max(max_ptr, max_ptr + range->brn_max_len);

            // Check if range overlaps with query
            if (BrinMinmaxOps::rangeOverlaps(range_min, range_max, min_value, max_value))
            {
                // Add all blocks in this range, with optional numeric pruning for uint64 BE values.
                // This is a heuristic for monotonic time-series ranges to avoid returning
                // known out-of-bound blocks in a partially overlapping range.
                uint32_t start_block = range->brn_start_block;
                uint32_t end_block = range->brn_end_block;

                uint64_t range_min_val = 0;
                uint64_t range_max_val = 0;
                uint64_t query_min_val = 0;
                uint64_t query_max_val = 0;

                const bool range_decodable =
                    decode_uint64_be(range_min, &range_min_val) &&
                    decode_uint64_be(range_max, &range_max_val) &&
                    range_max_val > range_min_val;

                if (range_decodable)
                {
                    uint32_t pruned_start = start_block;
                    uint32_t pruned_end = end_block;
                    bool pruned = false;
                    const uint64_t span = range_max_val - range_min_val;
                    const uint64_t steps = static_cast<uint64_t>(range->brn_end_block - range->brn_start_block);

                    if (min_value && decode_uint64_be(*min_value, &query_min_val) &&
                        query_min_val > range_min_val && query_min_val <= range_max_val && steps > 0)
                    {
                        const uint64_t numer = query_min_val - range_min_val;
                        const uint64_t offset = (numer * steps + span - 1) / span; // ceil
                        const uint64_t candidate = static_cast<uint64_t>(range->brn_start_block) + offset;
                        if (candidate > pruned_start)
                        {
                            pruned_start = static_cast<uint32_t>(candidate);
                            pruned = true;
                        }
                    }

                    if (max_value && decode_uint64_be(*max_value, &query_max_val) &&
                        query_max_val >= range_min_val && query_max_val < range_max_val && steps > 0)
                    {
                        const uint64_t numer = query_max_val - range_min_val;
                        const uint64_t offset = (numer * steps) / span; // floor
                        const uint64_t candidate = static_cast<uint64_t>(range->brn_start_block) + offset;
                        if (candidate < pruned_end)
                        {
                            pruned_end = static_cast<uint32_t>(candidate);
                            pruned = true;
                        }
                    }

                    // Only apply pruning if it doesn't eliminate the whole range.
                    if (pruned && pruned_start <= pruned_end)
                    {
                        start_block = pruned_start;
                        end_block = pruned_end;
                    }
                }

                if (start_block <= end_block)
                {
                    for (uint32_t block = start_block; block <= end_block; ++block)
                    {
                        block_numbers_out->push_back(block);
                    }
                }

                LOG_DEBUG(GENERAL, "BRIN: Range [%u-%u] matched query",
                         range->brn_start_block, range->brn_end_block);
            }

            // Move to next range
            size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
            range_ptr += range_size;
        }

        total_ranges += page->brin_count;

        // Move to next page
        uint64_t next_page = page->brin_right_sibling;
        unpinIndexPage(current_page_num, false, ctx);

        if (next_page == 0)
            break;

        current_page_num = static_cast<uint32_t>(next_page);
    }

    LOG_INFO(GENERAL, "BRIN scan: returned %zu blocks from %lu ranges",
             block_numbers_out->size(), total_ranges);

    return Status::OK;
}

Status BrinIndex::remove(const std::vector<uint8_t> &value,
                        uint32_t block_number,
                        ErrorContext *ctx)
{
    // BRIN doesn't track individual values, only range summaries
    // Deletion requires rescan of the block range to recompute min/max
    // For now, just mark this as needing summarization

    LOG_DEBUG(GENERAL, "BRIN: Remove called for block %u (range rescan needed)",
             block_number);

    return Status::OK;
}

Status BrinIndex::gcCompact(GcCompactionStats *stats_out, ErrorContext *ctx)
{
    if (!db_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No database");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t ranges_visited = 0;
    uint64_t ranges_removed = 0;

    // Get oldest active transaction
    uint64_t oldest_xid = txn_mgr->getOldestActiveXid();

    // Pin root page
    void *page_buffer = nullptr;
    Status status = pinIndexPage(index_info_.idx_root_page, &page_buffer, ctx,
                                 BufferPool::AccessStrategy::Vacuum);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

    // Scan ranges and remove dead ones
    uint8_t *range_ptr = page_data + sizeof(SBBrinPage);
    std::vector<size_t> dead_ranges;

    for (uint16_t i = 0; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        ranges_visited++;

        // Check if range is dead (xmax set and committed before oldest active)
        if (range->brn_xmax != 0 && range->brn_xmax < oldest_xid)
        {
            dead_ranges.push_back(i);
            ranges_removed++;
        }

        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        range_ptr += range_size;
    }

    // Remove dead ranges (compact page)
    if (!dead_ranges.empty())
    {
        // Compact the page by removing dead ranges
        // We'll rebuild the page with only live ranges

        std::vector<uint8_t> live_ranges_data;
        uint8_t *read_ptr = page_data + sizeof(SBBrinPage);

        for (uint16_t i = 0; i < page->brin_count; ++i)
        {
            SBBrinRange *range = reinterpret_cast<SBBrinRange*>(read_ptr);
            size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;

            // Check if this range is dead
            bool is_dead = std::find(dead_ranges.begin(), dead_ranges.end(), i) != dead_ranges.end();

            if (!is_dead)
            {
                // Keep this range - copy to temporary buffer
                live_ranges_data.insert(live_ranges_data.end(),
                                       read_ptr,
                                       read_ptr + range_size);
            }
            else
            {
                // Remove from revmap
                revmap_remove(range->brn_start_block);
            }

            read_ptr += range_size;
        }

        // Now rewrite the page with only live ranges
        uint8_t *write_ptr = page_data + sizeof(SBBrinPage);
        if (!live_ranges_data.empty())
        {
            std::memcpy(write_ptr, live_ranges_data.data(), live_ranges_data.size());
        }

        // Update page metadata
        uint16_t old_count = page->brin_count;
        page->brin_count = old_count - static_cast<uint16_t>(dead_ranges.size());
        page->brin_ranges_deleted += dead_ranges.size();

        // Recalculate free space
        size_t used_space = sizeof(SBBrinPage) + live_ranges_data.size();
        page->brin_free_space = db_->page_size() - used_space;

        LOG_DEBUG(GENERAL, "BRIN GC compaction: compacted page, removed %zu ranges, reclaimed %zu bytes",
                 dead_ranges.size(),
                 dead_ranges.size() > 0 ? (db_->page_size() - used_space) - page->brin_free_space : 0);
    }

    unpinIndexPage(index_info_.idx_root_page, ranges_removed > 0, ctx);

    if (stats_out)
    {
        stats_out->ranges_visited = ranges_visited;
        stats_out->ranges_removed = ranges_removed;
        stats_out->ranges_updated = 0;
        stats_out->bytes_reclaimed = ranges_removed > 0 ?
            (ranges_removed * (sizeof(SBBrinRange) + 32)) : 0; // Estimate 32 bytes per min/max
    }

    LOG_INFO(GENERAL, "BRIN GC compaction: visited %lu ranges, removed %lu",
             ranges_visited, ranges_removed);

    return Status::OK;
}

Status BrinIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                   uint64_t *entries_removed_out,
                                   uint64_t *pages_modified_out,
                                   ErrorContext *ctx)
{
    // MGA hardening: summary indexes must not blindly tombstone a range when only a
    // subset of heap blocks are dead. Until heap resummarization is added here, the
    // bounded safe behavior is:
    // - remove a range only when every block in that range is proven dead
    // - leave partially-dead ranges in place for later resummarization / rebuild work

    // Empty dead_tids is a no-op
    if (dead_tids.empty())
    {
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;
        return Status::OK;
    }

    // Step 1: Convert dead_tids into block numbers
    std::set<uint32_t> dead_blocks;
    for (const TID &tid : dead_tids)
    {
        uint64_t page_num = getPageNumber(tid);
        if (page_num <= UINT32_MAX)
        {
            dead_blocks.insert(static_cast<uint32_t>(page_num));
        }
    }

    if (dead_blocks.empty())
    {
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;
        return Status::OK;
    }

    // Get components
    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t current_xid = txn_mgr->getCurrentXid();
    uint64_t ranges_removed = 0;
    uint64_t pages_modified = 0;

    // Step 2: Traverse all BRIN pages and mark fully-dead ranges as DELETED
    uint32_t current_page_id = index_info_.idx_root_page;

    while (current_page_id != 0)
    {
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page_id, &page_buffer, ctx,
                                     BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            LOG_WARNING(GENERAL, "BRIN GC: Failed to pin page %u", current_page_id);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page during GC");
            return Status::IO_ERROR;
        }

        SBBrinPage *page = static_cast<SBBrinPage*>(page_buffer);
        uint8_t *page_data = reinterpret_cast<uint8_t*>(page_buffer) + sizeof(SBBrinPage);
        uint32_t offset = 0;
        bool page_modified = false;

        // For each range in this page
        for (uint16_t i = 0; i < page->brin_count; i++)
        {
            SBBrinRange *range = reinterpret_cast<SBBrinRange*>(page_data + offset);

            bool range_fully_dead = false;
            uint32_t range_start = range->brn_start_block;
            uint32_t range_end = range->brn_end_block;
            if (range_end >= range_start)
            {
                range_fully_dead = true;
                for (uint32_t block = range_start; block <= range_end; ++block)
                {
                    if (dead_blocks.find(block) == dead_blocks.end())
                    {
                        range_fully_dead = false;
                        break;
                    }

                    if (block == std::numeric_limits<uint32_t>::max())
                    {
                        break;
                    }
                }
            }

            if (range_fully_dead && range->brn_xmax == 0)
            {
                range->brn_xmax = current_xid;
                range->brn_flags |= static_cast<uint16_t>(BrinRangeFlags::DELETED);

                ranges_removed++;
                page_modified = true;
            }

            // Calculate size of this range and advance offset
            size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
            offset += range_size;
        }

        // Get next page in chain
        uint32_t next_page_id = static_cast<uint32_t>(page->brin_right_sibling);

        // Unpin page (mark dirty if modified)
        if (page_modified)
        {
            pages_modified++;
        }
        unpinIndexPage(current_page_id, page_modified, ctx);

        // Move to next page
        current_page_id = next_page_id;
    }

    // Set output counters
    if (entries_removed_out) *entries_removed_out = ranges_removed;
    if (pages_modified_out) *pages_modified_out = pages_modified;

    LOG_DEBUG(GENERAL, "BRIN GC: Marked %lu ranges deleted across %lu pages (conservative approach without heap rescan)",
             ranges_removed, pages_modified);

    return Status::OK;
}

Status BrinIndex::updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                           uint64_t *ranges_updated_out,
                                           uint64_t *pages_modified_out,
                                           ErrorContext *ctx)
{
    if (ranges_updated_out)
    {
        *ranges_updated_out = 0;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = 0;
    }

    if (tid_mapping.empty() || index_info_.idx_root_page == 0)
    {
        return Status::OK;
    }

    std::unordered_map<GPID, GPID> gpid_mapping;
    gpid_mapping.reserve(tid_mapping.size());
    for (const auto &pair : tid_mapping)
    {
        gpid_mapping.emplace(pair.first.gpid, pair.second.gpid);
    }

    uint16_t source_tablespace_id = getTablespaceID(tid_mapping.begin()->first.gpid);
    uint64_t ranges_updated = 0;
    uint64_t pages_modified = 0;

    uint32_t current_page_id = index_info_.idx_root_page;
    while (current_page_id != 0)
    {
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page_id, &page_buffer, ctx,
                                     BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK || !page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page during TID update");
            return Status::IO_ERROR;
        }

        auto *page = static_cast<SBBrinPage*>(page_buffer);
        uint8_t *page_data = reinterpret_cast<uint8_t*>(page_buffer) + sizeof(SBBrinPage);
        uint32_t offset = 0;
        bool page_dirty = false;

        for (uint16_t i = 0; i < page->brin_count; i++)
        {
            auto *range = reinterpret_cast<SBBrinRange*>(page_data + offset);

            bool range_dirty = false;
            GPID old_start_gpid = makeGPID(source_tablespace_id, range->brn_start_block);
            GPID old_end_gpid = makeGPID(source_tablespace_id, range->brn_end_block);

            auto start_it = gpid_mapping.find(old_start_gpid);
            if (start_it != gpid_mapping.end())
            {
                range->brn_start_block = static_cast<uint32_t>(getPageNumber(start_it->second));
                range_dirty = true;
            }

            auto end_it = gpid_mapping.find(old_end_gpid);
            if (end_it != gpid_mapping.end())
            {
                range->brn_end_block = static_cast<uint32_t>(getPageNumber(end_it->second));
                range_dirty = true;
            }

            if (range_dirty)
            {
                ++ranges_updated;
                page_dirty = true;
            }

            size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
            offset += range_size;
        }

        uint32_t next_page_id = static_cast<uint32_t>(page->brin_right_sibling);
        if (page_dirty)
        {
            ++pages_modified;
        }
        unpinIndexPage(current_page_id, page_dirty, ctx);

        current_page_id = next_page_id;
    }

    if (ranges_updated_out)
    {
        *ranges_updated_out = ranges_updated;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = pages_modified;
    }

    return Status::OK;
}

Status BrinIndex::getStats(BrinStats *stats_out, ErrorContext *ctx)
{
    if (!db_ || !stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    // Traverse all pages to collect statistics
    uint32_t current_page_num = index_info_.idx_root_page;
    uint64_t total_ranges = 0;
    uint64_t deleted_ranges = 0;
    uint64_t total_pages = 0;
    uint32_t min_block = UINT32_MAX;
    uint32_t max_block = 0;
    uint64_t total_blocks_in_ranges = 0;

    while (current_page_num != 0)
    {
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page_num, &page_buffer, ctx);
        if (status != Status::OK || !page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
            return Status::IO_ERROR;
        }

        uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
        SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

        total_pages++;
        total_ranges += page->brin_count;
        deleted_ranges += page->brin_ranges_deleted;

        // Scan ranges on this page
        uint8_t *range_ptr = page_data + sizeof(SBBrinPage);
        for (uint16_t i = 0; i < page->brin_count; ++i)
        {
            SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);

            // Track global block range
            if (range->brn_start_block < min_block)
                min_block = range->brn_start_block;
            if (range->brn_end_block > max_block)
                max_block = range->brn_end_block;

            // Count blocks covered by this range
            total_blocks_in_ranges += (range->brn_end_block - range->brn_start_block + 1);

            size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
            range_ptr += range_size;
        }

        // Move to next page
        uint64_t next_page = page->brin_right_sibling;
        unpinIndexPage(current_page_num, false, ctx);

        if (next_page == 0)
            break;

        current_page_num = static_cast<uint32_t>(next_page);
    }

    // Fill output statistics
    stats_out->total_ranges = total_ranges;
    stats_out->deleted_ranges = deleted_ranges;
    stats_out->total_pages = total_pages;
    stats_out->blocks_covered = (min_block <= max_block) ? (max_block - min_block + 1) : 0;

    // Calculate average range selectivity
    // Selectivity = (blocks in ranges) / (total blocks covered)
    // Higher selectivity means ranges are more densely packed (better index quality)
    if (stats_out->blocks_covered > 0)
    {
        stats_out->avg_range_selectivity =
            static_cast<double>(total_blocks_in_ranges) / static_cast<double>(stats_out->blocks_covered);
    }
    else
    {
        stats_out->avg_range_selectivity = 0.0;
    }

    return Status::OK;
}

// Static helper for visibility checking
static bool isRangeVisible(uint64_t xmin, uint64_t xmax,
                           uint64_t current_xid,
                           TransactionManager *txn_mgr)
{
    // Firebird MGA visibility rules
    if (xmin > current_xid)
    {
        return false;
    }

    if (xmax != 0 && xmax <= current_xid)
    {
        return false;
    }

    // Check transaction states via TIP
    if (!txn_mgr->isVersionVisible(xmin, current_xid))
    {
        return false;
    }

    if (xmax != 0 && txn_mgr->isVersionVisible(xmax, current_xid))
    {
        return false;
    }

    return true;
}

// =============================================================================
// Revmap Implementation (Phase 3)
// =============================================================================

Status BrinIndex::build_revmap(ErrorContext *ctx)
{
    if (!db_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No database");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    std::unique_lock lock(revmap_mutex_);
    revmap_.clear();

    // Traverse all pages and build revmap
    uint32_t current_page_num = index_info_.idx_root_page;
    uint64_t ranges_mapped = 0;

    while (current_page_num != 0)
    {
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page_num, &page_buffer, ctx);
        if (status != Status::OK || !page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page");
            return Status::IO_ERROR;
        }

        uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
        SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

        // Map all ranges on this page
        uint8_t *range_ptr = page_data + sizeof(SBBrinPage);
        for (uint16_t i = 0; i < page->brin_count; ++i)
        {
            SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
            revmap_[range->brn_start_block] = current_page_num;
            ranges_mapped++;

            size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
            range_ptr += range_size;
        }

        // Move to next page
        uint64_t next_page = page->brin_right_sibling;
        unpinIndexPage(current_page_num, false, ctx);

        if (next_page == 0)
            break;

        current_page_num = static_cast<uint32_t>(next_page);
    }

    LOG_INFO(GENERAL, "BRIN: Built revmap with %lu range entries", ranges_mapped);

    return Status::OK;
}

void BrinIndex::revmap_add(uint32_t range_start_block, uint32_t page_num)
{
    std::unique_lock lock(revmap_mutex_);
    revmap_[range_start_block] = page_num;
}

void BrinIndex::revmap_remove(uint32_t range_start_block)
{
    std::unique_lock lock(revmap_mutex_);
    revmap_.erase(range_start_block);
}

uint32_t BrinIndex::revmap_lookup(uint32_t range_start_block) const
{
    std::shared_lock lock(revmap_mutex_);
    auto it = revmap_.find(range_start_block);
    if (it != revmap_.end())
    {
        return it->second;
    }
    return 0; // Not found
}

// =============================================================================
// Page Split Implementation (Phase 2)
// =============================================================================

Status BrinIndex::split_page(uint64_t page_num, ErrorContext *ctx)
{
    if (!db_)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No database");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    PageManager *page_mgr = db_->page_manager();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !page_mgr || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t current_xid = txn_mgr->getCurrentXid();

    // Pin the page to split
    void *page_buffer = nullptr;
    Status status = pinIndexPage(static_cast<uint32_t>(page_num), &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin BRIN page for split");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBBrinPage *page = reinterpret_cast<SBBrinPage*>(page_data);

    // Allocate new sibling page
    uint32_t new_page_num = 0;
    GPID new_page_gpid = 0;
    status = page_mgr->allocatePageInTablespace(index_info_.idx_tablespace_id, &new_page_gpid, ctx);
    if (status != Status::OK)
    {
        unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to allocate new BRIN page");
        return Status::IO_ERROR;
    }
    new_page_num = static_cast<uint32_t>(getPageNumber(new_page_gpid));

    // Pin new page
    void *new_page_buffer = nullptr;
    status = pinIndexPage(new_page_num, &new_page_buffer, ctx);
    if (status != Status::OK || !new_page_buffer)
    {
        unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin new BRIN page");
        return Status::IO_ERROR;
    }

    uint8_t *new_page_data = static_cast<uint8_t*>(new_page_buffer);
    SBBrinPage *new_page = reinterpret_cast<SBBrinPage*>(new_page_data);

    // Initialize new page header (manual initialization)
    new_page->brin_header.magic = K_MAGIC_SBRD;
    new_page->brin_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
    new_page->brin_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BRIN_DATA);
    new_page->brin_header.page_size = db_->page_size();
    new_page->brin_header.page_id = new_page_num;
    new_page->brin_header.checksum = 0;
    new_page->brin_header.lsn = 0;
    new_page->brin_header.flags = 0;
    new_page->brin_header.generation = 0;
    pageSetLower(new_page->brin_header, sizeof(SBBrinPage));
    pageSetUpper(new_page->brin_header, db_->page_size());
    pageSetSpecial(new_page->brin_header, db_->page_size());

    // Initialize BRIN metadata
    std::memcpy(new_page->brin_index_uuid.bytes.data(), index_info_.idx_uuid.bytes.data(), 16);
    std::memcpy(new_page->brin_table_uuid.bytes.data(), index_info_.idx_table_uuid.bytes.data(), 16);
    new_page->brin_flags = 0;
    new_page->brin_count = 0;
    new_page->brin_free_space = db_->page_size() - sizeof(SBBrinPage);
    new_page->brin_range_size = page->brin_range_size;
    new_page->brin_first_block = 0;  // Will be updated
    new_page->brin_last_block = 0;   // Will be updated
    new_page->brin_left_sibling = page_num;
    new_page->brin_right_sibling = page->brin_right_sibling;
    new_page->brin_xmin = current_xid;
    new_page->brin_xmax = 0;
    new_page->brin_lsn = 0;
    new_page->brin_ranges_total = 0;
    new_page->brin_ranges_deleted = 0;
    std::memset(new_page->brin_padding, 0, sizeof(new_page->brin_padding));

    // Split ranges: Move upper half to new page
    uint16_t split_point = page->brin_count / 2;
    uint8_t *range_ptr = page_data + sizeof(SBBrinPage);
    uint8_t *new_range_ptr = new_page_data + sizeof(SBBrinPage);

    // Skip to split point
    for (uint16_t i = 0; i < split_point; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        range_ptr += range_size;
    }

    // Copy upper half to new page
    size_t bytes_to_move = 0;
    uint8_t *copy_start = range_ptr;
    uint32_t new_first_block = UINT32_MAX;
    uint32_t new_last_block = 0;

    for (uint16_t i = split_point; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;

        // Track block range
        if (range->brn_start_block < new_first_block)
            new_first_block = range->brn_start_block;
        if (range->brn_end_block > new_last_block)
            new_last_block = range->brn_end_block;

        bytes_to_move += range_size;
        range_ptr += range_size;
    }

    // Copy ranges to new page
    std::memcpy(new_range_ptr, copy_start, bytes_to_move);

    // Update revmap for moved ranges
    range_ptr = copy_start;
    for (uint16_t i = split_point; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        revmap_add(range->brn_start_block, new_page_num);
        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        range_ptr += range_size;
    }

    // Update new page metadata
    new_page->brin_count = page->brin_count - split_point;
    new_page->brin_free_space = db_->page_size() - sizeof(SBBrinPage) - bytes_to_move;
    new_page->brin_first_block = new_first_block;
    new_page->brin_last_block = new_last_block;
    new_page->brin_ranges_total = page->brin_count - split_point;

    // Update original page metadata
    page->brin_count = split_point;
    page->brin_free_space = db_->page_size() - sizeof(SBBrinPage) - (copy_start - (page_data + sizeof(SBBrinPage)));
    page->brin_right_sibling = new_page_num;

    // Update last_block for original page
    range_ptr = page_data + sizeof(SBBrinPage);
    uint32_t orig_last_block = 0;
    for (uint16_t i = 0; i < page->brin_count; ++i)
    {
        SBBrinRange *range = reinterpret_cast<SBBrinRange*>(range_ptr);
        if (range->brn_end_block > orig_last_block)
            orig_last_block = range->brn_end_block;
        size_t range_size = sizeof(SBBrinRange) + range->brn_min_len + range->brn_max_len;
        range_ptr += range_size;
    }
    page->brin_last_block = orig_last_block;

    // If original page had a right sibling, update its left pointer
    if (new_page->brin_right_sibling != 0)
    {
        void *right_buffer = nullptr;
        if (pinIndexPage(static_cast<uint32_t>(new_page->brin_right_sibling), &right_buffer, ctx) == Status::OK)
        {
            SBBrinPage *right_page = reinterpret_cast<SBBrinPage*>(right_buffer);
            right_page->brin_left_sibling = new_page_num;
            unpinIndexPage(static_cast<uint32_t>(new_page->brin_right_sibling), true, ctx);
        }
    }

    unpinIndexPage(new_page_num, true, ctx);
    unpinIndexPage(static_cast<uint32_t>(page_num), true, ctx);

    LOG_INFO(GENERAL, "BRIN: Split page %lu into %lu (ranges: %u / %u)",
             page_num, static_cast<uint64_t>(new_page_num), split_point, new_page->brin_count);

    return Status::OK;
}

} // namespace scratchbird::core
