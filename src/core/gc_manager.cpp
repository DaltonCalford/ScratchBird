/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/gc_manager.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/garbage_collector.h" // Phase 4: TOAST GC
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/uuidv7.h"
#include <chrono>
#include "scratchbird/core/logger.h"
#include <cstring>
#include <unordered_map>
#include <unordered_set> // Phase 4: TOAST GC

namespace scratchbird::core
{

    GcManager::GcManager(Database *db) : db_(db) {}

    GcManager::~GcManager() = default;

    auto GcManager::getGcHorizon(uint64_t *horizon_out, ErrorContext *ctx) -> Status
    {
        if (horizon_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "horizon_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        TransactionManager *txn_manager = (db_ != nullptr) ? db_->transaction_manager() : nullptr;
        if (txn_manager == nullptr)
        {
            *horizon_out = UINT64_MAX;
            SET_ERROR_CONTEXT(ctx,
                              Status::INVALID_ARGUMENT,
                              "GC_HORIZON_UNCERTAIN: transaction manager not available");
            return Status::OK;
        }

        ReclaimHorizonSnapshot horizons{};
        Status status = txn_manager->captureReclaimHorizons(horizons, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        *horizon_out = horizons.heap_reclaim_horizon;
        return Status::OK;
    }

    auto GcManager::gcTable(const ID &table_id, GcStats *stats_out, ErrorContext *ctx)
        -> Status
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        GcStats stats;

        // Get GC horizon
        uint64_t horizon;
        Status status = getGcHorizon(&horizon, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // If horizon is UINT64_MAX, skip GC (nothing can be cleaned)
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
        stats.gc_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }

        return Status::OK;
    }

    auto GcManager::gcDatabase(GcStats *stats_out, ErrorContext *ctx) -> Status
    {
        GcStats total_stats;
        auto start_time = std::chrono::high_resolution_clock::now();

        // Get catalog manager
        auto *catalog = db_->catalog_manager();
        if (catalog == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        // Get the default schema (public schema)
        // For simplicity, we'll GC all tables in all schemas
        std::vector<CatalogManager::SchemaInfo> schemas;
        Status status = catalog->listSchemas(schemas, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to list schemas for GC");
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

            // GC each table
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
                        std::unordered_set<ID, IDHash> orphaned_value_ids;
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

                // Regular table - GC normally
                GcStats table_stats;
                status = gcTable(table.table_id, &table_stats, ctx);
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
                    total_stats.pages_dead_space_warn += table_stats.pages_dead_space_warn;
                    total_stats.pages_dead_space_compact += table_stats.pages_dead_space_compact;
                    total_stats.pages_dead_space_rewrite += table_stats.pages_dead_space_rewrite;
                    total_stats.rewrite_recommendations += table_stats.rewrite_recommendations;
                    total_stats.slot_stable_compactions += table_stats.slot_stable_compactions;
                }
                // Continue with other tables even if one fails
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        total_stats.gc_time_us =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();

        if (stats_out != nullptr)
        {
            *stats_out = total_stats;
        }

        return Status::OK;
    }

    auto GcManager::gcPage(const ID &table_id, uint32_t page_id, GcStats *stats_out,
                            ErrorContext *ctx) -> Status
    {
        GcStats stats;

        // Get GC horizon
        uint64_t horizon;
        Status status = getGcHorizon(&horizon, ctx);
        if (status != Status::OK || horizon == UINT64_MAX)
        {
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return Status::OK;
        }

        void *page_buffer;
        status = db_->buffer_pool()->pinPage(
            page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        auto *page_hdr = reinterpret_cast<PageHeader *>(page_data);
        if (page_hdr->page_type != PAGE_TYPE_HEAP)
        {
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return Status::OK;
        }

        const auto *special = reinterpret_cast<const HeapPageSpecial *>(
            page_data + db_->page_size() - sizeof(HeapPageSpecial));
        const ID page_table_id = (special != nullptr) ? special->table_id : table_id;
        HeapPage heap(page_data, db_->page_size(), nullptr, db_, page_table_id);
        HeapPage::VersionChainAuditResult chain_audit{};
        status = heap.auditVersionChainMetadata(
            HeapPage::VersionChainAuditMode::READ_ONLY, &chain_audit, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return status;
        }
        if (chain_audit.cleanup_blocked)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, chain_audit.summary.c_str());
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return Status::DATA_CORRUPTED;
        }

        stats.pages_scanned++;
        std::vector<uint16_t> dead_items;
        uint32_t reclaimable_bytes = 0;
        uint16_t chain_depth_hint = 0;
        uint64_t oldest_interesting_txid = 0;
        const uint16_t item_count = heap.getItemCount();
        for (uint16_t item_id = 0; item_id < item_count; ++item_id)
        {
            const uint8_t *tuple_data = nullptr;
            uint32_t tuple_size = 0;
            Status tuple_status = heap.getTuple(item_id, &tuple_data, &tuple_size, ctx);
            if (tuple_status != Status::OK)
            {
                continue;
            }

            ++stats.tuples_scanned;
            const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
            if (tuple_hdr->hasBackVersion())
            {
                ++chain_depth_hint;
            }
            if (tuple_hdr->xmin != 0 &&
                (oldest_interesting_txid == 0 || tuple_hdr->xmin < oldest_interesting_txid))
            {
                oldest_interesting_txid = tuple_hdr->xmin;
            }
            if (tuple_hdr->xmax != 0 &&
                (oldest_interesting_txid == 0 || tuple_hdr->xmax < oldest_interesting_txid))
            {
                oldest_interesting_txid = tuple_hdr->xmax;
            }
        }

        HeapPage::VersionMaturityScan maturity_scan{};
        status = heap.scanVersionMaturity(horizon, &maturity_scan, &dead_items, nullptr, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            if (stats_out != nullptr)
            {
                *stats_out = stats;
            }
            return status;
        }
        reclaimable_bytes = maturity_scan.reclaimable_bytes;
        stats.dead_tuples_found += maturity_scan.reclaimable_item_count;

        BufferPool::MgaFrameHints hints{};
        hints.page_class = !dead_items.empty()
                               ? BufferPool::MgaPageClass::GC_CANDIDATE
                               : (chain_depth_hint >= 4 ? BufferPool::MgaPageClass::CHAIN_HEAVY
                                                        : BufferPool::MgaPageClass::VERSION_ROOT);
        hints.oldest_interesting_txid = oldest_interesting_txid;
        hints.prune_safe_horizon_hint = horizon;
        hints.dead_version_bytes = reclaimable_bytes;
        hints.chain_depth_hint = chain_depth_hint;
        (void)db_->buffer_pool()->publishMgaFrameHintsGlobal(convertPageIDtoGPID(page_id),
                                                             hints,
                                                             nullptr);

        db_->buffer_pool()->unpinPage(page_id, false, ctx);

        if (!dead_items.empty())
        {
            status = removeDeadTuplesFromPage(page_table_id, page_id, dead_items, &stats, ctx);
            if (status != Status::OK)
            {
                if (stats_out != nullptr)
                {
                    *stats_out = stats;
                }
                return status;
            }
        }

        status = pruneVersionChains(page_table_id, page_id, horizon, &stats, ctx);

        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }

        return status;
    }

    auto GcManager::scanHeapForDeadTuples(const ID &table_id, uint64_t horizon,
                                       std::vector<uint64_t> *dead_tids_out, GcStats *stats,
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
            SET_ERROR_CONTEXT(ctx, status, "Failed to get table info for GC");
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

            const auto *special = reinterpret_cast<const HeapPageSpecial *>(
                page_data + db_->page_size() - sizeof(HeapPageSpecial));
            const ID page_table_id = (special != nullptr) ? special->table_id : table_id;
            HeapPage heap(page_data, db_->page_size(), nullptr, db_, page_table_id);
            HeapPage::VersionChainAuditResult chain_audit{};
            Status audit_status = heap.auditVersionChainMetadata(
                HeapPage::VersionChainAuditMode::READ_ONLY, &chain_audit, ctx);
            if (audit_status != Status::OK || chain_audit.cleanup_blocked)
            {
                if (audit_status == Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, chain_audit.summary.c_str());
                }
                db_->buffer_pool()->unpinPage(page_id, false, ctx);
                continue;
            }
            stats->pages_scanned++;
            uint32_t reclaimable_bytes = 0;
            uint16_t chain_depth_hint = 0;
            uint64_t oldest_interesting_txid = 0;

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
                const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
                if (tuple_hdr->hasBackVersion())
                {
                    ++chain_depth_hint;
                }
                if (tuple_hdr->xmin != 0 &&
                    (oldest_interesting_txid == 0 || tuple_hdr->xmin < oldest_interesting_txid))
                {
                    oldest_interesting_txid = tuple_hdr->xmin;
                }
                if (tuple_hdr->xmax != 0 &&
                    (oldest_interesting_txid == 0 || tuple_hdr->xmax < oldest_interesting_txid))
                {
                    oldest_interesting_txid = tuple_hdr->xmax;
                }

            }

            HeapPage::VersionMaturityScan maturity_scan{};
            std::vector<uint16_t> reclaimable_item_ids;
            Status maturity_status = heap.scanVersionMaturity(horizon,
                                                              &maturity_scan,
                                                              &reclaimable_item_ids,
                                                              nullptr,
                                                              ctx);
            if (maturity_status == Status::OK)
            {
                reclaimable_bytes = maturity_scan.reclaimable_bytes;
                for (uint16_t item_id : reclaimable_item_ids)
                {
                    uint64_t tid = (static_cast<uint64_t>(page_id) << 32) |
                                   (static_cast<uint64_t>(item_id) << 16);
                    dead_tids_out->push_back(tid);
                }
                stats->dead_tuples_found += reclaimable_item_ids.size();
            }

            BufferPool::MgaFrameHints hints{};
            hints.page_class = (reclaimable_bytes > 0)
                                   ? BufferPool::MgaPageClass::GC_CANDIDATE
                                   : (chain_depth_hint >= 4
                                          ? BufferPool::MgaPageClass::CHAIN_HEAVY
                                          : BufferPool::MgaPageClass::VERSION_ROOT);
            hints.oldest_interesting_txid = oldest_interesting_txid;
            hints.prune_safe_horizon_hint = horizon;
            hints.dead_version_bytes = reclaimable_bytes;
            hints.chain_depth_hint = chain_depth_hint;
            (void)db_->buffer_pool()->publishMgaFrameHintsGlobal(convertPageIDtoGPID(page_id),
                                                                 hints,
                                                                 nullptr);

            db_->buffer_pool()->unpinPage(page_id, false, ctx);
        }

        return Status::OK;
    }

    auto GcManager::pruneVersionChains(const ID &table_id, uint32_t page_id, uint64_t horizon,
                                    GcStats *stats, ErrorContext *ctx) -> Status
    {
        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(
            page_id, &page_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        const auto *special = reinterpret_cast<const HeapPageSpecial *>(
            page_data + db_->page_size() - sizeof(HeapPageSpecial));
        const ID table_uuid = (special != nullptr) ? special->table_id : table_id;
        HeapPage heap(page_data, db_->page_size(), nullptr, db_, table_uuid);
        HeapPage::VersionChainAuditResult chain_audit{};
        Status audit_status = heap.auditVersionChainMetadata(
            HeapPage::VersionChainAuditMode::READ_ONLY, &chain_audit, ctx);
        if (audit_status != Status::OK)
        {
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            return audit_status;
        }
        if (chain_audit.cleanup_blocked)
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, chain_audit.summary.c_str());
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            return Status::DATA_CORRUPTED;
        }

        // Scan all tuples looking for reclaimable historical versions.
        uint16_t item_count = heap.getItemCount();
        uint16_t chain_depth_hint = 0;
        for (uint16_t item_id = 0; item_id < item_count; ++item_id)
        {
            const uint8_t *tuple_data;
            uint32_t tuple_size;
            status = heap.getTuple(item_id, &tuple_data, &tuple_size, ctx);
            if (status != Status::OK)
            {
                continue;
            }

            const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
            if (tuple_hdr->hasBackVersion())
            {
                ++chain_depth_hint;
            }

        }

        HeapPage::VersionMaturityScan maturity_scan{};
        Status maturity_status = heap.scanVersionMaturity(horizon, &maturity_scan, nullptr, nullptr, ctx);
        if (maturity_status != Status::OK)
        {
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            return maturity_status;
        }
        stats->version_chains_pruned += maturity_scan.prune_only_item_count;

        BufferPool::MgaFrameHints hints{};
        hints.page_class = (chain_depth_hint >= 4) ? BufferPool::MgaPageClass::CHAIN_HEAVY
                                                   : BufferPool::MgaPageClass::VERSION_ROOT;
        hints.prune_safe_horizon_hint = horizon;
        hints.chain_depth_hint = chain_depth_hint;
        (void)db_->buffer_pool()->publishMgaFrameHintsGlobal(convertPageIDtoGPID(page_id),
                                                             hints,
                                                             nullptr);

        db_->buffer_pool()->unpinPage(page_id, false, ctx);
        return Status::OK;
    }

    auto GcManager::removeDeadTuplesFromPage(const ID &table_id, uint32_t page_id,
                                          const std::vector<uint16_t> &dead_item_ids,
                                          GcStats *stats, ErrorContext *ctx) -> Status
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

        const uint64_t removed_before = stats->dead_tuples_removed;
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
        if (stats->dead_tuples_removed > removed_before)
        {
            Status compact_status = compactPage(table_id, page_id, stats, ctx);
            if (compact_status != Status::OK)
            {
                return compact_status;
            }
        }
        return Status::OK;
    }

    auto GcManager::compactPage(const ID &table_id, uint32_t page_id, GcStats *stats,
                                ErrorContext *ctx) -> Status
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
        HeapPage heap(page_data, db_->page_size(), nullptr, db_, table_id);
        HeapPage::FragmentationMetrics metrics{};
        status = heap.analyzeFragmentation(&metrics, ctx);
        if (status != Status::OK)
        {
            db_->buffer_pool()->unpinPage(page_id, false, ctx);
            return status;
        }

        if (metrics.warn_threshold)
        {
            ++stats->pages_dead_space_warn;
        }
        if (metrics.compact_threshold)
        {
            ++stats->pages_dead_space_compact;
        }
        if (metrics.rewrite_threshold)
        {
            ++stats->pages_dead_space_rewrite;
            ++stats->rewrite_recommendations;
        }

        StorageEngine::FragmentationAdvisory advisory{};
        advisory.page_id = page_id;
        advisory.live_tuple_bytes = metrics.live_tuple_bytes;
        advisory.reclaimable_bytes = metrics.reclaimable_bytes;
        advisory.free_bytes = metrics.free_bytes;
        advisory.live_slots = metrics.live_slots;
        advisory.deleted_slots = metrics.deleted_slots;
        advisory.unused_slots = metrics.unused_slots;
        advisory.chain_depth_hint = metrics.chain_depth_hint;
        advisory.same_page_back_versions = metrics.same_page_back_versions;
        advisory.same_page_update_ratio = metrics.same_page_update_ratio;
        advisory.dead_space_ratio = metrics.dead_space_ratio;
        advisory.warn_threshold = metrics.warn_threshold;
        advisory.compact_threshold = metrics.compact_threshold;
        advisory.rewrite_recommended = metrics.rewrite_threshold;

        auto *storage_engine = db_->storage_engine();
        if (!metrics.warn_threshold && storage_engine != nullptr)
        {
            storage_engine->clearFragmentationAdvisory(table_id, page_id);
        }

        bool page_dirty = false;
        if (metrics.compact_threshold)
        {
            ItemPointer *items = heap.header() != nullptr
                                     ? reinterpret_cast<ItemPointer *>(page_data + sizeof(PageHeader))
                                     : nullptr;
            const uint16_t item_count = heap.getItemCount();
            for (uint16_t item_id = 0; item_id < item_count; ++item_id)
            {
                if (items[item_id].isDeleted())
                {
                    Status mark_status = heap.markTupleUnused(item_id, ctx);
                    if (mark_status != Status::OK)
                    {
                        db_->buffer_pool()->unpinPage(page_id, page_dirty, ctx);
                        return mark_status;
                    }
                    page_dirty = true;
                }
            }

            uint32_t bytes_reclaimed = 0;
            status = heap.defragmentPage(&bytes_reclaimed, ctx);
            if (status != Status::OK)
            {
                db_->buffer_pool()->unpinPage(page_id, page_dirty, ctx);
                return status;
            }

            advisory.compaction_applied = true;
            ++stats->pages_compacted;
            ++stats->slot_stable_compactions;
            stats->free_space_recovered += bytes_reclaimed;
            page_dirty = true;
        }

        if (storage_engine != nullptr && metrics.warn_threshold)
        {
            storage_engine->publishFragmentationAdvisory(table_id, page_id, advisory);
        }

        db_->buffer_pool()->unpinPage(page_id, page_dirty, ctx);

        return Status::OK;
    }

    auto GcManager::freezeTable(const ID &table_id, uint64_t freeze_limit, GcStats *stats_out,
                             ErrorContext *ctx) -> Status
    {
        // Freeze tuples with xmin < freeze_limit to prevent XID wraparound
        GcStats stats;

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
