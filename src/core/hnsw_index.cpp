// PHASE 4A TASK 4A.2: HNSW Index Implementation (Stub)
// This is a minimal stub to allow compilation. Full implementation to follow.

#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <random>
#include <set>

namespace scratchbird::core
{

HnswIndex::HnswIndex(Database *db, SBHnswIndex index_info)
    : db_(db), index_info_(std::move(index_info))
{
}

HnswIndex::~HnswIndex()
{
}

Status HnswIndex::create(Database *db,
                        const UuidV7Bytes &index_uuid,
                        const UuidV7Bytes &table_uuid,
                        const std::vector<UuidV7Bytes> &column_uuids,
                        uint32_t dimensions,
                        DistanceMetric distance_metric,
                        uint32_t m,
                        uint32_t ef_construction,
                        uint32_t ef_search,
                        uint32_t *root_page_out,
                        ErrorContext *ctx)
{
    if (!db || !root_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments");
        return Status::INVALID_ARGUMENT;
    }

    if (dimensions == 0 || dimensions > 65536)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid dimensions");
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

std::unique_ptr<HnswIndex> HnswIndex::open(Database *db,
                                          const UuidV7Bytes &index_uuid,
                                          uint32_t root_page,
                                          ErrorContext *ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
        return nullptr;
    }

    SBHnswIndex index_info;
    std::memcpy(index_info.idx_uuid.bytes.data(), index_uuid.bytes.data(), 16);
    index_info.idx_root_page = root_page;
    index_info.idx_m = 16;
    index_info.idx_ef_construction = 200;
    index_info.idx_ef_search = 100;
    index_info.idx_dimensions = 1536; // Default for OpenAI embeddings
    index_info.idx_distance_metric = static_cast<uint8_t>(DistanceMetric::EUCLIDEAN);

    return std::make_unique<HnswIndex>(db, index_info);
}

Status HnswIndex::insert(const VectorValue &vector,
                        const TID &tid,
                        ErrorContext *ctx)
{
    // PHASE 1.5: Convert TID to legacy format for storage
    uint64_t legacy_tid = convertTIDtoLegacy(tid);
    if (legacy_tid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "Custom tablespace indexes not yet supported in ALPHA");
        return Status::NOT_IMPLEMENTED;
    }

    // Stub: Always succeed
    // TODO: Implement HNSW graph insertion
    (void)vector;
    (void)legacy_tid;
    return Status::OK;
}

Status HnswIndex::remove(const TID &tid,
                        ErrorContext *ctx)
{
    // PHASE 1.5: Convert TID to legacy format for lookup
    uint64_t legacy_tid = convertTIDtoLegacy(tid);
    if (legacy_tid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                          "Custom tablespace indexes not yet supported in ALPHA");
        return Status::NOT_IMPLEMENTED;
    }

    // Stub: Always succeed
    // TODO: Implement HNSW graph node deletion (soft delete: set xmax)
    (void)legacy_tid;
    return Status::OK;
}

Status HnswIndex::search(const VectorValue &query_vector,
                        uint32_t k,
                        struct Snapshot *snapshot,
                        std::vector<HnswSearchResult> *results_out,
                        ErrorContext *ctx)
{
    if (!results_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid output parameter");
        return Status::INVALID_ARGUMENT;
    }

    // Stub: Return empty result
    results_out->clear();
    return Status::OK;
}

Status HnswIndex::vacuum(VacuumStats *stats_out, ErrorContext *ctx)
{
    if (stats_out)
    {
        stats_out->nodes_visited = 0;
        stats_out->nodes_removed = 0;
        stats_out->links_updated = 0;
        stats_out->bytes_reclaimed = 0;
    }
    return Status::OK;
}

Status HnswIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                   uint64_t *entries_removed_out,
                                   uint64_t *pages_modified_out,
                                   ErrorContext *ctx)
{
    // PHASE 1.5: Convert TID structs to legacy format set for lookup
    std::set<uint64_t> dead_set;
    for (const TID &tid : dead_tids)
    {
        uint64_t legacy = convertTIDtoLegacy(tid);
        if (legacy != 0)  // Skip custom tablespace TIDs
        {
            dead_set.insert(legacy);
        }
    }

    // Stub: Report no entries removed
    // TODO: Implement HNSW graph node removal and link updates
    if (entries_removed_out)
    {
        *entries_removed_out = 0;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = 0;
    }

    (void)dead_set;  // Avoid unused variable warning
    return Status::OK;
}

Status HnswIndex::getStats(HnswStats *stats_out, ErrorContext *ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid stats output");
        return Status::INVALID_ARGUMENT;
    }

    stats_out->total_nodes = 0;
    stats_out->deleted_nodes = 0;
    stats_out->total_pages = 0;
    stats_out->max_layer = 0;
    stats_out->avg_connections = 0.0;
    stats_out->avg_path_length = 0.0;

    return Status::OK;
}

// Private helper methods (stubs)

uint16_t HnswIndex::select_layer()
{
    // Stub: Always return layer 0
    return 0;
}

Status HnswIndex::find_nearest(const VectorValue &query,
                              uint32_t k,
                              uint16_t layer,
                              uint64_t entry_point,
                              struct Snapshot *snapshot,
                              std::vector<HnswSearchResult> *results_out,
                              ErrorContext *ctx)
{
    return Status::OK;
}

double HnswIndex::compute_distance(const VectorValue &a, const VectorValue &b) const
{
    DistanceMetric metric = static_cast<DistanceMetric>(index_info_.idx_distance_metric);
    auto result = a.distance(b, metric);
    return result.value_or(0.0);
}

bool HnswIndex::is_node_visible(const SBHnswNode *node,
                                struct Snapshot *snapshot,
                                ErrorContext *ctx) const
{
    return true;
}

Status HnswIndex::add_link(uint64_t from_tid, uint64_t to_tid,
                           uint16_t layer, ErrorContext *ctx)
{
    return Status::OK;
}

Status HnswIndex::remove_link(uint64_t from_tid, uint64_t to_tid,
                              uint16_t layer, ErrorContext *ctx)
{
    return Status::OK;
}

Status HnswIndex::find_node(uint64_t tuple_id,
                            SBHnswNode **node_out,
                            uint64_t *page_num_out,
                            ErrorContext *ctx)
{
    return Status::OK;
}

Status HnswIndex::prune_connections(uint64_t node_tid, uint16_t layer,
                                   ErrorContext *ctx)
{
    return Status::OK;
}

// ==================================================================
// PHASE 5 TASK 5.3.2: Update TIDs After Tablespace Migration
// ==================================================================

Status HnswIndex::updateTIDsAfterMigration(
    const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
    uint64_t *tids_updated_out,
    uint64_t *pages_modified_out,
    ErrorContext *ctx)
{
    // Initialize output counters
    if (tids_updated_out != nullptr)
    {
        *tids_updated_out = 0;
    }
    if (pages_modified_out != nullptr)
    {
        *pages_modified_out = 0;
    }

    // Early exit if no TID mapping (empty table or no migration)
    if (tid_mapping.empty())
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
    uint64_t total_tids_updated = 0;
    uint64_t total_pages_modified = 0;
    bool had_errors = false;

    // ===== STEP 1: Determine max layer =====
    // We need to know how many layers exist in the HNSW graph
    // This info is stored in index_info_ or we can scan from the root page

    uint32_t root_page = index_info_.idx_root_page;
    if (root_page == 0)
    {
        // Empty index, nothing to update
        return Status::OK;
    }

    // Pin the root page to determine max layer
    void *root_buffer = nullptr;
    Status pin_status = bp->pinPage(root_page, &root_buffer, ctx);
    if (pin_status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, pin_status, "Failed to pin root page");
        return pin_status;
    }

    auto *root_page_hdr = reinterpret_cast<SBHnswPage *>(root_buffer);
    uint16_t max_layer = 0;

    // Scan the root page to find max layer
    uint8_t *page_data = reinterpret_cast<uint8_t *>(root_buffer);
    uint16_t node_count = root_page_hdr->hnsw_count;

    // Find max layer from nodes
    size_t offset = sizeof(SBHnswPage);
    for (uint16_t i = 0; i < node_count && offset < db_->page_size(); i++)
    {
        auto *node = reinterpret_cast<SBHnswNode *>(page_data + offset);
        if (node->node_layer > max_layer)
        {
            max_layer = node->node_layer;
        }

        // Calculate size of this variable-length node
        size_t node_size = sizeof(SBHnswNode);
        node_size += node->node_num_neighbors * sizeof(uint64_t);  // Neighbors
        node_size += node->node_vector_len;                        // Vector data

        offset += node_size;
    }

    bp->unpinPage(root_page, false, ctx);

    LOG_INFO(STORAGE, "HNSW index has %u layers, starting TID update scan", max_layer + 1);

    // ===== STEP 2: Scan all layers from bottom (0) to top (max_layer) =====
    for (uint16_t layer = 0; layer <= max_layer; layer++)
    {
        // For each layer, we need to scan all pages that belong to that layer
        // HNSW pages use left/right sibling pointers for horizontal navigation

        // Find the first page in this layer
        // Strategy: Start from root and scan for pages with hnsw_layer == layer

        std::vector<uint64_t> layer_pages;

        // Collect all pages in this layer (simplified: scan from root linearly)
        // In a full implementation, we'd maintain a layer-to-pages mapping
        // For now, we'll scan all pages and filter by layer

        uint64_t current_page = root_page;
        std::set<uint64_t> visited_pages;

        // BFS to find all pages in the index
        std::vector<uint64_t> pages_to_scan = {current_page};

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
            Status page_pin_status = bp->pinPage(page_num, &page_buffer, ctx);
            if (page_pin_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to pin HNSW page %lu during TID update: %d",
                           page_num, static_cast<int>(page_pin_status));
                had_errors = true;
                continue;
            }

            auto *hnsw_page = reinterpret_cast<SBHnswPage *>(page_buffer);

            // Check if this page belongs to the current layer
            if (hnsw_page->hnsw_layer == layer)
            {
                layer_pages.push_back(page_num);
            }

            // Add sibling pages to scan list
            if (hnsw_page->hnsw_left_sibling != 0 &&
                visited_pages.count(hnsw_page->hnsw_left_sibling) == 0)
            {
                pages_to_scan.push_back(hnsw_page->hnsw_left_sibling);
            }
            if (hnsw_page->hnsw_right_sibling != 0 &&
                visited_pages.count(hnsw_page->hnsw_right_sibling) == 0)
            {
                pages_to_scan.push_back(hnsw_page->hnsw_right_sibling);
            }

            bp->unpinPage(page_num, false, ctx);
        }

        LOG_INFO(STORAGE, "Layer %u: found %lu pages to update", layer, layer_pages.size());

        // ===== STEP 3: Update TIDs in all pages of this layer =====
        for (uint64_t page_num : layer_pages)
        {
            void *page_buffer = nullptr;
            Status page_pin_status = bp->pinPage(page_num, &page_buffer, ctx);
            if (page_pin_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to pin HNSW page %lu for TID update: %d",
                           page_num, static_cast<int>(page_pin_status));
                had_errors = true;
                continue;
            }

            auto *hnsw_page = reinterpret_cast<SBHnswPage *>(page_buffer);
            uint8_t *page_bytes = reinterpret_cast<uint8_t *>(page_buffer);

            bool page_modified = false;
            uint16_t nodes_on_page = hnsw_page->hnsw_count;

            // Scan all nodes on this page
            size_t node_offset = sizeof(SBHnswPage);
            for (uint16_t i = 0; i < nodes_on_page && node_offset < db_->page_size(); i++)
            {
                auto *node = reinterpret_cast<SBHnswNode *>(page_bytes + node_offset);

                // Extract old TID
                uint64_t old_tid = node->node_tuple_id;

                // Look up in mapping
                auto it = tid_mapping.find(old_tid);
                if (it != tid_mapping.end())
                {
                    // Found mapping - update TID
                    uint64_t new_tid = it->second;
                    node->node_tuple_id = new_tid;

                    total_tids_updated++;
                    page_modified = true;

                    LOG_DEBUG(STORAGE, "Updated HNSW node TID: %lu -> %lu (layer %u, page %lu)",
                             old_tid, new_tid, layer, page_num);
                }

                // Calculate size of this variable-length node
                size_t node_size = sizeof(SBHnswNode);
                node_size += node->node_num_neighbors * sizeof(uint64_t);
                node_size += node->node_vector_len;

                node_offset += node_size;
            }

            // Unpin page (mark dirty if modified)
            bp->unpinPage(page_num, page_modified, ctx);

            if (page_modified)
            {
                total_pages_modified++;
            }
        }
    }

    // Return statistics
    if (tids_updated_out != nullptr)
    {
        *tids_updated_out = total_tids_updated;
    }
    if (pages_modified_out != nullptr)
    {
        *pages_modified_out = total_pages_modified;
    }

    if (had_errors)
    {
        LOG_WARNING(STORAGE, "HNSW TID update completed with some errors: %lu TIDs updated, %lu pages modified",
                   total_tids_updated, total_pages_modified);
        // Return OK since migration can still proceed, errors are logged
    }
    else
    {
        LOG_INFO(STORAGE, "HNSW TID update completed successfully: %lu TIDs updated, %lu pages modified",
                total_tids_updated, total_pages_modified);
    }

    return Status::OK;
}

} // namespace scratchbird::core
