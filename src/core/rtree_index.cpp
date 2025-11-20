// =================================================================================================
// ScratchBird Database Engine
// R-Tree Index Implementation
// November 19, 2025
// =================================================================================================

#include "scratchbird/core/rtree_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/page_manager.h"

namespace scratchbird {
namespace core {

    RTreeIndex::RTreeIndex(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page)
        : db_(db), index_uuid_(index_uuid), meta_page_(meta_page), root_page_(0)
    {
    }

    RTreeIndex::~RTreeIndex() = default;

    Status RTreeIndex::create(Database *db, const UuidV7Bytes &index_uuid,
                              uint32_t *meta_page_out, ErrorContext *ctx)
    {
        // Allocate meta page and root page
        auto page_mgr = db->page_manager();
        uint32_t meta_page = page_mgr->allocatePage();
        uint32_t root_page = page_mgr->allocatePage();

        // Initialize meta page with root page number
        // In a real implementation, we'd store: root_page, tree_height, entry_count, etc.
        // For now, just store the root page number
        // TODO: Proper metadata page format

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
        // In a real implementation, read from meta_page
        // For now, assume root_page is stored at meta_page offset 0
        // TODO: Proper metadata loading
        index->root_page_ = meta_page + 1; // Placeholder

        return index;
    }

    Status RTreeIndex::insert(const std::vector<uint8_t> &key, const TID &tid,
                              uint64_t xmin, ErrorContext *ctx)
    {
        // Parse bounding box from key
        BoundingBox bbox;
        Status status = deserializeBoundingBox(key, &bbox, ctx);
        if (!status.ok())
        {
            return status;
        }

        // Create entry
        RTreeEntry entry;
        entry.bbox = bbox;
        entry.tid = tid;
        entry.xmin = xmin;
        entry.xmax = 0; // Active entry
        entry.is_leaf = true;

        // Choose leaf node for insertion
        uint32_t leaf_page = 0;
        status = chooseLeaf(bbox, &leaf_page, ctx);
        if (!status.ok())
        {
            return status;
        }

        // Insert entry into leaf
        status = insertEntry(leaf_page, entry, ctx);
        if (!status.ok())
        {
            return status;
        }

        // Adjust tree upwards (handle splits if needed)
        uint32_t split_page = 0;
        BoundingBox split_bbox;
        status = adjustTree(leaf_page, &split_page, &split_bbox, ctx);

        return status;
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

        // Search from root
        if (root_page_ == 0)
        {
            // Empty tree
            return Status::OK;
        }

        std::vector<TID> results;
        status = searchNode(root_page_, query, current_xid, &results, ctx);
        if (!status.ok())
        {
            return status;
        }

        if (results_out)
        {
            *results_out = std::move(results);
        }

        return Status::OK;
    }

    Status RTreeIndex::remove(const std::vector<uint8_t> &key, const TID &tid,
                              uint64_t xmax, ErrorContext *ctx)
    {
        // MGA logical deletion: Set xmax on matching entry
        // In R-Tree, we need to locate the entry by both bbox and TID
        // This is a simplified implementation that marks entries as deleted

        // Parse bounding box
        BoundingBox bbox;
        Status status = deserializeBoundingBox(key, &bbox, ctx);
        if (!status.ok())
        {
            return status;
        }

        // In production: traverse tree to find leaf containing (bbox, tid)
        // Then update entry's xmax field to mark as deleted
        // For now, return OK as we'd need page-level operations
        // TODO: Implement tree traversal and entry update

        return Status::OK;
    }

    Status RTreeIndex::vacuum(ErrorContext *ctx)
    {
        // Vacuum: Remove entries where xmax is set and transaction is committed
        // In production: traverse tree and compact nodes by removing dead entries
        // This would require reading TIP to check transaction commit status

        // For now, return OK as full vacuum requires:
        // 1. TIP access to check which transactions are committed
        // 2. Tree traversal to find all entries
        // 3. Node compaction/merge logic
        // TODO: Implement full vacuum with TIP integration

        return Status::OK;
    }

    Status RTreeIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                         uint64_t *entries_removed_out,
                                         uint64_t *pages_modified_out,
                                         ErrorContext *ctx)
    {
        // Garbage collection: Remove entries matching dead_tids
        // This is called by VACUUM to physically remove dead tuples from the index

        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;

        if (dead_tids.empty())
        {
            return Status::OK;
        }

        // In production: traverse tree and remove entries matching dead_tids
        // This requires:
        // 1. Tree traversal to find all leaf nodes
        // 2. For each leaf, check entries against dead_tids
        // 3. Remove matching entries and compact nodes
        // 4. Update parent nodes if needed
        // TODO: Implement full GC traversal

        return Status::OK;
    }

    // ========================================================================
    // Helper Methods
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

    Status RTreeIndex::chooseLeaf(const BoundingBox& bbox,
                                  uint32_t* leaf_page,
                                  ErrorContext* ctx)
    {
        // R-Tree ChooseLeaf algorithm
        // Start from root and descend to leaf, choosing subtree that needs
        // least area enlargement

        if (root_page_ == 0)
        {
            // Empty tree - need to create root
            // In production, allocate new root page
            if (ctx)
            {
                ctx->setError("Empty tree - root not initialized");
            }
            return Status::NotFound;
        }

        uint32_t current_page = root_page_;

        // In production: traverse tree until we reach a leaf
        // For each internal node, call chooseSubtree
        // For now, return root as leaf (simplified)
        // TODO: Implement full tree traversal with chooseSubtree

        *leaf_page = current_page;
        return Status::OK;
    }

    Status RTreeIndex::chooseSubtree(uint32_t node_page,
                                     const BoundingBox& bbox,
                                     uint32_t* child_page,
                                     ErrorContext* ctx)
    {
        // R-Tree ChooseSubtree algorithm
        // Among all entries in node, choose the one whose bbox needs least
        // area enlargement to include new bbox

        // In production:
        // 1. Read node page
        // 2. For each entry, calculate area increase
        // 3. Choose entry with minimum area increase (ties: smallest area)
        // 4. Return child_page of chosen entry

        // For now, simplified stub
        // TODO: Implement with actual page reads and area calculations

        *child_page = node_page; // Placeholder
        return Status::OK;
    }

    Status RTreeIndex::splitNode(uint32_t page_num,
                                 RTreeEntry* new_entry,
                                 uint32_t* new_sibling,
                                 BoundingBox* separator_bbox,
                                 ErrorContext* ctx)
    {
        // R* split algorithm
        // This is more sophisticated than original R-Tree split
        // Uses linear cost algorithm to minimize overlap and coverage

        // In production:
        // 1. Read all entries from page_num
        // 2. Add new_entry to collection
        // 3. Choose split axis (x or y) based on perimeter heuristic
        // 4. Sort entries along chosen axis
        // 5. Choose split index to minimize overlap
        // 6. Allocate new sibling page
        // 7. Distribute entries between original and sibling
        // 8. Calculate bounding boxes for both nodes
        // 9. Write both pages

        // For now, simplified stub
        // TODO: Implement full R* split algorithm

        auto page_mgr = db_->page_manager();
        *new_sibling = page_mgr->allocatePage();

        // Placeholder separator bbox
        if (separator_bbox)
        {
            *separator_bbox = new_entry->bbox;
        }

        return Status::OK;
    }

    Status RTreeIndex::adjustTree(uint32_t leaf_page,
                                  uint32_t* split_page,
                                  BoundingBox* split_bbox,
                                  ErrorContext* ctx)
    {
        // Adjust tree upwards after insertion
        // Propagate splits up the tree if needed

        // In production:
        // 1. Start from leaf_page
        // 2. Ascend to root, adjusting bounding boxes
        // 3. If a node overflows, split it
        // 4. If root splits, create new root

        // For now, simplified stub
        // TODO: Implement full tree adjustment with split propagation

        if (split_page)
        {
            *split_page = 0; // No split
        }

        return Status::OK;
    }

    Status RTreeIndex::insertEntry(uint32_t page_num,
                                   const RTreeEntry& entry,
                                   ErrorContext* ctx)
    {
        // Insert entry into node at page_num
        // If node overflows (> MAX_ENTRIES), need to split

        // In production:
        // 1. Read page
        // 2. Check if space available (<= MAX_ENTRIES)
        // 3. Add entry to page
        // 4. Write page back
        // 5. If overflow, return error code to trigger split

        // For now, simplified stub
        // TODO: Implement with actual page operations

        return Status::OK;
    }

    Status RTreeIndex::searchNode(uint32_t page_num,
                                  const BoundingBox& query,
                                  uint64_t current_xid,
                                  std::vector<TID>* results,
                                  ErrorContext* ctx)
    {
        // Recursive search algorithm
        // For each entry in node:
        //   - If entry bbox overlaps query
        //     - If leaf: add TID to results (if visible via MGA)
        //     - If internal: recursively search child

        // In production:
        // 1. Read page
        // 2. For each entry:
        //    a. Check if entry.bbox overlaps query
        //    b. Check MGA visibility: entry.xmin <= current_xid && (entry.xmax == 0 || entry.xmax > current_xid)
        //    c. If leaf and visible: add entry.tid to results
        //    d. If internal and overlaps: recursively call searchNode on entry.child_page

        // For now, simplified stub
        // TODO: Implement full recursive search with MGA visibility

        return Status::OK;
    }

} // namespace core
} // namespace scratchbird
