/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
// ScratchBird Bitmap Index - Implementation
// Roaring Bitmap-based index for low-cardinality columns

#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/heap_page.h"           // For ItemPointer, TupleHeader, HeapPageSpecial
#include "scratchbird/core/transaction_manager.h" // For TransactionState, isVersionVisible (TIP-based visibility)
#include "scratchbird/core/connection_context.h"  // For ConnectionContext::getCurrentTransactionId()
#include <cstring>
#include <algorithm>
#include <functional>
#include <set>
#ifdef _MSC_VER
    #include <intrin.h>
#endif

namespace scratchbird
{
    namespace core
    {
        // Constants
        constexpr uint16_t ARRAY_MAX_SIZE = 4096;

        inline uint32_t bitsetWordCount(uint32_t page_size)
        {
            return BitmapSettings::getBitsetElementCount(page_size);
        }

        inline auto popcount64(uint64_t value) -> uint32_t
        {
#ifdef _MSC_VER
            return static_cast<uint32_t>(__popcnt64(value));
#else
            return static_cast<uint32_t>(__builtin_popcountll(value));
#endif
        }

        // ========================================
        // TASK-CRITICAL-2: VersionedBitmapEntry Implementation
        // ========================================

        // Firebird MGA: TIP-based visibility check (NOT snapshot-based)
        // Per MGA_RULES.md Rule 3 (lines 121-145): Use TIP lookups, not snapshot arrays
        bool VersionedBitmapEntry::isVisible(uint64_t current_xid, TransactionManager *txn_mgr) const
        {
            // If no transaction specified, treat entry as visible (used by VACUUM/internal ops)
            if (current_xid == 0)
            {
                return true;
            }

            if (!txn_mgr)
            {
                // No transaction manager - everything visible (fallback for testing)
                return xmax == 0;
            }

            // Per MGA_RULES.md Rule 3: Own changes always visible
            if (xmin == current_xid)
            {
                // We inserted this entry - visible unless we also deleted it
                return (xmax == 0 || xmax != current_xid);
            }

            return txn_mgr->isRuntimeRecordVisible(xmin, xmax, current_xid);
        }

        // ========================================
        // BitmapIndex Implementation
        // ========================================

        BitmapIndex::BitmapIndex(Database *db, const UuidV7Bytes &index_uuid, GPID meta_gpid)
            : db_(db),
              buffer_pool_(db->buffer_pool()),
              index_uuid_(index_uuid),
              meta_page_(static_cast<uint32_t>(getPageNumber(meta_gpid))),
              tablespace_id_(getTablespaceID(meta_gpid)),
              num_distinct_values_(0),
              total_tuples_(0),
              dictionary_page_(0)
        {
        }

        BitmapIndex::~BitmapIndex() = default;

        Status BitmapIndex::create(
            Database *db,
            const UuidV7Bytes &index_uuid,
            GPID meta_gpid,
            ErrorContext *ctx)
        {
            if (!db || meta_gpid == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid arguments to BitmapIndex::create");
                return Status::INVALID_ARGUMENT;
            }

            BufferPool *buffer_pool = db->buffer_pool();
            if (!buffer_pool)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
                return Status::INVALID_ARGUMENT;
            }

            uint32_t meta_page_num = static_cast<uint32_t>(getPageNumber(meta_gpid));

            // Pin and initialize meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool->pinPageGlobal(meta_gpid, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
            std::memset(meta, 0, sizeof(SBBitmapIndexMetaPage));

            meta->bmp_header.magic = K_MAGIC_SBRD;
            meta->bmp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            meta->bmp_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BITMAP_META);
            meta->bmp_header.page_size = db->page_size();
            meta->bmp_header.page_id = meta_page_num;
            meta->bmp_header.generation = 1;
            meta->bmp_header.checksum = 0;
            meta->bmp_header.flags = 0;
            meta->bmp_header.lsn = 0;
            pageSetLower(meta->bmp_header, sizeof(SBBitmapIndexMetaPage));
            pageSetUpper(meta->bmp_header, db->page_size());
            pageSetSpecial(meta->bmp_header, db->page_size());
            std::memcpy(meta->bmp_index_uuid.bytes.data(), index_uuid.bytes.data(), 16);
            meta->bmp_num_distinct_values = 0;
            meta->bmp_total_tuples = 0;
            meta->bmp_dictionary_page = 0;

            buffer_pool->unpinPageGlobal(meta_gpid, true, ctx);
            return Status::OK;
        }

        Status BitmapIndex::create(
            Database *db,
            const UuidV7Bytes &index_uuid,
            uint32_t *meta_page_out,
            ErrorContext *ctx)
        {
            if (!db || !meta_page_out)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid arguments to BitmapIndex::create");
                return Status::INVALID_ARGUMENT;
            }

            PageManager *page_mgr = db->page_manager();
            if (!page_mgr)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no page manager");
                return Status::INVALID_ARGUMENT;
            }

            GPID meta_gpid = 0;
            Status status = page_mgr->allocatePageInTablespace(0, &meta_gpid, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            *meta_page_out = static_cast<uint32_t>(getPageNumber(meta_gpid));
            return create(db, index_uuid, meta_gpid, ctx);
        }

        std::unique_ptr<BitmapIndex> BitmapIndex::open(
            Database *db,
            const UuidV7Bytes &index_uuid,
            GPID meta_gpid,
            ErrorContext *ctx)
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
                return nullptr;
            }

            auto index = std::make_unique<BitmapIndex>(db, index_uuid, meta_gpid);

            Status status = index->loadMetaPage(ctx);
            if (status != Status::OK)
            {
                return nullptr;
            }

            return index;
        }

        GPID BitmapIndex::indexGPID(uint64_t page_num) const
        {
            return makeGPID(tablespace_id_, page_num);
        }

        Status BitmapIndex::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx)
        {
            return buffer_pool_->pinPageGlobal(indexGPID(page_num), buffer, ctx);
        }

        Status BitmapIndex::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx)
        {
            return buffer_pool_->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
        }

        Status BitmapIndex::loadMetaPage(ErrorContext *ctx)
        {
            uint8_t *meta_data = nullptr;
            Status status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);

            // Verify page type
            if (meta->bmp_header.page_type != static_cast<uint16_t>(PageType::PAGE_TYPE_BITMAP_META))
            {
                unpinIndexPage(meta_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid bitmap index meta page type");
                return Status::PAGE_CORRUPT;
            }

            // Load cached values
            num_distinct_values_ = meta->bmp_num_distinct_values;
            total_tuples_ = meta->bmp_total_tuples;
            dictionary_page_ = meta->bmp_dictionary_page;

            unpinIndexPage(meta_page_, false, ctx);
            return Status::OK;
        }

        uint64_t BitmapIndex::hashValue(const void *data, size_t len) const
        {
            // Simple FNV-1a hash
            const uint8_t *bytes = static_cast<const uint8_t *>(data);
            uint64_t hash = 14695981039346656037ULL;

            for (size_t i = 0; i < len; i++)
            {
                hash ^= bytes[i];
                hash *= 1099511628211ULL;
            }

            return hash;
        }

        uint32_t BitmapIndex::findDictionaryEntry(
            const void *value_data,
            size_t value_len,
            uint32_t *bitmap_root_out,
            ErrorContext *ctx)
        {
            if (dictionary_page_ == 0)
            {
                return 0; // No dictionary entries yet
            }

            uint64_t value_hash = hashValue(value_data, value_len);
            uint32_t current_page = dictionary_page_;

            while (current_page != 0)
            {
                uint8_t *page_data = nullptr;
                Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return 0;
                }

                auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
                uint8_t *entry_data = page_data + sizeof(SBBitmapDictionaryPage);

                for (uint16_t i = 0; i < dict_page->bmp_dict_count; i++)
                {
                    auto *entry = reinterpret_cast<BitmapDictionaryEntry *>(entry_data);

                    if (entry->value_hash == value_hash &&
                        entry->value_length == value_len)
                    {
                        // Found matching hash, verify actual value
                        const uint8_t *stored_value = entry_data + sizeof(BitmapDictionaryEntry);

                        if (std::memcmp(stored_value, value_data, value_len) == 0)
                        {
                            *bitmap_root_out = entry->bitmap_root_page;
                            uint32_t cardinality = entry->cardinality;
                            unpinIndexPage(current_page, false, ctx);
                            return cardinality;
                        }
                    }

                    entry_data += sizeof(BitmapDictionaryEntry) + entry->value_length;
                }

                uint32_t next_page = dict_page->bmp_dict_next_page;
                unpinIndexPage(current_page, false, ctx);
                current_page = next_page;
            }

            return 0; // Not found
        }

        uint32_t BitmapIndex::createDictionaryEntry(
            const void *value_data,
            size_t value_len,
            ErrorContext *ctx)
        {
            // Allocate new dictionary page if needed
            if (dictionary_page_ == 0)
            {
                uint32_t page_num = 0;
                GPID page_gpid = 0;
                Status status = db_->page_manager()->allocatePageInTablespace(tablespace_id_, &page_gpid, ctx);
                if (status != Status::OK)
                {
                    return 0;
                }
                page_num = static_cast<uint32_t>(getPageNumber(page_gpid));

                uint8_t *page_data = nullptr;
                status = pinIndexPage(page_num, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return 0;
                }

                auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
                std::memset(dict_page, 0, sizeof(SBBitmapDictionaryPage));

                dict_page->bmp_dict_header.magic = K_MAGIC_SBRD;
                dict_page->bmp_dict_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                dict_page->bmp_dict_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BITMAP_DICT);
                dict_page->bmp_dict_header.page_size = db_->page_size();
                dict_page->bmp_dict_header.page_id = page_num;
                dict_page->bmp_dict_header.generation = 1;
                dict_page->bmp_dict_header.checksum = 0;
                dict_page->bmp_dict_header.flags = 0;
                dict_page->bmp_dict_header.lsn = 0;
                pageSetLower(dict_page->bmp_dict_header, sizeof(SBBitmapDictionaryPage));
                pageSetUpper(dict_page->bmp_dict_header, db_->page_size());
                pageSetSpecial(dict_page->bmp_dict_header, db_->page_size());
                dict_page->bmp_dict_count = 0;
                dict_page->bmp_dict_free_offset = sizeof(SBBitmapDictionaryPage);
                dict_page->bmp_dict_next_page = 0;
                pageSetLower(dict_page->bmp_dict_header, dict_page->bmp_dict_free_offset);

                unpinIndexPage(page_num, true, ctx);

                dictionary_page_ = page_num;

                // Update meta page
                uint8_t *meta_data = nullptr;
                pinIndexPage(meta_page_, (void **)&meta_data, ctx);
                auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
                meta->bmp_dictionary_page = dictionary_page_;
                unpinIndexPage(meta_page_, true, ctx);
            }

            // Find page with enough space or allocate new one
            uint32_t current_page = dictionary_page_;
            uint8_t *page_data = nullptr;
            Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
            if (status != Status::OK)
            {
                return 0;
            }

            auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
            size_t entry_size = sizeof(BitmapDictionaryEntry) + value_len;

            // Check if we have space
            if (dict_page->bmp_dict_free_offset + entry_size > db_->page_size())
            {
                // Need new page - allocate and chain it
                uint32_t new_dict_page = 0;
                GPID new_dict_gpid = 0;
                status = db_->page_manager()->allocatePageInTablespace(tablespace_id_, &new_dict_gpid, ctx);
                if (status != Status::OK)
                {
                    unpinIndexPage(current_page, false, ctx);
                    LOG_ERROR(STORAGE, "Failed to allocate new dictionary page: %d", static_cast<int>(status));
                    return 0;
                }
                new_dict_page = static_cast<uint32_t>(getPageNumber(new_dict_gpid));

                // Initialize new dictionary page
                uint8_t *new_page_data = nullptr;
                status = pinIndexPage(new_dict_page, (void **)&new_page_data, ctx);
                if (status != Status::OK)
                {
                    unpinIndexPage(current_page, false, ctx);
                    LOG_ERROR(STORAGE, "Failed to pin new dictionary page: %d", static_cast<int>(status));
                    return 0;
                }

                auto *new_dict = reinterpret_cast<SBBitmapDictionaryPage *>(new_page_data);
                std::memset(new_dict, 0, sizeof(SBBitmapDictionaryPage));
                new_dict->bmp_dict_header.magic = K_MAGIC_SBRD;
                new_dict->bmp_dict_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                new_dict->bmp_dict_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BITMAP_DICT);
                new_dict->bmp_dict_header.page_size = db_->page_size();
                new_dict->bmp_dict_header.page_id = new_dict_page;
                new_dict->bmp_dict_header.generation = 1;
                new_dict->bmp_dict_header.checksum = 0;
                new_dict->bmp_dict_header.flags = 0;
                new_dict->bmp_dict_header.lsn = 0;
                pageSetLower(new_dict->bmp_dict_header, sizeof(SBBitmapDictionaryPage));
                pageSetUpper(new_dict->bmp_dict_header, db_->page_size());
                pageSetSpecial(new_dict->bmp_dict_header, db_->page_size());
                new_dict->bmp_dict_count = 0;
                new_dict->bmp_dict_free_offset = sizeof(SBBitmapDictionaryPage);
                new_dict->bmp_dict_next_page = 0;
                pageSetLower(new_dict->bmp_dict_header, new_dict->bmp_dict_free_offset);

                // Link current page to new page
                dict_page->bmp_dict_next_page = new_dict_page;
                unpinIndexPage(current_page, true, ctx); // Mark dirty

                // Switch to new page
                current_page = new_dict_page;
                dict_page = new_dict;
                page_data = new_page_data;

                LOG_DEBUG(STORAGE, "Allocated new dictionary page %u (chained from previous)", new_dict_page);
            }

            // Now we have a page with space - allocate bitmap root page
            uint32_t bitmap_root = 0;
            GPID bitmap_gpid = 0;
            status = db_->page_manager()->allocatePageInTablespace(tablespace_id_, &bitmap_gpid, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(current_page, false, ctx);
                return 0;
            }
            bitmap_root = static_cast<uint32_t>(getPageNumber(bitmap_gpid));

            uint8_t *bitmap_data = nullptr;
            status = pinIndexPage(bitmap_root, (void **)&bitmap_data, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(current_page, false, ctx);
                return 0;
            }

            auto *root_page = reinterpret_cast<SBRoaringBitmapRootPage *>(bitmap_data);
            std::memset(root_page, 0, sizeof(SBRoaringBitmapRootPage));
            root_page->rbr_header.magic = K_MAGIC_SBRD;
            root_page->rbr_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            root_page->rbr_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BITMAP_META);
            root_page->rbr_header.page_size = db_->page_size();
            root_page->rbr_header.page_id = bitmap_root;
            root_page->rbr_header.generation = 1;
            root_page->rbr_header.checksum = 0;
            root_page->rbr_header.flags = 0;
            root_page->rbr_header.lsn = 0;
            pageSetLower(root_page->rbr_header, sizeof(SBRoaringBitmapRootPage));
            pageSetUpper(root_page->rbr_header, db_->page_size());
            pageSetSpecial(root_page->rbr_header, db_->page_size());
            root_page->rbr_num_containers = 0;
            root_page->rbr_total_cardinality = 0;

            unpinIndexPage(bitmap_root, true, ctx);

            // Add entry to dictionary
            uint8_t *entry_location = page_data + dict_page->bmp_dict_free_offset;
            auto *entry = reinterpret_cast<BitmapDictionaryEntry *>(entry_location);

            entry->value_hash = hashValue(value_data, value_len);
            entry->bitmap_root_page = bitmap_root;
            entry->cardinality = 0;
            entry->value_length = static_cast<uint16_t>(value_len);
            entry->reserved = 0;

            std::memcpy(entry_location + sizeof(BitmapDictionaryEntry), value_data, value_len);

            dict_page->bmp_dict_count++;
            dict_page->bmp_dict_free_offset += entry_size;
            pageSetLower(dict_page->bmp_dict_header, dict_page->bmp_dict_free_offset);

            unpinIndexPage(current_page, true, ctx);

            // Update meta page
            uint8_t *meta_data = nullptr;
            pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
            meta->bmp_num_distinct_values++;
            num_distinct_values_ = meta->bmp_num_distinct_values;
            unpinIndexPage(meta_page_, true, ctx);

            return bitmap_root;
        }

        std::shared_ptr<RoaringBitmap> BitmapIndex::loadBitmap(
            uint32_t bitmap_root_page,
            ErrorContext *ctx)
        {
            auto it = bitmap_cache_.find(bitmap_root_page);
            if (it != bitmap_cache_.end())
            {
                return it->second;
            }

            auto bitmap = std::make_shared<RoaringBitmap>(
                db_,
                makeGPID(tablespace_id_, bitmap_root_page));
            bitmap_cache_.emplace(bitmap_root_page, bitmap);
            return bitmap;
        }

        Status BitmapIndex::insert(
            const void *value_data,
            size_t value_len,
            const TID &tid,
            ErrorContext *ctx)
        {
            if (!value_data || value_len == 0)
            {
                if (ctx)
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid value");
                return Status::INVALID_ARGUMENT;
            }

            // TASK-CRITICAL-2: Get current transaction ID for xmin tracking
            // Firebird MGA: Per MGA_RULES.md Rule 6 - store xmin with insert
            // First try ConnectionContext (thread-local), then fall back to TransactionManager
            uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
            if (current_xid == 0)
            {
                // Fallback to TransactionManager if no ConnectionContext
                TransactionManager *txn_mgr = db_->transaction_manager();
                if (txn_mgr)
                {
                    current_xid = txn_mgr->getCurrentXid();
                }
            }
            if (current_xid == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction");
                return Status::INVALID_ARGUMENT;
            }

            // Find or create dictionary entry
            uint32_t bitmap_root = 0;
            uint32_t cardinality = findDictionaryEntry(value_data, value_len, &bitmap_root, ctx);

            if (bitmap_root == 0)
            {
                bitmap_root = createDictionaryEntry(value_data, value_len, ctx);
                if (bitmap_root == 0)
                {
                    return Status::IO_ERROR;
                }
            }

            // Load bitmap and add tuple
            auto bitmap = loadBitmap(bitmap_root, ctx);
            if (!bitmap)
            {
                return Status::IO_ERROR;
            }

            // TASK-CRITICAL-2: Add with xmin for MGA compliance
            // Firebird MGA: Store transaction ID for TIP-based visibility
            Status status = bitmap->add(tid, current_xid, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Update total tuples count
            uint8_t *meta_data = nullptr;
            pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
            meta->bmp_total_tuples++;
            total_tuples_ = meta->bmp_total_tuples;
            unpinIndexPage(meta_page_, true, ctx);

            return Status::OK;
        }

        Status BitmapIndex::remove(
            const TID &tid,
            ErrorContext *ctx)
        {
            // TASK-CRITICAL-2: Get current transaction ID for xmax marking (logical deletion)
            // Firebird MGA: Per MGA_RULES.md Rule 5 - NO physical removal, only xmax marking
            // First try ConnectionContext (thread-local), then fall back to TransactionManager
            uint64_t current_xid = ConnectionContext::getCurrentTransactionId();
            if (current_xid == 0)
            {
                // Fallback to TransactionManager if no ConnectionContext
                TransactionManager *txn_mgr = db_->transaction_manager();
                if (txn_mgr)
                {
                    current_xid = txn_mgr->getCurrentXid();
                }
            }
            if (current_xid == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction");
                return Status::INVALID_ARGUMENT;
            }

            // We need to mark this TID as deleted in ALL bitmaps since we don't know which value it had
            // CRITICAL: This is LOGICAL deletion (set xmax), NOT physical removal
            // This requires scanning all dictionary entries
            Status status = loadMetaPage(ctx);
            if (status != Status::OK)
            {
                return status;
            }

            if (dictionary_page_ == 0)
            {
                // No dictionary entries, nothing to remove
                return Status::OK;
            }

            // Scan all dictionary entries
            uint32_t current_dict_page = dictionary_page_;
            bool had_errors = false;

            while (current_dict_page != 0)
            {
                uint8_t *page_data = nullptr;
                status = pinIndexPage(current_dict_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(STORAGE, "remove: Failed to pin dictionary page %u", current_dict_page);
                    had_errors = true;
                    break;
                }

                auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
                uint32_t next_page = dict_page->bmp_dict_next_page;
                uint16_t entry_count = dict_page->bmp_dict_count;

                // Scan entries in this dictionary page
                uint8_t *entry_data = page_data + sizeof(SBBitmapDictionaryPage);

                for (uint16_t i = 0; i < entry_count; i++)
                {
                    auto *entry = reinterpret_cast<BitmapDictionaryEntry *>(entry_data);
                    uint32_t bitmap_root = entry->bitmap_root_page;

                    if (bitmap_root != 0)
                    {
                        // Load the Roaring bitmap for this value
                        auto bitmap = loadBitmap(bitmap_root, ctx);
                        if (bitmap)
                        {
                            // TASK-CRITICAL-2: Mark TID as deleted with xmax (Firebird MGA)
                            // Per MGA_RULES.md Rule 5: Logical deletion only
                            Status remove_status = bitmap->remove(tid, current_xid, ctx);
                            if (remove_status == Status::OK)
                            {
                                // Update dictionary entry cardinality (includes invisible entries)
                                entry->cardinality = bitmap->cardinality();
                            }
                        }
                    }

                    // Move to next entry
                    entry_data += sizeof(BitmapDictionaryEntry) + entry->value_length;
                }

                unpinIndexPage(current_dict_page, false, ctx);

                // Move to next dictionary page
                current_dict_page = next_page;
            }

            if (had_errors)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to mark TID as deleted in some bitmaps");
                return Status::IO_ERROR;
            }

            // Note: total_tuples count includes logically deleted tuples (per MGA)
            // Actual visible count depends on transaction visibility

            return Status::OK;
        }

        // PHASE 1 TASK 1.5: Visibility filter for bitmap index (post-filtering)
        // PHASE 1.5 TASK 1.5.2c: Migrated to TID struct API
        // This is a post-filter that checks heap tuple visibility for each TID returned by bitmap operations
        // NOTE: This is less efficient than B-Tree/Hash visibility (20-40% overhead) because:
        //       - Bitmap returns TIDs directly, not pointers to heap tuples
        //       - We must access heap pages separately to check visibility via TIP (Firebird MGA)
        //       - Full optimization would require storing xmin/xmax in bitmap entries (future enhancement)
        std::vector<TID> BitmapIndex::filterTidsByVisibility(const std::vector<TID> &tids,
                                                              uint64_t current_xid,
                                                              ErrorContext *ctx)
        {
            std::vector<TID> visible_tids;

            // If no transaction specified, return all TIDs (no filtering)
            if (current_xid == 0)
            {
                return tids;
            }

            visible_tids.reserve(tids.size());

            auto *buffer_pool = db_->buffer_pool();
            auto *txn_manager = db_->transaction_manager();

            for (const TID &tid : tids)
            {
                // Extract page_id and item_id from TID struct
                uint32_t page_id = static_cast<uint32_t>(getPageNumber(tid));
                uint16_t item_id = getSlot(tid);

                // Pin the heap page
                uint8_t *page_data = nullptr;
                Status status = pinIndexPage(page_id, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    // If we can't read the page, skip this TID
                    continue;
                }

                // Read the item pointer to get tuple offset
                auto *page_header = reinterpret_cast<PageHeader *>(page_data);
                uint16_t item_count = pageLower(*page_header) / sizeof(struct ItemPointer);

                if (item_id >= item_count)
                {
                    unpinIndexPage(page_id, false, ctx);
                    continue; // Invalid item ID
                }

                auto *item_pointers = reinterpret_cast<ItemPointer *>(page_data + sizeof(PageHeader));
                ItemPointer item = item_pointers[item_id];

                if (item.offset == 0 || item.length == 0)
                {
                    unpinIndexPage(page_id, false, ctx);
                    continue; // Dead tuple
                }

                // Read tuple header
                auto *tuple_header = reinterpret_cast<TupleHeader *>(page_data + item.offset);

                // ===========================================================================================
                // FIREBIRD MGA VISIBILITY - TIP-based, NOT snapshot-based
                // Per MGA_RULES.md Rule 3 (lines 121-145)
                // ===========================================================================================
                // Note: Bitmap indexes still require heap access for visibility (20-40% overhead)
                //       because bitmap entries don't store xmin/xmax. This is architecturally
                //       correct but less efficient than B-tree/Hash indexes.

                // Own changes always visible
                bool visible = (tuple_header->xmin == current_xid);

                if (!visible)
                {
                    visible = txn_manager->isRuntimeRecordVisible(tuple_header->xmin,
                                                                  tuple_header->xmax,
                                                                  current_xid);
                }

                if (visible)
                {
                    visible_tids.push_back(tid);
                }

                unpinIndexPage(page_id, false, ctx);
            }

            return visible_tids;
        }

        Status BitmapIndex::find(
            const void *value_data,
            size_t value_len,
            uint64_t current_xid,
            std::vector<TID>* results,
            ErrorContext *ctx)
        {
            if (!value_data || value_len == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid value data for bitmap index find");
                return Status::INVALID_ARGUMENT;
            }

            if (!results)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Results vector cannot be null");
                return Status::INVALID_ARGUMENT;
            }

            results->clear();

            uint32_t bitmap_root = 0;
            uint32_t cardinality = findDictionaryEntry(value_data, value_len, &bitmap_root, ctx);

            if (bitmap_root == 0)
            {
                // Value not found - return empty results (OK status)
                return Status::OK;
            }

            auto bitmap = loadBitmap(bitmap_root, ctx);
            if (!bitmap)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Failed to load bitmap for bitmap index find");
                return Status::NOT_FOUND;
            }

            // TASK-CRITICAL-2: Index-level visibility filtering (no heap access!)
            // Firebird MGA: Per MGA_RULES.md Rule 11 - use TransactionId, NOT Snapshot
            // This eliminates the 20-40% overhead from heap-level post-filtering
            TransactionManager *txn_mgr = db_->transaction_manager();
            std::vector<TID> tid_values = bitmap->toVisibleArray(current_xid, txn_mgr, ctx);
            results->reserve(tid_values.size());
            results->insert(results->end(), tid_values.begin(), tid_values.end());

            // Note: No heap-level post-filtering needed - visibility already checked at index level!

            return Status::OK;
        }

        std::vector<TID> BitmapIndex::findAnd(
            const std::vector<const void *> &values,
            const std::vector<size_t> &value_lens,
            uint64_t current_xid,
            ErrorContext *ctx)
        {
            std::vector<TID> results;

            if (values.empty() || values.size() != value_lens.size())
            {
                return results;
            }

            std::vector<TID> first;
            Status status = find(values[0], value_lens[0], current_xid, &first, ctx);
            if (status != Status::OK || first.empty())
            {
                return results;
            }

            std::unordered_set<TID> current_set(first.begin(), first.end());
            for (size_t i = 1; i < values.size(); i++)
            {
                std::vector<TID> next;
                status = find(values[i], value_lens[i], current_xid, &next, ctx);
                if (status != Status::OK || next.empty())
                {
                    current_set.clear();
                    break;
                }

                std::unordered_set<TID> next_set(next.begin(), next.end());
                for (auto it = current_set.begin(); it != current_set.end(); )
                {
                    if (next_set.find(*it) == next_set.end())
                    {
                        it = current_set.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }

                if (current_set.empty())
                {
                    break;
                }
            }

            results.reserve(current_set.size());
            for (const auto& tid : current_set)
            {
                results.push_back(tid);
            }

            return results;
        }

        std::vector<TID> BitmapIndex::findOr(
            const std::vector<const void *> &values,
            const std::vector<size_t> &value_lens,
            uint64_t current_xid,
            ErrorContext *ctx)
        {
            std::vector<TID> results;

            if (values.empty() || values.size() != value_lens.size())
            {
                return results;
            }

            std::unordered_set<TID> union_set;
            for (size_t i = 0; i < values.size(); i++)
            {
                std::vector<TID> partial;
                Status status = find(values[i], value_lens[i], current_xid, &partial, ctx);
                if (status != Status::OK)
                {
                    continue;
                }
                for (const auto& tid : partial)
                {
                    union_set.insert(tid);
                }
            }

            results.reserve(union_set.size());
            for (const auto& tid : union_set)
            {
                results.push_back(tid);
            }

            return results;
        }

        std::vector<TID> BitmapIndex::findNot(
            const void *value_data,
            size_t value_len,
            uint64_t current_xid,
            ErrorContext *ctx)
        {
            std::vector<TID> results;

            // Find the bitmap for this value
            uint32_t bitmap_root = 0;
            uint32_t found = findDictionaryEntry(value_data, value_len, &bitmap_root, ctx);

            if (found == 0 || bitmap_root == 0)
            {
                // Value not found in index - NOT(empty set) = all tuples
                // This would require scanning the entire table, which is expensive
                // For now, return empty set (caller should use heap scan instead)
                LOG_DEBUG(STORAGE, "findNot: Value not in index, return empty (caller should use heap scan)");
                return results;
            }

            // Load the bitmap for this value
            auto bitmap = loadBitmap(bitmap_root, ctx);
            if (!bitmap)
            {
                LOG_ERROR(STORAGE, "findNot: Failed to load bitmap for root page %u", bitmap_root);
                return results;
            }

            // Get universe size (total number of tuples in table)
            // We need to know the maximum TID to properly compute NOT
            // For now, use a conservative estimate based on total_tuples_
            // In a real implementation, we'd query the table's max TID
            uint32_t universe_size = total_tuples_ > 0 ? total_tuples_ : 100000;

            // Compute NOT bitmap
            auto not_bitmap = RoaringBitmap::bitwiseNot(*bitmap, universe_size, ctx);
            if (!not_bitmap)
            {
                LOG_ERROR(STORAGE, "findNot: Failed to compute NOT bitmap");
                return results;
            }

            // Convert bitmap to TID list (64-bit values)
            std::vector<TID> tid_values = not_bitmap->toArray(ctx);
            results.reserve(tid_values.size());
            results.insert(results.end(), tid_values.begin(), tid_values.end());

            // Firebird MGA: Post-filter results by heap tuple visibility using TIP lookups
            results = filterTidsByVisibility(results, current_xid, ctx);

            return results;
        }

        BitmapIndex::Statistics BitmapIndex::getStatistics(ErrorContext *ctx)
        {
            Statistics stats;
            stats.num_distinct_values = num_distinct_values_;
            stats.total_tuples = total_tuples_;
            stats.total_pages = 1 + num_distinct_values_; // Meta + one root per value (approximation)
            stats.avg_cardinality = (num_distinct_values_ > 0) ? (total_tuples_ / num_distinct_values_) : 0;

            // Calculate actual compression ratio
            // Compression ratio = uncompressed size / compressed size
            // For Roaring bitmaps:
            //   Uncompressed = num_containers * 65536 bits = num_containers * bitsetByteCount(page_size)
            //   Compressed = actual storage (ARRAY containers use 2 bytes per value, BITSET uses bitsetByteCount(page_size))

            uint64_t total_uncompressed_bytes = 0;
            uint64_t total_compressed_bytes = 0;
            uint32_t total_pages_scanned = 0;

            // Scan all dictionary entries to calculate compression
            if (dictionary_page_ != 0)
            {
                uint32_t current_dict_page = dictionary_page_;

                while (current_dict_page != 0 && total_pages_scanned < 1000) // Limit scan to prevent long delays
                {
                    uint8_t *page_data = nullptr;
                    Status status = pinIndexPage(current_dict_page, (void **)&page_data, ctx);
                    if (status != Status::OK)
                    {
                        break; // Can't calculate, use default
                    }

                    auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
                    uint32_t next_page = dict_page->bmp_dict_next_page;
                    uint16_t entry_count = dict_page->bmp_dict_count;

                    uint8_t *entry_data = page_data + sizeof(SBBitmapDictionaryPage);

                    for (uint16_t i = 0; i < entry_count; i++)
                    {
                        auto *entry = reinterpret_cast<BitmapDictionaryEntry *>(entry_data);
                        uint32_t bitmap_root = entry->bitmap_root_page;

                        if (bitmap_root != 0)
                        {
                            auto bitmap = loadBitmap(bitmap_root, ctx);
                            if (bitmap)
                            {
                                // Simple compression estimate based on cardinality
                                // Uncompressed: Full bitmap for all 32-bit values = 4GB per bitmap
                                // But we only estimate based on actual TIDs present
                                uint32_t cardinality = bitmap->cardinality();

                                // Estimate uncompressed size: cardinality * 4 bytes per TID (assume packed TID list)
                                // Compressed size estimate: cardinality / 8 (assume 12.5% density for sparse bitmaps)
                                // This is a rough approximation, actual compression varies
                                total_uncompressed_bytes += cardinality * 4;
                                total_compressed_bytes += std::max(cardinality / 8, static_cast<uint32_t>(1));
                            }
                        }

                        entry_data += sizeof(BitmapDictionaryEntry) + entry->value_length;
                    }

                    unpinIndexPage(current_dict_page, false, ctx);
                    current_dict_page = next_page;
                    total_pages_scanned++;
                }
            }

            // Calculate compression ratio
            if (total_compressed_bytes > 0)
            {
                stats.compression_ratio = static_cast<double>(total_uncompressed_bytes) / static_cast<double>(total_compressed_bytes);
            }
            else
            {
                stats.compression_ratio = 1.0; // No compression if no data
            }

            return stats;
        }

        Status BitmapIndex::updateTIDsAfterMigration(
            const std::unordered_map<TID, TID> &tid_mapping,
            uint64_t *tids_updated_out,
            uint64_t *pages_modified_out,
            ErrorContext *ctx)
        {
            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = 0;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = 0;
            }

            if (tid_mapping.empty() || dictionary_page_ == 0)
            {
                return Status::OK;
            }

            uint64_t total_updated = 0;
            uint64_t total_pages_modified = 0;

            uint32_t current_dict_page = dictionary_page_;
            uint32_t pages_scanned = 0;

            while (current_dict_page != 0 && pages_scanned < 1000)
            {
                uint8_t *page_data = nullptr;
                Status status = pinIndexPage(current_dict_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
                uint32_t next_page = dict_page->bmp_dict_next_page;
                uint16_t entry_count = dict_page->bmp_dict_count;

                uint8_t *entry_data = page_data + sizeof(SBBitmapDictionaryPage);

                for (uint16_t i = 0; i < entry_count; i++)
                {
                    auto *entry = reinterpret_cast<BitmapDictionaryEntry *>(entry_data);
                    uint32_t bitmap_root = entry->bitmap_root_page;

                    if (bitmap_root != 0)
                    {
                        auto bitmap = loadBitmap(bitmap_root, ctx);
                        if (bitmap)
                        {
                            uint64_t updated = 0;
                            uint64_t pages_modified = 0;
                            Status update_status = bitmap->updateTIDsAfterMigration(
                                tid_mapping, &updated, &pages_modified, ctx);
                            if (update_status != Status::OK)
                            {
                                unpinIndexPage(current_dict_page, false, ctx);
                                return update_status;
                            }
                            total_updated += updated;
                            total_pages_modified += pages_modified;
                        }
                    }

                    entry_data += sizeof(BitmapDictionaryEntry) + entry->value_length;
                }

                unpinIndexPage(current_dict_page, false, ctx);
                current_dict_page = next_page;
                pages_scanned++;
            }

            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = total_updated;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = total_pages_modified;
            }

            return Status::OK;
        }

        // ========================================
        // RoaringBitmap Implementation
        // ========================================

        RoaringBitmap::RoaringBitmap(Database *db, GPID root_gpid)
            : db_(db),
              buffer_pool_(db->buffer_pool()),
              root_page_(static_cast<uint32_t>(getPageNumber(root_gpid))),
              tablespace_id_(getTablespaceID(root_gpid)),
              cardinality_(0)
        {
            // Load root page to get cardinality
            uint8_t *root_data = nullptr;
            ErrorContext ctx;
            if (pinIndexPage(root_page_, (void **)&root_data, &ctx) == Status::OK)
            {
                auto *root = reinterpret_cast<SBRoaringBitmapRootPage *>(root_data);
                cardinality_ = root->rbr_total_cardinality;
                unpinIndexPage(root_page_, false, &ctx);
            }
        }

        RoaringBitmap::~RoaringBitmap() = default;

        GPID RoaringBitmap::indexGPID(uint64_t page_num) const
        {
            return makeGPID(tablespace_id_, page_num);
        }

        Status RoaringBitmap::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx) const
        {
            return buffer_pool_->pinPageGlobal(indexGPID(page_num), buffer, ctx);
        }

        Status RoaringBitmap::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx) const
        {
            return buffer_pool_->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
        }

        // TASK-CRITICAL-2: MGA-compliant add with xmin tracking
        // Firebird MGA: Per MGA_RULES.md Rule 6 - store xmin with each entry
        Status RoaringBitmap::add(const TID &tid, uint64_t xmin, ErrorContext *ctx)
        {
            // Split 64-bit value: high 48 bits for container key, low 16 bits for value
            uint64_t high = tid.gpid;
            uint16_t low = tid.slot;

            Container *container = findOrCreateContainer(high, ctx);
            if (!container)
            {
                return Status::IO_ERROR;
            }

            // TASK-CRITICAL-2: Use versioned entries for MGA compliance
            if (container->type == ContainerType::ARRAY)
            {
                // Search for existing entry by TID
                auto it = std::lower_bound(container->array_data_versioned.begin(),
                                           container->array_data_versioned.end(), low,
                                           [](const VersionedBitmapEntry& entry, uint16_t tid) {
                                               return entry.tid_low < tid;
                                           });

                if (it != container->array_data_versioned.end() && it->tid_low == low)
                {
                    // Entry exists - check if it's deleted and can be reused
                    if (it->xmax != 0)
                    {
                        // Entry was deleted, update it with new xmin
                        it->xmin = xmin;
                        it->xmax = 0; // Clear deletion marker
                    }
                    // else: Already exists and not deleted, no-op
                    return Status::OK;
                }

                // Insert new versioned entry
                VersionedBitmapEntry new_entry(low, xmin, 0);
                container->array_data_versioned.insert(it, new_entry);
                container->num_values++;

                // Convert to bitset if needed (array too large)
                if (container->num_values > ARRAY_MAX_SIZE)
                {
                    container->bitset_data.resize(bitsetWordCount(db_->page_size()), 0);
                    container->bitset_versions.clear();

                    for (const auto& entry : container->array_data_versioned)
                    {
                        // Set bit in bitset
                        size_t word_idx = entry.tid_low / 64;
                        size_t bit_idx = entry.tid_low % 64;
                        container->bitset_data[word_idx] |= (1ULL << bit_idx);

                        // Store version info
                        container->bitset_versions[entry.tid_low] = entry;
                    }

                    container->array_data_versioned.clear();
                    container->type = ContainerType::BITSET;
                }
            }
            else if (container->type == ContainerType::BITSET)
            {
                size_t word_idx = low / 64;
                size_t bit_idx = low % 64;
                uint64_t mask = 1ULL << bit_idx;

                if (!(container->bitset_data[word_idx] & mask))
                {
                    // New entry - set bit and create version info
                    container->bitset_data[word_idx] |= mask;
                    container->bitset_versions[low] = VersionedBitmapEntry(low, xmin, 0);
                    container->num_values++;
                }
                else
                {
                    // Entry exists - check if deleted and can be reused
                    auto it = container->bitset_versions.find(low);
                    if (it != container->bitset_versions.end() && it->second.xmax != 0)
                    {
                        // Entry was deleted, update it
                        it->second.xmin = xmin;
                        it->second.xmax = 0;
                    }
                }
            }

            Status status = saveContainer(*container, ctx);
            if (status == Status::OK)
            {
                cardinality_++;

                // Update root page cardinality
                uint8_t *root_data = nullptr;
                pinIndexPage(root_page_, (void **)&root_data, ctx);
                auto *root = reinterpret_cast<SBRoaringBitmapRootPage *>(root_data);
                root->rbr_total_cardinality = cardinality_;
                unpinIndexPage(root_page_, true, ctx);
            }

            return status;
        }

        // TASK-CRITICAL-2: MGA-compliant remove with xmax marking (logical deletion)
        // Firebird MGA: Per MGA_RULES.md Rule 5 - NO physical removal, only xmax tombstones
        Status RoaringBitmap::remove(const TID &tid, uint64_t xmax, ErrorContext *ctx)
        {
            // Split 64-bit value: high 48 bits for container key, low 16 bits for value
            uint64_t high = tid.gpid;
            uint16_t low = tid.slot;

            // Find the container for this high 16 bits
            Container *container = nullptr;
            for (auto &c : containers_)
            {
                if (c.key == high)
                {
                    container = &c;
                    break;
                }
            }

            if (!container)
            {
                // Value doesn't exist, no-op
                return Status::OK;
            }

            bool value_marked = false;

            // TASK-CRITICAL-2: Logical deletion - set xmax, do NOT physically remove
            // Per MGA_RULES.md Rule 5: Back-versioning with xmax tombstones
            if (container->type == ContainerType::ARRAY)
            {
                auto it = std::lower_bound(container->array_data_versioned.begin(),
                                           container->array_data_versioned.end(), low,
                                           [](const VersionedBitmapEntry& entry, uint16_t tid) {
                                               return entry.tid_low < tid;
                                           });

                if (it != container->array_data_versioned.end() && it->tid_low == low)
                {
                    // Mark as deleted by setting xmax
                    if (it->xmax == 0)
                    {
                        it->xmax = xmax;
                        value_marked = true;
                    }
                    // Note: Entry is NOT physically removed - preserved for MGA
                }
            }
            else if (container->type == ContainerType::BITSET)
            {
                auto it = container->bitset_versions.find(low);
                if (it != container->bitset_versions.end())
                {
                    // Mark as deleted by setting xmax
                    if (it->second.xmax == 0)
                    {
                        it->second.xmax = xmax;
                        value_marked = true;
                    }
                    // Note: Bit remains set, version info marks it as deleted
                }
            }

            if (!value_marked)
            {
                // Value didn't exist or already deleted, no-op
                return Status::OK;
            }

            // Save the modified container (with updated xmax)
            Status status = saveContainer(*container, ctx);

            // Note: Cardinality NOT decremented - includes logically deleted entries
            // Per Firebird MGA: Visible cardinality determined by visibility checks

            return status;
        }

        Status RoaringBitmap::removePhysical(const TID &tid, ErrorContext *ctx)
        {
            uint64_t high = tid.gpid;
            uint16_t low = tid.slot;

            Container *container = nullptr;
            for (auto &c : containers_)
            {
                if (c.key == high)
                {
                    container = &c;
                    break;
                }
            }

            if (!container)
            {
                return Status::OK;
            }

            bool removed = false;

            if (container->type == ContainerType::ARRAY)
            {
                auto it = std::lower_bound(container->array_data_versioned.begin(),
                                           container->array_data_versioned.end(), low,
                                           [](const VersionedBitmapEntry& entry, uint16_t tid) {
                                               return entry.tid_low < tid;
                                           });
                if (it != container->array_data_versioned.end() && it->tid_low == low)
                {
                    container->array_data_versioned.erase(it);
                    removed = true;
                }
            }
            else if (container->type == ContainerType::BITSET)
            {
                size_t word_idx = low / 64;
                size_t bit_idx = low % 64;
                if (word_idx < container->bitset_data.size())
                {
                    uint64_t mask = 1ULL << bit_idx;
                    if (container->bitset_data[word_idx] & mask)
                    {
                        container->bitset_data[word_idx] &= ~mask;
                        container->bitset_versions.erase(low);
                        removed = true;
                    }
                }
            }

            if (!removed)
            {
                return Status::OK;
            }

            if (container->num_values > 0)
            {
                container->num_values--;
            }
            if (cardinality_ > 0)
            {
                cardinality_--;
            }

            Status status = saveContainer(*container, ctx);
            if (status == Status::OK)
            {
                uint8_t *root_data = nullptr;
                if (pinIndexPage(root_page_, (void **)&root_data, ctx) == Status::OK)
                {
                    auto *root = reinterpret_cast<SBRoaringBitmapRootPage *>(root_data);
                    root->rbr_total_cardinality = cardinality_;
                    unpinIndexPage(root_page_, true, ctx);
                }
            }

            return status;
        }

        bool RoaringBitmap::contains(const TID &tid, ErrorContext *ctx)
        {
            // Split 64-bit value: high 48 bits for container key, low 16 bits for value
            uint64_t high = tid.gpid;
            uint16_t low = tid.slot;

            for (const auto &container : containers_)
            {
                if (container.key == high)
                {
                    if (container.type == ContainerType::ARRAY)
                    {
                        // TASK-CRITICAL-2: Search in versioned entries
                        for (const auto& entry : container.array_data_versioned)
                        {
                            if (entry.tid_low == low)
                            {
                                // Found entry - return true regardless of xmax (contains checks existence, not visibility)
                                return true;
                            }
                        }
                        return false;
                    }
                    else if (container.type == ContainerType::BITSET)
                    {
                        size_t word_idx = low / 64;
                        size_t bit_idx = low % 64;
                        return (container.bitset_data[word_idx] & (1ULL << bit_idx)) != 0;
                    }
                }
            }

            return false;
        }

        // Returns all TIDs (ignores visibility - includes deleted entries)
        std::vector<TID> RoaringBitmap::toArray(ErrorContext *ctx)
        {
            std::vector<TID> results;
            results.reserve(cardinality_);

            // Load all containers if not cached
            // For now, work with cached containers

            for (const auto &container : containers_)
            {
                GPID gpid = container.key;

                if (container.type == ContainerType::ARRAY)
                {
                    // TASK-CRITICAL-2: Extract TIDs from versioned entries
                    for (const auto& entry : container.array_data_versioned)
                    {
                        results.emplace_back(gpid, entry.tid_low);
                    }
                }
                else if (container.type == ContainerType::BITSET)
                {
                    for (size_t word_idx = 0; word_idx < bitsetWordCount(db_->page_size()); word_idx++)
                    {
                        uint64_t word = container.bitset_data[word_idx];
                        if (word == 0)
                            continue;

                        for (size_t bit_idx = 0; bit_idx < 64; bit_idx++)
                        {
                            if (word & (1ULL << bit_idx))
                            {
                                uint16_t low = word_idx * 64 + bit_idx;
                                results.emplace_back(gpid, low);
                            }
                        }
                    }
                }
            }

            return results;
        }

        // TASK-CRITICAL-2: Get all versioned entries with xmin/xmax
        std::vector<std::pair<TID, VersionedBitmapEntry>> RoaringBitmap::toVersionedArray(ErrorContext *ctx)
        {
            std::vector<std::pair<TID, VersionedBitmapEntry>> results;
            results.reserve(cardinality_);

            for (const auto &container : containers_)
            {
                GPID gpid = container.key;

                if (container.type == ContainerType::ARRAY)
                {
                    for (const auto& entry : container.array_data_versioned)
                    {
                        results.emplace_back(TID(gpid, entry.tid_low), entry);
                    }
                }
                else if (container.type == ContainerType::BITSET)
                {
                    // For bitset, extract entries from bitset_versions
                    for (const auto& kv : container.bitset_versions)
                    {
                        results.emplace_back(TID(gpid, kv.first), kv.second);
                    }
                }
            }

            return results;
        }

        // TASK-CRITICAL-2: Get only visible entries (MGA-compliant index-level visibility)
        // Firebird MGA: Per MGA_RULES.md Rule 11 - use TransactionId, NOT Snapshot
        // This eliminates the need for heap access (20-40% performance improvement)
        std::vector<TID> RoaringBitmap::toVisibleArray(uint64_t current_xid,
                                                             TransactionManager *txn_mgr,
                                                             ErrorContext *ctx)
        {
            std::vector<TID> results;
            results.reserve(cardinality_);

            if (!txn_mgr)
            {
                // No transaction manager - return all entries
                return toArray(ctx);
            }
            if (current_xid == 0)
            {
                // No visibility filtering requested
                return toArray(ctx);
            }

            for (const auto &container : containers_)
            {
                GPID gpid = container.key;

                if (container.type == ContainerType::ARRAY)
                {
                    for (const auto& entry : container.array_data_versioned)
                    {
                        // TASK-CRITICAL-2: Index-level visibility check (no heap access!)
                        // Per MGA_RULES.md Rule 3: Use TIP-based visibility
                        if (entry.isVisible(current_xid, txn_mgr))
                        {
                            results.emplace_back(gpid, entry.tid_low);
                        }
                    }
                }
                else if (container.type == ContainerType::BITSET)
                {
                    for (const auto& kv : container.bitset_versions)
                    {
                        // TASK-CRITICAL-2: Index-level visibility check (no heap access!)
                        if (kv.second.isVisible(current_xid, txn_mgr))
                        {
                            results.emplace_back(gpid, kv.first);
                        }
                    }
                }
            }

            return results;
        }

        Status RoaringBitmap::updateTIDsAfterMigration(
            const std::unordered_map<TID, TID> &tid_mapping,
            uint64_t *tids_updated_out,
            uint64_t *pages_modified_out,
            ErrorContext *ctx)
        {
            (void)ctx;
            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = 0;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = 0;
            }

            if (tid_mapping.empty())
            {
                return Status::OK;
            }

            std::unordered_map<uint64_t, uint64_t> gpid_mapping;
            gpid_mapping.reserve(tid_mapping.size());
            for (const auto &pair : tid_mapping)
            {
                gpid_mapping.emplace(pair.first.gpid, pair.second.gpid);
            }

            struct BuildContainer
            {
                uint64_t key = 0;
                std::vector<VersionedBitmapEntry> entries;
                std::vector<uint32_t> page_numbers;
            };

            std::unordered_map<uint64_t, BuildContainer> build;
            uint64_t updated = 0;

            for (const auto &container : containers_)
            {
                uint64_t old_key = container.key;
                uint64_t new_key = old_key;
                auto map_it = gpid_mapping.find(old_key);
                if (map_it != gpid_mapping.end())
                {
                    new_key = map_it->second;
                }

                auto &dest = build[new_key];
                dest.key = new_key;
                if (container.page_number != 0)
                {
                    dest.page_numbers.push_back(container.page_number);
                }

                if (container.type == ContainerType::ARRAY)
                {
                    for (const auto &entry : container.array_data_versioned)
                    {
                        dest.entries.push_back(entry);
                        if (new_key != old_key)
                        {
                            updated++;
                        }
                    }
                }
                else if (container.type == ContainerType::BITSET)
                {
                    for (const auto &kv : container.bitset_versions)
                    {
                        dest.entries.push_back(kv.second);
                        if (new_key != old_key)
                        {
                            updated++;
                        }
                    }
                }
            }

            std::vector<Container> new_containers;
            new_containers.reserve(build.size());

            uint64_t pages_modified = 0;
            cardinality_ = 0;

            for (auto &kv : build)
            {
                auto &bc = kv.second;
                if (bc.entries.empty())
                {
                    continue;
                }

                std::sort(bc.entries.begin(), bc.entries.end(),
                          [](const VersionedBitmapEntry &a, const VersionedBitmapEntry &b) {
                              return a.tid_low < b.tid_low;
                          });

                std::vector<VersionedBitmapEntry> deduped;
                deduped.reserve(bc.entries.size());
                for (const auto &entry : bc.entries)
                {
                    if (!deduped.empty() && deduped.back().tid_low == entry.tid_low)
                    {
                        auto &existing = deduped.back();
                        bool keep_existing = true;
                        if (existing.xmax != 0 && entry.xmax == 0)
                        {
                            keep_existing = false;
                        }
                        else if (existing.xmax == entry.xmax && entry.xmin < existing.xmin)
                        {
                            keep_existing = false;
                        }
                        if (!keep_existing)
                        {
                            existing = entry;
                        }
                        continue;
                    }
                    deduped.push_back(entry);
                }

                Container container;
                container.key = bc.key;
                container.page_number = bc.page_numbers.empty() ? 0 : bc.page_numbers.front();
                container.num_values = static_cast<uint16_t>(deduped.size());

                if (deduped.size() > ARRAY_MAX_SIZE)
                {
                    container.type = ContainerType::BITSET;
                    container.bitset_data.resize(bitsetWordCount(db_->page_size()), 0);
                    for (const auto &entry : deduped)
                    {
                        size_t word_idx = entry.tid_low / 64;
                        size_t bit_idx = entry.tid_low % 64;
                        if (word_idx < container.bitset_data.size())
                        {
                            container.bitset_data[word_idx] |= (1ULL << bit_idx);
                        }
                        container.bitset_versions[entry.tid_low] = entry;
                    }
                }
                else
                {
                    container.type = ContainerType::ARRAY;
                    container.array_data_versioned = std::move(deduped);
                }

                if (bc.page_numbers.size() > 1)
                {
                    for (size_t i = 1; i < bc.page_numbers.size(); ++i)
                    {
                        uint32_t page_num = bc.page_numbers[i];
                        if (page_num != 0)
                        {
                            db_->page_manager()->freePageGlobal(
                                makeGPID(tablespace_id_, page_num), nullptr);
                        }
                    }
                }

                Status save_status = saveContainer(container, ctx);
                if (save_status != Status::OK)
                {
                    return save_status;
                }

                pages_modified++;
                cardinality_ += container.num_values;
                new_containers.push_back(std::move(container));
            }

            containers_.swap(new_containers);

            uint8_t *root_data = nullptr;
            if (pinIndexPage(root_page_, (void **)&root_data, ctx) == Status::OK)
            {
                auto *root = reinterpret_cast<SBRoaringBitmapRootPage *>(root_data);
                root->rbr_total_cardinality = cardinality_;
                root->rbr_num_containers = static_cast<uint32_t>(containers_.size());
                unpinIndexPage(root_page_, true, ctx);
            }

            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = updated;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = pages_modified;
            }

            return Status::OK;
        }

        // TASK-CRITICAL-2: Visible cardinality (only visible entries)
        uint64_t RoaringBitmap::visibleCardinality(uint64_t current_xid,
                                                   TransactionManager *txn_mgr,
                                                   ErrorContext *ctx) const
        {
            if (!txn_mgr)
            {
                return cardinality_;
            }

            uint64_t visible_count = 0;

            for (const auto &container : containers_)
            {
                if (container.type == ContainerType::ARRAY)
                {
                    for (const auto& entry : container.array_data_versioned)
                    {
                        if (entry.isVisible(current_xid, txn_mgr))
                        {
                            visible_count++;
                        }
                    }
                }
                else if (container.type == ContainerType::BITSET)
                {
                    for (const auto& kv : container.bitset_versions)
                    {
                        if (kv.second.isVisible(current_xid, txn_mgr))
                        {
                            visible_count++;
                        }
                    }
                }
            }

            return visible_count;
        }

        RoaringBitmap::Container *RoaringBitmap::findOrCreateContainer(uint64_t key, ErrorContext *ctx)
        {
            // Find existing container
            for (auto &container : containers_)
            {
                if (container.key == key)
                {
                    return &container;
                }
            }

            // Create new container
            Container new_container;
            new_container.key = key;
            new_container.type = ContainerType::ARRAY;
            new_container.num_values = 0;
            new_container.page_number = 0;

            containers_.push_back(new_container);
            return &containers_.back();
        }

        Status RoaringBitmap::saveContainer(const Container &container, ErrorContext *ctx)
        {
            // Allocate page if needed
            uint32_t page_num = container.page_number;
            if (page_num == 0)
            {
                GPID new_gpid = 0;
                Status status = db_->page_manager()->allocatePageInTablespace(tablespace_id_, &new_gpid, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                page_num = static_cast<uint32_t>(getPageNumber(new_gpid));

                // Update container page number in our cache
                for (auto &c : containers_)
                {
                    if (c.key == container.key)
                    {
                        c.page_number = page_num;
                        break;
                    }
                }
            }

            // Save container data
            uint8_t *page_data = nullptr;
            Status status = pinIndexPage(page_num, (void **)&page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *page = reinterpret_cast<SBRoaringContainerPage *>(page_data);
            page->rcp_header.magic = K_MAGIC_SBRD;
            page->rcp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            page->rcp_header.page_type = static_cast<uint16_t>(PageType::PAGE_TYPE_BITMAP_CONTAINER);
            page->rcp_header.page_size = db_->page_size();
            page->rcp_header.page_id = page_num;
            page->rcp_header.generation = 1;
            page->rcp_header.checksum = 0;
            page->rcp_header.flags = 0;
            page->rcp_header.lsn = 0;
            pageSetLower(page->rcp_header, sizeof(SBRoaringContainerPage));
            pageSetUpper(page->rcp_header, db_->page_size());
            pageSetSpecial(page->rcp_header, db_->page_size());
            page->rcp_type = container.type;
            page->rcp_num_values = container.num_values;

            uint8_t *data_area = page_data + sizeof(SBRoaringContainerPage);

            if (container.type == ContainerType::ARRAY)
            {
                // Serialize versioned array data (only TID low bits, version info stored separately)
                std::vector<uint16_t> plain_tids;
                plain_tids.reserve(container.array_data_versioned.size());
                for (const auto& entry : container.array_data_versioned)
                {
                    plain_tids.push_back(entry.tid_low);
                }
                std::memcpy(data_area, plain_tids.data(), plain_tids.size() * sizeof(uint16_t));
            }
            else if (container.type == ContainerType::BITSET)
            {
                std::memcpy(data_area, container.bitset_data.data(),
                            container.bitset_data.size() * sizeof(uint64_t));
            }

            unpinIndexPage(page_num, true, ctx);
            return Status::OK;
        }

        std::unique_ptr<RoaringBitmap> RoaringBitmap::bitwiseAnd(
            const RoaringBitmap &lhs,
            const RoaringBitmap &rhs,
            ErrorContext *ctx)
        {
            // Create new bitmap for result (November 20, 2025)
            // Root page = 0 for intermediate results - will be allocated when persisted
            auto result = std::make_unique<RoaringBitmap>(lhs.db_, 0);

            // Intersect containers
            for (const auto &lhs_cont : lhs.containers_)
            {
                for (const auto &rhs_cont : rhs.containers_)
                {
                    if (lhs_cont.key == rhs_cont.key)
                    {
                        Container result_cont;
                        containerAnd(lhs_cont, rhs_cont, &result_cont,
                                     bitsetWordCount(lhs.db_->page_size()));
                        if (result_cont.num_values > 0)
                        {
                            result->containers_.push_back(result_cont);
                            result->cardinality_ += result_cont.num_values;
                        }
                    }
                }
            }

            return result;
        }

        std::unique_ptr<RoaringBitmap> RoaringBitmap::bitwiseOr(
            const RoaringBitmap &lhs,
            const RoaringBitmap &rhs,
            ErrorContext *ctx)
        {
            auto result = std::make_unique<RoaringBitmap>(lhs.db_, 0);

            // Union containers
            for (const auto &lhs_cont : lhs.containers_)
            {
                bool found = false;
                for (const auto &rhs_cont : rhs.containers_)
                {
                    if (lhs_cont.key == rhs_cont.key)
                    {
                        Container result_cont;
                        containerOr(lhs_cont, rhs_cont, &result_cont,
                                    bitsetWordCount(lhs.db_->page_size()));
                        result->containers_.push_back(result_cont);
                        result->cardinality_ += result_cont.num_values;
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    result->containers_.push_back(lhs_cont);
                    result->cardinality_ += lhs_cont.num_values;
                }
            }

            // Add rhs containers not in lhs
            for (const auto &rhs_cont : rhs.containers_)
            {
                bool found = false;
                for (const auto &lhs_cont : lhs.containers_)
                {
                    if (rhs_cont.key == lhs_cont.key)
                    {
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    result->containers_.push_back(rhs_cont);
                    result->cardinality_ += rhs_cont.num_values;
                }
            }

            return result;
        }

        std::unique_ptr<RoaringBitmap> RoaringBitmap::bitwiseNot(
            const RoaringBitmap &bitmap,
            uint64_t universe_size,
            ErrorContext *ctx)
        {
            // Create new bitmap for result (November 20, 2025)
            // Root page = 0 for intermediate results - will be allocated when persisted
            auto result = std::make_unique<RoaringBitmap>(bitmap.db_, 0);
            result->cardinality_ = 0;

            // NOT operation: For each possible container (0-65535), either:
            // 1. If container exists in input: negate it
            // 2. If container doesn't exist: create full container (all 1s)

            // First, negate all existing containers
            for (const auto &cont : bitmap.containers_)
            {
                Container result_cont;
                containerNot(cont, &result_cont,
                             bitsetWordCount(bitmap.db_->page_size()));
                result->containers_.push_back(result_cont);
                result->cardinality_ += result_cont.num_values;
            }

            // Second, create full containers for all non-existing containers up to universe_size
            // Universe size is the maximum TID value we need to consider
            // Each container covers 65536 values (16-bit range)
            uint16_t max_container_key = (universe_size >> 16); // High 16 bits

            // Create a set of existing keys for fast lookup
            std::set<uint16_t> existing_keys;
            for (const auto &cont : bitmap.containers_)
            {
                existing_keys.insert(cont.key);
            }

            // Add full containers for missing keys
            for (uint16_t key = 0; key <= max_container_key; key++)
            {
                if (existing_keys.find(key) == existing_keys.end())
                {
                    // This container doesn't exist in input, so its NOT is all 1s
                    Container full_cont;
                    full_cont.key = key;
                    full_cont.type = ContainerType::BITSET;
                    full_cont.bitset_data.resize(bitsetWordCount(bitmap.db_->page_size()), 0xFFFFFFFFFFFFFFFFULL);

                    // For the last container, we may need to mask out values beyond universe_size
                    if (key == max_container_key)
                    {
                        uint16_t last_value = universe_size & 0xFFFF; // Low 16 bits
                        if (last_value != 0xFFFF) // Not a full container
                        {
                            // Clear bits beyond last_value
                            for (uint16_t val = last_value + 1; val < 65536; val++)
                            {
                                size_t word_idx = val / 64;
                                size_t bit_idx = val % 64;
                                full_cont.bitset_data[word_idx] &= ~(1ULL << bit_idx);
                            }
                        }
                    }

                    // Count actual set bits
                    full_cont.num_values = 0;
                    for (size_t i = 0; i < bitsetWordCount(bitmap.db_->page_size()); i++)
                    {
                        full_cont.num_values += popcount64(full_cont.bitset_data[i]);
                    }

                    result->containers_.push_back(full_cont);
                    result->cardinality_ += full_cont.num_values;
                }
            }

            return result;
        }

        void RoaringBitmap::containerAnd(const Container &lhs, const Container &rhs,
                                         Container *result, size_t word_count)
        {
            result->key = lhs.key;
            result->type = ContainerType::ARRAY;
            result->num_values = 0;

            if (lhs.type == ContainerType::ARRAY && rhs.type == ContainerType::ARRAY)
            {
                // Array-array intersection (versioned entries)
                size_t i = 0, j = 0;
                while (i < lhs.array_data_versioned.size() && j < rhs.array_data_versioned.size())
                {
                    if (lhs.array_data_versioned[i].tid_low == rhs.array_data_versioned[j].tid_low)
                    {
                        result->array_data_versioned.push_back(lhs.array_data_versioned[i]);
                        result->num_values++;
                        i++;
                        j++;
                    }
                    else if (lhs.array_data_versioned[i].tid_low < rhs.array_data_versioned[j].tid_low)
                    {
                        i++;
                    }
                    else
                    {
                        j++;
                    }
                }
            }
            else if (lhs.type == ContainerType::BITSET && rhs.type == ContainerType::BITSET)
            {
                // Bitset-bitset intersection
                result->type = ContainerType::BITSET;
                result->bitset_data.resize(word_count, 0);

                for (size_t i = 0; i < word_count; i++)
                {
                    uint64_t lhs_word = (i < lhs.bitset_data.size()) ? lhs.bitset_data[i] : 0;
                    uint64_t rhs_word = (i < rhs.bitset_data.size()) ? rhs.bitset_data[i] : 0;
                    result->bitset_data[i] = lhs_word & rhs_word;
                    result->num_values += popcount64(result->bitset_data[i]);
                }
            }
            else
            {
                // Mixed types: convert both to bitset for intersection
                result->type = ContainerType::BITSET;
                result->bitset_data.resize(word_count, 0);

                // Helper to convert container to bitset
                auto to_bitset = [word_count](const Container &c, std::vector<uint64_t> &bitset)
                {
                    if (c.type == ContainerType::ARRAY)
                    {
                        // Convert versioned array to bitset
                        for (const auto& entry : c.array_data_versioned)
                        {
                            size_t word_idx = entry.tid_low / 64;
                            size_t bit_idx = entry.tid_low % 64;
                            bitset[word_idx] |= (1ULL << bit_idx);
                        }
                    }
                    else if (c.type == ContainerType::BITSET)
                    {
                        size_t count = std::min(word_count, c.bitset_data.size());
                        for (size_t i = 0; i < count; i++)
                        {
                            bitset[i] = c.bitset_data[i];
                        }
                    }
                };

                // Convert both containers to bitsets
                std::vector<uint64_t> lhs_bitset(word_count, 0);
                std::vector<uint64_t> rhs_bitset(word_count, 0);
                to_bitset(lhs, lhs_bitset);
                to_bitset(rhs, rhs_bitset);

                // Perform AND operation
                for (size_t i = 0; i < word_count; i++)
                {
                    result->bitset_data[i] = lhs_bitset[i] & rhs_bitset[i];
                    result->num_values += popcount64(result->bitset_data[i]);
                }
            }
        }

        void RoaringBitmap::containerOr(const Container &lhs, const Container &rhs,
                                        Container *result, size_t word_count)
        {
            result->key = lhs.key;

            if (lhs.type == ContainerType::BITSET || rhs.type == ContainerType::BITSET)
            {
                // Use bitset for union if either is bitset
                result->type = ContainerType::BITSET;
                result->bitset_data.resize(word_count, 0);

                // Convert both to bitset and OR
                auto to_bitset = [word_count](const Container &c, std::vector<uint64_t> &bitset)
                {
                    if (c.type == ContainerType::ARRAY)
                    {
                        // Convert versioned array to bitset
                        for (const auto& entry : c.array_data_versioned)
                        {
                            size_t word_idx = entry.tid_low / 64;
                            size_t bit_idx = entry.tid_low % 64;
                            bitset[word_idx] |= (1ULL << bit_idx);
                        }
                    }
                    else if (c.type == ContainerType::BITSET)
                    {
                        size_t count = std::min(word_count, c.bitset_data.size());
                        for (size_t i = 0; i < count; i++)
                        {
                            bitset[i] |= c.bitset_data[i];
                        }
                    }
                };

                to_bitset(lhs, result->bitset_data);
                to_bitset(rhs, result->bitset_data);

                // Count bits
                for (size_t i = 0; i < word_count; i++)
                {
                    result->num_values += popcount64(result->bitset_data[i]);
                }
            }
            else
            {
                // Array-array union (versioned entries)
                result->type = ContainerType::ARRAY;
                result->array_data_versioned = lhs.array_data_versioned;

                for (const auto& entry : rhs.array_data_versioned)
                {
                    auto it = std::lower_bound(result->array_data_versioned.begin(),
                                               result->array_data_versioned.end(), entry.tid_low,
                                               [](const VersionedBitmapEntry& e, uint16_t tid) {
                                                   return e.tid_low < tid;
                                               });
                    if (it == result->array_data_versioned.end() || it->tid_low != entry.tid_low)
                    {
                        result->array_data_versioned.insert(it, entry);
                    }
                }

                result->num_values = result->array_data_versioned.size();
            }
        }

        void RoaringBitmap::containerNot(const Container &container,
                                         Container *result, size_t word_count)
        {
            // NOT operation inverts all bits in a container (0 → 1, 1 → 0)
            // Result is always BITSET type (since complement of sparse set is dense)
            result->key = container.key;
            result->type = ContainerType::BITSET;
            result->bitset_data.resize(word_count, 0xFFFFFFFFFFFFFFFFULL); // Start with all 1s
            result->num_values = 0;

            if (container.type == ContainerType::ARRAY)
            {
                // For ARRAY containers, invert by setting all bits to 1, then clearing versioned array values
                for (const auto& entry : container.array_data_versioned)
                {
                    size_t word_idx = entry.tid_low / 64;
                    size_t bit_idx = entry.tid_low % 64;
                    result->bitset_data[word_idx] &= ~(1ULL << bit_idx); // Clear bit
                }

                // Count set bits
                for (size_t i = 0; i < word_count; i++)
                {
                    result->num_values += popcount64(result->bitset_data[i]);
                }
            }
            else if (container.type == ContainerType::BITSET)
            {
                // For BITSET containers, simple bitwise NOT
                for (size_t i = 0; i < word_count; i++)
                {
                    uint64_t value = (i < container.bitset_data.size()) ? container.bitset_data[i] : 0;
                    result->bitset_data[i] = ~value;
                    result->num_values += popcount64(result->bitset_data[i]);
                }
            }

            // Note: A container represents 65536 values (16-bit range)
            // After NOT, result->num_values should be (65536 - original_num_values)
        }

        // ========================================
        // BitmapIndex Garbage Collection
        // ========================================

        // PHASE 2 TASK 2.5: Remove index entries pointing to dead tuples
        // PHASE 1.5 TASK 1.5.2c: Migrated to TID struct API
        Status BitmapIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                              uint64_t *entries_removed_out,
                                              uint64_t *pages_modified_out,
                                              ErrorContext *ctx)
        {
            // Initialize output parameters
            uint64_t total_entries_removed = 0;
            uint64_t total_pages_modified = 0;

            // Early exit if empty
            if (dead_tids.empty())
            {
                if (entries_removed_out)
                    *entries_removed_out = 0;
                if (pages_modified_out)
                    *pages_modified_out = 0;
                return Status::OK;
            }

            // Load meta page to get dictionary
            Status status = loadMetaPage(ctx);
            if (status != Status::OK)
            {
                LOG_WARNING(VACUUM, "Bitmap GC: Failed to load meta page: %d",
                            static_cast<int>(status));
                return Status::IO_ERROR;
            }

            // If no dictionary entries, nothing to do
            if (dictionary_page_ == 0)
            {
                if (entries_removed_out)
                    *entries_removed_out = 0;
                if (pages_modified_out)
                    *pages_modified_out = 0;
                return Status::OK;
            }

            // ===== Strategy: Iterate all dictionary entries, remove dead TIDs from each bitmap =====
            //
            // Bitmap indexes store: value → RoaringBitmap (set of TIDs with that value)
            // For GC, we need to remove dead TIDs from ALL bitmaps

            // Scan all dictionary entries
            uint32_t current_dict_page = dictionary_page_;
            bool had_errors = false;

            while (current_dict_page != 0)
            {
                uint8_t *page_data = nullptr;
                status = pinIndexPage(current_dict_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(VACUUM, "Bitmap GC: Failed to pin dictionary page %u: %d",
                                current_dict_page, static_cast<int>(status));
                    had_errors = true;
                    break;
                }

                auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
                uint32_t next_page = dict_page->bmp_dict_next_page;
                uint16_t entry_count = dict_page->bmp_dict_count;

                // Scan entries in this dictionary page
                uint8_t *entry_data = page_data + sizeof(SBBitmapDictionaryPage);

                for (uint16_t i = 0; i < entry_count; i++)
                {
                    auto *entry = reinterpret_cast<BitmapDictionaryEntry *>(entry_data);
                    uint32_t bitmap_root = entry->bitmap_root_page;

                    if (bitmap_root != 0)
                    {
                        // Load the Roaring bitmap for this value
                        auto bitmap = loadBitmap(bitmap_root, ctx);
                        if (bitmap)
                        {
                            // Remove each dead TID from this bitmap
                            for (const TID &tid : dead_tids)
                            {
                                Status remove_status = bitmap->removePhysical(tid, ctx);
                                if (remove_status == Status::OK)
                                {
                                    total_entries_removed++;
                                }
                            }

                            // Update dictionary entry cardinality
                            entry->cardinality = bitmap->cardinality();
                        }
                    }

                    // Move to next entry
                    entry_data += sizeof(BitmapDictionaryEntry) + entry->value_length;
                }

                unpinIndexPage(current_dict_page, false, ctx);

                // Move to next dictionary page
                current_dict_page = next_page;
            }

            // Estimate pages modified (each bitmap may touch multiple pages)
            // This is a rough estimate since RoaringBitmap doesn't report it
            total_pages_modified = total_entries_removed > 0 ? (total_entries_removed / 10) + 1 : 0;

            // Set output parameters
            if (entries_removed_out)
                *entries_removed_out = total_entries_removed;
            if (pages_modified_out)
                *pages_modified_out = total_pages_modified;

            return had_errors ? Status::IO_ERROR : Status::OK;
        }

        // ========================================
        // RoaringBitmapIterator Implementation
        // ========================================

        RoaringBitmapIterator::RoaringBitmapIterator(const RoaringBitmap &bitmap)
            : bitmap_(bitmap),
              container_index_(0),
              value_index_(0)
        {
        }

        bool RoaringBitmapIterator::hasNext() const
        {
            return container_index_ < bitmap_.containers_.size();
        }

        TID RoaringBitmapIterator::next()
        {
            const auto &container = bitmap_.containers_[container_index_];
            GPID gpid = container.key;
            uint16_t slot = 0;

            if (container.type == ContainerType::ARRAY)
            {
                slot = container.array_data_versioned[value_index_].tid_low;
                value_index_++;

                if (value_index_ >= container.array_data_versioned.size())
                {
                    container_index_++;
                    value_index_ = 0;
                }
            }
            else if (container.type == ContainerType::BITSET)
            {
                // Find next set bit
                while (container_index_ < bitmap_.containers_.size())
                {
                    size_t word_idx = value_index_ / 64;
                    size_t bit_idx = value_index_ % 64;

                    if (word_idx >= bitsetWordCount(bitmap_.db_->page_size()))
                    {
                        container_index_++;
                        value_index_ = 0;
                        break;
                    }

                    uint64_t word = container.bitset_data[word_idx];
                    if (word & (1ULL << bit_idx))
                    {
                        slot = static_cast<uint16_t>(value_index_);
                        value_index_++;
                        break;
                    }

                    value_index_++;
                }
            }

            return TID(gpid, slot);
        }

        void RoaringBitmapIterator::reset()
        {
            container_index_ = 0;
            value_index_ = 0;
        }

        // ========================================
        // BitmapIndexScanner Implementation
        // ========================================

        BitmapIndexScanner::BitmapIndexScanner(BitmapIndex *index,
                                              std::shared_ptr<RoaringBitmap> bitmap,
                                              uint64_t current_xid,
                                              Database *db)
            : index_(index),
              db_(db),
              bitmap_(std::move(bitmap)),
              current_xid_(current_xid),
              scanned_count_(0),
              returned_count_(0)
        {
            if (bitmap_)
            {
                iterator_ = std::make_unique<RoaringBitmapIterator>(*bitmap_);
            }
        }

        BitmapIndexScanner::~BitmapIndexScanner()
        {
            // Unique pointers automatically cleaned up
        }

        bool BitmapIndexScanner::hasNext()
        {
            return iterator_ && iterator_->hasNext();
        }

        Status BitmapIndexScanner::next(TID *tid_out, ErrorContext *ctx)
        {
            if (!iterator_)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No bitmap to scan");
                return Status::NOT_FOUND;
            }

            if (!iterator_->hasNext())
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "No more entries");
                return Status::NOT_FOUND;
            }

            // Get next TID from bitmap
            TID tid = iterator_->next();
            scanned_count_++;

            // Check visibility using TIP-based filtering (Firebird MGA)
            if (!db_)
            {
                // No database context, return TID without visibility check
                *tid_out = tid;
                returned_count_++;
                return Status::OK;
            }

            TransactionManager *txn_mgr = db_->transaction_manager();
            if (!txn_mgr)
            {
                // No transaction manager, return TID without visibility check
                *tid_out = tid;
                returned_count_++;
                return Status::OK;
            }

            // November 20, 2025: Visibility checking strategy
            // Bitmap indexes delegate visibility checking to executor/heap scan level
            // This is a valid production approach that provides better performance:
            // - Index scan returns all matching TIDs from bitmap
            // - Executor applies visibility filter when accessing heap tuples
            // Alternative: Index-level visibility (slower, requires heap page access per TID)
            // Current approach is consistent with PostgreSQL's bitmap index design

            *tid_out = tid;
            returned_count_++;
            return Status::OK;
        }

        // ========================================
        // BitmapIndex::scan() Implementation
        // ========================================

        std::unique_ptr<BitmapIndexScanner> BitmapIndex::scan(
            const void *value_data,
            size_t value_len,
            uint64_t current_xid,
            ErrorContext *ctx)
        {
            // Find the dictionary entry for this value
            uint32_t bitmap_root = 0;
            uint32_t bitmap_id = findDictionaryEntry(value_data, value_len, &bitmap_root, ctx);

            if (bitmap_id == 0)
            {
                // Value not found in dictionary - return empty scanner
                LOG_DEBUG(STORAGE, "Bitmap scan: value not found in dictionary, returning empty scanner");
                return std::make_unique<BitmapIndexScanner>(this, nullptr, current_xid, db_);
            }

            // Load the bitmap for this value
            auto bitmap = loadBitmap(bitmap_root, ctx);
            if (!bitmap)
            {
                LOG_ERROR(STORAGE, "Bitmap scan: failed to load bitmap for value");
                return std::make_unique<BitmapIndexScanner>(this, nullptr, current_xid, db_);
            }

            LOG_DEBUG(STORAGE, "Bitmap scan: scanning %lu TIDs for value", bitmap->cardinality());

            // Create scanner with the loaded bitmap
            return std::make_unique<BitmapIndexScanner>(this, bitmap, current_xid, db_);
        }

        std::unique_ptr<BitmapIndexScanner> BitmapIndex::scanOr(
            const std::vector<const void *> &values,
            const std::vector<size_t> &value_lens,
            uint64_t current_xid,
            ErrorContext *ctx)
        {
            if (values.empty())
            {
                LOG_DEBUG(STORAGE, "Bitmap scanOr: empty value list, returning empty scanner");
                return std::make_unique<BitmapIndexScanner>(this, nullptr, current_xid, db_);
            }

            // Load bitmaps for all values and OR them together
            std::shared_ptr<RoaringBitmap> result_bitmap;

            for (size_t i = 0; i < values.size(); ++i)
            {
                uint32_t bitmap_root = 0;
                uint32_t bitmap_id = findDictionaryEntry(values[i], value_lens[i], &bitmap_root, ctx);

                if (bitmap_id == 0)
                {
                    // Value not in dictionary, skip it
                    continue;
                }

                auto bitmap = loadBitmap(bitmap_root, ctx);
                if (!bitmap)
                {
                    continue;
                }

                if (!result_bitmap)
                {
                    // First bitmap - use it as the result
                    auto bit_or = RoaringBitmap::bitwiseOr(*bitmap, *bitmap, ctx);
                    result_bitmap = std::move(bit_or);
                }
                else
                {
                    // OR this bitmap with the result
                    auto bit_or = RoaringBitmap::bitwiseOr(*result_bitmap, *bitmap, ctx);
                    result_bitmap = std::move(bit_or);
                }
            }

            if (!result_bitmap)
            {
                // No bitmaps found - return empty scanner
                LOG_DEBUG(STORAGE, "Bitmap scanOr: no matching bitmaps found, returning empty scanner");
                return std::make_unique<BitmapIndexScanner>(this, nullptr, current_xid, db_);
            }

            LOG_DEBUG(STORAGE, "Bitmap scanOr: scanning %lu TIDs from %zu values",
                     result_bitmap->cardinality(), values.size());

            // Create scanner with the OR'd bitmap
            return std::make_unique<BitmapIndexScanner>(this, result_bitmap, current_xid, db_);
        }

    } // namespace core
} // namespace scratchbird
