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

        // Helper: Count total entries in bucket including overflow chain
        uint16_t HashIndex::countEntriesInBucket(uint32_t bucket_page, ErrorContext* ctx)
        {
            uint16_t count = 0;
            uint32_t current_page = bucket_page;

            while (current_page != 0)
            {
                uint8_t* page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, &page_data, ctx);
                if (status != Status::OK)
                {
                    break;
                }

                auto* bucket = reinterpret_cast<SBHashBucketPage*>(page_data);
                count += (bucket->hbp_entry_count - bucket->hbp_deleted_count);
                uint32_t next_page = bucket->hbp_overflow_page;

                buffer_pool_->unpinPage(current_page, false, ctx);
                current_page = next_page;
            }

            return count;
        }

        // Insert operation
        Status HashIndex::insert(const void* key_data, size_t key_len, uint64_t tuple_id,
                                ErrorContext* ctx)
        {
            if (!key_data || key_len == 0 || tuple_id == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid insert arguments");
                return Status::INVALID_ARGUMENT;
            }

            // Calculate hash
            uint64_t hash = MurmurHash64(key_data, key_len);

            // Find bucket page
            uint64_t bucket_page = findBucketPageForKey(hash, ctx);
            if (bucket_page == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Failed to find bucket page");
                return Status::IO_ERROR;
            }

            // Try to insert in bucket (or overflow chain)
            uint32_t current_page = bucket_page;
            while (current_page != 0)
            {
                uint8_t* page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, &page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto* bucket = reinterpret_cast<SBHashBucketPage*>(page_data);

                // Check if there's space in this page
                if (bucket->hbp_entry_count < MAX_ENTRIES_PER_BUCKET)
                {
                    // Add entry
                    HashEntry& entry = bucket->hbp_entries[bucket->hbp_entry_count];
                    entry.he_key_hash = hash;
                    entry.he_tuple_id = tuple_id;
                    bucket->hbp_entry_count++;

                    buffer_pool_->unpinPage(current_page, true, ctx);

                    // Update meta page statistics
                    uint8_t* meta_data = nullptr;
                    status = buffer_pool_->pinPage(meta_page_, &meta_data, ctx);
                    if (status == Status::OK)
                    {
                        auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);
                        meta->hip_num_tuples++;
                        buffer_pool_->unpinPage(meta_page_, true, ctx);
                    }

                    return Status::OK;
                }

                // Check if there's an overflow page
                if (bucket->hbp_overflow_page != 0)
                {
                    current_page = bucket->hbp_overflow_page;
                    buffer_pool_->unpinPage(bucket_page, false, ctx);
                    continue;
                }

                // No overflow page and bucket is full - need to split or add overflow
                // First check if we should split
                if (bucketNeedsSplit(bucket))
                {
                    buffer_pool_->unpinPage(current_page, false, ctx);

                    // Split the bucket
                    status = splitBucket(bucket_page, hash, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }

                    // Retry insert after split
                    return insert(key_data, key_len, tuple_id, ctx);
                }
                else
                {
                    // Add overflow page
                    uint32_t overflow_page = 0;
                    status = allocateOverflowPage(&overflow_page, ctx);
                    if (status != Status::OK)
                    {
                        buffer_pool_->unpinPage(current_page, false, ctx);
                        return status;
                    }

                    bucket->hbp_overflow_page = overflow_page;
                    buffer_pool_->unpinPage(current_page, true, ctx);

                    // Insert in new overflow page
                    current_page = overflow_page;
                }
            }

            SET_ERROR_CONTEXT(ctx, Status::INTERNAL_ERROR, "Insert failed unexpectedly");
            return Status::INTERNAL_ERROR;
        }

        // Split bucket operation
        Status HashIndex::splitBucket(uint32_t bucket_page, uint64_t hash, ErrorContext* ctx)
        {
            // Pin meta page to get global depth
            uint8_t* meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, &meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);
            uint32_t global_depth = meta->hip_global_depth;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Pin bucket page
            uint8_t* bucket_data = nullptr;
            status = buffer_pool_->pinPage(bucket_page, &bucket_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto* old_bucket = reinterpret_cast<SBHashBucketPage*>(bucket_data);
            uint32_t local_depth = old_bucket->hbp_local_depth;

            // Check if we need directory expansion
            if (local_depth >= global_depth)
            {
                buffer_pool_->unpinPage(bucket_page, false, ctx);

                // Expand directory first
                status = expandDirectory(ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                // Retry split after expansion
                return splitBucket(bucket_page, hash, ctx);
            }

            // Allocate new bucket
            uint32_t new_bucket_page = 0;
            status = allocateBucketPage(&new_bucket_page, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(bucket_page, false, ctx);
                return status;
            }

            // Pin new bucket
            uint8_t* new_bucket_data = nullptr;
            status = buffer_pool_->pinPage(new_bucket_page, &new_bucket_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(bucket_page, false, ctx);
                return status;
            }

            auto* new_bucket = reinterpret_cast<SBHashBucketPage*>(new_bucket_data);

            // Increment local depth for both buckets
            uint32_t new_local_depth = local_depth + 1;
            old_bucket->hbp_local_depth = new_local_depth;
            new_bucket->hbp_local_depth = new_local_depth;

            // Redistribute entries
            status = redistributeEntries(old_bucket, new_bucket, new_local_depth, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(new_bucket_page, false, ctx);
                buffer_pool_->unpinPage(bucket_page, false, ctx);
                return status;
            }

            buffer_pool_->unpinPage(new_bucket_page, true, ctx);
            buffer_pool_->unpinPage(bucket_page, true, ctx);

            // Update directory pointers
            // Pin directory page
            status = buffer_pool_->pinPage(meta_page_, &meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);
            uint64_t dir_page = meta->hip_directory_page;
            global_depth = meta->hip_global_depth;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            uint8_t* dir_data = nullptr;
            status = buffer_pool_->pinPage(dir_page, &dir_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto* dir = reinterpret_cast<SBHashDirectoryPage*>(dir_data);

            // Update directory entries that should now point to new bucket
            uint64_t bit_mask = (1ULL << (new_local_depth - 1));
            uint32_t num_pointers = (1U << global_depth);

            for (uint32_t i = 0; i < num_pointers; i++)
            {
                if (dir->hdp_bucket_pointers[i] == bucket_page)
                {
                    // Check if this entry should now point to new bucket
                    if (i & bit_mask)
                    {
                        dir->hdp_bucket_pointers[i] = new_bucket_page;
                    }
                }
            }

            buffer_pool_->unpinPage(dir_page, true, ctx);

            return Status::OK;
        }

        // Redistribute entries between old and new bucket
        Status HashIndex::redistributeEntries(SBHashBucketPage* old_bucket,
                                             SBHashBucketPage* new_bucket,
                                             uint32_t new_local_depth, ErrorContext* ctx)
        {
            // Bit mask for the new depth bit
            uint64_t bit_mask = (1ULL << (new_local_depth - 1));

            // Temporary storage for entries
            std::vector<HashEntry> old_entries;
            std::vector<HashEntry> new_entries;

            // Collect all entries (including overflow pages)
            for (uint16_t i = 0; i < old_bucket->hbp_entry_count; i++)
            {
                const HashEntry& entry = old_bucket->hbp_entries[i];
                if (entry.he_tuple_id != 0)  // Not deleted
                {
                    if (entry.he_key_hash & bit_mask)
                    {
                        new_entries.push_back(entry);
                    }
                    else
                    {
                        old_entries.push_back(entry);
                    }
                }
            }

            // Clear old bucket
            old_bucket->hbp_entry_count = 0;
            old_bucket->hbp_deleted_count = 0;

            // Repopulate old bucket
            for (const auto& entry : old_entries)
            {
                if (old_bucket->hbp_entry_count < MAX_ENTRIES_PER_BUCKET)
                {
                    old_bucket->hbp_entries[old_bucket->hbp_entry_count++] = entry;
                }
            }

            // Populate new bucket
            for (const auto& entry : new_entries)
            {
                if (new_bucket->hbp_entry_count < MAX_ENTRIES_PER_BUCKET)
                {
                    new_bucket->hbp_entries[new_bucket->hbp_entry_count++] = entry;
                }
            }

            return Status::OK;
        }

        // Expand directory (double its size)
        Status HashIndex::expandDirectory(ErrorContext* ctx)
        {
            // Pin meta page
            uint8_t* meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, &meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);
            uint32_t old_global_depth = meta->hip_global_depth;
            uint32_t new_global_depth = old_global_depth + 1;

            if (new_global_depth > MAX_GLOBAL_DEPTH)
            {
                buffer_pool_->unpinPage(meta_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::LIMIT_EXCEEDED, "Maximum directory depth exceeded");
                return Status::LIMIT_EXCEEDED;
            }

            uint64_t dir_page = meta->hip_directory_page;

            // Pin directory page
            uint8_t* dir_data = nullptr;
            status = buffer_pool_->pinPage(dir_page, &dir_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(meta_page_, false, ctx);
                return status;
            }

            auto* dir = reinterpret_cast<SBHashDirectoryPage*>(dir_data);

            // Double the directory by duplicating each pointer
            uint32_t old_size = (1U << old_global_depth);
            uint32_t new_size = (1U << new_global_depth);

            // Copy pointers (second half mirrors first half)
            for (uint32_t i = 0; i < old_size; i++)
            {
                dir->hdp_bucket_pointers[old_size + i] = dir->hdp_bucket_pointers[i];
            }

            buffer_pool_->unpinPage(dir_page, true, ctx);

            // Update meta page
            meta->hip_global_depth = new_global_depth;
            buffer_pool_->unpinPage(meta_page_, true, ctx);

            return Status::OK;
        }

        // Find operation
        std::vector<uint64_t> HashIndex::find(const void* key_data, size_t key_len,
                                             ErrorContext* ctx)
        {
            std::vector<uint64_t> results;

            if (!key_data || key_len == 0)
            {
                return results;
            }

            // Calculate hash
            uint64_t hash = MurmurHash64(key_data, key_len);

            // Find bucket page
            uint64_t bucket_page = findBucketPageForKey(hash, ctx);
            if (bucket_page == 0)
            {
                return results;
            }

            // Scan bucket and overflow chain
            uint32_t current_page = bucket_page;
            while (current_page != 0)
            {
                uint8_t* page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, &page_data, ctx);
                if (status != Status::OK)
                {
                    break;
                }

                auto* bucket = reinterpret_cast<SBHashBucketPage*>(page_data);

                // Scan entries in this page
                for (uint16_t i = 0; i < bucket->hbp_entry_count; i++)
                {
                    const HashEntry& entry = bucket->hbp_entries[i];

                    // Check if hash matches and entry is not deleted
                    if (entry.he_key_hash == hash && entry.he_tuple_id != 0)
                    {
                        results.push_back(entry.he_tuple_id);
                    }
                }

                uint32_t next_page = bucket->hbp_overflow_page;
                buffer_pool_->unpinPage(current_page, false, ctx);
                current_page = next_page;
            }

            return results;
        }

        // Remove operation
        Status HashIndex::remove(const void* key_data, size_t key_len, uint64_t tuple_id,
                                ErrorContext* ctx)
        {
            if (!key_data || key_len == 0 || tuple_id == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid remove arguments");
                return Status::INVALID_ARGUMENT;
            }

            // Calculate hash
            uint64_t hash = MurmurHash64(key_data, key_len);

            // Find bucket page
            uint64_t bucket_page = findBucketPageForKey(hash, ctx);
            if (bucket_page == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Bucket not found");
                return Status::NOT_FOUND;
            }

            // Scan bucket and overflow chain
            uint32_t current_page = bucket_page;
            while (current_page != 0)
            {
                uint8_t* page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, &page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto* bucket = reinterpret_cast<SBHashBucketPage*>(page_data);
                bool found = false;

                // Search for matching entry
                for (uint16_t i = 0; i < bucket->hbp_entry_count; i++)
                {
                    HashEntry& entry = bucket->hbp_entries[i];

                    if (entry.he_key_hash == hash && entry.he_tuple_id == tuple_id)
                    {
                        // Mark as deleted
                        entry.he_tuple_id = 0;
                        bucket->hbp_deleted_count++;
                        found = true;

                        buffer_pool_->unpinPage(current_page, true, ctx);

                        // Update meta page statistics
                        uint8_t* meta_data = nullptr;
                        status = buffer_pool_->pinPage(meta_page_, &meta_data, ctx);
                        if (status == Status::OK)
                        {
                            auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);
                            meta->hip_num_deleted++;
                            buffer_pool_->unpinPage(meta_page_, true, ctx);
                        }

                        return Status::OK;
                    }
                }

                uint32_t next_page = bucket->hbp_overflow_page;
                buffer_pool_->unpinPage(current_page, false, ctx);

                if (found)
                {
                    return Status::OK;
                }

                current_page = next_page;
            }

            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Entry not found in hash index");
            return Status::NOT_FOUND;
        }

        // Vacuum operation - remove deleted entries and consolidate
        Status HashIndex::vacuum(ErrorContext* ctx)
        {
            // TODO: Implement comprehensive vacuum
            // For now, just a placeholder
            return Status::OK;
        }

        // Get statistics
        HashIndex::Statistics HashIndex::getStatistics(ErrorContext* ctx)
        {
            Statistics stats = {};

            // Pin meta page
            uint8_t* meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, &meta_data, ctx);
            if (status != Status::OK)
            {
                return stats;
            }

            auto* meta = reinterpret_cast<SBHashIndexMetaPage*>(meta_data);

            stats.num_tuples = meta->hip_num_tuples;
            stats.num_deleted = meta->hip_num_deleted;
            stats.global_depth = meta->hip_global_depth;
            stats.num_buckets = (1U << meta->hip_global_depth);

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Calculate derived statistics
            if (stats.num_buckets > 0)
            {
                stats.avg_entries_per_bucket =
                    static_cast<double>(stats.num_tuples) / stats.num_buckets;

                uint64_t max_entries = stats.num_buckets * MAX_ENTRIES_PER_BUCKET;
                stats.load_factor =
                    (static_cast<double>(stats.num_tuples) / max_entries) * 100.0;
            }

            stats.num_overflow_pages = 0; // TODO: Count overflow pages

            return stats;
        }

    } // namespace core
} // namespace scratchbird
