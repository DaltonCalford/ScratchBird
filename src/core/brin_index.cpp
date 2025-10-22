// PHASE 4A TASK 4A.1: BRIN Index Implementation (Stub)
// This is a minimal stub to allow compilation. Full implementation to follow.

#include "scratchbird/core/brin_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <set>

namespace scratchbird::core
{

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
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No page manager");
        return Status::INVALID_ARGUMENT;
    }

    // Allocate root page
    uint32_t root_page = 0;
    Status status = page_mgr->allocatePage(root_page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    *root_page_out = root_page;
    return Status::OK;
}

std::unique_ptr<BrinIndex> BrinIndex::open(Database *db,
                                          const UuidV7Bytes &index_uuid,
                                          uint32_t root_page,
                                          ErrorContext *ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
        return nullptr;
    }

    SBBrinIndex index_info;
    std::memcpy(index_info.idx_uuid.bytes.data(), index_uuid.bytes.data(), 16);
    index_info.idx_root_page = root_page;
    index_info.idx_range_size = 128;

    return std::make_unique<BrinIndex>(db, index_info);
}

Status BrinIndex::insert(const std::vector<uint8_t> &value,
                        uint32_t block_number,
                        ErrorContext *ctx)
{
    // Stub: Always succeed
    return Status::OK;
}

Status BrinIndex::scan(const std::vector<uint8_t> *min_value,
                      const std::vector<uint8_t> *max_value,
                      struct Snapshot *snapshot,
                      std::vector<uint32_t> *block_numbers_out,
                      ErrorContext *ctx)
{
    if (!block_numbers_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid output parameter");
        return Status::INVALID_ARGUMENT;
    }

    // Stub: Return empty result
    block_numbers_out->clear();
    return Status::OK;
}

Status BrinIndex::remove(const std::vector<uint8_t> &value,
                        uint32_t block_number,
                        ErrorContext *ctx)
{
    // Stub: Always succeed
    return Status::OK;
}

Status BrinIndex::vacuum(VacuumStats *stats_out, ErrorContext *ctx)
{
    if (stats_out)
    {
        stats_out->ranges_visited = 0;
        stats_out->ranges_removed = 0;
        stats_out->ranges_updated = 0;
        stats_out->bytes_reclaimed = 0;
    }
    return Status::OK;
}

Status BrinIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                   uint64_t *entries_removed_out,
                                   uint64_t *pages_modified_out,
                                   ErrorContext *ctx)
{
    // PHASE 1.5: Extract block numbers from TID structs
    // BRIN indexes are block-based, not tuple-based
    std::set<uint32_t> dead_blocks;
    for (const TID &tid : dead_tids)
    {
        // Extract page number (block number) from TID
        uint64_t page_num = getPageNumber(tid);
        if (page_num <= UINT32_MAX)  // Ensure it fits in uint32_t
        {
            dead_blocks.insert(static_cast<uint32_t>(page_num));
        }
    }

    // Stub: Report no entries removed
    // TODO: Implement BRIN range summary removal based on dead blocks
    if (entries_removed_out)
    {
        *entries_removed_out = 0;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = 0;
    }

    (void)dead_blocks;  // Avoid unused variable warning
    return Status::OK;
}

Status BrinIndex::getStats(BrinStats *stats_out, ErrorContext *ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid stats output");
        return Status::INVALID_ARGUMENT;
    }

    stats_out->total_ranges = 0;
    stats_out->deleted_ranges = 0;
    stats_out->total_pages = 0;
    stats_out->blocks_covered = 0;
    stats_out->avg_range_selectivity = 0.0;

    return Status::OK;
}

// Private helper methods (stubs)

Status BrinIndex::find_range_page(uint32_t block_number,
                                  uint64_t *page_num_out,
                                  bool write_lock,
                                  ErrorContext *ctx)
{
    if (page_num_out)
    {
        *page_num_out = index_info_.idx_root_page;
    }
    return Status::OK;
}

bool BrinIndex::value_in_range(const uint8_t *value, uint16_t value_len,
                               const uint8_t *min_val, uint16_t min_len,
                               const uint8_t *max_val, uint16_t max_len) const
{
    return false;
}

int BrinIndex::compare_values(const uint8_t *v1, uint16_t v1_len,
                              const uint8_t *v2, uint16_t v2_len) const
{
    return std::memcmp(v1, v2, std::min(v1_len, v2_len));
}

bool BrinIndex::is_range_visible(const SBBrinRange *range,
                                 struct Snapshot *snapshot,
                                 ErrorContext *ctx) const
{
    return true;
}

Status BrinIndex::update_range_summary(uint64_t page_num,
                                      SBBrinRange *range,
                                      const std::vector<uint8_t> &value,
                                      ErrorContext *ctx)
{
    return Status::OK;
}

Status BrinIndex::split_page(uint64_t page_num, ErrorContext *ctx)
{
    return Status::OK;
}

// ==================================================================
// PHASE 5 TASK 5.3.4: Update Block Ranges After Tablespace Migration
// ==================================================================

Status BrinIndex::updateBlockRangesAfterMigration(
    const std::unordered_map<uint64_t, uint64_t> &page_mapping,
    uint64_t *ranges_updated_out,
    uint64_t *pages_modified_out,
    ErrorContext *ctx)
{
    // Initialize output counters
    if (ranges_updated_out != nullptr)
    {
        *ranges_updated_out = 0;
    }
    if (pages_modified_out != nullptr)
    {
        *pages_modified_out = 0;
    }

    // Early exit if no page mapping (empty table or no migration)
    if (page_mapping.empty())
    {
        return Status::OK;
    }

    BufferPool *bp = db_->buffer_pool();
    if (!bp)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
        return Status::INVALID_ARGUMENT;
    }

    // Statistics
    uint64_t total_ranges_updated = 0;
    uint64_t total_pages_modified = 0;
    bool had_errors = false;

    // ===== STEP 1: Get root page =====
    uint32_t root_page = index_info_.idx_root_page;
    if (root_page == 0)
    {
        // Empty index, nothing to update
        return Status::OK;
    }

    // ===== STEP 2: Scan all BRIN pages using sibling pointers =====
    // Start from root and scan left-to-right using brin_left/right_sibling pointers

    std::set<uint64_t> visited_pages;
    std::vector<uint64_t> pages_to_scan = {root_page};

    while (!pages_to_scan.empty())
    {
        uint64_t page_num = pages_to_scan.back();
        pages_to_scan.pop_back();

        if (visited_pages.count(page_num) > 0)
        {
            continue;  // Already visited
        }
        visited_pages.insert(page_num);

        void *page_buffer = nullptr;
        Status pin_status = bp->pinPage(static_cast<uint32_t>(page_num), &page_buffer, ctx);
        if (pin_status != Status::OK)
        {
            LOG_WARNING(STORAGE, "Failed to pin BRIN page %lu during block range update: %d",
                       page_num, static_cast<int>(pin_status));
            had_errors = true;
            continue;
        }

        auto *brin_page = reinterpret_cast<SBBrinPage *>(page_buffer);
        uint8_t *page_bytes = reinterpret_cast<uint8_t *>(page_buffer);

        bool page_modified = false;
        uint16_t ranges_on_page = brin_page->brin_count;

        // Add sibling pages to scan list
        if (brin_page->brin_left_sibling != 0 &&
            visited_pages.count(brin_page->brin_left_sibling) == 0)
        {
            pages_to_scan.push_back(brin_page->brin_left_sibling);
        }
        if (brin_page->brin_right_sibling != 0 &&
            visited_pages.count(brin_page->brin_right_sibling) == 0)
        {
            pages_to_scan.push_back(brin_page->brin_right_sibling);
        }

        // ===== STEP 3: Update block ranges in all SBBrinRange structures on this page =====
        size_t range_offset = sizeof(SBBrinPage);
        for (uint16_t i = 0; i < ranges_on_page && range_offset < db_->page_size(); i++)
        {
            auto *range = reinterpret_cast<SBBrinRange *>(page_bytes + range_offset);

            // Extract old block range
            uint32_t old_start_block = range->brn_start_block;
            uint32_t old_end_block = range->brn_end_block;

            bool range_updated = false;

            // Look up start block in mapping
            auto it_start = page_mapping.find(static_cast<uint64_t>(old_start_block));
            if (it_start != page_mapping.end())
            {
                // Found mapping for start block - update
                uint32_t new_start_block = static_cast<uint32_t>(it_start->second);
                range->brn_start_block = new_start_block;

                range_updated = true;
                page_modified = true;

                LOG_DEBUG(STORAGE, "Updated BRIN start block: %u -> %u (page %lu)",
                         old_start_block, new_start_block, page_num);
            }

            // Look up end block in mapping
            auto it_end = page_mapping.find(static_cast<uint64_t>(old_end_block));
            if (it_end != page_mapping.end())
            {
                // Found mapping for end block - update
                uint32_t new_end_block = static_cast<uint32_t>(it_end->second);
                range->brn_end_block = new_end_block;

                range_updated = true;
                page_modified = true;

                LOG_DEBUG(STORAGE, "Updated BRIN end block: %u -> %u (page %lu)",
                         old_end_block, new_end_block, page_num);
            }

            if (range_updated)
            {
                total_ranges_updated++;
            }

            // Calculate size of this variable-length range structure
            // Structure: SBBrinRange + min_value + max_value
            size_t range_size = sizeof(SBBrinRange);
            range_size += range->brn_min_len;  // Min value
            range_size += range->brn_max_len;  // Max value

            range_offset += range_size;
        }

        // Unpin page (mark dirty if modified)
        bp->unpinPage(static_cast<uint32_t>(page_num), page_modified, ctx);

        if (page_modified)
        {
            total_pages_modified++;
        }
    }

    // Return statistics
    if (ranges_updated_out != nullptr)
    {
        *ranges_updated_out = total_ranges_updated;
    }
    if (pages_modified_out != nullptr)
    {
        *pages_modified_out = total_pages_modified;
    }

    if (had_errors)
    {
        LOG_WARNING(STORAGE, "BRIN block range update completed with some errors: %lu ranges updated, %lu pages modified",
                   total_ranges_updated, total_pages_modified);
        // Return OK since migration can still proceed, errors are logged
    }
    else
    {
        LOG_INFO(STORAGE, "BRIN block range update completed successfully: %lu ranges updated, %lu pages modified",
                total_ranges_updated, total_pages_modified);
    }

    return Status::OK;
}

} // namespace scratchbird::core
