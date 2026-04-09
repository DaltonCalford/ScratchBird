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

#include "scratchbird/core/gc_publication.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>

namespace scratchbird::core
{
    // Forward declarations
    class Database;
    struct ErrorContext;
    using ID = UuidV7Bytes;

    // GC statistics (ScratchBird MGA GC, not PostgreSQL VACUUM)
    struct GcStats
    {
        uint64_t pages_scanned;
        uint64_t tuples_scanned;
        uint64_t dead_tuples_found;
        uint64_t dead_tuples_removed;
        uint64_t version_chains_pruned;
        uint64_t pages_compacted;
        uint64_t free_space_recovered; // bytes
        uint64_t tuples_frozen;        // tuples frozen to prevent wraparound
        uint64_t gc_time_us;           // microseconds
        uint64_t pages_dead_space_warn;
        uint64_t pages_dead_space_compact;
        uint64_t pages_dead_space_rewrite;
        uint64_t rewrite_recommendations;
        uint64_t slot_stable_compactions;
        uint64_t index_backlog_count;
        uint64_t index_backlog_pages;
        uint64_t index_backlog_bytes;

        GcStats()
            : pages_scanned(0), tuples_scanned(0), dead_tuples_found(0), dead_tuples_removed(0),
              version_chains_pruned(0), pages_compacted(0), free_space_recovered(0),
              tuples_frozen(0), gc_time_us(0), pages_dead_space_warn(0),
              pages_dead_space_compact(0), pages_dead_space_rewrite(0),
              rewrite_recommendations(0), slot_stable_compactions(0), index_backlog_count(0),
              index_backlog_pages(0), index_backlog_bytes(0)
        {
        }
    };

    // GC manager - reclaims space from dead tuples (ScratchBird MGA GC, not PostgreSQL VACUUM)
    class GcManager
    {
    public:
        explicit GcManager(Database *db);
        ~GcManager();

        // GC a single table
        Status gcTable(const ID &table_id, GcStats *stats_out, ErrorContext *ctx = nullptr);

        // GC entire database
        Status gcDatabase(GcStats *stats_out, ErrorContext *ctx = nullptr);

        // GC a single page (for targeted cleanup)
        Status gcPage(const ID &table_id, uint32_t page_id, GcStats *stats_out,
                      ErrorContext *ctx = nullptr,
                      const HeapReclaimPublicationContext *publication_ctx = nullptr);

        // Get GC horizon (oldest XID that might still see a tuple)
        Status getGcHorizon(uint64_t *horizon_out, ErrorContext *ctx = nullptr);

        // Freeze old tuples to prevent XID wraparound
        // freeze_limit: tuples with xmin < freeze_limit will be frozen
        Status freezeTable(const ID &table_id, uint64_t freeze_limit, GcStats *stats_out,
                           ErrorContext *ctx = nullptr);

    private:
        Database *db_;

        // Scan heap for dead tuples
        Status scanHeapForDeadTuples(const ID &table_id, uint64_t horizon,
                                     std::vector<uint64_t> *dead_tids_out, GcStats *stats,
                                     ErrorContext *ctx);

        // Prune version chains on a page
        Status pruneVersionChains(const ID &table_id, uint32_t page_id, uint64_t horizon,
                                  GcStats *stats, ErrorContext *ctx);

        // Remove dead tuples from a page
        Status removeDeadTuplesFromPage(const ID &table_id, uint32_t page_id,
                                        const std::vector<uint16_t> &dead_item_ids,
                                        GcStats *stats, ErrorContext *ctx);

        // Compact page to reclaim free space
        Status compactPage(const ID &table_id, uint32_t page_id, GcStats *stats,
                           ErrorContext *ctx);
    };

} // namespace scratchbird::core
