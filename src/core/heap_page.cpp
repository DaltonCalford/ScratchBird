#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include <cstring>
#include <algorithm>


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
                    // Correct the mismatch - the buffer size is authoritative
                    hdr->page_size = page_size_;
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
            // Validate input: tuple_size must include space for TupleHeader
            if (tuple_size < sizeof(TupleHeader))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Tuple size must be at least sizeof(TupleHeader)");
                return Status::INVALID_ARGUMENT;
            }

            // Check if we need to TOAST this tuple
            uint32_t actual_tuple_size = tuple_size;
            std::vector<uint8_t> toasted_data;
            const uint8_t *data_to_insert = tuple_data;

            if ((toast_mgr_ != nullptr) && (db_ != nullptr) && ToastManager::shouldToast(tuple_size, page_size_))
            {
                // Create a temporary buffer for the toasted tuple
                toasted_data.resize(sizeof(TupleHeader) + sizeof(ToastPointer));

                // Copy the tuple header
                auto *new_hdr = reinterpret_cast<TupleHeader *>(toasted_data.data());
                new_hdr->xmin = xmin;
                new_hdr->xmax = 0;
                new_hdr->flags = 0;
                new_hdr->null_bitmap_offset = 0;

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
            uint32_t tuple_offset = special->pd_upper - actual_tuple_size;

            // If not using TOAST, update the xmin in the existing tuple header
            if (data_to_insert == tuple_data)
            {
                // tuple_data already includes TupleHeader (validated at function entry)
                // Copy the entire tuple including the header
                memcpy(page_data_ + tuple_offset, tuple_data, tuple_size);

                // Update the xmin field in the tuple header
                auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + tuple_offset);
                tuple_hdr->xmin = xmin;
                if (tuple_hdr->xmax == 0)
                {
                    tuple_hdr->xmax = 0; // Ensure xmax is 0 for new tuples
                }
            }
            else
            {
                // Copy the already prepared toasted tuple (header already set correctly)
                memcpy(page_data_ + tuple_offset, data_to_insert, actual_tuple_size);
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
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Item pointer out of bounds or invalid");
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

        auto HeapPage::getTupleDetoasted(uint16_t item_id, std::vector<uint8_t> *buffer,
                                             uint64_t xmin, ErrorContext *ctx) -> Status
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

                    const auto *toast_ptr =
                        reinterpret_cast<const ToastPointer *>(data_ptr);

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
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Item pointer out of bounds or invalid");
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
                        const auto *toast_ptr =
                            reinterpret_cast<const ToastPointer *>(data_ptr);

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
            auto *tuple_hdr =
                reinterpret_cast<TupleHeader *>(page_data_ + items[item_id].offset);
            tuple_hdr->xmax = xmax;
            tuple_hdr->flags |= TupleHeader::FLAG_DELETED;

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

    } // namespace scratchbird::core
