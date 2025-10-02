#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/hash_functions.h"
#include <cstring>
#include <algorithm>

namespace scratchbird
{
    namespace core
    {
        // Constructor
        HashIndex::HashIndex(Database* db, const ID& index_uuid)
            : db_(db)
            , buffer_pool_(db->buffer_pool())
            , index_uuid_(index_uuid)
            , meta_page_(0)
        {
        }

        // Destructor
        HashIndex::~HashIndex() = default;

        // Create a new hash index
        Status HashIndex::create(Database* db, const ID& index_uuid,
                               uint32_t* meta_page_out, ErrorContext* ctx)
        {
            if (!db || !meta_page_out)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid arguments to HashIndex::create");
                return Status::INVALID_ARGUMENT;
            }

            BufferPool* buffer_pool = db->buffer_pool();
            if (!buffer_pool)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
                return Status::INVALID_ARGUMENT;
            }

            // Step 1: Allocate meta page
            uint32_t meta_page = 0;
            Status status = db->allocatePage(&meta_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Step 2: Pin and initialize meta page
            uint8_t* meta_page_data = nullptr;
            status = buffer_pool->pinPage(meta_page, &meta_page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_page_data);
            std::memset(meta, 0, sizeof(SBHashIndexMetaPage));

            // Initialize meta page header
            meta->hip_header.magic = MAGIC_NUMBER;
            meta->hip_header.version = FORMAT_VERSION;
            meta->hip_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_META);
            meta->hip_header.page_size = db->page_size();
            meta->hip_header.page_id = meta_page;

            // Initialize meta data
            meta->hip_index_uuid = index_uuid;
            meta->hip_hash_func_id = HASH_FUNC_MURMUR3;
            meta->hip_global_depth = INITIAL_GLOBAL_DEPTH;
            meta->hip_num_tuples = 0;
            meta->hip_num_deleted = 0;

            // Step 3: Allocate directory page
            uint32_t dir_page = 0;
            status = db->allocatePage(&dir_page, ctx);
            if (status != Status::OK)
            {
                buffer_pool->unpinPage(meta_page, false, ctx);
                return status;
            }

            meta->hip_directory_page = dir_page;

            // Step 4: Initialize directory page
            uint8_t* dir_page_data = nullptr;
            status = buffer_pool->pinPage(dir_page, &dir_page_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool->unpinPage(meta_page, false, ctx);
                return status;
            }

            auto* dir = reinterpret_cast<SBHashDirectoryPage*>(dir_page_data);
            std::memset(dir, 0, sizeof(SBHashDirectoryPage));

            dir->hdp_header.magic = MAGIC_NUMBER;
            dir->hdp_header.version = FORMAT_VERSION;
            dir->hdp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_DIRECTORY);
            dir->hdp_header.page_size = db->page_size();
            dir->hdp_header.page_id = dir_page;
            dir->hdp_next_page = 0;

            // Step 5: Allocate initial bucket pages (2^INITIAL_GLOBAL_DEPTH buckets)
            uint32_t num_buckets = (1U << INITIAL_GLOBAL_DEPTH);
            for (uint32_t i = 0; i < num_buckets; i++)
            {
                uint32_t bucket_page = 0;
                status = db->allocatePage(&bucket_page, ctx);
                if (status != Status::OK)
                {
                    buffer_pool->unpinPage(dir_page, false, ctx);
                    buffer_pool->unpinPage(meta_page, false, ctx);
                    return status;
                }

                // Initialize bucket page
                uint8_t* bucket_data = nullptr;
                status = buffer_pool->pinPage(bucket_page, &bucket_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool->unpinPage(dir_page, false, ctx);
                    buffer_pool->unpinPage(meta_page, false, ctx);
                    return status;
                }

                auto* bucket = reinterpret_cast<SBHashBucketPage*>(bucket_data);
                std::memset(bucket, 0, sizeof(SBHashBucketPage));

                bucket->hbp_header.magic = MAGIC_NUMBER;
                bucket->hbp_header.version = FORMAT_VERSION;
                bucket->hbp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_BUCKET);
                bucket->hbp_header.page_size = db->page_size();
                bucket->hbp_header.page_id = bucket_page;
                bucket->hbp_entry_count = 0;
                bucket->hbp_local_depth = INITIAL_GLOBAL_DEPTH;
                bucket->hbp_deleted_count = 0;
                bucket->hbp_overflow_page = 0;

                buffer_pool->unpinPage(bucket_page, true, ctx);

                // Add to directory
                dir->hdp_bucket_pointers[i] = bucket_page;
            }

            // Unpin pages
            buffer_pool->unpinPage(dir_page, true, ctx);
            buffer_pool->unpinPage(meta_page, true, ctx);

            *meta_page_out = meta_page;
            return Status::OK;
        }

        // Open an existing hash index
        std::unique_ptr<HashIndex> HashIndex::open(Database* db, const ID& index_uuid,
                                                   uint32_t meta_page, ErrorContext* ctx)
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
                return nullptr;
            }

            auto index = std::make_unique<HashIndex>(db, index_uuid);
            index->meta_page_ = meta_page;

            // Verify meta page
            uint8_t* meta_data = nullptr;
            Status status = db->buffer_pool()->pinPage(meta_page, &meta_data, ctx);
            if (status != Status::OK)
            {
                return nullptr;
            }

            auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);
            if (meta->hip_header.page_type != static_cast<uint16_t>(PageType::HASH_INDEX_META))
            {
                db->buffer_pool()->unpinPage(meta_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::CORRUPTED, "Invalid hash index meta page");
                return nullptr;
            }

            if (meta->hip_index_uuid != index_uuid)
            {
                db->buffer_pool()->unpinPage(meta_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::CORRUPTED, "Hash index UUID mismatch");
                return nullptr;
            }

            db->buffer_pool()->unpinPage(meta_page, false, ctx);

            return index;
        }

        // Helper: Calculate directory index from hash value
        uint32_t HashIndex::getDirectoryIndex(uint64_t hash, uint32_t global_depth)
        {
            // Use the first 'global_depth' bits of the hash
            return hash & ((1U << global_depth) - 1);
        }

        // Helper: Find bucket page for a given hash value
        uint64_t HashIndex::findBucketPageForKey(uint64_t hash, ErrorContext* ctx)
        {
            // Pin meta page
            uint8_t* meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, &meta_data, ctx);
            if (status != Status::OK)
            {
                return 0;
            }

            auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t dir_page = meta->hip_directory_page;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Calculate directory index
            uint32_t dir_index = getDirectoryIndex(hash, global_depth);

            // Pin directory page
            uint8_t* dir_data = nullptr;
            status = buffer_pool_->pinPage(dir_page, &dir_data, ctx);
            if (status != Status::OK)
            {
                return 0;
            }

            auto* dir = reinterpret_cast<SBHashDirectoryPage*>(dir_data);
            uint64_t bucket_page = dir->hdp_bucket_pointers[dir_index];

            buffer_pool_->unpinPage(dir_page, false, ctx);

            return bucket_page;
        }

        // Helper: Allocate a new bucket page
        Status HashIndex::allocateBucketPage(uint32_t* page_num_out, ErrorContext* ctx)
        {
            uint32_t page_num = 0;
            Status status = db_->allocatePage(&page_num, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            uint8_t* page_data = nullptr;
            status = buffer_pool_->pinPage(page_num, &page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto* bucket = reinterpret_cast<SBHashBucketPage*>(page_data);
            std::memset(bucket, 0, sizeof(SBHashBucketPage));

            bucket->hbp_header.magic = MAGIC_NUMBER;
            bucket->hbp_header.version = FORMAT_VERSION;
            bucket->hbp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_BUCKET);
            bucket->hbp_header.page_size = db_->page_size();
            bucket->hbp_header.page_id = page_num;
            bucket->hbp_entry_count = 0;
            bucket->hbp_local_depth = 0; // Will be set by caller
            bucket->hbp_deleted_count = 0;
            bucket->hbp_overflow_page = 0;

            buffer_pool_->unpinPage(page_num, true, ctx);

            *page_num_out = page_num;
            return Status::OK;
        }

        // Helper: Allocate overflow page
        Status HashIndex::allocateOverflowPage(uint32_t* page_num_out, ErrorContext* ctx)
        {
            // Overflow pages are just bucket pages
            return allocateBucketPage(page_num_out, ctx);
        }

        // Helper: Check if bucket needs splitting
        bool HashIndex::bucketNeedsSplit(SBHashBucketPage* bucket)
        {
            uint16_t total_entries = bucket->hbp_entry_count - bucket->hbp_deleted_count;
            uint16_t capacity = MAX_ENTRIES_PER_BUCKET;
            uint16_t threshold = (capacity * BUCKET_FILL_THRESHOLD) / 100;
            return total_entries >= threshold;
        }

        // Continue in next part due to size...

    } // namespace core
} // namespace scratchbird
