/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/vacuum.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/garbage_collector.h" // Phase 4: TOAST GC
#include "scratchbird/core/gpid.h"
#include <chrono>
#include "scratchbird/core/logger.h"
#include <cstring>
#include <unordered_map>
#include <unordered_set> // Phase 4: TOAST GC

namespace scratchbird::core
{

    Vacuum::Vacuum(Database *db) : db_(db) {}

    Vacuum::~Vacuum() = default;

    auto Vacuum::getVacuumHorizon(uint64_t *horizon_out, ErrorContext *ctx) -> Status
    {
        // Get oldest XID that might still see a tuple
        // This is the minimum of all backend xmin values

        Status status = ProcArrayManager::getVacuumHorizon(horizon_out, ctx);
        if (status != Status::OK)
        {
            // If ProcArray not available, use a conservative horizon
            // (no vacuum - everything is potentially visible)
            *horizon_out = UINT64_MAX;
            return Status::OK;
        }

        return Status::OK;
    }

    auto Vacuum::vacuumTable(const ID &table_id, VacuumStats *stats_out, ErrorContext *ctx)
        -> Status
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        VacuumStats stats;

        // Get vacuum horizon
        uint64_t horizon;
        Status status = getVacuumHorizon(&horizon, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // If horizon is UINT64_MAX, skip vacuum (nothing can be cleaned)
        if (horizon == UINT64_MAX)
        {
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return Status::OK;
        }

        // Scan heap for dead tuples
        std::vector<uint64_t> dead_tids;
        status = scanHeapForDeadTuples(table_id, horizon, &dead_tids, &stats, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Group dead TIDs by page
        // TID format: (page_id << 32) | (item_id << 16)
        std::unordered_map<uint32_t, std::vector<uint16_t>> dead_by_page;
        for (uint64_t tid : dead_tids)
        {
            uint32_t page_id = static_cast<uint32_t>(tid >> 32);
            uint16_t item_id = static_cast<uint16_t>((tid >> 16) & 0xFFFF);
            dead_by_page[page_id].push_back(item_id);
        }

        // Remove dead tuples page by page
        for (const auto &[page_id, dead_items] : dead_by_page)
        {
            status = removeDeadTuplesFromPage(table_id, page_id, dead_items, &stats, ctx);
            if (status != Status::OK)
            {
                // Continue with other pages even if one fails
                continue;
            }

            // Prune version chains on this page
            status = pruneVersionChains(table_id, page_id, horizon, &stats, ctx);
            // Continue even if pruning fails
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        stats.vacuum_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }

        return Status::OK;
    }

    auto Vacuum::vacuumDatabase(VacuumStats *stats_out, ErrorContext *ctx) -> Status
    {
        VacuumStats total_stats;
        auto start_time = std::chrono::high_resolution_clock::now();

        // Get catalog manager
        auto *catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Get the default schema (public schema)
        // For simplicity, we'll vacuum all tables in all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog->listSchemas(schemas, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to list schemas for vacuum");
            return status;
        }

        // Iterate over all schemas
        for (const auto &schema : schemas)
        {
            // Get all tables in this schema
            std::vector<CatalogManager::TableInfo> tables;
            status = catalog->listTables(schema.schema_id, tables, ctx);
            if (status != Status::OK)
            {
                // Continue with other schemas even if one fails
                continue;
            }

            // Vacuum each table
            for (const auto &table : tables)
            {
                // Phase 4 Task 4.3: Process TOAST tables for garbage collection
                if (table.table_type == CatalogManager::TableType::TOAST)
                {
                    // TOAST table - run orphan cleanup and TIP-based GC
                    auto* gc = db_->garbage_collector();
                    if (gc)
                    {
                        // Step 1: Detect orphaned TOAST chunks
                        std::unordered_set<uint32_t> orphaned_value_ids;
                        Status orphan_status = gc->detectOrphanedToastChunks(table.table_id,
                                                                              &orphaned_value_ids,
                                                                              ctx);

                        if (orphan_status == Status::OK && !orphaned_value_ids.empty())
                        {
                            // Step 2: Clean orphaned chunks
                            uint64_t orphans_deleted = 0;
                            Status clean_status = gc->cleanOrphanedToastChunks(table.table_id,
                                                                               orphaned_value_ids,
                                                                               &orphans_deleted,
                                                                               ctx);

                            if (clean_status == Status::OK)
                            {
                                LOG_INFO(VACUUM, "TOAST table %s: cleaned %lu orphaned chunks",
                                        table.table_name.c_str(), orphans_deleted);
                            }
                        }

                        // Step 3: TIP-based garbage collection
                        uint64_t tip_deleted = 0;
                        Status tip_status = gc->cleanToastChunksByTIP(table.table_id,
                                                                       &tip_deleted,
                                                                       ctx);

                        if (tip_status == Status::OK && tip_deleted > 0)
                        {
                            LOG_INFO(VACUUM, "TOAST table %s: TIP-based GC deleted %lu chunks",
                                    table.table_name.c_str(), tip_deleted);
                        }
                    }

                    continue; // Don't process TOAST as regular table
                }

                // Regular table - vacuum normally
                VacuumStats table_stats;
                status = vacuumTable(table.table_id, &table_stats, ctx);
                if (status == Status::OK)
                {
                    // Accumulate statistics
                    total_stats.pages_scanned += table_stats.pages_scanned;
                    total_stats.tuples_scanned += table_stats.tuples_scanned;
                    total_stats.dead_tuples_found += table_stats.dead_tuples_found;
                    total_stats.dead_tuples_removed += table_stats.dead_tuples_removed;
                    total_stats.version_chains_pruned += table_stats.version_chains_pruned;
                    total_stats.pages_compacted += table_stats.pages_compacted;
                    total_stats.free_space_recovered += table_stats.free_space_recovered;
                }
                // Continue with other tables even if one fails
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        total_stats.vacuum_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

        if (stats_out != nullptr)
        {
            *stats_out = total_stats;
        }

        return Status::OK;
    }

    auto Vacuum::vacuumPage(const ID &table_id, uint32_t page_id, VacuumStats *stats_out,
                            ErrorContext *ctx) -> Status
    {
        VacuumStats stats;

        // Get vacuum horizon
        uint64_t horizon;
        Status status = getVacuumHorizon(&horizon, ctx);
        if (status != Status::OK || horizon == UINT64_MAX)
        {
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return Status::OK;
        }

        // Prune version chains first
        status = pruneVersionChains(table_id, page_id, horizon, &stats, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Compact page to reclaim space
        status = compactPage(page_id, &stats, ctx);

        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }

        return status;
    }

    auto Vacuum::scanHeapForDeadTuples(const ID &table_id, uint64_t horizon,
                                       std::vector<uint64_t> *dead_tids_out, VacuumStats *stats,
                                       ErrorContext *ctx) -> Status
    {
        // Get table information from catalog
        auto *catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Get table info to find root page
        CatalogManager::TableInfo table_info;
        Status status = catalog->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table info for vacuum");
            return status;
        }

        // Start from the root page (first heap page for this table)
        // We'll scan all pages until we hit an IO_ERROR (page doesn't exist)
        uint32_t start_page = static_cast<uint32_t>(getPageNumber(table_info.root_gpid));
        if (start_page == 0)
        {
            // Empty table
            return Status::OK;
        }

        // Scan all pages of this table
        // We scan sequentially from the root page until we encounter a non-existent page
        for (uint32_t page_id = start_page;; ++page_id)
        {
            void *page_buffer;
            Status status = db_->buffer_pool()->pinPage(
                page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
            if (status == Status::IO_ERROR)
            {
                // Page doesn't exist, we've reached the end
                break;
            }
            if (status != Status::OK)
            {
                continue;
            }

            auto *page_data = static_cast<uint8_t *>(page_buffer);
            auto *page_hdr = reinterpret_cast<PageHeader *>(page_data);

            // Check if this is a heap page
            if (page_hdr->page_type != PAGE_TYPE_HEAP)
            {
                db_->buffer_pool()->unpinPage(page_id, false, ctx);
                continue;
            }

            HeapPage heap(page_data, db_->page_size());
            stats->pages_scanned++;

            // Scan all tuples on page
            uint16_t item_count = heap.getItemCount();
            for (uint16_t item_id = 0; item_id < item_count; ++item_id)
            {
                const uint8_t *tuple_data;
                uint32_t tuple_size;
                status = heap.getTuple(item_id, &tuple_data, &tuple_size, ctx);
                if (status != Status::OK)
                {
                    continue; // Skip deleted/invalid tuples
                }

                stats->tuples_scanned++;

                // Check if dead
                if (isTupleDead(tuple_data, horizon))
                {
                    // Build TID: (page_id << 32) | (item_id << 16)
                    uint64_t tid = (static_cast<uint64_t>(page_id) << 32) |
                                   (static_cast<uint64_t>(item_id) << 16);
                    dead_tids_out->push_back(tid);
                    stats->dead_tuples_found++;
                }
            }

            db_->buffer_pool()->unpinPage(page_id, false, ctx);
        }

        return Status::OK;
    }

    auto Vacuum::pruneVersionChains(const ID &table_id, uint32_t page_id, uint64_t horizon,
                                    VacuumStats *stats, ErrorContext *ctx) -> Status
    {
        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(
            page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        HeapPage heap(page_data, db_->page_size());
        bool page_modified = false;

        // Scan all tuples looking for prunable versions
        uint16_t item_count = heap.getItemCount();
        for (uint16_t item_id = 0; item_id < item_count; ++item_id)
        {
            const uint8_t *tuple_data;
            uint32_t tuple_size;
            status = heap.getTuple(item_id, &tuple_data, &tuple_size, ctx);
            if (status != Status::OK)
            {
                continue;
            }

            // Check if this version is prunable
            if (isVersionPrunable(tuple_data, horizon))
            {
                // Mark tuple as deleted (prune it)
                // This is a simplified version - full implementation would
                // need to update version chain pointers
                auto *tuple_hdr =
                    const_cast<TupleHeader *>(reinterpret_cast<const TupleHeader *>(tuple_data));

                // Set xmax if not already set
                if (tuple_hdr->xmax == 0)
                {
                    tuple_hdr->xmax = horizon;
                }

                // Mark as prunable
                tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;

                page_modified = true;
                stats->version_chains_pruned++;
            }
        }

        db_->buffer_pool()->unpinPage(page_id, page_modified, ctx);
        return Status::OK;
    }

    auto Vacuum::removeDeadTuplesFromPage(const ID &table_id, uint32_t page_id,
                                          const std::vector<uint16_t> &dead_item_ids,
                                          VacuumStats *stats, ErrorContext *ctx) -> Status
    {
        if (dead_item_ids.empty())
        {
            return Status::OK;
        }

        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(
            page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        HeapPage heap(page_data, db_->page_size());

        // Delete each tuple
        for (uint16_t item_id : dead_item_ids)
        {
            // Use a dummy xmax (the tuple is already dead)
            status = heap.deleteTuple(item_id, UINT64_MAX, ctx);
            if (status == Status::OK)
            {
                stats->dead_tuples_removed++;
            }
        }

        db_->buffer_pool()->unpinPage(page_id, true, ctx);
        return Status::OK;
    }

    auto Vacuum::compactPage(uint32_t page_id, VacuumStats *stats, ErrorContext *ctx) -> Status
    {
        // Pin the page
        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(
            page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        uint32_t page_size = db_->page_size();

        // Create a temporary buffer for compaction
        std::vector<uint8_t> temp_buffer(page_size);
        uint8_t *temp_data = temp_buffer.data();

        // Copy page header
        auto *old_page_hdr = reinterpret_cast<PageHeader *>(page_data);
        auto *temp_page_hdr = reinterpret_cast<PageHeader *>(temp_data);
        std::memcpy(temp_page_hdr, old_page_hdr, sizeof(PageHeader));

        // Get item arrays (starts right after PageHeader)
        auto *old_items = reinterpret_cast<ItemPointer *>(page_data + sizeof(PageHeader));
        auto *new_items = reinterpret_cast<ItemPointer *>(temp_data + sizeof(PageHeader));

        uint16_t old_item_count = old_page_hdr->item_count;
        uint16_t new_item_count = 0;

        // Calculate starting position for tuple data (from end of page, before special area)
        uint32_t tuple_data_offset = page_size - sizeof(HeapPageSpecial);

        // Track space reclaimed
        uint32_t old_free_space = old_page_hdr->free_space;

        // Pass 1: Copy non-deleted tuples and build new item array
        for (uint16_t i = 0; i < old_item_count; ++i)
        {
            const ItemPointer &old_item = old_items[i];

            // Skip deleted/unused items
            if (old_item.isDeleted() || old_item.offset == 0)
            {
                continue;
            }

            // Get tuple data
            const uint8_t *tuple_src = page_data + old_item.offset;
            uint32_t tuple_size = old_item.length;

            // Allocate space from end of page (before special area)
            tuple_data_offset -= tuple_size;

            // Copy tuple data to new location
            std::memcpy(temp_data + tuple_data_offset, tuple_src, tuple_size);

            // Create new item pointing to compacted location
            new_items[new_item_count].offset = tuple_data_offset;
            new_items[new_item_count].length = tuple_size;
            new_items[new_item_count].flags = old_item.flags;

            new_item_count++;
        }

        // Update header with new counts and free space
        temp_page_hdr->item_count = new_item_count;

        // Calculate new free space
        uint32_t items_size = new_item_count * sizeof(ItemPointer);
        uint32_t header_size = sizeof(PageHeader);
        uint32_t used_space =
            header_size + items_size + (page_size - sizeof(HeapPageSpecial) - tuple_data_offset);
        uint32_t new_free_space = page_size - sizeof(HeapPageSpecial) - used_space;

        temp_page_hdr->free_space = static_cast<uint16_t>(new_free_space);

        // Copy special area
        auto *old_special =
            reinterpret_cast<HeapPageSpecial *>(page_data + page_size - sizeof(HeapPageSpecial));
        auto *temp_special =
            reinterpret_cast<HeapPageSpecial *>(temp_data + page_size - sizeof(HeapPageSpecial));
        std::memcpy(temp_special, old_special, sizeof(HeapPageSpecial));

        // Update statistics
        stats->pages_compacted++;
        if (new_free_space > old_free_space)
        {
            stats->free_space_recovered += (new_free_space - old_free_space);
        }

        // Copy compacted page back to original buffer
        std::memcpy(page_data, temp_data, page_size);

        // Unpin page as dirty
        db_->buffer_pool()->unpinPage(page_id, true, ctx);

        return Status::OK;
    }

    bool Vacuum::isTupleDead(const uint8_t *tuple_data, uint64_t horizon) const
    {
        auto *header = reinterpret_cast<const TupleHeader *>(tuple_data);

        // Tuple is dead if:
        // 1. xmax is set (tuple was deleted or updated)
        // 2. xmax < horizon (delete/update is committed and visible to all)
        // 3. Not the head of a version chain (has no next version or is updated)

        if (header->xmax == 0)
        {
            return false; // Still alive
        }

        if (header->xmax >= horizon)
        {
            return false; // Delete not visible to all transactions yet
        }

        // xmax < horizon means all active transactions can see the delete
        // However, if this tuple was updated (not deleted), we need to check
        // if it's part of a version chain

        if ((header->infomask & TupleHeader::HEAP_UPDATED) != 0)
        {
            // Tuple was updated, not deleted
            // It's dead only if no transaction needs it for version chain traversal
            // For simplicity, if it has a next version and xmax < horizon, it's dead
            return header->hasNextVersion();
        }

        // Tuple was deleted (not updated)
        return (header->infomask & TupleHeader::HEAP_XMAX_COMMITTED) != 0;
    }

    bool Vacuum::isVersionPrunable(const uint8_t *tuple_data, uint64_t horizon) const
    {
        auto *header = reinterpret_cast<const TupleHeader *>(tuple_data);

        // A version is prunable if:
        // 1. It was updated (not the latest version)
        // 2. The update is committed and visible to all transactions
        // 3. No active transaction might traverse to this version

        if ((header->infomask & TupleHeader::HEAP_UPDATED) == 0)
        {
            return false; // Not updated, can't prune
        }

        if (!header->hasNextVersion())
        {
            return false; // No next version, this is the latest
        }

        if (header->xmax == 0 || header->xmax >= horizon)
        {
            return false; // Update not visible to all yet
        }

        // Update is committed and old enough to be invisible to all transactions
        return true;
    }

    auto Vacuum::freezeTable(const ID &table_id, uint64_t freeze_limit, VacuumStats *stats_out,
                             ErrorContext *ctx) -> Status
    {
        // Freeze tuples with xmin < freeze_limit to prevent XID wraparound
        VacuumStats stats;

        // Get table metadata from catalog
        CatalogManager *catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::TableInfo table_info;
        Status status = catalog->getTable(table_id, table_info, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        uint32_t start_page = static_cast<uint32_t>(getPageNumber(table_info.root_gpid));
        if (start_page == 0)
        {
            // Empty table
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return Status::OK;
        }

        // Scan all pages in the table
        BufferPool *bp = db_->buffer_pool();
        if (bp == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool not available");
            return Status::INVALID_ARGUMENT;
        }

        // Scan sequentially from root page until IO_ERROR
        for (uint32_t page_id = start_page;; ++page_id)
        {
            // Pin the page
            void *page_buffer;
            status = bp->pinPage(
                page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
            if (status == Status::IO_ERROR)
            {
                // Page doesn't exist, we've reached the end
                break;
            }
            if (status != Status::OK)
            {
                // Continue with next page
                continue;
            }

            auto *page_data = static_cast<uint8_t *>(page_buffer);
            auto *page_hdr = reinterpret_cast<PageHeader *>(page_data);

            // Check if this is a heap page
            if (page_hdr->page_type != PAGE_TYPE_HEAP)
            {
                bp->unpinPage(page_id, false, ctx);
                continue;
            }

            stats.pages_scanned++;

            HeapPage heap_page(page_data, db_->page_size());

            // Freeze tuples on this page
            uint32_t frozen_count = 0;
            status = heap_page.freezeTuples(freeze_limit, &frozen_count, ctx);

            bool page_modified = (frozen_count > 0);
            stats.tuples_frozen += frozen_count;

            // Unpin with dirty flag if we modified the page
            bp->unpinPage(page_id, page_modified, ctx);

            if (status != Status::OK)
            {
                // Continue with next page even if freeze failed
                continue;
            }
        }

        // Advance oldest_xid to freeze_limit after successful freeze
        // This allows old XIDs to be reclaimed
        if (stats.tuples_frozen > 0)
        {
            TransactionManager *txn_mgr = db_->transaction_manager();
            if (txn_mgr != nullptr)
            {
                status = txn_mgr->setOldestXid(freeze_limit, ctx);
                // Continue even if this fails - freezing already succeeded
            }
        }

        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }

        return Status::OK;
    }

} // namespace scratchbird::core
