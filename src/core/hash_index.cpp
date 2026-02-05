/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/hash_index.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/buffer_pool.h"
#include "scratchbird/core/page_manager.h"
#include "scratchbird/core/hash_functions.h"
#include "scratchbird/core/transaction_manager.h"  // For isVersionVisible()
#include "scratchbird/core/logger.h"
#include "scratchbird/core/gpid.h"
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <set>
#include <thread>  // P2-5: For std::this_thread::yield()
#include <string>
#include <vector>

namespace scratchbird
{
    namespace core
    {
        namespace
        {
        } // namespace

        // Constructor
        HashIndex::HashIndex(Database *db, const UuidV7Bytes &index_uuid)
            : db_(db), buffer_pool_(db->buffer_pool()), index_uuid_(index_uuid), meta_page_(0)
        {
        }

        // Destructor
        HashIndex::~HashIndex() = default;

        // Dynamic capacity calculation
        uint16_t HashIndex::getMaxEntriesPerBucket() const
        {
            return (db_->page_size() - 96) / sizeof(HashEntry);
        }

        // Create a new hash index
        Status HashIndex::create(Database *db, const UuidV7Bytes &index_uuid,
                                 GPID meta_gpid, ErrorContext *ctx)
        {
            if (!db || meta_gpid == 0)
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

            uint16_t tablespace_id = getTablespaceID(meta_gpid);
            uint32_t meta_page = static_cast<uint32_t>(getPageNumber(meta_gpid));

            // Step 2: Pin and initialize meta page
            uint8_t *meta_page_data = nullptr;
            Status status = buffer_pool->pinPageGlobal(meta_gpid, (void **)&meta_page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_page_data);
            std::memset(meta, 0, sizeof(SBHashIndexMetaPage));

            // Initialize meta page header
            meta->hip_header.magic = K_MAGIC_SBRD;
            meta->hip_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
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
            GPID dir_gpid = 0;
            status = db->page_manager()->allocatePageInTablespace(tablespace_id, &dir_gpid, ctx);
            if (status != Status::OK)
            {
                buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
                return status;
            }

            dir_page = static_cast<uint32_t>(getPageNumber(dir_gpid));
            meta->hip_directory_page = dir_page;

            // Step 4: Initialize directory page
            uint8_t *dir_page_data = nullptr;
            status = buffer_pool->pinPageGlobal(dir_gpid, (void **)&dir_page_data, ctx);
            if (status != Status::OK)
            {
                buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
                return status;
            }

            auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_page_data);
            std::memset(dir, 0, sizeof(SBHashDirectoryPage));

            dir->hdp_header.magic = K_MAGIC_SBRD;
            dir->hdp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            dir->hdp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_DIRECTORY);
            dir->hdp_header.page_size = db->page_size();
            dir->hdp_header.page_id = dir_page;
            dir->hdp_next_page = 0;

            // Step 5: Allocate initial bucket pages (2^INITIAL_GLOBAL_DEPTH buckets)
            uint32_t num_buckets = (1U << INITIAL_GLOBAL_DEPTH);
            for (uint32_t i = 0; i < num_buckets; i++)
            {
                uint32_t bucket_page = 0;
                GPID bucket_gpid = 0;
                status = db->page_manager()->allocatePageInTablespace(tablespace_id, &bucket_gpid, ctx);
                if (status != Status::OK)
                {
                    buffer_pool->unpinPageGlobal(dir_gpid, false, ctx);
                    buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
                    return status;
                }

                bucket_page = static_cast<uint32_t>(getPageNumber(bucket_gpid));

                // Initialize bucket page
                uint8_t *bucket_data = nullptr;
                status = buffer_pool->pinPageGlobal(bucket_gpid, (void **)&bucket_data, ctx);
                if (status != Status::OK)
                {
                    buffer_pool->unpinPageGlobal(dir_gpid, false, ctx);
                    buffer_pool->unpinPageGlobal(meta_gpid, false, ctx);
                    return status;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(bucket_data);
                std::memset(bucket, 0, sizeof(SBHashBucketPage));

                bucket->hbp_header.magic = K_MAGIC_SBRD;
                bucket->hbp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                bucket->hbp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_BUCKET);
                bucket->hbp_header.page_size = db->page_size();
                bucket->hbp_header.page_id = bucket_page;
                bucket->hbp_entry_count = 0;
                bucket->hbp_local_depth = INITIAL_GLOBAL_DEPTH;
                bucket->hbp_deleted_count = 0;
                bucket->hbp_overflow_page = 0;

                buffer_pool->unpinPageGlobal(bucket_gpid, true, ctx);

                // Add to directory
                dir->hdp_bucket_pointers[i] = bucket_page;
            }

            // Unpin pages
            buffer_pool->unpinPageGlobal(dir_gpid, true, ctx);
            buffer_pool->unpinPageGlobal(meta_gpid, true, ctx);
            return Status::OK;
        }

        // Open an existing hash index
        std::unique_ptr<HashIndex> HashIndex::open(Database *db, const UuidV7Bytes &index_uuid,
                                                   GPID meta_gpid, ErrorContext *ctx)
        {
            if (!db)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid database");
                return nullptr;
            }

            auto index = std::make_unique<HashIndex>(db, index_uuid);
            index->meta_page_ = static_cast<uint32_t>(getPageNumber(meta_gpid));
            index->tablespace_id_ = getTablespaceID(meta_gpid);

            // Verify meta page
            uint8_t *meta_data = nullptr;
            Status status = db->buffer_pool()->pinPageGlobal(meta_gpid, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return nullptr;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            if (meta->hip_header.page_type != static_cast<uint16_t>(PageType::HASH_INDEX_META))
            {
                db->buffer_pool()->unpinPageGlobal(meta_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Invalid hash index meta page");
                return nullptr;
            }

            if (std::memcmp(meta->hip_index_uuid, index_uuid.bytes.data(), 16) != 0)
            {
                db->buffer_pool()->unpinPageGlobal(meta_gpid, false, ctx);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_CORRUPT, "Hash index UUID mismatch");
                return nullptr;
            }

            // P2-5: Initialize cached directory info for concurrent access
            index->cached_global_depth_.store(meta->hip_global_depth, std::memory_order_relaxed);
            index->cached_directory_page_.store(meta->hip_directory_page, std::memory_order_relaxed);

            db->buffer_pool()->unpinPageGlobal(meta_gpid, false, ctx);

            return index;
        }

        GPID HashIndex::indexGPID(uint64_t page_num) const
        {
            return makeGPID(tablespace_id_, page_num);
        }

        Status HashIndex::pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx)
        {
            return buffer_pool_->pinPageGlobal(indexGPID(page_num), buffer, ctx);
        }

        Status HashIndex::unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx)
        {
            return buffer_pool_->unpinPageGlobal(indexGPID(page_num), dirty, ctx);
        }

        // Helper: Calculate directory index from hash value
        uint32_t HashIndex::getDirectoryIndex(uint64_t hash, uint32_t global_depth)
        {
            // Use the first 'global_depth' bits of the hash
            return hash & ((1U << global_depth) - 1);
        }

        // Helper: Find bucket page for a given hash value
        // P2-5: Uses cached directory info and shared lock for concurrent reads
        uint64_t HashIndex::findBucketPageForKey(uint64_t hash, ErrorContext *ctx)
        {
            // P2-5: Use shared lock to allow concurrent reads during resize
            std::shared_lock<std::shared_mutex> lock(directory_mutex_);

            // Use cached directory info (fast path - no meta page pin needed)
            uint32_t global_depth = cached_global_depth_.load(std::memory_order_acquire);
            uint64_t dir_page = cached_directory_page_.load(std::memory_order_acquire);

            // If cache not initialized, refresh from meta page
            if (global_depth == 0 || dir_page == 0)
            {
                lock.unlock();
                refreshCachedDirectoryInfo(ctx);
                lock.lock();
                global_depth = cached_global_depth_.load(std::memory_order_acquire);
                dir_page = cached_directory_page_.load(std::memory_order_acquire);
            }

            // Calculate directory index
            uint32_t dir_index = getDirectoryIndex(hash, global_depth);
            size_t pointers_per_page =
                (db_->page_size() - sizeof(SBHashDirectoryPage)) / sizeof(uint64_t);
            if (pointers_per_page == 0)
            {
                return 0;
            }

            uint32_t page_offset = dir_index / static_cast<uint32_t>(pointers_per_page);
            uint32_t entry_offset = dir_index % static_cast<uint32_t>(pointers_per_page);

            uint64_t target_dir_page = dir_page;
            for (uint32_t i = 0; i < page_offset; i++)
            {
                uint8_t *dir_data = nullptr;
                Status status = pinIndexPage(target_dir_page, (void **)&dir_data, ctx);
                if (status != Status::OK)
                {
                    return 0;
                }

                auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
                uint64_t next_page = dir->hdp_next_page;
                unpinIndexPage(target_dir_page, false, ctx);

                if (next_page == 0)
                {
                    return 0;
                }
                target_dir_page = next_page;
            }

            // Pin directory page
            uint8_t *dir_data = nullptr;
            Status status = pinIndexPage(target_dir_page, (void **)&dir_data, ctx);
            if (status != Status::OK)
            {
                return 0;
            }

            auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
            uint64_t bucket_page = dir->hdp_bucket_pointers[entry_offset];

            unpinIndexPage(target_dir_page, false, ctx);

            return bucket_page;
        }

        // P2-5: Helper to refresh cached directory info from meta page
        void HashIndex::refreshCachedDirectoryInfo(ErrorContext *ctx)
        {
            uint8_t *meta_data = nullptr;
            Status status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            cached_global_depth_.store(meta->hip_global_depth, std::memory_order_release);
            cached_directory_page_.store(meta->hip_directory_page, std::memory_order_release);

            unpinIndexPage(meta_page_, false, ctx);
        }

        // Helper: Allocate a new bucket page
        Status HashIndex::allocateBucketPage(uint32_t *page_num_out, ErrorContext *ctx)
        {
            uint32_t page_num = 0;
            GPID page_gpid = 0;
            Status status = db_->page_manager()->allocatePageInTablespace(tablespace_id_, &page_gpid, ctx);
            if (status != Status::OK)
            {
                return status;
            }
            page_num = static_cast<uint32_t>(getPageNumber(page_gpid));

            uint8_t *page_data = nullptr;
            status = pinIndexPage(page_num, (void **)&page_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);
            std::memset(bucket, 0, sizeof(SBHashBucketPage));

            bucket->hbp_header.magic = K_MAGIC_SBRD;
            bucket->hbp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
            bucket->hbp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_BUCKET);
            bucket->hbp_header.page_size = db_->page_size();
            bucket->hbp_header.page_id = page_num;
            bucket->hbp_entry_count = 0;
            bucket->hbp_local_depth = 0; // Will be set by caller
            bucket->hbp_deleted_count = 0;
            bucket->hbp_overflow_page = 0;

            unpinIndexPage(page_num, true, ctx);

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
            uint16_t capacity = getMaxEntriesPerBucket();
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
                Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    break;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);
                count += (bucket->hbp_entry_count - bucket->hbp_deleted_count);
                uint32_t next_page = bucket->hbp_overflow_page;

                unpinIndexPage(current_page, false, ctx);
                current_page = next_page;
            }

            return count;
        }

        // Insert operation
        // Firebird MGA: Sets xmin to creating transaction
        Status HashIndex::insert(const void *key_data, size_t key_len, const TID &tid,
                                 uint64_t xid, ErrorContext *ctx)
        {
            // PHASE 1.5: Now supports custom tablespaces via TID (GPID + slot)
            if (!key_data || key_len == 0 || !isValidGPID(tid.gpid) || tid.slot == 0)
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
                Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);

                // Check if there's space in this page
                if (bucket->hbp_entry_count < getMaxEntriesPerBucket())
                {
                    // Add entry (storing TID with GPID support)
                    // Firebird MGA: Set xmin to creating transaction, xmax to 0 (not deleted)
                    HashEntry &entry = bucket->hbp_entries[bucket->hbp_entry_count];
                    entry.he_key_hash = hash;
                    entry.setTID(tid);        // Store full TID (GPID + slot)
                    entry.he_padding = 0;     // Clear padding
                    entry.he_xmin = xid;      // Transaction that created this entry
                    entry.he_xmax = 0;        // Not deleted
                    bucket->hbp_entry_count++;

                    unpinIndexPage(current_page, true, ctx);

                    // Update meta page statistics
                    uint8_t *meta_data = nullptr;
                    status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
                    if (status == Status::OK)
                    {
                        auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
                        meta->hip_num_tuples++;
                        unpinIndexPage(meta_page_, true, ctx);
                    }

                    if (bloom_filter_)
                    {
                        Status bf_status = bloom_filter_->insert(&hash, sizeof(hash), ctx);
                        if (bf_status != Status::OK)
                        {
                            LOG_WARNING(STORAGE, "Hash bloom filter insert failed: %d",
                                        static_cast<int>(bf_status));
                        }
                    }

                    return Status::OK;
                }

                // Check if there's an overflow page
                if (bucket->hbp_overflow_page != 0)
                {
                    uint32_t next_page = bucket->hbp_overflow_page;
                    unpinIndexPage(current_page, false, ctx);
                    current_page = next_page;
                    continue;
                }

                // No overflow page and bucket is full - need to split or add overflow
                // First check if we should split
                if (bucketNeedsSplit(bucket))
                {
                    unpinIndexPage(current_page, false, ctx);

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
                        unpinIndexPage(current_page, false, ctx);
                        return status;
                    }

                    bucket->hbp_overflow_page = overflow_page;
                    unpinIndexPage(current_page, true, ctx);

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
            Status status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint32_t global_depth = meta->hip_global_depth;

            unpinIndexPage(meta_page_, false, ctx);

            // Pin bucket page
            uint8_t *bucket_data = nullptr;
            status = pinIndexPage(bucket_page, (void **)&bucket_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *old_bucket = reinterpret_cast<SBHashBucketPage *>(bucket_data);
            uint32_t local_depth = old_bucket->hbp_local_depth;
            LOG_DEBUG(HASH,
                      "Hash split begin bucket=%u hash=%llu local_depth=%u global_depth=%u entries=%u deleted=%u overflow=%u",
                      bucket_page, static_cast<unsigned long long>(hash), local_depth, global_depth,
                      old_bucket->hbp_entry_count, old_bucket->hbp_deleted_count,
                      static_cast<uint32_t>(old_bucket->hbp_overflow_page));

            if (local_depth > global_depth)
            {
                LOG_ERROR(HASH, "Hash split invariant failed: local_depth=%u > global_depth=%u bucket=%u",
                          local_depth, global_depth, bucket_page);
                unpinIndexPage(bucket_page, false, ctx);
                return Status::INDEX_CORRUPTED;
            }

            // Check if we need directory expansion
            if (local_depth >= global_depth)
            {
                unpinIndexPage(bucket_page, false, ctx);

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
                unpinIndexPage(bucket_page, false, ctx);
                return status;
            }

            // Pin new bucket
            uint8_t *new_bucket_data = nullptr;
            status = pinIndexPage(new_bucket_page, (void **)&new_bucket_data, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(bucket_page, false, ctx);
                return status;
            }

            auto *new_bucket = reinterpret_cast<SBHashBucketPage *>(new_bucket_data);

            // Increment local depth for both buckets
            uint32_t new_local_depth = local_depth + 1;
            old_bucket->hbp_local_depth = new_local_depth;
            new_bucket->hbp_local_depth = new_local_depth;

            // Update directory pointers
            // Pin directory page
            status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(new_bucket_page, false, ctx);
                unpinIndexPage(bucket_page, false, ctx);
                return status;
            }

            meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint64_t dir_page = meta->hip_directory_page;
            global_depth = meta->hip_global_depth;

            unpinIndexPage(meta_page_, false, ctx);

            // Update directory entries that should now point to new bucket
            uint64_t bit_mask = (1ULL << (new_local_depth - 1));
            uint32_t num_pointers = (1U << global_depth);
            size_t pointers_per_page =
                (db_->page_size() - sizeof(SBHashDirectoryPage)) / sizeof(uint64_t);
            if (pointers_per_page == 0)
            {
                return Status::IO_ERROR;
            }

            uint64_t current_dir_page = dir_page;
            uint32_t processed = 0;
            uint32_t updated_pointers = 0;
            while (current_dir_page != 0 && processed < num_pointers)
            {
                uint8_t *dir_data = nullptr;
                status = pinIndexPage(current_dir_page, (void **)&dir_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
                uint32_t entries_this_page =
                    static_cast<uint32_t>(std::min<size_t>(pointers_per_page, num_pointers - processed));
                bool page_dirty = false;

                for (uint32_t i = 0; i < entries_this_page; i++)
                {
                    uint32_t global_index = processed + i;
                    if (dir->hdp_bucket_pointers[i] == bucket_page)
                    {
                        if (global_index & bit_mask)
                        {
                            dir->hdp_bucket_pointers[i] = new_bucket_page;
                            page_dirty = true;
                            updated_pointers++;
                        }
                    }
                }

                uint64_t next_page = dir->hdp_next_page;
                unpinIndexPage(current_dir_page, page_dirty, ctx);
                current_dir_page = next_page;
                processed += entries_this_page;
            }
            LOG_DEBUG(HASH,
                      "Hash split directory update bucket=%u new_bucket=%u new_local_depth=%u global_depth=%u updated=%u/%u",
                      bucket_page, new_bucket_page, new_local_depth, global_depth, updated_pointers,
                      num_pointers);

            // Redistribute entries (after directory update)
            status = redistributeEntries(old_bucket, new_bucket, new_local_depth, ctx);
            if (status != Status::OK)
            {
                unpinIndexPage(new_bucket_page, false, ctx);
                unpinIndexPage(bucket_page, false, ctx);
                return status;
            }

            unpinIndexPage(new_bucket_page, true, ctx);
            unpinIndexPage(bucket_page, true, ctx);

            LOG_DEBUG(HASH, "Hash split complete bucket=%u new_bucket=%u", bucket_page, new_bucket_page);
            return Status::OK;
        }

        // Redistribute entries between old and new bucket
        Status HashIndex::redistributeEntries(SBHashBucketPage *old_bucket,
                                              SBHashBucketPage *new_bucket,
                                              uint32_t new_local_depth, ErrorContext *ctx)
        {
            const uint16_t max_entries = getMaxEntriesPerBucket();
            uint64_t bit_mask = (1ULL << (new_local_depth - 1));
            uint32_t total_before = 0;

            // Temporary storage for entries
            std::vector<HashEntry> old_entries;
            std::vector<HashEntry> new_entries;

            auto is_deleted_entry = [](const HashEntry &entry) -> bool
            {
                return entry.he_xmax != 0 || entry.getTID() == INVALID_TID;
            };

            // Collect all entries from the primary bucket
            for (uint16_t i = 0; i < old_bucket->hbp_entry_count; i++)
            {
                const HashEntry &entry = old_bucket->hbp_entries[i];
                if (entry.he_key_hash & bit_mask)
                {
                    new_entries.push_back(entry);
                }
                else
                {
                    old_entries.push_back(entry);
                }
                total_before++;
            }

            // Collect entries from overflow chain
            std::vector<uint32_t> overflow_pages;
            uint32_t overflow_page = static_cast<uint32_t>(old_bucket->hbp_overflow_page);
            while (overflow_page != 0)
            {
                uint8_t *overflow_data = nullptr;
                Status status = pinIndexPage(overflow_page, (void **)&overflow_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *overflow_bucket = reinterpret_cast<SBHashBucketPage *>(overflow_data);
                for (uint16_t i = 0; i < overflow_bucket->hbp_entry_count; i++)
                {
                    const HashEntry &entry = overflow_bucket->hbp_entries[i];
                    if (entry.he_key_hash & bit_mask)
                    {
                        new_entries.push_back(entry);
                    }
                    else
                    {
                        old_entries.push_back(entry);
                    }
                    total_before++;
                }

                uint32_t next = static_cast<uint32_t>(overflow_bucket->hbp_overflow_page);
                unpinIndexPage(overflow_page, false, ctx);
                overflow_pages.push_back(overflow_page);
                overflow_page = next;
            }

            if (total_before != (old_entries.size() + new_entries.size()))
            {
                LOG_ERROR(HASH,
                          "Hash redistribute invariant failed: total_before=%u old_entries=%zu new_entries=%zu",
                          total_before, old_entries.size(), new_entries.size());
                return Status::INDEX_CORRUPTED;
            }

            // Clear old bucket
            old_bucket->hbp_entry_count = 0;
            old_bucket->hbp_deleted_count = 0;
            old_bucket->hbp_overflow_page = 0;

            // Clear new bucket
            new_bucket->hbp_entry_count = 0;
            new_bucket->hbp_deleted_count = 0;
            new_bucket->hbp_overflow_page = 0;

            uint32_t old_page_id = old_bucket->hbp_header.page_id;
            uint32_t new_page_id = new_bucket->hbp_header.page_id;

            auto append_to_page = [&](uint32_t target_page, const HashEntry &entry) -> Status
            {
                uint32_t current_page = target_page;
                while (current_page != 0)
                {
                    SBHashBucketPage *bucket = nullptr;
                    bool pinned_here = false;
                    if (current_page == old_page_id)
                    {
                        bucket = old_bucket;
                    }
                    else if (current_page == new_page_id)
                    {
                        bucket = new_bucket;
                    }
                    else
                    {
                        uint8_t *page_data = nullptr;
                        Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
                        if (status != Status::OK)
                        {
                            return status;
                        }
                        bucket = reinterpret_cast<SBHashBucketPage *>(page_data);
                        pinned_here = true;
                    }

                    if (bucket->hbp_entry_count < max_entries)
                    {
                        bucket->hbp_entries[bucket->hbp_entry_count++] = entry;
                        if (is_deleted_entry(entry))
                        {
                            bucket->hbp_deleted_count++;
                        }
                        if (pinned_here)
                        {
                            unpinIndexPage(current_page, true, ctx);
                        }
                        return Status::OK;
                    }

                    uint32_t next_page = static_cast<uint32_t>(bucket->hbp_overflow_page);
                    if (next_page == 0)
                    {
                        Status alloc_status = allocateOverflowPage(&next_page, ctx);
                        if (alloc_status != Status::OK)
                        {
                            if (pinned_here)
                            {
                                unpinIndexPage(current_page, false, ctx);
                            }
                            return alloc_status;
                        }
                        bucket->hbp_overflow_page = next_page;
                        if (pinned_here)
                        {
                            unpinIndexPage(current_page, true, ctx);
                        }
                        current_page = next_page;
                        continue;
                    }

                    if (pinned_here)
                    {
                        unpinIndexPage(current_page, false, ctx);
                    }
                    current_page = next_page;
                }

                return Status::IO_ERROR;
            };

            for (const auto &entry : old_entries)
            {
                Status status = append_to_page(old_page_id, entry);
                if (status != Status::OK)
                {
                    return status;
                }
            }

            for (const auto &entry : new_entries)
            {
                Status status = append_to_page(new_page_id, entry);
                if (status != Status::OK)
                {
                    return status;
                }
            }

            auto count_chain_entries = [&](uint32_t start_page, uint32_t *out_count) -> Status
            {
                if (out_count == nullptr)
                {
                    return Status::INVALID_ARGUMENT;
                }
                uint32_t total = 0;
                uint32_t current_page = start_page;
                while (current_page != 0)
                {
                    SBHashBucketPage *bucket = nullptr;
                    bool pinned_here = false;
                    if (current_page == old_page_id)
                    {
                        bucket = old_bucket;
                    }
                    else if (current_page == new_page_id)
                    {
                        bucket = new_bucket;
                    }
                    else
                    {
                        uint8_t *page_data = nullptr;
                        Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
                        if (status != Status::OK)
                        {
                            return status;
                        }
                        bucket = reinterpret_cast<SBHashBucketPage *>(page_data);
                        pinned_here = true;
                    }

                    if (bucket->hbp_entry_count > max_entries)
                    {
                        LOG_ERROR(HASH, "Hash redistribute invariant failed: page=%u entry_count=%u > max=%u",
                                  current_page, bucket->hbp_entry_count, max_entries);
                        if (pinned_here)
                        {
                            unpinIndexPage(current_page, false, ctx);
                        }
                        return Status::INDEX_CORRUPTED;
                    }

                    total += bucket->hbp_entry_count;
                    uint32_t next_page = static_cast<uint32_t>(bucket->hbp_overflow_page);
                    if (pinned_here)
                    {
                        unpinIndexPage(current_page, false, ctx);
                    }
                    current_page = next_page;
                }
                *out_count = total;
                return Status::OK;
            };

            uint32_t old_after = 0;
            uint32_t new_after = 0;
            Status count_status = count_chain_entries(old_page_id, &old_after);
            if (count_status != Status::OK)
            {
                return count_status;
            }
            count_status = count_chain_entries(new_page_id, &new_after);
            if (count_status != Status::OK)
            {
                return count_status;
            }

            if (old_after != old_entries.size() || new_after != new_entries.size())
            {
                LOG_ERROR(HASH,
                          "Hash redistribute invariant failed: old_after=%u old_entries=%zu new_after=%u new_entries=%zu",
                          old_after, old_entries.size(), new_after, new_entries.size());
                return Status::INDEX_CORRUPTED;
            }

            // Free overflow pages from the old bucket chain
            auto page_mgr = db_->page_manager();
            if (page_mgr)
            {
                for (uint32_t page_id : overflow_pages)
                {
                    page_mgr->freePageGlobal(indexGPID(page_id), ctx);
                }
            }
            return Status::OK;
        }

        // Expand directory (double its size)
        // P2-5: Now uses concurrent resize - allocates new space first, then swaps atomically
        Status HashIndex::expandDirectory(ErrorContext *ctx)
        {
            // Delegate to concurrent version
            return expandDirectoryConcurrent(ctx);
        }

        // P2-5: Concurrent directory expansion
        // This version minimizes write blocking by:
        // 1. Allocating new directory pages outside the critical section
        // 2. Copying existing pointers without blocking readers
        // 3. Using exclusive lock only for the atomic swap
        Status HashIndex::expandDirectoryConcurrent(ErrorContext *ctx)
        {
            // Set resize flag to let readers know expansion is in progress
            bool expected = false;
            if (!resize_in_progress_.compare_exchange_strong(expected, true))
            {
                // Another thread is already expanding - wait and retry
                while (resize_in_progress_.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                // After other expansion completes, the bucket might not need split anymore
                return Status::OK;
            }

            // Read current state from meta page (without exclusive lock)
            uint8_t *meta_data = nullptr;
            Status status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                resize_in_progress_.store(false, std::memory_order_release);
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint32_t old_global_depth = meta->hip_global_depth;
            uint32_t new_global_depth = old_global_depth + 1;

            if (new_global_depth > MAX_GLOBAL_DEPTH)
            {
                unpinIndexPage(meta_page_, false, ctx);
                resize_in_progress_.store(false, std::memory_order_release);
                SET_ERROR_CONTEXT(ctx, Status::PAGE_FULL, "Maximum directory depth exceeded");
                return Status::PAGE_FULL;
            }

            uint64_t old_dir_page = meta->hip_directory_page;
            unpinIndexPage(meta_page_, false, ctx);
            // Calculate sizes
            uint32_t old_size = (1U << old_global_depth);
            uint32_t new_size = (1U << new_global_depth);

            // Check if we need additional directory pages
            // Each directory page can hold approximately (page_size - 72) / 8 pointers
            size_t pointers_per_page = (db_->page_size() - sizeof(SBHashDirectoryPage)) / sizeof(uint64_t);

            // For simplicity, if new size fits in one page, expand in place
            // Otherwise, allocate new page(s)
            if (new_size <= pointers_per_page)
            {
                // In-place expansion (fits in single page)
                // Take exclusive lock for the actual modification
                std::unique_lock<std::shared_mutex> lock(directory_mutex_);

                // Re-read meta page with lock held
                status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
                if (status != Status::OK)
                {
                    resize_in_progress_.store(false, std::memory_order_release);
                    return status;
                }

                meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);

                // Check if another thread already expanded
                if (meta->hip_global_depth >= new_global_depth)
                {
                    unpinIndexPage(meta_page_, false, ctx);
                    resize_in_progress_.store(false, std::memory_order_release);
                    // Update cache
                    cached_global_depth_.store(meta->hip_global_depth, std::memory_order_release);
                    return Status::OK;
                }

                old_dir_page = meta->hip_directory_page;

                // Pin directory page for modification
                uint8_t *dir_data = nullptr;
                status = pinIndexPage(old_dir_page, (void **)&dir_data, ctx);
                if (status != Status::OK)
                {
                    unpinIndexPage(meta_page_, false, ctx);
                    resize_in_progress_.store(false, std::memory_order_release);
                    return status;
                }

                auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);

                // Double the directory by duplicating each pointer (second half mirrors first half).
                // New index i maps to old index i, and i + old_size maps to the same bucket.
                for (uint32_t i = 0; i < old_size; i++)
                {
                    dir->hdp_bucket_pointers[i + old_size] = dir->hdp_bucket_pointers[i];
                }

                unpinIndexPage(old_dir_page, true, ctx);

                // Update meta page
                meta->hip_global_depth = new_global_depth;
                unpinIndexPage(meta_page_, true, ctx);

                // Update cache atomically
                cached_global_depth_.store(new_global_depth, std::memory_order_release);

                if (lock.owns_lock())
                {
                    lock.unlock();
                }

                resize_in_progress_.store(false, std::memory_order_release);
                return Status::OK;
            }
            else
            {
                // Multi-page directory - expand across as many pages as needed
                std::unique_lock<std::shared_mutex> lock(directory_mutex_);

                // Re-read meta page with lock held
                status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
                if (status != Status::OK)
                {
                    resize_in_progress_.store(false, std::memory_order_release);
                    return status;
                }

                meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);

                // Check if another thread already expanded
                if (meta->hip_global_depth >= new_global_depth)
                {
                    unpinIndexPage(meta_page_, false, ctx);
                    resize_in_progress_.store(false, std::memory_order_release);
                    cached_global_depth_.store(meta->hip_global_depth, std::memory_order_release);
                    return Status::OK;
                }

                old_dir_page = meta->hip_directory_page;

                // Load existing directory page chain
                std::vector<uint32_t> dir_pages;
                uint64_t current_dir_page = old_dir_page;
                while (current_dir_page != 0)
                {
                    dir_pages.push_back(static_cast<uint32_t>(current_dir_page));
                    uint8_t *dir_data = nullptr;
                    status = pinIndexPage(current_dir_page, (void **)&dir_data, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(meta_page_, false, ctx);
                        resize_in_progress_.store(false, std::memory_order_release);
                        return status;
                    }
                    auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
                    uint64_t next_page = dir->hdp_next_page;
                    unpinIndexPage(current_dir_page, false, ctx);
                    current_dir_page = next_page;
                }

                // Ensure enough pages for new_size
                size_t pages_needed = (new_size + pointers_per_page - 1) / pointers_per_page;
                if (pages_needed == 0)
                {
                    unpinIndexPage(meta_page_, false, ctx);
                    resize_in_progress_.store(false, std::memory_order_release);
                    return Status::IO_ERROR;
                }

                while (dir_pages.size() < pages_needed)
                {
                    uint32_t new_dir_page_num = 0;
                    GPID new_dir_gpid = 0;
                    status = db_->page_manager()->allocatePageInTablespace(tablespace_id_, &new_dir_gpid, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(meta_page_, false, ctx);
                        resize_in_progress_.store(false, std::memory_order_release);
                        return status;
                    }
                    new_dir_page_num = static_cast<uint32_t>(getPageNumber(new_dir_gpid));

                    uint8_t *new_dir_data = nullptr;
                    status = pinIndexPage(new_dir_page_num, (void **)&new_dir_data, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(meta_page_, false, ctx);
                        resize_in_progress_.store(false, std::memory_order_release);
                        return status;
                    }

                    auto *new_dir = reinterpret_cast<SBHashDirectoryPage *>(new_dir_data);
                    std::memset(new_dir, 0, sizeof(SBHashDirectoryPage));
                    new_dir->hdp_header.magic = K_MAGIC_SBRD;
                    new_dir->hdp_header.version = static_cast<uint16_t>(DB_VERSION_ALPHA_1_0_1 & 0xFFFF);
                    new_dir->hdp_header.page_type = static_cast<uint16_t>(PageType::HASH_INDEX_DIRECTORY);
                    new_dir->hdp_header.page_size = db_->page_size();
                    new_dir->hdp_header.page_id = new_dir_page_num;
                    new_dir->hdp_next_page = 0;

                    unpinIndexPage(new_dir_page_num, true, ctx);
                    dir_pages.push_back(new_dir_page_num);
                }

                // Link pages in the chain
                for (size_t i = 0; i < dir_pages.size(); i++)
                {
                    uint32_t page_id = dir_pages[i];
                    uint64_t next_page = (i + 1 < dir_pages.size()) ? dir_pages[i + 1] : 0;
                    uint8_t *dir_data = nullptr;
                    status = pinIndexPage(page_id, (void **)&dir_data, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(meta_page_, false, ctx);
                        resize_in_progress_.store(false, std::memory_order_release);
                        return status;
                    }
                    auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
                    dir->hdp_next_page = next_page;
                    unpinIndexPage(page_id, true, ctx);
                }

                // Read old directory pointers
                std::vector<uint64_t> old_pointers;
                old_pointers.reserve(old_size);
                uint32_t collected = 0;
                for (size_t page_idx = 0; page_idx < dir_pages.size() && collected < old_size; page_idx++)
                {
                    uint8_t *dir_data = nullptr;
                    status = pinIndexPage(dir_pages[page_idx], (void **)&dir_data, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(meta_page_, false, ctx);
                        resize_in_progress_.store(false, std::memory_order_release);
                        return status;
                    }

                    auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
                    uint32_t entries_this_page =
                        static_cast<uint32_t>(std::min<size_t>(pointers_per_page, old_size - collected));

                    for (uint32_t i = 0; i < entries_this_page; i++)
                    {
                        old_pointers.push_back(dir->hdp_bucket_pointers[i]);
                    }

                    unpinIndexPage(dir_pages[page_idx], false, ctx);
                    collected += entries_this_page;
                }

                if (old_pointers.size() != old_size)
                {
                    unpinIndexPage(meta_page_, false, ctx);
                    resize_in_progress_.store(false, std::memory_order_release);
                    return Status::IO_ERROR;
                }

                // Write new directory pointers
                uint32_t written = 0;
                for (size_t page_idx = 0; page_idx < dir_pages.size() && written < new_size; page_idx++)
                {
                    uint8_t *dir_data = nullptr;
                    status = pinIndexPage(dir_pages[page_idx], (void **)&dir_data, ctx);
                    if (status != Status::OK)
                    {
                        unpinIndexPage(meta_page_, false, ctx);
                        resize_in_progress_.store(false, std::memory_order_release);
                        return status;
                    }

                    auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
                    uint32_t entries_this_page =
                        static_cast<uint32_t>(std::min<size_t>(pointers_per_page, new_size - written));

                    for (uint32_t i = 0; i < entries_this_page; i++)
                    {
                        uint32_t global_index = written + i;
                        dir->hdp_bucket_pointers[i] = old_pointers[global_index % old_size];
                    }

                    // Zero any remaining entries on the last page
                    if (entries_this_page < pointers_per_page)
                    {
                        for (uint32_t i = entries_this_page; i < pointers_per_page; i++)
                        {
                            dir->hdp_bucket_pointers[i] = 0;
                        }
                    }

                    unpinIndexPage(dir_pages[page_idx], true, ctx);
                    written += entries_this_page;
                }

                // Update meta page
                meta->hip_global_depth = new_global_depth;
                unpinIndexPage(meta_page_, true, ctx);

                // Update cache
                cached_global_depth_.store(new_global_depth, std::memory_order_release);

                if (lock.owns_lock())
                {
                    lock.unlock();
                }

                resize_in_progress_.store(false, std::memory_order_release);
                return Status::OK;
            }
        }

        // Find operation
        // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
        // Per MGA_RULES.md Rule 11: Uses TransactionId for visibility checks
        Status HashIndex::find(const void *key_data, size_t key_len,
                               uint64_t current_xid,
                               std::vector<TID>* results,
                               ErrorContext *ctx)
        {
            if (!key_data || key_len == 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid key data for hash index find");
                return Status::INVALID_ARGUMENT;
            }

            if (!results)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Results vector cannot be null");
                return Status::INVALID_ARGUMENT;
            }

            results->clear();

            // Calculate hash
            uint64_t hash = MurmurHash64(key_data, key_len);
            if (bloom_filter_ && !bloom_filter_->test(&hash, sizeof(hash), ctx))
            {
                return Status::NOT_FOUND;
            }

            // Find bucket page
            uint64_t bucket_page = findBucketPageForKey(hash, ctx);
            if (bucket_page == 0)
            {
                // Key not found (bucket doesn't exist yet)
                return Status::OK;
            }

            // Get transaction manager for TIP-based visibility checks
            TransactionManager *txn_mgr = db_->transaction_manager();

            // Scan bucket and overflow chain
            uint32_t current_page = bucket_page;
            while (current_page != 0)
            {
                uint8_t *page_data = nullptr;
                Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
                if (status != Status::OK)
                {
                    SET_ERROR_CONTEXT(ctx, status, "Failed to pin hash bucket page during find");
                    return status;
                }

                auto *bucket = reinterpret_cast<SBHashBucketPage *>(page_data);

                // Scan entries in this page
                for (uint16_t i = 0; i < bucket->hbp_entry_count; i++)
                {
                    const HashEntry &entry = bucket->hbp_entries[i];

                    // Check if hash matches and entry is not deleted
                    if (entry.he_key_hash == hash)
                    {
                        if (entry.getTID() == INVALID_TID)
                        {
                            continue;
                        }

                        // Firebird MGA: Check visibility using TIP-based visibility (NOT snapshots)
                        // If current_xid is 0, return all entries (used by GC)
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
                            // Get TID from entry (GPID + slot)
                            results->push_back(entry.getTID());
                        }
                    }
                }

                uint32_t next_page = bucket->hbp_overflow_page;
                unpinIndexPage(current_page, false, ctx);
                current_page = next_page;
            }

            return Status::OK;
        }

        // Remove operation
        // Firebird MGA: Uses soft delete (set xmax) instead of physical removal
        Status HashIndex::remove(const void *key_data, size_t key_len, const TID &tid,
                                 uint64_t xid, ErrorContext *ctx)
        {
            // PHASE 1.5: Convert TID to legacy format for comparison
            // PHASE 1.5: Now supports custom tablespaces via TID (GPID + slot)
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
                Status status = pinIndexPage(current_page, (void **)&page_data, ctx);
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

                    if (entry.he_key_hash == hash && entry.getTID() == tid)
                    {
                        // Firebird MGA: Soft delete - set xmax instead of physical removal
                        // This allows transactions to still see the entry until their snapshot
                        // Do NOT set he_tuple_id = 0 (that was PostgreSQL MVCC pattern)
                        entry.he_xmax = xid;  // Mark as deleted by this transaction
                        bucket->hbp_deleted_count++;
                        found = true;

                        unpinIndexPage(current_page, true, ctx);

                        // Update meta page statistics
                        uint8_t *meta_data = nullptr;
                        status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
                        if (status == Status::OK)
                        {
                            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
                            meta->hip_num_deleted++;
                            unpinIndexPage(meta_page_, true, ctx);
                        }

                        return Status::OK;
                    }
                }

                uint32_t next_page = bucket->hbp_overflow_page;
                unpinIndexPage(current_page, false, ctx);

                if (found)
                {
                    return Status::OK;
                }

                current_page = next_page;
            }

            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Entry not found in hash index");
            return Status::NOT_FOUND;
        }

        // GC compaction - remove deleted entries and consolidate (ScratchBird MGA GC)
        Status HashIndex::gcCompact(ErrorContext *ctx)
        {
            // Pin meta page to get directory info
            uint8_t *meta_data = nullptr;
            Status status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t dir_page = meta->hip_directory_page;
            uint64_t deleted_before = meta->hip_num_deleted;

            unpinIndexPage(meta_page_, false, ctx);

            // Pin directory page
            uint8_t *dir_data = nullptr;
            status = pinIndexPage(dir_page, (void **)&dir_data, ctx);
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

            unpinIndexPage(dir_page, false, ctx);

            // GC-compact each unique bucket
            uint64_t total_deleted_removed = 0;

            for (uint32_t bucket_page : unique_buckets)
            {
                // STOR-L1: GC-compact this bucket and its overflow chain
                // Track previous page to unlink empty overflow pages
                uint32_t current_page = bucket_page;
                uint32_t prev_page = 0;
                bool is_first_page = true;

                while (current_page != 0)
                {
                    uint8_t *page_data = nullptr;
                    status = pinIndexPage(current_page, (void **)&page_data, ctx);
                    if (status != Status::OK)
                    {
                        prev_page = current_page;
                        is_first_page = false;
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

                            // Keep non-deleted entries (deleted entries have he_xmax set or invalid TID)
                            if (entry.he_xmax == 0 && entry.getTID() != INVALID_TID)
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

                    // STOR-L1: Check if this overflow page is now empty and can be freed
                    // Only free overflow pages (not the primary bucket page)
                    if (!is_first_page && bucket->hbp_entry_count == 0)
                    {
                        // This overflow page is empty - unlink it from the chain
                        // First, update the previous page's overflow pointer
                        if (prev_page != 0)
                        {
                            uint8_t *prev_data = nullptr;
                            Status prev_status = pinIndexPage(prev_page, (void **)&prev_data, ctx);
                            if (prev_status == Status::OK)
                            {
                                auto *prev_bucket = reinterpret_cast<SBHashBucketPage *>(prev_data);
                                // Skip this empty page - point to the next one
                                prev_bucket->hbp_overflow_page = next_page;
                                unpinIndexPage(prev_page, true, ctx);
                            }
                        }

                        // Free this empty overflow page via page manager
                        auto page_mgr = db_->page_manager();
                        if (page_mgr)
                        {
                            // Mark page as free for reuse
                            // Note: freePage marks the page in the free space map
                            page_mgr->freePageGlobal(indexGPID(current_page), ctx);
                        }

                        unpinIndexPage(current_page, true, ctx);
                        // Don't update prev_page since we removed current_page
                        current_page = next_page;
                        continue;
                    }

                    unpinIndexPage(current_page, true, ctx);

                    prev_page = current_page;
                    is_first_page = false;
                    current_page = next_page;
                }
            }

            // Update meta page with new deleted count
            status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
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

                unpinIndexPage(meta_page_, true, ctx);
            }

            return Status::OK;
        }

        // Get statistics
        HashIndex::Statistics HashIndex::getStatistics(ErrorContext *ctx)
        {
            Statistics stats = {};

            // Pin meta page
            uint8_t *meta_data = nullptr;
            Status status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status != Status::OK)
            {
                return stats;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);

            stats.num_tuples = meta->hip_num_tuples;
            stats.num_deleted = meta->hip_num_deleted;
            stats.global_depth = meta->hip_global_depth;
            stats.num_buckets = (1U << meta->hip_global_depth);

            unpinIndexPage(meta_page_, false, ctx);

            // Calculate derived statistics
            if (stats.num_buckets > 0)
            {
                stats.avg_entries_per_bucket =
                    static_cast<double>(stats.num_tuples) / stats.num_buckets;

                uint64_t max_entries = stats.num_buckets * getMaxEntriesPerBucket();
                stats.load_factor = (static_cast<double>(stats.num_tuples) / max_entries) * 100.0;
            }

            // STOR-L2: Count overflow pages by traversing all buckets
            stats.num_overflow_pages = 0;

            // Re-pin meta page to get directory
            status = pinIndexPage(meta_page_, (void **)&meta_data, ctx);
            if (status == Status::OK)
            {
                meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_data);
                uint64_t dir_page = meta->hip_directory_page;
                uint32_t global_depth = meta->hip_global_depth;
                unpinIndexPage(meta_page_, false, ctx);

                if (dir_page != 0)
                {
                    uint32_t num_buckets = (1U << global_depth);
                    size_t pointers_per_page =
                        (db_->page_size() - sizeof(SBHashDirectoryPage)) / sizeof(uint64_t);
                    if (pointers_per_page == 0)
                    {
                        return stats;
                    }

                    // Track unique bucket pages to avoid double-counting
                    std::vector<uint32_t> seen_buckets;
                    seen_buckets.reserve(num_buckets);

                    uint64_t current_dir_page = dir_page;
                    uint32_t processed = 0;
                    while (current_dir_page != 0 && processed < num_buckets)
                    {
                        uint8_t *dir_data = nullptr;
                        status = pinIndexPage(current_dir_page, (void **)&dir_data, ctx);
                        if (status != Status::OK)
                        {
                            break;
                        }

                        auto *dir = reinterpret_cast<SBHashDirectoryPage *>(dir_data);
                        uint32_t entries_this_page =
                            static_cast<uint32_t>(std::min<size_t>(pointers_per_page, num_buckets - processed));

                        for (uint32_t i = 0; i < entries_this_page; i++)
                        {
                            uint32_t bucket_page = dir->hdp_bucket_pointers[i];

                            bool already_seen = false;
                            for (uint32_t seen : seen_buckets)
                            {
                                if (seen == bucket_page)
                                {
                                    already_seen = true;
                                    break;
                                }
                            }

                            if (already_seen)
                            {
                                continue;
                            }
                            seen_buckets.push_back(bucket_page);

                            // Count overflow pages in this bucket's chain
                            uint8_t *bucket_data = nullptr;
                            status = pinIndexPage(bucket_page, (void **)&bucket_data, ctx);
                            if (status == Status::OK)
                            {
                                auto *bucket = reinterpret_cast<SBHashBucketPage *>(bucket_data);
                                uint32_t overflow_page = bucket->hbp_overflow_page;
                                unpinIndexPage(bucket_page, false, ctx);

                                while (overflow_page != 0)
                                {
                                    stats.num_overflow_pages++;

                                    uint8_t *overflow_data = nullptr;
                                    status = pinIndexPage(overflow_page, (void **)&overflow_data, ctx);
                                    if (status != Status::OK)
                                    {
                                        break;
                                    }

                                    auto *overflow = reinterpret_cast<SBHashBucketPage *>(overflow_data);
                                    uint32_t current_overflow = overflow_page;
                                    overflow_page = overflow->hbp_overflow_page;
                                    unpinIndexPage(current_overflow, false, ctx);
                                }
                            }
                        }

                        uint64_t next_page = dir->hdp_next_page;
                        unpinIndexPage(current_dir_page, false, ctx);
                        current_dir_page = next_page;
                        processed += entries_this_page;
                    }
                }
            }

            return stats;
        }

        Status HashIndex::attachBloomFilter(const BloomFilterConfig &config,
                                            uint64_t estimated_keys,
                                            ErrorContext *ctx)
        {
            if (bloom_filter_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Bloom filter already attached");
                return Status::INVALID_ARGUMENT;
            }

            GPID meta_gpid = 0;
            Status status = BloomFilter::create(db_, index_uuid_, config, estimated_keys,
                                               tablespace_id_, &meta_gpid, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            bloom_filter_ = BloomFilter::open(db_, meta_gpid, ctx);
            if (!bloom_filter_)
            {
                return Status::IO_ERROR;
            }
            bloom_filter_->setTargetFpr(config.target_fpr);

            return rebuildBloomFilter(ctx);
        }

        Status HashIndex::loadBloomFilter(GPID meta_gpid, double target_fpr, ErrorContext *ctx)
        {
            if (bloom_filter_)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Bloom filter already attached");
                return Status::INVALID_ARGUMENT;
            }

            bloom_filter_ = BloomFilter::open(db_, meta_gpid, ctx);
            if (!bloom_filter_)
            {
                return Status::IO_ERROR;
            }

            bloom_filter_->setTargetFpr(target_fpr);
            return Status::OK;
        }

        Status HashIndex::detachBloomFilter(ErrorContext *ctx)
        {
            if (!bloom_filter_)
            {
                return Status::OK;
            }

            Status status = bloom_filter_->drop(ctx);
            bloom_filter_.reset();
            return status;
        }

        Status HashIndex::rebuildBloomFilter(ErrorContext *ctx)
        {
            if (!bloom_filter_)
            {
                return Status::OK;
            }

            Status status = bloom_filter_->clear(ctx);
            if (status != Status::OK)
            {
                return status;
            }

            void *meta_buffer = nullptr;
            status = pinIndexPage(meta_page_, &meta_buffer, ctx);
            if (status != Status::OK)
            {
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_buffer);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t directory_page = meta->hip_directory_page;
            unpinIndexPage(meta_page_, false, ctx);

            uint32_t num_buckets = 1U << global_depth;
            std::set<uint64_t> visited_buckets;
            std::vector<uint64_t> bucket_pages;
            uint64_t current_dir_page = directory_page;

            while (current_dir_page != 0)
            {
                void *dir_buffer = nullptr;
                status = pinIndexPage(current_dir_page, &dir_buffer, ctx);
                if (status != Status::OK)
                {
                    return status;
                }

                auto *dir_page = reinterpret_cast<SBHashDirectoryPage *>(dir_buffer);
                uint64_t next_dir_page = dir_page->hdp_next_page;

                uint32_t entries_in_this_page = std::min(num_buckets, 1015U);
                for (uint32_t i = 0; i < entries_in_this_page && bucket_pages.size() < num_buckets; i++)
                {
                    uint64_t bucket_page = dir_page->hdp_bucket_pointers[i];
                    if (bucket_page != 0 && visited_buckets.insert(bucket_page).second)
                    {
                        bucket_pages.push_back(bucket_page);
                    }
                }

                unpinIndexPage(current_dir_page, false, ctx);
                current_dir_page = next_dir_page;
            }

            TransactionManager *txn_mgr = db_->transaction_manager();
            uint64_t oit = txn_mgr ? txn_mgr->getOldestXid() : 0;

            for (uint64_t bucket_page_num : bucket_pages)
            {
                uint64_t current_bucket = bucket_page_num;
                while (current_bucket != 0)
                {
                    void *bucket_buffer = nullptr;
                    status = pinIndexPage(current_bucket, &bucket_buffer, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }

                    auto *bucket = reinterpret_cast<SBHashBucketPage *>(bucket_buffer);
                    uint16_t entry_count = bucket->hbp_entry_count;
                    uint64_t overflow_page = bucket->hbp_overflow_page;

                    for (uint16_t i = 0; i < entry_count; i++)
                    {
                        HashEntry &entry = bucket->hbp_entries[i];
                        if (entry.getTID() == INVALID_TID)
                        {
                            continue;
                        }

                        if (!txn_mgr ||
                            (txn_mgr->isVersionVisible(entry.he_xmin, oit) &&
                             (entry.he_xmax == 0 || !txn_mgr->isVersionVisible(entry.he_xmax, oit))))
                        {
                            uint64_t hash = entry.he_key_hash;
                            Status bf_status = bloom_filter_->insert(&hash, sizeof(hash), ctx);
                            if (bf_status != Status::OK)
                            {
                                unpinIndexPage(current_bucket, false, ctx);
                                return bf_status;
                            }
                        }
                    }

                    unpinIndexPage(current_bucket, false, ctx);
                    current_bucket = overflow_page;
                }
            }

            return Status::OK;
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

            // PHASE 1.5: Build set of dead TIDs for fast lookup
            // Now supports custom tablespaces via TID (GPID + slot)
            std::set<TID> dead_set(dead_tids.begin(), dead_tids.end());

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
            Status status = pinIndexPage(meta_page_, &meta_buffer, ctx);
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

            unpinIndexPage(meta_page_, false, ctx);

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
                status = pinIndexPage(current_dir_page, &dir_buffer, ctx);
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

                unpinIndexPage(current_dir_page, false, ctx);
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
                    status = pinIndexPage(current_bucket, &bucket_buffer, ctx);
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

                        // Skip already deleted entries (INVALID_TID)
                        if (entry.getTID() == INVALID_TID)
                        {
                            continue;
                        }

                        // Check if this tuple ID is in the dead set
                        if (dead_set.find(entry.getTID()) != dead_set.end())
                        {
                            // Mark entry as deleted by setting to INVALID_TID
                            entry.setTID(INVALID_TID);
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
                    unpinIndexPage(current_bucket, page_modified, ctx);

                    // Move to overflow page
                    current_bucket = overflow_page;
                }
            }

            // Update meta page statistics
            if (total_deleted_marked > 0)
            {
                status = pinIndexPage(meta_page_, &meta_buffer, ctx);
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

                    unpinIndexPage(meta_page_, true, ctx);
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

            if (bloom_filter_ && total_entries_removed > 0)
            {
                Status bf_status = rebuildBloomFilter(ctx);
                if (bf_status != Status::OK)
                {
                    LOG_WARNING(VACUUM, "Hash bloom filter rebuild failed: %d",
                                static_cast<int>(bf_status));
                }
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
         *    c. For each non-deleted entry (getTID() != INVALID_TID):
         *       - Extract GPID from entry
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
            const std::unordered_map<TID, TID> &tid_mapping,
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
            Status status = pinIndexPage(meta_page_, &meta_buffer, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx, status, "Failed to pin meta page during TID update");
                return status;
            }

            auto *meta = reinterpret_cast<SBHashIndexMetaPage *>(meta_buffer);
            uint32_t global_depth = meta->hip_global_depth;
            uint64_t directory_page = meta->hip_directory_page;

            unpinIndexPage(meta_page_, false, ctx);

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
                status = pinIndexPage(current_dir_page, &dir_buffer, ctx);
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

                unpinIndexPage(current_dir_page, false, ctx);
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
                    status = pinIndexPage(current_bucket, &bucket_buffer, ctx);
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

                        // Skip deleted entries (invalid TID)
                        if (entry.getTID() == INVALID_TID)
                        {
                            continue;
                        }

                        TID old_tid = entry.getTID();

                        // Check if this TID was migrated
                        auto it = tid_mapping.find(old_tid);
                        if (it != tid_mapping.end())
                        {
                            // Found! Update TID
                            entry.setTID(it->second);
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
                    unpinIndexPage(current_bucket, page_modified, ctx);

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
