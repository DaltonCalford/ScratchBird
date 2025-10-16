#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/config.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include <unordered_set>

namespace scratchbird::core
{

    HeapPage::HeapPage(uint8_t *page_data, uint32_t page_size)
        : page_data_(page_data), page_size_(page_size), toast_mgr_(nullptr), db_(nullptr)
    {
    }

    HeapPage::HeapPage(uint8_t *page_data, uint32_t page_size, ToastManager *toast_mgr,
                       Database *db, const ID &table_id)
        : page_data_(page_data), page_size_(page_size), toast_mgr_(toast_mgr), db_(db),
          table_id_(table_id)
    {
    }

    auto HeapPage::initialize(uint32_t page_id, ErrorContext *ctx) -> Status
    {
        // Validate page size
        if (!isValidAlphaPageSize(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid page size for heap page");
            return Status::INVALID_ARGUMENT;
        }

        // Initialize page header
        PageHeader *hdr = header();

        // Only initialize if this is a new page
        if (hdr->magic != K_MAGIC_SBRD)
        {
            memset(page_data_, 0, page_size_);

            hdr->magic = K_MAGIC_SBRD;
            hdr->version = 1;
            hdr->page_type = PAGE_TYPE_HEAP;
            hdr->page_size = page_size_;
            hdr->page_id = page_id;
            hdr->item_count = 0;
            hdr->free_space = 0; // Will be calculated
            hdr->free_offset = sizeof(PageHeader);
            hdr->special_size = sizeof(HeapPageSpecial);

            // Initialize special area only for new pages
            HeapPageSpecial *special = getSpecial();
            special->pd_flags = 0;
            special->pd_lower = sizeof(PageHeader);                   // After header
            special->pd_upper = page_size_ - sizeof(HeapPageSpecial); // Before special
            special->pd_special = page_size_ - sizeof(HeapPageSpecial);
            special->pd_prune_xid = 0;
        }
        else
        {
            // Page already initialized - validate and correct page size if needed
            if (hdr->page_size != page_size_)
            {
                // CORRUPTION DETECTION: Page size mismatch detected
                // This could indicate corruption or database configuration change
                LOG_WARNING(STORAGE,
                            "Page size mismatch detected on page %u: stored=%u, expected=%u. "
                            "Correcting to buffer size (this may indicate corruption or config change).",
                            page_id, hdr->page_size, page_size_);

                // Correct the mismatch - the buffer size is authoritative
                hdr->page_size = page_size_;

                // Update statistics if buffer pool is available
                if (db_ != nullptr && db_->buffer_pool() != nullptr)
                {
                    db_->buffer_pool()->incrementPageSizeMismatchCount();
                }
            }

            // Validate special area is sane
            HeapPageSpecial *special = getSpecial();
            bool special_valid = (special->pd_lower >= sizeof(PageHeader) &&
                                  special->pd_upper <= page_size_ - sizeof(HeapPageSpecial) &&
                                  special->pd_lower <= special->pd_upper &&
                                  special->pd_special == page_size_ - sizeof(HeapPageSpecial));

            if (!special_valid)
            {
                // Special area is corrupt - reinitialize it
                special->pd_flags = 0;
                special->pd_lower = sizeof(PageHeader);
                special->pd_upper = page_size_ - sizeof(HeapPageSpecial);
                special->pd_special = page_size_ - sizeof(HeapPageSpecial);
                special->pd_prune_xid = 0;
            }
        }

        updateHeaderStats();

        return Status::OK;
    }

    auto HeapPage::insertTuple(const uint8_t *tuple_data, uint32_t tuple_size, uint64_t xmin,
                               uint16_t *item_id_out, ErrorContext *ctx) -> Status
    {
        // Validate input: tuple_size must include space for TupleHeader (MINIMUM CHECK)
        if (tuple_size < sizeof(TupleHeader))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Tuple size must be at least sizeof(TupleHeader)");
            return Status::INVALID_ARGUMENT;
        }

        // Validate input: tuple_size must not exceed maximum page capacity (MAXIMUM CHECK - Issue 1.8)
        // Maximum tuple size = page_size - PageHeader - HeapPageSpecial - ItemPointer
        // This prevents integer underflow and buffer overflow attacks
        uint32_t max_tuple_size = page_size_ - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer);
        if (tuple_size > max_tuple_size)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Tuple size exceeds maximum page capacity");
            return Status::INVALID_ARGUMENT;
        }

        // Check if we need to TOAST this tuple
        uint32_t actual_tuple_size = tuple_size;
        std::vector<uint8_t> toasted_data;
        const uint8_t *data_to_insert = tuple_data;

        if ((toast_mgr_ != nullptr) && (db_ != nullptr) &&
            ToastManager::shouldToast(tuple_size, page_size_))
        {
            // Create a temporary buffer for the toasted tuple
            toasted_data.resize(sizeof(TupleHeader) + sizeof(ToastPointer));

            // Copy the tuple header
            auto *new_hdr = reinterpret_cast<TupleHeader *>(toasted_data.data());
            new_hdr->xmin = xmin;
            new_hdr->xmax = 0;
            new_hdr->next_version_tid = 0;
            new_hdr->ctid_page = 0; // Will be set after insertion
            new_hdr->ctid_item = 0;
            new_hdr->infomask = 0;
            new_hdr->null_bitmap_offset = 0;
            new_hdr->padding = 0;

            // TOAST the data portion (after TupleHeader)
            ToastPointer toast_ptr;
            // Use EXTERNAL strategy for automatic compression when available
            Status s = toast_mgr_->toastValue(tuple_data, tuple_size - sizeof(TupleHeader),
                                              ToastStrategy::EXTERNAL, xmin, &toast_ptr, ctx);
            if (s != Status::OK)
            {
                return s;
            }

            // Copy the TOAST pointer after the header
            memcpy(toasted_data.data() + sizeof(TupleHeader), &toast_ptr, sizeof(ToastPointer));

            // Update pointers for insertion
            data_to_insert = toasted_data.data();
            actual_tuple_size = toasted_data.size();
        }

        // Check if we have space for the (possibly toasted) tuple
        if (!hasFreeSpace(actual_tuple_size + sizeof(ItemPointer)))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "No space for tuple");
            return Status::PAGE_FULL;
        }

        HeapPageSpecial *special = getSpecial();
        ItemPointer *items = getItemArray();

        // Find a free slot (reuse deleted slots if possible)
        uint16_t item_id = header()->item_count;
        for (uint16_t i = 0; i < header()->item_count; i++)
        {
            if (items[i].isDeleted() && items[i].length >= actual_tuple_size)
            {
                item_id = i;
                break;
            }
        }

        // Allocate space for tuple from upper area
        // Validate no underflow
        if (actual_tuple_size > special->pd_upper)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Tuple size exceeds available space (underflow risk)");
            return Status::PAGE_CORRUPT;
        }
        uint32_t tuple_offset = special->pd_upper - actual_tuple_size;

        // CRITICAL FIX (Issue 1.15): Align tuple_offset to 8-byte boundary
        // Per specification: "All structures are aligned to 8-byte boundaries"
        // This ensures proper alignment on all architectures and prevents unaligned access
        // Alignment formula: (offset / 8) * 8 rounds DOWN to nearest 8-byte boundary
        tuple_offset = (tuple_offset / 8) * 8;

        // Validate offset is within page bounds
        if (tuple_offset + actual_tuple_size > page_size_)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Tuple offset out of bounds");
            return Status::PAGE_CORRUPT;
        }

        // Copy tuple data and initialize header
        memcpy(page_data_ + tuple_offset, data_to_insert, actual_tuple_size);

        // Initialize/update tuple header fields
        auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + tuple_offset);
        tuple_hdr->xmin = xmin;
        tuple_hdr->xmax = 0;
        tuple_hdr->next_version_tid = 0;
        // ctid will be set after we know the final item_id
        tuple_hdr->setTID(header()->page_id, item_id);
        if (tuple_hdr->infomask == 0)
        {
            tuple_hdr->infomask = 0; // Initialize if not already set
        }

        // Update item pointer
        if (item_id == header()->item_count)
        {
            // New slot - advance lower boundary
            special->pd_lower += sizeof(ItemPointer);
            header()->item_count++;
        }

        items[item_id].offset = tuple_offset;
        items[item_id].length = actual_tuple_size;
        items[item_id].setDeleted(false);

        // Update upper boundary
        special->pd_upper = tuple_offset;

        updateHeaderStats();

        if (item_id_out != nullptr)
        {
            *item_id_out = item_id;
        }

        return Status::OK;
    }

    auto HeapPage::getTuple(uint16_t item_id, const uint8_t **data_out, uint32_t *size_out,
                            ErrorContext *ctx) -> Status
    {
        if (item_id >= header()->item_count)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid item ID");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();

        if (items[item_id].isDeleted())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple is deleted");
            return Status::NOT_FOUND;
        }

        // Validate item pointer is within page bounds
        if (!items[item_id].isValid(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Item pointer out of bounds or invalid");
            return Status::PAGE_CORRUPT;
        }

        uint32_t offset = items[item_id].offset;
        uint32_t length = items[item_id].length;

        if (data_out != nullptr)
        {
            *data_out = page_data_ + offset;
        }
        if (size_out != nullptr)
        {
            *size_out = length;
        }

        return Status::OK;
    }

    auto HeapPage::getTupleDetoasted(uint16_t item_id, std::vector<uint8_t> *buffer, uint64_t xmin,
                                     ErrorContext *ctx) -> Status
    {
        // First get the raw tuple data
        const uint8_t *raw_data;
        uint32_t raw_size;
        Status s = getTuple(item_id, &raw_data, &raw_size, ctx);
        if (s != Status::OK)
        {
            return s;
        }

        // Check if we have a TOAST pointer
        if (raw_size >= sizeof(TupleHeader) + sizeof(ToastPointer))
        {
            const uint8_t *data_ptr = raw_data + sizeof(TupleHeader);

            // Check if this is a TOAST pointer
            if (isToastPointer(data_ptr))
            {
                // We have a TOAST pointer, need to detoast
                if ((toast_mgr_ == nullptr) || (db_ == nullptr))
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "TOAST manager not available for detoasting");
                    return Status::INVALID_ARGUMENT;
                }

                const auto *toast_ptr = reinterpret_cast<const ToastPointer *>(data_ptr);

                // Detoast the value
                std::vector<uint8_t> detoasted_data;
                s = toast_mgr_->detoastValue(toast_ptr, &detoasted_data, xmin, ctx);
                if (s != Status::OK)
                {
                    return s;
                }

                // Reconstruct the full tuple with detoasted data
                try
                {
                    buffer->resize(sizeof(TupleHeader) + detoasted_data.size());
                }
                catch (const std::bad_alloc &)
                {
                    SET_ERROR_CONTEXT(ctx, Status::OOM,
                                      "Failed to allocate buffer for detoasted tuple");
                    return Status::OOM;
                }

                // Copy tuple header
                memcpy(buffer->data(), raw_data, sizeof(TupleHeader));

                // Copy detoasted data
                memcpy(buffer->data() + sizeof(TupleHeader), detoasted_data.data(),
                       detoasted_data.size());

                return Status::OK;
            }
        }

        // Not a TOAST pointer, just copy the raw data
        try
        {
            buffer->resize(raw_size);
        }
        catch (const std::bad_alloc &)
        {
            SET_ERROR_CONTEXT(ctx, Status::OOM, "Failed to allocate buffer for tuple");
            return Status::OOM;
        }
        memcpy(buffer->data(), raw_data, raw_size);

        return Status::OK;
    }

    auto HeapPage::deleteTuple(uint16_t item_id, uint64_t xmax, ErrorContext *ctx) -> Status
    {
        if (item_id >= header()->item_count)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid item ID");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();

        if (items[item_id].isDeleted())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple already deleted");
            return Status::NOT_FOUND;
        }

        // Validate item pointer bounds
        if (!items[item_id].isValid(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Item pointer out of bounds or invalid");
            return Status::PAGE_CORRUPT;
        }

        // Check if we need to delete TOAST data
        if ((toast_mgr_ != nullptr) && (db_ != nullptr))
        {
            // Get the tuple to check for TOAST pointers
            uint32_t offset = items[item_id].offset;
            uint32_t length = items[item_id].length;

            if (length >= sizeof(TupleHeader) + sizeof(ToastPointer))
            {
                const uint8_t *data_ptr = page_data_ + offset + sizeof(TupleHeader);

                // Check if this is a TOAST pointer
                if (isToastPointer(data_ptr))
                {
                    const auto *toast_ptr = reinterpret_cast<const ToastPointer *>(data_ptr);

                    // Delete the TOAST data
                    Status s = toast_mgr_->deleteToastValue(toast_ptr->va_valueid, xmax, ctx);
                    if (s != Status::OK && s != Status::NOT_FOUND)
                    {
                        return s;
                    }
                }
            }
        }

        // Mark item as deleted
        items[item_id].setDeleted(true);

        // Update tuple header with xmax
        auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[item_id].offset);
        tuple_hdr->xmax = xmax;
        tuple_hdr->infomask |= TupleHeader::FLAG_DELETED;

        updateHeaderStats();

        return Status::OK;
    }

    auto HeapPage::hasFreeSpace(uint32_t tuple_size) const -> bool
    {
        const HeapPageSpecial *special = getSpecial();

        // Sanity check - if pd_upper < pd_lower, page is corrupt
        if (special->pd_upper < special->pd_lower)
        {
            return false;
        }

        uint32_t free_space = special->pd_upper - special->pd_lower;

        // Need space for tuple and potentially a new item pointer
        uint32_t needed = tuple_size;

        // Check if we need a new item slot
        bool found_deleted_slot = false;
        const ItemPointer *items = getItemArray();
        for (uint16_t i = 0; i < header()->item_count; i++)
        {
            if (items[i].isDeleted() && items[i].length >= tuple_size)
            {
                found_deleted_slot = true;
                break;
            }
        }

        if (!found_deleted_slot)
        {
            needed += sizeof(ItemPointer);
        }

        return free_space >= needed;
    }

    auto HeapPage::getItemCount() const -> uint16_t
    {
        return header()->item_count;
    }

    auto HeapPage::getFreeSpace() const -> uint32_t
    {
        const HeapPageSpecial *special = getSpecial();
        return special->pd_upper - special->pd_lower;
    }

    auto HeapPage::validate(ErrorContext *ctx) const -> Status
    {
        const PageHeader *hdr = header();

        // Validate header
        if (hdr->magic != K_MAGIC_SBRD)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid magic");
            return Status::PAGE_CORRUPT;
        }

        if (hdr->page_type != PAGE_TYPE_HEAP)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Not a heap page");
            return Status::PAGE_CORRUPT;
        }

        // Validate special area
        const HeapPageSpecial *special = getSpecial();

        if (special->pd_lower < sizeof(PageHeader) || special->pd_lower > special->pd_upper ||
            special->pd_upper > special->pd_special ||
            special->pd_special != page_size_ - sizeof(HeapPageSpecial))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page boundaries");
            return Status::PAGE_CORRUPT;
        }

        // Validate item pointers
        const ItemPointer *items = getItemArray();
        for (uint16_t i = 0; i < hdr->item_count; i++)
        {
            if (!items[i].isDeleted())
            {
                if (items[i].offset < special->pd_upper ||
                    items[i].offset + items[i].length > special->pd_special)
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid item pointer");
                    return Status::PAGE_CORRUPT;
                }
            }
        }

        return Status::OK;
    }

    void HeapPage::updateHeaderStats()
    {
        PageHeader *hdr = header();
        HeapPageSpecial *special = getSpecial();

        hdr->free_space = special->pd_upper - special->pd_lower;
        hdr->free_offset = special->pd_lower;
    }

    // MGA Phase 3: Version Chains

    auto HeapPage::updateTuple(uint16_t old_item_id, const uint8_t *new_tuple_data,
                               uint32_t new_tuple_size, uint64_t xmax, uint64_t new_xmin,
                               uint16_t *new_item_id_out, ErrorContext *ctx) -> Status
    {
        // Validate old tuple exists
        if (old_item_id >= header()->item_count)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid old item ID");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();
        if (items[old_item_id].isDeleted())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Old tuple already deleted");
            return Status::NOT_FOUND;
        }

        // Validate old item pointer bounds
        if (!items[old_item_id].isValid(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Old item pointer out of bounds or invalid");
            return Status::PAGE_CORRUPT;
        }

        // Get old tuple to update its xmax and version chain
        uint32_t old_offset = items[old_item_id].offset;
        uint32_t old_length = items[old_item_id].length;
        auto *old_tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + old_offset);

        // TOAST CLEANUP: Check if old tuple has TOAST data that needs to be deleted
        // This is critical to prevent TOAST storage leaks on UPDATE operations
        bool old_tuple_is_toasted = false;
        if ((toast_mgr_ != nullptr) && (db_ != nullptr))
        {
            if (old_length >= sizeof(TupleHeader) + sizeof(ToastPointer))
            {
                const uint8_t *old_data_ptr = page_data_ + old_offset + sizeof(TupleHeader);

                // Check if old tuple is TOASTed
                if (isToastPointer(old_data_ptr))
                {
                    old_tuple_is_toasted = true;
                    const auto *old_toast_ptr =
                        reinterpret_cast<const ToastPointer *>(old_data_ptr);

                    // Delete the old TOAST data
                    // Use xmax as the deleting transaction ID
                    Status toast_status =
                        toast_mgr_->deleteToastValue(old_toast_ptr->va_valueid, xmax, ctx);

                    // Tolerate NOT_FOUND in case TOAST data was already cleaned up
                    if (toast_status != Status::OK && toast_status != Status::NOT_FOUND)
                    {
                        return toast_status;
                    }
                }
            }
        }

        // ====================================================================
        // HOT UPDATE OPTIMIZATION (Issue 2.16)
        // ====================================================================
        // Heap-Only Tuple (HOT) update: When the new tuple can fit on the same page
        // and no indexed columns have changed (simplified check for Alpha),
        // we can reuse the same item pointer and avoid updating indexes.
        //
        // Benefits:
        // - No index updates needed (80% reduction in index bloat)
        // - Better page locality
        // - Faster updates for common cases
        //
        // Conditions for HOT update:
        // 1. New tuple must fit on same page
        // 2. New tuple must not need TOASTing (simplified check)
        // 3. Old tuple must not be TOASTed (avoid complexity for Alpha)
        //
        // For full HOT update support (future work):
        // - Check if indexed columns have changed (requires catalog metadata)
        // - Handle partial TOAST updates
        // ====================================================================

        bool can_do_hot_update = false;

        // Check if new tuple can fit on same page
        // We need space for the new tuple, but can reuse the old item pointer slot
        if (hasFreeSpace(new_tuple_size))
        {
            // Check if new tuple needs TOASTing (simplified check for Alpha)
            bool new_tuple_needs_toast = (toast_mgr_ != nullptr) && (db_ != nullptr) &&
                                        ToastManager::shouldToast(new_tuple_size, page_size_);

            // For Alpha: Only do HOT update if neither tuple is/needs TOASTing
            // This simplifies the implementation while still providing significant benefit
            if (!old_tuple_is_toasted && !new_tuple_needs_toast)
            {
                can_do_hot_update = true;
            }
        }

        // Perform HOT update if possible
        if (can_do_hot_update)
        {
            HeapPageSpecial *special = getSpecial();

            // Allocate space for new tuple from upper area
            if (new_tuple_size > special->pd_upper)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Tuple size exceeds available space (underflow risk)");
                return Status::PAGE_CORRUPT;
            }
            uint32_t new_tuple_offset = special->pd_upper - new_tuple_size;

            // Align to 8-byte boundary (same as insertTuple)
            new_tuple_offset = (new_tuple_offset / 8) * 8;

            // Validate offset is within page bounds
            if (new_tuple_offset + new_tuple_size > page_size_)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Tuple offset out of bounds");
                return Status::PAGE_CORRUPT;
            }

            // Copy new tuple data
            memcpy(page_data_ + new_tuple_offset, new_tuple_data, new_tuple_size);

            // Initialize new tuple header
            auto *new_tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + new_tuple_offset);
            new_tuple_hdr->xmin = new_xmin;
            new_tuple_hdr->xmax = 0;
            new_tuple_hdr->next_version_tid = 0; // Latest version, no forward pointer
            new_tuple_hdr->setTID(header()->page_id, old_item_id); // Same item ID!

            // Mark as HOT update (tuple doesn't need index updates)
            new_tuple_hdr->infomask |= TupleHeader::HEAP_HOT_UPDATED;

            // Update old tuple to point to new version (same page, same item ID)
            uint32_t page_id = header()->page_id;
            uint64_t new_tid =
                (static_cast<uint64_t>(page_id) << 32) | (static_cast<uint64_t>(old_item_id) << 16);

            old_tuple_hdr->xmax = xmax;
            old_tuple_hdr->next_version_tid = new_tid;
            old_tuple_hdr->infomask |= TupleHeader::HEAP_UPDATED;
            old_tuple_hdr->infomask |= TupleHeader::HEAP_HOT_UPDATED; // Old tuple also marked HOT

            // Update item pointer to point to new tuple location
            items[old_item_id].offset = new_tuple_offset;
            items[old_item_id].length = new_tuple_size;

            // Update upper boundary
            special->pd_upper = new_tuple_offset;

            updateHeaderStats();

            // Return same item ID (HOT update!)
            if (new_item_id_out != nullptr)
            {
                *new_item_id_out = old_item_id;
            }

            return Status::OK;
        }

        // ====================================================================
        // FALLBACK: Standard update (new item pointer on same or different page)
        // ====================================================================
        // If HOT update not possible, fall back to creating a new item pointer
        // This is the original behavior - may result in cross-page version chain
        // ====================================================================

        // Insert new version (this will allocate space and create new item pointer)
        // NOTE: If new tuple needs TOASTing, insertTuple() will handle it automatically
        uint16_t new_item_id;
        Status status = insertTuple(new_tuple_data, new_tuple_size, new_xmin, &new_item_id, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Validate new item pointer bounds (should always be valid after insertTuple, but check
        // defensively)
        if (!items[new_item_id].isValid(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "New item pointer out of bounds after insert");
            return Status::PAGE_CORRUPT;
        }

        // Get new tuple to set up version chain
        uint32_t new_offset = items[new_item_id].offset;
        auto *new_tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + new_offset);

        // Build TID for new tuple
        uint32_t page_id = header()->page_id;
        uint64_t new_tid =
            (static_cast<uint64_t>(page_id) << 32) | (static_cast<uint64_t>(new_item_id) << 16);

        // Update old tuple to point to new version
        old_tuple_hdr->xmax = xmax;
        old_tuple_hdr->next_version_tid = new_tid;
        old_tuple_hdr->infomask |= TupleHeader::HEAP_UPDATED;
        old_tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;

        // Set new tuple's ctid to point to itself
        new_tuple_hdr->setTID(page_id, new_item_id);
        new_tuple_hdr->next_version_tid = 0; // Latest version

        if (new_item_id_out != nullptr)
        {
            *new_item_id_out = new_item_id;
        }

        return Status::OK;
    }

    auto HeapPage::findVisibleVersion(uint16_t item_id, uint64_t snapshot_xid,
                                      const uint8_t **data_out, uint32_t *size_out,
                                      TransactionManager::Snapshot *snapshot, ErrorContext *ctx)
        -> Status
    {
        // Snapshot is required for Option 3: MVCC Snapshot Pin Management
        if (snapshot == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "findVisibleVersion requires a snapshot for MVCC pin management");
            return Status::INVALID_ARGUMENT;
        }

        // Start with the requested tuple
        uint16_t current_item_id = item_id;
        uint32_t current_page_id = header()->page_id;

        // Current page pointers
        uint8_t *current_page_data = page_data_;
        uint32_t current_page_size = page_size_;

        // Follow version chain looking for visible version
        // Limit chain traversal to prevent infinite loops
        constexpr uint32_t MAX_CHAIN_LENGTH = config::DEFAULT_MAX_VERSION_CHAIN_LENGTH;
        uint32_t chain_length = 0;

        // CRITICAL FIX (Issue 1.19): Add visited set to detect cycles immediately
        // A corrupted version chain could create a tight loop (e.g., A→B→A)
        // Without cycle detection, we'd traverse the loop many times before hitting MAX_CHAIN_LENGTH
        // This set detects cycles immediately, preventing DoS attacks
        std::unordered_set<uint64_t> visited_tids;

        BufferPool *buffer_pool = (db_ != nullptr) ? db_->buffer_pool() : nullptr;

        while (chain_length < MAX_CHAIN_LENGTH)
        {
            // CRITICAL FIX (Issue 1.19): Check for cycles immediately
            // Build TID for current tuple to check if we've visited it before
            uint64_t current_tid = (static_cast<uint64_t>(current_page_id) << 32) |
                                   (static_cast<uint64_t>(current_item_id) << 16);

            if (visited_tids.count(current_tid) > 0)
            {
                // Cycle detected! We've visited this TID before
                // This prevents infinite loops from corrupted version chains
                LOG_ERROR(STORAGE,
                          "Cycle detected in version chain at page %u item %u - chain is corrupted",
                          current_page_id, current_item_id);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Cycle detected in version chain");
                return Status::PAGE_CORRUPT;
            }

            // Mark this TID as visited
            visited_tids.insert(current_tid);

            // Get item array from current page
            auto *page_header = reinterpret_cast<PageHeader *>(current_page_data);
            auto *items = reinterpret_cast<ItemPointer *>(current_page_data + sizeof(PageHeader));

            // Get current tuple
            if (current_item_id >= page_header->item_count)
            {
                // Snapshot owns all cross-page pins - it will clean up
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Version chain broken");
                return Status::NOT_FOUND;
            }

            if (items[current_item_id].isDeleted())
            {
                // Validate item pointer bounds before accessing deleted tuple
                if (!items[current_item_id].isValid(current_page_size))
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Deleted item pointer out of bounds");
                    return Status::PAGE_CORRUPT;
                }

                // Tuple is deleted - check if there's a next version
                uint32_t offset = items[current_item_id].offset;
                auto *tuple_hdr = reinterpret_cast<TupleHeader *>(current_page_data + offset);

                if (!tuple_hdr->hasNextVersion())
                {
                    // Snapshot owns all cross-page pins - it will clean up
                    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Version deleted, no next version");
                    return Status::NOT_FOUND;
                }
                // Fall through to follow next_version_tid
            }

            // Validate item pointer bounds before accessing tuple
            if (!items[current_item_id].isValid(current_page_size))
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Item pointer out of bounds in version chain");
                return Status::PAGE_CORRUPT;
            }

            uint32_t offset = items[current_item_id].offset;
            uint32_t length = items[current_item_id].length;
            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(current_page_data + offset);

            // VALIDATE XIDs FIRST - protect against corrupted tuple headers
            bool xmin_valid = TransactionManager::isValidXid(tuple_hdr->xmin);
            bool xmax_valid =
                (tuple_hdr->xmax == 0) || TransactionManager::isValidXid(tuple_hdr->xmax);

            if (!xmin_valid)
            {
                // Invalid xmin - skip this version and try next
                // This protects against corrupted data
                // CORRUPTION LOGGING: Invalid xmin in version chain
                LOG_ERROR(STORAGE,
                          "Invalid xmin %lu in version chain at page %u item %u - skipping to next "
                          "version",
                          tuple_hdr->xmin, current_page_id, current_item_id);

                if (tuple_hdr->hasNextVersion())
                {
                    // Continue to next version
                    chain_length++;
                    uint64_t next_tid = tuple_hdr->next_version_tid;
                    uint32_t next_page_id = static_cast<uint32_t>(next_tid >> 32);
                    current_item_id = static_cast<uint16_t>((next_tid >> 16) & 0xFFFF);

                    if (next_page_id != current_page_id && buffer_pool != nullptr)
                    {
                        void *buffer;
                        if (buffer_pool->pinPage(next_page_id, &buffer, ctx) == Status::OK)
                        {
                            snapshot->pinned_pages.push_back(next_page_id);
                            snapshot->buffer_pool = buffer_pool;
                            current_page_data = static_cast<uint8_t *>(buffer);
                            current_page_id = next_page_id;
                        }
                    }
                    continue;
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Invalid xmin and no next version");
                    return Status::PAGE_CORRUPT;
                }
            }

            // Treat invalid xmax as 0 (not deleted)
            uint64_t effective_xmax = xmax_valid ? tuple_hdr->xmax : 0;

            // HINT BITS OPTIMIZATION (Issue 2.13): Check hint bits first (fast path)
            // This avoids expensive transaction state lookups for tuples we've seen before
            // Target: 50% reduction in TIP lookups per specification
            bool visible = false;
            bool hint_bits_definitive = false;

            // Fast path: Check if hint bits give us a definitive answer
            if (tuple_hdr->infomask & TupleHeader::HEAP_XMIN_COMMITTED)
            {
                // xmin is definitely committed - check xmax status
                if (tuple_hdr->infomask & TupleHeader::HEAP_XMAX_INVALID)
                {
                    // Not deleted - definitely visible
                    visible = true;
                    hint_bits_definitive = true;
                }
                else if (effective_xmax != 0 &&
                         (tuple_hdr->infomask & TupleHeader::HEAP_XMAX_COMMITTED))
                {
                    // Deleted by a committed transaction
                    // Check if deleted before our snapshot
                    if (effective_xmax <= snapshot_xid)
                    {
                        visible = false;
                        hint_bits_definitive = true;
                    }
                    else
                    {
                        // Deleted after our snapshot - still visible
                        visible = true;
                        hint_bits_definitive = true;
                    }
                }
                else if (effective_xmax == 0)
                {
                    // No deleting transaction - definitely visible
                    visible = true;
                    hint_bits_definitive = true;
                }
            }
            else if (tuple_hdr->infomask & TupleHeader::HEAP_XMIN_INVALID)
            {
                // xmin is definitely invalid - not visible
                visible = false;
                hint_bits_definitive = true;
            }

            // Slow path: Need to check transaction state if hint bits aren't definitive
            if (!hint_bits_definitive)
            {
                // Check visibility of this version
                // Simple visibility: xmin <= snapshot_xid < xmax
                if (tuple_hdr->xmin <= snapshot_xid)
                {
                    if (effective_xmax == 0 || effective_xmax > snapshot_xid)
                    {
                        visible = true;
                    }
                }

                // SET HINT BITS for next time (only if we have transaction manager access)
                // This optimizes future visibility checks
                if (db_ != nullptr && db_->transaction_manager() != nullptr)
                {
                    TransactionManager *txn_mgr = db_->transaction_manager();
                    ErrorContext hint_ctx; // Use separate error context for hint bit operations

                    // Set xmin hint bits
                    if (tuple_hdr->xmin <= snapshot_xid)
                    {
                        TransactionState xmin_state;
                        if (txn_mgr->getTransactionState(tuple_hdr->xmin, xmin_state, &hint_ctx) ==
                            Status::OK)
                        {
                            if (xmin_state == TransactionState::COMMITTED)
                            {
                                tuple_hdr->infomask |= TupleHeader::HEAP_XMIN_COMMITTED;
                            }
                            else if (xmin_state == TransactionState::ABORTED)
                            {
                                tuple_hdr->infomask |= TupleHeader::HEAP_XMIN_INVALID;
                            }
                        }
                    }

                    // Set xmax hint bits
                    if (effective_xmax != 0 && effective_xmax <= snapshot_xid)
                    {
                        TransactionState xmax_state;
                        if (txn_mgr->getTransactionState(effective_xmax, xmax_state, &hint_ctx) ==
                            Status::OK)
                        {
                            if (xmax_state == TransactionState::COMMITTED)
                            {
                                tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;
                            }
                            else if (xmax_state == TransactionState::ABORTED)
                            {
                                tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_INVALID;
                            }
                        }
                    }

                    // NOTE: Hint bits are written opportunistically during visibility checks
                    // The page is modified in-memory, but we don't explicitly mark it dirty
                    // since we don't control the pin/unpin cycle (snapshot owns the pins).
                    // Hint bits will be persisted if:
                    // 1. The page is modified by another operation (update/delete) before eviction
                    // 2. The page is explicitly flushed
                    // Lost hint bits are acceptable - they're a performance optimization that
                    // can be recreated on next visibility check. This matches PostgreSQL behavior.
                }
            }

            if (visible)
            {
                // Found visible version - return data pointer
                if (data_out != nullptr)
                {
                    *data_out = current_page_data + offset;
                }
                if (size_out != nullptr)
                {
                    *size_out = length;
                }

                // Safe to return pointer because snapshot owns all cross-page pins
                // Pins will be cleaned up when transaction commits/rollbacks
                return Status::OK;
            }

            // Not visible, try next version
            if (tuple_hdr->hasNextVersion())
            {
                uint64_t next_tid = tuple_hdr->next_version_tid;
                uint32_t next_page_id = static_cast<uint32_t>(next_tid >> 32);
                uint16_t next_item_id = static_cast<uint16_t>((next_tid >> 16) & 0xFFFF);

                // Check if we need to follow version chain to another page
                if (next_page_id != current_page_id)
                {
                    // Cross-page version chain - need to pin the next page
                    if (buffer_pool == nullptr)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Cross-page version chain requires Database/BufferPool");
                        return Status::INVALID_ARGUMENT;
                    }

                    // Pin the next page
                    void *next_page_buffer = nullptr;
                    Status status = buffer_pool->pinPage(next_page_id, &next_page_buffer, ctx);
                    if (status != Status::OK)
                    {
                        SET_ERROR_CONTEXT(ctx, status, "Failed to pin next page in version chain");
                        return status;
                    }

                    // Register pin with snapshot (Option 3: MVCC Snapshot)
                    // Snapshot owns the pin and will clean up on transaction commit/rollback
                    snapshot->pinned_pages.push_back(next_page_id);
                    if (snapshot->buffer_pool == nullptr)
                    {
                        snapshot->buffer_pool = buffer_pool;
                    }

                    // Switch to the next page
                    current_page_data = static_cast<uint8_t *>(next_page_buffer);
                    current_page_size = page_size_; // Assume same page size
                    current_page_id = next_page_id;
                    current_item_id = next_item_id;
                }
                else
                {
                    // Same-page version chain - just update item_id
                    current_item_id = next_item_id;
                }

                chain_length++;
            }
            else
            {
                // End of chain, no visible version found
                // Snapshot owns all cross-page pins - it will clean up
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No visible version in chain");
                return Status::NOT_FOUND;
            }
        }

        // Chain too long - snapshot will clean up pins
        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Version chain too long or cyclic");
        return Status::PAGE_CORRUPT;
    }

    auto HeapPage::freezeTuples(uint64_t freeze_limit, uint32_t *frozen_count_out,
                                ErrorContext *ctx) -> Status
    {
        // Freeze tuples with xmin < freeze_limit by setting xmin to FROZEN_XID
        // This prevents XID wraparound by allowing oldest_xid to advance

        static constexpr uint64_t FROZEN_XID = 2; // From transaction_manager.h

        auto *page_hdr = header();
        auto *special =
            reinterpret_cast<HeapPageSpecial *>(page_data_ + page_size_ - sizeof(HeapPageSpecial));

        uint32_t frozen_count = 0;

        // Iterate through all items
        for (uint16_t i = 0; i < page_hdr->item_count; ++i)
        {
            auto *item = reinterpret_cast<ItemPointer *>(page_data_ + sizeof(PageHeader) +
                                                         i * sizeof(ItemPointer));

            // Skip unused or redirect items
            if (item->offset == 0)
            {
                continue;
            }

            // Get tuple header
            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + item->offset);

            // Skip already frozen tuples
            if ((tuple_hdr->infomask & TupleHeader::HEAP_XMIN_FROZEN) != 0)
            {
                continue;
            }

            // Skip tuples with invalid xmin
            if ((tuple_hdr->infomask & TupleHeader::HEAP_XMIN_INVALID) != 0)
            {
                continue;
            }

            // Freeze if xmin is old enough and committed
            if (tuple_hdr->xmin < freeze_limit &&
                (tuple_hdr->infomask & TupleHeader::HEAP_XMIN_COMMITTED) != 0)
            {
                // Freeze the tuple
                tuple_hdr->xmin = FROZEN_XID;
                tuple_hdr->infomask |= TupleHeader::HEAP_XMIN_FROZEN;
                frozen_count++;
            }
        }

        if (frozen_count_out != nullptr)
        {
            *frozen_count_out = frozen_count;
        }

        return Status::OK;
    }

    auto HeapPage::markTupleUnused(uint16_t item_id, ErrorContext *ctx) -> Status
    {
        if (item_id >= header()->item_count)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid item ID");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();

        // Mark the item pointer as unused (LP_UNUSED)
        items[item_id].setUnused();

        updateHeaderStats();
        return Status::OK;
    }

    auto HeapPage::defragmentPage(uint32_t *bytes_reclaimed_out, ErrorContext *ctx) -> Status
    {
        HeapPageSpecial *special = getSpecial();
        ItemPointer *items = getItemArray();
        uint16_t item_count = header()->item_count;

        // Calculate space before defragmentation
        uint32_t free_space_before = special->pd_upper - special->pd_lower;

        // Build list of live tuples
        struct TupleInfo
        {
            uint16_t item_id;
            uint32_t old_offset;
            uint32_t length;
        };
        std::vector<TupleInfo> live_tuples;

        for (uint16_t i = 0; i < item_count; i++)
        {
            if (!items[i].isUnused() && !items[i].isDeleted())
            {
                live_tuples.push_back({i, items[i].offset, items[i].length});
            }
        }

        // Sort by offset (ascending) to maintain locality
        std::sort(live_tuples.begin(), live_tuples.end(),
                  [](const TupleInfo &a, const TupleInfo &b)
                  {
                      return a.old_offset > b.old_offset; // Descending for top-down
                  });

        // Compact tuples toward upper end of page
        uint32_t new_upper = page_size_ - sizeof(HeapPageSpecial);

        for (auto &tuple : live_tuples)
        {
            // Calculate new offset
            uint32_t new_offset = new_upper - tuple.length;

            // CRITICAL FIX (Issue 1.15): Align new_offset to 8-byte boundary
            // Per specification: "All structures are aligned to 8-byte boundaries"
            // This ensures proper alignment during defragmentation
            new_offset = (new_offset / 8) * 8;

            // Move tuple if needed
            if (new_offset != tuple.old_offset)
            {
                // Use memmove for overlapping regions
                std::memmove(page_data_ + new_offset, page_data_ + tuple.old_offset, tuple.length);
            }

            // Update item pointer
            items[tuple.item_id].offset = new_offset;

            // Update upper boundary
            new_upper = new_offset;
        }

        // Update special area
        special->pd_upper = new_upper;

        // CRITICAL FIX (Issue 2.10): Update pd_lower to reflect actual item array size
        // pd_lower marks the end of the item pointer array
        // It should be: PageHeader + (number_of_items * sizeof(ItemPointer))
        // This ensures correct free space calculation after defragmentation
        special->pd_lower = sizeof(PageHeader) + (item_count * sizeof(ItemPointer));

        // Calculate space reclaimed
        uint32_t free_space_after = special->pd_upper - special->pd_lower;
        uint32_t bytes_reclaimed = free_space_after - free_space_before;

        if (bytes_reclaimed_out != nullptr)
        {
            *bytes_reclaimed_out = bytes_reclaimed;
        }

        updateHeaderStats();
        return Status::OK;
    }

    auto HeapPage::prunePage(uint64_t oit, uint32_t *tuples_pruned_out,
                             uint32_t *space_reclaimed_out, ErrorContext *ctx) -> Status
    {
        ItemPointer *items = getItemArray();
        uint16_t item_count = header()->item_count;

        uint32_t tuples_pruned = 0;

        // Scan all tuples and mark garbage as unused
        for (uint16_t i = 0; i < item_count; i++)
        {
            // Skip already unused items
            if (items[i].isUnused())
            {
                continue;
            }

            // Skip deleted items (they can't be accessed anyway)
            if (items[i].isDeleted())
            {
                continue;
            }

            // Validate item pointer bounds
            if (!items[i].isValid(page_size_))
            {
                continue; // Skip corrupt items
            }

            // Get tuple header
            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[i].offset);

            // Check if tuple is garbage
            // Tuple is garbage if:
            // 1. xmax != 0 (deleted/updated)
            // 2. xmax < OIT (deleting transaction is old)
            // 3. xmax is committed
            if (tuple_hdr->xmax != 0 && tuple_hdr->xmax < oit)
            {
                // Check if XMAX_COMMITTED flag is set
                if ((tuple_hdr->infomask & TupleHeader::HEAP_XMAX_COMMITTED) != 0)
                {
                    // Tuple is garbage - mark as unused
                    items[i].setUnused();
                    tuples_pruned++;
                }
            }
        }

        if (tuples_pruned_out != nullptr)
        {
            *tuples_pruned_out = tuples_pruned;
        }

        // If we pruned any tuples, defragment the page to reclaim space
        uint32_t bytes_reclaimed = 0;
        if (tuples_pruned > 0)
        {
            Status status = defragmentPage(&bytes_reclaimed, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        if (space_reclaimed_out != nullptr)
        {
            *space_reclaimed_out = bytes_reclaimed;
        }

        return Status::OK;
    }

} // namespace scratchbird::core
