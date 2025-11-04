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

namespace scratchbird::core
{

// Constants
constexpr uint16_t SPGIST_MAX_ENTRIES_PER_PAGE = 100;
constexpr uint16_t SPGIST_LEAF_THRESHOLD = 50; // Split leaf when exceeding this

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

    LOG_INFO(INDEX, "Initializing SP-GiST index %s with operator class %s",
             uuidToString(index_uuid_).c_str(),
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
    initPageHeader(&root->spgist_header, PageType::SPGIST_INDEX_PAGE);
    root->spgist_index_uuid = index_uuid_;
    root->spgist_table_uuid = table_uuid_;
    root->spgist_flags = static_cast<uint16_t>(SPGiSTFlags::ROOT);
    root->spgist_node_type = static_cast<uint16_t>(SPGiSTNodeType::LEAF);
    root->spgist_count = 0;
    root->spgist_free_space = 8192 - sizeof(SBSPGiSTPage);
    root->spgist_opclass_id = opclass_->getOpClassId();
    root->spgist_xmin = txn_manager_->getCurrentXid();
    root->spgist_total_entries = 0;

    LOG_INFO(INDEX, "SP-GiST index %s initialized with root page %lu",
             uuidToString(index_uuid_).c_str(), root_page_);

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

        LOG_DEBUG(INDEX, "SP-GiST: Inserted value into leaf page %lu (count=%u)",
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
                ctx->code = Status::INTERNAL_ERROR;
                ctx->message = "Empty inner node in SP-GiST";
            }
            return Status::INTERNAL_ERROR;
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
                LOG_DEBUG(INDEX, "SP-GiST: MATCH_ADD_NODE not yet implemented");
                break;

            case SPGiSTMatchType::MATCH_SPLIT:
                // Need to split current node
                LOG_DEBUG(INDEX, "SP-GiST: MATCH_SPLIT not yet implemented");
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

    // Collect all leaf values
    std::vector<std::vector<uint8_t>> values;
    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBSPGiSTPage);

    for (uint16_t i = 0; i < page->spgist_count; ++i)
    {
        SBSPGiSTLeafTuple* leaf = reinterpret_cast<SBSPGiSTLeafTuple*>(entry_ptr);

        std::vector<uint8_t> value(leaf->leaf_valueSize);
        std::memcpy(value.data(), entry_ptr + sizeof(SBSPGiSTLeafTuple),
                   leaf->leaf_valueSize);
        values.push_back(value);

        entry_ptr += leaf->leaf_size;
    }

    // Ask operator class how to partition
    std::vector<uint8_t> prefix;
    std::vector<std::vector<uint8_t>> labels;
    std::vector<size_t> assignments;

    opclass_->pickSplit(values, prefix, labels, assignments);

    // Convert leaf to inner node
    // TODO: Allocate child pages and distribute values

    LOG_DEBUG(INDEX, "SP-GiST: Split leaf page %lu into %zu partitions",
             page_num, labels.size());

    return Status::OK;
}

Status SPGiSTIndex::remove(const std::vector<uint8_t>& value,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // Logical deletion: find entry and set xmax
    // TODO: Implement entry lookup and deletion

    deleted_count_++;
    return Status::OK;
}

Status SPGiSTIndex::removeDeadEntries(uint64_t oldest_active_xid, ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // TODO: Traverse tree and physically remove entries where xmax < oldest_active_xid
    LOG_INFO(INDEX, "SP-GiST garbage collection: %lu dead entries to remove",
             deleted_count_);

    return Status::OK;
}

uint64_t SPGiSTIndex::getDeadEntryCount() const
{
    std::shared_lock lock(mutex_);
    return deleted_count_;
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
    uint8_t* page_data = buffer_pool_->pinPage(page_num, ctx);
    if (page_data == nullptr)
    {
        if (ctx)
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Failed to pin SP-GiST page " + std::to_string(page_num);
        }
        return Status::IO_ERROR;
    }

    *page = reinterpret_cast<SBSPGiSTPage*>(page_data);
    return Status::OK;
}

Status SPGiSTIndex::allocatePage(uint64_t* page_num, ErrorContext* ctx)
{
    static uint64_t next_page = 100000; // Offset from other indexes
    *page_num = next_page++;

    uint8_t* page_data = buffer_pool_->pinPage(*page_num, ctx);
    if (page_data == nullptr)
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

    LOG_INFO(INDEX, "Registered SP-GiST operator class: %s (ID %u)",
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
