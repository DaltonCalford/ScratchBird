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

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/charset.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/gpid.h"
#include "scratchbird/core/bloom_filter.h"
#include "scratchbird/core/buffer_pool.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <unordered_map>

namespace scratchbird
{
    namespace core
    {

        // Forward declarations
        class Database;
        class BufferPool;
        class PageManager;
        class TransactionManager;
        struct ErrorContext;

        // Page flags
        enum class BTreeFlags : uint16_t
        {
            LEAF = 0x0001,        // Leaf page
            ROOT = 0x0002,        // Root page
            RIGHTMOST = 0x0004,   // Rightmost page at this level
            LEFTMOST = 0x0008,    // Leftmost page at this level
            COMPRESSED = 0x0010,  // Compression enabled
            ENCRYPTED = 0x0020,   // Page is encrypted
            HAS_GARBAGE = 0x0040, // Page has deleted entries
            INCOMPLETE = 0x0080   // Split in progress
        };

        // Compression types
        enum class BTreeCompressionType : uint8_t
        {
            NONE = 0,
            PREFIX = 1,
            SUFFIX = 2,
            BOTH = 3,
            ZSTD = 4,
            ADAPTIVE = 5
        };

        // Node flags
        enum class BTreeNodeFlags : uint16_t
        {
            DELETED = 0x0001,        // Logically deleted
            HAS_DUPLICATES = 0x0002, // Multiple tuple IDs
            FIRST_ON_PAGE = 0x0004,  // First node on page (no prefix)
            LAST_ON_PAGE = 0x0008,   // Last node on page
            NULL_KEY = 0x0010,       // NULL key value
            INFINITY_KEY = 0x0020    // Positive infinity (rightmost)
        };

#pragma pack(push, 1)
        // ScratchBird B-Tree page structure (all page sizes supported)
        struct SBBTreePage
        {
            // Standard page header
            PageHeader btr_header; // Standard ScratchBird page header

            // Index identification
            ID btr_index_uuid; // Index UUID v7 (not numeric ID)
            ID btr_table_uuid; // Table this index belongs to

            // Tree structure
            uint16_t btr_level;      // Level (0 = leaf, increases upward)
            uint16_t btr_flags;      // Page flags (see above)
            uint16_t btr_count;      // Number of entries in this page
            uint16_t btr_free_space; // Free space in bytes

            // Sibling navigation
            uint64_t btr_left_sibling;  // Left sibling page number
            uint64_t btr_right_sibling; // Right sibling page number
            uint64_t btr_parent_page;   // Parent page (for fast traversal)

            // B-Tree rightmost child pointer (for internal nodes only)
            // In a B-tree, internal nodes with N keys have N+1 children
            // Each key has a left child pointer (stored in SBBTreeNode->btn_child_page)
            // The rightmost child pointer is stored here in the page header
            uint64_t
                btr_rightmost_child; // Rightmost child page (internal nodes only, 0 for leaves)

            // Compression metadata
            uint16_t btr_prefix_total;  // Total prefix compression bytes saved
            uint16_t btr_suffix_total;  // Total suffix truncation bytes saved
            uint8_t btr_compression;    // Compression type (see enum above)
            uint8_t btr_min_prefix_len; // Minimum prefix length on page

            // Multi-version support for MGA
            uint64_t btr_xmin; // Page creation transaction
            uint64_t btr_xmax; // Page deletion transaction (0 if active)
            uint64_t btr_lsn;  // Last LSN that modified this page

            // High water mark
            uint16_t btr_high_water; // Highest used offset in page
        };

        // B-Tree node structure (variable length)
        // The actual node data is stored on the page, this struct is for access.
        struct SBBTreeNode
        {
            // Node header (fixed part)
            uint16_t btn_flags;        // Node flags
            uint16_t btn_prefix_len;   // Prefix compression length
            uint16_t btn_suffix_trunc; // Suffix truncation length
            uint16_t btn_key_len;      // Actual key length (after compression)

            // For leaf nodes
            uint32_t btn_tuple_count; // Number of tuples (for duplicates)

            // For internal nodes
            uint64_t btn_child_page; // Child page number (left of this key)

            // Multi-version support
            uint64_t btn_xmin; // Node creation transaction
            uint64_t btn_xmax; // Node deletion transaction

            // Variable length data follows this header in memory on the page
            // [key_data][tuple_ids or child_pointer]
        };
#pragma pack(pop)

        static_assert(sizeof(SBBTreePage) == 184, "SBBTreePage size must be 184 bytes (80-byte PageHeader + 104 bytes)");
        static_assert(sizeof(SBBTreeNode) == 36, "SBBTreeNode size must be 36 bytes");

        // In-memory representation of a B-Tree index
        struct SBBTreeIndex
        {
            ID idx_uuid;
            ID idx_table_uuid;
            std::vector<ID> idx_column_ids; // Fixed: Changed from uint16_t to ID (UUID)
            uint32_t idx_flags;

            uint64_t idx_root_page;
            uint16_t idx_tablespace_id = 0;
            uint16_t idx_height;

            uint64_t idx_tuple_count;
            uint64_t idx_page_count;
            uint64_t idx_deleted_count;

            uint32_t idx_collation_id = 100; // Default: utf8_bin (binary comparison)
                                             // Use column's collation for text indexes
        };

        // Forward declaration for iterator
        class BTreeIterator;

        // B-tree implementation
        // PHASE 2 TASK 2.2: Implements IndexGCInterface for garbage collection
        class BTree : public IndexGCInterface
        {
        public:
            BTree(Database *db, SBBTreeIndex index_info);
            ~BTree();

            // Static factory methods
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 const UuidV7Bytes &table_uuid,
                                 const std::vector<UuidV7Bytes> &column_uuids,
                                 GPID root_gpid, ErrorContext *ctx = nullptr);

            static std::unique_ptr<BTree> open(Database *db, const UuidV7Bytes &index_uuid,
                                               GPID root_gpid, ErrorContext *ctx = nullptr);

            // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
            // Task 17 MGA Phase 3.1: Added xid parameter for transaction tracking
            Status insert(const std::vector<uint8_t> &key, const TID &tid,
                          uint64_t xid,  // Transaction ID for btn_xmin
                          ErrorContext *ctx = nullptr);

            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Pass 0 as current_xid to return ALL matching TIDs (used by GC/internal operations)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            Status search(const std::vector<uint8_t> &key,
                          uint64_t current_xid,  // Transaction ID for visibility checks
                          std::vector<TID> *tids_out,
                          ErrorContext *ctx = nullptr);

            // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
            // Task 17 MGA Phase 3.1: Added xid parameter for transaction tracking
            Status remove(const std::vector<uint8_t> &key, const TID &tid,
                          uint64_t xid,  // Transaction ID for btn_xmax
                          ErrorContext *ctx = nullptr);

            // Task 17 MGA Phase 3.2: Soft deletion support (mark deleted instead of physical removal)
            /**
             * Mark index entry as deleted by setting btn_xmax
             *
             * More efficient than physical removal - entry remains in index but becomes
             * invisible to transactions >= xmax (if xmax transaction commits).
             * Physical removal happens later during GC compaction.
             *
             * @param key Index key
             * @param tid Tuple ID to mark deleted
             * @param xmax Transaction ID deleting this entry
             * @param ctx Error context
             * @return Status::OK on success, Status::NOT_FOUND if entry not found
             */
            Status markDeleted(const std::vector<uint8_t> &key,
                              const TID &tid,
                              uint64_t xmax,
                              ErrorContext *ctx = nullptr);

            // Range scan operations
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            std::unique_ptr<BTreeIterator>
            rangeScan(const std::vector<uint8_t> *start_key, // nullptr for beginning
                      const std::vector<uint8_t> *end_key,   // nullptr for end
                      uint64_t current_xid,                  // Transaction ID for visibility
                      bool start_inclusive = true, bool end_inclusive = false,
                      ErrorContext *ctx = nullptr);

            // GC compaction operations (ScratchBird MGA GC, not PostgreSQL VACUUM)
            struct GcCompactionStats
            {
                uint64_t pages_visited;
                uint64_t pages_compacted;
                uint64_t nodes_removed;
                uint64_t bytes_reclaimed;
                uint64_t pages_merged;
            };

            // GC compaction: removes logically deleted entries and reclaims space
            // This is NOT PostgreSQL VACUUM.
            Status gcCompact(GcCompactionStats *stats_out = nullptr, ErrorContext *ctx = nullptr);

            // P1-11: Bulk loading optimization for initial index construction
            // Build index bottom-up from sorted data for O(N) vs O(N log N) performance
            // @param entries Vector of (key, TID) pairs to insert (will be sorted)
            // @param xid Transaction ID for btn_xmin on all entries
            // @return Status::OK on success
            Status bulkLoad(std::vector<std::pair<std::vector<uint8_t>, TID>> &entries,
                           uint64_t xid,
                           ErrorContext *ctx = nullptr);

            Status attachBloomFilter(const BloomFilterConfig &config,
                                     uint64_t estimated_keys,
                                     ErrorContext *ctx = nullptr);
            Status loadBloomFilter(GPID meta_gpid, double target_fpr,
                                   ErrorContext *ctx = nullptr);
            Status detachBloomFilter(ErrorContext *ctx = nullptr);
            Status rebuildBloomFilter(ErrorContext *ctx = nullptr);
            BloomFilter *getBloomFilter() const { return bloom_filter_.get(); }

            // PHASE 2 TASK 2.2: IndexGCInterface implementation
            // Remove index entries pointing to dead tuples
            // Called by garbage collector after heap sweep identifies dead TIDs
            // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            // Get index type name for logging
            const char *indexTypeName() const override
            {
                return "B-Tree";
            }

            // PHASE 5 TASK 5.2: Update TIDs during tablespace migration
            // Traverses leaf nodes and updates TIDs based on old GPID -> new GPID mapping
            // Used when migrating tables to different tablespaces
            Status updateTIDsAfterMigration(const std::unordered_map<TID, TID> &tid_mapping,
                                            uint64_t *tids_updated_out = nullptr,
                                            uint64_t *pages_modified_out = nullptr,
                                            ErrorContext *ctx = nullptr);

        private:
            Database *db_;
            SBBTreeIndex index_info_;
            CharsetManager charset_manager_; // For collation-aware key comparisons

            Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr,
                                BufferPool::AccessStrategy strategy = BufferPool::AccessStrategy::Normal);
            Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr);
            GPID indexGPID(uint64_t page_num) const;

            // Collation-aware key comparison using CharsetManager
            // Returns: -1 if key1 < key2, 0 if equal, 1 if key1 > key2
            int compare_keys(const std::vector<uint8_t> &key1,
                             const std::vector<uint8_t> &key2) const
            {
                return charset_manager_.compare(key1.data(), static_cast<uint32_t>(key1.size()),
                                                key2.data(), static_cast<uint32_t>(key2.size()),
                                                index_info_.idx_collation_id);
            }

            // ISSUE 3.6 FIX: Optimized key comparison that avoids temporary vector allocation
            // This overload takes raw pointers and lengths, eliminating heap allocation overhead
            // Returns: -1 if key1 < key2, 0 if equal, 1 if key1 > key2
            int compare_keys(const std::vector<uint8_t> &key1, const uint8_t *key2_data,
                             uint16_t key2_len) const
            {
                return charset_manager_.compare(key1.data(), static_cast<uint32_t>(key1.size()),
                                                key2_data, static_cast<uint32_t>(key2_len),
                                                index_info_.idx_collation_id);
            }

            // Traverses the B-Tree to find the correct leaf page for a given key.
            Status find_leaf_page(const std::vector<uint8_t> &key, uint64_t *page_num_out,
                                  bool write_lock, ErrorContext *ctx);

            // Searches for a key within a single B-Tree page using binary search.
            // Firebird MGA: Uses TIP-based visibility filtering
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            bool searchPage(const SBBTreePage *page, const std::vector<uint8_t> &key,
                            uint64_t current_xid,
                            std::vector<TID> *tids_out) const;

            // Page split operations
            // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
            Status split_leaf_page(uint64_t left_page_num, const std::vector<uint8_t> &new_key,
                                   const TID &new_tid, ErrorContext *ctx);
            Status split_internal_page(uint64_t left_page_num,
                                       uint64_t left_child_page_num,
                                       const std::vector<uint8_t> &separator_key,
                                       uint64_t right_page_num,
                                       uint16_t separator_suffix_trunc,
                                       ErrorContext *ctx);
            Status insert_into_parent(uint64_t left_page_num,
                                      const std::vector<uint8_t> &separator_key,
                                      uint64_t right_page_num,
                                      uint16_t separator_suffix_trunc,
                                      ErrorContext *ctx);
            Status create_new_root(uint64_t left_page_num,
                                   const std::vector<uint8_t> &separator_key,
                                   uint64_t right_page_num,
                                   uint16_t separator_suffix_trunc,
                                   ErrorContext *ctx);

            // Firebird MGA: TIP-based visibility checking for index entries
            /**
             * Check if index entry is visible using Firebird MGA visibility rules
             *
             * Per MGA_RULES.md Rule 3:
             * - Entry created by xmin is visible if: xmin == current_xid OR (xmin is COMMITTED and xmin < current_xid)
             * - Entry is deleted if: xmax != 0 AND xmax is visible
             *
             * @param xmin Transaction that created entry
             * @param xmax Transaction that deleted entry (0 if active)
             * @param current_xid Transaction ID checking visibility
             * @return true if visible, false otherwise
             */
            bool isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const;

            // Allow iterator to access internal members
            friend class BTreeIterator;

            // GC compaction helpers
            Status gcCompactPage(uint32_t page_id, GcCompactionStats &stats, ErrorContext *ctx);
            Status compactPage(uint8_t *page_data, uint32_t page_size, GcCompactionStats &stats, ErrorContext *ctx);
            bool shouldMergePages(const SBBTreePage *page1, const SBBTreePage *page2) const;
            Status mergePages(uint32_t left_page, uint32_t right_page, GcCompactionStats &stats,
                              ErrorContext *ctx);
            Status removeFromParent(uint64_t parent_page_num, uint64_t child_page_id,
                                    ErrorContext *ctx);

            // STOR-L3: Check if parent page is underutilized and merge recursively
            void checkAndMergeParentRecursive(uint64_t page_num, GcCompactionStats &stats,
                                              ErrorContext *ctx);

            std::unique_ptr<BloomFilter> bloom_filter_;
        };

        // B-tree range scan iterator
        class BTreeIterator
        {
        public:
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            BTreeIterator(BTree *btree, const std::vector<uint8_t> *start_key,
                          const std::vector<uint8_t> *end_key,
                          uint64_t current_xid,
                          bool start_inclusive,
                          bool end_inclusive);
            ~BTreeIterator();

            // Iterator operations
            bool hasNext();
            // PHASE 1.5 TASK 1.5.2a: Migrated to TID struct API
            Status next(std::vector<uint8_t> *key_out, TID *tid_out,
                        ErrorContext *ctx = nullptr);

            // Get current position
            Status getCurrentKey(std::vector<uint8_t> *key_out, ErrorContext *ctx = nullptr) const;
            uint64_t getScannedCount() const
            {
                return scanned_count_;
            }

        private:
            BTree *btree_;
            Database *db_;

            // Firebird MGA: Transaction ID for TIP-based visibility filtering
            // Pass 0 to return all tuples (used by GC/internal operations)
            uint64_t current_xid_;

            // Range bounds
            std::vector<uint8_t> start_key_;
            std::vector<uint8_t> end_key_;
            bool has_start_;
            bool has_end_;
            bool start_inclusive_;
            bool end_inclusive_;

            // Current position
            uint32_t current_page_;
            uint16_t current_slot_;
            uint16_t current_tuple_index_; // For duplicate keys
            bool initialized_;
            bool exhausted_;

            // Statistics
            uint64_t scanned_count_;

            // Internal navigation
            Status initialize(ErrorContext *ctx);
            Status advanceToNextValid(ErrorContext *ctx);
            Status moveToNextSlot(ErrorContext *ctx);
            Status moveToNextPage(ErrorContext *ctx);
            bool isKeyInRange(const std::vector<uint8_t> &key) const;
            int compareKeys(const std::vector<uint8_t> &k1, const std::vector<uint8_t> &k2) const;
        };

    } // namespace core
} // namespace scratchbird
