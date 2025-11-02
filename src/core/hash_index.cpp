#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/hash_functions.h"
#include "scratchbird/core/logger.h"
#include <cstring>
#include <algorithm>
#include <set>

namespace scratchbird
{
    namespace core
    {
        // Constructor
        HashIndex::HashIndex(Database *db, const UuidV7Bytes &index_uuid)
            : db_(db), buffer_pool_(db->buffer_pool()), index_uuid_(index_uuid), meta_page_(0)
        {
        }

        // Destructor
        HashIndex::~HashIndex() = default;

        // Create a new hash index
        Status HashIndex::create(Database *db, const UuidV7Bytes &index_uuid,
                                 uint32_t *meta_page_out, ErrorContext *ctx)
        {
            if (!db || !meta_page_out)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Invalid arguments to HashIndex::create");
                return Status::INVALID_ARGUMENT;
            }

            BufferPool *buffer_pool = db->buffer_pool();
            if (!buffer_pool)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Database has no buffer pool");
                return Status::INVALID_ARGUMENT;
            }

            // Step 1: Allocate meta page
            uint32_t meta_page = 0;
            Status status = db->page_manager()->allocatePage(meta_page, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            // Step 2: Pin and initialize meta page
            uint8_t *meta_page_data = nullptr;
            status = buffer_pool->pinPage(meta_page, (void **)&meta_page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_page_data);
            std::memset(meta, 0, sizeof(SBHashIndexMetaPage));

            // Initialize meta page header
            meta->hip_header.magic = K_MAGIC_SBRD;
            meta->hip_header.version = DB_VERSION_ALPHA_1_0_1;
            meta->hip_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_META);
            meta->hip_header.page_size = db->page_size();
            meta->hip_header.page_id = meta_page;

            // Initialize meta data
            std::memcpy(meta->hip_index_uuid, index_uuid.bytes.data(), 16);
            meta->hip_hash_func_id = HASH_FUNC_MURMUR3;
            meta->hip_global_depth = INITIAL_GLOBAL_DEPTH;
            meta->hip_num_tuples = 0;
            meta->hip_num_deleted = 0;

            // Step 3: Allocate directory page
            uint32_t dir_page = 0;
            status = db->page_manager()->allocatePage(dir_page, ctx);
            if (status != Status::OK)
            {
                buffer_pool->unpinPage(meta_page, false, ctx);
                return status;
            }

            meta->hip_directory_page = dir_page;

            // Step 4: Initialize directory page
            uint8_t *dir_page_data = nullptr;
            status = buffer_pool->pinPage(dir_page, (void **)&dir_page_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool->unpinPage(meta_page, false, ctx);
                return status;
            }

            auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_page_data);
            std::memset(dir, 0, sizeof(SBHashDirectoryPage));

            dir->hdp_header.magic = K_MAGIC_SBRD;
            dir->hdp_header.version = DB_VERSION_ALPHA_1_0_1;
            dir->hdp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_DIRECTORY);
            dir->hdp_header.page_size = db->page_size();
            dir->hdp_header.page_id = dir_page;
            dir->hdp_next_page = 0;

            // Step 5: Allocate initial bucket pages (2^INITIAL_GLOBAL_DEPTH buckets)
            uint32_t num_buckets = (1U << INITIAL_GLOBAL_DEPTH);
            for (uint32_t i = 0; i < num_buckets; i++)
            {
                uint32_t bucket_page = 0;
                status = db->page_manager()->allocatePage(bucket_page, ctx);
                if (status != Status::OK)
                {
                    buffer_pool->unpinPage(dir_page, false, ctx);
                    buffer_pool->unpinPage(meta_page, false, ctx);
                    return status;
                }

                // Initialize bucket page
                uint8_t *bucket_data = nullptr;
                status = buffer_pool->pinPage(bucket_page, (void **)&bucket_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool->unpinPage(dir_page, false, ctx);
                    buffer_pool->unpinPage(meta_page, false, ctx);
                    return status;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(bucket_data);
                std::memset(bucket, 0, sizeof(SBHashBucketPage));

                bucket->hbp_header.magic = K_MAGIC_SBRD;
                bucket->hbp_header.version = DB_VERSION_ALPHA_1_0_1;
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
        std::unique_ptr<HashIndex> HashIndex::open(Database *db, const UuidV7Bytes &index_uuid,
                                                   uint32_t meta_page, ErrorContext *ctx)
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
                return nullptr;
            }

            auto index = std::make_unique<HashIndex>(db, index_uuid);
            index->meta_page_ = meta_page;

            // Verify meta page
            uint8_t *meta_data = nullptr;
            Status status = db->buffer_pool()->pinPage(meta_page, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return nullptr;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            if (meta->hip_header.page_type != static_cast<uint16_t>(PageType::HASH_INDEX_META))
            {
                db->buffer_pool()->unpinPage(meta_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid hash index meta page");
                return nullptr;
            }

            if (std::memcmp(meta->hip_index_uuid, index_uuid.bytes.data(), 16) != 0)
            {
                db->buffer_pool()->unpinPage(meta_page, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Hash index UUID mismatch");
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
        uint64_t HashIndex::findBucketPageForKey(uint64_t hash, ErrorContext *ctx)
        {
            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return 0;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t dir_page = meta->hip_directory_page;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Calculate directory index
            uint32_t dir_index = getDirectoryIndex(hash, global_depth);

            // Pin directory page
            uint8_t *dir_data = nullptr;
            status = buffer_pool_->pinPage(dir_page, (void **)&dir_data, ctx);
            if (status != Status::OK)
            {
                return 0;
            }

            auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
            uint64_t bucket_page = dir->hdp_bucket_pointers[dir_index];

            buffer_pool_->unpinPage(dir_page, false, ctx);

            return bucket_page;
        }

        // Helper: Allocate a new bucket page
        Status HashIndex::allocateBucketPage(uint32_t *page_num_out, ErrorContext *ctx)
        {
            uint32_t page_num = 0;
            Status status = db_->page_manager()->allocatePage(page_num, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            uint8_t *page_data = nullptr;
            status = buffer_pool_->pinPage(page_num, (void **)&page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);
            std::memset(bucket, 0, sizeof(SBHashBucketPage));

            bucket->hbp_header.magic = K_MAGIC_SBRD;
            bucket->hbp_header.version = DB_VERSION_ALPHA_1_0_1;
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
        Status HashIndex::allocateOverflowPage(uint32_t *page_num_out, ErrorContext *ctx)
        {
            // Overflow pages are just bucket pages
            return allocateBucketPage(page_num_out, ctx);
        }

        // Helper: Check if bucket needs splitting
        bool HashIndex::bucketNeedsSplit(SBHashBucketPage *bucket)
        {
            uint16_t total_entries = bucket->hbp_entry_count - bucket->hbp_deleted_count;
            uint16_t capacity = MAX_ENTRIES_PER_BUCKET;
            uint16_t threshold = (capacity * BUCKET_FILL_THRESHOLD) / 100;
            return total_entries >= threshold;
        }

        // Helper: Count total entries in bucket including overflow chain
        uint16_t HashIndex::countEntriesInBucket(uint32_t bucket_page, ErrorContext *ctx)
        {
            uint16_t count = 0;
            uint32_t current_page = bucket_page;

            while (current_page != 0)
            {
                uint8_t *page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    break;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);
                count += (bucket->hbp_entry_count - bucket->hbp_deleted_count);
                uint32_t next_page = bucket->hbp_overflow_page;

                buffer_pool_->unpinPage(current_page, false, ctx);
                current_page = next_page;
            }

            return count;
        }

        // Insert operation
        // Firebird MGA: Sets xmin to creating transaction
        Status HashIndex::insert(const void *key_data, size_t key_len, const TID &tid,
                                 uint64_t xid, ErrorContext *ctx)
        {
            // PHASE 1.5: Convert TID to legacy format for storage
            uint64_t legacy_tid = convertTIDtoLegacy(tid);
            if (legacy_tid == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                  "Custom tablespace indexes not yet supported in ALPHA");
                return Status::NOT_IMPLEMENTED;
            }

            if (!key_data || key_len == 0)
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
                uint8_t *page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);

                // Check if there's space in this page
                if (bucket->hbp_entry_count < MAX_ENTRIES_PER_BUCKET)
                {
                    // Add entry (storing legacy format)
                    // Firebird MGA: Set xmin to creating transaction, xmax to 0 (not deleted)
                    HashEntry &entry = bucket->hbp_entries[bucket->hbp_entry_count];
                    entry.he_key_hash = hash;
                    entry.he_tuple_id = legacy_tid;
                    entry.he_xmin = xid;  // Transaction that created this entry
                    entry.he_xmax = 0;    // Not deleted
                    bucket->hbp_entry_count++;

                    buffer_pool_->unpinPage(current_page, true, ctx);

                    // Update meta page statistics
                    uint8_t *meta_data = nullptr;
                    status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
                    if (status == Status::OK)
                    {
                        auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
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
                    return insert(key_data, key_len, tid, xid, ctx);
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

            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "Insert failed unexpectedly");
            return Status::IO_ERROR;
        }

        // Split bucket operation
        Status HashIndex::splitBucket(uint32_t bucket_page, uint64_t hash, ErrorContext *ctx)
        {
            // Pin meta page to get global depth
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint32_t global_depth = meta->hip_global_depth;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Pin bucket page
            uint8_t *bucket_data = nullptr;
            status = buffer_pool_->pinPage(bucket_page, (void **)&bucket_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *old_bucket = reinterpret_cast<SBHashBucketPage *>(bucket_data);
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
            uint8_t *new_bucket_data = nullptr;
            status = buffer_pool_->pinPage(new_bucket_page, (void **)&new_bucket_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(bucket_page, false, ctx);
                return status;
            }

            auto *new_bucket = reinterpret_cast<SBHashBucketPage *>(new_bucket_data);

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
            status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint64_t dir_page = meta->hip_directory_page;
            global_depth = meta->hip_global_depth;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            uint8_t *dir_data = nullptr;
            status = buffer_pool_->pinPage(dir_page, (void **)&dir_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);

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
        Status HashIndex::redistributeEntries(SBHashBucketPage *old_bucket,
                                              SBHashBucketPage *new_bucket,
                                              uint32_t new_local_depth, ErrorContext *ctx)
        {
            // Bit mask for the new depth bit
            uint64_t bit_mask = (1ULL << (new_local_depth - 1));

            // Temporary storage for entries
            std::vector<HashEntry> old_entries;
            std::vector<HashEntry> new_entries;

            // Collect all entries (including overflow pages)
            for (uint16_t i = 0; i < old_bucket->hbp_entry_count; i++)
            {
                const HashEntry &entry = old_bucket->hbp_entries[i];
                if (entry.he_tuple_id != 0) // Not deleted
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
            for (const auto &entry : old_entries)
            {
                if (old_bucket->hbp_entry_count < MAX_ENTRIES_PER_BUCKET)
                {
                    old_bucket->hbp_entries[old_bucket->hbp_entry_count++] = entry;
                }
            }

            // Populate new bucket
            for (const auto &entry : new_entries)
            {
                if (new_bucket->hbp_entry_count < MAX_ENTRIES_PER_BUCKET)
                {
                    new_bucket->hbp_entries[new_bucket->hbp_entry_count++] = entry;
                }
            }

            return Status::OK;
        }

        // Expand directory (double its size)
        Status HashIndex::expandDirectory(ErrorContext *ctx)
        {
            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint32_t old_global_depth = meta->hip_global_depth;
            uint32_t new_global_depth = old_global_depth + 1;

            if (new_global_depth > MAX_GLOBAL_DEPTH)
            {
                buffer_pool_->unpinPage(meta_page_, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Maximum directory depth exceeded");
                return Status::PAGE_FULL;
            }

            uint64_t dir_page = meta->hip_directory_page;

            // Pin directory page
            uint8_t *dir_data = nullptr;
            status = buffer_pool_->pinPage(dir_page, (void **)&dir_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool_->unpinPage(meta_page_, false, ctx);
                return status;
            }

            auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);

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
        // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
        // Per MGA_RULES.md Rule 11: Uses TransactionId for visibility checks
        std::vector<TID> HashIndex::find(const void *key_data, size_t key_len,
                                         uint64_t current_xid,
                                         ErrorContext *ctx)
        {
            std::vector<TID> results;

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

            // Get transaction manager for TIP-based visibility checks
            TransactionManager *txn_mgr = db_->transaction_manager();

            // Scan bucket and overflow chain
            uint32_t current_page = bucket_page;
            while (current_page != 0)
            {
                uint8_t *page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    break;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);

                // Scan entries in this page
                for (uint16_t i = 0; i < bucket->hbp_entry_count; i++)
                {
                    const HashEntry &entry = bucket->hbp_entries[i];

                    // Check if hash matches and entry is not deleted
                    if (entry.he_key_hash == hash && entry.he_tuple_id != 0)
                    {
                        // Firebird MGA: Check visibility using TIP-based visibility (NOT snapshots)
                        // If current_xid is 0, return all entries (used by VACUUM)
                        bool visible = (current_xid == 0);

                        if (!visible && txn_mgr != nullptr)
                        {
                            // Own changes always visible
                            if (entry.he_xmin == current_xid)
                            {
                                visible = true;
                            }
                            // Check if creating transaction is visible
                            else if (txn_mgr->isVersionVisible(entry.he_xmin, current_xid))
                            {
                                // Entry is visible if not deleted OR deletion not yet visible
                                if (entry.he_xmax == 0 ||
                                    !txn_mgr->isVersionVisible(entry.he_xmax, current_xid))
                                {
                                    visible = true;
                                }
                            }
                        }

                        if (visible)
                        {
                            // Convert stored uint64_t to TID struct
                            TID tid = convertLegacyTID(entry.he_tuple_id);
                            results.push_back(tid);
                        }
                    }
                }

                uint32_t next_page = bucket->hbp_overflow_page;
                buffer_pool_->unpinPage(current_page, false, ctx);
                current_page = next_page;
            }

            return results;
        }

        // Remove operation
        // Firebird MGA: Uses soft delete (set xmax) instead of physical removal
        Status HashIndex::remove(const void *key_data, size_t key_len, const TID &tid,
                                 uint64_t xid, ErrorContext *ctx)
        {
            // PHASE 1.5: Convert TID to legacy format for comparison
            uint64_t legacy_tid = convertTIDtoLegacy(tid);
            if (legacy_tid == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::NOT_IMPLEMENTED,
                                  "Custom tablespace indexes not yet supported in ALPHA");
                return Status::NOT_IMPLEMENTED;
            }

            if (!key_data || key_len == 0)
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
                uint8_t *page_data = nullptr;
                Status status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);
                bool found = false;

                // Search for matching entry
                for (uint16_t i = 0; i < bucket->hbp_entry_count; i++)
                {
                    HashEntry &entry = bucket->hbp_entries[i];

                    if (entry.he_key_hash == hash && entry.he_tuple_id == legacy_tid)
                    {
                        // Firebird MGA: Soft delete - set xmax instead of physical removal
                        // This allows transactions to still see the entry until their snapshot
                        // Do NOT set he_tuple_id = 0 (that was PostgreSQL MVCC pattern)
                        entry.he_xmax = xid;  // Mark as deleted by this transaction
                        bucket->hbp_deleted_count++;
                        found = true;

                        buffer_pool_->unpinPage(current_page, true, ctx);

                        // Update meta page statistics
                        uint8_t *meta_data = nullptr;
                        status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
                        if (status == Status::OK)
                        {
                            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
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
        Status HashIndex::vacuum(ErrorContext *ctx)
        {
            // Pin meta page to get directory info
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t dir_page = meta->hip_directory_page;
            uint64_t deleted_before = meta->hip_num_deleted;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Pin directory page
            uint8_t *dir_data = nullptr;
            status = buffer_pool_->pinPage(dir_page, (void **)&dir_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
            uint32_t num_buckets = (1U << global_depth);

            // Track unique bucket pages (directory may have duplicates)
            std::vector<uint32_t> unique_buckets;
            for (uint32_t i = 0; i < num_buckets; i++)
            {
                uint32_t bucket_page = dir->hdp_bucket_pointers[i];

                // Check if we've already processed this bucket
                bool already_processed = false;
                for (uint32_t processed : unique_buckets)
                {
                    if (processed == bucket_page)
                    {
                        already_processed = true;
                        break;
                    }
                }

                if (!already_processed)
                {
                    unique_buckets.push_back(bucket_page);
                }
            }

            buffer_pool_->unpinPage(dir_page, false, ctx);

            // Vacuum each unique bucket
            uint64_t total_deleted_removed = 0;

            for (uint32_t bucket_page : unique_buckets)
            {
                // Vacuum this bucket and its overflow chain
                uint32_t current_page = bucket_page;

                while (current_page != 0)
                {
                    uint8_t *page_data = nullptr;
                    status = buffer_pool_->pinPage(current_page, (void **)&page_data, ctx);
                    if (status != Status::OK)
                    {
                        continue;
                    }

                    auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);

                    // Compact entries by removing deleted ones
                    if (bucket->hbp_deleted_count > 0)
                    {
                        uint16_t write_idx = 0;
                        for (uint16_t read_idx = 0; read_idx < bucket->hbp_entry_count; read_idx++)
                        {
                            const HashEntry &entry = bucket->hbp_entries[read_idx];

                            // Keep non-deleted entries
                            if (entry.he_tuple_id != 0)
                            {
                                if (write_idx != read_idx)
                                {
                                    bucket->hbp_entries[write_idx] = entry;
                                }
                                write_idx++;
                            }
                            else
                            {
                                total_deleted_removed++;
                            }
                        }

                        // Update counts
                        bucket->hbp_entry_count = write_idx;
                        bucket->hbp_deleted_count = 0;
                    }

                    uint32_t next_page = bucket->hbp_overflow_page;

                    // If this overflow page is now empty, we could free it
                    // For now, just mark it dirty
                    buffer_pool_->unpinPage(current_page, true, ctx);

                    current_page = next_page;
                }
            }

            // Update meta page with new deleted count
            status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status == Status::OK)
            {
                meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);

                // Reduce deleted count by amount removed
                if (total_deleted_removed <= meta->hip_num_deleted)
                {
                    meta->hip_num_deleted -= total_deleted_removed;
                }
                else
                {
                    meta->hip_num_deleted = 0;
                }

                buffer_pool_->unpinPage(meta_page_, true, ctx);
            }

            return Status::OK;
        }

        // Get statistics
        HashIndex::Statistics HashIndex::getStatistics(ErrorContext *ctx)
        {
            Statistics stats = {};

            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return stats;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);

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
                stats.load_factor = (static_cast<double>(stats.num_tuples) / max_entries) * 100.0;
            }

            stats.num_overflow_pages = 0; // TODO: Count overflow pages

            return stats;
        }

        // PHASE 2 TASK 2.3: IndexGCInterface implementation
        // Remove index entries pointing to dead tuples
        // PHASE 1.5 TASK 1.5.2b: Migrated to TID struct API
        Status HashIndex::removeDeadEntries(const std::vector<TID> &dead_tids,
                                            uint64_t *entries_removed_out,
                                            uint64_t *pages_modified_out,
                                            ErrorContext *ctx)
        {
            // Initialize output counters
            if (entries_removed_out != nullptr)
            {
                *entries_removed_out = 0;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = 0;
            }

            // Early exit if no dead TIDs
            if (dead_tids.empty())
            {
                return Status::OK;
            }

            // PHASE 1.5: Convert TID structs to legacy format for lookup
            // On-disk format still stores uint64_t, so we build a set of legacy TIDs
            std::set<uint64_t> dead_set;
            for (const TID &tid : dead_tids)
            {
                uint64_t legacy = convertTIDtoLegacy(tid);
                if (legacy != 0)  // Skip custom tablespace TIDs (not supported in ALPHA)
                {
                    dead_set.insert(legacy);
                }
            }

            if (!buffer_pool_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
                return Status::INVALID_ARGUMENT;
            }

            // Statistics
            uint64_t total_entries_removed = 0;
            uint64_t total_pages_modified = 0;
            uint64_t total_deleted_marked = 0;
            bool had_errors = false;

            // Load meta page to get directory info
            void *meta_buffer = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, &meta_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin meta page during GC");
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_buffer);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t directory_page = meta->hip_directory_page;
            uint64_t original_num_tuples = meta->hip_num_tuples;
            uint64_t original_num_deleted = meta->hip_num_deleted;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Calculate number of buckets
            uint32_t num_buckets = 1U << global_depth;

            // Strategy: Scan all bucket pages and overflow pages
            // We need to visit ALL buckets because hash index doesn't have
            // a sorted structure like B-Tree

            // Track visited bucket pages to avoid duplicates (due to directory aliasing)
            std::set<uint64_t> visited_buckets;

            // Load directory pages and collect unique bucket pages
            std::vector<uint64_t> bucket_pages;
            uint64_t current_dir_page = directory_page;

            while (current_dir_page != 0)
            {
                void *dir_buffer = nullptr;
                status = buffer_pool_->pinPage(current_dir_page, &dir_buffer, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(VACUUM, "Hash GC: Failed to pin directory page %lu: %d",
                                current_dir_page, static_cast<int>(status));
                    had_errors = true;
                    break;
                }

                auto *dir_page = reinterpret_cast<SBHashDirectoryPage *>(dir_buffer);
                uint64_t next_dir_page = dir_page->hdp_next_page;

                // Collect bucket pointers from this directory page
                uint32_t entries_in_this_page = std::min(num_buckets, 1015U);
                for (uint32_t i = 0; i < entries_in_this_page && bucket_pages.size() < num_buckets; i++)
                {
                    uint64_t bucket_page = dir_page->hdp_bucket_pointers[i];
                    if (bucket_page != 0 && visited_buckets.find(bucket_page) == visited_buckets.end())
                    {
                        visited_buckets.insert(bucket_page);
                        bucket_pages.push_back(bucket_page);
                    }
                }

                buffer_pool_->unpinPage(current_dir_page, false, ctx);
                current_dir_page = next_dir_page;
            }

            // Now scan all unique bucket pages (and their overflow chains)
            for (uint64_t bucket_page_num : bucket_pages)
            {
                uint64_t current_bucket = bucket_page_num;

                // Follow overflow chain for this bucket
                while (current_bucket != 0)
                {
                    void *bucket_buffer = nullptr;
                    status = buffer_pool_->pinPage(current_bucket, &bucket_buffer, ctx);
                    if (status != Status::OK)
                    {
                        LOG_WARNING(VACUUM, "Hash GC: Failed to pin bucket page %lu: %d",
                                    current_bucket, static_cast<int>(status));
                        had_errors = true;
                        break;
                    }

                    auto *bucket = reinterpret_cast<SBHashBucketPage *>(bucket_buffer);
                    uint16_t entry_count = bucket->hbp_entry_count;
                    uint64_t overflow_page = bucket->hbp_overflow_page;
                    uint32_t deleted_count = bucket->hbp_deleted_count;

                    bool page_modified = false;
                    uint32_t entries_removed_this_page = 0;

                    // Scan all entries on this bucket page
                    for (uint16_t i = 0; i < entry_count; i++)
                    {
                        HashEntry &entry = bucket->hbp_entries[i];

                        // Skip already deleted entries (he_tuple_id == 0)
                        if (entry.he_tuple_id == 0)
                        {
                            continue;
                        }

                        // Check if this tuple ID is in the dead set
                        if (dead_set.find(entry.he_tuple_id) != dead_set.end())
                        {
                            // Mark entry as deleted by setting tuple_id to 0
                            entry.he_tuple_id = 0;
                            entries_removed_this_page++;
                            deleted_count++;
                            page_modified = true;
                        }
                    }

                    // Update bucket's deleted count
                    if (page_modified)
                    {
                        bucket->hbp_deleted_count = deleted_count;
                        total_pages_modified++;
                        total_entries_removed += entries_removed_this_page;
                        total_deleted_marked += entries_removed_this_page;
                    }

                    // Unpin bucket page
                    buffer_pool_->unpinPage(current_bucket, page_modified, ctx);

                    // Move to overflow page
                    current_bucket = overflow_page;
                }
            }

            // Update meta page statistics
            if (total_deleted_marked > 0)
            {
                status = buffer_pool_->pinPage(meta_page_, &meta_buffer, ctx);
                if (status == Status::OK)
                {
                    meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_buffer);

                    // Decrease num_tuples and increase num_deleted
                    if (meta->hip_num_tuples >= total_deleted_marked)
                    {
                        meta->hip_num_tuples -= total_deleted_marked;
                    }
                    else
                    {
                        meta->hip_num_tuples = 0;
                    }

                    meta->hip_num_deleted += total_deleted_marked;

                    buffer_pool_->unpinPage(meta_page_, true, ctx);
                    total_pages_modified++; // Count meta page
                }
                else
                {
                    LOG_WARNING(VACUUM, "Hash GC: Failed to update meta page statistics: %d",
                                static_cast<int>(status));
                    had_errors = true;
                }
            }

            // Update output counters
            if (entries_removed_out != nullptr)
            {
                *entries_removed_out = total_entries_removed;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = total_pages_modified;
            }

            // Return appropriate status
            if (had_errors)
            {
                // Had some errors but may have removed some entries
                // Caller can check entries_removed_out to see progress
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

        // ============================================================================
        // PHASE 5 TASK 5.3.1: HASH INDEX TID UPDATES FOR TABLESPACE MIGRATION
        // ============================================================================

        /**
         * updateTIDsAfterMigration - Update TIDs in hash index after table migration
         *
         * This method scans all buckets (and overflow pages) in the hash index and
         * updates TIDs that reference migrated heap pages.
         *
         * Algorithm:
         * 1. Load meta page to get directory info and bucket count
         * 2. Load directory pages to collect unique bucket page numbers
         * 3. For each bucket:
         *    a. Follow overflow chain (hbp_overflow_page)
         *    b. Scan all entries in bucket page
         *    c. For each entry with he_tuple_id != 0:
         *       - Extract GPID from legacy TID
         *       - Check if GPID is in tid_mapping
         *       - If found, update with new GPID
         *    d. Mark page as dirty if any TIDs updated
         * 4. Return statistics
         *
         * @param tid_mapping Map of old GPID -> new GPID for migrated pages
         * @param tids_updated_out Output: Total number of TIDs updated
         * @param pages_modified_out Output: Total number of bucket pages modified
         * @param ctx Error context
         * @return Status::OK on success, error status otherwise
         */
        Status HashIndex::updateTIDsAfterMigration(
            const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
            uint64_t *tids_updated_out,
            uint64_t *pages_modified_out,
            ErrorContext *ctx)
        {
            // Initialize output counters
            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = 0;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = 0;
            }

            // Early exit if no TID mapping
            if (tid_mapping.empty())
            {
                return Status::OK;
            }

            if (!buffer_pool_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Buffer pool is null");
                return Status::INVALID_ARGUMENT;
            }

            // Statistics
            uint64_t total_tids_updated = 0;
            uint64_t total_pages_modified = 0;
            bool had_errors = false;

            // Load meta page to get directory info
            void *meta_buffer = nullptr;
            Status status = buffer_pool_->pinPage(meta_page_, &meta_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin meta page during TID update");
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_buffer);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t directory_page = meta->hip_directory_page;

            buffer_pool_->unpinPage(meta_page_, false, ctx);

            // Calculate number of buckets
            uint32_t num_buckets = 1U << global_depth;

            // Track visited bucket pages to avoid duplicates (due to directory aliasing)
            std::set<uint64_t> visited_buckets;

            // Load directory pages and collect unique bucket pages
            std::vector<uint64_t> bucket_pages;
            uint64_t current_dir_page = directory_page;

            while (current_dir_page != 0)
            {
                void *dir_buffer = nullptr;
                status = buffer_pool_->pinPage(current_dir_page, &dir_buffer, ctx);
                if (status != Status::OK)
                {
                    LOG_WARNING(CATALOG,
                               "Hash TID update: Failed to pin directory page %lu: %d",
                               current_dir_page, static_cast<int>(status));
                    had_errors = true;
                    break;
                }

                auto *dir_page = reinterpret_cast<SBHashDirectoryPage *>(dir_buffer);
                uint64_t next_dir_page = dir_page->hdp_next_page;

                // Collect bucket pointers from this directory page
                uint32_t entries_in_this_page = std::min(num_buckets, 1015U);
                for (uint32_t i = 0; i < entries_in_this_page && bucket_pages.size() < num_buckets; i++)
                {
                    uint64_t bucket_page = dir_page->hdp_bucket_pointers[i];
                    if (bucket_page != 0 && visited_buckets.find(bucket_page) == visited_buckets.end())
                    {
                        visited_buckets.insert(bucket_page);
                        bucket_pages.push_back(bucket_page);
                    }
                }

                buffer_pool_->unpinPage(current_dir_page, false, ctx);
                current_dir_page = next_dir_page;
            }

            // Now scan all unique bucket pages (and their overflow chains)
            for (uint64_t bucket_page_num : bucket_pages)
            {
                uint64_t current_bucket = bucket_page_num;

                // Follow overflow chain for this bucket
                while (current_bucket != 0)
                {
                    void *bucket_buffer = nullptr;
                    status = buffer_pool_->pinPage(current_bucket, &bucket_buffer, ctx);
                    if (status != Status::OK)
                    {
                        LOG_WARNING(CATALOG,
                                   "Hash TID update: Failed to pin bucket page %lu: %d",
                                   current_bucket, static_cast<int>(status));
                        had_errors = true;
                        break;
                    }

                    auto *bucket = reinterpret_cast<SBHashBucketPage *>(bucket_buffer);
                    uint16_t entry_count = bucket->hbp_entry_count;
                    uint64_t overflow_page = bucket->hbp_overflow_page;

                    bool page_modified = false;
                    uint32_t tids_updated_this_page = 0;

                    // Scan all entries on this bucket page
                    for (uint16_t i = 0; i < entry_count; i++)
                    {
                        HashEntry &entry = bucket->hbp_entries[i];

                        // Skip deleted entries (he_tuple_id == 0)
                        if (entry.he_tuple_id == 0)
                        {
                            continue;
                        }

                        // Extract GPID from legacy TID format
                        uint64_t legacy_tid = entry.he_tuple_id;
                        uint32_t page_id = static_cast<uint32_t>(legacy_tid >> 32);
                        uint16_t item_id = static_cast<uint16_t>((legacy_tid >> 16) & 0xFFFF);
                        GPID old_gpid = makeGPID(PRIMARY_TABLESPACE_ID, static_cast<uint64_t>(page_id));

                        // Check if this GPID was migrated
                        auto it = tid_mapping.find(old_gpid);
                        if (it != tid_mapping.end())
                        {
                            // Found! Update TID with new GPID
                            GPID new_gpid = it->second;

                            // Extract new page number from new GPID
                            uint64_t new_page_number = getPageNumber(TID(new_gpid, 0));
                            uint32_t new_page_id = static_cast<uint32_t>(new_page_number);

                            // Construct new legacy TID with updated page_id
                            uint64_t new_legacy_tid = (static_cast<uint64_t>(new_page_id) << 32) |
                                                     (static_cast<uint64_t>(item_id) << 16);

                            // Update the TID in-place
                            entry.he_tuple_id = new_legacy_tid;
                            tids_updated_this_page++;
                            page_modified = true;
                        }
                    }

                    // If we modified this page, update statistics
                    if (page_modified)
                    {
                        total_pages_modified++;
                        total_tids_updated += tids_updated_this_page;
                    }

                    // Unpin bucket page (mark dirty if modified)
                    buffer_pool_->unpinPage(current_bucket, page_modified, ctx);

                    // Move to overflow page
                    current_bucket = overflow_page;
                }
            }

            // Update output counters
            if (tids_updated_out != nullptr)
            {
                *tids_updated_out = total_tids_updated;
            }
            if (pages_modified_out != nullptr)
            {
                *pages_modified_out = total_pages_modified;
            }

            // Return appropriate status
            if (had_errors)
            {
                SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                                "Errors encountered during Hash index TID update");
                return Status::IO_ERROR;
            }

            return Status::OK;
        }

    } // namespace core
} // namespace scratchbird
