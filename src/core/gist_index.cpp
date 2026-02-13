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
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/page_manager.h"
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
    , tablespace_id_(0)
    , height_(0)
    , entry_count_(0)
    , deleted_count_(0)
{
}

GiSTIndex::~GiSTIndex()
{
    // Cleanup handled by buffer pool
}

Status GiSTIndex::create(Database* db,
                         const ID& index_uuid,
                         const ID& table_uuid,
                         const std::vector<ID>& column_ids,
                         std::shared_ptr<GiSTOperatorClass> opclass,
                         GPID root_gpid,
                         ErrorContext* ctx)
{
    if (!db || !opclass || root_gpid == 0)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to GiSTIndex::create");
        return Status::INVALID_ARGUMENT;
    }

    // Create index instance
    GiSTIndex index(db, index_uuid, table_uuid, column_ids, opclass);
    index.tablespace_id_ = getTablespaceID(root_gpid);
    index.root_page_ = getPageNumber(root_gpid);

    // Initialize (allocates root page)
    Status status = index.initialize(ctx);
    if (status != Status::OK)
    {
        return status;
    }

    return Status::OK;
}

Status GiSTIndex::create(Database* db,
                         const ID& index_uuid,
                         const ID& table_uuid,
                         const std::vector<ID>& column_ids,
                         std::shared_ptr<GiSTOperatorClass> opclass,
                         uint16_t tablespace_id,
                         uint32_t *root_page_out,
                         ErrorContext* ctx)
{
    if (!db || !root_page_out)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to GiSTIndex::create");
        return Status::INVALID_ARGUMENT;
    }

    PageManager *page_mgr = db->page_manager();
    if (!page_mgr)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page manager not available");
        return Status::INVALID_ARGUMENT;
    }

    GPID root_gpid = 0;
    Status status = page_mgr->allocatePageInTablespace(tablespace_id, &root_gpid, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    *root_page_out = static_cast<uint32_t>(getPageNumber(root_gpid));
    return create(db, index_uuid, table_uuid, column_ids, opclass, root_gpid, ctx);
}

Status GiSTIndex::create(Database* db,
                         const ID& index_uuid,
                         const ID& table_uuid,
                         const std::vector<ID>& column_ids,
                         std::shared_ptr<GiSTOperatorClass> opclass,
                         uint32_t *root_page_out,
                         ErrorContext* ctx)
{
    return create(db, index_uuid, table_uuid, column_ids, opclass,
                  PRIMARY_TABLESPACE_ID, root_page_out, ctx);
}

std::unique_ptr<GiSTIndex> GiSTIndex::open(Database* db,
                                            const ID& index_uuid,
                                            const ID& table_uuid,
                                            const std::vector<ID>& column_ids,
                                            std::shared_ptr<GiSTOperatorClass> opclass,
                                            GPID root_gpid,
                                            ErrorContext* ctx)
{
    if (!db || !opclass)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to GiSTIndex::open");
        return nullptr;
    }

    // Create index instance
    auto index = std::make_unique<GiSTIndex>(db, index_uuid, table_uuid, column_ids, opclass);

    // Set root page (don't call initialize, index already exists)
    index->root_page_ = getPageNumber(root_gpid);
    index->tablespace_id_ = getTablespaceID(root_gpid);

    // Load index metadata from root page (November 20, 2025)
    ErrorContext load_ctx;
    SBGiSTPage* root = nullptr;
    Status status = index->loadPage(index->root_page_, &root, &load_ctx);
    if (status == Status::OK && root)
    {
        // Calculate height by traversing to leaf
        uint64_t current_page = index->root_page_;
        uint32_t height = 0;
        while (current_page != 0)
        {
            SBGiSTPage* page = nullptr;
            if (index->loadPage(current_page, &page, &load_ctx) == Status::OK && page)
            {
                if (page->gist_level == 0)  // Level 0 = leaf
                {
                    break; // Reached leaf
                }
                height++;
                // Follow first child
                if (page->gist_count > 0)
                {
                    // Get first entry to access child page
                    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
                    SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);
                    current_page = entry->entry_child_page;
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        index->height_ = height;

        // For entry count, we'd need to traverse entire tree
        // Set to 0 for now - it will be updated as operations occur
        index->entry_count_ = 0;
        index->deleted_count_ = 0;
    }

    return index;
}

std::unique_ptr<GiSTIndex> GiSTIndex::open(Database* db,
                                            const ID& index_uuid,
                                            GPID root_gpid,
                                            ErrorContext* ctx)
{
    if (!db)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database pointer");
        return nullptr;
    }

    // Load index metadata from catalog
    auto catalog = db->catalog_manager();
    if (!catalog)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Catalog manager not available");
        return nullptr;
    }

    CatalogManager::IndexInfo index_info;
    Status status = catalog->getIndex(index_uuid, index_info, ctx);
    if (status != Status::OK)
    {
        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                         ("GiST index not found in catalog: " + index_uuid.toString()).c_str());
        return nullptr;
    }

    // Load root page to get operator class ID
    BufferPool* buffer_pool = db->buffer_pool();
    if (!buffer_pool)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Buffer pool not available");
        return nullptr;
    }

    uint32_t root_page = static_cast<uint32_t>(getPageNumber(root_gpid));
    SBGiSTPage* root = nullptr;
    Status pin_status = buffer_pool->pinPageGlobal(root_gpid, reinterpret_cast<void**>(&root), ctx);
    if (pin_status != Status::OK || !root)
    {
        SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to load GiST root page");
        return nullptr;
    }

    uint32_t opclass_id = root->gist_opclass_id;

    // Look up operator class from registry
    auto opclass = GiSTOperatorClassRegistry::instance().getOperatorClass(opclass_id);
    if (!opclass)
    {
        SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR,
                         ("GiST operator class not found in registry: ID " + std::to_string(opclass_id)).c_str());
        return nullptr;
    }

    // Call the full open() method with loaded metadata
    return open(db, index_uuid, index_info.table_id, index_info.column_ids, opclass, root_gpid, ctx);
}

GPID GiSTIndex::indexGPID(uint64_t page_num) const
{
    return makeGPID(tablespace_id_, page_num);
}

Status GiSTIndex::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext* ctx)
{
    return buffer_pool_->pinPageGlobal(indexGPID(page_num), buffer, ctx);
}

Status GiSTIndex::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext* ctx)
{
    return buffer_pool_->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
}

Status GiSTIndex::initialize(ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    LOG_INFO(CATALOG, "Initializing GiST index with operator class %s",
             opclass_->getOpClassName().c_str());

    // Allocate root page if not already assigned
    if (root_page_ == 0)
    {
        Status status = allocatePage(&root_page_, ctx);
        if (status != Status::OK)
        {
            return status;
        }
    }

    // Initialize root as empty leaf
    SBGiSTPage* root = nullptr;
    Status status = loadPage(root_page_, &root, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    std::memset(root, 0, sizeof(SBGiSTPage));

    // Initialize page header fields
    root->gist_header.magic = K_MAGIC_SBRD;
    root->gist_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
    root->gist_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_GIST);
    root->gist_header.page_size = db_->page_size();
    root->gist_header.page_id = root_page_;
    root->gist_header.checksum = 0;
    root->gist_header.lsn = 0;
    root->gist_header.flags = 0;
    root->gist_header.generation = 0;
    pageSetLower(root->gist_header, sizeof(SBGiSTPage));
    pageSetUpper(root->gist_header, db_->page_size());
    pageSetSpecial(root->gist_header, db_->page_size());

    // Initialize GiST-specific fields
    root->gist_index_uuid = index_uuid_;
    root->gist_table_uuid = table_uuid_;
    root->gist_flags = static_cast<uint16_t>(GiSTFlags::ROOT) |
                       static_cast<uint16_t>(GiSTFlags::LEAF);
    root->gist_count = 0;
    root->gist_free_space = db_->page_size() - sizeof(SBGiSTPage);
    root->gist_level = 0;
    root->gist_opclass_id = opclass_->getOpClassId();
    root->gist_xmin = txn_manager_->getCurrentXid();
    root->gist_total_entries = 0;

    height_ = 1;

    LOG_INFO(CATALOG, "GiST index %s initialized with root page %lu",
             index_uuid_.toString().c_str(), root_page_);

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

        // Initialize page header
        new_root->gist_header.magic = K_MAGIC_SBRD;
        new_root->gist_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1);
        new_root->gist_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_GIST);
        new_root->gist_header.page_size = db_->page_size();
        new_root->gist_header.page_id = root_page_;
        new_root->gist_header.checksum = 0;
        new_root->gist_header.lsn = 0;
        new_root->gist_header.flags = 0;
        new_root->gist_header.generation = 0;
        pageSetLower(new_root->gist_header, sizeof(SBGiSTPage));
        pageSetUpper(new_root->gist_header, db_->page_size());
        pageSetSpecial(new_root->gist_header, db_->page_size());

        // Initialize GiST-specific fields
        new_root->gist_index_uuid = index_uuid_;
        new_root->gist_table_uuid = table_uuid_;
        new_root->gist_flags = static_cast<uint16_t>(GiSTFlags::ROOT);
        new_root->gist_level = height_;
        new_root->gist_opclass_id = opclass_->getOpClassId();
        new_root->gist_xmin = current_xid;
        new_root->gist_xmax = 0;
        new_root->gist_count = 0;
        new_root->gist_free_space = db_->page_size() - sizeof(SBGiSTPage);

        // We need to get the union predicate for the old_root (left side)
        // Since splitPage was already called on old_root, we need to compute its union
        // For now, we'll scan the old_root page to get all entries and compute union

        SBGiSTPage* old_root_page = nullptr;
        status = loadPage(old_root, &old_root_page, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Collect all predicates from old_root to compute union
        std::vector<GiSTPredicate> old_root_predicates;
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(old_root_page) + sizeof(SBGiSTPage);
        for (uint16_t i = 0; i < old_root_page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);
            GiSTPredicate pred;
            pred.opclass_id = old_root_page->gist_opclass_id;
            pred.data.resize(entry->entry_pred_size);
            std::memcpy(pred.data.data(), entry_ptr + sizeof(SBGiSTEntry), entry->entry_pred_size);
            old_root_predicates.push_back(pred);
            entry_ptr += entry->entry_size;
        }

        GiSTPredicate old_root_pred = opclass_->unionPredicates(old_root_predicates);

        // Unpin old_root (we're done reading it)
        unpinIndexPage(static_cast<uint32_t>(old_root), false, ctx);

        // Add entry pointing to old_root (left child)
        uint8_t* new_root_entry_ptr = reinterpret_cast<uint8_t*>(new_root) + sizeof(SBGiSTPage);
        uint16_t left_entry_size = sizeof(SBGiSTEntry) + old_root_pred.data.size();

        SBGiSTEntry* left_entry = reinterpret_cast<SBGiSTEntry*>(new_root_entry_ptr);
        left_entry->entry_size = left_entry_size;
        left_entry->entry_flags = 0;
        left_entry->entry_pred_size = old_root_pred.data.size();
        left_entry->entry_child_page = old_root;
        left_entry->entry_xmin = current_xid;
        left_entry->entry_xmax = 0;

        // Copy predicate data
        std::memcpy(new_root_entry_ptr + sizeof(SBGiSTEntry), old_root_pred.data.data(),
                   old_root_pred.data.size());

        new_root->gist_count++;
        new_root->gist_free_space -= left_entry_size;

        // Add entry pointing to new_right_page (right child)
        new_root_entry_ptr += left_entry_size;
        uint16_t right_entry_size = sizeof(SBGiSTEntry) + new_right_pred.data.size();

        SBGiSTEntry* right_entry = reinterpret_cast<SBGiSTEntry*>(new_root_entry_ptr);
        right_entry->entry_size = right_entry_size;
        right_entry->entry_flags = 0;
        right_entry->entry_pred_size = new_right_pred.data.size();
        right_entry->entry_child_page = new_right_page;
        right_entry->entry_xmin = current_xid;
        right_entry->entry_xmax = 0;

        // Copy predicate data
        std::memcpy(new_root_entry_ptr + sizeof(SBGiSTEntry), new_right_pred.data.data(),
                   new_right_pred.data.size());

        new_root->gist_count++;
        new_root->gist_free_space -= right_entry_size;

        // Mark new root as dirty
        unpinIndexPage(static_cast<uint32_t>(root_page_), true, ctx);

        height_++;

        LOG_DEBUG(CATALOG, "GiST root split, new root page %lu, height %u, left child %lu, right child %lu",
                 root_page_, height_, old_root, new_right_page);
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
            // Need to split - unpin first
            unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);
            return splitPage(page_num, nullptr, new_right_pred, new_right_page, ctx);
        }

        // Add entry to leaf
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) +
                            sizeof(SBGiSTPage) +
                            (db_->page_size() - sizeof(SBGiSTPage) - page->gist_free_space);

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

        // Mark page as dirty
        unpinIndexPage(static_cast<uint32_t>(page_num), true, ctx);

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
            // Check if page has space for the new entry
            uint16_t entry_size = sizeof(SBGiSTEntry) + child_new_pred.data.size();
            if (page->gist_free_space < entry_size)
            {
                // Need to split this internal node too
                // First, unpin the current page
                unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);

                // Split this page and propagate upward
                return splitPage(page_num, nullptr, new_right_pred, new_right_page, ctx);
            }

            // Add entry for new child page
            uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) +
                                sizeof(SBGiSTPage) +
                                (db_->page_size() - sizeof(SBGiSTPage) - page->gist_free_space);

            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);
            entry->entry_size = entry_size;
            entry->entry_flags = 0;
            entry->entry_pred_size = child_new_pred.data.size();
            entry->entry_child_page = child_new_right;
            entry->entry_xmin = current_xid;
            entry->entry_xmax = 0;

            // Copy predicate data
            std::memcpy(entry_ptr + sizeof(SBGiSTEntry), child_new_pred.data.data(),
                       child_new_pred.data.size());

            page->gist_count++;
            page->gist_free_space -= entry_size;

            // Mark page as dirty
            unpinIndexPage(static_cast<uint32_t>(page_num), true, ctx);

            LOG_DEBUG(CATALOG, "Added split child entry to internal page %lu, new child %lu",
                     page_num, child_new_right);
        }
        else
        {
            // No split, just unpin the page (not modified)
            unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);
        }

        return Status::OK;
    }
}

Status GiSTIndex::search(const std::vector<uint8_t>& query,
                        GiSTStrategy strategy,
                        uint64_t current_xid,
                        std::vector<TID>* results,
                        ErrorContext* ctx)
{
    if (!results)
    {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Results vector cannot be null");
        return Status::INVALID_ARGUMENT;
    }

    std::shared_lock lock(mutex_);

    results->clear();

    if (root_page_ == 0)
    {
        // Empty index
        return Status::OK;
    }

    return searchRecursive(root_page_, query, strategy, current_xid, *results, ctx);
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
    // Use a recursive helper to traverse the tree and find the entry
    Status status = removeRecursive(root_page_, predicate, tid, current_xid, ctx);
    if (status == Status::OK)
    {
        deleted_count_++;
    }

    return status;
}

Status GiSTIndex::removeRecursive(uint64_t page_num,
                                  const GiSTPredicate& predicate,
                                  const TID& tid,
                                  uint64_t current_xid,
                                  ErrorContext* ctx)
{
    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;
    bool found = false;

    if (is_leaf)
    {
        // Search for the entry with matching TID
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            // Skip already deleted entries
            if (entry->entry_xmax != 0)
            {
                entry_ptr += entry->entry_size;
                continue;
            }

            // Check if this is the entry we're looking for (match by TID)
            if (entry->entry_row_id.gpid == tid.gpid && entry->entry_row_id.slot == tid.slot)
            {
                // Found it - set xmax for logical deletion (MGA compliance)
                entry->entry_xmax = current_xid;
                found = true;

                LOG_DEBUG(CATALOG, "GiST removed entry from leaf page %lu, TID (%u, %u), xmax=%lu",
                         page_num, tid.gpid, tid.slot, current_xid);

                // Mark page as dirty
                unpinIndexPage(static_cast<uint32_t>(page_num), true, ctx);
                return Status::OK;
            }

            entry_ptr += entry->entry_size;
        }

        // Not found on this leaf
        unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);
        return Status::NOT_FOUND;
    }
    else
    {
        // Internal node - find the subtree that might contain the entry
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            // Skip deleted entries
            if (entry->entry_xmax != 0)
            {
                entry_ptr += entry->entry_size;
                continue;
            }

            // Extract predicate
            GiSTPredicate entry_pred;
            entry_pred.opclass_id = page->gist_opclass_id;
            entry_pred.data.resize(entry->entry_pred_size);
            std::memcpy(entry_pred.data.data(), entry_ptr + sizeof(SBGiSTEntry),
                       entry->entry_pred_size);

            // Check if this subtree could contain the entry
            // Use "overlaps" strategy to check if predicates are consistent
            if (opclass_->consistent(entry_pred, predicate.data, GiSTStrategy::OVERLAPS))
            {
                uint64_t child_page = entry->entry_child_page;

                // Unpin current page before recursing
                unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);

                // Recurse into child
                status = removeRecursive(child_page, predicate, tid, current_xid, ctx);
                if (status == Status::OK)
                {
                    return Status::OK;
                }
                // If NOT_FOUND, continue searching other subtrees
                // (predicate overlap is not exact, so entry might be in another subtree)

                // Re-pin page for next iteration
                status = loadPage(page_num, &page, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
                // Restart loop (need to recalculate entry_ptr positions)
                i = (uint16_t)-1; // Will be incremented to 0
                continue;
            }

            entry_ptr += entry->entry_size;
        }

        // Not found in any subtree
        unpinIndexPage(static_cast<uint32_t>(page_num), false, ctx);
        return Status::NOT_FOUND;
    }
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

    // Load the new right page
    SBGiSTPage* right_page = nullptr;
    status = loadPage(*new_right_page, &right_page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    // Initialize right page header (copy metadata from left page)
    right_page->gist_index_uuid = page->gist_index_uuid;
    right_page->gist_table_uuid = page->gist_table_uuid;
    right_page->gist_flags = page->gist_flags & ~static_cast<uint16_t>(GiSTFlags::ROOT);  // Not root
    right_page->gist_count = 0;
    right_page->gist_free_space = db_->page_size() - sizeof(SBGiSTPage);  // Start with full free space
    right_page->gist_level = page->gist_level;
    right_page->gist_opclass_id = page->gist_opclass_id;
    right_page->gist_left_sibling = page_num;
    right_page->gist_right_sibling = page->gist_right_sibling;
    right_page->gist_parent_page = page->gist_parent_page;
    right_page->gist_xmin = page->gist_xmin;  // MGA: Same creation transaction
    right_page->gist_xmax = 0;  // MGA: Not deleted
    right_page->gist_lsn = 0;
    right_page->gist_total_entries = 0;
    right_page->gist_deleted_entries = 0;

    // Update left page sibling pointer
    page->gist_right_sibling = *new_right_page;

    // Distribute entries to left and right pages
    // First, collect all original entries (we need full entry data, not just predicates)
    std::vector<std::vector<uint8_t>> entry_data;
    entry_data.reserve(page->gist_count);

    uint8_t* entry_data_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
    for (uint16_t i = 0; i < page->gist_count; ++i)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_data_ptr);

        // Copy entire entry (header + predicate data)
        std::vector<uint8_t> entry_copy(entry->entry_size);
        std::memcpy(entry_copy.data(), entry_data_ptr, entry->entry_size);
        entry_data.push_back(std::move(entry_copy));

        entry_data_ptr += entry->entry_size;
    }

    // Clear left page (reset to empty)
    page->gist_count = 0;
    page->gist_free_space = db_->page_size() - sizeof(SBGiSTPage);

    // Write entries to left page based on left_indices
    uint8_t* left_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
    std::vector<GiSTPredicate> left_predicates;

    for (size_t idx : left_indices)
    {
        if (idx >= entry_data.size())
        {
            LOG_ERROR(CATALOG, "GiST split: Invalid left index %zu (max %zu)", idx, entry_data.size());
            continue;
        }

        const std::vector<uint8_t>& entry_bytes = entry_data[idx];
        SBGiSTEntry* src_entry = reinterpret_cast<SBGiSTEntry*>(
            const_cast<uint8_t*>(entry_bytes.data()));

        // Check if entry fits
        if (page->gist_free_space < entry_bytes.size())
        {
            LOG_ERROR(CATALOG, "GiST split: Left page full during distribution");
            return Status::PAGE_CORRUPT;
        }

        // Copy entry to left page
        std::memcpy(left_ptr, entry_bytes.data(), entry_bytes.size());

        // Collect predicate for union computation
        GiSTPredicate pred;
        pred.opclass_id = page->gist_opclass_id;
        pred.data.resize(src_entry->entry_pred_size);
        std::memcpy(pred.data.data(), entry_bytes.data() + sizeof(SBGiSTEntry),
                   src_entry->entry_pred_size);
        left_predicates.push_back(pred);

        left_ptr += entry_bytes.size();
        page->gist_count++;
        page->gist_free_space -= entry_bytes.size();
    }

    // Write entries to right page based on right_indices
    uint8_t* right_ptr = reinterpret_cast<uint8_t*>(right_page) + sizeof(SBGiSTPage);
    std::vector<GiSTPredicate> right_predicates;

    for (size_t idx : right_indices)
    {
        if (idx >= entry_data.size())
        {
            LOG_ERROR(CATALOG, "GiST split: Invalid right index %zu (max %zu)", idx, entry_data.size());
            continue;
        }

        const std::vector<uint8_t>& entry_bytes = entry_data[idx];
        SBGiSTEntry* src_entry = reinterpret_cast<SBGiSTEntry*>(
            const_cast<uint8_t*>(entry_bytes.data()));

        // Check if entry fits
        if (right_page->gist_free_space < entry_bytes.size())
        {
            LOG_ERROR(CATALOG, "GiST split: Right page full during distribution");
            return Status::PAGE_CORRUPT;
        }

        // Copy entry to right page
        std::memcpy(right_ptr, entry_bytes.data(), entry_bytes.size());

        // Collect predicate for union computation
        GiSTPredicate pred;
        pred.opclass_id = page->gist_opclass_id;
        pred.data.resize(src_entry->entry_pred_size);
        std::memcpy(pred.data.data(), entry_bytes.data() + sizeof(SBGiSTEntry),
                   src_entry->entry_pred_size);
        right_predicates.push_back(pred);

        right_ptr += entry_bytes.size();
        right_page->gist_count++;
        right_page->gist_free_space -= entry_bytes.size();
    }

    // Compute union predicates for both pages
    if (left_pred != nullptr && !left_predicates.empty())
    {
        *left_pred = opclass_->unionPredicates(left_predicates);
    }

    if (right_pred != nullptr && !right_predicates.empty())
    {
        *right_pred = opclass_->unionPredicates(right_predicates);
    }

    LOG_DEBUG(CATALOG, "GiST page %lu split into %lu (%u entries) and %lu (%u entries)",
             page_num, page_num, page->gist_count, *new_right_page, right_page->gist_count);

    // Unpin both pages (mark as dirty)
    unpinIndexPage(static_cast<uint32_t>(page_num), true, ctx);
    unpinIndexPage(static_cast<uint32_t>(*new_right_page), true, ctx);

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

Status GiSTIndex::removeDeadEntries(const std::vector<TID>& dead_tids,
                                   uint64_t* entries_removed_out,
                                   uint64_t* pages_modified_out,
                                   ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

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

    // Get OIT (Oldest Interesting Transaction) from TransactionManager
    // Per MGA_RULES.md Rule 10: Only physically remove entries with xmax < OIT
    uint64_t oit = txn_manager_->getOldestXid();

    LOG_DEBUG(CATALOG, "GiST GC: OIT=%lu, checking %zu dead TIDs", oit, dead_tids.size());

    // Create a set of TIDs for fast lookup
    std::set<TID> dead_tid_set(dead_tids.begin(), dead_tids.end());

    // Plan 01 Task D.5: GiST Recursive GC
    // Traverse tree recursively and physically remove entries where:
    // 1. entry_row_id is in dead_tids (heap confirmed dead) - LEAF NODES ONLY
    // 2. entry_xmax < OIT (no transaction can see them)
    uint64_t removed_count = 0;
    uint64_t pages_modified = 0;

    Status status = removeDeadEntriesRecursive(root_page_, dead_tid_set, oit,
                                              &removed_count, &pages_modified, ctx);

    if (status == Status::OK)
    {
        if (deleted_count_ >= removed_count)
        {
            deleted_count_ -= removed_count;
        }
        else
        {
            deleted_count_ = 0;
        }

        LOG_INFO(CATALOG, "GiST garbage collection: removed %lu dead entries from %lu pages, %lu remaining",
                 removed_count, pages_modified, deleted_count_);
    }

    if (entries_removed_out)
        *entries_removed_out = removed_count;
    if (pages_modified_out)
        *pages_modified_out = pages_modified;

    return status;
}

Status GiSTIndex::removeDeadEntriesRecursive(uint64_t page_num,
                                             const std::set<TID>& dead_tid_set,
                                             uint64_t oit,
                                             uint64_t* removed_count,
                                             uint64_t* pages_modified,
                                             ErrorContext* ctx)
{
    SBGiSTPage* page = nullptr;
    Status status = loadPage(page_num, &page, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;
    bool page_modified = false;

    if (!is_leaf)
    {
        // Internal node - first recursively clean children
        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            // Only visit live entries (we'll clean up dead ones below)
            if (entry->entry_xmax == 0 || entry->entry_xmax >= oit)
            {
                uint64_t child_page = entry->entry_child_page;

                // Unpin current page before recursing
                unpinIndexPage(static_cast<uint32_t>(page_num), page_modified, ctx);

                // Recurse into child
                status = removeDeadEntriesRecursive(child_page, dead_tid_set, oit,
                                                   removed_count, pages_modified, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // Re-pin page
                status = loadPage(page_num, &page, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                // Note: entry_ptr is now invalid, will be recalculated below
            }

            entry_ptr += entry->entry_size;
        }
    }

    // Now physically remove dead entries from this page
    // Collect live entries
    std::vector<std::vector<uint8_t>> live_entries;
    uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);

    for (uint16_t i = 0; i < page->gist_count; ++i)
    {
        SBGiSTEntry* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

        bool should_remove = false;

        // Check if entry is dead:
        // 1. xmax < OIT (no transaction can see it)
        // 2. For LEAF nodes only: TID must be in dead_tid_set (heap confirmed dead)
        if (entry->entry_xmax != 0 && entry->entry_xmax < oit)
        {
            if (is_leaf)
            {
                // Leaf node: Check if TID is in dead_tid_set
                if (dead_tid_set.find(entry->entry_row_id) != dead_tid_set.end())
                {
                    should_remove = true;
                }
            }
            else
            {
                // Internal node: Remove if xmax < OIT (no TID check needed)
                should_remove = true;
            }
        }

        if (should_remove)
        {
            // Entry is dead - skip it (physical removal)
            (*removed_count)++;
            page_modified = true;

            LOG_DEBUG(CATALOG, "GiST GC: removing dead entry from page %lu (leaf=%d), xmax=%lu < OIT=%lu",
                     page_num, is_leaf, entry->entry_xmax, oit);
        }
        else
        {
            // Entry is live - keep it
            std::vector<uint8_t> entry_copy(entry->entry_size);
            std::memcpy(entry_copy.data(), entry_ptr, entry->entry_size);
            live_entries.push_back(std::move(entry_copy));
        }

        entry_ptr += entry->entry_size;
    }

    // If we removed any entries, rewrite the page
    if (page_modified)
    {
        // Clear page
        page->gist_count = 0;
        page->gist_free_space = db_->page_size() - sizeof(SBGiSTPage);

        // Write back live entries
        uint8_t* write_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        for (const auto& entry_data : live_entries)
        {
            std::memcpy(write_ptr, entry_data.data(), entry_data.size());
            write_ptr += entry_data.size();
            page->gist_count++;
            page->gist_free_space -= entry_data.size();
        }

        // Update total entries count
        page->gist_total_entries -= (*removed_count);

        // Clear HAS_GARBAGE flag if all garbage removed
        if (page->gist_deleted_entries > 0)
        {
            page->gist_deleted_entries = 0;
            page->gist_flags &= ~static_cast<uint16_t>(GiSTFlags::HAS_GARBAGE);
        }

        // Increment pages_modified counter
        (*pages_modified)++;
    }

    // Unpin page (mark dirty if modified)
    unpinIndexPage(static_cast<uint32_t>(page_num), page_modified, ctx);

    return Status::OK;
}

Status GiSTIndex::updateTIDsAfterMigration(const std::unordered_map<TID, TID>& tid_mapping,
                                           uint64_t* tids_updated_out,
                                           uint64_t* pages_modified_out,
                                           ErrorContext* ctx)
{
    std::unique_lock lock(mutex_);

    if (tids_updated_out)
    {
        *tids_updated_out = 0;
    }
    if (pages_modified_out)
    {
        *pages_modified_out = 0;
    }

    if (root_page_ == 0 || tid_mapping.empty())
    {
        return Status::OK;
    }

    uint64_t tids_updated = 0;
    uint64_t pages_modified = 0;
    std::vector<uint64_t> stack;
    stack.push_back(root_page_);

    while (!stack.empty())
    {
        uint64_t page_num = stack.back();
        stack.pop_back();

        void* page_buffer = nullptr;
        Status status = pinIndexPage(page_num, &page_buffer, ctx);
        if (status != Status::OK || !page_buffer)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to pin GiST page during TID update");
            return Status::IO_ERROR;
        }

        auto* page = reinterpret_cast<SBGiSTPage*>(page_buffer);
        bool is_leaf = (page->gist_flags & static_cast<uint16_t>(GiSTFlags::LEAF)) != 0;
        bool page_dirty = false;

        uint8_t* entry_ptr = reinterpret_cast<uint8_t*>(page) + sizeof(SBGiSTPage);
        for (uint16_t i = 0; i < page->gist_count; ++i)
        {
            auto* entry = reinterpret_cast<SBGiSTEntry*>(entry_ptr);

            if (is_leaf)
            {
                TID old_tid = entry->entry_row_id;
                auto it = tid_mapping.find(old_tid);
                if (it != tid_mapping.end())
                {
                    entry->entry_row_id = it->second;
                    page_dirty = true;
                    ++tids_updated;
                }
            }
            else
            {
                stack.push_back(entry->entry_child_page);
            }

            entry_ptr += entry->entry_size;
        }

        if (page_dirty)
        {
            ++pages_modified;
        }

        unpinIndexPage(static_cast<uint32_t>(page_num), page_dirty, ctx);
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
    void* page_buffer = nullptr;
    Status status = pinIndexPage(page_num, &page_buffer, ctx);
    if (status != Status::OK)
    {
        if (ctx)
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Failed to pin GiST page " + std::to_string(page_num);
        }
        return Status::IO_ERROR;
    }

    *page = reinterpret_cast<SBGiSTPage*>(page_buffer);
    return Status::OK;
}

Status GiSTIndex::allocatePage(uint64_t* page_num, ErrorContext* ctx)
{
    PageManager* page_mgr = db_->page_manager();
    if (!page_mgr)
    {
        if (ctx && ctx->message.empty())
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Page manager not available for GiST allocation";
        }
        return Status::IO_ERROR;
    }

    GPID new_gpid = 0;
    Status status = page_mgr->allocatePageInTablespace(tablespace_id_, &new_gpid, ctx);
    if (status != Status::OK)
    {
        if (ctx && ctx->message.empty())
        {
            ctx->code = Status::IO_ERROR;
            ctx->message = "Failed to allocate GiST page in tablespace";
        }
        return Status::IO_ERROR;
    }

    *page_num = static_cast<uint64_t>(getPageNumber(new_gpid));

    void* page_buffer = nullptr;
    status = pinIndexPage(*page_num, &page_buffer, ctx);
    if (status != Status::OK)
    {
        return status;
    }

    SBGiSTPage* gist_page = reinterpret_cast<SBGiSTPage*>(page_buffer);
    gist_page->gist_header.magic = K_MAGIC_SBRD;
    gist_page->gist_header.version = 1;
    gist_page->gist_header.page_type = PAGE_TYPE_GIST;
    gist_page->gist_header.page_size = db_->page_size();
    gist_page->gist_header.page_id = *page_num;
    gist_page->gist_header.generation = 1;
    gist_page->gist_header.checksum = 0;
    gist_page->gist_header.flags = 0;
    gist_page->gist_header.lsn = 0;
    pageSetLower(gist_page->gist_header, sizeof(SBGiSTPage));
    pageSetUpper(gist_page->gist_header, db_->page_size());
    pageSetSpecial(gist_page->gist_header, db_->page_size());
    gist_page->gist_count = 0;
    gist_page->gist_level = 0;
    gist_page->gist_flags = 0;
    gist_page->gist_right_sibling = 0;
    gist_page->gist_left_sibling = 0;
    gist_page->gist_parent_page = 0;
    gist_page->gist_xmin = 0;
    gist_page->gist_xmax = 0;

    unpinIndexPage(*page_num, true, ctx);
    return Status::OK;
}

// =============================================================================
// Default Operator Class Implementation
// =============================================================================

/**
 * Default GiST operator class for simple binary comparison
 *
 * This provides basic functionality for indexes without specific operator classes.
 * It treats predicates as opaque byte arrays and uses simple byte comparison.
 */
class DefaultGiSTOperatorClass : public GiSTOperatorClass
{
public:
    uint32_t getOpClassId() const override { return 0; }  // ID 0 = default
    std::string getOpClassName() const override { return "default_ops"; }

    bool consistent(const GiSTPredicate& predicate,
                   const std::vector<uint8_t>& query,
                   GiSTStrategy strategy) const override
    {
        // For default ops, only support EQUALS strategy
        if (strategy != GiSTStrategy::EQUALS)
        {
            return false;
        }

        // Simple byte-wise comparison
        return predicate.data == query;
    }

    GiSTPredicate unionPredicates(
        const std::vector<GiSTPredicate>& entries) const override
    {
        // For default ops, union is just the first predicate (conservative)
        if (entries.empty())
        {
            return GiSTPredicate({}, 0);
        }
        return entries[0];
    }

    double penalty(const GiSTPredicate& base,
                  const GiSTPredicate& add) const override
    {
        // For default ops, penalty is always 0 (no preference)
        return 0.0;
    }

    void picksplit(const std::vector<GiSTPredicate>& entries,
                  std::vector<size_t>& left_indices,
                  std::vector<size_t>& right_indices) const override
    {
        // Simple 50/50 split
        left_indices.clear();
        right_indices.clear();

        for (size_t i = 0; i < entries.size(); ++i)
        {
            if (i < entries.size() / 2)
            {
                left_indices.push_back(i);
            }
            else
            {
                right_indices.push_back(i);
            }
        }
    }

    bool same(const GiSTPredicate& a,
             const GiSTPredicate& b) const override
    {
        return a.data == b.data;
    }
};

// =============================================================================
// GiSTOperatorClassRegistry Implementation
// =============================================================================

GiSTOperatorClassRegistry& GiSTOperatorClassRegistry::instance()
{
    static GiSTOperatorClassRegistry registry;

    // Register default operator class on first access
    static bool initialized = false;
    if (!initialized)
    {
        registry.registerOperatorClass(std::make_shared<DefaultGiSTOperatorClass>());
        initialized = true;
    }

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

    LOG_INFO(CATALOG, "Registered GiST operator class: %s (ID %u)",
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
