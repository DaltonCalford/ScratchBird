#pragma once

#include "scratchbird/core/status.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include <cstdint>
#include <vector>
#include <memory>

namespace scratchbird
{
    namespace core
    {

        // Forward declarations
        class Database;
        class BufferPool;
        class PageManager;
        struct ErrorContext;

// Page flags
enum class BTreeFlags : uint16_t {
    LEAF = 0x0001,           // Leaf page
    ROOT = 0x0002,           // Root page
    RIGHTMOST = 0x0004,      // Rightmost page at this level
    LEFTMOST = 0x0008,       // Leftmost page at this level
    COMPRESSED = 0x0010,     // Compression enabled
    ENCRYPTED = 0x0020,      // Page is encrypted
    HAS_GARBAGE = 0x0040,    // Page has deleted entries
    INCOMPLETE = 0x0080      // Split in progress
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
enum class BTreeNodeFlags : uint16_t {
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

        static_assert(sizeof(SBBTreePage) == 160, "SBBTreePage size must be 160 bytes");
        static_assert(sizeof(SBBTreeNode) == 36, "SBBTreeNode size must be 36 bytes");

        // In-memory representation of a B-Tree index
        struct SBBTreeIndex
        {
            ID idx_uuid;
            ID idx_table_uuid;
            std::vector<ID> idx_column_ids;  // Fixed: Changed from uint16_t to ID (UUID)
            uint32_t idx_flags;

            uint64_t idx_root_page;
            uint16_t idx_height;

            uint64_t idx_tuple_count;
            uint64_t idx_page_count;
            uint64_t idx_deleted_count;
        };

        // B-tree implementation
        class BTree
        {
        public:
            BTree(Database *db, SBBTreeIndex index_info);
            ~BTree();

            Status insert(const std::vector<uint8_t> &key, uint64_t tuple_id,
                          ErrorContext *ctx = nullptr);
            Status search(const std::vector<uint8_t> &key, std::vector<uint64_t> *tuple_ids_out,
                          ErrorContext *ctx = nullptr);
            Status remove(const std::vector<uint8_t> &key, uint64_t tuple_id,
                          ErrorContext *ctx = nullptr);

        private:
            Database *db_;
            SBBTreeIndex index_info_;

            // Traverses the B-Tree to find the correct leaf page for a given key.
            Status find_leaf_page(const std::vector<uint8_t> &key, uint64_t *page_num_out,
                                  bool write_lock, ErrorContext *ctx);
        };

    } // namespace core
} // namespace scratchbird
