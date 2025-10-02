#include "scratchbird/core/vacuum.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/error_context.h"
#include <chrono>
#include <cstring>

namespace scratchbird::core
{

    Vacuum::Vacuum(Database* db) : db_(db)
    {
    }

    Vacuum::~Vacuum() = default;

    auto Vacuum::getVacuumHorizon(uint64_t* horizon_out, ErrorContext* ctx) -> Status
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

    auto Vacuum::vacuumTable(const ID& table_id, VacuumStats* stats_out,
                            ErrorContext* ctx) -> Status
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
        for (const auto& [page_id, dead_items] : dead_by_page)
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
        stats.vacuum_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                  end_time - start_time).count();

        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }

        return Status::OK;
    }

    auto Vacuum::vacuumDatabase(VacuumStats* stats_out, ErrorContext* ctx) -> Status
    {
        // TODO: Iterate over all tables and vacuum each one
        // For now, just return OK (requires catalog iteration)
        VacuumStats stats;
        if (stats_out != nullptr)
        {
            *stats_out = stats;
        }
        return Status::OK;
    }

    auto Vacuum::vacuumPage(const ID& table_id, uint32_t page_id,
                           VacuumStats* stats_out, ErrorContext* ctx) -> Status
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

    auto Vacuum::scanHeapForDeadTuples(const ID& table_id, uint64_t horizon,
                                      std::vector<uint64_t>* dead_tids_out,
                                      VacuumStats* stats, ErrorContext* ctx) -> Status
    {
        // TODO: Get table page range from catalog
        // For Phase 4, we'll scan a fixed range of pages
        constexpr uint32_t START_PAGE = 7;   // First heap page
        constexpr uint32_t MAX_PAGES = 1000; // Scan up to 1000 pages

        for (uint32_t page_id = START_PAGE; page_id < START_PAGE + MAX_PAGES; ++page_id)
        {
            void* page_buffer;
            Status status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
            if (status == Status::IO_ERROR)
            {
                // Page doesn't exist, we've reached the end
                break;
            }
            if (status != Status::OK)
            {
                continue;
            }

            auto* page_data = static_cast<uint8_t*>(page_buffer);
            auto* page_hdr = reinterpret_cast<PageHeader*>(page_data);

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
                const uint8_t* tuple_data;
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

    auto Vacuum::pruneVersionChains(const ID& table_id, uint32_t page_id,
                                   uint64_t horizon, VacuumStats* stats,
                                   ErrorContext* ctx) -> Status
    {
        void* page_buffer;
        Status status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto* page_data = static_cast<uint8_t*>(page_buffer);
        HeapPage heap(page_data, db_->page_size());
        bool page_modified = false;

        // Scan all tuples looking for prunable versions
        uint16_t item_count = heap.getItemCount();
        for (uint16_t item_id = 0; item_id < item_count; ++item_id)
        {
            const uint8_t* tuple_data;
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
                auto* tuple_hdr = const_cast<TupleHeader*>(
                    reinterpret_cast<const TupleHeader*>(tuple_data));

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

    auto Vacuum::removeDeadTuplesFromPage(const ID& table_id, uint32_t page_id,
                                         const std::vector<uint16_t>& dead_item_ids,
                                         VacuumStats* stats, ErrorContext* ctx) -> Status
    {
        if (dead_item_ids.empty())
        {
            return Status::OK;
        }

        void* page_buffer;
        Status status = db_->buffer_pool()->pinPage(page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto* page_data = static_cast<uint8_t*>(page_buffer);
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

    auto Vacuum::compactPage(uint32_t page_id, VacuumStats* stats,
                            ErrorContext* ctx) -> Status
    {
        // TODO: Implement page compaction
        // This would:
        // 1. Pin page
        // 2. Rebuild item array, removing deleted items
        // 3. Defragment tuple storage area
        // 4. Update free space pointers
        // 5. Mark page dirty and unpin
        //
        // For Phase 4, we defer this to avoid complexity
        return Status::NOT_IMPLEMENTED;
    }

    bool Vacuum::isTupleDead(const uint8_t* tuple_data, uint64_t horizon)
    {
        auto* header = reinterpret_cast<const TupleHeader*>(tuple_data);

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

    bool Vacuum::isVersionPrunable(const uint8_t* tuple_data, uint64_t horizon)
    {
        auto* header = reinterpret_cast<const TupleHeader*>(tuple_data);

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

} // namespace scratchbird::core
