/**
 * SP-GiST (Space-Partitioned Generalized Search Tree) Implementation
 * Phase 1: Core Framework
 *
 * Implements unbalanced tree structures with space partitioning for:
 * - Quad-trees (2D points)
 * - k-d trees (multi-dimensional data)
 * - Radix trees (string prefix search)
 */

#include "scratchbird/core/spgist_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include <algorithm>
#include <cstring>
#include <set>

namespace scratchbird::core
{

// Constants
constexpr uint16_t SPGIST_MAX_ENTRIES_PER_PAGE = 100;
constexpr uint16_t SPGIST_LEAF_THRESHOLD = 50; // Split leaf when exceeding this

// Helper struct for collecting leaf entries during split/GC
struct LeafEntry {
    std::vector<uint8_t> value;
    TID tid;
    uint64_t xmin;
    uint64_t xmax;
};

// =============================================================================
// SPGiSTIndex Implementation
// =============================================================================

SPGiSTIndex::SPGiSTIndex(Database* db,
                         const ID& index_uuid,
                         const ID& table_uuid,
                         const std::vector<ID>& column_ids,
                         std::shared_ptr<SPGiSTOperatorClass> opclass)
    : db_(db)
    , buffer_pool_(db->buffer_pool())
    , txn_manager_(db->transaction_manager())
    , index_uuid_(index_uuid)
    , table_uuid_(table_uuid)
    , column_ids_(column_ids)
    , opclass_(opclass)
    , root_page_(0)
    , entry_count_(0)
    , deleted_count_(0)
{
}

SPGiSTIndex::~SPGiSTIndex()
{
    // Cleanup handled by buffer pool
}

Status SPGiSTIndex::initialize(ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    LOG_INFO(CATALOG, "Initializing SP-GiST index %s with operator class %s",
             index_uuid_.toString().c_str(),
             opclass_->getOpClassName().c_str());

    // Allocate root page
    Status status = allocatePage(&root_page_, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Initialize root as empty leaf
    SBSPGiSTPage* root = nullptr;
    status = loadPage(root_page_, &root, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memset(root, 0, sizeof(SBSPGiSTPage));

    // Manual page header initialization (no initPageHeader() function exists)
    root->spgist_header.magic = K_MAGIC_SBRD;
    root->spgist_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
    root->spgist_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_SPGIST);
    root->spgist_header.page_size = 8192;
    root->spgist_header.page_id = static_cast<uint32_t>(root_page_);
    root->spgist_header.checksum = 0;
    root->spgist_header.lsn = 0;
    root->spgist_header.flags = 0;
    std::memcpy(root->spgist_header.database_uuid, db_->uuid().bytes.data(), 16);
    root->spgist_header.generation = 0;
    root->spgist_header.free_space = 0;
    root->spgist_header.item_count = 0;
    root->spgist_header.free_offset = 0;
    root->spgist_header.special_size = 0;

    root->spgist_index_uuid = index_uuid_;
    root->spgist_table_uuid = table_uuid_;
    root->spgist_flags = static_cast<uint16_t>(SPGiSTFlags::ROOT);
    root->spgist_node_type = static_cast<uint16_t>(SPGiSTNodeType::LEAF);
    root->spgist_count = 0;
    root->spgist_free_space = 8192 - sizeof(SBSPGiSTPage);
    root->spgist_opclass_id = opclass_->getOpClassId();
    root->spgist_xmin = txn_manager_->getCurrentXid();
    root->spgist_total_entries = 0;

    LOG_INFO(CATALOG, "SP-GiST index %s initialized with root page %lu",
             index_uuid_.toString().c_str(), root_page_);

    return Status::OK;
}

Status SPGiSTIndex::insert(const std::vector<uint8_t>& value,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    Status status = insertRecursive(root_page_, value, tid, current_xid, 0, ctx);
    if (status == Status::OK)
    {
        entry_count_++;
    }

    return status;
}

Status SPGiSTIndex::insertRecursive(uint64_t page_num,
                                    const std::vector<uint8_t>& value,
                                    const TID& tid,
                                    uint64_t current_xid,
                                    int level,
                                    ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SPGiSTNodeType node_type = static_cast<SPGiSTNodeType>(page->spgist_node_type);

    if (node_type == SPGiSTNodeType::LEAF)
    {
        // Insert into leaf page
        uint16_t entry_size = sizeof(SBSPGiSTLeafTuple) + value.size();

        // Check if page has space
        if (page->spgist_free_space < entry_size)
        {
            // Need to split leaf into inner node
            return splitNode(page_num, ctx);
        }

        // Add leaf entry
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) +
                            sizeof(SBSPGiSTPage) +
                            (8192 - sizeof(SBSPGiSTPage) - page->spgist_free_space);

        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);
        leaf->leaf_size = entry_size;
        leaf->leaf_valueSize = value.size();
        leaf->leaf_tid = tid;
        leaf->leaf_xmin = current_xid;
        leaf->leaf_xmax = 0;

        // Copy value data
        std::memcpy(entry_ptr + sizeof(SBSPGiSTLeafTuple), value.data(), value.size());

        page->spgist_count++;
        page->spgist_free_space -= entry_size;
        page->spgist_total_entries++;

        LOG_DEBUG(CATALOG, "SP-GiST: Inserted value into leaf page %lu (count=%u)",
                 page_num, page->spgist_count);

        return Status::OK;
    }
    else // SPGiSTNodeType::INNER
    {
        // Extract inner node information
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        if (page->spgist_count == 0)
        {
            // Empty inner node, shouldn't happen
            if (ctx)
            {
                ctx->code = Status::PAGE_CORRUPT;
                ctx->message = "Empty inner node in SP-GiST";
            }
            return Status::PAGE_CORRUPT;
        }

        // Read first (and only) inner tuple
        SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(entry_ptr);

        // Extract prefix
        std::vector<uint8_t> prefix(inner->inner_prefixSize);
        std::memcpy(prefix.data(), entry_ptr + sizeof(SBSPGiSTInnerTuple),
                   inner->inner_prefixSize);

        // Extract node labels and child pages
        std::vector<SPGiSTNodeLabel> node_labels;
        size_t offset = sizeof(SBSPGiSTInnerTuple) + inner->inner_prefixSize;

        auto config = opclass_->config();
        size_t label_size = config.labelSize;
        if (label_size == 0)
            label_size = 4; // Default variable size

        for (uint16_t i = 0; i < inner->inner_nNodes; ++i)
        {
            SPGiSTNodeLabel label;
            label.data.resize(label_size);
            std::memcpy(label.data.data(), entry_ptr + offset, label_size);
            offset += label_size;

            // Read child page number
            std::memcpy(&label.child_page, entry_ptr + offset, sizeof(uint64_t));
            offset += sizeof(uint64_t);

            node_labels.push_back(label);
        }

        // Ask operator class where to insert
        SPGiSTTraversal traversal = opclass_->choose(prefix, node_labels, value);

        switch (traversal.match_type)
        {
            case SPGiSTMatchType::MATCH_NODE:
                // Descend into specified child
                if (traversal.node_index < node_labels.size())
                {
                    uint64_t child_page = node_labels[traversal.node_index].child_page;
                    return insertRecursive(child_page, value, tid, current_xid,
                                         level + 1, ctx);
                }
                break;

            case SPGiSTMatchType::MATCH_ADD_NODE:
                // Need to add new child node
                // TODO: Allocate new leaf page and add to inner node
                LOG_DEBUG(CATALOG, "SP-GiST: MATCH_ADD_NODE not yet implemented");
                break;

            case SPGiSTMatchType::MATCH_SPLIT:
                // Need to split current node
                LOG_DEBUG(CATALOG, "SP-GiST: MATCH_SPLIT not yet implemented");
                break;
        }

        return Status::OK;
    }
}

Status SPGiSTIndex::search(const std::vector<uint8_t>& query,
                          uint64_t current_xid,
                          std::vector<TID>& results,
                          ErrorContext* ctx)
{
    std::shared_lock lock(mutex_);

    if (root_page_ == 0)
    {
        return Status::OK;
    }

    return searchRecursive(root_page_, query, current_xid, results, ctx);
}

Status SPGiSTIndex::searchRecursive(uint64_t page_num,
                                    const std::vector<uint8_t>& query,
                                    uint64_t current_xid,
                                    std::vector<TID>& results,
                                    ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SPGiSTNodeType node_type = static_cast<SPGiSTNodeType>(page->spgist_node_type);

    if (node_type == SPGiSTNodeType::LEAF)
    {
        // Scan leaf entries
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        for (uint16_t i = 0; i < page->spgist_count; ++i)
        {
            SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

            // Check MGA visibility
            if (!isEntryVisible(leaf->leaf_xmin, leaf->leaf_xmax, current_xid))
            {
                entry_ptr += leaf->leaf_size;
                continue;
            }

            // Extract value
            std::vector<uint8_t> value(leaf->leaf_valueSize);
            std::memcpy(value.data(), entry_ptr + sizeof(SBSPGiSTLeafTuple),
                       leaf->leaf_valueSize);

            // Check if value matches query
            if (opclass_->leafConsistent(value, query))
            {
                results.push_back(leaf->leaf_tid);
            }

            entry_ptr += leaf->leaf_size;
        }

        return Status::OK;
    }
    else // SPGiSTNodeType::INNER
    {
        // Extract inner node information
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        if (page->spgist_count == 0)
        {
            return Status::OK;
        }

        SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(entry_ptr);

        // Extract prefix
        std::vector<uint8_t> prefix(inner->inner_prefixSize);
        std::memcpy(prefix.data(), entry_ptr + sizeof(SBSPGiSTInnerTuple),
                   inner->inner_prefixSize);

        // Extract node labels
        auto config = opclass_->config();
        size_t label_size = config.labelSize > 0 ? config.labelSize : 4;
        size_t offset = sizeof(SBSPGiSTInnerTuple) + inner->inner_prefixSize;

        // Check each child to see if it could contain matches
        for (uint16_t i = 0; i < inner->inner_nNodes; ++i)
        {
            std::vector<uint8_t> label_data(label_size);
            std::memcpy(label_data.data(), entry_ptr + offset, label_size);
            offset += label_size;

            uint64_t child_page;
            std::memcpy(&child_page, entry_ptr + offset, sizeof(uint64_t));
            offset += sizeof(uint64_t);

            // Ask operator class if this child could contain matches
            if (opclass_->innerConsistent(prefix, label_data, query))
            {
                // Recursively search this child
                status = searchRecursive(child_page, query, current_xid, results, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }

        return Status::OK;
    }
}

Status SPGiSTIndex::splitNode(uint64_t page_num, ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    uint64_t current_xid = txn_manager_->getCurrentXid();

    // Collect all leaf values and their TIDs/xmin/xmax
    std::vector<LeafEntry> entries;
    std::vector<std::vector<uint8_t>> values;

    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

    for (uint16_t i = 0; i < page->spgist_count; ++i)
    {
        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

        LeafEntry entry;
        entry.value.resize(leaf->leaf_valueSize);
        std::memcpy(entry.value.data(), entry_ptr + sizeof(SBSPGiSTLeafTuple),
                   leaf->leaf_valueSize);
        entry.tid = leaf->leaf_tid;
        entry.xmin = leaf->leaf_xmin;
        entry.xmax = leaf->leaf_xmax;

        entries.push_back(entry);
        values.push_back(entry.value);

        entry_ptr += leaf->leaf_size;
    }

    // Ask operator class how to partition
    std::vector<uint8_t> prefix;
    std::vector<std::vector<uint8_t>> labels;
    std::vector<size_t> assignments;

    opclass_->pickSplit(values, prefix, labels, assignments);

    size_t num_partitions = labels.size();

    // Allocate child pages (one per partition)
    std::vector<uint64_t> child_pages(num_partitions);
    std::vector<SBSPGiSTPage*> child_page_ptrs(num_partitions);

    for (size_t i = 0; i < num_partitions; ++i)
    {
        status = allocatePage(&child_pages[i], ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = loadPage(child_pages[i], &child_page_ptrs[i], ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize child page as leaf
        std::memset(child_page_ptrs[i], 0, sizeof(SBSPGiSTPage));

        child_page_ptrs[i]->spgist_header.magic = K_MAGIC_SBRD;
        child_page_ptrs[i]->spgist_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
        child_page_ptrs[i]->spgist_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_SPGIST);
        child_page_ptrs[i]->spgist_header.page_size = 8192;
        child_page_ptrs[i]->spgist_header.page_id = static_cast<uint32_t>(child_pages[i]);
        child_page_ptrs[i]->spgist_header.checksum = 0;
        child_page_ptrs[i]->spgist_header.lsn = 0;
        child_page_ptrs[i]->spgist_header.flags = 0;
        std::memcpy(child_page_ptrs[i]->spgist_header.database_uuid, db_->uuid().bytes.data(), 16);

        child_page_ptrs[i]->spgist_index_uuid = index_uuid_;
        child_page_ptrs[i]->spgist_table_uuid = table_uuid_;
        child_page_ptrs[i]->spgist_flags = 0;
        child_page_ptrs[i]->spgist_node_type = static_cast<uint16_t>(SPGiSTNodeType::LEAF);
        child_page_ptrs[i]->spgist_count = 0;
        child_page_ptrs[i]->spgist_free_space = 8192 - sizeof(SBSPGiSTPage);
        child_page_ptrs[i]->spgist_opclass_id = opclass_->getOpClassId();
        child_page_ptrs[i]->spgist_parent_page = page_num;
        child_page_ptrs[i]->spgist_xmin = current_xid;
        child_page_ptrs[i]->spgist_xmax = 0;
    }

    // Distribute entries to child pages based on assignments
    for (size_t i = 0; i < entries.size(); ++i)
    {
        size_t partition = assignments[i];
        SBSPGiSTPage* child = child_page_ptrs[partition];

        // Calculate entry size
        uint16_t entry_size = sizeof(SBSPGiSTLeafTuple) + entries[i].value.size();

        // Add entry to child page
        uint8_t* child_entry_ptr = reinterpret_cast<uint8_t*>(child) +
                                   sizeof(SBSPGiSTPage) +
                                   (8192 - sizeof(SBSPGiSTPage) - child->spgist_free_space);

        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(child_entry_ptr);
        leaf->leaf_size = entry_size;
        leaf->leaf_valueSize = entries[i].value.size();
        leaf->leaf_tid = entries[i].tid;
        leaf->leaf_xmin = entries[i].xmin;  // Preserve original xmin
        leaf->leaf_xmax = entries[i].xmax;  // Preserve original xmax

        std::memcpy(child_entry_ptr + sizeof(SBSPGiSTLeafTuple),
                   entries[i].value.data(), entries[i].value.size());

        child->spgist_count++;
        child->spgist_free_space -= entry_size;
    }

    // Convert current page to inner node
    std::memset(page, 0, 8192);

    // Re-initialize page header
    page->spgist_header.magic = K_MAGIC_SBRD;
    page->spgist_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
    page->spgist_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_SPGIST);
    page->spgist_header.page_size = 8192;
    page->spgist_header.page_id = static_cast<uint32_t>(page_num);
    page->spgist_header.checksum = 0;
    page->spgist_header.lsn = 0;
    page->spgist_header.flags = 0;
    std::memcpy(page->spgist_header.database_uuid, db_->uuid().bytes.data(), 16);

    page->spgist_index_uuid = index_uuid_;
    page->spgist_table_uuid = table_uuid_;
    page->spgist_flags = 0;
    page->spgist_node_type = static_cast<uint16_t>(SPGiSTNodeType::INNER);
    page->spgist_count = 1;  // One inner tuple
    page->spgist_opclass_id = opclass_->getOpClassId();
    page->spgist_xmin = current_xid;
    page->spgist_xmax = 0;

    // Create inner tuple
    entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);
    SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(entry_ptr);

    inner->inner_nNodes = num_partitions;
    inner->inner_prefixSize = prefix.size();
    inner->inner_xmin = current_xid;
    inner->inner_xmax = 0;

    // Calculate total inner tuple size
    size_t label_size = labels.empty() ? 0 : labels[0].size();  // Assume fixed label size
    inner->inner_size = sizeof(SBSPGiSTInnerTuple) +
                       prefix.size() +
                       (num_partitions * label_size) +
                       (num_partitions * sizeof(uint64_t));

    // Write prefix
    entry_ptr += sizeof(SBSPGiSTInnerTuple);
    std::memcpy(entry_ptr, prefix.data(), prefix.size());
    entry_ptr += prefix.size();

    // Write node labels
    for (size_t i = 0; i < num_partitions; ++i)
    {
        std::memcpy(entry_ptr, labels[i].data(), label_size);
        entry_ptr += label_size;
    }

    // Write child page numbers
    std::memcpy(entry_ptr, child_pages.data(), num_partitions * sizeof(uint64_t));

    page->spgist_free_space = 8192 - sizeof(SBSPGiSTPage) - inner->inner_size;

    LOG_DEBUG(CATALOG, "SP-GiST: Split leaf page %lu into %zu partitions (%zu entries distributed)",
             page_num, labels.size(), entries.size());

    return Status::OK;
}

Status SPGiSTIndex::remove(const std::vector<uint8_t>& value,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    Status status = removeRecursive(root_page_, value, tid, current_xid, ctx);
    if (status == Status::OK)
    {
        deleted_count_++;
    }

    return status;
}

Status SPGiSTIndex::removeRecursive(uint64_t page_num,
                                   const std::vector<uint8_t>& value,
                                   const TID& tid,
                                   uint64_t current_xid,
                                   ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SPGiSTNodeType node_type = static_cast<SPGiSTNodeType>(page->spgist_node_type);

    if (node_type == SPGiSTNodeType::LEAF)
    {
        // Search for matching entry in leaf
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        for (uint16_t i = 0; i < page->spgist_count; ++i)
        {
            SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

            // Check if TID matches
            if (leaf->leaf_tid.gpid == tid.gpid && leaf->leaf_tid.slot == tid.slot)
            {
                // Found the entry - mark as deleted with xmax
                if (leaf->leaf_xmax == 0)  // Only if not already deleted
                {
                    leaf->leaf_xmax = current_xid;
                    LOG_DEBUG(CATALOG, "SP-GiST: Marked entry as deleted (TID %lu:%u, xmax=%lu)",
                             tid.gpid, tid.slot, current_xid);
                    return Status::OK;
                }
                else
                {
                    // Already deleted
                    return Status::NOT_FOUND;
                }
            }

            entry_ptr += leaf->leaf_size;
        }

        // Entry not found in this leaf
        return Status::NOT_FOUND;
    }
    else  // SPGiSTNodeType::INNER
    {
        // Extract inner node information
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);
        SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(entry_ptr);

        // Extract prefix
        std::vector<uint8_t> prefix(inner->inner_prefixSize);
        std::memcpy(prefix.data(), entry_ptr + sizeof(SBSPGiSTInnerTuple),
                   inner->inner_prefixSize);

        // Extract labels and child pages
        entry_ptr += sizeof(SBSPGiSTInnerTuple) + inner->inner_prefixSize;

        size_t label_size = (inner->inner_nNodes > 0) ?
                           (inner->inner_size - sizeof(SBSPGiSTInnerTuple) -
                            inner->inner_prefixSize - (inner->inner_nNodes * sizeof(uint64_t))) /
                            inner->inner_nNodes : 0;

        std::vector<SPGiSTNodeLabel> node_labels;
        uint8_t* child_pages_ptr = entry_ptr + (inner->inner_nNodes * label_size);
        uint64_t* child_pages = reinterpret_cast<uint64_t*>(child_pages_ptr);

        for (uint16_t i = 0; i < inner->inner_nNodes; ++i)
        {
            SPGiSTNodeLabel label;
            label.data.resize(label_size);
            std::memcpy(label.data.data(), entry_ptr + (i * label_size), label_size);
            label.child_page = child_pages[i];
            node_labels.push_back(label);
        }

        // Ask operator class which child to traverse
        SPGiSTTraversal traversal = opclass_->choose(prefix, node_labels, value);

        if (traversal.match_type == SPGiSTMatchType::MATCH_NODE)
        {
            // Descend into specific child
            uint64_t child_page = node_labels[traversal.node_index].child_page;
            return removeRecursive(child_page, value, tid, current_xid, ctx);
        }
        else
        {
            // For MATCH_SPLIT or MATCH_ADD_NODE, entry doesn't exist yet
            return Status::NOT_FOUND;
        }
    }
}

Status SPGiSTIndex::removeDeadEntries(const std::vector<TID>& dead_tids,
                                     uint64_t* entries_removed_out,
                                     uint64_t* pages_modified_out,
                                     ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    if (dead_tids.empty())
    {
        if (entries_removed_out) *entries_removed_out = 0;
        if (pages_modified_out) *pages_modified_out = 0;
        return Status::OK;
    }

    // Create a set for fast lookup
    std::set<TID> dead_set;
    for (const auto& tid : dead_tids)
    {
        dead_set.insert(tid);
    }

    uint64_t entries_removed = 0;
    uint64_t pages_modified = 0;

    Status status = removeDeadEntriesRecursive(root_page_, dead_set,
                                               &entries_removed, &pages_modified, ctx);

    if (status == Status::OK)
    {
        deleted_count_ -= entries_removed;
        LOG_INFO(CATALOG, "SP-GiST garbage collection: removed %lu entries from %lu pages",
                 entries_removed, pages_modified);
    }

    if (entries_removed_out) *entries_removed_out = entries_removed;
    if (pages_modified_out) *pages_modified_out = pages_modified;

    return status;
}

Status SPGiSTIndex::removeDeadEntriesRecursive(uint64_t page_num,
                                               const std::set<TID>& dead_set,
                                               uint64_t* entries_removed,
                                               uint64_t* pages_modified,
                                               ErrorContext* ctx)
{
    SBSPGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SPGiSTNodeType node_type = static_cast<SPGiSTNodeType>(page->spgist_node_type);

    if (node_type == SPGiSTNodeType::LEAF)
    {
        // Scan leaf entries and collect live ones
        std::vector<LeafEntry> live_entries;
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

        for (uint16_t i = 0; i < page->spgist_count; ++i)
        {
            SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

            // Check if this TID is in the dead set
            if (dead_set.find(leaf->leaf_tid) != dead_set.end())
            {
                // Dead entry - skip it
                (*entries_removed)++;
            }
            else
            {
                // Live entry - keep it
                LeafEntry entry;
                entry.value.resize(leaf->leaf_valueSize);
                std::memcpy(entry.value.data(), entry_ptr + sizeof(SBSPGiSTLeafTuple),
                           leaf->leaf_valueSize);
                entry.tid = leaf->leaf_tid;
                entry.xmin = leaf->leaf_xmin;
                entry.xmax = leaf->leaf_xmax;
                live_entries.push_back(entry);
            }

            entry_ptr += leaf->leaf_size;
        }

        // If we removed any entries, rewrite the page
        if (live_entries.size() != page->spgist_count)
        {
            // Clear entry area
            entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);
            uint16_t new_count = 0;
            uint16_t bytes_used = 0;

            // Rewrite live entries
            for (const auto& entry : live_entries)
            {
                uint16_t entry_size = sizeof(SBSPGiSTLeafTuple) + entry.value.size();

                SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);
                leaf->leaf_size = entry_size;
                leaf->leaf_valueSize = entry.value.size();
                leaf->leaf_tid = entry.tid;
                leaf->leaf_xmin = entry.xmin;
                leaf->leaf_xmax = entry.xmax;

                std::memcpy(entry_ptr + sizeof(SBSPGiSTLeafTuple),
                           entry.value.data(), entry.value.size());

                entry_ptr += entry_size;
                bytes_used += entry_size;
                new_count++;
            }

            // Update page metadata
            page->spgist_count = new_count;
            page->spgist_free_space = 8192 - sizeof(SBSPGiSTPage) - bytes_used;

            (*pages_modified)++;
        }

        return Status::OK;
    }
    else  // SPGiSTNodeType::INNER
    {
        // Extract inner node information
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);
        SBSPGiSTInnerTuple* inner = reinterpret_cast<SBSPGiSTInnerTuple*>(entry_ptr);

        // Calculate label size and get child pages
        entry_ptr += sizeof(SBSPGiSTInnerTuple) + inner->inner_prefixSize;

        size_t label_size = (inner->inner_nNodes > 0) ?
                           (inner->inner_size - sizeof(SBSPGiSTInnerTuple) -
                            inner->inner_prefixSize - (inner->inner_nNodes * sizeof(uint64_t))) /
                            inner->inner_nNodes : 0;

        uint8_t* child_pages_ptr = entry_ptr + (inner->inner_nNodes * label_size);
        uint64_t* child_pages = reinterpret_cast<uint64_t*>(child_pages_ptr);

        // Recursively clean all children
        for (uint16_t i = 0; i < inner->inner_nNodes; ++i)
        {
            status = removeDeadEntriesRecursive(child_pages[i], dead_set,
                                               entries_removed, pages_modified, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        return Status::OK;
    }
}

bool SPGiSTIndex::isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const
{
    // Firebird MGA visibility rules
    if (xmin > current_xid)
    {
        return false;
    }

    if (xmax != 0 && xmax <= current_xid)
    {
        return false;
    }

    // Check transaction states via TIP
    if (!txn_manager_->isVersionVisible(xmin, current_xid))
    {
        return false;
    }

    if (xmax != 0 && txn_manager_->isVersionVisible(xmax, current_xid))
    {
        return false;
    }

    return true;
}

Status SPGiSTIndex::loadPage(uint64_t page_num, SBSPGiSTPage** page, ErrorContext* ctx)
{
    void* page_buffer = nullptr;
    Status status = buffer_pool_->pinPage(static_cast<uint32_t>(page_num), &page_buffer, ctx);
    if (status != Status::OK)
    {
        if (ctx)
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Failed to pin SP-GiST page " + std::to_string(page_num);
        }
        return Status::IO_ERROR;
    }

    *page = reinterpret_cast<SBSPGiSTPage*>(page_buffer);
    return Status::OK;
}

Status SPGiSTIndex::allocatePage(uint64_t* page_num, ErrorContext* ctx)
{
    static uint64_t next_page = 100000; // Offset from other indexes
    *page_num = next_page++;

    void* page_buffer = nullptr;
    Status status = buffer_pool_->pinPage(static_cast<uint32_t>(*page_num), &page_buffer, ctx);
    if (status != Status::OK)
    {
        if (ctx)
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Failed to allocate SP-GiST page";
        }
        return Status::IO_ERROR;
    }

    return Status::OK;
}

// =============================================================================
// SPGiSTOperatorClassRegistry Implementation
// =============================================================================

SPGiSTOperatorClassRegistry& SPGiSTOperatorClassRegistry::instance()
{
    static SPGiSTOperatorClassRegistry registry;
    return registry;
}

void SPGiSTOperatorClassRegistry::registerOperatorClass(
    std::shared_ptr<SPGiSTOperatorClass> opclass)
{
    std::unique_lock lock(mutex_);
    uint32_t id = opclass->getOpClassId();
    std::string name = opclass->getOpClassName();

    opclasses_by_id_[id] = opclass;
    opclasses_by_name_[name] = opclass;

    LOG_INFO(CATALOG, "Registered SP-GiST operator class: %s (ID %u)",
             name.c_str(), id);
}

std::shared_ptr<SPGiSTOperatorClass> SPGiSTOperatorClassRegistry::getOperatorClass(
    uint32_t opclass_id) const
{
    std::shared_lock lock(mutex_);
    auto it = opclasses_by_id_.find(opclass_id);
    return (it != opclasses_by_id_.end()) ? it->second : nullptr;
}

std::shared_ptr<SPGiSTOperatorClass> SPGiSTOperatorClassRegistry::getOperatorClass(
    const std::string& name) const
{
    std::shared_lock lock(mutex_);
    auto it = opclasses_by_name_.find(name);
    return (it != opclasses_by_name_.end()) ? it->second : nullptr;
}

} // namespace scratchbird::core
