/**
 * GiST (Generalized Search Tree) Implementation
 * Phase 1: Core Framework
 *
 * Implements the GiST framework with extensible operator class support.
 * This is a generalized indexing structure that can implement various
 * index types (R-Tree, range indexes, etc.) through operator classes.
 */

#include "scratchbird/core/gist_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include <algorithm>
#include <cstring>
#include <cmath>
#include <queue>

namespace scratchbird::core
{

// Constants
constexpr uint16_t GIST_MAX_ENTRIES_PER_PAGE = 100; // Conservative estimate
constexpr double REINSER_FRACTION = 0.3; // R*-tree: reinsert 30% on overflow

// =============================================================================
// GiSTIndex Implementation
// =============================================================================

GiSTIndex::GiSTIndex(Database* db,
                     const ID& index_uuid,
                     const ID& table_uuid,
                     const std::vector<ID>& column_ids,
                     std::shared_ptr<GiSTOperatorClass> opclass)
    : db_(db)
    , buffer_pool_(db->buffer_pool())
    , txn_manager_(db->transaction_manager())
    , index_uuid_(index_uuid)
    , table_uuid_(table_uuid)
    , column_ids_(column_ids)
    , opclass_(opclass)
    , root_page_(0)
    , height_(0)
    , entry_count_(0)
    , deleted_count_(0)
{
}

GiSTIndex::~GiSTIndex()
{
    // Cleanup handled by buffer pool
}

Status GiSTIndex::initialize(ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    LOG_INFO(INDEX, "Initializing GiST index %s with operator class %s",
             uuidToString(index_uuid_).c_str(),
             opclass_->getOpClassName().c_str());

    // Allocate root page
    Status status = allocatePage(&root_page_, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Initialize root as empty leaf
    SBGiSTPage* root = nullptr;
    status = loadPage(root_page_, &root, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memset(root, 0, sizeof(SBGiSTPage));
    initPageHeader(&root->gist_header, PageType::GIST_INDEX_PAGE);
    root->gist_index_uuid = index_uuid_;
    root->gist_table_uuid = table_uuid_;
    root->gist_flags = static_cast<uint16_t>(GiSTFlags::ROOT) |
                       static_cast<uint16_t>(GiSTFlags::LEAF);
    root->gist_count = 0;
    root->gist_free_space = 8192 - sizeof(SBGiSTPage);
    root->gist_level = 0;
    root->gist_opclass_id = opclass_->getOpClassId();
    root->gist_xmin = txn_manager_->getCurrentXid();
    root->gist_total_entries = 0;

    height_ = 1;

    LOG_INFO(INDEX, "GiST index %s initialized with root page %lu",
             uuidToString(index_uuid_).c_str(), root_page_);

    return Status::OK;
}

Status GiSTIndex::insert(const GiSTPredicate& predicate,
                        const TID& tid,
                        uint64_t current_xid,
                        ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    uint64_t new_right_page = 0;
    GiSTPredicate new_right_pred;

    // Insert into tree (may cause split)
    Status status = insertRecursive(root_page_, predicate, tid, current_xid,
                                    &new_right_page, &new_right_pred, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // If root split, create new root
    if (new_right_page != 0)
    {
        uint64_t old_root = root_page_;
        status = allocatePage(&root_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        SBGiSTPage* new_root = nullptr;
        status = loadPage(root_page_, &new_root, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Initialize new root with two children
        std::memset(new_root, 0, sizeof(SBGiSTPage));
        initPageHeader(&new_root->gist_header, PageType::GIST_INDEX_PAGE);
        new_root->gist_index_uuid = index_uuid_;
        new_root->gist_table_uuid = table_uuid_;
        new_root->gist_flags = static_cast<uint16_t>(GiSTFlags::ROOT);
        new_root->gist_level = height_;
        new_root->gist_opclass_id = opclass_->getOpClassId();
        new_root->gist_xmin = current_xid;
        new_root->gist_count = 2;

        // Create predicates for old root and new right page
        // TODO: Add entries pointing to old_root and new_right_page
        // This requires storing the union predicates

        height_++;

        LOG_DEBUG(INDEX, "GiST root split, new root page %lu, height %u",
                 root_page_, height_);
    }

    entry_count_++;
    return Status::OK;
}

Status GiSTIndex::insertRecursive(uint64_t page_num,
                                  const GiSTPredicate& predicate,
                                  const TID& tid,
                                  uint64_t current_xid,
                                  uint64_t* new_right_page,
                                  GiSTPredicate* new_right_pred,
                                  ErrorContext* ctx)
{
    *new_right_page = 0;

    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;

    if (is_leaf)
    {
        // Insert into leaf page
        // Check if page has space
        uint16_t entry_size = sizeof(SBGiSTEntry) + predicate.data.size();
        if (page->gist_free_space < entry_size)
        {
            // Need to split
            return splitPage(page_num, nullptr, new_right_pred, new_right_page, ctx);
        }

        // Add entry to leaf
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) +
                            sizeof(SBGiSTPage) +
                            (8192 - sizeof(SBGiSTPage) - page->gist_free_space);

        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);
        entry->entry_size = entry_size;
        entry->entry_flags = 0;
        entry->entry_pred_size = predicate.data.size();
        entry->entry_row_id = tid;
        entry->entry_xmin = current_xid;
        entry->entry_xmax = 0;

        // Copy predicate data
        std::memcpy(entry_ptr + sizeof(SBGiSTEntry), predicate.data.data(),
                   predicate.data.size());

        page->gist_count++;
        page->gist_free_space -= entry_size;
        page->gist_total_entries++;

        return Status::OK;
    }
    else
    {
        // Internal node - choose subtree and recurse
        uint64_t chosen_child = 0;
        status = chooseSubtree(page_num, predicate, &chosen_child, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        uint64_t child_new_right = 0;
        GiSTPredicate child_new_pred;

        status = insertRecursive(chosen_child, predicate, tid, current_xid,
                                &child_new_right, &child_new_pred, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // If child split, insert the new pointer
        if (child_new_right != 0)
        {
            // TODO: Insert new entry pointing to child_new_right
            // Check if this page needs to split too
        }

        return Status::OK;
    }
}

Status GiSTIndex::search(const std::vector<uint8_t>& query,
                        GiSTStrategy strategy,
                        uint64_t current_xid,
                        std::vector<TID>& results,
                        ErrorContext* ctx)
{
    std::shared_lock lock(mutex_);

    if (root_page_ == 0)
    {
        // Empty index
        return Status::OK;
    }

    return searchRecursive(root_page_, query, strategy, current_xid, results, ctx);
}

Status GiSTIndex::searchRecursive(uint64_t page_num,
                                  const std::vector<uint8_t>& query,
                                  GiSTStrategy strategy,
                                  uint64_t current_xid,
                                  std::vector<TID>& results,
                                  ErrorContext* ctx)
{
    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;

    // Iterate through entries
    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
    for (uint16_t i = 0; i < page->gist_count; ++i)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

        // Skip deleted entries (MGA visibility check)
        if (!isEntryVisible(entry->entry_xmin, entry->entry_xmax, current_xid))
        {
            entry_ptr += entry->entry_size;
            continue;
        }

        // Extract predicate
        GiSTPredicate predicate;
        predicate.opclass_id = page->gist_opclass_id;
        predicate.data.resize(entry->entry_pred_size);
        std::memcpy(predicate.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                   entry->entry_pred_size);

        // Check consistency
        if (opclass_->consistent(predicate, query, strategy))
        {
            if (is_leaf)
            {
                // Add TID to results
                results.push_back(entry->entry_row_id);
            }
            else
            {
                // Recurse into child
                uint64_t child_page = entry->entry_child_page;
                status = searchRecursive(child_page, query, strategy, current_xid,
                                        results, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }

        entry_ptr += entry->entry_size;
    }

    return Status::OK;
}

Status GiSTIndex::remove(const GiSTPredicate& predicate,
                        const TID& tid,
                        uint64_t current_xid,
                        ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // Logical deletion: find entry and set xmax
    // TODO: Implement entry lookup and deletion
    // For now, just increment deleted count

    deleted_count_++;
    return Status::OK;
}

Status GiSTIndex::nearestNeighbor(const std::vector<uint8_t>& query,
                                  size_t k,
                                  uint64_t current_xid,
                                  std::vector<TID>& results,
                                  ErrorContext* ctx)
{
    std::shared_lock lock(mutex_);

    if (root_page_ == 0 || k == 0)
    {
        return Status::OK;
    }

    // Priority queue: (distance, page_num, is_leaf)
    using QueueEntry = std::tuple<double, uint64_t, bool, TID>;
    auto cmp = [](const QueueEntry& a, const QueueEntry& b) {
        return std::get<0>(a) > std::get<0>(b); // Min heap
    };
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, decltype(cmp)> pq(cmp);

    // Start with root
    pq.push({0.0, root_page_, false, TID()});

    while (!pq.empty() && results.size() < k)
    {
        auto [dist, page_num, is_result, result_tid] = pq.top();
        pq.pop();

        if (is_result)
        {
            // This is a result TID
            results.push_back(result_tid);
            continue;
        }

        // Load page and examine entries
        SBGiSTPage* page = nullptr;
        Status status = loadPage(page_num, &page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;

        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            if (!isEntryVisible(entry->entry_xmin, entry->entry_xmax, current_xid))
            {
                entry_ptr += entry->entry_size;
                continue;
            }

            GiSTPredicate predicate;
            predicate.opclass_id = page->gist_opclass_id;
            predicate.data.resize(entry->entry_pred_size);
            std::memcpy(predicate.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                       entry->entry_pred_size);

            double entry_dist = opclass_->distance(predicate, query);

            if (is_leaf)
            {
                pq.push({entry_dist, 0, true, entry->entry_row_id});
            }
            else
            {
                pq.push({entry_dist, entry->entry_child_page, false, TID()});
            }

            entry_ptr += entry->entry_size;
        }
    }

    return Status::OK;
}

Status GiSTIndex::splitPage(uint64_t page_num,
                            GiSTPredicate* left_pred,
                            GiSTPredicate* right_pred,
                            uint64_t* new_right_page,
                            ErrorContext* ctx)
{
    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Collect all entries
    std::vector<GiSTPredicate> entries;
    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
    for (uint16_t i = 0; i < page->gist_count; ++i)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

        GiSTPredicate pred;
        pred.opclass_id = page->gist_opclass_id;
        pred.data.resize(entry->entry_pred_size);
        std::memcpy(pred.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                   entry->entry_pred_size);
        entries.push_back(pred);

        entry_ptr += entry->entry_size;
    }

    // Use operator class to pick split
    std::vector<size_t> left_indices, right_indices;
    opclass_->picksplit(entries, left_indices, right_indices);

    // Allocate new page for right entries
    status = allocatePage(new_right_page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // TODO: Distribute entries to left and right pages
    // TODO: Compute union predicates for both pages

    LOG_DEBUG(INDEX, "GiST page %lu split into %lu and %lu",
             page_num, page_num, *new_right_page);

    return Status::OK;
}

Status GiSTIndex::chooseSubtree(uint64_t page_num,
                                const GiSTPredicate& predicate,
                                uint64_t* chosen_child,
                                ErrorContext* ctx)
{
    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    double min_penalty = std::numeric_limits<double>::max();
    uint64_t best_child = 0;

    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
    for (uint16_t i = 0; i < page->gist_count; ++i)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

        GiSTPredicate entry_pred;
        entry_pred.opclass_id = page->gist_opclass_id;
        entry_pred.data.resize(entry->entry_pred_size);
        std::memcpy(entry_pred.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                   entry->entry_pred_size);

        double penalty = opclass_->penalty(entry_pred, predicate);
        if (penalty < min_penalty)
        {
            min_penalty = penalty;
            best_child = entry->entry_child_page;
        }

        entry_ptr += entry->entry_size;
    }

    *chosen_child = best_child;
    return Status::OK;
}

Status GiSTIndex::removeDeadEntries(uint64_t oldest_active_xid, ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    // TODO: Traverse tree and physically remove entries where xmax < oldest_active_xid
    LOG_INFO(INDEX, "GiST garbage collection: %lu dead entries to remove",
             deleted_count_);

    return Status::OK;
}

uint64_t GiSTIndex::getDeadEntryCount() const
{
    std::shared_lock lock(mutex_);
    return deleted_count_;
}

bool GiSTIndex::isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const
{
    // Firebird MGA visibility rules
    if (xmin > current_xid)
    {
        // Created by a future transaction
        return false;
    }

    if (xmax != 0 && xmax <= current_xid)
    {
        // Deleted by a committed transaction visible to us
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

Status GiSTIndex::loadPage(uint64_t page_num, SBGiSTPage** page, ErrorContext* ctx)
{
    // Pin page from buffer pool
    uint8_t* page_data = buffer_pool_->pinPage(page_num, ctx);
    if (page_data == nullptr)
    {
        if (ctx)
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Failed to pin GiST page " + std::to_string(page_num);
        }
        return Status::IO_ERROR;
    }

    *page = reinterpret_cast<SBGiSTPage*>(page_data);
    return Status::OK;
}

Status GiSTIndex::allocatePage(uint64_t* page_num, ErrorContext* ctx)
{
    // Allocate new page via buffer pool
    // For now, use a simple counter (in production, use free page list)
    static uint64_t next_page = 1;
    *page_num = next_page++;

    uint8_t* page_data = buffer_pool_->pinPage(*page_num, ctx);
    if (page_data == nullptr)
    {
        if (ctx)
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Failed to allocate GiST page";
        }
        return Status::IO_ERROR;
    }

    return Status::OK;
}

// =============================================================================
// GiSTOperatorClassRegistry Implementation
// =============================================================================

GiSTOperatorClassRegistry& GiSTOperatorClassRegistry::instance()
{
    static GiSTOperatorClassRegistry registry;
    return registry;
}

void GiSTOperatorClassRegistry::registerOperatorClass(
    std::shared_ptr<GiSTOperatorClass> opclass)
{
    std::unique_lock lock(mutex_);
    uint32_t id = opclass->getOpClassId();
    std::string name = opclass->getOpClassName();

    opclasses_by_id_[id] = opclass;
    opclasses_by_name_[name] = opclass;

    LOG_INFO(INDEX, "Registered GiST operator class: %s (ID %u)",
             name.c_str(), id);
}

std::shared_ptr<GiSTOperatorClass> GiSTOperatorClassRegistry::getOperatorClass(
    uint32_t opclass_id) const
{
    std::shared_lock lock(mutex_);
    auto it = opclasses_by_id_.find(opclass_id);
    return (it != opclasses_by_id_.end()) ? it->second : nullptr;
}

std::shared_ptr<GiSTOperatorClass> GiSTOperatorClassRegistry::getOperatorClass(
    const std::string& name) const
{
    std::shared_lock lock(mutex_);
    auto it = opclasses_by_name_.find(name);
    return (it != opclasses_by_name_.end()) ? it->second : nullptr;
}

} // namespace scratchbird::core
