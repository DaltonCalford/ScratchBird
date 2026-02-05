/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/bloom_filter.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_set>
#include <shared_mutex>
#include <atomic>

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;
        class BufferPool;

        // Constants
        constexpr uint32_t INITIAL_GLOBAL_DEPTH = 4;   // 16 initial buckets
        constexpr uint32_t MAX_GLOBAL_DEPTH = 20;      // 1M max buckets
        constexpr uint32_t BUCKET_FILL_THRESHOLD = 90; // Split at 90% full
        constexpr uint32_t MAX_OVERFLOW_CHAIN = 5;     // Force split after 5 overflow pages

        // Hash function ID
        constexpr uint32_t HASH_FUNC_MURMUR3 = 1;

        // ===== On-Disk Structures =====

        // Meta Page - Page 0 of hash index
        struct SBHashIndexMetaPage
        {
            PageHeader hip_header;       // Standard page header (64 bytes)
            uint8_t hip_index_uuid[16];  // Index UUID bytes (16 bytes)
            uint32_t hip_hash_func_id;   // Hash function ID (4 bytes) - always HASH_FUNC_MURMUR3
            uint32_t hip_global_depth;   // Global depth (4 bytes) - max 20
            uint64_t hip_directory_page; // First directory page number (8 bytes)
            uint64_t hip_num_tuples;     // Total number of indexed tuples (8 bytes)
            uint64_t hip_num_deleted;    // Number of deleted entries (8 bytes)
            uint8_t hip_reserved[];      // Reserved for future use (page_size - 112)
        } __attribute__((packed));

        // Directory Page - Maps hash values to bucket pages
        struct SBHashDirectoryPage
        {
            PageHeader hdp_header;  // Standard page header (64 bytes)
            uint64_t hdp_next_page; // Next directory page (0 if last) (8 bytes)
            uint64_t hdp_bucket_pointers[]; // Bucket page numbers (flexible array, capacity depends on page size)
        } __attribute__((packed));

        // Hash Entry - Stores hash, tuple ID, and transaction tracking
        // Firebird MGA: Added xmin/xmax for TIP-based visibility (NOT snapshots)
        // PHASE 1.5: Upgraded to use GPID + slot for custom tablespace support
        struct HashEntry
        {
            uint64_t he_key_hash; // Full 64-bit hash of the key
            GPID he_gpid;         // Global Page ID (8 bytes) - supports custom tablespaces
            uint16_t he_slot;     // Slot number within page (2 bytes)
            uint16_t he_padding;  // Padding to maintain alignment (2 bytes)
            uint64_t he_xmin;     // Transaction that created this entry
            uint64_t he_xmax;     // Transaction that deleted this entry (0 if active)

            // Helper to get TID
            TID getTID() const { return TID(he_gpid, he_slot); }
            void setTID(const TID &tid) { he_gpid = tid.gpid; he_slot = tid.slot; }
        } __attribute__((packed));

        static_assert(sizeof(HashEntry) == 36, "HashEntry must be 36 bytes (GPID + slot + xmin + xmax)");

        // Bucket Page - Stores hash entries
        struct SBHashBucketPage
        {
            PageHeader hbp_header;                   // Standard page header (64 bytes)
            uint16_t hbp_entry_count;                // Number of entries in this page (2 bytes)
            uint16_t hbp_local_depth;                // Local depth of this bucket (2 bytes)
            uint32_t hbp_deleted_count;              // Number of deleted entries (4 bytes)
            uint64_t hbp_overflow_page;              // Next overflow page (0 if none) (8 bytes)
            uint8_t hbp_reserved[16];                // Reserved for alignment (16 bytes)
            HashEntry hbp_entries[];                 // Hash entries (flexible array, capacity depends on page size)
        } __attribute__((packed));

        // ===== Hash Index Class =====

        // PHASE 2 TASK 2.3: Implements IndexGCInterface for garbage collection
        class HashIndex : public IndexGCInterface
        {
        public:
            // Constructor - requires database and index UUID
            // The index must already exist (pages allocated)
            HashIndex(Database *db, const UuidV7Bytes &index_uuid);

            // Create a new hash index
            // Initializes preallocated meta page and allocates initial directory/bucket pages
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 GPID meta_gpid, ErrorContext *ctx = nullptr);

            // Open an existing hash index
            static std::unique_ptr<HashIndex> open(Database *db, const UuidV7Bytes &index_uuid,
                                                   GPID meta_gpid, ErrorContext *ctx = nullptr);

            // Destructor
            ~HashIndex();

            // Insert a key-value pair
            // key_data: pointer to key data
            // key_len: length of key in bytes
            // xid: Transaction ID that is creating this entry (for he_xmin)
            Status insert(const void *key_data, size_t key_len, const TID &tid,
                          uint64_t xid, ErrorContext *ctx = nullptr);

            // Find all tuple IDs for a given key
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Pass 0 for current_xid to return ALL matching TIDs (used by GC)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            Status find(const void *key_data, size_t key_len,
                        uint64_t current_xid,
                        std::vector<TID>* results,
                        ErrorContext *ctx = nullptr);

            // Remove a specific entry
            // Only removes the entry matching both key and tid
            // xid: Transaction ID that is deleting this entry (for he_xmax soft delete)
            Status remove(const void *key_data, size_t key_len, const TID &tid,
                          uint64_t xid, ErrorContext *ctx = nullptr);

            // GC compaction (ScratchBird MGA GC, not PostgreSQL VACUUM)
            Status gcCompact(ErrorContext *ctx = nullptr);

            // Get index statistics
            struct Statistics
            {
                uint64_t num_tuples;           // Total entries
                uint64_t num_deleted;          // Deleted entries
                uint32_t global_depth;         // Current global depth
                uint32_t num_buckets;          // Number of buckets (2^global_depth)
                uint32_t num_overflow_pages;   // Number of overflow pages
                double avg_entries_per_bucket; // Average entries per bucket
                double load_factor;            // Percentage full
            };

            Statistics getStatistics(ErrorContext *ctx = nullptr);

            // Get index UUID
            const UuidV7Bytes &getIndexUuid() const
            {
                return index_uuid_;
            }

            // Get meta page number
            uint32_t getMetaPage() const
            {
                return meta_page_;
            }

            // Dynamic capacity calculations based on page size
            // Replaces the removed compile-time constant
            uint16_t getMaxEntriesPerBucket() const;

            // PHASE 2 TASK 2.3: IndexGCInterface implementation
            // Remove index entries pointing to dead tuples
            // Called by garbage collector after heap sweep identifies dead TIDs
            // PHASE 1.5 TASK 1.5.2b: Migrated to TID struct API
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            // Get index type name for logging
            const char *indexTypeName() const override
            {
                return "Hash";
            }

            // PHASE 5 TASK 5.3.1: Update TIDs during tablespace migration
            // Scans all buckets and updates TIDs based on old GPID -> new GPID mapping
            // Used when migrating tables to different tablespaces
            Status updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                            uint64_t *tids_updated_out = nullptr,
                                            uint64_t *pages_modified_out = nullptr,
                                            ErrorContext *ctx = nullptr);

            Status attachBloomFilter(const BloomFilterConfig &config,
                                     uint64_t estimated_keys,
                                     ErrorContext *ctx = nullptr);
            Status loadBloomFilter(GPID meta_gpid, double target_fpr,
                                   ErrorContext *ctx = nullptr);
            Status detachBloomFilter(ErrorContext *ctx = nullptr);
            Status rebuildBloomFilter(ErrorContext *ctx = nullptr);
            BloomFilter *getBloomFilter() const { return bloom_filter_.get(); }

        private:
            Database *db_;
            BufferPool *buffer_pool_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;
            uint16_t tablespace_id_ = 0;
            std::unique_ptr<BloomFilter> bloom_filter_;

            // P2-5: Concurrent directory resize infrastructure
            // Reader-writer lock for directory access
            // - Shared lock: read directory (find operations)
            // - Exclusive lock: modify directory (expand/split)
            mutable std::shared_mutex directory_mutex_;

            // Flag indicating resize is in progress (allows readers to wait or retry)
            std::atomic<bool> resize_in_progress_{false};

            // Cached directory info for fast reads (updated atomically during resize)
            mutable std::atomic<uint32_t> cached_global_depth_{0};
            mutable std::atomic<uint64_t> cached_directory_page_{0};

            // Helper methods
            uint32_t getDirectoryIndex(uint64_t hash, uint32_t global_depth);
            uint64_t findBucketPageForKey(uint64_t hash, ErrorContext *ctx);
            Status allocateBucketPage(uint32_t *page_num_out, ErrorContext *ctx);
            Status allocateOverflowPage(uint32_t *page_num_out, ErrorContext *ctx);
            Status splitBucket(uint32_t bucket_page, uint64_t hash, ErrorContext *ctx);
            Status expandDirectory(ErrorContext *ctx);
            Status expandDirectoryConcurrent(ErrorContext *ctx);  // P2-5: Concurrent version
            bool bucketNeedsSplit(SBHashBucketPage *bucket);
            uint16_t countEntriesInBucket(uint32_t bucket_page, ErrorContext *ctx);
            Status redistributeEntries(SBHashBucketPage *old_bucket, SBHashBucketPage *new_bucket,
                                       uint32_t new_local_depth, ErrorContext *ctx);

            // P2-5: Helper to refresh cached directory info from meta page
            void refreshCachedDirectoryInfo(ErrorContext *ctx);

            Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr);
            Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr);
            GPID indexGPID(uint64_t page_num) const;

            // No copy or move
            HashIndex(const HashIndex &) = delete;
            HashIndex &operator=(const HashIndex &) = delete;
            HashIndex(HashIndex &&) = delete;
            HashIndex &operator=(HashIndex &&) = delete;
        };

    } // namespace core
} // namespace scratchbird
