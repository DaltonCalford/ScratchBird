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
#include <functional>
#include <string>
#include <map>
#include <set>
#include <unordered_map>

namespace scratchbird::core
{

// Forward declarations
class Database;
class BufferPool;
class PageManager;
class TransactionManager;
struct ErrorContext;

/**
 * SP-GiST (Space-Partitioned Generalized Search Tree)
 *
 * SP-GiST supports unbalanced tree structures with space partitioning, making it
 * ideal for implementing:
 * - Quad-trees (2D point data)
 * - k-d trees (multi-dimensional data)
 * - Radix trees (string prefix search, LIKE 'abc%')
 * - Suffix trees (string suffix search)
 * - Range trees (multi-dimensional ranges)
 *
 * ## Key Differences from GiST
 *
 * | Feature | GiST | SP-GiST |
 * |---------|------|---------|
 * | Tree Balance | Balanced | Unbalanced allowed |
 * | Node Type | Single type | Inner/Leaf distinction |
 * | Partitioning | Overlapping regions | Non-overlapping partitions |
 * | Split Logic | Data-driven | Space-driven |
 * | Use Cases | Geometric (R-Tree) | Points, Strings, Discrete spaces |
 *
 * ## Architecture
 *
 * ```
 * Inner Node (Partitions space):
 *     [Partition 1] → Inner Node
 *     [Partition 2] → Inner Node
 *     [Partition 3] → Leaf Node
 *     [Partition 4] → Leaf Node
 *
 * Leaf Node (Stores data):
 *     [Value → TID:1]
 *     [Value → TID:2]
 *     [Value → TID:3]
 * ```
 *
 * ## Example: Quad-Tree
 *
 * ```
 * Root (splits space into 4 quadrants):
 *     NW → Inner (splits again)
 *          NW → Leaf [points in NW-NW]
 *          NE → Leaf [points in NW-NE]
 *          SW → Leaf [points in NW-SW]
 *          SE → Leaf [points in NW-SE]
 *     NE → Leaf [points in NE]
 *     SW → Leaf [points in SW]
 *     SE → Leaf [points in SE]
 * ```
 *
 * ## Example: Radix Tree (Text)
 *
 * ```
 * Root:
 *     'a' → Inner
 *           'p' → Leaf [apple, application]
 *           'r' → Leaf [art, arrow]
 *     'b' → Inner
 *           'a' → Leaf [ball, bat]
 *           'e' → Leaf [bear, beer]
 * ```
 *
 * ## MGA Compliance
 *
 * - TIP-based visibility (no snapshots)
 * - xmin/xmax tracking on entries
 * - Stable TID references
 * - Garbage collection support
 */

// SP-GiST node type
enum class SPGiSTNodeType : uint16_t
{
    INNER = 0,  // Inner node (partitions space)
    LEAF = 1    // Leaf node (stores values and TIDs)
};

// SP-GiST page flags
enum class SPGiSTFlags : uint16_t
{
    ROOT = 0x0001,        // Root page
    HAS_GARBAGE = 0x0002, // Page has deleted entries
    NEEDS_REPACK = 0x0004 // Page needs reorganization
};

#pragma pack(push, 1)

/**
 * SP-GiST on-disk page structure
 */
struct SBSPGiSTPage
{
    // Standard page header (64 bytes)
    PageHeader spgist_header;

    // Index identification (32 bytes)
    ID spgist_index_uuid;  // Index UUID v7 (16 bytes)
    ID spgist_table_uuid;  // Table UUID (16 bytes)

    // SP-GiST metadata (24 bytes)
    uint16_t spgist_flags;      // Page flags
    uint16_t spgist_node_type;  // SPGiSTNodeType (INNER or LEAF)
    uint16_t spgist_count;      // Number of entries on page
    uint16_t spgist_free_space; // Free space in bytes
    uint32_t spgist_opclass_id; // Operator class ID
    uint64_t spgist_parent_page; // Parent page number

    // MGA compliance (24 bytes)
    uint64_t spgist_xmin;            // Page creation transaction
    uint64_t spgist_xmax;            // Page deletion transaction (0 if active)
    uint64_t spgist_lsn;             // Last LSN that modified this page

    // Statistics (16 bytes)
    uint64_t spgist_total_entries;   // Total entries in entire index
    uint64_t spgist_deleted_entries; // Deleted entries

    uint8_t spgist_padding[52]; // Padding to align entries (total: 208 bytes)

    // Variable-size entries follow immediately after header
};

/**
 * SP-GiST inner tuple (partition descriptor)
 *
 * Inner tuples define how space is partitioned and which child
 * nodes correspond to which partitions.
 */
struct SBSPGiSTInnerTuple
{
    uint16_t inner_size;        // Total size of this entry
    uint16_t inner_nNodes;      // Number of child nodes
    uint16_t inner_prefixSize;  // Size of prefix data (e.g., radix tree prefix)
    uint16_t inner_reserved;    // Reserved for alignment

    // MGA compliance (16 bytes)
    uint64_t inner_xmin; // Transaction that created this entry
    uint64_t inner_xmax; // Transaction that deleted this entry (0 if active)

    // Variable-length data follows:
    // 1. Prefix data (inner_prefixSize bytes)
    // 2. Node labels array (inner_nNodes * label_size bytes)
    // 3. Child page numbers (inner_nNodes * 8 bytes)
};

/**
 * SP-GiST leaf tuple (actual indexed value)
 *
 * Leaf tuples store the actual indexed values and their corresponding TIDs.
 */
struct SBSPGiSTLeafTuple
{
    uint16_t leaf_size;       // Total size of this entry
    uint16_t leaf_valueSize;  // Size of value data
    uint32_t leaf_reserved;   // Reserved for alignment

    // Heap tuple reference (16 bytes)
    TID leaf_tid;             // Tuple ID in heap

    // MGA compliance (16 bytes)
    uint64_t leaf_xmin;       // Transaction that created this entry
    uint64_t leaf_xmax;       // Transaction that deleted this entry (0 if active)

    // Variable-length value data follows (leaf_valueSize bytes)
};

#pragma pack(pop)

static_assert(sizeof(SBSPGiSTPage) == 224, "SP-GiST page header must be 224 bytes (80-byte PageHeader + 144 bytes)");
static_assert(sizeof(SBSPGiSTInnerTuple) == 24, "SP-GiST inner tuple header must be 24 bytes");
static_assert(sizeof(SBSPGiSTLeafTuple) == 40, "SP-GiST leaf tuple header must be 40 bytes");

/**
 * In-memory representation of node label
 *
 * Labels identify which partition a child node represents.
 * For quad-trees: NW, NE, SW, SE
 * For radix trees: character prefixes
 */
struct SPGiSTNodeLabel
{
    std::vector<uint8_t> data; // Opaque label data
    uint64_t child_page;       // Page number of child node

    SPGiSTNodeLabel() : child_page(0) {}
    SPGiSTNodeLabel(const std::vector<uint8_t>& d, uint64_t page)
        : data(d), child_page(page) {}
};

/**
 * SP-GiST traversal result
 *
 * Returned by choose() to indicate where to insert/search
 */
enum class SPGiSTMatchType
{
    MATCH_NODE,     // Descend into specific child node
    MATCH_SPLIT,    // Need to split current node
    MATCH_ADD_NODE  // Add new node at this level
};

struct SPGiSTTraversal
{
    SPGiSTMatchType match_type;
    size_t node_index;           // Which child to descend into (if MATCH_NODE)
    std::vector<uint8_t> prefix; // Prefix for new node (if MATCH_ADD_NODE)
    std::vector<std::vector<uint8_t>> new_labels; // Labels for split (if MATCH_SPLIT)
};

/**
 * SP-GiST operator class interface
 *
 * Defines how SP-GiST works with a specific data type through
 * space partitioning logic.
 */
class SPGiSTOperatorClass
{
public:
    virtual ~SPGiSTOperatorClass() = default;

    // Operator class identification
    virtual uint32_t getOpClassId() const = 0;
    virtual std::string getOpClassName() const = 0;

    /**
     * Config - Return configuration parameters
     *
     * @param prefixType Type info for prefix data (optional)
     * @param labelType Type info for node labels
     * @param leafType Type info for leaf values
     * @param canReturnData Whether index-only scans are supported
     */
    struct Config
    {
        bool canReturnData;     // Supports index-only scans
        size_t labelSize;       // Fixed size of labels (0 = variable)
        size_t maxInnerNodes;   // Maximum children per inner node
    };

    virtual Config config() const = 0;

    /**
     * Choose - Determine where to insert/search in inner node
     *
     * @param innerPrefix Prefix data from inner node
     * @param nodeLabels Labels of all child nodes
     * @param query The value being inserted/searched
     * @return Traversal decision (which child, or split/add)
     */
    virtual SPGiSTTraversal choose(
        const std::vector<uint8_t>& innerPrefix,
        const std::vector<SPGiSTNodeLabel>& nodeLabels,
        const std::vector<uint8_t>& query) const = 0;

    /**
     * PickSplit - Decide how to split an overfull node
     *
     * @param values All values in the node
     * @param prefix Output: prefix for this inner node
     * @param labels Output: labels for child nodes
     * @param assignments Output: which child each value goes to
     */
    virtual void pickSplit(
        const std::vector<std::vector<uint8_t>>& values,
        std::vector<uint8_t>& prefix,
        std::vector<std::vector<uint8_t>>& labels,
        std::vector<size_t>& assignments) const = 0;

    /**
     * InnerConsistent - Test if inner node could contain matching values
     *
     * @param innerPrefix Prefix data from inner node
     * @param nodeLabel Label of child node being considered
     * @param query The search query
     * @return true if this child could contain matches
     */
    virtual bool innerConsistent(
        const std::vector<uint8_t>& innerPrefix,
        const std::vector<uint8_t>& nodeLabel,
        const std::vector<uint8_t>& query) const = 0;

    /**
     * LeafConsistent - Test if leaf value matches query
     *
     * @param leafValue The value stored in leaf
     * @param query The search query
     * @return true if leaf value matches query
     */
    virtual bool leafConsistent(
        const std::vector<uint8_t>& leafValue,
        const std::vector<uint8_t>& query) const = 0;
};

/**
 * SP-GiST index implementation
 */
class SPGiSTIndex : public IndexGCInterface
{
public:
    /**
     * Constructor - requires database and index configuration
     */
    SPGiSTIndex(Database* db,
                const ID& index_uuid,
                const ID& table_uuid,
                const std::vector<ID>& column_ids,
                std::shared_ptr<SPGiSTOperatorClass> opclass);

    /**
     * Create a new SP-GiST index
     * Allocates root page and initializes the index structure
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param table_uuid Table UUID
     * @param column_ids Columns being indexed
     * @param opclass Operator class to use
     * @param root_gpid Root page GPID
     * @param ctx Error context
     * @return Status code
     */
    static Status create(Database* db,
                        const ID& index_uuid,
                        const ID& table_uuid,
                        const std::vector<ID>& column_ids,
                        std::shared_ptr<SPGiSTOperatorClass> opclass,
                        GPID root_gpid,
                        ErrorContext* ctx = nullptr);

    /**
     * Open an existing SP-GiST index
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param table_uuid Table UUID
     * @param column_ids Columns being indexed
     * @param opclass Operator class to use
     * @param root_gpid Root page GPID
     * @param ctx Error context
     * @return Unique pointer to opened index, or nullptr on error
     */
    static std::unique_ptr<SPGiSTIndex> open(Database* db,
                                             const ID& index_uuid,
                                             const ID& table_uuid,
                                             const std::vector<ID>& column_ids,
                                             std::shared_ptr<SPGiSTOperatorClass> opclass,
                                             GPID root_gpid,
                                             ErrorContext* ctx = nullptr);

    /**
     * Open an existing SP-GiST index (minimal interface for executor template)
     * Loads metadata from catalog and operator class from registry
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param root_gpid Root page GPID
     * @param ctx Error context
     * @return Unique pointer to opened index, or nullptr on error
     */
    static std::unique_ptr<SPGiSTIndex> open(Database* db,
                                             const ID& index_uuid,
                                             GPID root_gpid,
                                             ErrorContext* ctx = nullptr);

    ~SPGiSTIndex();

    // Disable copy/move
    SPGiSTIndex(const SPGiSTIndex&) = delete;
    SPGiSTIndex& operator=(const SPGiSTIndex&) = delete;

    /**
     * Initialize the SP-GiST index
     */
    Status initialize(ErrorContext* ctx);

    /**
     * Insert a value into the SP-GiST index
     *
     * @param value The value to index (serialized)
     * @param tid The tuple ID being indexed
     * @param current_xid Current transaction ID (for MGA visibility)
     * @param ctx Error context
     */
    Status insert(const std::vector<uint8_t>& value,
                  const TID& tid,
                  uint64_t current_xid,
                  ErrorContext* ctx);

    /**
     * Search the SP-GiST index
     *
     * @param query The query value (serialized)
     * @param current_xid Current transaction ID (for MGA visibility)
     * @param results Output: matching tuple IDs
     * @param ctx Error context
     */
    Status search(const std::vector<uint8_t>& query,
                  uint64_t current_xid,
                  std::vector<TID>* results,
                  ErrorContext* ctx);

    /**
     * Delete an entry from the SP-GiST index (logical deletion)
     */
    Status remove(const std::vector<uint8_t>& value,
                  const TID& tid,
                  uint64_t current_xid,
                  ErrorContext* ctx);

    // IndexGCInterface
    Status removeDeadEntries(const std::vector<TID>& dead_tids,
                            uint64_t* entries_removed_out = nullptr,
                            uint64_t* pages_modified_out = nullptr,
                            ErrorContext* ctx = nullptr) override;
    const char* indexTypeName() const override { return "SP-GiST"; }

    // Update TIDs after tablespace migration (GPID remap)
    Status updateTIDsAfterMigration(const std::unordered_map<TID, TID>& tid_mapping,
                                    uint64_t* tids_updated_out = nullptr,
                                    uint64_t* pages_modified_out = nullptr,
                                    ErrorContext* ctx = nullptr);

    // Index metadata
    const ID& getIndexUUID() const { return index_uuid_; }
    const ID& getTableUUID() const { return table_uuid_; }
    uint64_t getRootPage() const { return root_page_; }
    uint64_t getEntryCount() const { return entry_count_; }
    uint64_t getDeletedCount() const { return deleted_count_; }

    // Statistics
    struct SPGiSTStats {
        uint64_t total_entries;
        uint64_t deleted_entries;
        uint64_t max_depth;
        double avg_leaf_density;
    };

    SPGiSTStats getStats() const;

private:
    // Core operations
    Status insertRecursive(uint64_t page_num,
                          const std::vector<uint8_t>& value,
                          const TID& tid,
                          uint64_t current_xid,
                          int level,
                          ErrorContext* ctx);

    Status searchRecursive(uint64_t page_num,
                          const std::vector<uint8_t>& query,
                          uint64_t current_xid,
                          std::vector<TID>& results,
                          ErrorContext* ctx);

    Status splitNode(uint64_t page_num,
                    ErrorContext* ctx);

    Status removeRecursive(uint64_t page_num,
                          const std::vector<uint8_t>& value,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx);

    Status removeDeadEntriesRecursive(uint64_t page_num,
                                     const std::set<TID>& dead_set,
                                     uint64_t* entries_removed,
                                     uint64_t* pages_modified,
                                     ErrorContext* ctx);

    Status updateTIDsAfterMigrationRecursive(uint64_t page_num,
                                             const std::unordered_map<TID, TID>& tid_mapping,
                                             uint64_t* tids_updated,
                                             uint64_t* pages_modified,
                                             ErrorContext* ctx);

    // Helper methods
    bool isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const;
    Status loadPage(uint64_t page_num, SBSPGiSTPage** page, ErrorContext* ctx);
    Status allocatePage(uint64_t* page_num, ErrorContext* ctx);
    GPID indexGPID(uint64_t page_num) const;
    Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext* ctx);
    Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext* ctx);

    void calculateStatsRecursive(uint64_t page_num,
                                 uint64_t current_depth,
                                 uint64_t* max_depth,
                                 uint64_t* total_leaf_pages,
                                 uint64_t* total_leaf_entries) const;

    // Member variables
    Database* db_;
    BufferPool* buffer_pool_;
    TransactionManager* txn_manager_;

    ID index_uuid_;
    ID table_uuid_;
    std::vector<ID> column_ids_;
    std::shared_ptr<SPGiSTOperatorClass> opclass_;

    uint64_t root_page_;
    uint16_t tablespace_id_;
    uint64_t entry_count_;
    uint64_t deleted_count_;

    mutable std::shared_mutex mutex_;
};

/**
 * SP-GiST operator class registry
 */
class SPGiSTOperatorClassRegistry
{
public:
    static SPGiSTOperatorClassRegistry& instance();

    void registerOperatorClass(std::shared_ptr<SPGiSTOperatorClass> opclass);
    std::shared_ptr<SPGiSTOperatorClass> getOperatorClass(uint32_t opclass_id) const;
    std::shared_ptr<SPGiSTOperatorClass> getOperatorClass(const std::string& name) const;

private:
    SPGiSTOperatorClassRegistry() = default;

    std::map<uint32_t, std::shared_ptr<SPGiSTOperatorClass>> opclasses_by_id_;
    std::map<std::string, std::shared_ptr<SPGiSTOperatorClass>> opclasses_by_name_;
    mutable std::shared_mutex mutex_;
};

} // namespace scratchbird::core
