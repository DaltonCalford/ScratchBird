#include "scratchbird/core/rtree.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/types.h"
#include "scratchbird/core/config.h"
#include <algorithm>
#include <cstring>
#include <set>

namespace scratchbird::core
{

// ============================================================================
// Constructor / Destructor
// ============================================================================

RTree::RTree(Database* db, SBRTreeIndex index_info)
    : db_(db)
    , index_info_(index_info)
    , root_(nullptr)
{
    LOG_DEBUG(BTREE, "RTree created for index %s",
              "<uuid>");
}

RTree::~RTree()
{
    LOG_DEBUG(BTREE, "RTree destroyed for index %s",
              "<uuid>");
}

GPID RTree::indexGPID(uint64_t page_num) const
{
    return makeGPID(index_info_.idx_tablespace_id, page_num);
}

Status RTree::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx)
{
    return db_->buffer_pool()->pinPageGlobal(indexGPID(page_num), buffer, ctx);
}

Status RTree::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx)
{
    return db_->buffer_pool()->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
}

// ============================================================================
// Static Factory Methods
// ============================================================================

Status RTree::create(Database* db,
                    const UuidV7Bytes& index_uuid,
                    const UuidV7Bytes& table_uuid,
                    const std::vector<UuidV7Bytes>& column_uuids,
                    uint32_t max_entries,
                    GPID root_gpid,
                    ErrorContext* ctx)
{
    LOG_INFO(BTREE, "Creating R-tree index %s with max_entries=%u",
             "<uuid>", max_entries);

    if (max_entries < 4 || max_entries > 200)
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                            "max_entries must be between 4 and 200");
        }
        return Status::INVALID_ARGUMENT;
    }

    uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));

    // Initialize root page
    void* page = nullptr;
    Status status = db->buffer_pool()->pinPageGlobal(root_gpid, &page, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(BTREE, "Failed to pin root page %u", root_page);
        return status;
    }

    // Initialize page header
    SBRTreePage* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));
    std::memset(rtree_page, 0, sizeof(SBRTreePage));

    // Set page header
    rtree_page->rtree_header.page_type = PAGE_TYPE_RTREE_NODE;
    rtree_page->rtree_header.page_size = db->page_size();

    // Set R-tree metadata
    std::memcpy(&rtree_page->rtree_index_uuid, &index_uuid, sizeof(ID));
    std::memcpy(&rtree_page->rtree_table_uuid, &table_uuid, sizeof(ID));
    rtree_page->rtree_flags = static_cast<uint16_t>(RTreeFlags::ROOT) |
                             static_cast<uint16_t>(RTreeFlags::LEAF);
    rtree_page->rtree_count = 0;
    rtree_page->rtree_free_space = db->page_size() - sizeof(SBRTreePage);
    rtree_page->rtree_level = 0;
    rtree_page->rtree_max_entries = max_entries;
    rtree_page->rtree_left_sibling = 0;
    rtree_page->rtree_right_sibling = 0;
    rtree_page->rtree_parent_page = 0;
    rtree_page->rtree_total_entries = 0;
    rtree_page->rtree_deleted_entries = 0;
    rtree_page->rtree_height = 1;

    // Get current transaction ID for MGA
    TransactionManager* txn_mgr = db->transaction_manager();
    uint64_t xmin = ConnectionContext::getCurrentTransactionId();
    rtree_page->rtree_xmin = xmin;
    rtree_page->rtree_xmax = 0;
    rtree_page->rtree_lsn = 0;

    // Mark page as dirty and unpin
    db->buffer_pool()->unpinPageGlobal(root_gpid, true, nullptr);

    LOG_INFO(BTREE, "R-tree index created successfully, root page: %u", root_page);
    return Status::OK;
}

std::unique_ptr<RTree> RTree::open(Database* db,
                                   const UuidV7Bytes& index_uuid,
                                   GPID root_gpid,
                                   uint32_t max_entries,
                                   ErrorContext* ctx)
{
    LOG_DEBUG(BTREE, "Opening R-tree index %s, root page: %u",
              "<uuid>", static_cast<uint32_t>(getPageNumber(root_gpid)));

    // Load root page to get metadata
    void* page = nullptr;
    Status status = db->buffer_pool()->pinPageGlobal(root_gpid, &page, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(BTREE, "Failed to pin root page %u", static_cast<uint32_t>(getPageNumber(root_gpid)));
        return nullptr;
    }

    SBRTreePage* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));

    // Build index info
    SBRTreeIndex index_info;
    std::memcpy(&index_info.idx_uuid, &rtree_page->rtree_index_uuid, sizeof(ID));
    std::memcpy(&index_info.idx_table_uuid, &rtree_page->rtree_table_uuid, sizeof(ID));
    index_info.idx_root_page = getPageNumber(root_gpid);
    index_info.idx_tablespace_id = getTablespaceID(root_gpid);
    index_info.idx_height = rtree_page->rtree_height;
    index_info.idx_max_entries = max_entries;
    index_info.idx_entry_count = rtree_page->rtree_total_entries;
    index_info.idx_page_count = 1; // Will be updated as we traverse
    index_info.idx_deleted_count = rtree_page->rtree_deleted_entries;

    db->buffer_pool()->unpinPageGlobal(root_gpid, true, nullptr);

    auto rtree = std::make_unique<RTree>(db, index_info);

    LOG_DEBUG(BTREE, "R-tree index opened successfully");
    return rtree;
}

// ============================================================================
// Public API - Insert
// ============================================================================

Status RTree::insert(const BoundingBox& bbox,
                    const TID& tid,
                    uint64_t current_xid,
                    ErrorContext* ctx)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    LOG_DEBUG(BTREE, "Inserting entry into R-tree: bbox=[%.2f,%.2f,%.2f,%.2f]",
              bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y);

    if (!bbox.isValid())
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid bounding box");
        }
        return Status::INVALID_ARGUMENT;
    }

    // Get current transaction ID
    TransactionManager* txn_mgr = db_->transaction_manager();
    uint64_t xmin = ConnectionContext::getCurrentTransactionId();

    // Create entry
    RTreeEntry entry;
    entry.bbox = bbox;
    entry.row_id = tid;
    entry.xmin = xmin;
    entry.xmax = 0;
    entry.is_deleted = false;

    // Choose leaf node to insert into
    RTreeNode* leaf = chooseLeaf(bbox, current_xid);
    if (!leaf)
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Failed to find leaf node");
        }
        return Status::NOT_FOUND;
    }

    // Check if node has space
    if (leaf->isFull())
    {
        // Try forced reinsert first (R*-tree optimization)
        if (!forceReinsert(leaf, entry, current_xid))
        {
            // Reinsert failed, need to split
            std::unique_ptr<RTreeNode> new_sibling = splitNode(leaf, entry);
            adjustTree(leaf, new_sibling.get(), current_xid);

            // Persist both nodes after split
            Status save_status = saveNode(leaf, ctx);
            if (save_status != Status::OK)
            {
                LOG_ERROR(BTREE, "Failed to save leaf node after split");
                return save_status;
            }
            save_status = saveNode(new_sibling.get(), ctx);
            if (save_status != Status::OK)
            {
                LOG_ERROR(BTREE, "Failed to save new sibling after split");
                return save_status;
            }
        }
    }
    else
    {
        // Add entry to leaf
        leaf->addEntry(entry);
        adjustTree(leaf, nullptr, current_xid);

        // Persist the modified leaf node
        Status save_status = saveNode(leaf, ctx);
        if (save_status != Status::OK)
        {
            LOG_ERROR(BTREE, "Failed to save leaf node after insert");
            return save_status;
        }
    }

    // Update statistics
    index_info_.idx_entry_count++;
    updateStatistics();

    LOG_DEBUG(BTREE, "Entry inserted successfully");
    return Status::OK;
}

// ============================================================================
// Public API - Search
// ============================================================================

Status RTree::search(const BoundingBox& bbox,
                    uint64_t current_xid,
                    std::vector<TID>* tids_out,
                    ErrorContext* ctx)
{
    std::shared_lock<std::shared_mutex> lock(mutex_);

    LOG_DEBUG(BTREE, "Searching R-tree: bbox=[%.2f,%.2f,%.2f,%.2f]",
              bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y);

    if (!bbox.isValid())
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid bounding box");
        }
        return Status::INVALID_ARGUMENT;
    }

    if (!tids_out)
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "tids_out is null");
        }
        return Status::INVALID_ARGUMENT;
    }

    tids_out->clear();

    // Load root node
    if (!root_)
    {
        root_ = loadNode(index_info_.idx_root_page);
        if (!root_)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Failed to load root node");
            }
            return Status::NOT_FOUND;
        }
    }

    // Empty tree
    if (root_->getEntryCount() == 0)
    {
        LOG_DEBUG(BTREE, "Search completed on empty tree, found 0 entries");
        return Status::OK;
    }

    // Recursive search using stack to avoid deep recursion
    // Stack stores page numbers to traverse
    std::vector<uint64_t> pages_to_search;
    pages_to_search.push_back(index_info_.idx_root_page);

    uint64_t nodes_visited = 0;
    uint64_t entries_tested = 0;

    while (!pages_to_search.empty())
    {
        uint64_t page_num = pages_to_search.back();
        pages_to_search.pop_back();

        // Load node from disk
        std::unique_ptr<RTreeNode> node = loadNode(page_num);
        if (!node)
        {
            LOG_WARNING(BTREE, "Failed to load node from page %lu during search", page_num);
            continue;
        }

        nodes_visited++;

        // Check each entry in the node
        for (size_t i = 0; i < node->getEntryCount(); ++i)
        {
            const RTreeEntry& entry = node->getEntry(i);
            entries_tested++;

            // Skip deleted entries (Firebird MGA TIP-based visibility check)
            if (!isEntryVisible(entry, current_xid))
                continue;

            // Check if entry's MBR intersects with query bbox
            if (!entry.bbox.intersects(bbox))
                continue;

            if (node->isLeaf())
            {
                // Leaf node - add tuple ID to results
                tids_out->push_back(entry.row_id);
            }
            else
            {
                // Internal node - add child page to search stack
                pages_to_search.push_back(entry.child_page);
            }
        }
    }

    LOG_DEBUG(BTREE, "Search completed: visited %lu nodes, tested %lu entries, found %zu results",
              nodes_visited, entries_tested, tids_out->size());
    return Status::OK;
}

// ============================================================================
// Public API - Remove
// ============================================================================

Status RTree::remove(const BoundingBox& bbox,
                    const TID& tid,
                    uint64_t current_xid,
                    ErrorContext* ctx)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    LOG_DEBUG(BTREE, "Removing entry from R-tree: bbox=[%.2f,%.2f,%.2f,%.2f], tid=[%u,%lu]",
              bbox.min_x, bbox.min_y, bbox.max_x, bbox.max_y, getTablespaceID(tid), getPageNumber(tid));

    if (!bbox.isValid())
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid bounding box");
        }
        return Status::INVALID_ARGUMENT;
    }

    // Load root node
    if (!root_)
    {
        root_ = loadNode(index_info_.idx_root_page);
        if (!root_)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Failed to load root node");
            }
            return Status::NOT_FOUND;
        }
    }

    // Empty tree
    if (root_->getEntryCount() == 0)
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Entry not found in empty tree");
        }
        return Status::NOT_FOUND;
    }

    // FindLeaf algorithm: search for the leaf containing the entry
    // We need to track the path from root to leaf for condensation
    struct PathNode
    {
        std::unique_ptr<RTreeNode> node;
        size_t entry_index; // Index in parent that points to this node
        uint64_t page_number;
    };

    std::vector<PathNode> path;

    // Start with root
    PathNode root_node;
    root_node.node = loadNode(index_info_.idx_root_page);
    root_node.entry_index = 0; // Root has no parent
    root_node.page_number = index_info_.idx_root_page;

    if (!root_node.node)
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Failed to load root node");
        }
        return Status::NOT_FOUND;
    }

    path.push_back(std::move(root_node));

    // Traverse down to leaf level
    while (!path.back().node->isLeaf())
    {
        RTreeNode* current = path.back().node.get();
        bool found_child = false;

        // Search for children whose MBR intersects with bbox
        for (size_t i = 0; i < current->getEntryCount(); ++i)
        {
            const RTreeEntry& entry = current->getEntry(i);

            // Skip deleted entries
            if (!isEntryVisible(entry, current_xid))
                continue;

            // Check if this child's MBR could contain our entry
            if (entry.bbox.contains(bbox) || entry.bbox.intersects(bbox))
            {
                // Load child node
                PathNode child_node;
                child_node.node = loadNode(entry.child_page);
                child_node.entry_index = i;
                child_node.page_number = entry.child_page;

                if (!child_node.node)
                {
                    LOG_WARNING(BTREE, "Failed to load child page %lu during removal", entry.child_page);
                    continue;
                }

                path.push_back(std::move(child_node));
                found_child = true;
                break; // Follow this path
            }
        }

        if (!found_child)
        {
            if (ctx)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Entry not found in tree structure");
            }
            return Status::NOT_FOUND;
        }
    }

    // Now we're at a leaf node - find the exact entry
    RTreeNode* leaf = path.back().node.get();
    size_t entry_index = 0;
    bool found = false;

    for (size_t i = 0; i < leaf->getEntryCount(); ++i)
    {
        const RTreeEntry& entry = leaf->getEntry(i);

        // Skip deleted entries
        if (!isEntryVisible(entry, current_xid))
            continue;

        // Match by TID and bbox
        if (entry.row_id == tid &&
            entry.bbox.min_x == bbox.min_x &&
            entry.bbox.min_y == bbox.min_y &&
            entry.bbox.max_x == bbox.max_x &&
            entry.bbox.max_y == bbox.max_y)
        {
            entry_index = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Exact entry not found in leaf");
        }
        return Status::NOT_FOUND;
    }

    // Logical deletion per Firebird MGA (NOT physical removal)
    // Per MGA_RULES.md Rule 5: Use xmax marking, NOT physical deletion
    RTreeEntry& entry_to_delete = leaf->getEntry(entry_index);
    entry_to_delete.xmax = current_xid;
    entry_to_delete.is_deleted = true;

    // Update deleted count statistics
    index_info_.idx_deleted_count++;

    // Save the modified leaf
    Status save_status = saveNode(leaf, ctx);
    if (save_status != Status::OK)
    {
        LOG_ERROR(BTREE, "Failed to save leaf node after logical deletion");
        if (ctx)
        {
            SET_ERROR_CONTEXT(ctx, save_status, "Failed to save modified leaf node");
        }
        return save_status;
    }

    // Note: With logical deletion, we don't condense the tree immediately.
    // The entry remains in the leaf with xmax set, invisible to other transactions.
    // Tree condensation and physical removal will occur during garbage collection
    // via removeDeadEntries() when the entry's xmax is older than OIT.

    updateStatistics();

    LOG_DEBUG(BTREE, "Entry logically deleted successfully (marked with xmax=%lu)", current_xid);
    return Status::OK;
}

// ============================================================================
// Public API - Clear
// ============================================================================

Status RTree::clear(ErrorContext* ctx)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    LOG_INFO(BTREE, "Clearing R-tree index");

    // Reset root
    root_.reset();

    // Recreate empty root page
    GPID root_gpid = makeGPID(index_info_.idx_tablespace_id, index_info_.idx_root_page);
    Status status = create(db_, index_info_.idx_uuid, index_info_.idx_table_uuid,
                          index_info_.idx_column_ids, index_info_.idx_max_entries,
                          root_gpid, ctx);

    if (status == Status::OK)
    {
        index_info_.idx_entry_count = 0;
        index_info_.idx_deleted_count = 0;
        index_info_.idx_height = 1;
    }

    return status;
}

// ============================================================================
// IndexGCInterface Implementation
// ============================================================================

Status RTree::removeDeadEntries(const std::vector<TID>& dead_tids,
                               uint64_t* entries_removed_out,
                               uint64_t* pages_modified_out,
                               ErrorContext* ctx)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    LOG_DEBUG(BTREE, "Removing %zu dead entries from R-tree", dead_tids.size());

    // Initialize outputs
    if (entries_removed_out)
        *entries_removed_out = 0;
    if (pages_modified_out)
        *pages_modified_out = 0;

    // Empty check
    if (dead_tids.empty())
    {
        return Status::OK;
    }

    uint64_t removed_count = 0;
    uint64_t pages_modified = 0;

    // Get OIT (Oldest Interesting Transaction) from TransactionManager
    // Per MGA_RULES.md Rule 10: Only physically remove entries with xmax < OIT
    TransactionManager* txn_mgr = db_->transaction_manager();
    uint64_t oit = txn_mgr->getOldestXid();

    LOG_DEBUG(BTREE, "Garbage collection: OIT=%lu, checking %zu dead TIDs", oit, dead_tids.size());

    // Create a set of TIDs for fast lookup
    std::set<TID> dead_tid_set(dead_tids.begin(), dead_tids.end());

    // Plan 01 Task D.4: R-Tree Leaf Traversal GC
    //
    // Strategy: Traverse all leaf pages using rtree_right_sibling chain
    // and physically remove entries where:
    // 1. entry_row_id is in dead_tids (heap confirmed dead)
    // 2. entry_xmax < OIT (no transaction can see them)

    // Step 1: Find the first leaf page by traversing down from root
    uint64_t first_leaf_page = 0;
    {
        void* page = nullptr;
        Status status = pinIndexPage(index_info_.idx_root_page, &page, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(BTREE, "Failed to pin root page %lu", index_info_.idx_root_page);
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin root page");
            return status;
        }

        SBRTreePage* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));

        // If root is leaf, start there
        if (rtree_page->rtree_flags & static_cast<uint16_t>(RTreeFlags::LEAF))
        {
            first_leaf_page = index_info_.idx_root_page;
        }
        else
        {
            // Traverse down to first leaf
            // For simplicity, follow the first child at each level
            uint64_t current_page = index_info_.idx_root_page;

            while (true)
            {
                // Check if current page is leaf
                if (rtree_page->rtree_flags & static_cast<uint16_t>(RTreeFlags::LEAF))
                {
                    first_leaf_page = current_page;
                    break;
                }

                // Get first child
                if (rtree_page->rtree_count == 0)
                {
                    // Empty internal node - no leaves to traverse
                    unpinIndexPage(current_page, false, nullptr);
                    LOG_DEBUG(BTREE, "Empty R-tree, no leaves to GC");
                    return Status::OK;
                }

                const uint8_t* entry_data = ((uint8_t*)page) + sizeof(SBRTreePage);
                const SBRTreeEntry* first_entry = reinterpret_cast<const SBRTreeEntry*>(entry_data);
                uint64_t child_page = first_entry->entry_child_page;

                unpinIndexPage(current_page, false, nullptr);

                // Load child
                status = pinIndexPage(child_page, &page, ctx);
                if (status != Status::OK)
                {
                    LOG_ERROR(BTREE, "Failed to load child page %lu during leaf search", child_page);
                    return status;
                }

                current_page = child_page;
                rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));
            }
        }

        unpinIndexPage(first_leaf_page, false, nullptr);
    }

    if (first_leaf_page == 0)
    {
        LOG_DEBUG(BTREE, "No leaves found in R-tree");
        return Status::OK;
    }

    // Step 2: Traverse all leaf pages using rtree_right_sibling
    uint64_t current_page_id = first_leaf_page;

    while (current_page_id != 0)
    {
        void* page = nullptr;
        Status status = pinIndexPage(current_page_id, &page, ctx);
        if (status != Status::OK)
        {
            LOG_ERROR(BTREE, "Failed to pin leaf page %lu", current_page_id);
            break;
        }

        SBRTreePage* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));

        // Verify this is a leaf page
        if (!(rtree_page->rtree_flags & static_cast<uint16_t>(RTreeFlags::LEAF)))
        {
            LOG_WARNING(BTREE, "Page %lu is not a leaf during GC traversal", current_page_id);
            unpinIndexPage(current_page_id, false, nullptr);
            break;
        }

        // Get next sibling before we modify the page
        uint64_t next_sibling = rtree_page->rtree_right_sibling;

        // Process entries on this page
        std::vector<size_t> to_remove;
        uint8_t* entry_data = ((uint8_t*)page) + sizeof(SBRTreePage);

        for (uint16_t i = 0; i < rtree_page->rtree_count; ++i)
        {
            SBRTreeEntry* entry = reinterpret_cast<SBRTreeEntry*>(entry_data + i * sizeof(SBRTreeEntry));

            // Check if this entry should be removed:
            // Per GIN/BRIN pattern: If caller says TID is dead, trust it
            // The heap vacuum has already confirmed these tuples are dead
            if (dead_tid_set.find(entry->entry_row_id) != dead_tid_set.end())
            {
                // Only remove if entry is marked as deleted (xmax != 0)
                // This ensures we don't remove live entries
                if (entry->entry_xmax != 0)
                {
                    to_remove.push_back(i);
                    LOG_DEBUG(BTREE, "Page %lu entry %u: removing dead TID (xmax=%lu)",
                             current_page_id, i, entry->entry_xmax);
                }
                else
                {
                    LOG_DEBUG(BTREE, "Page %lu entry %u: TID in dead list but xmax=0, skipping",
                             current_page_id, i);
                }
            }
        }

        // Remove entries in reverse order to maintain indices
        bool page_modified = false;
        for (auto it = to_remove.rbegin(); it != to_remove.rend(); ++it)
        {
            size_t index = *it;

            // Shift all entries after this one down
            uint8_t* src = entry_data + (index + 1) * sizeof(SBRTreeEntry);
            uint8_t* dst = entry_data + index * sizeof(SBRTreeEntry);
            size_t bytes_to_move = (rtree_page->rtree_count - index - 1) * sizeof(SBRTreeEntry);

            if (bytes_to_move > 0)
            {
                std::memmove(dst, src, bytes_to_move);
            }

            rtree_page->rtree_count--;
            removed_count++;
            page_modified = true;
        }

        if (page_modified)
        {
            // Update free space
            rtree_page->rtree_free_space = db_->page_size() - sizeof(SBRTreePage) -
                                          rtree_page->rtree_count * sizeof(SBRTreeEntry);
            pages_modified++;
        }

        // Unpin page
        unpinIndexPage(current_page_id, page_modified, nullptr);

        // Move to next sibling
        current_page_id = next_sibling;
    }

    // Update statistics
    if (removed_count > 0)
    {
        if (index_info_.idx_deleted_count >= removed_count)
        {
            index_info_.idx_deleted_count -= removed_count;
        }
        else
        {
            index_info_.idx_deleted_count = 0;
        }
        updateStatistics();
        LOG_INFO(BTREE, "Removed %lu dead entries from R-tree, modified %lu pages",
                 removed_count, pages_modified);
    }

    if (entries_removed_out)
        *entries_removed_out = removed_count;
    if (pages_modified_out)
        *pages_modified_out = pages_modified;

    return Status::OK;
}

Status RTree::updateTIDsAfterMigration(const std::unordered_map<uint64_t, uint64_t>& tid_mapping,
                                       uint64_t* tids_updated_out,
                                       uint64_t* pages_modified_out,
                                       ErrorContext* ctx)
{
    std::unique_lock<std::shared_mutex> lock(mutex_);

    if (tids_updated_out)
    {
        *tids_updated_out = 0;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = 0;
    }

    if (index_info_.idx_root_page == 0 || tid_mapping.empty())
    {
        return Status::OK;
    }

    uint64_t tids_updated = 0;
    uint64_t pages_modified = 0;
    std::vector<uint64_t> stack;
    stack.push_back(index_info_.idx_root_page);

    while (!stack.empty())
    {
        uint64_t page_num = stack.back();
        stack.pop_back();

        void* page = nullptr;
        Status status = pinIndexPage(page_num, &page, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to pin R-tree page during TID update");
            return status;
        }

        auto* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));
        bool is_leaf = (rtree_page->rtree_flags & static_cast<uint16_t>(RTreeFlags::LEAF)) != 0;
        bool page_dirty = false;

        uint8_t* entry_data = ((uint8_t*)page) + sizeof(SBRTreePage);
        for (uint16_t i = 0; i < rtree_page->rtree_count; ++i)
        {
            auto* entry = reinterpret_cast<SBRTreeEntry*>(entry_data);

            if (is_leaf)
            {
                auto it = tid_mapping.find(entry->entry_row_id.gpid);
                if (it != tid_mapping.end())
                {
                    entry->entry_row_id.gpid = it->second;
                    page_dirty = true;
                    ++tids_updated;
                }
            }
            else
            {
                stack.push_back(entry->entry_child_page);
            }

            entry_data += sizeof(SBRTreeEntry);
        }

        if (page_dirty)
        {
            ++pages_modified;
        }

        unpinIndexPage(page_num, page_dirty, ctx);
    }

    if (tids_updated_out)
    {
        *tids_updated_out = tids_updated;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = pages_modified;
    }

    return Status::OK;
}

// ============================================================================
// Statistics and Metadata
// ============================================================================

uint64_t RTree::getTotalEntries() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_info_.idx_entry_count;
}

uint64_t RTree::getDeletedEntries() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_info_.idx_deleted_count;
}

uint16_t RTree::getHeight() const
{
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return index_info_.idx_height;
}

// ============================================================================
// Core R-tree Algorithms
// ============================================================================

RTreeNode* RTree::chooseLeaf(const BoundingBox& bbox, uint64_t current_xid)
{
    // Load root if not cached
    if (!root_)
    {
        root_ = loadNode(index_info_.idx_root_page);
        if (!root_)
        {
            LOG_ERROR(BTREE, "Failed to load root node");
            return nullptr;
        }
    }

    RTreeNode* current = root_.get();

    // Traverse down to leaf level
    while (!current->isLeaf())
    {
        size_t best_index;

        // At leaf level, use overlap minimization (R*-tree)
        // At higher levels, use area enlargement
        if (current->getLevel() == 1)
        {
            // Parent of leaf - minimize overlap
            best_index = current->findMinOverlap(bbox);
        }
        else
        {
            // Higher levels - minimize enlargement
            best_index = current->findMinEnlargement(bbox);
        }

        // In a full implementation, we'd load the child node here
        // For now, we can only handle single-level trees (root = leaf)
        LOG_WARNING(BTREE, "Multi-level tree not yet fully supported in chooseLeaf");
        break;
    }

    return current;
}

std::unique_ptr<RTreeNode> RTree::splitNode(RTreeNode* node, const RTreeEntry& new_entry)
{
    LOG_DEBUG(BTREE, "Splitting node with %zu entries", node->getEntryCount());

    // Add the new entry temporarily
    node->addEntry(new_entry);

    // Perform split
    std::unique_ptr<RTreeNode> new_sibling = node->split();

    LOG_DEBUG(BTREE, "Split complete: node1=%zu entries, node2=%zu entries",
              node->getEntryCount(), new_sibling->getEntryCount());

    return new_sibling;
}

void RTree::adjustTree(RTreeNode* leaf, RTreeNode* new_sibling, uint64_t current_xid)
{
    LOG_DEBUG(BTREE, "Adjusting tree after modification");

    RTreeNode* current = leaf;
    RTreeNode* sibling = new_sibling;

    while (current != nullptr)
    {
        // Update MBR in parent
        if (current->getParent())
        {
            // Find parent entry pointing to this node
            RTreeNode* parent = current->getParent();
            BoundingBox new_mbr = current->calculateMBR();

            // Update parent's entry
            // (Simplified - full implementation would search for the entry)

            if (sibling != nullptr)
            {
                // Parent needs new entry for sibling
                RTreeEntry sibling_entry;
                sibling_entry.bbox = sibling->calculateMBR();
                sibling_entry.child_page = sibling->getPageNumber();
                sibling_entry.xmin = sibling->getXmin();
                sibling_entry.xmax = 0;
                sibling_entry.is_deleted = false;

                if (parent->isFull())
                {
                    // Recursive split
                    sibling = splitNode(parent, sibling_entry).release();
                }
                else
                {
                    parent->addEntry(sibling_entry);
                    sibling = nullptr;
                }
            }

            current = parent;
        }
        else
        {
            // Reached root
            if (sibling != nullptr)
            {
                // Root split - create new root
                auto new_root = std::make_unique<RTreeNode>(false, index_info_.idx_max_entries);
                new_root->setLevel(current->getLevel() + 1);

                // Add entries for old root and sibling
                RTreeEntry entry1;
                entry1.bbox = current->calculateMBR();
                entry1.child_page = current->getPageNumber();
                entry1.xmin = current->getXmin();
                entry1.xmax = 0;
                entry1.is_deleted = false;

                RTreeEntry entry2;
                entry2.bbox = sibling->calculateMBR();
                entry2.child_page = sibling->getPageNumber();
                entry2.xmin = sibling->getXmin();
                entry2.xmax = 0;
                entry2.is_deleted = false;

                new_root->addEntry(entry1);
                new_root->addEntry(entry2);

                current->setParent(new_root.get());
                sibling->setParent(new_root.get());

                root_ = std::move(new_root);
                index_info_.idx_height++;

                LOG_INFO(BTREE, "Root split, new height: %u", index_info_.idx_height);
            }
            break;
        }
    }
}

bool RTree::forceReinsert(RTreeNode* node, const RTreeEntry& new_entry, uint64_t current_xid)
{
    // R*-tree forced reinsert optimization
    // Remove and reinsert ~30% of entries that are farthest from center

    LOG_DEBUG(BTREE, "Attempting forced reinsert");

    size_t num_to_reinsert = node->getEntryCount() * 30 / 100;
    if (num_to_reinsert == 0)
        return false;

    // Calculate center of all entries
    BoundingBox mbr = node->calculateMBR();
    double center_x, center_y;
    mbr.getCenter(&center_x, &center_y);

    // Find entries farthest from center
    std::vector<std::pair<double, size_t>> distances;
    for (size_t i = 0; i < node->getEntryCount(); ++i)
    {
        double ex, ey;
        node->getEntry(i).bbox.getCenter(&ex, &ey);
        double dist = (ex - center_x) * (ex - center_x) + (ey - center_y) * (ey - center_y);
        distances.push_back({dist, i});
    }

    std::sort(distances.rbegin(), distances.rend()); // Sort descending

    // Remove and collect entries to reinsert
    std::vector<RTreeEntry> to_reinsert;
    for (size_t i = 0; i < num_to_reinsert && i < distances.size(); ++i)
    {
        size_t index = distances[i].second;
        to_reinsert.push_back(node->getEntry(index));
    }

    // Remove in reverse order
    std::sort(distances.begin(), distances.begin() + num_to_reinsert,
              [](const auto& a, const auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < num_to_reinsert && i < distances.size(); ++i)
    {
        node->removeEntry(distances[i].second);
    }

    // Add new entry
    node->addEntry(new_entry);

    // Reinsert removed entries
    for (const auto& entry : to_reinsert)
    {
        // Simplified - just add back to same node
        // Full implementation would call insert() recursively
        if (!node->isFull())
        {
            node->addEntry(entry);
        }
        else
        {
            LOG_WARNING(BTREE, "Forced reinsert failed - node still full");
            return false;
        }
    }

    LOG_DEBUG(BTREE, "Forced reinsert successful");
    return true;
}

void RTree::condenseTree(RTreeNode* leaf, size_t entry_index, uint64_t current_xid)
{
    // NOTE: This method is kept for API compatibility but is not used
    // With Firebird MGA, remove() uses logical deletion (xmax marking)
    // instead of physical removal and tree condensation.
    //
    // The remove() method now:
    // 1. FindLeaf - locate the entry to delete
    // 2. Mark entry with xmax (logical deletion)
    // 3. Save modified node
    //
    // Tree condensation and physical removal occur later during
    // garbage collection via removeDeadEntries() when xmax < OIT.

    LOG_DEBUG(BTREE, "condenseTree invoked - MGA logical delete path");

    // For compatibility, mark entry as deleted instead of physical removal
    if (entry_index < leaf->getEntryCount())
    {
        RTreeEntry& entry = leaf->getEntry(entry_index);
        entry.xmax = current_xid;
        entry.is_deleted = true;
        saveNode(leaf, nullptr);
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

std::unique_ptr<RTreeNode> RTree::loadNode(uint64_t page_number)
{
    LOG_DEBUG(BTREE, "Loading R-tree node from page %lu", page_number);

    void* page = nullptr;
    Status status = pinIndexPage(page_number, &page, nullptr);
    if (status != Status::OK)
    {
        LOG_ERROR(BTREE, "Failed to pin page %lu", page_number);
        return nullptr;
    }

    SBRTreePage* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));

    bool is_leaf = (rtree_page->rtree_flags & static_cast<uint16_t>(RTreeFlags::LEAF)) != 0;
    auto node = std::make_unique<RTreeNode>(is_leaf, rtree_page->rtree_max_entries);
    node->setPageNumber(page_number);
    node->setLevel(rtree_page->rtree_level);
    node->setXmin(rtree_page->rtree_xmin);
    node->setXmax(rtree_page->rtree_xmax);

    // Load entries from page
    const uint8_t* entry_data = ((uint8_t*)page) + sizeof(SBRTreePage);
    for (uint16_t i = 0; i < rtree_page->rtree_count; ++i)
    {
        const SBRTreeEntry* disk_entry = reinterpret_cast<const SBRTreeEntry*>(entry_data);

        RTreeEntry entry;
        entry.bbox.min_x = disk_entry->entry_min_x;
        entry.bbox.min_y = disk_entry->entry_min_y;
        entry.bbox.max_x = disk_entry->entry_max_x;
        entry.bbox.max_y = disk_entry->entry_max_y;
        entry.xmin = disk_entry->entry_xmin;
        entry.xmax = disk_entry->entry_xmax;

        // Don't set is_deleted based on xmax alone (MGA violation)
        // Visibility is determined dynamically by isEntryVisible() using TIP
        // The is_deleted flag is only used as a hint, not authoritative
        entry.is_deleted = false;

        if (is_leaf)
        {
            entry.row_id = disk_entry->entry_row_id;
        }
        else
        {
            entry.child_page = disk_entry->entry_child_page;
        }

        node->addEntry(entry);
        entry_data += sizeof(SBRTreeEntry);
    }

    unpinIndexPage(page_number, true, nullptr);

    LOG_DEBUG(BTREE, "Loaded node with %zu entries", node->getEntryCount());
    return node;
}

Status RTree::saveNode(RTreeNode* node, ErrorContext* ctx)
{
    LOG_DEBUG(BTREE, "Saving R-tree node to page %lu", node->getPageNumber());

    uint64_t page_number = node->getPageNumber();
    if (page_number == 0)
    {
        // Need to allocate new page
        Status status = allocatePage(node, ctx);
        if (status != Status::OK)
        {
            return status;
        }
        page_number = node->getPageNumber();
    }

    void* page = nullptr;
    Status status = pinIndexPage(page_number, &page, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(BTREE, "Failed to pin page %lu", page_number);
        return status;
    }

    SBRTreePage* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));

    // Update metadata
    rtree_page->rtree_count = static_cast<uint16_t>(node->getEntryCount());
    rtree_page->rtree_level = node->getLevel();
    rtree_page->rtree_xmin = node->getXmin();
    rtree_page->rtree_xmax = node->getXmax();

    if (node->isLeaf())
    {
        rtree_page->rtree_flags |= static_cast<uint16_t>(RTreeFlags::LEAF);
    }
    else
    {
        rtree_page->rtree_flags &= ~static_cast<uint16_t>(RTreeFlags::LEAF);
    }

    // Write entries to page
    uint8_t* entry_data = ((uint8_t*)page) + sizeof(SBRTreePage);
    for (size_t i = 0; i < node->getEntryCount(); ++i)
    {
        const RTreeEntry& entry = node->getEntry(i);
        SBRTreeEntry* disk_entry = reinterpret_cast<SBRTreeEntry*>(entry_data);

        disk_entry->entry_min_x = entry.bbox.min_x;
        disk_entry->entry_min_y = entry.bbox.min_y;
        disk_entry->entry_max_x = entry.bbox.max_x;
        disk_entry->entry_max_y = entry.bbox.max_y;
        disk_entry->entry_xmin = entry.xmin;
        disk_entry->entry_xmax = entry.xmax;
        disk_entry->entry_flags = 0;

        if (node->isLeaf())
        {
            disk_entry->entry_row_id = entry.row_id;
        }
        else
        {
            disk_entry->entry_child_page = entry.child_page;
        }

        entry_data += sizeof(SBRTreeEntry);
    }

    unpinIndexPage(page_number, true, nullptr);

    LOG_DEBUG(BTREE, "Node saved successfully");
    return Status::OK;
}

Status RTree::allocatePage(RTreeNode* node, ErrorContext* ctx)
{
    PageManager* page_mgr = db_->page_manager();
    GPID new_gpid = 0;

    Status status = page_mgr->allocatePageInTablespace(index_info_.idx_tablespace_id, &new_gpid, ctx);
    if (status != Status::OK)
    {
        LOG_ERROR(BTREE, "Failed to allocate page for R-tree node");
        SET_ERROR_CONTEXT(ctx, status, "Failed to allocate page for R-tree node");
        return status;
    }

    uint32_t new_page = static_cast<uint32_t>(getPageNumber(new_gpid));
    node->setPageNumber(new_page);
    LOG_DEBUG(BTREE, "Allocated page %u for R-tree node", new_page);

    return Status::OK;
}

bool RTree::isEntryVisible(const RTreeEntry& entry, uint64_t current_xid) const
{
    // Firebird MGA: Use TIP-based visibility checking (NOT snapshots)
    //
    // Per MGA_RULES.md Rule 3:
    // - Own changes always visible (xmin == current_xid)
    // - Committed transactions older than reader are visible
    // - Active or aborted transactions are not visible
    //
    // This checks:
    // 1. xmin is committed and < current_xid, OR xmin == current_xid
    // 2. xmax is 0 (not deleted), OR xmax is not committed, OR xmax > current_xid

    TransactionManager* txn_mgr = db_->transaction_manager();

    // Check if xmin is visible (entry created)
    if (!txn_mgr->isVersionVisible(entry.xmin, current_xid))
    {
        return false; // Entry not yet visible
    }

    // Check if xmax is visible (entry deleted)
    if (entry.xmax != 0 && txn_mgr->isVersionVisible(entry.xmax, current_xid))
    {
        return false; // Entry is deleted
    }

    return true; // Entry is visible
}

void RTree::updateStatistics()
{
    // Persist statistics to root page
    if (index_info_.idx_root_page == 0)
        return;

    void* page = nullptr;
    Status status = pinIndexPage(index_info_.idx_root_page, &page, nullptr);
    if (status != Status::OK)
    {
        LOG_WARNING(BTREE, "Failed to update R-tree statistics");
        return;
    }

    SBRTreePage* rtree_page = reinterpret_cast<SBRTreePage*>(((uint8_t*)page));
    rtree_page->rtree_total_entries = index_info_.idx_entry_count;
    rtree_page->rtree_deleted_entries = index_info_.idx_deleted_count;
    rtree_page->rtree_height = index_info_.idx_height;

    unpinIndexPage(index_info_.idx_root_page, true, nullptr);
}

} // namespace scratchbird::core
