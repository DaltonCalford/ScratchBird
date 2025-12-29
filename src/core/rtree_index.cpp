// =================================================================================================
// ScratchBird Database Engine
// R-Tree Index Wrapper Implementation
// November 20, 2025
//
// This file provides a thin wrapper around the real RTree implementation (rtree.cpp).
// It handles serialization/deserialization of bounding boxes and delegates to RTree.
// =================================================================================================

#include "scratchbird/core/rtree_index.h"
#include "scratchbird/core/rtree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"
#include <cstring>

namespace scratchbird {
namespace core {

    RTreeIndex::RTreeIndex(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page)
        : db_(db), index_uuid_(index_uuid), meta_page_(meta_page), root_page_(0), rtree_(nullptr)
    {
    }

    RTreeIndex::~RTreeIndex() = default;

    Status RTreeIndex::create(Database *db, const UuidV7Bytes &index_uuid,
                              uint32_t *meta_page_out, ErrorContext *ctx)
    {
        // Generate dummy UUIDs for table and columns (wrapper doesn't track these)
        UuidV7Bytes table_uuid = generateUuidV7();
        std::vector<UuidV7Bytes> column_uuids = {generateUuidV7()};

        // Delegate to low-level RTree::create with default max_entries
        uint32_t root_page = 0;
        Status status = RTree::create(db, index_uuid, table_uuid, column_uuids,
                                     MAX_ENTRIES, &root_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        if (meta_page_out)
        {
            *meta_page_out = root_page;
        }

        return Status::OK;
    }

    std::unique_ptr<RTreeIndex> RTreeIndex::open(Database *db, const UuidV7Bytes &index_uuid,
                                                  uint32_t meta_page, ErrorContext *ctx)
    {
        auto index = std::make_unique<RTreeIndex>(db, index_uuid, meta_page);

        // Delegate to low-level RTree::open
        index->root_page_ = meta_page; // Root page is the meta page
        index->rtree_ = RTree::open(db, index_uuid, meta_page, MAX_ENTRIES, ctx);
        if (!index->rtree_)
        {
            return nullptr;
        }

        return index;
    }

    Status RTreeIndex::insert(const std::vector<uint8_t> &key, const TID &tid,
                              uint64_t xmin, ErrorContext *ctx)
    {
        if (!rtree_)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "R-Tree index not opened");
            return Status::NOT_FOUND;
        }

        // Parse bounding box from serialized key
        scratchbird::core::BoundingBox bbox;
        Status status = deserializeBoundingBox(key, &bbox, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Delegate to real RTree implementation
        return rtree_->insert(bbox, tid, xmin, ctx);
    }

    Status RTreeIndex::search(const std::vector<uint8_t> &query_box, uint64_t current_xid,
                              std::vector<TID> *results_out, ErrorContext *ctx)
    {
        if (!rtree_)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "R-Tree index not opened");
            return Status::NOT_FOUND;
        }

        if (results_out)
        {
            results_out->clear();
        }

        // Parse query bounding box
        scratchbird::core::BoundingBox query;
        Status status = deserializeBoundingBox(query_box, &query, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Delegate to real RTree implementation
        return rtree_->search(query, current_xid, results_out, ctx);
    }

    Status RTreeIndex::remove(const std::vector<uint8_t> &key, const TID &tid,
                              uint64_t xmax, ErrorContext *ctx)
    {
        if (!rtree_)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "R-Tree index not opened");
            return Status::NOT_FOUND;
        }

        // Parse bounding box
        scratchbird::core::BoundingBox bbox;
        Status status = deserializeBoundingBox(key, &bbox, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Delegate to real RTree implementation
        // Note: RTree::remove takes current_xid, not xmax, so we pass xmax as current_xid
        // This is for MGA logical deletion where xmax marks when the entry was deleted
        return rtree_->remove(bbox, tid, xmax, ctx);
    }

    Status RTreeIndex::vacuum(ErrorContext *ctx)
    {
        if (!rtree_)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "R-Tree index not opened");
            return Status::NOT_FOUND;
        }

        // Delegate to RTree clear (vacuum equivalent)
        return rtree_->clear(ctx);
    }

    Status RTreeIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                         uint64_t *entries_removed_out,
                                         uint64_t *pages_modified_out,
                                         ErrorContext *ctx)
    {
        if (!rtree_)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "R-Tree index not opened");
            return Status::NOT_FOUND;
        }

        // Initialize outputs
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;

        if (dead_tids.empty())
        {
            return Status::OK;
        }

        // Delegate to real RTree implementation (IndexGCInterface)
        return rtree_->removeDeadEntries(dead_tids, entries_removed_out, pages_modified_out, ctx);
    }

    // ========================================================================
    // Helper Methods - Bounding Box Serialization
    // ========================================================================

    Status RTreeIndex::deserializeBoundingBox(const std::vector<uint8_t>& key,
                                              scratchbird::core::BoundingBox* bbox,
                                              ErrorContext* ctx)
    {
        // Expect 4 doubles: min_x, min_y, max_x, max_y
        if (key.size() != 32) // 4 * sizeof(double)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid bounding box size");
            return Status::INVALID_ARGUMENT;
        }

        const double* coords = reinterpret_cast<const double*>(key.data());
        bbox->min_x = coords[0];
        bbox->min_y = coords[1];
        bbox->max_x = coords[2];
        bbox->max_y = coords[3];

        // Validate
        if (bbox->min_x > bbox->max_x || bbox->min_y > bbox->max_y)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid bounding box coordinates");
            return Status::INVALID_ARGUMENT;
        }

        return Status::OK;
    }

    std::vector<uint8_t> RTreeIndex::serializeBoundingBox(const scratchbird::core::BoundingBox& bbox)
    {
        std::vector<uint8_t> result(32); // 4 doubles
        double* coords = reinterpret_cast<double*>(result.data());
        coords[0] = bbox.min_x;
        coords[1] = bbox.min_y;
        coords[2] = bbox.max_x;
        coords[3] = bbox.max_y;
        return result;
    }

} // namespace core
} // namespace scratchbird
