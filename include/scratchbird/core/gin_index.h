#pragma once

#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/status.h"
#include "scratchbird/core/error_context.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/tid.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>

namespace scratchbird
{
    namespace core
    {
        // Forward declarations
        class Database;
        class BufferPool;
        class TransactionManager; // For TIP-based visibility checks (Firebird MGA)
        struct ErrorContext;

        // ===== GIN Index Constants =====

        constexpr uint32_t GIN_PENDING_LIST_THRESHOLD = 1000; // Merge after 1000 entries
        constexpr uint32_t GIN_POSTING_LIST_THRESHOLD = 64;   // Convert to tree after 64 TIDs

        // ===== On-Disk Structures =====

        // Meta Page - Page 0 of GIN index
        struct SBGinIndexMetaPage
        {
            PageHeader hip_header;           // Standard page header (64 bytes)
            uint8_t gin_index_uuid[16];      // Index UUID bytes (16 bytes)
            uint64_t gin_keys_btree_root;    // Root page of Keys B-Tree (8 bytes)
            uint64_t gin_pending_list_head;  // Head of pending list pages (8 bytes)
            uint64_t gin_pending_list_tail;  // Tail of pending list (8 bytes)
            uint64_t gin_pending_list_count; // Number of entries in pending list (8 bytes)
            uint64_t gin_num_keys;           // Total number of unique keys (8 bytes)
            uint64_t gin_num_tuples;         // Total number of indexed tuples (8 bytes)
            uint8_t gin_reserved[8064];      // Reserved for future use (8192 - 128 = 8064)
        } __attribute__((packed));

        static_assert(sizeof(SBGinIndexMetaPage) == 8192, "GIN meta page must be exactly 8KB");

        // Pending Entry - Single entry in pending list
        struct GinPendingEntry
        {
            GPID gpid;            // Global Page ID (8 bytes) - supports custom tablespaces
            uint16_t slot;        // Slot number within page (2 bytes)
            uint16_t padding;     // Padding for alignment (2 bytes)
            uint64_t xmin;        // Transaction ID that inserted this entry (for MVCC)
            uint16_t key_len;     // Key length in bytes
            uint8_t key_data[50]; // Key data (inline for small keys, reduced from 54 to fit)

            // Helper methods for TID access
            TID getTID() const { return TID(gpid, slot); }
            void setTID(const TID &tid) { gpid = tid.gpid; slot = tid.slot; }
        } __attribute__((packed));

        static_assert(sizeof(GinPendingEntry) == 72, "GinPendingEntry must be 72 bytes (8+2+2+8+2+50)");

        // Pending List Page - Stores pending entries
        struct SBGinPendingListPage
        {
            PageHeader gpp_header;                       // Standard page header (64 bytes)
            uint64_t gpp_next_page;                      // Next page in chain (0 if last) (8 bytes)
            uint16_t gpp_entry_count;                    // Number of entries (2 bytes)
            uint8_t gpp_reserved[54];                    // Reserved for alignment (54 bytes)
            GinPendingEntry gpp_entries[(8192 - 128) / 72]; // Pending entries (112 entries)
        } __attribute__((packed));

        static_assert(sizeof(SBGinPendingListPage) == 8192, "Pending list page must be exactly 8KB");

        // Maximum entries per pending page
        constexpr uint16_t MAX_PENDING_ENTRIES_PER_PAGE = (8192 - 128) / 72;

        // Posting List Entry - Single TID in a posting list
        struct GinPostingEntry
        {
            GPID gpid;       // Global Page ID (8 bytes) - supports custom tablespaces
            uint16_t slot;   // Slot number within page (2 bytes)

            // Helper methods for TID access
            TID getTID() const { return TID(gpid, slot); }
            void setTID(const TID &tid) { gpid = tid.gpid; slot = tid.slot; }

            // Comparison operators for sorting
            bool operator<(const GinPostingEntry &other) const {
                if (gpid != other.gpid) return gpid < other.gpid;
                return slot < other.slot;
            }
            bool operator==(const GinPostingEntry &other) const {
                return gpid == other.gpid && slot == other.slot;
            }
        } __attribute__((packed));

        static_assert(sizeof(GinPostingEntry) == 10, "GinPostingEntry must be 10 bytes (GPID + slot)");

        // Posting List Page - Stores sorted TIDs for a key (small lists)
        // For large lists, we use a B-Tree of TIDs instead
        struct SBGinPostingListPage
        {
            PageHeader gpl_header;                          // Standard page header (64 bytes)
            uint16_t gpl_entry_count;                       // Number of TIDs (2 bytes)
            uint8_t gpl_is_tree;                            // 0 = list, 1 = tree root pointer (1 byte)
            uint8_t gpl_is_compressed;                      // 1 = compressed, 0 = uncompressed (1 byte)
            uint16_t gpl_compressed_bytes;                  // Size of compressed data (2 bytes)
            uint8_t gpl_reserved[10];                       // Reserved for alignment (10 bytes)
            union
            {
                uint8_t gpl_compressed_data[8192 - 80];       // Compressed TID data
                GinPostingEntry gpl_entries[(8192 - 80) / 10]; // Uncompressed TIDs (811 entries)
                uint64_t gpl_tree_root;                       // Root page of posting B-Tree
            } gpl_data;
        } __attribute__((packed));

        static_assert(sizeof(SBGinPostingListPage) == 8192, "Posting list page must be exactly 8KB");

        // Maximum TIDs per posting list page (reduced from 1014 to 811 for GPID support)
        constexpr uint16_t MAX_POSTING_ENTRIES_PER_PAGE = (8192 - 80) / 10;

        // ===== Posting Tree Structures =====
        // When a posting list exceeds GIN_POSTING_LIST_THRESHOLD (64 TIDs),
        // it's converted to a B-Tree of TIDs for efficient search and insertion

        // Posting Tree Internal Node Entry
        struct GinPostingTreeInternalEntry
        {
            GPID separator_gpid;    // GPID that separates this subtree from the next (8 bytes)
            uint16_t separator_slot; // Slot number of separator TID (2 bytes)
            uint32_t child_page;    // Page number of child node (4 bytes)

            // Helper to get separator TID
            TID getSeparatorTID() const { return TID(separator_gpid, separator_slot); }
            void setSeparatorTID(const TID &tid) { separator_gpid = tid.gpid; separator_slot = tid.slot; }
        } __attribute__((packed));

        static_assert(sizeof(GinPostingTreeInternalEntry) == 14, "Internal entry must be 14 bytes (GPID + slot + page)");

        // Posting Tree Internal Node
        struct SBGinPostingTreeInternal
        {
            PageHeader gpt_header;                              // Standard page header (64 bytes)
            uint16_t gpt_entry_count;                           // Number of entries (2 bytes)
            uint16_t gpt_is_leaf;                               // 0 for internal nodes (2 bytes)
            uint8_t gpt_reserved[24];                           // Reserved for alignment (24 bytes)
            GinPostingTreeInternalEntry gpt_entries[(8192 - 92) / 14]; // Internal entries (578 entries)
        } __attribute__((packed));

        static_assert(sizeof(SBGinPostingTreeInternal) <= 8192, "Posting tree internal must fit in 8KB");

        // Maximum internal entries per posting tree internal node (reduced from 675 to 578 for GPID)
        constexpr uint16_t MAX_POSTING_TREE_INTERNAL_ENTRIES = (8192 - 92) / 14;

        // Posting Tree Leaf Node
        struct SBGinPostingTreeLeaf
        {
            PageHeader gpt_header;                      // Standard page header (64 bytes)
            uint16_t gpt_entry_count;                   // Number of TIDs (2 bytes)
            uint16_t gpt_is_leaf;                       // 1 for leaf nodes (2 bytes)
            uint64_t gpt_next_leaf;                     // Next leaf page for range scans (8 bytes)
            uint8_t gpt_reserved[12];                   // Reserved for alignment (12 bytes)
            GinPostingEntry gpt_tids[(8192 - 88) / 10]; // Sorted TID array (810 entries)
        } __attribute__((packed));

        static_assert(sizeof(SBGinPostingTreeLeaf) <= 8192, "Posting tree leaf must fit in 8KB");

        // Maximum TIDs per posting tree leaf node (reduced from 1013 to 810 for GPID support)
        constexpr uint16_t MAX_POSTING_TREE_LEAF_TIDS = (8192 - 88) / 10;

        // Entry in the Keys B-Tree
        // The key is the indexed item (e.g., a word, array element)
        // The value is a pointer to a posting list or posting tree
        struct GinEntryTreeValue
        {
            uint64_t posting_list_page; // Page number of posting list/tree
            uint32_t num_tids;          // Number of TIDs for this key
        } __attribute__((packed));

        static_assert(sizeof(GinEntryTreeValue) == 12, "GinEntryTreeValue must be 12 bytes");

        // ===== Entry Tree (Keys B-Tree) Structures =====
        // Maps keys → posting lists/trees
        // Uses variable-length keys with offset array

        // Entry Tree Leaf Entry - stored in data area
        struct GinEntryTreeLeafEntry
        {
            uint16_t key_len;                // Key length
            GinEntryTreeValue value;         // Posting page + TID count
            uint8_t key_data[1];             // Variable-length key (flexible array)
        } __attribute__((packed));

        // Entry Tree Leaf Page
        struct SBGinEntryTreeLeaf
        {
            PageHeader get_header;           // Standard page header (64 bytes)
            uint16_t get_entry_count;        // Number of entries (2 bytes)
            uint16_t get_is_leaf;            // 1 for leaf nodes (2 bytes)
            uint16_t get_free_space;         // Free space in data area (2 bytes)
            uint16_t get_data_end;           // End of data area (offset from page start) (2 bytes)
            uint8_t get_reserved[12];        // Reserved (12 bytes)
            uint16_t get_offsets[500];       // Offset array (1000 bytes, max 500 entries)
            // Data area starts at offset 1084, grows upward
            // Keys and values stored here
        } __attribute__((packed));

        static_assert(sizeof(SBGinEntryTreeLeaf) <= 8192, "Entry tree leaf must fit in 8KB");

        // Maximum entries per entry tree leaf (approximate, depends on key sizes)
        constexpr uint16_t MAX_ENTRY_TREE_LEAF_ENTRIES = 500;

        // Entry Tree Internal Entry - stored in data area
        struct GinEntryTreeInternalEntry
        {
            uint16_t key_len;                // Separator key length
            uint32_t child_page;             // Child page pointer
            uint8_t key_data[1];             // Variable-length separator key
        } __attribute__((packed));

        // Entry Tree Internal Page
        struct SBGinEntryTreeInternal
        {
            PageHeader get_header;           // Standard page header (64 bytes)
            uint16_t get_entry_count;        // Number of entries (2 bytes)
            uint16_t get_is_leaf;            // 0 for internal nodes (2 bytes)
            uint16_t get_free_space;         // Free space in data area (2 bytes)
            uint16_t get_data_end;           // End of data area (2 bytes)
            uint32_t get_rightmost_child;    // Rightmost child page (4 bytes)
            uint8_t get_reserved[8];         // Reserved (8 bytes)
            uint16_t get_offsets[500];       // Offset array (1000 bytes)
            // Data area starts at offset 1084, grows upward
        } __attribute__((packed));

        static_assert(sizeof(SBGinEntryTreeInternal) <= 8192, "Entry tree internal must fit in 8KB");

        // Maximum entries per entry tree internal (approximate)
        constexpr uint16_t MAX_ENTRY_TREE_INTERNAL_ENTRIES = 500;

        // ===== GIN Index Class =====

        // PHASE 2 TASK 2.4: Implements IndexGCInterface for garbage collection
        class GinIndex : public IndexGCInterface
        {
        public:
            // Constructor
            GinIndex(Database *db, const UuidV7Bytes &index_uuid);

            // Create a new GIN index
            static Status create(Database *db, const UuidV7Bytes &index_uuid,
                                 uint32_t *meta_page_out, ErrorContext *ctx = nullptr);

            // Open an existing GIN index
            static std::unique_ptr<GinIndex> open(Database *db, const UuidV7Bytes &index_uuid,
                                                  uint32_t meta_page, ErrorContext *ctx = nullptr);

            // Destructor
            ~GinIndex();

            // Insert a composite value (e.g., array, JSONB)
            // PHASE 1.5 TASK 1.5.2f: Migrated to TID struct API
            // Keys are extracted using the provided key_extractor function
            Status insert(const void *value_data, size_t value_len, const TID &tid,
                          std::function<std::vector<std::vector<uint8_t>>(const void *, size_t)> key_extractor,
                          ErrorContext *ctx = nullptr);

            // Find all tuple IDs containing a specific key
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            std::vector<TID> find(const void *key_data, size_t key_len,
                                  uint64_t current_xid,
                                  ErrorContext *ctx = nullptr);

            // Find tuple IDs matching ALL keys (AND operation)
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            std::vector<TID> findAll(const std::vector<std::vector<uint8_t>> &keys,
                                     uint64_t current_xid,
                                     ErrorContext *ctx = nullptr);

            // Find tuple IDs matching ANY key (OR operation)
            // Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
            // Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
            std::vector<TID> findAny(const std::vector<std::vector<uint8_t>> &keys,
                                     uint64_t current_xid,
                                     ErrorContext *ctx = nullptr);

            // Merge pending list into main index
            Status mergePendingList(ErrorContext *ctx = nullptr);

            // Vacuum the index - consolidate posting lists
            Status vacuum(ErrorContext *ctx = nullptr);

            // Get index statistics
            struct Statistics
            {
                uint64_t num_keys;              // Total unique keys
                uint64_t num_tuples;            // Total indexed tuples
                uint64_t pending_list_count;    // Entries in pending list
                uint32_t keys_tree_height;      // Height of keys B-Tree
                double avg_tids_per_key;        // Average TIDs per key
                uint64_t num_posting_trees;     // Number of large posting trees
                uint64_t num_posting_lists;     // Number of small posting lists
            };

            Statistics getStatistics(ErrorContext *ctx = nullptr);

            // ===== Phase 5: Advanced Features =====

            // GIN Operator types (PostgreSQL compatible)
            enum class GinOperator
            {
                CONTAINS,       // @>  - left contains right
                CONTAINED_BY,   // <@  - left contained by right
                OVERLAP,        // &&  - has common elements
                EQUALS,         // =   - exact match
                EXISTS,         // ?   - key exists
                EXISTS_ANY,     // ?|  - any key exists
                EXISTS_ALL,     // ?&  - all keys exist
                TEXT_SEARCH     // @@  - full text search
            };

            // Query options for advanced queries
            struct QueryOptions
            {
                bool optimize_key_order;    // Reorder keys by selectivity
                bool parallel_execution;    // Use parallel execution
                uint32_t max_edit_distance; // For fuzzy matching (0 = exact)
                bool case_sensitive;        // Case sensitivity for text
                uint32_t max_threads;       // Max threads for parallel execution
            };

            // Key cardinality estimate for query optimization
            struct KeyCardinality
            {
                std::vector<uint8_t> key;
                uint32_t estimated_tids;
            };

            // ===== Phase 5 Methods =====

            // Optimized multi-key query with selectivity-based reordering
            // PHASE 1.5 TASK 1.5.2f: Migrated to TID struct API
            std::vector<TID> findAllOptimized(
                const std::vector<std::vector<uint8_t>> &keys,
                const QueryOptions &options,
                ErrorContext *ctx = nullptr);

            std::vector<TID> findAnyOptimized(
                const std::vector<std::vector<uint8_t>> &keys,
                const QueryOptions &options,
                ErrorContext *ctx = nullptr);

            // PostgreSQL GIN operator support
            // PHASE 1.5 TASK 1.5.2f: Migrated to TID struct API
            std::vector<TID> executeOperator(
                GinOperator op,
                const std::vector<std::vector<uint8_t>> &left_keys,
                const std::vector<std::vector<uint8_t>> &right_keys,
                ErrorContext *ctx = nullptr);

            // Wildcard query support
            // PHASE 1.5 TASK 1.5.2f: Migrated to TID struct API
            std::vector<TID> findWithWildcard(
                const void *pattern, size_t pattern_len,
                ErrorContext *ctx = nullptr);

            // Fuzzy matching with edit distance
            // PHASE 1.5 TASK 1.5.2f: Migrated to TID struct API
            std::vector<TID> findFuzzy(
                const void *key_data, size_t key_len,
                uint32_t max_edit_distance,
                ErrorContext *ctx = nullptr);

            // Estimate cardinality for a key (for query optimization)
            uint32_t estimateKeyCardinality(
                const std::vector<uint8_t> &key,
                ErrorContext *ctx = nullptr);

            // ===== Phase 6: Advanced Performance & Features =====

            // Range query structures
            struct RangeQuery
            {
                std::vector<uint8_t> lower_bound;
                std::vector<uint8_t> upper_bound;
                bool lower_inclusive; // true for [, false for (
                bool upper_inclusive; // true for ], false for )
            };

            // Range query: find TIDs with keys in range
            std::vector<uint64_t> findInRange(
                const RangeQuery &range,
                ErrorContext *ctx = nullptr);

            // Optimized wildcard query with B-Tree prefix scan
            std::vector<uint64_t> findWithWildcardOptimized(
                const void *pattern, size_t pattern_len,
                ErrorContext *ctx = nullptr);

            // Optimized fuzzy matching with BK-tree
            std::vector<uint64_t> findFuzzyOptimized(
                const void *key_data, size_t key_len,
                uint32_t max_edit_distance,
                ErrorContext *ctx = nullptr);

            // Parallel multi-key AND query
            std::vector<uint64_t> findAllParallel(
                const std::vector<std::vector<uint8_t>> &keys,
                uint32_t max_threads,
                ErrorContext *ctx = nullptr);

            // Parallel multi-key OR query
            std::vector<uint64_t> findAnyParallel(
                const std::vector<std::vector<uint8_t>> &keys,
                uint32_t max_threads,
                ErrorContext *ctx = nullptr);

            // SIMD-optimized intersection
            static std::vector<uint64_t> mergeTidListsSIMD(
                const std::vector<std::vector<uint64_t>> &tid_lists);

            // SIMD-optimized union
            static std::vector<uint64_t> unionTidListsSIMD(
                const std::vector<std::vector<uint64_t>> &tid_lists);

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

            // PHASE 2 TASK 2.4: IndexGCInterface implementation
            // PHASE 1.5 TASK 1.5.2f: Migrated to TID struct API
            // Remove index entries pointing to dead tuples
            // Called by garbage collector after heap sweep identifies dead TIDs
            Status removeDeadEntries(const std::vector<TID> &dead_tids,
                                     uint64_t *entries_removed_out = nullptr,
                                     uint64_t *pages_modified_out = nullptr,
                                     ErrorContext *ctx = nullptr) override;

            // Get index type name for logging
            const char *indexTypeName() const override
            {
                return "GIN";
            }

            /**
             * PHASE 5 TASK 5.3.3: Update TIDs after tablespace migration
             *
             * Traverses all parts of the GIN index structure and updates TID references
             * based on the provided TID mapping. Updates:
             * 1. Pending list entries (if present)
             * 2. Posting list pages (small lists)
             * 3. Posting tree leaf nodes (large lists)
             *
             * @param tid_mapping Map from old TID (uint64_t) to new TID (uint64_t)
             * @param tids_updated_out Output: number of TIDs updated
             * @param pages_modified_out Output: number of pages modified
             * @param ctx Error context
             * @return Status::OK if successful
             */
            Status updateTIDsAfterMigration(
                const std::unordered_map<uint64_t, uint64_t> &tid_mapping,
                uint64_t *tids_updated_out = nullptr,
                uint64_t *pages_modified_out = nullptr,
                ErrorContext *ctx = nullptr);

        private:
            Database *db_;
            BufferPool *buffer_pool_;
            UuidV7Bytes index_uuid_;
            uint32_t meta_page_;

            // Helper: Insert into pending list (internal - uses legacy format)
            Status insertIntoPendingList(const std::vector<uint8_t> &key, uint64_t tuple_id_legacy,
                                         ErrorContext *ctx);

            // Helper: Find or create posting list for a key
            Status findOrCreatePostingList(const std::vector<uint8_t> &key,
                                           uint64_t *posting_page_out,
                                           ErrorContext *ctx);

            // Helper: Insert TID into posting list (internal - uses legacy format)
            Status insertIntoPostingList(uint32_t posting_page, uint64_t tuple_id_legacy,
                                         ErrorContext *ctx);

            // Helper: Convert posting list to posting tree
            Status convertListToTree(uint32_t posting_page, ErrorContext *ctx);

            // === Posting Tree Operations ===

            // Helper: Insert TID into posting tree (internal - uses legacy format)
            Status insertIntoPostingTree(uint32_t posting_page, uint64_t tid_legacy,
                                         ErrorContext *ctx);

            // Helper: Search for TID in posting tree (returns true if found)
            bool searchPostingTree(uint32_t tree_root_page, uint64_t tid,
                                   ErrorContext *ctx);

            // Helper: Get all TIDs from posting tree
            Status getPostingTreeTids(uint32_t tree_root_page,
                                      std::vector<uint64_t> *tids_out,
                                      ErrorContext *ctx);

            // Helper: Insert into posting tree leaf node (may cause split)
            Status insertIntoPostingTreeLeaf(uint32_t leaf_page, uint64_t tid,
                                             uint32_t *new_sibling_out,
                                             uint64_t *separator_tid_out,
                                             ErrorContext *ctx);

            // Helper: Insert into posting tree internal node (may cause split)
            Status insertIntoPostingTreeInternal(uint32_t internal_page,
                                                 uint64_t separator_tid,
                                                 uint32_t child_page,
                                                 uint32_t *new_sibling_out,
                                                 uint64_t *separator_tid_out,
                                                 ErrorContext *ctx);

            // Helper: Split posting tree leaf node
            Status splitPostingTreeLeaf(uint32_t leaf_page,
                                        uint32_t *new_sibling_out,
                                        uint64_t *separator_tid_out,
                                        ErrorContext *ctx);

            // Helper: Split posting tree internal node
            Status splitPostingTreeInternal(uint32_t internal_page,
                                            uint32_t *new_sibling_out,
                                            uint64_t *separator_tid_out,
                                            ErrorContext *ctx);

            // Helper: Find leaf page for TID in posting tree
            Status findPostingTreeLeaf(uint32_t tree_root_page, uint64_t tid,
                                       uint32_t *leaf_page_out,
                                       ErrorContext *ctx);

            // === Entry Tree (Keys B-Tree) Operations ===

            // Helper: Search for key in keys B-Tree
            Status searchKeysTree(const std::vector<uint8_t> &key,
                                  uint64_t *posting_page_out,
                                  ErrorContext *ctx);

            // Helper: Insert key into keys B-Tree
            Status insertIntoKeysTree(const std::vector<uint8_t> &key,
                                      uint64_t posting_page,
                                      ErrorContext *ctx);

            // Helper: Find leaf page for key in entry tree
            Status findEntryTreeLeaf(uint32_t tree_root, const std::vector<uint8_t> &key,
                                     uint32_t *leaf_page_out,
                                     ErrorContext *ctx);

            // Helper: Search for key in entry tree leaf page
            Status searchEntryTreeLeaf(uint32_t leaf_page, const std::vector<uint8_t> &key,
                                       GinEntryTreeValue *value_out, int32_t *position_out,
                                       ErrorContext *ctx);

            // Helper: Insert into entry tree leaf (may cause split)
            Status insertIntoEntryTreeLeaf(uint32_t leaf_page,
                                           const std::vector<uint8_t> &key,
                                           const GinEntryTreeValue &value,
                                           uint32_t *new_sibling_out,
                                           std::vector<uint8_t> *separator_key_out,
                                           ErrorContext *ctx);

            // Helper: Split entry tree leaf node
            Status splitEntryTreeLeaf(uint32_t leaf_page,
                                      uint32_t *new_sibling_out,
                                      std::vector<uint8_t> *separator_key_out,
                                      ErrorContext *ctx);

            // Helper: Insert into entry tree internal node
            Status insertIntoEntryTreeInternal(uint32_t internal_page,
                                               const std::vector<uint8_t> &separator_key,
                                               uint32_t child_page,
                                               uint32_t *new_sibling_out,
                                               std::vector<uint8_t> *separator_key_out,
                                               ErrorContext *ctx);

            // Helper: Split entry tree internal node
            Status splitEntryTreeInternal(uint32_t internal_page,
                                          uint32_t *new_sibling_out,
                                          std::vector<uint8_t> *separator_key_out,
                                          ErrorContext *ctx);

            // Helper: Create new entry tree root
            Status createNewEntryTreeRoot(uint32_t left_child, uint32_t right_child,
                                          const std::vector<uint8_t> &separator_key,
                                          uint32_t *new_root_out,
                                          ErrorContext *ctx);

            // Helper: Compare two keys (returns <0, 0, >0)
            static int compareKeys(const std::vector<uint8_t> &key1,
                                   const std::vector<uint8_t> &key2);

            // Helper: Get all TIDs from posting list
            Status getPostingListTids(uint32_t posting_page,
                                      std::vector<uint64_t> *tids_out,
                                      ErrorContext *ctx);

            // Helper: Merge sorted TID lists (for AND operation)
            static std::vector<uint64_t> mergeTidLists(
                const std::vector<std::vector<uint64_t>> &tid_lists);

            // Helper: Union sorted TID lists (for OR operation)
            static std::vector<uint64_t> unionTidLists(
                const std::vector<std::vector<uint64_t>> &tid_lists);

            // ===== Firebird MGA: TIP-based Visibility Helpers =====

            // Helper: Check if a transaction is visible to current transaction
            // Uses TIP (Transaction Inventory Pages), NOT snapshots
            // Returns true if the transaction (xmin) is visible to the reader (current_xid)
            bool isTransactionVisible(uint64_t xmin, uint64_t current_xid, ErrorContext *ctx);

            // Helper: Filter TID list by heap tuple visibility
            // For each TID, checks if the corresponding heap tuple is visible to current transaction
            // Uses TIP-based visibility (Firebird MGA), NOT snapshots
            // Returns a new vector containing only visible TIDs
            std::vector<uint64_t> filterTidsByVisibility(const std::vector<uint64_t> &tids,
                                                          uint64_t current_xid,
                                                          ErrorContext *ctx);

            // ===== Phase 6 Helper Methods =====

            // Helper: Scan entry tree for keys in range
            Status scanEntriesInRange(
                const std::vector<uint8_t> &lower_bound,
                const std::vector<uint8_t> &upper_bound,
                bool lower_inclusive,
                bool upper_inclusive,
                std::vector<std::vector<uint8_t>> *matching_keys_out,
                ErrorContext *ctx);

            // Helper: Pattern matching (wildcard)
            static bool matchesPattern(
                const std::vector<uint8_t> &key,
                const std::string &pattern);

            // Helper: Levenshtein distance
            static uint32_t levenshteinDistance(
                const std::vector<uint8_t> &s1,
                const std::vector<uint8_t> &s2);

            // Helper: Detect SIMD capabilities
            static bool hasSIMDSupport();

            // Helper: SIMD-optimized two-pointer intersection
            static std::vector<uint64_t> intersectTwoListsSIMD(
                const std::vector<uint64_t> &list1,
                const std::vector<uint64_t> &list2);

            // No copy or move
            GinIndex(const GinIndex &) = delete;
            GinIndex &operator=(const GinIndex &) = delete;
            GinIndex(GinIndex &&) = delete;
            GinIndex &operator=(GinIndex &&) = delete;
        };

    } // namespace core
} // namespace scratchbird
