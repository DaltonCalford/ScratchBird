#pragma once

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

    // Vacuum statistics
    struct VacuumStats
    {
        uint64_t pages_scanned;
        uint64_t tuples_scanned;
        uint64_t dead_tuples_found;
        uint64_t dead_tuples_removed;
        uint64_t version_chains_pruned;
        uint64_t pages_compacted;
        uint64_t free_space_recovered; // bytes
        uint64_t tuples_frozen;        // tuples frozen to prevent wraparound
        uint64_t vacuum_time_us;       // microseconds

        VacuumStats()
            : pages_scanned(0), tuples_scanned(0), dead_tuples_found(0), dead_tuples_removed(0),
              version_chains_pruned(0), pages_compacted(0), free_space_recovered(0),
              tuples_frozen(0), vacuum_time_us(0)
        {
        }
    };

    // Vacuum manager - reclaims space from dead tuples
    class Vacuum
    {
    public:
        explicit Vacuum(Database *db);
        ~Vacuum();

        // Vacuum a single table
        Status vacuumTable(const ID &table_id, VacuumStats *stats_out, ErrorContext *ctx = nullptr);

        // Vacuum entire database
        Status vacuumDatabase(VacuumStats *stats_out, ErrorContext *ctx = nullptr);

        // Vacuum a single page (for targeted cleanup)
        Status vacuumPage(const ID &table_id, uint32_t page_id, VacuumStats *stats_out,
                          ErrorContext *ctx = nullptr);

        // Get vacuum horizon (oldest XID that might still see a tuple)
        Status getVacuumHorizon(uint64_t *horizon_out, ErrorContext *ctx = nullptr);

        // Freeze old tuples to prevent XID wraparound
        // freeze_limit: tuples with xmin < freeze_limit will be frozen
        Status freezeTable(const ID &table_id, uint64_t freeze_limit, VacuumStats *stats_out,
                           ErrorContext *ctx = nullptr);

    private:
        Database *db_;

        // Scan heap for dead tuples
        Status scanHeapForDeadTuples(const ID &table_id, uint64_t horizon,
                                     std::vector<uint64_t> *dead_tids_out, VacuumStats *stats,
                                     ErrorContext *ctx);

        // Prune version chains on a page
        Status pruneVersionChains(const ID &table_id, uint32_t page_id, uint64_t horizon,
                                  VacuumStats *stats, ErrorContext *ctx);

        // Remove dead tuples from a page
        Status removeDeadTuplesFromPage(const ID &table_id, uint32_t page_id,
                                        const std::vector<uint16_t> &dead_item_ids,
                                        VacuumStats *stats, ErrorContext *ctx);

        // Compact page to reclaim free space
        Status compactPage(uint32_t page_id, VacuumStats *stats, ErrorContext *ctx);

        // Check if tuple is dead (not visible to any transaction)
        bool isTupleDead(const uint8_t *tuple_data, uint64_t horizon) const;

        // Check if tuple version is prunable
        bool isVersionPrunable(const uint8_t *tuple_data, uint64_t horizon) const;
    };

} // namespace scratchbird::core
