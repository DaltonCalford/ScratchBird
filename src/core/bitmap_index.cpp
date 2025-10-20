// ScratchBird Bitmap Index - Implementation
// Roaring Bitmap-based index for low-cardinality columns

#include "scratchbird/core/bitmap_index.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/heap_page.h"           // For ItemPointer, TupleHeader, HeapPageSpecial
#include "scratchbird/core/transaction_manager.h" // For Snapshot, TransactionState, isSnapshotVisible
#include <cstring>
#include <algorithm>
#include <functional>

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
            meta->bmp_header.version = DB_VERSION_ALPHA_1_0_1;
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
                dict_page->bmp_dict_header.version = DB_VERSION_ALPHA_1_0_1;
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
                // Need new page
                buffer_pool_->unpinPage(current_page, false, ctx);
                // TODO: Implement multi-page dictionary
                return 0;
            }

            // Allocate bitmap root page
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
            root_page->rbr_header.version = DB_VERSION_ALPHA_1_0_1;
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
            uint64_t tuple_id,
            ErrorContext *ctx)
        {
            if (!value_data || value_len == 0)
            {
                if (ctx)
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid value");
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

            // Convert 64-bit tuple ID to 32-bit integer
            uint32_t int_id = static_cast<uint32_t>(tuple_id);

            Status status = bitmap->add(int_id, ctx);
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

        // PHASE 1 TASK 1.5: Visibility filter for bitmap index (post-filtering)
        // This is a post-filter that checks heap tuple visibility for each TID returned by bitmap operations
        // NOTE: This is less efficient than B-Tree/Hash visibility (20-40% overhead) because:
        //       - Bitmap returns TIDs directly, not pointers to heap tuples
        //       - We must access heap pages separately to check visibility
        //       - Full MVCC redesign would require storing xmin/xmax in bitmap entries (deferred to Beta)
        std::vector<uint64_t> BitmapIndex::filterTidsByVisibility(const std::vector<uint64_t> &tids,
                                                                   const Snapshot *snapshot,
                                                                   ErrorContext *ctx)
        {
            std::vector<uint64_t> visible_tids;

            // If no snapshot provided, return all TIDs (no filtering)
            if (snapshot == nullptr)
            {
                return tids;
            }

            visible_tids.reserve(tids.size());

            auto *buffer_pool = db_->buffer_pool();
            auto *txn_manager = db_->transaction_manager();

            for (uint64_t tid : tids)
            {
                // Extract page_id and item_id from TID
                // TID format: (page_id << 32) | (item_id << 16)
                uint32_t page_id = static_cast<uint32_t>(tid >> 32);
                uint16_t item_id = static_cast<uint16_t>((tid >> 16) & 0xFFFF);

                // Pin the heap page
                uint8_t *page_data = nullptr;
                Status status = buffer_pool->pinPage(page_id, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    // If we can't read the page, skip this TID
                    continue;
                }

                // Read the item pointer to get tuple offset
                auto *page_special = reinterpret_cast<HeapPageSpecial *>(page_data + 8192 - sizeof(HeapPageSpecial));
                uint16_t item_count = page_special->pd_lower / sizeof(struct ItemPointer);

                if (item_id >= item_count)
                {
                    buffer_pool->unpinPage(page_id, false, ctx);
                    continue; // Invalid item ID
                }

                auto *item_pointers = reinterpret_cast<ItemPointer *>(page_data + 8192 - sizeof(HeapPageSpecial) - sizeof(ItemPointer) * (item_id + 1));
                ItemPointer item = *item_pointers;

                if (item.offset == 0 || item.length == 0)
                {
                    buffer_pool->unpinPage(page_id, false, ctx);
                    continue; // Dead tuple
                }

                // Read tuple header
                auto *tuple_header = reinterpret_cast<TupleHeader *>(page_data + item.offset);

                // Check visibility using snapshot
                // Cast snapshot pointer to actual TransactionManager::Snapshot type
                auto *txn_snapshot = reinterpret_cast<const TransactionManager::Snapshot *>(snapshot);

                bool xmin_visible = txn_manager->isSnapshotVisible(tuple_header->xmin, txn_snapshot);
                bool xmax_visible = (tuple_header->xmax != 0) &&
                                    txn_manager->isSnapshotVisible(tuple_header->xmax, txn_snapshot);

                // Tuple is visible if inserted by visible transaction and not deleted by visible transaction
                if (xmin_visible && !xmax_visible)
                {
                    visible_tids.push_back(tid);
                }

                buffer_pool->unpinPage(page_id, false, ctx);
            }

            return visible_tids;
        }

        std::vector<uint64_t> BitmapIndex::find(
            const void *value_data,
            size_t value_len,
            Snapshot *snapshot,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> results;

            uint32_t bitmap_root = 0;
            uint32_t cardinality = findDictionaryEntry(value_data, value_len, &bitmap_root, ctx);

            if (bitmap_root == 0)
            {
                return results; // Value not found
            }

            auto bitmap = loadBitmap(bitmap_root, ctx);
            if (!bitmap)
            {
                return results;
            }

            std::vector<uint32_t> int_results = bitmap->toArray(ctx);
            results.reserve(int_results.size());

            for (uint32_t id : int_results)
            {
                results.push_back(static_cast<uint64_t>(id));
            }

            // PHASE 1 TASK 1.5: Post-filter results by heap tuple visibility
            results = filterTidsByVisibility(results, snapshot, ctx);

            return results;
        }

        std::vector<uint64_t> BitmapIndex::findAnd(
            const std::vector<const void *> &values,
            const std::vector<size_t> &value_lens,
            Snapshot *snapshot,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> results;

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

            std::vector<uint32_t> int_results = result_bitmap->toArray(ctx);
            results.reserve(int_results.size());

            for (uint32_t id : int_results)
            {
                results.push_back(static_cast<uint64_t>(id));
            }

            // PHASE 1 TASK 1.5: Post-filter results by heap tuple visibility
            results = filterTidsByVisibility(results, snapshot, ctx);

            return results;
        }

        std::vector<uint64_t> BitmapIndex::findOr(
            const std::vector<const void *> &values,
            const std::vector<size_t> &value_lens,
            Snapshot *snapshot,
            ErrorContext *ctx)
        {
            std::vector<uint64_t> results;

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

            std::vector<uint32_t> int_results = result_bitmap->toArray(ctx);
            results.reserve(int_results.size());

            for (uint32_t id : int_results)
            {
                results.push_back(static_cast<uint64_t>(id));
            }

            // PHASE 1 TASK 1.5: Post-filter results by heap tuple visibility
            results = filterTidsByVisibility(results, snapshot, ctx);

            return results;
        }

        BitmapIndex::Statistics BitmapIndex::getStatistics(ErrorContext *ctx)
        {
            Statistics stats;
            stats.num_distinct_values = num_distinct_values_;
            stats.total_tuples = total_tuples_;
            stats.total_pages = 1 + num_distinct_values_; // Meta + one root per value (approximation)
            stats.avg_cardinality = (num_distinct_values_ > 0) ? (total_tuples_ / num_distinct_values_) : 0;
            stats.compression_ratio = 1.0; // TODO: Calculate actual compression

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

        Status RoaringBitmap::add(uint32_t value, ErrorContext *ctx)
        {
            uint16_t high = value >> 16;
            uint16_t low = value & 0xFFFF;

            Container *container = findOrCreateContainer(high, ctx);
            if (!container)
            {
                return Status::IO_ERROR;
            }

            // Check if value already exists
            if (container->type == ContainerType::ARRAY)
            {
                auto it = std::lower_bound(container->array_data.begin(),
                                           container->array_data.end(), low);
                if (it != container->array_data.end() && *it == low)
                {
                    return Status::OK; // Already exists
                }

                container->array_data.insert(it, low);
                container->num_values++;

                // Convert to bitset if needed
                if (container->num_values > ARRAY_MAX_SIZE)
                {
                    container->bitset_data.resize(BITSET_SIZE_UINT64, 0);
                    for (uint16_t val : container->array_data)
                    {
                        size_t word_idx = val / 64;
                        size_t bit_idx = val % 64;
                        container->bitset_data[word_idx] |= (1ULL << bit_idx);
                    }
                    container->array_data.clear();
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
                    container->bitset_data[word_idx] |= mask;
                    container->num_values++;
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

        // PHASE 2 TASK 2.5: Remove a value from the bitmap
        Status RoaringBitmap::remove(uint32_t value, ErrorContext *ctx)
        {
            uint16_t high = value >> 16;
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

            bool value_removed = false;

            // Remove from container based on type
            if (container->type == ContainerType::ARRAY)
            {
                auto it = std::lower_bound(container->array_data.begin(),
                                           container->array_data.end(), low);
                if (it != container->array_data.end() && *it == low)
                {
                    container->array_data.erase(it);
                    container->num_values--;
                    value_removed = true;
                }
            }
            else if (container->type == ContainerType::BITSET)
            {
                size_t word_idx = low / 64;
                size_t bit_idx = low % 64;
                uint64_t mask = 1ULL << bit_idx;

                if (container->bitset_data[word_idx] & mask)
                {
                    container->bitset_data[word_idx] &= ~mask;
                    container->num_values--;
                    value_removed = true;

                    // Optionally convert back to array if sparse enough
                    // For simplicity, we keep as bitset for now
                }
            }

            if (!value_removed)
            {
                // Value didn't exist, no-op
                return Status::OK;
            }

            // Save the modified container
            Status status = saveContainer(*container, ctx);
            if (status == Status::OK)
            {
                cardinality_--;

                // Update root page cardinality
                uint8_t *root_data = nullptr;
                status = buffer_pool_->pinPage(root_page_, (void **)&root_data, ctx);
                if (status == Status::OK)
                {
                    auto *root = reinterpret_cast<SBRoaringBitmapRootPage *>(root_data);
                    root->rbr_total_cardinality = cardinality_;
                    buffer_pool_->unpinPage(root_page_, true, ctx);
                }
            }

            return status;
        }

        bool RoaringBitmap::contains(uint32_t value, ErrorContext *ctx)
        {
            uint16_t high = value >> 16;
            uint16_t low = value & 0xFFFF;

            for (const auto &container : containers_)
            {
                if (container.key == high)
                {
                    if (container.type == ContainerType::ARRAY)
                    {
                        return std::binary_search(container.array_data.begin(),
                                                  container.array_data.end(), low);
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

        std::vector<uint32_t> RoaringBitmap::toArray(ErrorContext *ctx)
        {
            std::vector<uint32_t> results;
            results.reserve(cardinality_);

            // Load all containers if not cached
            // For now, work with cached containers

            for (const auto &container : containers_)
            {
                uint32_t high_bits = static_cast<uint32_t>(container.key) << 16;

                if (container.type == ContainerType::ARRAY)
                {
                    for (uint16_t low : container.array_data)
                    {
                        results.push_back(high_bits | low);
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

        RoaringBitmap::Container *RoaringBitmap::findOrCreateContainer(uint16_t key, ErrorContext *ctx)
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
                std::memcpy(data_area, container.array_data.data(),
                            container.array_data.size() * sizeof(uint16_t));
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
            // Create new bitmap for result
            auto result = std::make_unique<RoaringBitmap>(lhs.db_, 0); // TODO: Allocate root page

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

        void RoaringBitmap::containerAnd(const Container &lhs, const Container &rhs, Container *result)
        {
            result->key = lhs.key;
            result->type = ContainerType::ARRAY;
            result->num_values = 0;

            if (lhs.type == ContainerType::ARRAY && rhs.type == ContainerType::ARRAY)
            {
                // Array-array intersection
                size_t i = 0, j = 0;
                while (i < lhs.array_data.size() && j < rhs.array_data.size())
                {
                    if (lhs.array_data[i] == rhs.array_data[j])
                    {
                        result->array_data.push_back(lhs.array_data[i]);
                        result->num_values++;
                        i++;
                        j++;
                    }
                    else if (lhs.array_data[i] < rhs.array_data[j])
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
            // TODO: Handle mixed types
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
                        for (uint16_t val : c.array_data)
                        {
                            size_t word_idx = val / 64;
                            size_t bit_idx = val % 64;
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
                // Array-array union
                result->type = ContainerType::ARRAY;
                result->array_data = lhs.array_data;

                for (uint16_t val : rhs.array_data)
                {
                    auto it = std::lower_bound(result->array_data.begin(),
                                               result->array_data.end(), val);
                    if (it == result->array_data.end() || *it != val)
                    {
                        result->array_data.insert(it, val);
                    }
                }

                result->num_values = result->array_data.size();
            }
        }

        // ========================================
        // BitmapIndex Garbage Collection
        // ========================================

        // PHASE 2 TASK 2.5: Remove index entries pointing to dead tuples
        Status BitmapIndex::removeDeadEntries(const std::vector<uint64_t> &dead_tids,
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
            //
            // Algorithm:
            // 1. Scan dictionary chain (all distinct values)
            // 2. For each dictionary entry:
            //    a. Load the RoaringBitmap for this value
            //    b. For each dead TID:
            //       - Convert 64-bit TID to 32-bit (TID format varies by implementation)
            //       - Call bitmap->remove(tid_32bit)
            //    c. Track entries removed and pages modified
            //
            // TID Conversion: Roaring bitmaps use 32-bit values
            // ScratchBird TIDs are 64-bit (page_id << 32 | item_id)
            // We need to convert or use a mapping scheme

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
                            // NOTE: We assume TID fits in 32 bits for Roaring bitmap
                            // If TIDs are larger, we need a different mapping scheme
                            for (uint64_t dead_tid : dead_tids)
                            {
                                // Convert 64-bit TID to 32-bit
                                // WARNING: This truncates! A better approach would use
                                // a sequential mapping or different bitmap organization
                                uint32_t tid_32 = static_cast<uint32_t>(dead_tid & 0xFFFFFFFF);

                                Status remove_status = bitmap->remove(tid_32, ctx);
                                if (remove_status == Status::OK)
                                {
                                    total_entries_removed++;
                                    // Note: pages_modified tracked internally by RoaringBitmap
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

        uint32_t RoaringBitmapIterator::next()
        {
            const auto &container = bitmap_.containers_[container_index_];
            uint32_t high_bits = static_cast<uint32_t>(container.key) << 16;
            uint32_t result = 0;

            if (container.type == ContainerType::ARRAY)
            {
                result = high_bits | container.array_data[value_index_];
                value_index_++;

                if (value_index_ >= container.array_data.size())
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

    } // namespace core
} // namespace scratchbird
