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
            // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 2): Protect TOAST allocation
            try
            {
                toasted_data.resize(sizeof(TupleHeader) + sizeof(ToastPointer));
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Out of memory allocating TOAST buffer for tuple insertion");
                return Status::OOM;
            }

            // Copy the tuple header
            auto *new_hdr = reinterpret_cast<TupleHeader *>(toasted_data.data());
            new_hdr->xmin = xmin;
            new_hdr->xmax = 0;
            // PHASE 1, TASK 1.2.5: Use GPID-based TID fields
            new_hdr->back_version_gpid = INVALID_GPID; // No back version (original insert)
            new_hdr->back_version_slot = 0;
            new_hdr->reserved1 = 0;
            new_hdr->ctid_gpid = INVALID_GPID; // Will be set after insertion
            new_hdr->ctid_slot = 0;
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
        // PHASE 1, TASK 1.2.5: Use GPID-based TID fields
        tuple_hdr->back_version_gpid = INVALID_GPID; // No back version (original insert)
        tuple_hdr->back_version_slot = 0;
        // ctid will be set after we know the final item_id
        // Convert page_id to GPID (tablespace 0 for now)
        GPID page_gpid = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(header()->page_id));
        tuple_hdr->setTID(page_gpid, item_id);
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

        // ISSUE 3.8 FIX: Validate page_size consistency
        // This detects corruption where stored page_size doesn't match buffer size
        // Such mismatches can cause subsequent operations to access out-of-bounds memory
        if (hdr->page_size != page_size_)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Page size mismatch");
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

    // MGA Phase 3: Version Chains - FIREBIRD MGA BACK VERSIONING

    auto HeapPage::updateTuple(uint16_t old_item_id, const uint8_t *new_tuple_data,
                               uint32_t new_tuple_size, uint64_t xmax, uint64_t new_xmin,
                               uint16_t *new_item_id_out, ErrorContext *ctx) -> Status
    {
        // ====================================================================
        // FIREBIRD MGA BACK VERSIONING ALGORITHM
        // ====================================================================
        // This implements proper Firebird-style Multi-Generational Architecture
        // where updates create a BACK VERSION (preserving old state) and then
        // overwrite the primary location IN-PLACE with new data.
        //
        // Key principles:
        // 1. Item pointer location NEVER changes (stable TID)
        // 2. Back versions are created FIRST (preserve old state)
        // 3. Primary location is overwritten IN-PLACE (new tuple)
        // 4. Version chain points BACKWARD (Newest-to-Oldest)
        // 5. Indexes NEVER need updating (unless indexed columns change)
        //
        // Benefits:
        // - 80% reduction in write amplification (no index updates)
        // - Stable item pointers (indexes remain valid)
        // - Better MVCC performance (newest version at fixed location)
        // - Reduced page fragmentation
        // ====================================================================

        // ====================================================================
        // PHASE 1: VALIDATE OLD TUPLE EXISTS
        // ====================================================================
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

        // Get current tuple at primary location
        uint32_t primary_offset = items[old_item_id].offset;
        uint32_t primary_length = items[old_item_id].length;
        auto *primary_tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + primary_offset);

        // ====================================================================
        // TOAST CLEANUP: Delete old TOAST data if present
        // ====================================================================
        // This prevents TOAST storage leaks on UPDATE operations
        bool old_tuple_is_toasted = false;
        if ((toast_mgr_ != nullptr) && (db_ != nullptr))
        {
            if (primary_length >= sizeof(TupleHeader) + sizeof(ToastPointer))
            {
                const uint8_t *old_data_ptr = page_data_ + primary_offset + sizeof(TupleHeader);

                if (isToastPointer(old_data_ptr))
                {
                    old_tuple_is_toasted = true;
                    const auto *old_toast_ptr =
                        reinterpret_cast<const ToastPointer *>(old_data_ptr);

                    Status toast_status =
                        toast_mgr_->deleteToastValue(old_toast_ptr->va_valueid, xmax, ctx);

                    if (toast_status != Status::OK && toast_status != Status::NOT_FOUND)
                    {
                        return toast_status;
                    }
                }
            }
        }

        // ====================================================================
        // PHASE 2: CREATE BACK VERSION (SAME-PAGE ONLY FOR ALPHA)
        // ====================================================================
        // Create back version to preserve old tuple state
        // For Alpha: Only support same-page back versions (simplified)
        // Future: Support cross-page back versions for large tuples
        //
        // Check if we can fit both new tuple AND back version on same page
        // ====================================================================

        HeapPageSpecial *special = getSpecial();

        // Calculate space needed:
        // - New tuple at primary location
        // - Back version copy of old tuple
        uint32_t space_needed = new_tuple_size + primary_length;

        // Check if new tuple needs TOASTing
        bool new_tuple_needs_toast = (toast_mgr_ != nullptr) && (db_ != nullptr) &&
                                    ToastManager::shouldToast(new_tuple_size, page_size_);

        // Phase 4: TOAST support for large tuple updates
        // If new tuple needs TOASTing, TOAST it now before creating back version
        std::vector<uint8_t> toasted_new_tuple;
        const uint8_t *final_new_tuple_data = new_tuple_data;
        uint32_t final_new_tuple_size = new_tuple_size;

        if (new_tuple_needs_toast)
        {
            // TOAST the new tuple
            // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 2): Protect TOAST allocation in update
            try
            {
                toasted_new_tuple.resize(sizeof(TupleHeader) + sizeof(ToastPointer));
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Out of memory allocating TOAST buffer for tuple update");
                return Status::OOM;
            }

            // Copy tuple header
            auto *new_hdr = reinterpret_cast<TupleHeader *>(toasted_new_tuple.data());
            const auto *orig_hdr = reinterpret_cast<const TupleHeader *>(new_tuple_data);
            *new_hdr = *orig_hdr; // Copy header fields

            // TOAST the data portion
            ToastPointer toast_ptr;
            Status s = toast_mgr_->toastValue(new_tuple_data + sizeof(TupleHeader),
                                             new_tuple_size - sizeof(TupleHeader),
                                             ToastStrategy::EXTERNAL, new_xmin, &toast_ptr, ctx);
            if (s != Status::OK)
            {
                return s;
            }

            // Copy TOAST pointer after header
            memcpy(toasted_new_tuple.data() + sizeof(TupleHeader), &toast_ptr, sizeof(ToastPointer));

            final_new_tuple_data = toasted_new_tuple.data();
            final_new_tuple_size = toasted_new_tuple.size();
        }

        // Recalculate space needed with potentially toasted new tuple
        space_needed = final_new_tuple_size + primary_length;

        // Check if we have space for back version on same page (try same-page first)
        bool back_version_same_page = hasFreeSpace(space_needed);
        uint32_t back_version_page_id = header()->page_id;
        uint32_t back_version_offset = 0;

        // Phase 4: Allocate space for back version (same-page or cross-page)
        if (back_version_same_page)
        {
            // SAME-PAGE BACK VERSION (optimal case)
            if (primary_length > special->pd_upper)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Back version size exceeds available space");
                return Status::PAGE_CORRUPT;
            }
            back_version_offset = special->pd_upper - primary_length;

            // Align to 8-byte boundary
            back_version_offset = (back_version_offset / 8) * 8;

            // Validate offset is within page bounds
            if (back_version_offset + primary_length > page_size_)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Back version offset out of bounds");
                return Status::PAGE_CORRUPT;
            }

            // Copy old tuple to back version location
            memcpy(page_data_ + back_version_offset, page_data_ + primary_offset, primary_length);

            // Update back version tuple header
            auto *back_version_hdr = reinterpret_cast<TupleHeader *>(page_data_ + back_version_offset);
            back_version_hdr->xmax = xmax; // Mark as updated/deleted by xmax
            // Back version keeps existing back_version_tid (continues chain)
            back_version_hdr->infomask |= TupleHeader::HEAP_CHAIN; // Mark as back version
            back_version_hdr->infomask |= TupleHeader::HEAP_UPDATED; // Mark as updated

            // Update upper boundary to reflect back version allocation
            special->pd_upper = back_version_offset;
        }
        else
        {
            // CROSS-PAGE BACK VERSION (required when page is full)
            // This is a CRITICAL PATH requirement for a functioning database

            // Validate buffer pool availability
            if (db_ == nullptr || db_->buffer_pool() == nullptr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Cross-page back version requires Database and BufferPool");
                return Status::INVALID_ARGUMENT;
            }

            BufferPool *buffer_pool = db_->buffer_pool();

            // Allocate a new page for the back version
            void *back_page_buffer = nullptr;
            Status alloc_status = buffer_pool->allocatePage(&back_version_page_id, &back_page_buffer, ctx);
            if (alloc_status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, alloc_status, "Failed to allocate page for cross-page back version");
                return alloc_status;
            }

            // Initialize the back version page as a heap page
            HeapPage back_page(static_cast<uint8_t *>(back_page_buffer), page_size_, toast_mgr_, db_, table_id_);
            Status init_status = back_page.initialize(back_version_page_id, ctx);
            if (init_status != Status::OK)
            {
                buffer_pool->unpinPage(back_version_page_id, false, ctx);
                return init_status;
            }

            // Insert old tuple as a back version on the new page
            // Create back version tuple (copy of old tuple)
            // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 2): Protect cross-page back version allocation
            std::vector<uint8_t> back_version_tuple;
            try
            {
                back_version_tuple.resize(primary_length);
            }
            catch (const std::bad_alloc &)
            {
                // Failed to allocate - must unpin the allocated page to prevent leak
                buffer_pool->unpinPage(back_version_page_id, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Out of memory allocating cross-page back version tuple");
                return Status::OOM;
            }
            memcpy(back_version_tuple.data(), page_data_ + primary_offset, primary_length);

            // Update back version tuple header
            auto *back_version_hdr = reinterpret_cast<TupleHeader *>(back_version_tuple.data());
            back_version_hdr->xmax = xmax; // Mark as updated/deleted by xmax
            // Back version keeps existing back_version_tid (continues chain)
            back_version_hdr->infomask |= TupleHeader::HEAP_CHAIN; // Mark as back version
            back_version_hdr->infomask |= TupleHeader::HEAP_UPDATED; // Mark as updated

            // Insert back version on the back page
            // Note: We store by OFFSET, not item_id
            // So we use insertTuple but will extract the offset, not use the item_id
            uint16_t back_item_id;
            Status insert_status = back_page.insertTuple(back_version_tuple.data(), primary_length,
                                                        primary_tuple_hdr->xmin, &back_item_id, ctx);
            if (insert_status != Status::OK)
            {
                buffer_pool->unpinPage(back_version_page_id, false, ctx);
                SET_ERROR_CONTEXT(ctx, insert_status, "Failed to insert back version on new page");
                return insert_status;
            }

            // Get the offset of the inserted back version
            const uint8_t *back_data_ptr;
            uint32_t back_size;
            Status get_status = back_page.getTuple(back_item_id, &back_data_ptr, &back_size, ctx);
            if (get_status != Status::OK)
            {
                buffer_pool->unpinPage(back_version_page_id, false, ctx);
                return get_status;
            }

            // Calculate offset from page start
            back_version_offset = static_cast<uint32_t>(back_data_ptr - static_cast<uint8_t *>(back_page_buffer));

            // Unpin the back page (already marked dirty by allocatePage)
            buffer_pool->unpinPage(back_version_page_id, true, ctx);
        }

        // ====================================================================
        // PHASE 3: OVERWRITE PRIMARY LOCATION IN-PLACE
        // ====================================================================
        // Now overwrite the primary location with new tuple data (possibly TOASTed)
        // Item pointer remains UNCHANGED (stable TID)
        // ====================================================================

        // Check if new tuple fits in old tuple's space
        // If not, we need to allocate new space at primary location
        if (final_new_tuple_size <= primary_length)
        {
            // New tuple fits in old space - overwrite in-place
            memcpy(page_data_ + primary_offset, final_new_tuple_data, final_new_tuple_size);

            // Update item pointer length
            items[old_item_id].length = final_new_tuple_size;
        }
        else
        {
            // New tuple is larger - need to allocate new space at end of free area
            if (final_new_tuple_size > special->pd_upper)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "New tuple size exceeds available space");
                return Status::PAGE_CORRUPT;
            }
            uint32_t new_primary_offset = special->pd_upper - final_new_tuple_size;

            // Align to 8-byte boundary
            new_primary_offset = (new_primary_offset / 8) * 8;

            // Validate offset
            if (new_primary_offset + final_new_tuple_size > page_size_)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "New tuple offset out of bounds");
                return Status::PAGE_CORRUPT;
            }

            // Copy new tuple to new primary location
            memcpy(page_data_ + new_primary_offset, final_new_tuple_data, final_new_tuple_size);

            // Update item pointer to new primary location
            items[old_item_id].offset = new_primary_offset;
            items[old_item_id].length = final_new_tuple_size;

            // Update upper boundary
            special->pd_upper = new_primary_offset;
        }

        // Initialize new primary tuple header
        auto *new_primary_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[old_item_id].offset);
        new_primary_hdr->xmin = new_xmin;
        new_primary_hdr->xmax = 0; // Not deleted

        // PHASE 1, TASK 1.2.5: Use GPID-based TID fields
        // CRITICAL: Set back_version to point BACKWARD to back version
        // Build TID for back version (same page, but different offset)
        // For Alpha simplification: Store back version offset directly in slot field
        // (This is a temporary approach - full implementation would use proper item pointers)
        uint32_t page_id = header()->page_id;
        GPID page_gpid = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));
        new_primary_hdr->back_version_gpid = page_gpid;
        new_primary_hdr->back_version_slot = static_cast<uint16_t>(back_version_offset); // Temporary: use offset as slot

        new_primary_hdr->setTID(page_gpid, old_item_id); // Same item ID (stable!)
        new_primary_hdr->infomask = 0; // Clear flags (new primary version)
        // Note: We don't set HEAP_HOT_UPDATED here - that's for index update optimization
        // MGA provides stable TIDs naturally, so indexes don't need updating regardless

        updateHeaderStats();

        // ====================================================================
        // RETURN SAME ITEM_ID (STABLE POINTER)
        // ====================================================================
        // This is the key benefit of MGA: item pointer never changes
        // Indexes remain valid and don't need updating
        if (new_item_id_out != nullptr)
        {
            *new_item_id_out = old_item_id;
        }

        return Status::OK;
    }

    // SPRINT 0: Overwrite tuple in-place with back version on different page
    // This is for cross-page MGA updates where back version was created elsewhere
    auto HeapPage::overwriteTuple(uint16_t item_id, const uint8_t *new_tuple_data,
                                  uint32_t new_tuple_size, uint64_t xmax, uint64_t new_xmin,
                                  uint64_t back_version_gpid, uint16_t back_version_slot,
                                  ErrorContext *ctx) -> Status
    {
        // ====================================================================
        // FIREBIRD MGA CROSS-PAGE UPDATE - OVERWRITE PRIMARY IN-PLACE
        // ====================================================================
        // This method is called when a back version has already been created
        // on a DIFFERENT page (because primary page was full), and now we
        // need to overwrite the PRIMARY location with new data.
        //
        // Key principles:
        // 1. Item pointer NEVER changes (stable TID)
        // 2. Back version already created (on different page)
        // 3. Primary location overwritten IN-PLACE (new tuple data)
        // 4. back_version_gpid/slot point to the back version on different page
        // 5. Indexes remain valid (TID unchanged)
        // ====================================================================

        if (page_data_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page data is null");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();
        auto *special = getSpecial();

        // Validate item_id
        if (item_id >= getItemCount())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Item ID out of range");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *item_ptr = &items[item_id];

        // Check if item is deleted or unused
        if (item_ptr->isDeleted() || item_ptr->isUnused())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple is deleted or unused");
            return Status::NOT_FOUND;
        }

        // Get current tuple location
        uint32_t old_offset = item_ptr->offset;
        uint32_t old_length = item_ptr->length;

        // Calculate size needed (TupleHeader + data)
        uint32_t final_new_tuple_size = sizeof(TupleHeader) + new_tuple_size;

        // Check if new tuple fits in old tuple's space
        if (final_new_tuple_size <= old_length)
        {
            // New tuple fits in old space - overwrite in-place
            // Copy tuple header + data
            memcpy(page_data_ + old_offset, new_tuple_data, final_new_tuple_size);

            // Update item pointer length
            item_ptr->length = final_new_tuple_size;
        }
        else
        {
            // New tuple is larger - need to allocate new space at end of free area
            uint32_t free_space = special->pd_upper - special->pd_lower;
            if (final_new_tuple_size > free_space)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Not enough space for larger tuple");
                return Status::PAGE_FULL;
            }

            // Allocate space from end of page
            uint32_t new_offset = special->pd_upper - final_new_tuple_size;

            // Align to 8-byte boundary
            new_offset = (new_offset / 8) * 8;

            // Validate offset
            if (new_offset + final_new_tuple_size > page_size_)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "New tuple offset out of bounds");
                return Status::PAGE_CORRUPT;
            }

            // Copy new tuple to new location
            memcpy(page_data_ + new_offset, new_tuple_data, final_new_tuple_size);

            // Update item pointer to new location
            item_ptr->offset = new_offset;
            item_ptr->length = final_new_tuple_size;

            // Update upper boundary
            special->pd_upper = new_offset;
        }

        // ====================================================================
        // UPDATE TUPLE HEADER WITH CROSS-PAGE BACK VERSION POINTERS
        // ====================================================================
        // This is the CRITICAL part: Set back version to point to different page
        // ====================================================================

        auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + item_ptr->offset);

        // Set transaction IDs
        tuple_hdr->xmin = new_xmin;  // New version created by this XID
        tuple_hdr->xmax = 0;          // Not deleted

        // Set back version pointers (CROSS-PAGE!)
        tuple_hdr->back_version_gpid = back_version_gpid;  // Different page!
        tuple_hdr->back_version_slot = back_version_slot;

        // Set current TID (UNCHANGED - stable!)
        uint32_t page_id = header()->page_id;
        GPID page_gpid = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));
        tuple_hdr->setTID(page_gpid, item_id);  // Same item_id!

        // Clear flags for new primary version
        tuple_hdr->infomask = 0;
        tuple_hdr->infomask |= TupleHeader::HEAP_CHAIN;  // Part of version chain

        // Update page statistics
        updateHeaderStats();

        return Status::OK;
    }

    auto HeapPage::findVisibleVersion(uint16_t item_id, uint64_t current_xid,
                                      const uint8_t **data_out, uint32_t *size_out,
                                      ErrorContext *ctx)
        -> Status
    {
        // ====================================================================
        // FIREBIRD MGA BACK VERSION TRAVERSAL (Newest-to-Oldest)
        // ====================================================================
        // This function traverses BACKWARD through version chains to find
        // the visible version for the current transaction.
        //
        // Key principles:
        // 1. Start at PRIMARY location (item_id) - newest version
        // 2. Follow back_version_tid pointers BACKWARD (N2O traversal)
        // 3. Back versions stored by OFFSET (not item_id)
        // 4. Only same-page back versions supported (Alpha limitation)
        // 5. Uses TIP-based visibility (Firebird MGA), NOT snapshots
        // ====================================================================

        // Validate inputs
        if (data_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "data_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Start with the PRIMARY tuple (newest version at stable item_id)
        uint16_t current_item_id = item_id;
        uint32_t current_page_id = header()->page_id;
        bool is_back_version = false;  // Track if we're at back version (offset-based)
        uint32_t current_offset = 0;  // For back versions accessed by offset

        // Current page pointers (Alpha: only same-page back versions supported)
        uint8_t *current_page_data = page_data_;
        uint32_t current_page_size = page_size_;

        // Follow version chain looking for visible version
        // Limit chain traversal to prevent infinite loops
        constexpr uint32_t MAX_CHAIN_LENGTH = config::DEFAULT_MAX_VERSION_CHAIN_LENGTH;
        uint32_t chain_length = 0;

        // CRITICAL FIX (Issue 1.19): Add visited set to detect cycles immediately
        // For back versions (offset-based), use offset as key
        // For primary versions (item_id-based), use item_id as key
        std::unordered_set<uint64_t> visited_locations;

        BufferPool *buffer_pool = (db_ != nullptr) ? db_->buffer_pool() : nullptr;

        while (chain_length < MAX_CHAIN_LENGTH)
        {
            // CRITICAL FIX (Issue 1.19): Check for cycles immediately
            // Build location key: for primary use item_id, for back version use offset
            uint64_t location_key;
            if (is_back_version)
            {
                location_key = (static_cast<uint64_t>(current_page_id) << 32) | current_offset;
            }
            else
            {
                location_key = (static_cast<uint64_t>(current_page_id) << 32) |
                              (static_cast<uint64_t>(current_item_id) << 16);
            }

            if (visited_locations.count(location_key) > 0)
            {
                // Cycle detected! We've visited this location before
                LOG_ERROR(STORAGE,
                          "Cycle detected in version chain at page %u - chain is corrupted",
                          current_page_id);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Cycle detected in version chain");
                return Status::PAGE_CORRUPT;
            }

            // Mark this location as visited
            // EXCEPTION SAFETY (ERROR-CRITICAL-2 Priority 1): Protect cycle detection
            // If insert fails due to OOM, we lose corruption protection! Must handle gracefully.
            try
            {
                visited_locations.insert(location_key);
            }
            catch (const std::bad_alloc &)
            {
                // CRITICAL: Allocation failure in cycle detection
                // We can't safely continue without cycle protection - abort the chain traversal
                LOG_ERROR(STORAGE,
                          "OOM during cycle detection in version chain - aborting traversal for safety");
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Out of memory during version chain cycle detection");
                return Status::OOM;
            }

            // Get tuple data based on whether we're at primary or back version
            uint32_t offset;
            uint32_t length;
            TupleHeader *tuple_hdr;

            if (is_back_version)
            {
                // Back version: access directly by offset (no item pointer)
                offset = current_offset;

                // Validate offset bounds
                if (offset < sizeof(PageHeader) ||
                    offset >= current_page_size - sizeof(HeapPageSpecial))
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Back version offset out of bounds");
                    return Status::PAGE_CORRUPT;
                }

                tuple_hdr = reinterpret_cast<TupleHeader *>(current_page_data + offset);

                // Back versions don't have item pointers, so we need to calculate length
                // For now, we'll need to trust the back_version_tid chain
                // In a full implementation, we'd store length in tuple header or use
                // the next tuple's offset
                // For Alpha, we'll use a simplified approach
                length = 0;  // Will be determined from tuple structure if needed
            }
            else
            {
                // Primary tuple: access via item pointer array
                auto *page_header = reinterpret_cast<PageHeader *>(current_page_data);
                auto *items = reinterpret_cast<ItemPointer *>(current_page_data + sizeof(PageHeader));

                // Validate item_id
                if (current_item_id >= page_header->item_count)
                {
                    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Version chain broken");
                    return Status::NOT_FOUND;
                }

                if (items[current_item_id].isDeleted())
                {
                    // Primary tuple is deleted - check if there's a back version
                    if (!items[current_item_id].isValid(current_page_size))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                          "Deleted item pointer out of bounds");
                        return Status::PAGE_CORRUPT;
                    }

                    offset = items[current_item_id].offset;
                    tuple_hdr = reinterpret_cast<TupleHeader *>(current_page_data + offset);

                    if (!tuple_hdr->hasBackVersion())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                          "Version deleted, no back version");
                        return Status::NOT_FOUND;
                    }
                    // Fall through to follow back version chain
                    length = items[current_item_id].length;
                }
                else
                {
                    // Validate item pointer bounds before accessing tuple
                    if (!items[current_item_id].isValid(current_page_size))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                          "Item pointer out of bounds in version chain");
                        return Status::PAGE_CORRUPT;
                    }

                    offset = items[current_item_id].offset;
                    length = items[current_item_id].length;
                    tuple_hdr = reinterpret_cast<TupleHeader *>(current_page_data + offset);
                }
            }

            // VALIDATE XIDs FIRST - protect against corrupted tuple headers
            bool xmin_valid = TransactionManager::isValidXid(tuple_hdr->xmin);
            bool xmax_valid =
                (tuple_hdr->xmax == 0) || TransactionManager::isValidXid(tuple_hdr->xmax);

            if (!xmin_valid)
            {
                // Invalid xmin - skip this version and try BACK version
                // This protects against corrupted data
                LOG_ERROR(STORAGE,
                          "Invalid xmin %lu in version chain at page %u - skipping to back version",
                          tuple_hdr->xmin, current_page_id);

                if (tuple_hdr->hasBackVersion())
                {
                    // Continue to BACK version (N2O traversal)
                    chain_length++;
                    // PHASE 1, TASK 1.2.5: Use GPID-based TID fields
                    TID back_tid = tuple_hdr->getBackVersionTID();
                    uint64_t back_page_num = getPageNumber(back_tid);
                    uint32_t back_offset = back_tid.slot; // Alpha: slot is actually offset

                    // For Alpha: Only same-page back versions supported
                    if (back_page_num != static_cast<uint64_t>(current_page_id))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                          "Cross-page back versions not yet supported (Alpha)");
                        return Status::NOT_IMPLEMENTED;
                    }

                    // Move to back version (offset-based)
                    current_offset = back_offset;
                    is_back_version = true;
                    continue;
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Invalid xmin and no back version");
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
                    // Check if deleted before our transaction
                    if (effective_xmax <= current_xid)
                    {
                        visible = false;
                        hint_bits_definitive = true;
                    }
                    else
                    {
                        // Deleted after our transaction started - still visible
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
                // Check visibility of this version using Firebird MGA TIP-based visibility
                // Simple visibility: xmin <= current_xid < xmax
                if (tuple_hdr->xmin <= current_xid)
                {
                    if (effective_xmax == 0 || effective_xmax > current_xid)
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
                    if (tuple_hdr->xmin <= current_xid)
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
                    if (effective_xmax != 0 && effective_xmax <= current_xid)
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
                    // The page is modified in-memory, but we don't explicitly mark it dirty.
                    // Hint bits will be persisted if:
                    // 1. The page is modified by another operation (update/delete) before eviction
                    // 2. The page is explicitly flushed
                    // Lost hint bits are acceptable - they're a performance optimization that
                    // can be recreated on next visibility check.
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
                    // For back versions (offset-based), length might be 0
                    // In that case, we need to determine it from tuple structure
                    // For Alpha: We'll return 0 and let caller handle it
                    // Full implementation would calculate from tuple header + data size
                    *size_out = length;
                }

                // Safe to return pointer - page is pinned by caller
                return Status::OK;
            }

            // ====================================================================
            // NOT VISIBLE: Follow BACK version chain (N2O traversal)
            // ====================================================================
            // This version is not visible to our transaction.
            // Follow back version pointer BACKWARD to older version.
            if (tuple_hdr->hasBackVersion())
            {
                // PHASE 1, TASK 1.2.5: Use GPID-based TID fields
                // Extract back version TID
                TID back_tid = tuple_hdr->getBackVersionTID();
                uint64_t back_page_num = getPageNumber(back_tid);

                // CRITICAL: Extract OFFSET (not item_id) from slot field
                // This is the key change from forward versioning
                // Alpha: slot is actually offset
                uint32_t back_offset = back_tid.slot;

                // Check if we need to follow version chain to another page
                if (back_page_num != static_cast<uint64_t>(current_page_id))
                {
                    // CROSS-PAGE BACK VERSION CHAIN - Not supported in Alpha
                    SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                      "Cross-page back versions not yet supported (Alpha limitation)");
                    return Status::NOT_IMPLEMENTED;
                }

                // Same-page back version - update offset and set flag
                current_offset = back_offset;
                is_back_version = true; // Switch to offset-based access

                chain_length++;
            }
            else
            {
                // End of chain, no visible version found
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No visible version in chain");
                return Status::NOT_FOUND;
            }
        }

        // Chain too long - possible cycle or corruption
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

    // PHASE 2 TASK 2.6: Collect dead tuple IDs for index cleanup
    // PHASE 1.5 TASK 1.5.3: Migrated to TID struct API
    auto HeapPage::collectDeadTuples(uint64_t oit, std::vector<TID> *dead_tids_out,
                                     ErrorContext *ctx) -> Status
    {
        if (dead_tids_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "dead_tids_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Get page header
        auto *pg_header = header();
        auto *special = reinterpret_cast<HeapPageSpecial *>(page_data_ + page_size_ -
                                                            sizeof(HeapPageSpecial));

        // Get item pointer array
        auto *items = reinterpret_cast<ItemPointer *>(page_data_ + sizeof(PageHeader));
        uint16_t item_count = special->pd_lower / sizeof(ItemPointer);

        dead_tids_out->clear();

        // Scan all items looking for dead tuples
        for (uint16_t i = 0; i < item_count; i++)
        {
            // Skip unused items
            if (items[i].isUnused())
            {
                continue;
            }

            // Skip deleted line pointers
            if (items[i].isDeleted())
            {
                continue;
            }

            // Get tuple header
            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[i].offset);

            // Tuple is dead if:
            // 1. xmax != 0 (deleted/updated)
            // 2. xmax < OIT (deleting transaction is old enough)
            // 3. xmax is committed
            if (tuple_hdr->xmax != 0 && tuple_hdr->xmax < oit)
            {
                // Check if XMAX_COMMITTED flag is set
                if ((tuple_hdr->infomask & TupleHeader::HEAP_XMAX_COMMITTED) != 0)
                {
                    // PHASE 1.5: Create TID struct (already using GPID internally)
                    TID tid = tuple_hdr->getTID();
                    dead_tids_out->push_back(tid);
                }
            }
        }

        return Status::OK;
    }

} // namespace scratchbird::core
