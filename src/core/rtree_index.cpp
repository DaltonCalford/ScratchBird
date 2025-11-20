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
        // Allocate meta page for storing index metadata
        auto page_mgr = db->page_manager();
        uint32_t meta_page = page_mgr->allocatePage();

        // The real RTree will be created lazily on first use
        // or we could delegate to RTree::create() here

        if (meta_page_out)
        {
            *meta_page_out = meta_page;
        }

        return Status::OK;
    }

    std::unique_ptr<RTreeIndex> RTreeIndex::open(Database *db, const UuidV7Bytes &index_uuid,
                                                  uint32_t meta_page, ErrorContext *ctx)
    {
        auto index = std::make_unique<RTreeIndex>(db, index_uuid, meta_page);

        // Load root page from metadata
        // The real RTree will be initialized lazily
        index->root_page_ = meta_page + 1; // Placeholder - real impl would load from meta page

        return index;
    }

    Status RTreeIndex::insert(const std::vector<uint8_t> &key, const TID &tid,
                              uint64_t xmin, ErrorContext *ctx)
    {
        // Parse bounding box from serialized key
        BoundingBox bbox;
        Status status = deserializeBoundingBox(key, &bbox, ctx);
        if (!status.ok())
        {
            return status;
        }

        // Lazy initialization of RTree
        if (!rtree_)
        {
            // Build SBRTreeIndex metadata structure
            SBRTreeIndex index_info;
            std::memcpy(&index_info.idx_uuid, &index_uuid_, sizeof(ID));
            // Note: idx_table_uuid and idx_column_ids would need to be loaded from meta page
            // For now, using defaults
            index_info.idx_root_page = root_page_;
            index_info.idx_max_entries = MAX_ENTRIES;
            index_info.idx_height = 0;
            index_info.idx_entry_count = 0;
            index_info.idx_page_count = 0;
            index_info.idx_deleted_count = 0;

            rtree_ = std::make_unique<RTree>(db_, index_info);
        }

        // Delegate to real RTree implementation
        return rtree_->insert(bbox, tid, xmin, ctx);
    }

    Status RTreeIndex::search(const std::vector<uint8_t> &query_box, uint64_t current_xid,
                              std::vector<TID> *results_out, ErrorContext *ctx)
    {
        if (results_out)
        {
            results_out->clear();
        }

        // Parse query bounding box
        BoundingBox query;
        Status status = deserializeBoundingBox(query_box, &query, ctx);
        if (!status.ok())
        {
            return status;
        }

        // Lazy initialization of RTree
        if (!rtree_)
        {
            SBRTreeIndex index_info;
            std::memcpy(&index_info.idx_uuid, &index_uuid_, sizeof(ID));
            index_info.idx_root_page = root_page_;
            index_info.idx_max_entries = MAX_ENTRIES;
            index_info.idx_height = 0;
            index_info.idx_entry_count = 0;
            index_info.idx_page_count = 0;
            index_info.idx_deleted_count = 0;

            rtree_ = std::make_unique<RTree>(db_, index_info);
        }

        // Delegate to real RTree implementation
        return rtree_->search(query, current_xid, results_out, ctx);
    }

    Status RTreeIndex::remove(const std::vector<uint8_t> &key, const TID &tid,
                              uint64_t xmax, ErrorContext *ctx)
    {
        // Parse bounding box
        BoundingBox bbox;
        Status status = deserializeBoundingBox(key, &bbox, ctx);
        if (!status.ok())
        {
            return status;
        }

        // Lazy initialization of RTree
        if (!rtree_)
        {
            SBRTreeIndex index_info;
            std::memcpy(&index_info.idx_uuid, &index_uuid_, sizeof(ID));
            index_info.idx_root_page = root_page_;
            index_info.idx_max_entries = MAX_ENTRIES;
            index_info.idx_height = 0;
            index_info.idx_entry_count = 0;
            index_info.idx_page_count = 0;
            index_info.idx_deleted_count = 0;

            rtree_ = std::make_unique<RTree>(db_, index_info);
        }

        // Delegate to real RTree implementation
        // Note: RTree::remove takes current_xid, not xmax, so we pass xmax as current_xid
        // This is for MGA logical deletion where xmax marks when the entry was deleted
        return rtree_->remove(bbox, tid, xmax, ctx);
    }

    Status RTreeIndex::vacuum(ErrorContext *ctx)
    {
        // Delegate to RTree if initialized
        if (rtree_)
        {
            return rtree_->clear(ctx);
        }

        return Status::OK;
    }

    Status RTreeIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                         uint64_t *entries_removed_out,
                                         uint64_t *pages_modified_out,
                                         ErrorContext *ctx)
    {
        // Initialize outputs
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;

        if (dead_tids.empty())
        {
            return Status::OK;
        }

        // Lazy initialization of RTree
        if (!rtree_)
        {
            SBRTreeIndex index_info;
            std::memcpy(&index_info.idx_uuid, &index_uuid_, sizeof(ID));
            index_info.idx_root_page = root_page_;
            index_info.idx_max_entries = MAX_ENTRIES;
            index_info.idx_height = 0;
            index_info.idx_entry_count = 0;
            index_info.idx_page_count = 0;
            index_info.idx_deleted_count = 0;

            rtree_ = std::make_unique<RTree>(db_, index_info);
        }

        // Delegate to real RTree implementation (IndexGCInterface)
        return rtree_->removeDeadEntries(dead_tids, entries_removed_out, pages_modified_out, ctx);
    }

    // ========================================================================
    // Helper Methods - Bounding Box Serialization
    // ========================================================================

    Status RTreeIndex::deserializeBoundingBox(const std::vector<uint8_t>& key,
                                              BoundingBox* bbox,
                                              ErrorContext* ctx)
    {
        // Expect 4 doubles: min_x, min_y, max_x, max_y
        if (key.size() != 32) // 4 * sizeof(double)
        {
            if (ctx)
            {
                ctx->setError("Invalid bounding box size");
            }
            return Status::InvalidArgument;
        }

        const double* coords = reinterpret_cast<const double*>(key.data());
        bbox->min_x = coords[0];
        bbox->min_y = coords[1];
        bbox->max_x = coords[2];
        bbox->max_y = coords[3];

        // Validate
        if (bbox->min_x > bbox->max_x || bbox->min_y > bbox->max_y)
        {
            if (ctx)
            {
                ctx->setError("Invalid bounding box coordinates");
            }
            return Status::InvalidArgument;
        }

        return Status::OK;
    }

    std::vector<uint8_t> RTreeIndex::serializeBoundingBox(const BoundingBox& bbox)
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
