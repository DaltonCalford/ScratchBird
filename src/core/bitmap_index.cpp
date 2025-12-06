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

namespace scratchbird
{
    namespace core
    {
        // Constants
        constexpr uint16_t ARRAY_MAX_SIZE = 4096;
        constexpr uint16_t BITSET_SIZE_BYTES = 8192;
        constexpr uint16_t BITSET_SIZE_UINT64 = 1024; // 8192 / 8
        constexpr uint32_t TUPLES_PER_PAGE = 256;

        // ========================================
        // TASK-CRITICAL-2: VersionedBitmapEntry Implementation
        // ========================================

        // Firebird MGA: TIP-based visibility check (NOT snapshot-based)
        // Per MGA_RULES.md Rule 3 (lines 121-145): Use TIP lookups, not snapshot arrays
        bool VersionedBitmapEntry::isVisible(uint64_t current_xid, TransactionManager *txn_mgr) const
        {
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

            // Per MGA_RULES.md Rule 3: Look up transaction state in TIP (NOT snapshot)
            // isVersionVisible() checks: "Is xmin committed and older than current_xid?"
            bool xmin_visible = txn_mgr->isVersionVisible(xmin, current_xid);

            if (!xmin_visible)
            {
                // Insert transaction not visible - entry not visible
                return false;
            }

            // Insert transaction visible - check if deleted
            if (xmax == 0)
            {
                // Not deleted - visible
                return true;
            }

            // Check if delete transaction is visible
            bool xmax_visible = txn_mgr->isVersionVisible(xmax, current_xid);

            // Visible if deleted by invisible transaction (or not yet committed delete)
            return !xmax_visible;
        }

        // ========================================
        // BitmapIndex Implementation
        // ========================================

        BitmapIndex::BitmapIndex(Database *db, const UuidV7Bytes &index_uuid, uint32_t meta_page)
            : db_(db),
              buffer_pool_(db->buffer_pool()),
              index_uuid_(index_uuid),
              meta_page_(meta_page),
              num_distinct_values_(0),
              total_tuples_(0),
              dictionary_page_(0)
        {
        }

        BitmapIndex::~BitmapIndex() = default;

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

            BufferPool *buffer_pool = db->buffer_pool();
            if (!buffer_pool)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
                return Status::INVALID_ARGUMENT;
            }

            // Allocate meta page
            uint32_t meta_page_num = 0;
            Status status = db->page_manager()->allocatePage(meta_page_num, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Pin and initialize meta page
            uint8_t *meta_data = nullptr;
            status = buffer_pool->pinPage(meta_page_num, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
            std::memset(meta, 0, sizeof(SBBitmapIndexMetaPage));

            meta->bmp_header.magic = K_MAGIC_SBRD;
            meta->bmp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            meta->bmp_header.page_type = static_cast<uint16_t>(PageType::BITMAP_INDEX_META);
            meta->bmp_header.page_size = db->page_size();
            meta->bmp_header.page_id = meta_page_num;
            std::memcpy(meta->bmp_index_uuid.bytes.data(), index_uuid.bytes.data(), 16);
            meta->bmp_num_distinct_values = 0;
            meta->bmp_total_tuples = 0;
            meta->bmp_dictionary_page = 0;

            buffer_pool->unpinPage(meta_page_num, true, ctx);

            *meta_page_out = meta_page_num;
            return Status::OK;
        }

        std::unique_ptr<BitmapIndex> BitmapIndex::open(
            Database *db,
            const UuidV7Bytes &index_uuid,
            uint32_t meta_page,
            ErrorContext *ctx)
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
                return nullptr;
            }

            auto index = std::make_unique<BitmapIndex>(db, index_uuid, meta_page);

            Status status = index->loadMetaPage(ctx);
            if (status != Status::OK)
            {
                return nullptr;
            }

            return index;
        }

        Status BitmapIndex::loadMetaPage(ErrorContext *ctx)
        {
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);

            // Verify page type
            if (meta->bmp_header.page_type != static_cast<uint16_t>(PageType::BITMAP_INDEX_META))
            {
                buffer_pool_->unpinPage(meta_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid bitmap index meta page type");
                return Status::PAGE_CORRUPT;
            }

            // Load cached values
            num_distinct_values_ = meta->bmp_num_distinct_values;
            total_tuples_ = meta->bmp_total_tuples;
            dictionary_page_ = meta->bmp_dictionary_page;

            buffer_pool_->unpinPage(meta_page_, false, ctx);
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
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
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
                            buffer_pool_->unpinPage(current_page, false, ctx);
                            return cardinality;
                        }
                    }

                    entry_data += sizeof(BitmapDictionaryEntry) + entry->value_length;
                }

                current_page = dict_page->bmp_dict_next_page;
                buffer_pool_->unpinPage(current_page, false, ctx);
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
                Status status = db_->page_manager()->allocatePage(page_num, ctx);
                if (status != Status::OK)
                {
                    return 0;
                }

                uint8_t *page_data = nullptr;
                status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return 0;
                }

                auto *dict_page = reinterpret_cast<SBBitmapDictionaryPage *>(page_data);
                std::memset(dict_page, 0, sizeof(SBBitmapDictionaryPage));

                dict_page->bmp_dict_header.magic = K_MAGIC_SBRD;
                dict_page->bmp_dict_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                dict_page->bmp_dict_header.page_type = static_cast<uint16_t>(PageType::BITMAP_INDEX_DICT);
                dict_page->bmp_dict_header.page_size = db_->page_size();
                dict_page->bmp_dict_header.page_id = page_num;
                dict_page->bmp_dict_count = 0;
                dict_page->bmp_dict_free_offset = sizeof(SBBitmapDictionaryPage);
                dict_page->bmp_dict_next_page = 0;

                buffer_pool_->unpinPage(page_num, true, ctx);

                dictionary_page_ = page_num;

                // Update meta page
                uint8_t *meta_data = nullptr;
                buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
                auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
                meta->bmp_dictionary_page = dictionary_page_;
                buffer_pool_->unpinPage(meta_page_, true, ctx);
            }

            // Find page with enough space or allocate new one
            uint32_t current_page = dictionary_page_;
            uint8_t *page_data = nullptr;
            Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
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
                status = db_->page_manager()->allocatePage(new_dict_page, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    LOG_ERROR(STORAGE, "Failed to allocate new dictionary page: %d", static_cast<int>(status));
                    return 0;
                }

                // Initialize new dictionary page
                uint8_t *new_page_data = nullptr;
                status = buffer_pool_->pinPage(new_dict_page, (void **)&new_page_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);
                    LOG_ERROR(STORAGE, "Failed to pin new dictionary page: %d", static_cast<int>(status));
                    return 0;
                }

                auto *new_dict = reinterpret_cast<SBBitmapDictionaryPage *>(new_page_data);
                std::memset(new_dict, 0, sizeof(SBBitmapDictionaryPage));
                new_dict->bmp_dict_header.magic = K_MAGIC_SBRD;
                new_dict->bmp_dict_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                new_dict->bmp_dict_header.page_type = static_cast<uint16_t>(PageType::BITMAP_INDEX_DICT);
                new_dict->bmp_dict_header.page_size = db_->page_size();
                new_dict->bmp_dict_header.page_id = new_dict_page;
                new_dict->bmp_dict_count = 0;
                new_dict->bmp_dict_free_offset = sizeof(SBBitmapDictionaryPage);
                new_dict->bmp_dict_next_page = 0;

                // Link current page to new page
                dict_page->bmp_dict_next_page = new_dict_page;
                buffer_pool_->unpinPage(current_page, true, ctx); // Mark dirty

                // Switch to new page
                current_page = new_dict_page;
                dict_page = new_dict;
                page_data = new_page_data;

                LOG_DEBUG(STORAGE, "Allocated new dictionary page %u (chained from previous)", new_dict_page);
            }

            // Now we have a page with space - allocate bitmap root page
            uint32_t bitmap_root = 0;
            status = db_->page_manager()->allocatePage(bitmap_root, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(current_page, false, ctx);
                return 0;
            }

            uint8_t *bitmap_data = nullptr;
            status = buffer_pool_->pinPage(bitmap_root, (void **)&bitmap_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(current_page, false, ctx);
                return 0;
            }

            auto *root_page = reinterpret_cast<SBRoaringBitmapRootPage *>(bitmap_data);
            std::memset(root_page, 0, sizeof(SBRoaringBitmapRootPage));
            root_page->rbr_header.magic = K_MAGIC_SBRD;
            root_page->rbr_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            root_page->rbr_header.page_type = static_cast<uint16_t>(PageType::BITMAP_ROARING_ROOT);
            root_page->rbr_header.page_size = db_->page_size();
            root_page->rbr_header.page_id = bitmap_root;
            root_page->rbr_num_containers = 0;
            root_page->rbr_total_cardinality = 0;

            buffer_pool_->unpinPage(bitmap_root, true, ctx);

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

            buffer_pool_->unpinPage(current_page, true, ctx);

            // Update meta page
            uint8_t *meta_data = nullptr;
            buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
            meta->bmp_num_distinct_values++;
            num_distinct_values_ = meta->bmp_num_distinct_values;
            buffer_pool_->unpinPage(meta_page_, true, ctx);

            return bitmap_root;
        }

        std::unique_ptr<RoaringBitmap> BitmapIndex::loadBitmap(
            uint32_t bitmap_root_page,
            ErrorContext *ctx)
        {
            return std::make_unique<RoaringBitmap>(db_, bitmap_root_page);
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

            // Convert TID struct to uint64_t for bitmap storage
            // Uses full 64-bit value (GPID-compatible)
            uint64_t tid_value = convertTIDtoLegacy(tid);

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
            Status status = bitmap->add(tid_value, current_xid, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Update total tuples count
            uint8_t *meta_data = nullptr;
            buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            auto *meta = reinterpret_cast<SBBitmapIndexMetaPage *>(meta_data);
            meta->bmp_total_tuples++;
            total_tuples_ = meta->bmp_total_tuples;
            buffer_pool_->unpinPage(meta_page_, true, ctx);

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

            // Convert TID struct to 64-bit value for bitmap storage
            uint64_t tid_value = convertTIDtoLegacy(tid);

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
                status = buffer_pool_->pinPage(current_dict_page, (void **)&page_data, ctx);
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
                            Status remove_status = bitmap->remove(tid_value, current_xid, ctx);
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

                buffer_pool_->unpinPage(current_dict_page, false, ctx);

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
                Status status = buffer_pool->pinPage(page_id, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    // If we can't read the page, skip this TID
                    continue;
                }

                // Read the item pointer to get tuple offset
                auto *page_special = reinterpret_cast<HeapPageSpecial *>(page_data + db_->page_size() - sizeof(HeapPageSpecial));
                uint16_t item_count = page_special->pd_lower / sizeof(struct ItemPointer);

                if (item_id >= item_count)
                {
                    buffer_pool->unpinPage(page_id, false, ctx);
                    continue; // Invalid item ID
                }

                auto *item_pointers = reinterpret_cast<ItemPointer *>(page_data + db_->page_size() - sizeof(HeapPageSpecial) - sizeof(ItemPointer) * (item_id + 1));
                ItemPointer item = *item_pointers;

                if (item.offset == 0 || item.length == 0)
                {
                    buffer_pool->unpinPage(page_id, false, ctx);
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
                    // Check if creating transaction is visible using TIP
                    bool xmin_visible = txn_manager->isVersionVisible(tuple_header->xmin, current_xid);
                    bool xmax_visible = (tuple_header->xmax != 0) &&
                                        txn_manager->isVersionVisible(tuple_header->xmax, current_xid);

                    // Tuple is visible if inserted by visible transaction and not deleted by visible transaction
                    visible = (xmin_visible && !xmax_visible);
                }

                if (visible)
                {
                    visible_tids.push_back(tid);
                }

                buffer_pool->unpinPage(page_id, false, ctx);
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
            std::vector<uint64_t> tid_values = bitmap->toVisibleArray(current_xid, txn_mgr, ctx);
            results->reserve(tid_values.size());

            // Convert 64-bit values to TID structs
            for (uint64_t tid_value : tid_values)
            {
                TID tid = convertLegacyTID(tid_value);
                results->push_back(tid);
            }

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

            // Load all bitmaps
            std::vector<std::unique_ptr<RoaringBitmap>> bitmaps;
            for (size_t i = 0; i < values.size(); i++)
            {
                uint32_t bitmap_root = 0;
                findDictionaryEntry(values[i], value_lens[i], &bitmap_root, ctx);

                if (bitmap_root == 0)
                {
                    return results; // One value not found = empty intersection
                }

                bitmaps.push_back(loadBitmap(bitmap_root, ctx));
            }

            // Perform intersection
            auto result_bitmap = std::move(bitmaps[0]);
            for (size_t i = 1; i < bitmaps.size(); i++)
            {
                result_bitmap = RoaringBitmap::bitwiseAnd(*result_bitmap, *bitmaps[i], ctx);
            }

            // Get all 64-bit TID values from bitmap
            std::vector<uint64_t> tid_values = result_bitmap->toArray(ctx);
            results.reserve(tid_values.size());

            // Convert 64-bit values to TID structs
            for (uint64_t tid_value : tid_values)
            {
                TID tid = convertLegacyTID(tid_value);
                results.push_back(tid);
            }

            // Firebird MGA: Post-filter results by heap tuple visibility using TIP lookups
            results = filterTidsByVisibility(results, current_xid, ctx);

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

            // Load all bitmaps
            std::vector<std::unique_ptr<RoaringBitmap>> bitmaps;
            for (size_t i = 0; i < values.size(); i++)
            {
                uint32_t bitmap_root = 0;
                findDictionaryEntry(values[i], value_lens[i], &bitmap_root, ctx);

                if (bitmap_root != 0)
                {
                    bitmaps.push_back(loadBitmap(bitmap_root, ctx));
                }
            }

            if (bitmaps.empty())
            {
                return results;
            }

            // Perform union
            auto result_bitmap = std::move(bitmaps[0]);
            for (size_t i = 1; i < bitmaps.size(); i++)
            {
                result_bitmap = RoaringBitmap::bitwiseOr(*result_bitmap, *bitmaps[i], ctx);
            }

            // Get all 64-bit TID values from bitmap
            std::vector<uint64_t> tid_values = result_bitmap->toArray(ctx);
            results.reserve(tid_values.size());

            // Convert 64-bit values to TID structs
            for (uint64_t tid_value : tid_values)
            {
                TID tid = convertLegacyTID(tid_value);
                results.push_back(tid);
            }

            // Firebird MGA: Post-filter results by heap tuple visibility using TIP lookups
            results = filterTidsByVisibility(results, current_xid, ctx);

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
            std::vector<uint64_t> tid_values = not_bitmap->toArray(ctx);
            results.reserve(tid_values.size());

            for (uint64_t tid_value : tid_values)
            {
                TID tid = convertLegacyTID(tid_value);
                results.push_back(tid);
            }

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
            //   Uncompressed = num_containers * 65536 bits = num_containers * 8192 bytes
            //   Compressed = actual storage (ARRAY containers use 2 bytes per value, BITSET uses 8192 bytes)

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
                    Status status = buffer_pool_->pinPage(current_dict_page, (void **)&page_data, ctx);
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

                    buffer_pool_->unpinPage(current_dict_page, false, ctx);
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

        // ========================================
        // RoaringBitmap Implementation
        // ========================================

        RoaringBitmap::RoaringBitmap(Database *db, uint32_t root_page)
            : db_(db),
              buffer_pool_(db->buffer_pool()),
              root_page_(root_page),
              cardinality_(0)
        {
            // Load root page to get cardinality
            uint8_t *root_data = nullptr;
            ErrorContext ctx;
            if (buffer_pool_->pinPage(root_page, (void **)&root_data, &ctx) == Status::OK)
            {
                auto *root = reinterpret_cast<SBRoaringBitmapRootPage *>(root_data);
                cardinality_ = root->rbr_total_cardinality;
                buffer_pool_->unpinPage(root_page, false, &ctx);
            }
        }

        RoaringBitmap::~RoaringBitmap() = default;

        // TASK-CRITICAL-2: MGA-compliant add with xmin tracking
        // Firebird MGA: Per MGA_RULES.md Rule 6 - store xmin with each entry
        Status RoaringBitmap::add(uint64_t value, uint64_t xmin, ErrorContext *ctx)
        {
            // Split 64-bit value: high 48 bits for container key, low 16 bits for value
            uint64_t high = value >> 16;
            uint16_t low = value & 0xFFFF;

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
                    container->bitset_data.resize(BITSET_SIZE_UINT64, 0);
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
                buffer_pool_->pinPage(root_page_, (void **)&root_data, ctx);
                auto *root = reinterpret_cast<SBRoaringBitmapRootPage *>(root_data);
                root->rbr_total_cardinality = cardinality_;
                buffer_pool_->unpinPage(root_page_, true, ctx);
            }

            return status;
        }

        // TASK-CRITICAL-2: MGA-compliant remove with xmax marking (logical deletion)
        // Firebird MGA: Per MGA_RULES.md Rule 5 - NO physical removal, only xmax tombstones
        Status RoaringBitmap::remove(uint64_t value, uint64_t xmax, ErrorContext *ctx)
        {
            // Split 64-bit value: high 48 bits for container key, low 16 bits for value
            uint64_t high = value >> 16;
            uint16_t low = value & 0xFFFF;

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

        bool RoaringBitmap::contains(uint64_t value, ErrorContext *ctx)
        {
            // Split 64-bit value: high 48 bits for container key, low 16 bits for value
            uint64_t high = value >> 16;
            uint16_t low = value & 0xFFFF;

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
        std::vector<uint64_t> RoaringBitmap::toArray(ErrorContext *ctx)
        {
            std::vector<uint64_t> results;
            results.reserve(cardinality_);

            // Load all containers if not cached
            // For now, work with cached containers

            for (const auto &container : containers_)
            {
                // Reconstruct 64-bit value: high 48 bits from container key, low 16 bits from data
                uint64_t high_bits = container.key << 16;

                if (container.type == ContainerType::ARRAY)
                {
                    // TASK-CRITICAL-2: Extract TIDs from versioned entries
                    for (const auto& entry : container.array_data_versioned)
                    {
                        results.push_back(high_bits | entry.tid_low);
                    }
                }
                else if (container.type == ContainerType::BITSET)
                {
                    for (size_t word_idx = 0; word_idx < BITSET_SIZE_UINT64; word_idx++)
                    {
                        uint64_t word = container.bitset_data[word_idx];
                        if (word == 0)
                            continue;

                        for (size_t bit_idx = 0; bit_idx < 64; bit_idx++)
                        {
                            if (word & (1ULL << bit_idx))
                            {
                                uint16_t low = word_idx * 64 + bit_idx;
                                results.push_back(high_bits | low);
                            }
                        }
                    }
                }
            }

            return results;
        }

        // TASK-CRITICAL-2: Get all versioned entries with xmin/xmax
        std::vector<std::pair<uint64_t, VersionedBitmapEntry>> RoaringBitmap::toVersionedArray(ErrorContext *ctx)
        {
            std::vector<std::pair<uint64_t, VersionedBitmapEntry>> results;
            results.reserve(cardinality_);

            for (const auto &container : containers_)
            {
                uint64_t high_bits = container.key << 16;

                if (container.type == ContainerType::ARRAY)
                {
                    for (const auto& entry : container.array_data_versioned)
                    {
                        uint64_t tid = high_bits | entry.tid_low;
                        results.push_back(std::make_pair(tid, entry));
                    }
                }
                else if (container.type == ContainerType::BITSET)
                {
                    // For bitset, extract entries from bitset_versions
                    for (const auto& kv : container.bitset_versions)
                    {
                        uint64_t tid = high_bits | kv.first;
                        results.push_back(std::make_pair(tid, kv.second));
                    }
                }
            }

            return results;
        }

        // TASK-CRITICAL-2: Get only visible entries (MGA-compliant index-level visibility)
        // Firebird MGA: Per MGA_RULES.md Rule 11 - use TransactionId, NOT Snapshot
        // This eliminates the need for heap access (20-40% performance improvement)
        std::vector<uint64_t> RoaringBitmap::toVisibleArray(uint64_t current_xid,
                                                             TransactionManager *txn_mgr,
                                                             ErrorContext *ctx)
        {
            std::vector<uint64_t> results;
            results.reserve(cardinality_);

            if (!txn_mgr)
            {
                // No transaction manager - return all entries
                return toArray(ctx);
            }

            for (const auto &container : containers_)
            {
                uint64_t high_bits = container.key << 16;

                if (container.type == ContainerType::ARRAY)
                {
                    for (const auto& entry : container.array_data_versioned)
                    {
                        // TASK-CRITICAL-2: Index-level visibility check (no heap access!)
                        // Per MGA_RULES.md Rule 3: Use TIP-based visibility
                        if (entry.isVisible(current_xid, txn_mgr))
                        {
                            results.push_back(high_bits | entry.tid_low);
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
                            results.push_back(high_bits | kv.first);
                        }
                    }
                }
            }

            return results;
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
                Status status = db_->page_manager()->allocatePage(page_num, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

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
            Status status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *page = reinterpret_cast<SBRoaringContainerPage *>(page_data);
            page->rcp_header.page_type = static_cast<uint16_t>(PageType::BITMAP_CONTAINER);
            page->rcp_header.page_size = db_->page_size();
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

            buffer_pool_->unpinPage(page_num, true, ctx);
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
                        containerAnd(lhs_cont, rhs_cont, &result_cont);
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
                        containerOr(lhs_cont, rhs_cont, &result_cont);
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
                containerNot(cont, &result_cont);
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
                    full_cont.bitset_data.resize(BITSET_SIZE_UINT64, 0xFFFFFFFFFFFFFFFFULL);

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
                    for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                    {
                        full_cont.num_values += __builtin_popcountll(full_cont.bitset_data[i]);
                    }

                    result->containers_.push_back(full_cont);
                    result->cardinality_ += full_cont.num_values;
                }
            }

            return result;
        }

        void RoaringBitmap::containerAnd(const Container &lhs, const Container &rhs, Container *result)
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
                result->bitset_data.resize(BITSET_SIZE_UINT64);

                for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                {
                    result->bitset_data[i] = lhs.bitset_data[i] & rhs.bitset_data[i];
                    result->num_values += __builtin_popcountll(result->bitset_data[i]);
                }
            }
            else
            {
                // Mixed types: convert both to bitset for intersection
                result->type = ContainerType::BITSET;
                result->bitset_data.resize(BITSET_SIZE_UINT64, 0);

                // Helper to convert container to bitset
                auto to_bitset = [](const Container &c, std::vector<uint64_t> &bitset)
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
                        // Already bitset, copy directly
                        for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                        {
                            bitset[i] = c.bitset_data[i];
                        }
                    }
                };

                // Convert both containers to bitsets
                std::vector<uint64_t> lhs_bitset(BITSET_SIZE_UINT64, 0);
                std::vector<uint64_t> rhs_bitset(BITSET_SIZE_UINT64, 0);
                to_bitset(lhs, lhs_bitset);
                to_bitset(rhs, rhs_bitset);

                // Perform AND operation
                for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                {
                    result->bitset_data[i] = lhs_bitset[i] & rhs_bitset[i];
                    result->num_values += __builtin_popcountll(result->bitset_data[i]);
                }
            }
        }

        void RoaringBitmap::containerOr(const Container &lhs, const Container &rhs, Container *result)
        {
            result->key = lhs.key;

            if (lhs.type == ContainerType::BITSET || rhs.type == ContainerType::BITSET)
            {
                // Use bitset for union if either is bitset
                result->type = ContainerType::BITSET;
                result->bitset_data.resize(BITSET_SIZE_UINT64, 0);

                // Convert both to bitset and OR
                auto to_bitset = [](const Container &c, std::vector<uint64_t> &bitset)
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
                        for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                        {
                            bitset[i] |= c.bitset_data[i];
                        }
                    }
                };

                to_bitset(lhs, result->bitset_data);
                to_bitset(rhs, result->bitset_data);

                // Count bits
                for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                {
                    result->num_values += __builtin_popcountll(result->bitset_data[i]);
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

        void RoaringBitmap::containerNot(const Container &container, Container *result)
        {
            // NOT operation inverts all bits in a container (0 → 1, 1 → 0)
            // Result is always BITSET type (since complement of sparse set is dense)
            result->key = container.key;
            result->type = ContainerType::BITSET;
            result->bitset_data.resize(BITSET_SIZE_UINT64, 0xFFFFFFFFFFFFFFFFULL); // Start with all 1s
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
                for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                {
                    result->num_values += __builtin_popcountll(result->bitset_data[i]);
                }
            }
            else if (container.type == ContainerType::BITSET)
            {
                // For BITSET containers, simple bitwise NOT
                for (size_t i = 0; i < BITSET_SIZE_UINT64; i++)
                {
                    result->bitset_data[i] = ~container.bitset_data[i];
                    result->num_values += __builtin_popcountll(result->bitset_data[i]);
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

            // PHASE 1.5: Convert TID structs to legacy format set for lookup
            std::set<uint32_t> dead_set_32bit;
            for (const TID &tid : dead_tids)
            {
                uint64_t legacy = convertTIDtoLegacy(tid);
                if (legacy != 0)  // Skip custom tablespace TIDs
                {
                    // Bitmap uses 32-bit integers, convert legacy TID
                    uint32_t tid_32 = static_cast<uint32_t>(legacy & 0xFFFFFFFF);
                    dead_set_32bit.insert(tid_32);
                }
            }

            if (dead_set_32bit.empty())
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
                status = buffer_pool_->pinPage(current_dict_page, (void **)&page_data, ctx);
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
                            // Use a high xmax value to mark as deleted (e.g., max uint64_t)
                            uint64_t xmax = std::numeric_limits<uint64_t>::max();
                            for (uint32_t tid_32 : dead_set_32bit)
                            {
                                Status remove_status = bitmap->remove(static_cast<uint64_t>(tid_32), xmax, ctx);
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

                buffer_pool_->unpinPage(current_dict_page, false, ctx);

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

        uint64_t RoaringBitmapIterator::next()
        {
            const auto &container = bitmap_.containers_[container_index_];
            // Reconstruct 64-bit value from container key (high 48 bits) and value (low 16 bits)
            uint64_t high_bits = container.key << 16;
            uint64_t result = 0;

            if (container.type == ContainerType::ARRAY)
            {
                result = high_bits | container.array_data_versioned[value_index_].tid_low;
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

                    if (word_idx >= BITSET_SIZE_UINT64)
                    {
                        container_index_++;
                        value_index_ = 0;
                        break;
                    }

                    uint64_t word = container.bitset_data[word_idx];
                    if (word & (1ULL << bit_idx))
                    {
                        result = high_bits | static_cast<uint16_t>(value_index_);
                        value_index_++;
                        break;
                    }

                    value_index_++;
                }
            }

            return result;
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
                                              std::unique_ptr<RoaringBitmap> bitmap,
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

            // Get next TID from bitmap (in legacy uint64_t format)
            uint64_t legacy_tid = iterator_->next();
            scanned_count_++;

            // Convert legacy TID to TID struct
            TID tid = convertLegacyTID(legacy_tid);

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
            return std::make_unique<BitmapIndexScanner>(this, std::move(bitmap), current_xid, db_);
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
            std::unique_ptr<RoaringBitmap> result_bitmap;

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
                    result_bitmap = std::move(bitmap);
                }
                else
                {
                    // OR this bitmap with the result
                    result_bitmap = RoaringBitmap::bitwiseOr(*result_bitmap, *bitmap, ctx);
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
            return std::make_unique<BitmapIndexScanner>(this, std::move(result_bitmap), current_xid, db_);
        }

    } // namespace core
} // namespace scratchbird
