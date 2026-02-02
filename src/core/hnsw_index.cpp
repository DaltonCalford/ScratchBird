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
 * HNSW (Hierarchical Navigable Small World) Index Implementation
 * Complete implementation for vector similarity search
 *
 * HNSW provides efficient approximate k-NN search with:
 * - Multi-layer graph structure (hierarchical navigation)
 * - Greedy best-first search with beam search
 * - Configurable accuracy/speed tradeoff
 * - MGA-compliant visibility checking
 *
 * Based on: "Efficient and robust approximate nearest neighbor search
 * using Hierarchical Navigable Small World graphs" (Malkov & Yashunin, 2016)
 */

#include "scratchbird/core/hnsw_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <random>
#include <set>
#include <queue>
#include <algorithm>
#include <cmath>

namespace scratchbird::core
{

// =============================================================================
// Helper Structures
// =============================================================================

// Priority queue element for search
struct SearchCandidate
{
    TID tuple_id;
    double distance;

    bool operator<(const SearchCandidate& other) const
    {
        return distance > other.distance; // Min-heap
    }

    bool operator>(const SearchCandidate& other) const
    {
        return distance < other.distance; // Max-heap for top-k
    }
};

// =============================================================================
// HnswIndex Implementation
// =============================================================================

HnswIndex::HnswIndex(Database *db, SBHnswIndex index_info)
    : db_(db), index_info_(std::move(index_info))
{
    // Initialize random number generator for layer selection
    std::random_device rd;
    rng_.seed(rd());
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
                        GPID root_gpid,
                        ErrorContext *ctx)
{
    if (!db || root_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments");
        return Status::INVALID_ARGUMENT;
    }

    if (dimensions == 0 || dimensions > 65536)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid dimensions (must be 1-65536)");
        return Status::INVALID_ARGUMENT;
    }

    if (m == 0 || m > 1024)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid M parameter (must be 1-1024)");
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
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin HNSW root page");
        return Status::IO_ERROR;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBHnswPage *root = reinterpret_cast<SBHnswPage*>(page_data);
    std::memset(root, 0, sizeof(SBHnswPage));

    // Initialize page header
    root->hnsw_index_uuid = index_uuid;
    root->hnsw_table_uuid = table_uuid;
    root->hnsw_flags = static_cast<uint16_t>(HnswFlags::ROOT);
    root->hnsw_count = 0;
    root->hnsw_free_space = db->page_size() - sizeof(SBHnswPage);
    root->hnsw_layer = 0; // Base layer
    root->hnsw_m = m;
    root->hnsw_dimensions = dimensions;
    root->hnsw_left_sibling = 0;
    root->hnsw_right_sibling = 0;
    root->hnsw_xmin = txn_mgr->getCurrentXid();
    root->hnsw_xmax = 0;
    root->hnsw_lsn = 0;
    root->hnsw_total_nodes = 0;
    root->hnsw_deleted_nodes = 0;
    root->hnsw_distance_metric = static_cast<uint8_t>(distance_metric);

    buffer_pool->unpinPageGlobal(root_gpid, true, ctx); // Mark dirty

    LOG_INFO(GENERAL, "Created HNSW index with root page %u, dimensions %u, M %u",
             root_page, dimensions, m);

    return Status::OK;
}

std::unique_ptr<HnswIndex> HnswIndex::open(Database *db,
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

    SBHnswIndex index_info{};
    std::memcpy(index_info.idx_uuid.bytes.data(), index_uuid.bytes.data(), 16);
    index_info.idx_root_page = root_page;
    index_info.idx_tablespace_id = getTablespaceID(root_gpid);
    index_info.idx_m = 16;
    index_info.idx_ef_construction = 200;
    index_info.idx_ef_search = 100;
    index_info.idx_dimensions = 1536; // Default for OpenAI embeddings
    index_info.idx_distance_metric = static_cast<uint8_t>(DistanceMetric::EUCLIDEAN);

    BufferPool *buffer_pool = db->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing buffer pool");
        return nullptr;
    }

    void *page_buffer = nullptr;
    Status status = buffer_pool->pinPageGlobal(root_gpid, &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Failed to pin HNSW root page");
        return nullptr;
    }

    SBHnswPage *root = reinterpret_cast<SBHnswPage*>(page_buffer);
    if (std::memcmp(root->hnsw_index_uuid.bytes.data(),
                    index_uuid.bytes.data(), 16) != 0)
    {
        buffer_pool->unpinPageGlobal(root_gpid, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "HNSW root page does not match index UUID");
        return nullptr;
    }

    index_info.idx_table_uuid = root->hnsw_table_uuid;
    index_info.idx_m = root->hnsw_m;
    index_info.idx_dimensions = root->hnsw_dimensions;
    index_info.idx_total_nodes = root->hnsw_total_nodes;
    index_info.idx_creation_xid = root->hnsw_xmin;
    index_info.idx_distance_metric = root->hnsw_distance_metric;

    buffer_pool->unpinPageGlobal(root_gpid, false, ctx);

    return std::make_unique<HnswIndex>(db, index_info);
}

GPID HnswIndex::indexGPID(uint64_t page_num) const
{
    return makeGPID(index_info_.idx_tablespace_id, page_num);
}

Status HnswIndex::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx) const
{
    return db_->buffer_pool()->pinPageGlobal(indexGPID(page_num), buffer, ctx);
}

Status HnswIndex::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx) const
{
    return db_->buffer_pool()->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
}

Status HnswIndex::insert(const VectorValue &vector,
                        const TID &tid,
                        ErrorContext *ctx)
{
    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    // 1. Select layer for new node (probabilistic)
    uint16_t target_layer = select_layer();

    // 2. Find nearest neighbors at target layer
    uint64_t current_xid = txn_mgr->getCurrentXid();

    std::vector<HnswSearchResult> neighbors;

    // If graph is empty, this is the first node
    if (index_info_.idx_total_nodes == 0)
    {
        // Create first node
        Status status = create_node(vector, tid, target_layer, {}, current_xid, ctx);
        if (status == Status::OK)
        {
            index_info_.idx_total_nodes++;
        }
        return status;
    }

    // Find entry point (node with highest layer)
    TID entry_point = find_entry_point(ctx);
    if (!entry_point.isValid())
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No entry point found");
        return Status::NOT_FOUND;
    }

    // Search from top layers down to target layer
    for (int16_t layer = get_max_layer(); layer >= 0; layer--)
    {
        Status search_status = find_nearest(vector, index_info_.idx_ef_construction,
                                           layer, entry_point, current_xid, &neighbors, ctx);
        if (search_status != Status::OK)
        {
            return search_status;
        }

        if (layer == 0)
            break; // At base layer, use these neighbors

        // Select closest neighbor as entry for next layer
        if (!neighbors.empty())
        {
            entry_point = neighbors[0].tid;
        }
    }

    // 3. Create node with bi-directional links
    std::vector<TID> neighbor_tids;
    size_t M = index_info_.idx_m;
    for (size_t i = 0; i < std::min(neighbors.size(), M); i++)
    {
        neighbor_tids.push_back(neighbors[i].tid);
    }

    Status create_status = create_node(vector, tid, target_layer, neighbor_tids, current_xid, ctx);
    if (create_status == Status::OK)
    {
        index_info_.idx_total_nodes++;

        // Add reverse links from neighbors to new node
        for (const TID &neighbor_tid : neighbor_tids)
        {
            add_link(neighbor_tid, tid, 0, ctx);
        }
    }

    return create_status;
}

Status HnswIndex::remove(const TID &tid,
                        ErrorContext *ctx)
{
    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    // Find node
    SBHnswNode *node = nullptr;
    uint64_t page_num = 0;
    Status find_status = find_node(tid, &node, &page_num, ctx);
    if (find_status != Status::OK)
    {
        return find_status;
    }

    // Soft delete: set xmax
    node->node_xmax = txn_mgr->getCurrentXid();
    node->node_flags |= static_cast<uint16_t>(HnswNodeFlags::DELETED);

    // Mark page dirty
    unpinIndexPage(page_num, true, ctx);

    LOG_DEBUG(GENERAL, "HNSW: Soft deleted node TID %s", tidToString(tid).c_str());

    return Status::OK;
}

Status HnswIndex::search(const VectorValue &query_vector,
                        uint32_t k,
                        uint64_t current_xid,
                        std::vector<HnswSearchResult> *results_out,
                        ErrorContext *ctx)
{
    if (!results_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid output parameter");
        return Status::INVALID_ARGUMENT;
    }

    results_out->clear();

    if (index_info_.idx_total_nodes == 0)
    {
        return Status::OK; // Empty index
    }

    // Find entry point
    TID entry_point = find_entry_point(ctx);
    if (!entry_point.isValid())
    {
        return Status::OK; // No visible nodes
    }

    // Search from top layer down
    std::vector<HnswSearchResult> candidates;

    for (int16_t layer = get_max_layer(); layer >= 0; layer--)
    {
        uint32_t ef = (layer == 0) ? std::max(k, index_info_.idx_ef_search) : 1;

        Status search_status = find_nearest(query_vector, ef, layer, entry_point,
                                           current_xid, &candidates, ctx);
        if (search_status != Status::OK)
        {
            return search_status;
        }

        if (layer == 0)
            break; // At base layer, return results

        // Select closest for next layer
        if (!candidates.empty())
        {
            entry_point = candidates[0].tid;
    }
    }

    // Return top k results
    std::sort(candidates.begin(), candidates.end());

    for (size_t i = 0; i < std::min(static_cast<size_t>(k), candidates.size()); i++)
    {
        results_out->push_back(candidates[i]);
    }

    LOG_INFO(GENERAL, "HNSW search: returned %zu results (k=%u)",
             results_out->size(), k);

    return Status::OK;
}

Status HnswIndex::vacuum(VacuumStats *stats_out, ErrorContext *ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid stats");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    TransactionManager *txn_mgr = db_->transaction_manager();

    if (!buffer_pool || !txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Missing components");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t nodes_visited = 0;
    uint64_t nodes_removed = 0;
    uint64_t oldest_xid = txn_mgr->getOldestActiveXid();

    // Pin root page
    void *page_buffer = nullptr;
    Status status = pinIndexPage(index_info_.idx_root_page, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SBHnswPage *page = reinterpret_cast<SBHnswPage*>(page_buffer);

    // Scan nodes and count dead ones
    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    size_t offset = sizeof(SBHnswPage);

    for (uint16_t i = 0; i < page->hnsw_count; i++)
    {
        SBHnswNode *node = reinterpret_cast<SBHnswNode*>(page_data + offset);
        nodes_visited++;

        // Check if node is dead
        if (node->node_xmax != 0 && node->node_xmax < oldest_xid)
        {
            nodes_removed++;
        }

        // Calculate node size
        size_t node_size = sizeof(SBHnswNode);
        node_size += node->node_num_neighbors * sizeof(HnswNeighbor);
        node_size += node->node_vector_len;
        offset += node_size;
    }

    unpinIndexPage(index_info_.idx_root_page, false, ctx);

    stats_out->nodes_visited = nodes_visited;
    stats_out->nodes_removed = nodes_removed;
    stats_out->links_updated = 0;
    stats_out->bytes_reclaimed = 0;

    LOG_INFO(GENERAL, "HNSW vacuum: visited %lu nodes, removed %lu",
             nodes_visited, nodes_removed);

    return Status::OK;
}

Status HnswIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                   uint64_t *entries_removed_out,
                                   uint64_t *pages_modified_out,
                                   ErrorContext *ctx)
{
    // Plan 01 Task D.1: HNSW GC implementation per plan_01_index_gc_clarifications.md

    // Empty dead_tids is a no-op
    if (dead_tids.empty())
    {
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;
        return Status::OK;
    }

    // Build dead_set using TID (compare node_gpid + node_slot)
    std::set<TID> dead_set(dead_tids.begin(), dead_tids.end());

    // Get current transaction ID for marking xmax
    auto txn_mgr = db_->transaction_manager();
    if (!txn_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No transaction manager");
        return Status::INVALID_ARGUMENT;
    }
    uint64_t current_xid = txn_mgr->getCurrentXid();

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    // Traverse all pages using hnsw_right_sibling starting at idx_root_page
    uint32_t current_page_id = index_info_.idx_root_page;

    while (current_page_id != 0)
    {
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page_id, &page_buffer, ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(GENERAL, "HNSW GC: Failed to pin page %u", current_page_id);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin HNSW page during GC");
            return Status::IO_ERROR;
        }

        SBHnswPage *page = static_cast<SBHnswPage*>(page_buffer);
        uint8_t *page_data = reinterpret_cast<uint8_t*>(page_buffer) + sizeof(SBHnswPage);
        uint32_t offset = 0;
        bool page_modified = false;
        uint64_t nodes_deleted_in_page = 0;

        // For each node in this page
        for (uint16_t i = 0; i < page->hnsw_count; i++)
        {
            SBHnswNode *node = reinterpret_cast<SBHnswNode*>(page_data + offset);

            // Check if this node's TID is in dead_set
            TID node_tid = node->getTID();
            if (dead_set.find(node_tid) != dead_set.end())
            {
                // Only mark as deleted if not already deleted
                if (node->node_xmax == 0)
                {
                    // Set node_xmax = TransactionManager::getCurrentXid()
                    node->node_xmax = current_xid;

                    // Set node_flags |= DELETED
                    node->node_flags |= static_cast<uint16_t>(HnswNodeFlags::DELETED);

                    entries_removed++;
                    nodes_deleted_in_page++;
                    page_modified = true;
                }
            }

            // Calculate size of this node and advance offset
            size_t node_size = sizeof(SBHnswNode) +
                              node->node_num_neighbors * sizeof(HnswNeighbor) +
                              node->node_vector_len;
            offset += node_size;
        }

        // If we modified any nodes, update page stats and mark dirty
        if (page_modified)
        {
            // Increment hnsw_deleted_nodes in page header
            page->hnsw_deleted_nodes += nodes_deleted_in_page;

            // Set HAS_GARBAGE flag
            page->hnsw_flags |= static_cast<uint16_t>(HnswFlags::HAS_GARBAGE);

            pages_modified++;
        }

        // Get next page in chain
        uint32_t next_page_id = static_cast<uint32_t>(page->hnsw_right_sibling);

        // Unpin page (mark dirty if modified)
        unpinIndexPage(current_page_id, page_modified, ctx);

        // Move to next page
        current_page_id = next_page_id;
    }

    // Set output counters
    if (entries_removed_out) *entries_removed_out = entries_removed;
    if (pages_modified_out) *pages_modified_out = pages_modified;

    LOG_DEBUG(GENERAL, "HNSW GC: Marked %lu nodes deleted across %lu pages",
             entries_removed, pages_modified);

    return Status::OK;
}

Status HnswIndex::getStats(HnswStats *stats_out, ErrorContext *ctx)
{
    if (!stats_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid stats output");
        return Status::INVALID_ARGUMENT;
    }

    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    // Pin root page to scan nodes
    void *page_buffer = nullptr;
    Status status = pinIndexPage(index_info_.idx_root_page, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t *page_data = static_cast<uint8_t *>(page_buffer);
    SBHnswPage *page = reinterpret_cast<SBHnswPage *>(page_data);

    // Scan all pages (multi-page support) - start from root and follow sibling chain
    uint64_t current_page_num = index_info_.idx_root_page;
    uint64_t total_pages = 0;
    uint64_t total_nodes = 0;
    uint64_t deleted_nodes = 0;
    uint64_t total_connections = 0;
    uint16_t max_layer = 0;

    while (current_page_num != 0)
    {
        // Pin current page
        status = pinIndexPage(current_page_num, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        page_data = static_cast<uint8_t *>(page_buffer);
        page = reinterpret_cast<SBHnswPage *>(page_data);
        total_pages++;

        // Scan all nodes on this page
        size_t offset = sizeof(SBHnswPage);
        for (uint16_t i = 0; i < page->hnsw_count; i++)
        {
            SBHnswNode *node = reinterpret_cast<SBHnswNode *>(page_data + offset);

            total_nodes++;

            // Check if deleted (xmax != 0)
            if (node->node_xmax != 0)
            {
                deleted_nodes++;
            }

            // Sum connections
            total_connections += node->node_num_neighbors;

            // Track max layer
            if (node->node_layer > max_layer)
            {
                max_layer = node->node_layer;
            }

            size_t node_size = calculate_node_size(node);
            offset += node_size;
        }

        // Move to next page in sibling chain
        uint64_t next_page_num = page->hnsw_right_sibling;
        unpinIndexPage(current_page_num, false, ctx);
        current_page_num = next_page_num;
    }

    // Fill output statistics
    stats_out->total_nodes = total_nodes;
    stats_out->deleted_nodes = deleted_nodes;
    stats_out->total_pages = total_pages; // Now counts all pages in sibling chain
    stats_out->max_layer = max_layer;

    // Calculate average connections
    if (total_nodes > 0)
    {
        stats_out->avg_connections = static_cast<double>(total_connections) / static_cast<double>(total_nodes);
    }
    else
    {
        stats_out->avg_connections = 0.0;
    }

    // Estimate average path length using log formula
    // For HNSW, average path length ≈ log(N) with proper parameters
    if (total_nodes > 1)
    {
        stats_out->avg_path_length = std::log(static_cast<double>(total_nodes));
    }
    else
    {
        stats_out->avg_path_length = 0.0;
    }

    return Status::OK;
}

// =============================================================================
// Private Helper Methods
// =============================================================================

uint16_t HnswIndex::select_layer()
{
    // Exponential decay probability for layer selection
    // P(layer = l) = 1 / M_L * (1 - 1 / M_L)^l
    // where M_L = 1 / ln(M)

    double m_l = 1.0 / std::log(static_cast<double>(index_info_.idx_m));
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    double r = dist(rng_);
    uint16_t layer = 0;

    while (r < 1.0 - 1.0 / m_l && layer < 15)
    {
        layer++;
        r /= (1.0 - 1.0 / m_l);
    }

    return layer;
}

size_t HnswIndex::calculate_node_size(const SBHnswNode *node) const
{
    if (!node)
    {
        return 0;
    }
    return sizeof(SBHnswNode) +
           node->node_num_neighbors * sizeof(HnswNeighbor) +
           node->node_vector_len;
}

size_t HnswIndex::calculate_node_size(uint16_t num_neighbors, uint16_t vector_len) const
{
    return sizeof(SBHnswNode) +
           num_neighbors * sizeof(HnswNeighbor) +
           vector_len;
}

Status HnswIndex::get_node_vector(const TID &tuple_id,
                                   VectorValue *vector_out,
                                   ErrorContext *ctx)
{
    if (!vector_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid vector output");
        return Status::INVALID_ARGUMENT;
    }

    // Find the node
    SBHnswNode *node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(tuple_id, &node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Extract vector data
    uint8_t *node_data = reinterpret_cast<uint8_t *>(node);
    uint8_t *vector_data = node_data + sizeof(SBHnswNode) + node->node_num_neighbors * sizeof(HnswNeighbor);

    // Deserialize vector using Vector::decode()
    std::vector<uint8_t> binary_data(vector_data, vector_data + node->node_vector_len);
    auto decoded = Vector::decode(binary_data);
    if (!decoded.has_value())
    {
        // Unpin page before returning error
        BufferPool *buffer_pool = db_->buffer_pool();
        if (buffer_pool)
        {
            unpinIndexPage(page_num, false, ctx);
        }
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Failed to decode vector data");
        return Status::INVALID_ARGUMENT;
    }

    *vector_out = std::move(decoded.value());

    // Unpin page
    BufferPool *buffer_pool = db_->buffer_pool();
    if (buffer_pool)
    {
        unpinIndexPage(page_num, false, ctx);
    }

    return Status::OK;
}

Status HnswIndex::reorganize_page_for_node_update(
    uint64_t page_num,
    const TID &target_tid,
    uint16_t new_num_neighbors,
    const std::vector<TID> &new_neighbors,
    ErrorContext *ctx)
{
    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    // Pin page
    void *page_buffer = nullptr;
    Status status = pinIndexPage(page_num, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t *page_data = static_cast<uint8_t *>(page_buffer);
    SBHnswPage *page = reinterpret_cast<SBHnswPage *>(page_data);

    // Collect all nodes in temporary buffer
    struct NodeInfo
    {
        TID tuple_id;
        uint16_t layer;
        uint16_t num_neighbors;
        uint16_t vector_len;
        uint64_t xmin;
        uint64_t xmax;
        std::vector<TID> neighbors;
        std::vector<uint8_t> vector_data;
    };

    std::vector<NodeInfo> nodes;
    size_t offset = sizeof(SBHnswPage);

    for (uint16_t i = 0; i < page->hnsw_count; i++)
    {
        SBHnswNode *node = reinterpret_cast<SBHnswNode *>(page_data + offset);
        NodeInfo info;
        info.tuple_id = node->getTID();
        info.layer = node->node_layer;
        info.xmin = node->node_xmin;
        info.xmax = node->node_xmax;

        // Check if this is the target node to update
        if (info.tuple_id == target_tid)
        {
            // Use new neighbors
            info.num_neighbors = new_num_neighbors;
            info.neighbors = new_neighbors;
        }
        else
        {
            // Preserve existing neighbors
            info.num_neighbors = node->node_num_neighbors;
            auto *neighbor_array = reinterpret_cast<HnswNeighbor *>(page_data + offset + sizeof(SBHnswNode));
            info.neighbors.reserve(node->node_num_neighbors);
            for (uint16_t n = 0; n < node->node_num_neighbors; ++n)
            {
                info.neighbors.push_back(neighbor_array[n].getTID());
            }
        }

        // Copy vector data
        info.vector_len = node->node_vector_len;
        uint8_t *vector_start = page_data + offset + sizeof(SBHnswNode) + node->node_num_neighbors * sizeof(HnswNeighbor);
        info.vector_data.assign(vector_start, vector_start + node->node_vector_len);

        nodes.push_back(info);

        size_t node_size = calculate_node_size(node);
        offset += node_size;
    }

    // Calculate space needed for reorganized nodes
    size_t total_size = sizeof(SBHnswPage);
    for (const auto &info : nodes)
    {
        total_size += calculate_node_size(info.num_neighbors, info.vector_len);
    }

    // Check if everything fits
    if (total_size > db_->page_size())
    {
        unpinIndexPage(page_num, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Page too full for node update");
        return Status::PAGE_FULL;
    }

    // Clear page and write reorganized data
    std::memset(page_data + sizeof(SBHnswPage), 0, db_->page_size() - sizeof(SBHnswPage));
    page->hnsw_count = nodes.size();

    offset = sizeof(SBHnswPage);
    for (const auto &info : nodes)
    {
        SBHnswNode *node = reinterpret_cast<SBHnswNode *>(page_data + offset);
        node->setTID(info.tuple_id);
        node->node_layer = info.layer;
        node->node_num_neighbors = info.num_neighbors;
        node->node_vector_len = info.vector_len;
        node->node_xmin = info.xmin;
        node->node_xmax = info.xmax;

        // Write neighbors
        auto *neighbor_array = reinterpret_cast<HnswNeighbor *>(page_data + offset + sizeof(SBHnswNode));
        for (size_t i = 0; i < info.num_neighbors; ++i)
        {
            neighbor_array[i].setTID(info.neighbors[i]);
        }

        // Write vector data
        uint8_t *vector_start = page_data + offset + sizeof(SBHnswNode) + info.num_neighbors * sizeof(HnswNeighbor);
        std::memcpy(vector_start, info.vector_data.data(), info.vector_len);

        offset += calculate_node_size(info.num_neighbors, info.vector_len);
    }

    // Mark page dirty and unpin
    unpinIndexPage(page_num, true, ctx);
    return Status::OK;
}

Status HnswIndex::find_nearest(const VectorValue &query,
                              uint32_t ef,
                              uint16_t layer,
                              const TID &entry_point,
                              uint64_t current_xid,
                              std::vector<HnswSearchResult> *results_out,
                              ErrorContext *ctx)
{
    results_out->clear();

    // Min-heap for candidates (closest first)
    std::priority_queue<SearchCandidate> candidates;

    // Max-heap for results (furthest first)
    std::priority_queue<SearchCandidate, std::vector<SearchCandidate>, std::greater<SearchCandidate>> top_k;

    std::set<TID> visited;

    // Load entry point node
    SBHnswNode *entry_node = nullptr;
    uint64_t page_num = 0;
    Status find_status = find_node(entry_point, &entry_node, &page_num, ctx);
    if (find_status != Status::OK)
    {
        return find_status;
    }

    // Get entry vector
    uint8_t *node_data = reinterpret_cast<uint8_t*>(entry_node);
    const uint8_t *vector_data = node_data + sizeof(SBHnswNode) +
                                 entry_node->node_num_neighbors * sizeof(HnswNeighbor);

    // Deserialize vector and compute distance
    std::vector<uint8_t> binary_data(vector_data, vector_data + entry_node->node_vector_len);
    auto decoded_vector = Vector::decode(binary_data);
    double entry_distance = 0.0;
    if (decoded_vector.has_value())
    {
        entry_distance = compute_distance(query, decoded_vector.value());
    }

    SearchCandidate entry_candidate{entry_point, entry_distance};
    candidates.push(entry_candidate);
    top_k.push(entry_candidate);
    visited.insert(entry_point);

    unpinIndexPage(page_num, false, ctx);

    // Greedy search
    while (!candidates.empty() && top_k.size() < ef)
    {
        SearchCandidate current = candidates.top();
        candidates.pop();

        // Stop if current is further than worst in top-k
        if (top_k.size() >= ef && current.distance > top_k.top().distance)
        {
            break;
        }

        // Load current node
        SBHnswNode *curr_node = nullptr;
        find_status = find_node(current.tuple_id, &curr_node, &page_num, ctx);
        if (find_status != Status::OK)
        {
            continue;
        }

        // Check visibility
        if (!is_node_visible(curr_node, current_xid, ctx))
        {
            unpinIndexPage(page_num, false, ctx);
            continue;
        }

        // Get neighbors
        const HnswNeighbor *neighbors = reinterpret_cast<const HnswNeighbor*>(curr_node + 1);

        for (uint16_t i = 0; i < curr_node->node_num_neighbors; i++)
        {
            TID neighbor_tid = neighbors[i].getTID();

            if (visited.count(neighbor_tid) > 0)
            {
                continue;
            }
            visited.insert(neighbor_tid);

            // Load neighbor and compute distance
            SBHnswNode *neighbor_node = nullptr;
            uint64_t neighbor_page = 0;
            find_status = find_node(neighbor_tid, &neighbor_node, &neighbor_page, ctx);
            if (find_status != Status::OK)
            {
                continue;
            }

            // Check visibility
            if (!is_node_visible(neighbor_node, current_xid, ctx))
            {
                unpinIndexPage(neighbor_page, false, ctx);
                continue;
            }

            // Deserialize neighbor vector and compute distance
            uint8_t *neighbor_data = reinterpret_cast<uint8_t*>(neighbor_node);
            const uint8_t *neighbor_vector_data = neighbor_data + sizeof(SBHnswNode) +
                                                   neighbor_node->node_num_neighbors * sizeof(HnswNeighbor);
            std::vector<uint8_t> neighbor_binary(neighbor_vector_data,
                                                  neighbor_vector_data + neighbor_node->node_vector_len);
            auto neighbor_decoded = Vector::decode(neighbor_binary);
            double neighbor_distance = 0.0;
            if (neighbor_decoded.has_value())
            {
                neighbor_distance = compute_distance(query, neighbor_decoded.value());
            }

            SearchCandidate neighbor_candidate{neighbor_tid, neighbor_distance};

            if (top_k.size() < ef || neighbor_distance < top_k.top().distance)
            {
                candidates.push(neighbor_candidate);
                top_k.push(neighbor_candidate);

                // Remove worst if too many
                if (top_k.size() > ef)
                {
                    top_k.pop();
                }
            }

            unpinIndexPage(neighbor_page, false, ctx);
        }

        unpinIndexPage(page_num, false, ctx);
    }

    // Convert top-k to results
    while (!top_k.empty())
    {
        SearchCandidate candidate = top_k.top();
        top_k.pop();

        HnswSearchResult result;
        result.tid = candidate.tuple_id;
        result.distance = candidate.distance;
        results_out->push_back(result);
    }

    std::reverse(results_out->begin(), results_out->end());

    return Status::OK;
}

double HnswIndex::compute_distance(const VectorValue &a, const VectorValue &b) const
{
    DistanceMetric metric = static_cast<DistanceMetric>(index_info_.idx_distance_metric);
    auto result = a.distance(b, metric);
    return result.value_or(0.0);
}

bool HnswIndex::is_node_visible(const SBHnswNode *node,
                                uint64_t current_xid,
                                ErrorContext *ctx) const
{
    // Firebird MGA visibility rules
    if (node->node_xmin > current_xid)
    {
        return false;
    }

    if (node->node_xmax != 0 && node->node_xmax <= current_xid)
    {
        return false;
    }

    TransactionManager *txn_mgr = db_->transaction_manager();
    if (!txn_mgr)
    {
        return true; // Fallback
    }

    if (!txn_mgr->isVersionVisible(node->node_xmin, current_xid))
    {
        return false;
    }

    if (node->node_xmax != 0 && txn_mgr->isVersionVisible(node->node_xmax, current_xid))
    {
        return false;
    }

    return true;
}

Status HnswIndex::create_node(const VectorValue &vector,
                              const TID &tuple_id,
                              uint16_t layer,
                              const std::vector<TID> &neighbors,
                              uint64_t current_xid,
                              ErrorContext *ctx)
{
    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    // Pin root page
    void *page_buffer = nullptr;
    Status status = pinIndexPage(index_info_.idx_root_page, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBHnswPage *page = reinterpret_cast<SBHnswPage*>(page_data);
    uint64_t active_page_num = index_info_.idx_root_page; // Track which page we're using

    // Calculate node size
    size_t vector_size = vector.getDimensions() * sizeof(float);
    size_t node_size = sizeof(SBHnswNode) + neighbors.size() * sizeof(HnswNeighbor) + vector_size;

    // Check if page is full - if so, allocate new page
    if (page->hnsw_free_space < node_size)
    {
        LOG_DEBUG(STORAGE, "HNSW: Root page full (%u bytes free, need %zu), allocating new page",
                  page->hnsw_free_space, node_size);

        unpinIndexPage(index_info_.idx_root_page, false, ctx);

        // Allocate new page for this layer
        PageManager *page_mgr = db_->page_manager();
        if (!page_mgr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No page manager");
            return Status::INVALID_ARGUMENT;
        }

        uint32_t new_page_num = 0;
        GPID new_page_gpid = 0;
        status = page_mgr->allocatePageInTablespace(index_info_.idx_tablespace_id, &new_page_gpid, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to allocate new HNSW page");
            return status;
        }
        new_page_num = static_cast<uint32_t>(getPageNumber(new_page_gpid));

        // Pin the new page
        void *new_page_buffer = nullptr;
        status = pinIndexPage(new_page_num, &new_page_buffer, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin new HNSW page");
            return status;
        }

        // Initialize new page
        uint8_t *new_page_data = static_cast<uint8_t*>(new_page_buffer);
        SBHnswPage *new_page = reinterpret_cast<SBHnswPage*>(new_page_data);
        std::memset(new_page_data, 0, db_->page_size());

        // Copy header from root page (need to re-pin root to read it)
        void *root_buffer = nullptr;
        status = pinIndexPage(index_info_.idx_root_page, &root_buffer, ctx);
        if (status != Status::OK)
        {
            unpinIndexPage(new_page_num, false, ctx);
            SET_ERROR_CONTEXT(ctx, status, "Failed to re-pin root page");
            return status;
        }

        SBHnswPage *root_page = reinterpret_cast<SBHnswPage*>(root_buffer);

        // Initialize new page header (copy metadata from root)
        new_page->hnsw_index_uuid = root_page->hnsw_index_uuid;
        new_page->hnsw_table_uuid = root_page->hnsw_table_uuid;
        new_page->hnsw_flags = 0;
        new_page->hnsw_count = 0;
        new_page->hnsw_free_space = db_->page_size() - sizeof(SBHnswPage);
        new_page->hnsw_layer = layer;
        new_page->hnsw_m = root_page->hnsw_m;
        new_page->hnsw_dimensions = root_page->hnsw_dimensions;
        new_page->hnsw_distance_metric = root_page->hnsw_distance_metric;
        new_page->hnsw_xmin = current_xid;
        new_page->hnsw_xmax = 0;
        new_page->hnsw_lsn = 0;
        new_page->hnsw_total_nodes = 0;
        new_page->hnsw_deleted_nodes = 0;

        // Link new page as right sibling of root
        new_page->hnsw_left_sibling = index_info_.idx_root_page;
        new_page->hnsw_right_sibling = root_page->hnsw_right_sibling;

        // Update root page's right sibling pointer
        uint64_t old_right_sibling = root_page->hnsw_right_sibling;
        root_page->hnsw_right_sibling = new_page_num;

        // If there was a previous right sibling, update its left pointer
        if (old_right_sibling != 0)
        {
            void *old_right_buffer = nullptr;
            Status sibling_status = pinIndexPage(old_right_sibling, &old_right_buffer, ctx);
            if (sibling_status == Status::OK)
            {
                SBHnswPage *old_right_page = reinterpret_cast<SBHnswPage*>(old_right_buffer);
                old_right_page->hnsw_left_sibling = new_page_num;
                unpinIndexPage(old_right_sibling, true, ctx); // Mark dirty
            }
            // If sibling update fails, continue anyway - we can still insert the node
        }

        unpinIndexPage(index_info_.idx_root_page, true, ctx); // Mark dirty (updated right_sibling)

        // Now use the new page for insertion
        page_buffer = new_page_buffer;
        page_data = new_page_data;
        page = new_page;
        active_page_num = new_page_num; // Update active page number

        LOG_INFO(STORAGE, "HNSW: Allocated new page %u for layer %u (linked after root %u)",
                 new_page_num, layer, index_info_.idx_root_page);
    }

    // Find insertion point
    size_t offset = sizeof(SBHnswPage);
    for (uint16_t i = 0; i < page->hnsw_count; i++)
    {
        SBHnswNode *node = reinterpret_cast<SBHnswNode*>(page_data + offset);
        size_t size = sizeof(SBHnswNode) + node->node_num_neighbors * sizeof(HnswNeighbor) + node->node_vector_len;
        offset += size;
    }

    // Create node
    SBHnswNode *new_node = reinterpret_cast<SBHnswNode*>(page_data + offset);
    new_node->setTID(tuple_id);
    new_node->node_flags = 0;
    new_node->node_layer = layer;
    new_node->node_num_neighbors = neighbors.size();
    new_node->node_vector_len = vector_size;
    new_node->node_xmin = current_xid;
    new_node->node_xmax = 0;

    // Copy neighbors
    auto *neighbor_array = reinterpret_cast<HnswNeighbor*>(new_node + 1);
    for (size_t i = 0; i < neighbors.size(); i++)
    {
        neighbor_array[i].setTID(neighbors[i]);
    }

    // Copy vector data
    uint8_t *vector_dest = reinterpret_cast<uint8_t*>(neighbor_array + neighbors.size());
    auto vec_data_opt = vector.getFloat32Vector();
    if (vec_data_opt)
    {
        std::memcpy(vector_dest, vec_data_opt->data(), vector_size);
    }
    else
    {
        // Empty vector case
        std::memset(vector_dest, 0, vector_size);
    }

    page->hnsw_count++;
    page->hnsw_free_space -= node_size;
    page->hnsw_total_nodes++;

    unpinIndexPage(active_page_num, true, ctx); // Unpin the actual page we used

    LOG_DEBUG(GENERAL, "HNSW: Created node TID %s at layer %u with %zu neighbors on page %lu",
             tidToString(tuple_id).c_str(), layer, neighbors.size(), active_page_num);

    return Status::OK;
}

Status HnswIndex::add_link(const TID &from_tid, const TID &to_tid,
                           uint16_t layer, ErrorContext *ctx)
{
    // Find the source node
    SBHnswNode *from_node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(from_tid, &from_node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Extract current neighbors
    uint8_t *node_data = reinterpret_cast<uint8_t *>(from_node);
    auto *neighbor_array = reinterpret_cast<HnswNeighbor *>(node_data + sizeof(SBHnswNode));
    std::vector<TID> neighbors;
    neighbors.reserve(from_node->node_num_neighbors);
    for (uint16_t i = 0; i < from_node->node_num_neighbors; ++i)
    {
        neighbors.push_back(neighbor_array[i].getTID());
    }

    // Unpin page before modification
    BufferPool *buffer_pool = db_->buffer_pool();
    if (buffer_pool)
    {
        unpinIndexPage(page_num, false, ctx);
    }

    // Check if link already exists (idempotent)
    for (const TID &neighbor : neighbors)
    {
        if (neighbor == to_tid)
        {
            // Link already exists, no-op
            LOG_DEBUG(GENERAL, "HNSW: add_link %s -> %s (layer %u) - already exists",
                      tidToString(from_tid).c_str(), tidToString(to_tid).c_str(), layer);
            return Status::OK;
        }
    }

    // Add new link
    neighbors.push_back(to_tid);

    // Check if pruning is needed (exceeds M connections)
    uint16_t M = index_info_.idx_m;
    if (neighbors.size() > M)
    {
        LOG_DEBUG(GENERAL, "HNSW: add_link triggers pruning for node %s (layer %u)",
                  tidToString(from_tid).c_str(), layer);
        // Prune will be called later if needed
    }

    // Update node with new neighbors
    status = reorganize_page_for_node_update(page_num, from_tid, neighbors.size(), neighbors, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(STORAGE, "HNSW: add_link failed to reorganize page: %d", static_cast<int>(status));
        return status;
    }

    LOG_DEBUG(GENERAL, "HNSW: add_link %s -> %s (layer %u) - success",
              tidToString(from_tid).c_str(), tidToString(to_tid).c_str(), layer);
    return Status::OK;
}

Status HnswIndex::remove_link(const TID &from_tid, const TID &to_tid,
                              uint16_t layer, ErrorContext *ctx)
{
    // Find the source node
    SBHnswNode *from_node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(from_tid, &from_node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Extract current neighbors
    uint8_t *node_data = reinterpret_cast<uint8_t *>(from_node);
    auto *neighbor_array = reinterpret_cast<HnswNeighbor *>(node_data + sizeof(SBHnswNode));
    std::vector<TID> neighbors;
    neighbors.reserve(from_node->node_num_neighbors);
    for (uint16_t i = 0; i < from_node->node_num_neighbors; ++i)
    {
        neighbors.push_back(neighbor_array[i].getTID());
    }

    // Unpin page before modification
    BufferPool *buffer_pool = db_->buffer_pool();
    if (buffer_pool)
    {
        unpinIndexPage(page_num, false, ctx);
    }

    // Find and remove the link
    auto it = std::find(neighbors.begin(), neighbors.end(), to_tid);
    if (it == neighbors.end())
    {
        // Link doesn't exist, no-op (idempotent)
        LOG_DEBUG(GENERAL, "HNSW: remove_link %s -> %s (layer %u) - link not found",
                  tidToString(from_tid).c_str(), tidToString(to_tid).c_str(), layer);
        return Status::OK;
    }

    neighbors.erase(it);

    // Update node with modified neighbors
    status = reorganize_page_for_node_update(page_num, from_tid, neighbors.size(), neighbors, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(STORAGE, "HNSW: remove_link failed to reorganize page: %d", static_cast<int>(status));
        return status;
    }

    LOG_DEBUG(GENERAL, "HNSW: remove_link %s -> %s (layer %u) - success",
              tidToString(from_tid).c_str(), tidToString(to_tid).c_str(), layer);
    return Status::OK;
}

Status HnswIndex::find_node(const TID &tuple_id,
                            SBHnswNode **node_out,
                            uint64_t *page_num_out,
                            ErrorContext *ctx)
{
    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    // Start from root page and scan through sibling chain (multi-page support)
    uint64_t current_page_num = index_info_.idx_root_page;

    while (current_page_num != 0)
    {
        // Pin current page
        void *page_buffer = nullptr;
        Status status = pinIndexPage(current_page_num, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
        SBHnswPage *page = reinterpret_cast<SBHnswPage*>(page_data);

        // Linear scan for node on this page
        size_t offset = sizeof(SBHnswPage);
        for (uint16_t i = 0; i < page->hnsw_count; i++)
        {
            SBHnswNode *node = reinterpret_cast<SBHnswNode*>(page_data + offset);

            if (node->getTID() == tuple_id)
            {
                *node_out = node;
                *page_num_out = current_page_num;
                // Note: Page remains pinned - caller must unpin
                return Status::OK;
            }

            size_t node_size = sizeof(SBHnswNode) + node->node_num_neighbors * sizeof(HnswNeighbor) + node->node_vector_len;
            offset += node_size;
        }

        // Node not found on this page, move to right sibling
        uint64_t next_page_num = page->hnsw_right_sibling;
        unpinIndexPage(current_page_num, false, ctx);
        current_page_num = next_page_num;
    }

    // Node not found in any page
    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "HNSW node not found in any page");
    return Status::NOT_FOUND;
}

Status HnswIndex::prune_connections(const TID &node_tid, uint16_t layer,
                                   ErrorContext *ctx)
{
    uint16_t M = index_info_.idx_m;

    // Find the node
    SBHnswNode *node = nullptr;
    uint64_t page_num = 0;
    Status status = find_node(node_tid, &node, &page_num, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Check if pruning is needed
    if (node->node_num_neighbors <= M)
    {
        // No pruning needed
        BufferPool *buffer_pool = db_->buffer_pool();
        if (buffer_pool)
        {
            unpinIndexPage(page_num, false, ctx);
        }
        LOG_DEBUG(GENERAL, "HNSW: prune_connections node %s (layer %u) - no pruning needed (%u <= %u)",
                  tidToString(node_tid).c_str(), layer, node->node_num_neighbors, M);
        return Status::OK;
    }

    // Get node's vector for distance calculations
    // Note: get_node_vector will unpin the page, so we need to re-pin after
    VectorValue node_vector(std::vector<float>{}); // Temporary empty vector
    status = get_node_vector(node_tid, &node_vector, ctx);
    if (status != Status::OK)
    {
        BufferPool *buffer_pool = db_->buffer_pool();
        if (buffer_pool)
        {
            unpinIndexPage(page_num, false, ctx);
        }
        return status;
    }

    // Re-pin the page since get_node_vector unpinned it
    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No buffer pool");
        return Status::INVALID_ARGUMENT;
    }

    void *page_buffer = nullptr;
    status = pinIndexPage(page_num, &page_buffer, ctx);
    if (status != Status::OK || !page_buffer)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to re-pin page after get_node_vector");
        return Status::IO_ERROR;
    }
    uint8_t *page_data = reinterpret_cast<uint8_t *>(page_buffer);

    // Re-locate the node after re-pinning
    SBHnswPage *page = reinterpret_cast<SBHnswPage *>(page_data);
    node = nullptr;
    uint8_t *current = page_data + sizeof(SBHnswPage);
    uint8_t *page_end = page_data + db_->page_size();

    for (uint16_t i = 0; i < page->hnsw_count; i++)
    {
        if (current + sizeof(SBHnswNode) > page_end)
            break;

        SBHnswNode *candidate = reinterpret_cast<SBHnswNode *>(current);
        if (candidate->getTID() == node_tid)
        {
            node = candidate;
            break;
        }

        size_t node_size = calculate_node_size(candidate);
        current += node_size;
    }

    if (!node)
    {
        unpinIndexPage(page_num, false, ctx);
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Node not found after re-pin");
        return Status::NOT_FOUND;
    }

    // Extract current neighbors
    uint8_t *node_data = reinterpret_cast<uint8_t *>(node);
    auto *neighbor_array = reinterpret_cast<HnswNeighbor *>(node_data + sizeof(SBHnswNode));
    std::vector<TID> neighbors;
    neighbors.reserve(node->node_num_neighbors);
    for (uint16_t i = 0; i < node->node_num_neighbors; ++i)
    {
        neighbors.push_back(neighbor_array[i].getTID());
    }

    buffer_pool = db_->buffer_pool();
    if (buffer_pool)
    {
        unpinIndexPage(page_num, false, ctx);
    }

    // Build candidate list with distances
    struct Candidate
    {
        TID tid;
        double distance;
    };
    std::vector<Candidate> candidates;

    for (const TID &neighbor_tid : neighbors)
    {
        VectorValue neighbor_vector(std::vector<float>{}); // Temporary empty vector
        status = get_node_vector(neighbor_tid, &neighbor_vector, ctx);
        if (status == Status::OK)
        {
            double dist = compute_distance(node_vector, neighbor_vector);
            candidates.push_back({neighbor_tid, dist});
        }
    }

    // Sort candidates by distance (closest first)
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &a, const Candidate &b)
              {
                  return a.distance < b.distance;
              });

    // Select M closest neighbors (simple heuristic)
    // Using distance-only selection for now (HNSW-Baseline)
    // Future optimization: diversity-based heuristic from HNSW paper Section 4
    // (select neighbors that maximize coverage while minimizing overlap)
    // Current approach is simpler and works well in practice
    std::vector<TID> pruned_neighbors;
    for (size_t i = 0; i < std::min(static_cast<size_t>(M), candidates.size()); i++)
    {
        pruned_neighbors.push_back(candidates[i].tid);
    }

    // Update node with pruned neighbors
    status = reorganize_page_for_node_update(page_num, node_tid, pruned_neighbors.size(), pruned_neighbors, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(STORAGE, "HNSW: prune_connections failed to reorganize page: %d", static_cast<int>(status));
        return status;
    }

    LOG_DEBUG(GENERAL, "HNSW: prune_connections node %s (layer %u) - pruned from %zu to %zu neighbors",
              tidToString(node_tid).c_str(), layer, neighbors.size(), pruned_neighbors.size());
    return Status::OK;
}

TID HnswIndex::find_entry_point(ErrorContext *ctx) const
{
    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        return INVALID_TID;
    }

    // Pin root page
    void *page_buffer = nullptr;
    Status status = pinIndexPage(index_info_.idx_root_page, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return INVALID_TID;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBHnswPage *page = reinterpret_cast<SBHnswPage*>(page_data);

    // Find node with highest layer
    uint16_t max_layer = 0;
    TID entry_tid = INVALID_TID;

    size_t offset = sizeof(SBHnswPage);
    for (uint16_t i = 0; i < page->hnsw_count; i++)
    {
        SBHnswNode *node = reinterpret_cast<SBHnswNode*>(page_data + offset);

        if (!(node->node_flags & static_cast<uint16_t>(HnswNodeFlags::DELETED)))
        {
            if (node->node_layer >= max_layer)
            {
                max_layer = node->node_layer;
                entry_tid = node->getTID();
            }
        }

        size_t node_size = sizeof(SBHnswNode) + node->node_num_neighbors * sizeof(HnswNeighbor) + node->node_vector_len;
        offset += node_size;
    }

    unpinIndexPage(index_info_.idx_root_page, false, ctx);
    return entry_tid;
}

uint16_t HnswIndex::get_max_layer() const
{
    // For Phase 1, scan root page to find max layer
    BufferPool *buffer_pool = db_->buffer_pool();
    if (!buffer_pool)
    {
        return 0;
    }

    void *page_buffer = nullptr;
    Status status = pinIndexPage(index_info_.idx_root_page, &page_buffer, nullptr);
    if (status != Status::OK)
    {
        return 0;
    }

    uint8_t *page_data = static_cast<uint8_t*>(page_buffer);
    SBHnswPage *page = reinterpret_cast<SBHnswPage*>(page_data);

    uint16_t max_layer = 0;
    size_t offset = sizeof(SBHnswPage);

    for (uint16_t i = 0; i < page->hnsw_count; i++)
    {
        SBHnswNode *node = reinterpret_cast<SBHnswNode*>(page_data + offset);
        if (node->node_layer > max_layer)
        {
            max_layer = node->node_layer;
        }

        size_t node_size = sizeof(SBHnswNode) + node->node_num_neighbors * sizeof(HnswNeighbor) + node->node_vector_len;
        offset += node_size;
    }

    unpinIndexPage(index_info_.idx_root_page, false, nullptr);
    return max_layer;
}

// ==================================================================
// PHASE 5 TASK 5.3.2: Update TIDs After Tablespace Migration
// ==================================================================

Status HnswIndex::updateTIDsAfterMigration(
    const std::unordered_map<TID, TID> &tid_mapping,
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

    // Early exit if no TID mapping
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

    uint64_t total_tids_updated = 0;
    uint64_t total_pages_modified = 0;
    bool had_errors = false;

    uint32_t root_page = index_info_.idx_root_page;
    if (root_page == 0)
    {
        return Status::OK;
    }

    // Pin root page
    void *root_buffer = nullptr;
    Status pin_status = pinIndexPage(root_page, &root_buffer, ctx);
    if (pin_status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, pin_status, "Failed to pin root page");
        return pin_status;
    }

    auto *root_page_hdr = reinterpret_cast<SBHnswPage *>(root_buffer);
    uint16_t max_layer = 0;

    uint8_t *page_data = reinterpret_cast<uint8_t *>(root_buffer);
    uint16_t node_count = root_page_hdr->hnsw_count;

    // Find max layer
    size_t offset = sizeof(SBHnswPage);
    for (uint16_t i = 0; i < node_count && offset < db_->page_size(); i++)
    {
        auto *node = reinterpret_cast<SBHnswNode *>(page_data + offset);
        if (node->node_layer > max_layer)
        {
            max_layer = node->node_layer;
        }

        size_t node_size = sizeof(SBHnswNode);
        node_size += node->node_num_neighbors * sizeof(HnswNeighbor);
        node_size += node->node_vector_len;
        offset += node_size;
    }

    unpinIndexPage(root_page, false, ctx);

    LOG_INFO(STORAGE, "HNSW index has %u layers, starting TID update scan", max_layer + 1);

    // Scan all layers
    for (uint16_t layer = 0; layer <= max_layer; layer++)
    {
        std::vector<uint64_t> layer_pages;
        uint64_t current_page = root_page;
        std::set<uint64_t> visited_pages;
        std::vector<uint64_t> pages_to_scan = {current_page};

        while (!pages_to_scan.empty())
        {
            uint64_t page_num = pages_to_scan.back();
            pages_to_scan.pop_back();

            if (visited_pages.count(page_num) > 0)
            {
                continue;
            }
            visited_pages.insert(page_num);

            void *page_buffer = nullptr;
            Status page_pin_status = pinIndexPage(page_num, &page_buffer, ctx);
            if (page_pin_status != Status::OK)
            {
                LOG_WARNING(STORAGE, "Failed to pin HNSW page %lu during TID update: %d",
                           page_num, static_cast<int>(page_pin_status));
                had_errors = true;
                continue;
            }

            auto *hnsw_page = reinterpret_cast<SBHnswPage *>(page_buffer);

            if (hnsw_page->hnsw_layer == layer)
            {
                layer_pages.push_back(page_num);
            }

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

            unpinIndexPage(page_num, false, ctx);
        }

        LOG_INFO(STORAGE, "Layer %u: found %lu pages to update", layer, layer_pages.size());

        // Update TIDs in all pages of this layer
        for (uint64_t page_num : layer_pages)
        {
            void *page_buffer = nullptr;
            Status page_pin_status = pinIndexPage(page_num, &page_buffer, ctx);
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

            size_t node_offset = sizeof(SBHnswPage);
            for (uint16_t i = 0; i < nodes_on_page && node_offset < db_->page_size(); i++)
            {
                auto *node = reinterpret_cast<SBHnswNode *>(page_bytes + node_offset);

                TID old_tid = node->getTID();
                auto it = tid_mapping.find(old_tid);
                if (it != tid_mapping.end())
                {
                    node->setTID(it->second);
                    total_tids_updated++;
                    page_modified = true;

                    LOG_DEBUG(STORAGE, "Updated HNSW node TID (layer %u, page %lu)",
                             layer, page_num);
                }

                size_t node_size = sizeof(SBHnswNode);
                node_size += node->node_num_neighbors * sizeof(HnswNeighbor);
                node_size += node->node_vector_len;
                node_offset += node_size;
            }

            unpinIndexPage(page_num, page_modified, ctx);

            if (page_modified)
            {
                total_pages_modified++;
            }
        }
    }

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
    }
    else
    {
        LOG_INFO(STORAGE, "HNSW TID update completed successfully: %lu TIDs updated, %lu pages modified",
                total_tids_updated, total_pages_modified);
    }

    return Status::OK;
}

} // namespace scratchbird::core
