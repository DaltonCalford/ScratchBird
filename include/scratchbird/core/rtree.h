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
#include "scratchbird/core/rtree_node.h"
#include "scratchbird/core/tid.h"
#include "scratchbird/core/ondisk.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/uuidv7.h"
#include "scratchbird/core/index_gc_interface.h"
#include "scratchbird/core/gpid.h"
#include <cstdint>
#include <vector>
#include <memory>
#include <shared_mutex>
#include <unordered_map>

namespace scratchbird::core
{

// Forward declarations
class Database;
class BufferPool;
class PageManager;
class TransactionManager;
struct ErrorContext;
// Firebird MGA: TransactionManager used for TIP-based visibility checks (NOT snapshots)

/**
 * RTree - R-tree Spatial Index with R*-tree optimizations
 *
 * R-trees are balanced search trees for indexing multi-dimensional spatial data.
 * They are particularly efficient for spatial queries like:
 * - Point queries (WHERE ST_Contains(geom, point))
 * - Range queries (WHERE ST_Intersects(geom, bbox))
 * - Nearest neighbor queries (ORDER BY ST_Distance(geom, point))
 *
 * ## Architecture (Firebird MGA with Stable TIDs)
 *
 * ```
 * Root (Level 2):     [MBR1 → N1] [MBR2 → N2]
 *                          |            |
 * Internal (Level 1): [MBR3→N3] ... [MBR6→N6]
 *                          |            |
 * Leaf (Level 0):     [MBR7→TID:1] [MBR8→TID:2] ...
 *                           ↓              ↓
 *                      Heap Tuple 1   Heap Tuple 2
 *                      xmin=10        xmin=11
 *                      xmax=0         xmax=15 (deleted)
 * ```
 *
 * ## R*-tree Optimizations
 *
 * This implementation uses R*-tree variant with the following optimizations:
 *
 * 1. **ChooseLeaf with overlap minimization**:
 *    - For leaf level insertions, minimize overlap increase
 *    - For higher levels, minimize area enlargement
 *
 * 2. **Forced reinsert**:
 *    - On node overflow, first try reinserting a percentage of entries
 *    - Only split if reinsertion doesn't resolve overflow
 *    - Improves tree structure quality
 *
 * 3. **Quadratic split algorithm**:
 *    - Choose seeds that maximize wasted area
 *    - Assign entries to minimize total area
 *
 * ## MGA Compliance (Phase 6 - November 2025)
 *
 * - **xmin/xmax tracking**: Each entry has xmin/xmax
 * - **TIP-based visibility**: TransactionId parameter in search/insert APIs (NOT snapshots)
 * - **Visibility filtering**: During tree traversal, skip deleted entries using TIP
 * - **Garbage collection**: removeDeadEntries() for dead entry removal
 * - **Stable TIDs**: Entries reference stable tuple IDs (heap TIDs)
 *
 * ## Concurrency
 *
 * - Thread-safe for concurrent reads using std::shared_mutex
 * - Write operations require exclusive lock
 * - Read operations (search) acquire shared lock
 *
 * ## Performance Characteristics
 *
 * - Insert: O(log N) average, O(N) worst case
 * - Search: O(log N) for point queries, O(√N) for range queries
 * - Delete: O(log N) average
 * - Space: O(N) where N is number of entries
 */

// R-tree page flags
enum class RTreeFlags : uint16_t
{
    ROOT = 0x0001,        // Root page
    LEAF = 0x0002,        // Leaf page
    HAS_GARBAGE = 0x0004, // Page has deleted entries
    NEEDS_REPACK = 0x0008 // Page needs reorganization
};

// R-tree node flags (for entries)
enum class RTreeNodeFlags : uint16_t
{
    DELETED = 0x0001,     // Logically deleted (xmax set)
    OVERFLOW = 0x0002     // Entry caused overflow (reinserted)
};

#pragma pack(push, 1)
/**
 * R-tree page structure
 *
 * Stores R-tree nodes with their MBRs and child pointers or tuple IDs.
 */
struct SBRTreePage
{
    // Standard page header
    PageHeader rtree_header; // Standard ScratchBird page header

    // Index identification
    ID rtree_index_uuid; // Index UUID v7
    ID rtree_table_uuid; // Table this index belongs to

    // R-tree metadata
    uint16_t rtree_flags;      // Page flags (see RTreeFlags)
    uint16_t rtree_count;      // Number of entries on page
    uint16_t rtree_free_space; // Free space in bytes
    uint16_t rtree_level;      // Tree level (0 = leaf)
    uint32_t rtree_max_entries; // Max entries per node (M)

    // Sibling navigation
    uint64_t rtree_left_sibling;  // Left sibling page number
    uint64_t rtree_right_sibling; // Right sibling page number
    uint64_t rtree_parent_page;   // Parent page number

    // MGA compliance
    uint64_t rtree_xmin; // Page creation transaction
    uint64_t rtree_xmax; // Page deletion transaction (0 if active)
    uint64_t rtree_lsn;  // Last LSN that modified this page

    // Statistics
    uint64_t rtree_total_entries;   // Total entries in entire index
    uint64_t rtree_deleted_entries; // Deleted entries (need GC compaction)
    uint64_t rtree_height;          // Tree height

    uint8_t rtree_padding[16]; // Reserved for future use

    // Entries follow immediately after header
    // Each entry is variable-size: RTreeEntry structure
};

/**
 * R-tree on-disk entry structure
 *
 * Variable-size structure stored on R-tree pages.
 */
struct SBRTreeEntry
{
    // Bounding box (32 bytes)
    double entry_min_x;
    double entry_min_y;
    double entry_max_x;
    double entry_max_y;

    // Union for leaf vs internal node
    union {
        TID entry_row_id;         // For leaf: tuple ID (16 bytes)
        uint64_t entry_child_page; // For internal: child page (8 bytes)
    };

    // MGA compliance
    uint64_t entry_xmin; // Transaction that created this entry
    uint64_t entry_xmax; // Transaction that deleted this entry

    uint16_t entry_flags;   // Entry flags (see RTreeNodeFlags)
    uint8_t entry_padding[6]; // Alignment
};
#pragma pack(pop)

// Note: Static asserts disabled during development
// Will be re-enabled once page layout is finalized
// static_assert(sizeof(SBRTreePage) == 168, "SBRTreePage size must be 168 bytes");
// static_assert(sizeof(SBRTreeEntry) == 64, "SBRTreeEntry size must be 64 bytes");

/**
 * In-memory representation of an R-tree index
 */
struct SBRTreeIndex
{
    ID idx_uuid;
    ID idx_table_uuid;
    std::vector<ID> idx_column_ids; // Columns indexed (typically 1 for spatial)
    uint32_t idx_flags;

    uint64_t idx_root_page;
    uint16_t idx_tablespace_id = 0;
    uint16_t idx_height;
    uint32_t idx_max_entries; // M parameter (default: 50)

    uint64_t idx_entry_count;
    uint64_t idx_page_count;
    uint64_t idx_deleted_count;
};

/**
 * RTree - R-tree spatial index implementation
 *
 * Implements IndexGCInterface for garbage collection integration.
 */
class RTree : public IndexGCInterface
{
public:
    /**
     * Constructor
     *
     * @param db Database instance
     * @param index_info Index metadata
     */
    RTree(Database* db, SBRTreeIndex index_info);

    /**
     * Destructor
     */
    ~RTree();

    /**
     * Create a new R-tree index
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param table_uuid Table UUID
     * @param column_uuids Column UUIDs to index
     * @param max_entries Maximum entries per node (default: 50)
     * @param root_gpid Root page GPID
     * @param ctx Error context
     * @return Status
     */
    static Status create(Database* db,
                        const UuidV7Bytes& index_uuid,
                        const UuidV7Bytes& table_uuid,
                        const std::vector<UuidV7Bytes>& column_uuids,
                        uint32_t max_entries,
                        GPID root_gpid,
                        ErrorContext* ctx = nullptr);

    /**
     * Open an existing R-tree index
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param root_gpid Root page GPID
     * @param max_entries Maximum entries per node
     * @param ctx Error context
     * @return Unique pointer to RTree instance
     */
    static std::unique_ptr<RTree> open(Database* db,
                                       const UuidV7Bytes& index_uuid,
                                       GPID root_gpid,
                                       uint32_t max_entries,
                                       ErrorContext* ctx = nullptr);

    // ========================================================================
    // Public API
    // ========================================================================

    /**
     * Insert a spatial entry into the R-tree
     *
     * @param bbox Bounding box of the spatial object
     * @param tid Tuple ID (row ID in heap)
     * @param current_xid Current transaction ID for TIP-based visibility
     * @param ctx Error context
     * @return Status
     *
     * Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
     */
    Status insert(const BoundingBox& bbox,
                 const TID& tid,
                 uint64_t current_xid,
                 ErrorContext* ctx = nullptr);

    /**
     * Search for entries that intersect with the given bounding box
     *
     * Firebird MGA: Uses TIP-based visibility filtering (NOT snapshots)
     *
     * @param bbox Query bounding box
     * @param current_xid Current transaction ID for TIP-based visibility
     * @param tids_out Output: matching tuple IDs
     * @param ctx Error context
     * @return Status
     *
     * Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
     */
    Status search(const BoundingBox& bbox,
                 uint64_t current_xid,
                 std::vector<TID>* tids_out,
                 ErrorContext* ctx = nullptr);

    /**
     * Remove a spatial entry from the R-tree
     *
     * @param bbox Bounding box of the spatial object
     * @param tid Tuple ID to remove
     * @param current_xid Current transaction ID for TIP-based visibility
     * @param ctx Error context
     * @return Status
     *
     * Per MGA_RULES.md Rule 11: Use TransactionId, NOT Snapshot*
     */
    Status remove(const BoundingBox& bbox,
                 const TID& tid,
                 uint64_t current_xid,
                 ErrorContext* ctx = nullptr);

    /**
     * Clear all entries from the R-tree
     *
     * @param ctx Error context
     * @return Status
     */
    Status clear(ErrorContext* ctx = nullptr);

    // ========================================================================
    // IndexGCInterface Implementation
    // ========================================================================

    /**
     * Remove dead entries from the index (garbage collection)
     *
     * @param dead_tids Vector of TIDs that are confirmed dead
     * @param entries_removed_out Output: number of entries removed
     * @param pages_modified_out Output: number of pages modified
     * @param ctx Error context
     * @return Status
     */
    Status removeDeadEntries(const std::vector<TID>& dead_tids,
                            uint64_t* entries_removed_out = nullptr,
                            uint64_t* pages_modified_out = nullptr,
                            ErrorContext* ctx = nullptr) override;

    /**
     * Update leaf entry TIDs after table migration.
     *
     * Uses the provided old->new GPID mapping to rewrite leaf TIDs.
     */
    Status updateTIDsAfterMigration(const std::unordered_map<TID, TID>& tid_mapping,
                                   uint64_t* tids_updated_out = nullptr,
                                   uint64_t* pages_modified_out = nullptr,
                                   ErrorContext* ctx = nullptr);

    /**
     * Get index type name for logging
     */
    const char* indexTypeName() const override { return "R-Tree"; }

    // ========================================================================
    // Statistics and Metadata
    // ========================================================================

    /**
     * Get index statistics
     *
     * @return Index metadata
     */
    const SBRTreeIndex& getIndexInfo() const { return index_info_; }

    /**
     * Get total number of entries (including deleted)
     */
    uint64_t getTotalEntries() const;

    /**
     * Get number of deleted entries
     */
    uint64_t getDeletedEntries() const;

    /**
     * Get tree height
     */
    uint16_t getHeight() const;

private:
    Database* db_;
    SBRTreeIndex index_info_;

    // In-memory root node (cached for performance)
    std::unique_ptr<RTreeNode> root_;

    GPID indexGPID(uint64_t page_num) const;
    Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext *ctx = nullptr);
    Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext *ctx = nullptr);

    // Thread-safety: shared_mutex allows concurrent reads
    mutable std::shared_mutex mutex_;

    // ========================================================================
    // Core R-tree Algorithms
    // ========================================================================

    /**
     * Choose the leaf node to insert a new entry
     *
     * Algorithm:
     * 1. Start from root
     * 2. At each level, choose the child with minimum enlargement
     * 3. For leaf level, also consider overlap minimization (R*-tree)
     * 4. Return the leaf node
     *
     * @param bbox Bounding box to insert
     * @param current_xid Current transaction ID for TIP-based visibility checks
     * @return Leaf node to insert into
     */
    RTreeNode* chooseLeaf(const BoundingBox& bbox, uint64_t current_xid);

    /**
     * Split a node that has overflowed
     *
     * Uses R*-tree quadratic split algorithm:
     * 1. Pick two seeds (maximize wasted area)
     * 2. Distribute remaining entries
     * 3. Minimize total area
     *
     * @param node Node to split
     * @param new_entry Entry that caused the overflow
     * @return New sibling node
     */
    std::unique_ptr<RTreeNode> splitNode(RTreeNode* node, const RTreeEntry& new_entry);

    /**
     * Adjust the tree after insertion or deletion
     *
     * Algorithm:
     * 1. Update MBR of the modified node
     * 2. Propagate MBR changes up to the root
     * 3. Split nodes if necessary
     * 4. Adjust height if root split
     *
     * @param leaf Leaf node that was modified
     * @param new_sibling New sibling from split (nullptr if no split)
     * @param current_xid Current transaction ID for TIP-based visibility checks
     */
    void adjustTree(RTreeNode* leaf, RTreeNode* new_sibling, uint64_t current_xid);

    /**
     * Forced reinsert optimization (R*-tree)
     *
     * When a node overflows, first try reinserting a percentage of entries.
     * This improves tree structure quality.
     *
     * @param node Node that overflowed
     * @param new_entry Entry that caused overflow
     * @param current_xid Current transaction ID for TIP-based visibility checks
     * @return true if reinsert resolved overflow, false if split needed
     */
    bool forceReinsert(RTreeNode* node, const RTreeEntry& new_entry, uint64_t current_xid);

    /**
     * Condense the tree after deletion
     *
     * Algorithm:
     * 1. Remove the entry from leaf
     * 2. If node underflows, eliminate it and reinsert entries
     * 3. Propagate changes up to root
     * 4. Adjust height if root has only one child
     *
     * @param leaf Leaf node to remove from
     * @param entry_index Index of entry to remove
     * @param current_xid Current transaction ID for TIP-based visibility checks
     */
    void condenseTree(RTreeNode* leaf, size_t entry_index, uint64_t current_xid);

    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * Load a node from disk
     *
     * @param page_number Page number
     * @return Node instance
     */
    std::unique_ptr<RTreeNode> loadNode(uint64_t page_number);

    /**
     * Save a node to disk
     *
     * @param node Node to save
     * @param ctx Error context
     * @return Status
     */
    Status saveNode(RTreeNode* node, ErrorContext* ctx);

    /**
     * Allocate a new page for a node
     *
     * @param node Node to allocate page for
     * @param ctx Error context
     * @return Status
     */
    Status allocatePage(RTreeNode* node, ErrorContext* ctx);

    /**
     * Check if an entry is visible to the current transaction
     *
     * Firebird MGA: Uses TIP-based visibility checking (NOT snapshots)
     *
     * @param entry Entry to check
     * @param current_xid Current transaction ID for TIP-based visibility
     * @return true if visible
     */
    bool isEntryVisible(const RTreeEntry& entry, uint64_t current_xid) const;

    /**
     * Update index statistics
     */
    void updateStatistics();
};

} // namespace scratchbird::core
