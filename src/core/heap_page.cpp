/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/heap_page.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/toast.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/buffer_pool_guard.h"
#include "scratchbird/core/transaction_manager.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/storage_engine.h"
#include <cstring>
#include <algorithm>
#include <vector>
#include <unordered_set>

namespace scratchbird::core
{
    static auto isZeroIdLocal(const ID &id) -> bool
    {
        for (uint8_t b : id.bytes)
        {
            if (b != 0)
            {
                return false;
            }
        }
        return true;
    }

    static auto normalizeSessionId(Database *db, const ID &table_id, const ID &proposed) -> ID
    {
        if (db == nullptr || isZeroIdLocal(table_id))
        {
            return proposed;
        }

        auto *catalog = db->catalog_manager();
        if (catalog == nullptr)
        {
            return proposed;
        }

        CatalogManager::TableInfo table_info;
        ErrorContext ctx;
        if (catalog->getTable(table_id, table_info, &ctx) != Status::OK)
        {
            return proposed;
        }

        if (table_info.temp_data_scope == CatalogManager::TempDataScope::NONE)
        {
            return ID{};
        }

        return proposed;
    }

    static void applyCanonicalRecordContract(TupleHeader *tuple_hdr, const uint8_t *tuple_data,
                                             uint32_t tuple_size, const ID *preferred_row_uuid = nullptr)
    {
        if (tuple_hdr == nullptr)
        {
            return;
        }

        // Populate stable row UUID if not explicitly present in the source tuple.
        if (isZeroIdLocal(tuple_hdr->row_uuid))
        {
            if (preferred_row_uuid != nullptr && !isZeroIdLocal(*preferred_row_uuid))
            {
                tuple_hdr->row_uuid = *preferred_row_uuid;
            }
            else
            {
                tuple_hdr->row_uuid = generateUuidV7();
            }
        }

        if (tuple_hdr->record_format == 0)
        {
            tuple_hdr->record_format = TupleHeader::RECORD_FORMAT_V1;
        }

        tuple_hdr->payload_len = (tuple_size > sizeof(TupleHeader))
                                     ? (tuple_size - sizeof(TupleHeader))
                                     : 0u;

        // Keep canonical record flags aligned with MGA/heap infomask semantics.
        tuple_hdr->record_flags &= ~(TupleHeader::RHD_DELETED |
                                     TupleHeader::RHD_CHAINED |
                                     TupleHeader::RHD_MOVED |
                                     TupleHeader::RHD_TOAST_PTR);
        tuple_hdr->setRecordFlag(TupleHeader::RHD_CHAINED,
                                 (tuple_hdr->infomask & TupleHeader::HEAP_CHAIN) != 0u);
        tuple_hdr->setRecordFlag(TupleHeader::RHD_DELETED,
                                 (tuple_hdr->infomask & TupleHeader::HEAP_XMAX_COMMITTED) != 0u &&
                                     (tuple_hdr->infomask & TupleHeader::HEAP_UPDATED) == 0u);
        tuple_hdr->setRecordFlag(TupleHeader::RHD_MOVED,
                                 (tuple_hdr->infomask & TupleHeader::HEAP_MOVED) != 0u);

        bool has_toast_ptr = false;
        if (tuple_data != nullptr && tuple_size >= sizeof(TupleHeader) + sizeof(ToastPointer))
        {
            const uint8_t *payload = tuple_data + sizeof(TupleHeader);
            has_toast_ptr = isToastPointer(payload, sizeof(ToastPointer));
        }
        tuple_hdr->setRecordFlag(TupleHeader::RHD_TOAST_PTR, has_toast_ptr);
    }

    static auto resolveCommittedDeleteState(Database *db, TupleHeader *tuple_hdr,
                                            bool *xmax_committed_out, ErrorContext *ctx) -> Status
    {
        if (xmax_committed_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "xmax_committed_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        *xmax_committed_out = false;
        if (tuple_hdr == nullptr || tuple_hdr->xmax == 0)
        {
            return Status::OK;
        }

        if ((tuple_hdr->infomask & TupleHeader::HEAP_XMAX_COMMITTED) != 0u)
        {
            *xmax_committed_out = true;
            return Status::OK;
        }

        if ((tuple_hdr->infomask & TupleHeader::HEAP_XMAX_INVALID) != 0u)
        {
            return Status::OK;
        }

        if (db == nullptr || db->transaction_manager() == nullptr)
        {
            return Status::OK;
        }

        TransactionState state = TransactionState::ACTIVE;
        Status state_status = db->transaction_manager()->getTransactionState(tuple_hdr->xmax, state, ctx);
        if (state_status != Status::OK)
        {
            return state_status;
        }

        if (state == TransactionState::COMMITTED)
        {
            tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_COMMITTED;
            tuple_hdr->infomask &= ~TupleHeader::HEAP_XMAX_INVALID;
            tuple_hdr->setRecordFlag(TupleHeader::RHD_DELETED,
                                     (tuple_hdr->infomask & TupleHeader::HEAP_UPDATED) == 0u);
            *xmax_committed_out = true;
            return Status::OK;
        }

        if (state == TransactionState::ABORTED)
        {
            tuple_hdr->infomask |= TupleHeader::HEAP_XMAX_INVALID;
            tuple_hdr->infomask &= ~TupleHeader::HEAP_XMAX_COMMITTED;
            tuple_hdr->setRecordFlag(TupleHeader::RHD_DELETED, false);
        }

        return Status::OK;
    }

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
            hdr->generation = 1;
            hdr->checksum = 0;
            hdr->flags = 0;
            hdr->lsn = 0;
            setDatabaseUuid(*hdr, db_ ? db_->uuid() : ID{});
            setObjectUuid(*hdr, table_id_);
            hdr->item_count = 0;
            pageSetLower(*hdr, sizeof(PageHeader));
            pageSetUpper(*hdr, page_size_ - sizeof(HeapPageSpecial));
            pageSetSpecial(*hdr, page_size_ - sizeof(HeapPageSpecial));

            // Initialize special area only for new pages
            HeapPageSpecial *special = getSpecial();
            special->pd_flags = 0;
            special->table_id = table_id_;
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
            bool special_valid = (pageLower(*hdr) >= sizeof(PageHeader) &&
                                  pageUpper(*hdr) <= page_size_ - sizeof(HeapPageSpecial) &&
                                  pageLower(*hdr) <= pageUpper(*hdr) &&
                                  pageSpecial(*hdr) == page_size_ - sizeof(HeapPageSpecial));

            if (!special_valid)
            {
                // Special area is corrupt - reinitialize it
                special->pd_flags = 0;
                pageSetLower(*hdr, sizeof(PageHeader));
                pageSetUpper(*hdr, page_size_ - sizeof(HeapPageSpecial));
                pageSetSpecial(*hdr, page_size_ - sizeof(HeapPageSpecial));
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
            const auto *src_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
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
            new_hdr->session_id = src_hdr->session_id;

            // TOAST the data portion (after TupleHeader)
            ToastPointer toast_ptr;
            // Use EXTERNAL strategy for automatic compression when available
            Status s = toast_mgr_->toastValue(tuple_data + sizeof(TupleHeader),
                                              tuple_size - sizeof(TupleHeader),
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
        PageHeader *hdr = header();

        // Find a free slot (reuse deleted slots if possible)
        uint16_t item_id = getItemCount();
        for (uint16_t i = 0; i < getItemCount(); i++)
        {
            if (items[i].isDeleted() && items[i].length >= actual_tuple_size)
            {
                if (items[i].isValid(page_size_))
                {
                    auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[i].offset);
                    if (tuple_hdr->xmax == UINT64_MAX)
                    {
                        item_id = i;
                        break;
                    }
                }
            }
        }

        // Allocate space for tuple from upper area
        // Validate no underflow
        if (actual_tuple_size > pageUpper(*hdr))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Tuple size exceeds available space (underflow risk)");
            return Status::PAGE_CORRUPT;
        }
        uint32_t tuple_offset = pageUpper(*hdr) - actual_tuple_size;

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
        // Default to no null bitmap and no session scope for permanent tables.
        // This avoids random data in visibility/session checks when callers
        // provide raw tuple payloads without a prefilled header.
        if (!tuple_hdr->hasNulls())
        {
            tuple_hdr->null_bitmap_offset = 0;
        }
        tuple_hdr->padding = 0;
        // Preserve caller-provided session scope for temp tables.
        // StorageEngine stamps tuple header session_id before insert.
        tuple_hdr->session_id = normalizeSessionId(db_, table_id_, tuple_hdr->session_id);
        applyCanonicalRecordContract(tuple_hdr, page_data_ + tuple_offset, actual_tuple_size);

        // Update item pointer
        if (item_id == getItemCount())
        {
            // New slot - advance lower boundary
            pageSetLower(*hdr, pageLower(*hdr) + (sizeof(ItemPointer)));
        }

        items[item_id].offset = tuple_offset;
        items[item_id].length = actual_tuple_size;
        items[item_id].setDeleted(false);

        // Update upper boundary
        pageSetUpper(*hdr, tuple_offset);

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
        if (item_id >= getItemCount())
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
            if (isToastPointer(data_ptr, sizeof(ToastPointer)))
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

    auto HeapPage::extractSystemColumns(const uint8_t *tuple_data, uint32_t tuple_size,
                                        ID *row_uuid_out, uint64_t *last_edit_txid_out,
                                        ErrorContext *ctx) -> Status
    {
        if (tuple_data == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "tuple_data cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        if (tuple_size < sizeof(TupleHeader))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "tuple_size must include TupleHeader");
            return Status::INVALID_ARGUMENT;
        }

        const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
        if (row_uuid_out != nullptr)
        {
            *row_uuid_out = tuple_hdr->row_uuid;
        }
        if (last_edit_txid_out != nullptr)
        {
            *last_edit_txid_out = tuple_hdr->getLastEditTxidSystem();
        }

        return Status::OK;
    }

    auto HeapPage::deleteTuple(uint16_t item_id, uint64_t xmax, ErrorContext *ctx,
                               bool defer_toast_cleanup) -> Status
    {
        const bool force_delete = (xmax == UINT64_MAX);

        if (item_id >= getItemCount())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid item ID");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();

        if (items[item_id].isDeleted())
        {
            if (!force_delete)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple already deleted");
                return Status::NOT_FOUND;
            }
            return Status::OK;
        }

        // Validate item pointer bounds
        if (!items[item_id].isValid(page_size_))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Item pointer out of bounds or invalid");
            return Status::PAGE_CORRUPT;
        }

        // Check if we need to delete TOAST data
        if (!defer_toast_cleanup && (toast_mgr_ != nullptr) && (db_ != nullptr))
        {
            // Get the tuple to check for TOAST pointers
            uint32_t offset = items[item_id].offset;
            uint32_t length = items[item_id].length;

            if (length >= sizeof(TupleHeader) + sizeof(ToastPointer))
            {
                const uint8_t *data_ptr = page_data_ + offset + sizeof(TupleHeader);

                // Check if this is a TOAST pointer
                if (isToastPointer(data_ptr, sizeof(ToastPointer)))
                {
                    const auto *toast_ptr = reinterpret_cast<const ToastPointer *>(data_ptr);

                    // Delete the TOAST data
                    Status s = toast_mgr_->deleteToastValue(toast_ptr->lob_uuid, xmax, ctx);
                    if (s != Status::OK && s != Status::NOT_FOUND)
                    {
                        return s;
                    }
                }
            }
        }

        // Update tuple header with xmax
        auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[item_id].offset);
        if (!force_delete)
        {
            if (tuple_hdr->xmax != 0 || (tuple_hdr->infomask & TupleHeader::FLAG_DELETED))
            {
                bool replace_aborted_delete = false;
                if (tuple_hdr->xmax != 0 && db_ != nullptr && db_->transaction_manager() != nullptr)
                {
                    TransactionState prior_delete_state = TransactionState::ACTIVE;
                    Status prior_state_status = db_->transaction_manager()->getTransactionState(
                        tuple_hdr->xmax, prior_delete_state, nullptr);
                    replace_aborted_delete =
                        prior_state_status == Status::OK &&
                        prior_delete_state == TransactionState::ABORTED;
                }

                if (!replace_aborted_delete)
                {
                    SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple already deleted");
                    return Status::NOT_FOUND;
                }

                tuple_hdr->xmax = 0;
                tuple_hdr->infomask = static_cast<uint16_t>(
                    tuple_hdr->infomask &
                    ~(TupleHeader::HEAP_XMAX_COMMITTED |
                      TupleHeader::HEAP_XMAX_INVALID |
                      TupleHeader::FLAG_DELETED));
            }

            tuple_hdr->xmax = xmax;
            tuple_hdr->infomask |= TupleHeader::FLAG_DELETED;
            applyCanonicalRecordContract(tuple_hdr, page_data_ + items[item_id].offset,
                                         items[item_id].length);
        }

        // MGA soft deletes must keep the line pointer live so visibility can consult xmax/TIP state.
        // Only force-delete paths may tombstone the item pointer immediately.
        if (force_delete)
        {
            items[item_id].setDeleted(true);
        }

        updateHeaderStats();

        return Status::OK;
    }

    auto HeapPage::hasFreeSpace(uint32_t tuple_size) const -> bool
    {
        const PageHeader *hdr = header();

        // Sanity check - if pd_upper < pd_lower, page is corrupt
        if (pageUpper(*hdr) < pageLower(*hdr))
        {
            return false;
        }

        uint32_t free_space = pageUpper(*hdr) - pageLower(*hdr);

        // Need space for tuple and potentially a new item pointer.
        uint32_t lower_bound = pageLower(*hdr);

        // Check if we need a new item slot
        bool found_deleted_slot = false;
        const ItemPointer *items = getItemArray();
        for (uint16_t i = 0; i < getItemCount(); i++)
        {
            if (items[i].isDeleted() && items[i].length >= tuple_size)
            {
                found_deleted_slot = true;
                break;
            }
        }

        if (!found_deleted_slot)
        {
            lower_bound += sizeof(ItemPointer);
        }

        if (tuple_size > pageUpper(*hdr))
        {
            return false;
        }

        uint32_t raw_tuple_offset = pageUpper(*hdr) - tuple_size;
        uint32_t aligned_tuple_offset = (raw_tuple_offset / 8) * 8;

        return aligned_tuple_offset >= lower_bound && free_space >= (lower_bound - pageLower(*hdr)) + tuple_size;
    }

    auto HeapPage::getItemCount() const -> uint16_t
    {
        const PageHeader *hdr = header();
        if (pageLower(*hdr) < sizeof(PageHeader))
        {
            return 0;
        }
        return static_cast<uint16_t>(
            (pageLower(*hdr) - sizeof(PageHeader)) / sizeof(ItemPointer));
    }

    auto HeapPage::getFreeSpace() const -> uint32_t
    {
        const PageHeader *hdr = header();
        return pageUpper(*hdr) - pageLower(*hdr);
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

        if (pageLower(*hdr) < sizeof(PageHeader) || pageLower(*hdr) > pageUpper(*hdr) ||
            pageUpper(*hdr) > pageSpecial(*hdr) ||
            pageSpecial(*hdr) != page_size_ - sizeof(HeapPageSpecial))
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid page boundaries");
            return Status::PAGE_CORRUPT;
        }

        uint32_t item_bytes = pageLower(*hdr) - sizeof(PageHeader);
        uint32_t max_items = item_bytes / sizeof(ItemPointer);
        if (getItemCount() > max_items)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Item count exceeds page bounds");
            return Status::PAGE_CORRUPT;
        }

        // Validate item pointers
        const ItemPointer *items = getItemArray();
        for (uint16_t i = 0; i < getItemCount(); i++)
        {
            if (items[i].isUnused())
            {
                continue;
            }

            if (!items[i].isDeleted())
            {
                if (items[i].offset < pageUpper(*hdr) ||
                    items[i].offset + items[i].length > pageSpecial(*hdr))
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

        // pd_lower/pd_upper already tracked in header
    }

    // MGA Phase 3: Version Chains - FIREBIRD MGA BACK VERSIONING

    auto HeapPage::finalizeBackVersionMetadata(uint16_t item_id,
                                               const TupleHeader &source_old_header,
                                               uint64_t update_xid,
                                               GPID primary_gpid,
                                               uint16_t primary_item_id,
                                               bool cross_page_back_version,
                                               ErrorContext *ctx) -> Status
    {
        const uint8_t *back_version_data = nullptr;
        uint32_t back_version_size = 0;
        Status read_status = getTuple(item_id, &back_version_data, &back_version_size, ctx);
        if (read_status != Status::OK)
        {
            return read_status;
        }

        auto *back_version_hdr =
            reinterpret_cast<TupleHeader *>(const_cast<uint8_t *>(back_version_data));
        back_version_hdr->xmin = source_old_header.xmin;
        back_version_hdr->xmax = update_xid;
        back_version_hdr->back_version_gpid = source_old_header.back_version_gpid;
        back_version_hdr->back_version_slot = source_old_header.back_version_slot;
        back_version_hdr->session_id = source_old_header.session_id;
        back_version_hdr->setTID(primary_gpid, primary_item_id);

        uint16_t preserved_infomask =
            back_version_hdr->infomask &
            (TupleHeader::HEAP_HAS_NULLS |
             TupleHeader::HEAP_XMIN_COMMITTED |
             TupleHeader::HEAP_XMIN_INVALID |
             TupleHeader::HEAP_XMIN_FROZEN);
        back_version_hdr->infomask =
            preserved_infomask | TupleHeader::HEAP_CHAIN | TupleHeader::HEAP_UPDATED;
        if (cross_page_back_version)
        {
            back_version_hdr->infomask |= TupleHeader::HEAP_MOVED;
        }

        ID stable_row_uuid = source_old_header.row_uuid;
        applyCanonicalRecordContract(back_version_hdr, back_version_data,
                                     back_version_size, &stable_row_uuid);
        return Status::OK;
    }

    auto HeapPage::installUpdatedPrimaryVersion(uint16_t item_id,
                                                const uint8_t *new_tuple_data,
                                                uint32_t new_tuple_size,
                                                uint64_t new_xmin,
                                                GPID back_version_gpid,
                                                uint16_t back_version_slot,
                                                const ID &session_id,
                                                const ID &stable_row_uuid,
                                                bool *tuple_location_moved_out,
                                                ErrorContext *ctx) -> Status
    {
        if (page_data_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page data is null");
            return Status::INVALID_ARGUMENT;
        }

        if (item_id >= getItemCount())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Item ID out of range");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();
        ItemPointer *item_ptr = &items[item_id];
        if (item_ptr->isDeleted() || item_ptr->isUnused())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple is deleted or unused");
            return Status::NOT_FOUND;
        }

        auto *hdr = header();
        uint32_t old_length = item_ptr->length;
        bool tuple_location_moved = false;

        if (new_tuple_size <= old_length)
        {
            std::memcpy(page_data_ + item_ptr->offset, new_tuple_data, new_tuple_size);
            item_ptr->length = new_tuple_size;
        }
        else
        {
            uint32_t free_space = pageUpper(*hdr) - pageLower(*hdr);
            if (new_tuple_size > free_space)
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Not enough space for updated tuple");
                return Status::PAGE_FULL;
            }

            uint32_t new_offset = (pageUpper(*hdr) - new_tuple_size) & ~uint32_t{7};
            if (new_offset + new_tuple_size > page_size_ || new_offset < pageLower(*hdr))
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL,
                                  "Not enough aligned space for updated tuple");
                return Status::PAGE_FULL;
            }

            std::memcpy(page_data_ + new_offset, new_tuple_data, new_tuple_size);
            item_ptr->offset = new_offset;
            item_ptr->length = new_tuple_size;
            tuple_location_moved = true;
            pageSetUpper(*hdr, new_offset);
        }

        auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + item_ptr->offset);
        tuple_hdr->xmin = new_xmin;
        tuple_hdr->xmax = 0;
        tuple_hdr->session_id = session_id;
        tuple_hdr->back_version_gpid = back_version_gpid;
        tuple_hdr->back_version_slot = back_version_slot;
        tuple_hdr->setTID(makeGPID(PRIMARY_TABLESPACE_ID,
                                   static_cast<uint64_t>(header()->page_id)),
                          item_id);

        uint16_t preserved_infomask = tuple_hdr->infomask & TupleHeader::HEAP_HAS_NULLS;
        tuple_hdr->infomask = preserved_infomask;
        if (tuple_location_moved)
        {
            tuple_hdr->infomask |= TupleHeader::HEAP_MOVED;
        }

        ID preferred_row_uuid = stable_row_uuid;
        applyCanonicalRecordContract(tuple_hdr, page_data_ + item_ptr->offset,
                                     item_ptr->length, &preferred_row_uuid);
        updateHeaderStats();

        if (tuple_location_moved_out != nullptr)
        {
            *tuple_location_moved_out = tuple_location_moved;
        }

        return Status::OK;
    }

    auto HeapPage::updateTuple(uint16_t old_item_id, const uint8_t *new_tuple_data,
                               uint32_t new_tuple_size, uint64_t xmax, uint64_t new_xmin,
                               uint16_t *new_item_id_out, ErrorContext *ctx,
                               bool defer_old_toast_cleanup) -> Status
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
        if (old_item_id >= getItemCount())
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

        auto *hdr = header();
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
        ID stable_row_uuid = primary_tuple_hdr->row_uuid;
        if (isZeroIdLocal(stable_row_uuid))
        {
            stable_row_uuid = generateUuidV7();
        }

        // ====================================================================
        // TOAST CLEANUP: Delete old TOAST data if present
        // ====================================================================
        // This prevents TOAST storage leaks on UPDATE operations
        if (!defer_old_toast_cleanup && (toast_mgr_ != nullptr) && (db_ != nullptr))
        {
            if (primary_length >= sizeof(TupleHeader) + sizeof(ToastPointer))
            {
                const uint8_t *old_data_ptr = page_data_ + primary_offset + sizeof(TupleHeader);

                if (isToastPointer(old_data_ptr, sizeof(ToastPointer)))
                {
                    const auto *old_toast_ptr =
                        reinterpret_cast<const ToastPointer *>(old_data_ptr);

                    Status toast_status =
                        toast_mgr_->deleteToastValue(old_toast_ptr->lob_uuid, xmax, ctx);

                    if (toast_status != Status::OK && toast_status != Status::NOT_FOUND)
                    {
                        return toast_status;
                    }
                }
            }
        }

        // ====================================================================
        // PHASE 2: CREATE BACK VERSION USING CANONICAL SLOT ENCODING
        // ====================================================================
        // Same-page version chains must use the same page+slot encoding as
        // cross-page chains. Raw offsets are not allowed in the tuple header.
        // ====================================================================

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

        auto canAllocateSamePageBackVersion = [&]() -> bool
        {
            uint32_t simulated_lower = pageLower(*hdr);
            bool reuse_deleted_slot = false;
            for (uint16_t slot = 0; slot < getItemCount(); ++slot)
            {
                if (slot == old_item_id)
                {
                    continue;
                }
                if (items[slot].isDeleted() && items[slot].length >= primary_length)
                {
                    reuse_deleted_slot = true;
                    break;
                }
            }
            if (!reuse_deleted_slot)
            {
                simulated_lower += sizeof(ItemPointer);
            }

            if (primary_length > pageUpper(*hdr))
            {
                return false;
            }

            uint32_t simulated_upper = (pageUpper(*hdr) - primary_length) & ~uint32_t{7};
            if (simulated_upper < simulated_lower || simulated_upper + primary_length > page_size_)
            {
                return false;
            }

            if (final_new_tuple_size <= primary_length)
            {
                return true;
            }

            if (final_new_tuple_size > simulated_upper)
            {
                return false;
            }

            uint32_t new_primary_offset =
                (simulated_upper - final_new_tuple_size) & ~uint32_t{7};
            return new_primary_offset >= simulated_lower &&
                   new_primary_offset + final_new_tuple_size <= page_size_;
        };

        bool back_version_same_page = canAllocateSamePageBackVersion();
        uint16_t back_version_item_id = 0;

        if (back_version_same_page)
        {
            std::vector<uint8_t> old_tuple_buffer;
            try
            {
                old_tuple_buffer.resize(primary_length);
            }
            catch (const std::bad_alloc &)
            {
                SET_ERROR_CONTEXT(ctx, Status::OOM,
                                  "Out of memory allocating back-version buffer");
                return Status::OOM;
            }

            std::memcpy(old_tuple_buffer.data(), page_data_ + primary_offset, primary_length);
            const auto *source_old_hdr =
                reinterpret_cast<const TupleHeader *>(old_tuple_buffer.data());

            Status insert_status =
                insertTuple(old_tuple_buffer.data(), primary_length, source_old_hdr->xmin,
                            &back_version_item_id, ctx);
            if (insert_status != Status::OK)
            {
                return insert_status;
            }

            Status finalize_status =
                finalizeBackVersionMetadata(back_version_item_id,
                                            *source_old_hdr,
                                            xmax,
                                            makeGPID(PRIMARY_TABLESPACE_ID,
                                                     static_cast<uint64_t>(header()->page_id)),
                                            old_item_id,
                                            false,
                                            ctx);
            if (finalize_status != Status::OK)
            {
                return finalize_status;
            }
        }
        else
        {
            // HeapPage supports same-page back versions only.
            // Cross-page handling is implemented by StorageEngine::updateTuple().
            SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL,
                              "No space for same-page back version");
            return Status::PAGE_FULL;
        }

        Status install_status =
            installUpdatedPrimaryVersion(old_item_id,
                                         final_new_tuple_data,
                                         final_new_tuple_size,
                                         new_xmin,
                                         makeGPID(PRIMARY_TABLESPACE_ID,
                                                  static_cast<uint64_t>(header()->page_id)),
                                         back_version_item_id,
                                         normalizeSessionId(db_, table_id_,
                                                            primary_tuple_hdr->session_id),
                                         stable_row_uuid,
                                         nullptr,
                                         ctx);
        if (install_status != Status::OK)
        {
            return install_status;
        }

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

        (void)xmax;

        if (page_data_ == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Page data is null");
            return Status::INVALID_ARGUMENT;
        }

        if (item_id >= getItemCount())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Item ID out of range");
            return Status::INVALID_ARGUMENT;
        }

        ItemPointer *items = getItemArray();
        ItemPointer *item_ptr = &items[item_id];
        if (item_ptr->isDeleted() || item_ptr->isUnused())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Tuple is deleted or unused");
            return Status::NOT_FOUND;
        }

        const auto *existing_hdr =
            reinterpret_cast<const TupleHeader *>(page_data_ + item_ptr->offset);
        return installUpdatedPrimaryVersion(item_id,
                                            new_tuple_data,
                                            new_tuple_size,
                                            new_xmin,
                                            back_version_gpid,
                                            back_version_slot,
                                            normalizeSessionId(db_, table_id_,
                                                               existing_hdr->session_id),
                                            existing_hdr->row_uuid,
                                            nullptr,
                                            ctx);
    }

    auto HeapPage::findVisibleVersion(uint16_t item_id, uint64_t current_xid,
                                      const uint8_t **data_out, uint32_t *size_out,
                                      TID *visible_tid_out,
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
        // 3. Back versions use the same canonical page+slot encoding for
        //    same-page and cross-page traversal
        // 4. Uses TIP-based visibility (Firebird MGA), NOT snapshots
        // ====================================================================

        // Validate inputs
        if (data_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "data_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        // Start with the PRIMARY tuple (newest version at stable item_id)
        const uint32_t primary_page_id = header()->page_id;
        const GPID primary_gpid =
            makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(primary_page_id));
        uint16_t current_item_id = item_id;
        GPID current_gpid = primary_gpid;

        uint8_t *current_page_data = page_data_;
        uint32_t current_page_size = page_size_;

        // Follow version chain looking for visible version
        // Limit chain traversal to prevent infinite loops
        constexpr uint32_t MAX_CHAIN_LENGTH = config::DEFAULT_MAX_VERSION_CHAIN_LENGTH;
        uint32_t chain_length = 0;

        struct VersionLocation
        {
            GPID gpid;
            uint16_t slot;

            [[nodiscard]] auto operator==(const VersionLocation &other) const -> bool
            {
                return gpid == other.gpid && slot == other.slot;
            }
        };

        struct VersionLocationHash
        {
            [[nodiscard]] auto operator()(const VersionLocation &location) const noexcept
                -> std::size_t
            {
                return std::hash<uint64_t>{}(location.gpid) ^
                       (static_cast<std::size_t>(location.slot) << 1u);
            }
        };

        std::unordered_set<VersionLocation, VersionLocationHash> visited_locations;

        BufferPool *buffer_pool = (db_ != nullptr) ? db_->buffer_pool() : nullptr;
        GPID pinned_gpid = INVALID_GPID;
        uint8_t *pinned_page_data = nullptr;

        struct PinnedPageGuard
        {
            BufferPool *pool;
            GPID *gpid;
            ErrorContext *ctx;
            ~PinnedPageGuard()
            {
                if (pool && gpid && *gpid != INVALID_GPID)
                {
                    pool->unpinPageGlobal(*gpid, false, ctx);
                }
            }
        };

        PinnedPageGuard pinned_guard{buffer_pool, &pinned_gpid, ctx};

        auto unpinPinnedPage = [&]()
        {
            if (pinned_gpid != INVALID_GPID && buffer_pool != nullptr)
            {
                buffer_pool->unpinPageGlobal(pinned_gpid, false, ctx);
                pinned_gpid = INVALID_GPID;
                pinned_page_data = nullptr;
            }
        };

        auto switchToPage = [&](GPID target_gpid) -> Status
        {
            if (target_gpid == current_gpid)
            {
                return Status::OK;
            }

            if (target_gpid == primary_gpid)
            {
                unpinPinnedPage();
                current_page_data = page_data_;
                current_gpid = primary_gpid;
                current_page_size = page_size_;
                return Status::OK;
            }

            if (!buffer_pool)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Buffer pool not available for cross-page traversal");
                return Status::INVALID_ARGUMENT;
            }

            unpinPinnedPage();

            Status pin_status = buffer_pool->pinPageGlobal(
                target_gpid, reinterpret_cast<void **>(&pinned_page_data), ctx);
            if (pin_status != Status::OK)
            {
                return pin_status;
            }

            pinned_gpid = target_gpid;
            current_page_data = pinned_page_data;
            current_gpid = target_gpid;
            current_page_size = page_size_;
            return Status::OK;
        };

        while (chain_length < MAX_CHAIN_LENGTH)
        {
            VersionLocation location_key{current_gpid, current_item_id};
            if (visited_locations.count(location_key) > 0)
            {
                // Cycle detected! We've visited this location before
                LOG_ERROR(STORAGE,
                          "Cycle detected in version chain at page %u slot %u - chain is corrupted",
                          static_cast<uint32_t>(getPageNumber(current_gpid)),
                          current_item_id);
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

            auto *page_header = reinterpret_cast<PageHeader *>(current_page_data);
            auto *items = reinterpret_cast<ItemPointer *>(current_page_data + sizeof(PageHeader));
            uint16_t item_count = static_cast<uint16_t>(
                (pageLower(*page_header) - sizeof(PageHeader)) / sizeof(ItemPointer));
            if (current_item_id >= item_count)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Version chain broken");
                return Status::NOT_FOUND;
            }

            if (items[current_item_id].isUnused() ||
                !items[current_item_id].isValid(current_page_size))
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Item pointer out of bounds in version chain");
                return Status::PAGE_CORRUPT;
            }

            uint32_t offset = items[current_item_id].offset;
            uint32_t length = items[current_item_id].length;
            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(current_page_data + offset);

            if (items[current_item_id].isDeleted() && !tuple_hdr->hasBackVersion())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                                  "Version deleted, no back version");
                return Status::NOT_FOUND;
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
                          tuple_hdr->xmin,
                          static_cast<uint32_t>(getPageNumber(current_gpid)));

                if (tuple_hdr->hasBackVersion())
                {
                    chain_length++;
                    TID back_tid = tuple_hdr->getBackVersionTID();
                    if (back_tid.gpid == INVALID_GPID)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                          "Invalid back version target");
                        return Status::PAGE_CORRUPT;
                    }

                    Status switch_status = switchToPage(back_tid.gpid);
                    if (switch_status != Status::OK)
                    {
                        return switch_status;
                    }

                    current_item_id = back_tid.slot;
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

            bool create_visible = false;
            bool delete_visible = false;

            if (db_ != nullptr && db_->transaction_manager() != nullptr)
            {
                TransactionManager *txn_mgr = db_->transaction_manager();
                ConnectionContext *conn_ctx = ConnectionContext::getCurrent();
                VisibilityMode visibility_mode = VisibilityMode::READ_CURRENT_TRANSACTION;
                const TransactionSnapshot *snapshot = nullptr;
                uint64_t reader_visibility_xid = current_xid;

                if (conn_ctx != nullptr)
                {
                    if (const auto *replay_snapshot = conn_ctx->getForensicReplaySnapshot();
                        replay_snapshot != nullptr)
                    {
                        visibility_mode = VisibilityMode::SNAPSHOT;
                        snapshot = replay_snapshot;
                    }
                    else
                    {
                        switch (conn_ctx->getIsolationLevel())
                        {
                            case IsolationLevel::SNAPSHOT:
                            case IsolationLevel::SNAPSHOT_TABLE_STABILITY:
                                if (const auto *retained_snapshot =
                                        conn_ctx->getRetainedTransactionSnapshot();
                                    retained_snapshot != nullptr)
                                {
                                    visibility_mode = VisibilityMode::SNAPSHOT;
                                    snapshot = retained_snapshot;
                                }
                                else
                                {
                                    visibility_mode = VisibilityMode::READ_CURRENT_VERSION;
                                }
                                break;

                            case IsolationLevel::READ_COMMITTED_READ_CONSISTENCY:
                            {
                                const auto visibility_context =
                                    conn_ctx->resolveReadConsistencyVisibilityContext();
                                if (!visibility_context.valid)
                                {
                                    LOG_ERROR(STORAGE,
                                              "Missing statement snapshot for heap visibility: "
                                              "proc_id=%d xid=%lu",
                                              ConnectionContext::getCurrentProcId(),
                                              conn_ctx->getCurrentXid());
                                    create_visible = false;
                                    delete_visible = true;
                                    continue;
                                }
                                visibility_mode = visibility_context.mode;
                                snapshot = visibility_context.snapshot;
                                reader_visibility_xid = visibility_context.reader_xid;
                                break;
                            }

                            case IsolationLevel::READ_COMMITTED:
                            default:
                                visibility_mode = VisibilityMode::READ_CURRENT_TRANSACTION;
                                break;
                        }
                    }
                }

                const RecordVisibilityDecision visibility_decision =
                    txn_mgr->evaluateRecordVisibility(tuple_hdr->xmin,
                                                      effective_xmax,
                                                      reader_visibility_xid,
                                                      visibility_mode,
                                                      snapshot);
                create_visible = visibility_decision.create_visible;
                delete_visible = visibility_decision.delete_visible;
            }
            else
            {
                create_visible = tuple_hdr->xmin <= current_xid;
                delete_visible = effective_xmax != 0 && effective_xmax <= current_xid;
            }

            const bool visible = create_visible && !delete_visible;

            if (visible)
            {
                if (current_gpid == primary_gpid)
                {
                    if (data_out != nullptr)
                    {
                        *data_out = current_page_data + offset;
                    }
                    if (size_out != nullptr)
                    {
                        *size_out = length;
                    }
                    if (visible_tid_out != nullptr)
                    {
                        *visible_tid_out = TID(current_gpid, current_item_id);
                    }

                    // Safe to return pointer - page is pinned by caller
                    return Status::OK;
                }

                if (length > page_size_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                      "Visible tuple size invalid on cross-page chain");
                    return Status::PAGE_CORRUPT;
                }

                cross_page_buffer_.resize(length);
                std::memcpy(cross_page_buffer_.data(), current_page_data + offset, length);

                if (data_out != nullptr)
                {
                    *data_out = cross_page_buffer_.data();
                }
                if (size_out != nullptr)
                {
                    *size_out = length;
                }
                if (visible_tid_out != nullptr)
                {
                    *visible_tid_out = TID(current_gpid, current_item_id);
                }

                return Status::OK;
            }

            // ====================================================================
            // NOT VISIBLE: Follow BACK version chain (N2O traversal)
            // ====================================================================
            // This version is not visible to our transaction.
            // Follow back version pointer BACKWARD to older version.
            if (!create_visible)
            {
                if (tuple_hdr->hasBackVersion())
                {
                    TID back_tid = tuple_hdr->getBackVersionTID();
                    if (back_tid.gpid == INVALID_GPID)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                          "Invalid back version target");
                        return Status::PAGE_CORRUPT;
                    }

                    Status switch_status = switchToPage(back_tid.gpid);
                    if (switch_status != Status::OK)
                    {
                        return switch_status;
                    }
                    current_item_id = back_tid.slot;

                    chain_length++;
                    continue;
                }
            }

            // End of chain, or this logical row is deleted for the reader snapshot.
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No visible version in chain");
            return Status::NOT_FOUND;
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
        for (uint16_t i = 0; i < getItemCount(); ++i)
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

    auto HeapPage::repairVersionChainMetadata(uint32_t *repairs_out,
                                              bool *cleanup_blocked_out,
                                              ErrorContext *ctx) -> Status
    {
        if (repairs_out != nullptr)
        {
            *repairs_out = 0;
        }
        if (cleanup_blocked_out != nullptr)
        {
            *cleanup_blocked_out = false;
        }

        ItemPointer *items = getItemArray();
        const uint16_t item_count = getItemCount();
        const GPID current_gpid =
            makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(header()->page_id));

        uint32_t repairs = 0;

        auto blockCleanup = [&](const char *message) -> Status
        {
            if (cleanup_blocked_out != nullptr)
            {
                *cleanup_blocked_out = true;
            }
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, message);
            return Status::DATA_CORRUPTED;
        };

        for (uint16_t item_id = 0; item_id < item_count; ++item_id)
        {
            if (items[item_id].isUnused() || items[item_id].isDeleted())
            {
                continue;
            }
            if (!items[item_id].isValid(page_size_))
            {
                continue;
            }

            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[item_id].offset);
            const bool is_chain_member =
                (tuple_hdr->infomask & TupleHeader::HEAP_CHAIN) != 0u;
            if (!is_chain_member &&
                (tuple_hdr->ctid_gpid != current_gpid || tuple_hdr->ctid_slot != item_id))
            {
                tuple_hdr->setTID(current_gpid, item_id);
                ++repairs;
            }

            if (!tuple_hdr->hasBackVersion())
            {
                continue;
            }

            const TID back_tid = tuple_hdr->getBackVersionTID();
            if (back_tid.gpid == INVALID_GPID)
            {
                return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: tuple advertises an invalid back-version target");
            }

            if (back_tid.gpid == current_gpid)
            {
                if (back_tid.slot == item_id)
                {
                    return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: self-referential version chain");
                }
                if (back_tid.slot >= item_count)
                {
                    return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: back-version slot is out of bounds");
                }
                if (items[back_tid.slot].isUnused() || items[back_tid.slot].isDeleted() ||
                    !items[back_tid.slot].isValid(page_size_))
                {
                    return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: back-version target slot is not reclaim-safe");
                }

                auto *back_hdr =
                    reinterpret_cast<TupleHeader *>(page_data_ + items[back_tid.slot].offset);
                if (back_hdr->ctid_gpid != current_gpid || back_hdr->ctid_slot != item_id)
                {
                    back_hdr->setTID(current_gpid, item_id);
                    ++repairs;
                }
                continue;
            }

            if (db_ == nullptr || db_->buffer_pool() == nullptr)
            {
                return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: cross-page chain target cannot be validated");
            }

            void *target_buffer = nullptr;
            Status pin_status = db_->buffer_pool()->pinPageGlobal(
                back_tid.gpid, &target_buffer, ctx, BufferPool::AccessStrategy::Vacuum);
            if (pin_status != Status::OK)
            {
                return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: cross-page chain target cannot be pinned");
            }

            bool target_dirty = false;
            auto unpinTarget = [&]()
            {
                db_->buffer_pool()->unpinPageGlobal(back_tid.gpid, target_dirty, ctx);
            };

            auto *target_page_data = static_cast<uint8_t *>(target_buffer);
            auto *target_page_hdr = reinterpret_cast<PageHeader *>(target_page_data);
            if (target_page_hdr->page_type != PAGE_TYPE_HEAP ||
                target_page_hdr->page_size != db_->page_size())
            {
                unpinTarget();
                return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: cross-page chain target is not a valid heap page");
            }

            auto *target_items =
                reinterpret_cast<ItemPointer *>(target_page_data + sizeof(PageHeader));
            const uint16_t target_item_count = static_cast<uint16_t>(
                (pageLower(*target_page_hdr) - sizeof(PageHeader)) / sizeof(ItemPointer));
            if (back_tid.slot >= target_item_count)
            {
                unpinTarget();
                return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: cross-page back-version slot is out of bounds");
            }

            if (target_items[back_tid.slot].isUnused() || target_items[back_tid.slot].isDeleted() ||
                !target_items[back_tid.slot].isValid(db_->page_size()))
            {
                unpinTarget();
                return blockCleanup("GC_CHAIN_REPAIR_REQUIRED: cross-page back-version target is not reclaim-safe");
            }

            auto *target_tuple_hdr = reinterpret_cast<TupleHeader *>(
                target_page_data + target_items[back_tid.slot].offset);
            if (target_tuple_hdr->ctid_gpid != current_gpid ||
                target_tuple_hdr->ctid_slot != item_id)
            {
                target_tuple_hdr->setTID(current_gpid, item_id);
                target_dirty = true;
                ++repairs;
            }

            unpinTarget();
        }

        if (repairs_out != nullptr)
        {
            *repairs_out = repairs;
        }
        return Status::OK;
    }

    auto HeapPage::markTupleUnused(uint16_t item_id, ErrorContext *ctx) -> Status
    {
        if (item_id >= getItemCount())
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
        auto *hdr = header();
        HeapPageSpecial *special = getSpecial();
        ItemPointer *items = getItemArray();
        uint16_t item_count = getItemCount();

        // Calculate space before defragmentation
        uint32_t free_space_before = pageUpper(*hdr) - pageLower(*hdr);

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
        pageSetUpper(*hdr, new_upper);

        // CRITICAL FIX (Issue 2.10): Update pd_lower to reflect actual item array size
        // pd_lower marks the end of the item pointer array
        // It should be: PageHeader + (number_of_items * sizeof(ItemPointer))
        // This ensures correct free space calculation after defragmentation
        pageSetLower(*hdr, sizeof(PageHeader) + (item_count * sizeof(ItemPointer)));

        // Calculate space reclaimed
        uint32_t free_space_after = pageUpper(*hdr) - pageLower(*hdr);
        uint32_t bytes_reclaimed = free_space_after - free_space_before;

        if (bytes_reclaimed_out != nullptr)
        {
            *bytes_reclaimed_out = bytes_reclaimed;
        }

        updateHeaderStats();
        return Status::OK;
    }

    auto HeapPage::analyzeFragmentation(FragmentationMetrics *metrics_out,
                                        ErrorContext *ctx) const -> Status
    {
        if (metrics_out == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "metrics_out cannot be null");
            return Status::INVALID_ARGUMENT;
        }

        const auto *hdr = header();
        if (hdr->page_type != PAGE_TYPE_HEAP)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Not a heap page");
            return Status::PAGE_CORRUPT;
        }

        FragmentationMetrics metrics;
        const ItemPointer *items = getItemArray();
        const uint16_t item_count = getItemCount();

        for (uint16_t i = 0; i < item_count; ++i)
        {
            const ItemPointer &item = items[i];
            if (item.isUnused())
            {
                ++metrics.unused_slots;
                continue;
            }

            if (!item.isValid(page_size_))
            {
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                                  "Fragmentation analysis encountered invalid item pointer");
                return Status::PAGE_CORRUPT;
            }

            if (item.isDeleted())
            {
                ++metrics.deleted_slots;
            }
            else
            {
                ++metrics.live_slots;
                metrics.live_tuple_bytes += item.length;
            }

            const uint8_t *tuple_data = page_data_ + item.offset;
            const auto *tuple_hdr = reinterpret_cast<const TupleHeader *>(tuple_data);
            if (tuple_hdr->hasBackVersion())
            {
                ++metrics.chain_depth_hint;
                if (getPageNumber(tuple_hdr->back_version_gpid) == hdr->page_id)
                {
                    ++metrics.same_page_back_versions;
                }
            }
        }

        const uint32_t lower = pageLower(*hdr);
        const uint32_t upper = pageUpper(*hdr);
        const uint32_t special = pageSpecial(*hdr);
        if (lower > upper || upper > special || special > page_size_)
        {
            SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT,
                              "Fragmentation analysis encountered corrupt page bounds");
            return Status::PAGE_CORRUPT;
        }

        metrics.free_bytes = upper - lower;
        const uint32_t tuple_region_bytes = special - upper;
        if (tuple_region_bytes > metrics.live_tuple_bytes)
        {
            metrics.reclaimable_bytes = tuple_region_bytes - metrics.live_tuple_bytes;
        }

        const uint32_t usable_page_bytes = special - sizeof(PageHeader);
        if (usable_page_bytes != 0)
        {
            metrics.dead_space_ratio = static_cast<double>(metrics.reclaimable_bytes) /
                                       static_cast<double>(usable_page_bytes);
        }
        if (metrics.chain_depth_hint != 0)
        {
            metrics.same_page_update_ratio =
                static_cast<double>(metrics.same_page_back_versions) /
                static_cast<double>(metrics.chain_depth_hint);
        }

        metrics.warn_threshold =
            metrics.dead_space_ratio >= FragmentationMetrics::DEAD_SPACE_WARN_RATIO;
        metrics.compact_threshold =
            metrics.dead_space_ratio >= FragmentationMetrics::DEAD_SPACE_COMPACT_RATIO;
        metrics.rewrite_threshold =
            metrics.dead_space_ratio >= FragmentationMetrics::DEAD_SPACE_REWRITE_RATIO;

        *metrics_out = metrics;
        return Status::OK;
    }

    auto HeapPage::prunePage(uint64_t oit, uint32_t *tuples_pruned_out,
                             uint32_t *space_reclaimed_out, ErrorContext *ctx) -> Status
    {
        ItemPointer *items = getItemArray();
        uint16_t item_count = getItemCount();

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
                bool xmax_committed = false;
                Status resolve_status =
                    resolveCommittedDeleteState(db_, tuple_hdr, &xmax_committed, ctx);
                if (resolve_status != Status::OK)
                {
                    return resolve_status;
                }

                if (xmax_committed)
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

        // Get item pointer array
        auto *items = reinterpret_cast<ItemPointer *>(page_data_ + sizeof(PageHeader));
        uint16_t item_count =
            static_cast<uint16_t>((pageLower(*pg_header) - sizeof(PageHeader)) / sizeof(ItemPointer));

        dead_tids_out->clear();

        // Scan all items looking for dead tuples
        for (uint16_t i = 0; i < item_count; i++)
        {
            // Skip unused items
            if (items[i].isUnused())
            {
                continue;
            }

            // Deleted line pointers still contain tuple headers; we need them for index cleanup.

            // Get tuple header
            if (!items[i].isValid(page_size_))
            {
                continue;
            }
            auto *tuple_hdr = reinterpret_cast<TupleHeader *>(page_data_ + items[i].offset);

            // Tuple is dead if:
            // 1. xmax != 0 (deleted/updated)
            // 2. xmax < OIT (deleting transaction is old enough)
            // 3. xmax is committed
            if (tuple_hdr->xmax != 0 && tuple_hdr->xmax < oit)
            {
                bool xmax_committed = false;
                Status resolve_status =
                    resolveCommittedDeleteState(db_, tuple_hdr, &xmax_committed, ctx);
                if (resolve_status != Status::OK)
                {
                    return resolve_status;
                }

                if (xmax_committed)
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
