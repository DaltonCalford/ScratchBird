#include "scratchbird/core/btree.h"
#include "scratchbird/core/btree_page.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include <cstring>

namespace scratchbird::core
{

    // BTree::rangeScan implementation
    // PHASE 1 TASK 1.1.1: Added Snapshot parameter
    auto BTree::rangeScan(const std::vector<uint8_t> *start_key,
                          const std::vector<uint8_t> *end_key,
                          Snapshot *snapshot,
                          bool start_inclusive,
                          bool end_inclusive, ErrorContext *ctx) -> std::unique_ptr<BTreeIterator>
    {
        return std::make_unique<BTreeIterator>(this, start_key, end_key, snapshot, start_inclusive,
                                               end_inclusive);
    }

    // BTreeIterator implementation
    // PHASE 1 TASK 1.1.1: Added Snapshot parameter
    BTreeIterator::BTreeIterator(BTree *btree, const std::vector<uint8_t> *start_key,
                                 const std::vector<uint8_t> *end_key,
                                 Snapshot *snapshot,
                                 bool start_inclusive,
                                 bool end_inclusive)
        : btree_(btree), db_(btree->db_), snapshot_(snapshot), has_start_(start_key != nullptr),
          has_end_(end_key != nullptr), start_inclusive_(start_inclusive),
          end_inclusive_(end_inclusive), current_page_(0), current_slot_(0),
          current_tuple_index_(0), initialized_(false), exhausted_(false), scanned_count_(0)
    {
        if (has_start_)
        {
            start_key_ = *start_key;
        }
        if (has_end_)
        {
            end_key_ = *end_key;
        }
    }

    BTreeIterator::~BTreeIterator()
    {
        // Iterator cleanup
    }

    bool BTreeIterator::hasNext()
    {
        if (exhausted_)
        {
            return false;
        }

        if (!initialized_)
        {
            ErrorContext ctx;
            Status status = initialize(&ctx);
            if (status != Status::OK)
            {
                exhausted_ = true;
                return false;
            }
        }

        // Check if current position has valid data
        void *page_buffer;
        ErrorContext ctx;
        Status status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, &ctx);
        if (status != Status::OK)
        {
            exhausted_ = true;
            return false;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
        bool has_data = (current_slot_ < page->btr_count);

        db_->buffer_pool()->unpinPage(current_page_, false, &ctx);

        if (!has_data)
        {
            exhausted_ = true;
            return false;
        }

        return !exhausted_;
    }

    // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
    auto BTreeIterator::next(std::vector<uint8_t> *key_out, TID *tid_out,
                             ErrorContext *ctx) -> Status
    {
        if (exhausted_)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Iterator exhausted");
            return Status::NOT_FOUND;
        }

        if (!initialized_)
        {
            Status status = initialize(ctx);
            if (status != Status::OK)
            {
                exhausted_ = true;
                return status;
            }
        }

        // Pin current page
        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            exhausted_ = true;
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);
        auto *page = reinterpret_cast<SBBTreePage *>(page_data);

        // Task 17 MGA Phase 3.3: Get node structure to access btn_xmin/btn_xmax
        const auto *offsets = reinterpret_cast<const uint16_t *>(page_data + sizeof(SBBTreePage));
        const auto *node = reinterpret_cast<const SBBTreeNode *>(page_data + offsets[current_slot_]);

        // Get current entry using BTreePage helper
        std::vector<uint8_t> key;
        std::vector<uint64_t> tuple_ids;
        status = BTreePage::get_node(page_data, db_->page_size(), current_slot_, key, tuple_ids);

        db_->buffer_pool()->unpinPage(current_page_, false, ctx);

        if (status != Status::OK)
        {
            exhausted_ = true;
            SET_ERROR_CONTEXT(ctx, status, "Failed to read node from B-tree page");
            return status;
        }

        // Task 17 MGA Phase 3.3: Check visibility before returning entry
        if (!btree_->isEntryVisible(node->btn_xmin, node->btn_xmax, snapshot_))
        {
            // Entry not visible - skip to next entry
            status = moveToNextSlot(ctx);
            if (status != Status::OK)
            {
                exhausted_ = true;
                return Status::NOT_FOUND;
            }
            // Recursively call next() to get next visible entry
            return next(key_out, tid_out, ctx);
        }

        // Check if key is still in range
        if (!isKeyInRange(key))
        {
            exhausted_ = true;
            return Status::NOT_FOUND;
        }

        // Return current tuple
        if (current_tuple_index_ < tuple_ids.size())
        {
            if (key_out)
            {
                *key_out = key;
            }
            if (tid_out)
            {
                // PHASE 1.5: Convert stored uint64_t to TID struct
                uint64_t legacy_tid = tuple_ids[current_tuple_index_];
                *tid_out = convertLegacyTID(legacy_tid);
            }

            scanned_count_++;

            // Move to next tuple/slot
            current_tuple_index_++;
            if (current_tuple_index_ >= tuple_ids.size())
            {
                // Move to next slot
                status = moveToNextSlot(ctx);
                if (status != Status::OK)
                {
                    exhausted_ = true;
                }
            }

            return Status::OK;
        }

        exhausted_ = true;
        return Status::NOT_FOUND;
    }

    auto BTreeIterator::getCurrentKey(std::vector<uint8_t> *key_out) const -> Status
    {
        if (!initialized_ || exhausted_)
        {
            return Status::NOT_FOUND;
        }

        // Pin current page
        void *page_buffer;
        ErrorContext ctx;
        Status status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, &ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page_data = static_cast<uint8_t *>(page_buffer);

        std::vector<uint8_t> key;
        std::vector<uint64_t> tuple_ids;
        status = BTreePage::get_node(page_data, db_->page_size(), current_slot_, key, tuple_ids);

        db_->buffer_pool()->unpinPage(current_page_, false, &ctx);

        if (status == Status::OK && key_out)
        {
            *key_out = key;
        }

        return status;
    }

    auto BTreeIterator::initialize(ErrorContext *ctx) -> Status
    {
        if (initialized_)
        {
            return Status::OK;
        }

        // Find starting leaf page
        if (has_start_)
        {
            // Find leaf containing or after start key
            uint64_t leaf_page;
            Status status = btree_->find_leaf_page(start_key_, &leaf_page, false, ctx);
            if (status != Status::OK)
            {
                // If start key not found, try to find first key >= start_key
                // For now, start from leftmost leaf
                leaf_page = btree_->index_info_.idx_root_page;

                // Navigate to leftmost leaf
                void *page_buffer;
                status = db_->buffer_pool()->pinPage(leaf_page, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);

                // Walk down to leftmost leaf
                while ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) == 0)
                {
                    // Internal node - follow first child
                    std::vector<uint8_t> first_key;
                    std::vector<uint64_t> child_pages;
                    Status get_status =
                        BTreePage::get_node(static_cast<uint8_t *>(page_buffer), db_->page_size(),
                                            0, first_key, child_pages);

                    uint64_t next_page = (get_status == Status::OK && !child_pages.empty())
                                             ? child_pages[0]
                                             : page->btr_left_sibling;

                    db_->buffer_pool()->unpinPage(leaf_page, false, ctx);

                    if (next_page == 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid B-tree structure");
                        return Status::PAGE_CORRUPT;
                    }

                    leaf_page = next_page;
                    status = db_->buffer_pool()->pinPage(leaf_page, &page_buffer, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    page = reinterpret_cast<SBBTreePage *>(page_buffer);
                }

                db_->buffer_pool()->unpinPage(leaf_page, false, ctx);
            }

            current_page_ = leaf_page;
        }
        else
        {
            // No start key - begin from leftmost leaf
            current_page_ = btree_->index_info_.idx_root_page;

            void *page_buffer;
            Status status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);

            // Navigate to leftmost leaf
            while ((page->btr_flags & static_cast<uint16_t>(BTreeFlags::LEAF)) == 0)
            {
                // Get first child page
                std::vector<uint8_t> first_key;
                std::vector<uint64_t> child_pages;
                BTreePage::get_node(static_cast<uint8_t *>(page_buffer), db_->page_size(), 0,
                                    first_key, child_pages);

                uint64_t next_page = (!child_pages.empty()) ? child_pages[0] : 0;
                db_->buffer_pool()->unpinPage(current_page_, false, ctx);

                if (next_page == 0)
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid B-tree structure");
                    return Status::PAGE_CORRUPT;
                }

                current_page_ = next_page;
                status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                page = reinterpret_cast<SBBTreePage *>(page_buffer);
            }

            db_->buffer_pool()->unpinPage(current_page_, false, ctx);
        }

        current_slot_ = 0;
        current_tuple_index_ = 0;
        initialized_ = true;

        // Skip to first valid entry
        if (has_start_)
        {
            // Skip entries before start_key
            bool found_start = false;
            while (!found_start)
            {
                void *page_buffer;
                Status status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);

                if (current_slot_ >= page->btr_count)
                {
                    // Move to next page
                    db_->buffer_pool()->unpinPage(current_page_, false, ctx);
                    status = moveToNextPage(ctx);
                    if (status != Status::OK)
                    {
                        exhausted_ = true;
                        return status;
                    }
                    continue;
                }

                std::vector<uint8_t> key;
                std::vector<uint64_t> tuple_ids;
                status = BTreePage::get_node(static_cast<uint8_t *>(page_buffer), db_->page_size(),
                                             current_slot_, key, tuple_ids);

                db_->buffer_pool()->unpinPage(current_page_, false, ctx);

                if (status != Status::OK)
                {
                    return status;
                }

                int cmp = compareKeys(key, start_key_);
                if (cmp > 0 || (cmp == 0 && start_inclusive_))
                {
                    found_start = true;
                }
                else
                {
                    // Move to next slot
                    status = moveToNextSlot(ctx);
                    if (status != Status::OK)
                    {
                        exhausted_ = true;
                        return status;
                    }
                }
            }
        }

        return Status::OK;
    }

    auto BTreeIterator::moveToNextSlot(ErrorContext *ctx) -> Status
    {
        current_slot_++;
        current_tuple_index_ = 0;

        // Check if we need to move to next page
        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
        bool need_next_page = (current_slot_ >= page->btr_count);

        db_->buffer_pool()->unpinPage(current_page_, false, ctx);

        if (need_next_page)
        {
            return moveToNextPage(ctx);
        }

        return Status::OK;
    }

    auto BTreeIterator::moveToNextPage(ErrorContext *ctx) -> Status
    {
        // Pin current page to get sibling
        void *page_buffer;
        Status status = db_->buffer_pool()->pinPage(current_page_, &page_buffer, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        auto *page = reinterpret_cast<SBBTreePage *>(page_buffer);
        uint64_t next_page = page->btr_right_sibling;

        db_->buffer_pool()->unpinPage(current_page_, false, ctx);

        if (next_page == 0)
        {
            // No more pages
            return Status::NOT_FOUND;
        }

        current_page_ = next_page;
        current_slot_ = 0;
        current_tuple_index_ = 0;

        return Status::OK;
    }

    bool BTreeIterator::isKeyInRange(const std::vector<uint8_t> &key) const
    {
        // Check start bound
        if (has_start_)
        {
            int cmp = compareKeys(key, start_key_);
            if (cmp < 0 || (cmp == 0 && !start_inclusive_))
            {
                return false;
            }
        }

        // Check end bound
        if (has_end_)
        {
            int cmp = compareKeys(key, end_key_);
            if (cmp > 0 || (cmp == 0 && !end_inclusive_))
            {
                return false;
            }
        }

        return true;
    }

    int BTreeIterator::compareKeys(const std::vector<uint8_t> &k1,
                                   const std::vector<uint8_t> &k2) const
    {
        // Use the BTree's collation-aware comparison
        return btree_->compare_keys(k1, k2);
    }

} // namespace scratchbird::core
