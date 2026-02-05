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
#include <unordered_map>
#include <functional>
#include <string>
#include <map>
#include <set>

namespace scratchbird::core
{

// Forward declarations
class Database;
class BufferPool;
class PageManager;
class TransactionManager;
struct ErrorContext;

/**
 * GiST (Generalized Search Tree) - Extensible Indexing Framework
 *
 * GiST is a balanced tree structure that provides a framework for implementing
 * various index types through operator classes. Unlike traditional indexes that
 * are hardcoded for specific data types, GiST allows custom index types through
 * a well-defined API.
 *
 * ## Architecture
 *
 * ```
 * Root (Level 2):     [Pred1 → N1] [Pred2 → N2]
 *                          |            |
 * Internal (Level 1): [Pred3→N3] ... [Pred6→N6]
 *                          |            |
 * Leaf (Level 0):     [Pred7→TID:1] [Pred8→TID:2] ...
 *                           ↓              ↓
 *                      Heap Tuple 1   Heap Tuple 2
 *                      xmin=10        xmin=11
 *                      xmax=0         xmax=15 (deleted)
 * ```
 *
 * ## Key Concepts
 *
 * - **Predicate**: A value stored in the index tree that describes a property
 *   of its subtree. For R-Trees, this is an MBR. For range types, it's a range.
 * - **Operator Class**: Defines how to work with specific data types through
 *   a set of required methods (consistent, union, penalty, picksplit, etc.)
 * - **Consistent**: Tests if a predicate satisfies a query condition
 * - **Union**: Creates a predicate that covers multiple entries
 * - **Penalty**: Estimates the "cost" of inserting an entry
 * - **Picksplit**: Divides entries when a node overflows
 *
 * ## Built-in Operator Classes
 *
 * 1. **box_ops**: Geometric boxes (R-Tree semantics)
 * 2. **circle_ops**: Circles
 * 3. **polygon_ops**: Polygons
 * 4. **range_ops**: Range types (int4range, tsrange, etc.)
 * 5. **inet_ops**: Network addresses
 * 6. **tsvector_ops**: Text search vectors
 *
 * ## MGA Compliance
 *
 * - **xmin/xmax tracking**: Each entry has transaction IDs
 * - **TIP-based visibility**: Uses TransactionManager for visibility checks
 * - **Stable TIDs**: Index entries reference heap tuple IDs
 * - **Garbage collection**: Implements IndexGarbageCollectorInterface
 *
 * ## Concurrency
 *
 * - Thread-safe for concurrent reads using std::shared_mutex
 * - Write operations require exclusive lock
 * - Read operations acquire shared lock
 */

// GiST page flags
enum class GiSTFlags : uint16_t
{
    ROOT = 0x0001,        // Root page
    LEAF = 0x0002,        // Leaf page
    HAS_GARBAGE = 0x0004, // Page has deleted entries
    NEEDS_REPACK = 0x0008, // Page needs reorganization
    SPLIT_PENDING = 0x0010 // Split in progress
};

// GiST entry flags
enum class GiSTEntryFlags : uint16_t
{
    DELETED = 0x0001,      // Logically deleted (xmax set)
    INVALID = 0x0002,      // Invalid entry (e.g., inconsistent predicate)
    COMPRESSED = 0x0004    // Predicate is compressed
};

// GiST operator class strategy numbers
enum class GiSTStrategy : uint16_t
{
    OVERLAPS = 1,      // && (overlap)
    CONTAINS = 2,      // @> (contains)
    CONTAINED_BY = 3,  // <@ (contained by)
    LEFT_OF = 4,       // << (left of)
    RIGHT_OF = 5,      // >> (right of)
    BELOW = 6,         // <<| (below)
    ABOVE = 7,         // |>> (above)
    EQUALS = 8,        // = (equals)
    ADJACENT = 9,      // -|- (adjacent)
    DISTANCE = 15      // <-> (distance, for nearest neighbor)
};

#pragma pack(push, 1)

/**
 * GiST on-disk page structure
 */
struct SBGiSTPage
{
    // Standard page header (64 bytes)
    PageHeader gist_header;

    // Index identification (32 bytes)
    ID gist_index_uuid;      // Index UUID v7 (16 bytes)
    ID gist_table_uuid;      // Table UUID (16 bytes)

    // GiST metadata (32 bytes)
    uint16_t gist_flags;      // Page flags (see GiSTFlags)
    uint16_t gist_count;      // Number of entries on page
    uint16_t gist_free_space; // Free space in bytes
    uint16_t gist_level;      // Tree level (0 = leaf)
    uint32_t gist_opclass_id; // Operator class ID
    uint8_t gist_reserved[20]; // Reserved for alignment (pad to 32 bytes)

    // Sibling navigation (24 bytes)
    uint64_t gist_left_sibling;  // Left sibling page number
    uint64_t gist_right_sibling; // Right sibling page number
    uint64_t gist_parent_page;   // Parent page number

    // MGA compliance (24 bytes)
    uint64_t gist_xmin;            // Page creation transaction
    uint64_t gist_xmax;            // Page deletion transaction (0 if active)
    uint64_t gist_lsn;             // Last LSN that modified this page

    // Statistics (16 bytes)
    uint64_t gist_total_entries;   // Total entries in entire index
    uint64_t gist_deleted_entries; // Deleted entries (need GC compaction)

    uint8_t gist_padding[16]; // Reserved for future use (total: 208 bytes)

    // Variable-size entries follow immediately after header
    // Each entry: uint16_t entry_size, then SBGiSTEntry data
};

/**
 * GiST on-disk entry structure (variable size)
 *
 * Layout:
 * - Fixed header (40 bytes)
 * - Variable-length predicate data (pred_size bytes)
 */
struct SBGiSTEntry
{
    // Entry metadata (8 bytes)
    uint16_t entry_size;       // Total size of this entry (including header)
    uint16_t entry_flags;      // Entry flags (see GiSTEntryFlags)
    uint16_t entry_pred_size;  // Size of predicate data in bytes
    uint16_t entry_reserved;   // Reserved for alignment

    // Union for leaf vs internal node (16 bytes)
    union {
        TID entry_row_id;          // For leaf: tuple ID (16 bytes)
        uint64_t entry_child_page; // For internal: child page (8 bytes + 8 padding)
        uint8_t entry_child_data[16];
    };

    // MGA compliance (16 bytes)
    uint64_t entry_xmin; // Transaction that created this entry
    uint64_t entry_xmax; // Transaction that deleted this entry (0 if active)

    // Variable-length predicate data follows
    // uint8_t entry_predicate[entry_pred_size];
};

#pragma pack(pop)

static_assert(sizeof(SBGiSTPage) == 224, "GiST page header must be 224 bytes (80-byte PageHeader + 144 bytes)");
static_assert(sizeof(SBGiSTEntry) == 40, "GiST entry fixed header must be 40 bytes");

/**
 * In-memory GiST predicate representation
 *
 * The predicate is the value stored in the tree that describes properties
 * of its subtree. Its format depends on the operator class.
 */
struct GiSTPredicate
{
    std::vector<uint8_t> data; // Opaque predicate data
    uint32_t opclass_id;       // Which operator class owns this predicate

    GiSTPredicate() : opclass_id(0) {}
    GiSTPredicate(const std::vector<uint8_t>& d, uint32_t oc)
        : data(d), opclass_id(oc) {}
};

/**
 * GiST operator class interface
 *
 * Each operator class must implement these methods to define how GiST
 * works with a specific data type.
 */
class GiSTOperatorClass
{
public:
    virtual ~GiSTOperatorClass() = default;

    // Operator class identification
    virtual uint32_t getOpClassId() const = 0;
    virtual std::string getOpClassName() const = 0;

    /**
     * Consistent - Test if a predicate satisfies a query condition
     *
     * @param predicate The predicate from the index entry
     * @param query The query value
     * @param strategy The query strategy (e.g., OVERLAPS, CONTAINS)
     * @return true if the predicate is consistent with the query
     */
    virtual bool consistent(const GiSTPredicate& predicate,
                           const std::vector<uint8_t>& query,
                           GiSTStrategy strategy) const = 0;

    /**
     * Union - Create a predicate that covers all entries
     *
     * @param entries List of predicates to union
     * @return A new predicate that covers all input predicates
     */
    virtual GiSTPredicate unionPredicates(
        const std::vector<GiSTPredicate>& entries) const = 0;

    /**
     * Penalty - Estimate cost of inserting an entry
     *
     * @param base The existing predicate
     * @param add The predicate to add
     * @return Penalty value (lower is better, typically area/size increase)
     */
    virtual double penalty(const GiSTPredicate& base,
                          const GiSTPredicate& add) const = 0;

    /**
     * Picksplit - Divide entries when a node overflows
     *
     * @param entries All entries that need to be split
     * @param left_entries Output: entries for left node
     * @param right_entries Output: entries for right node
     */
    virtual void picksplit(const std::vector<GiSTPredicate>& entries,
                          std::vector<size_t>& left_indices,
                          std::vector<size_t>& right_indices) const = 0;

    /**
     * Same - Test if two predicates are equal
     *
     * @param a First predicate
     * @param b Second predicate
     * @return true if predicates are identical
     */
    virtual bool same(const GiSTPredicate& a,
                     const GiSTPredicate& b) const = 0;

    /**
     * Compress - Optionally compress a predicate for storage
     *
     * @param predicate The predicate to compress
     * @return Compressed predicate (or original if compression not beneficial)
     */
    virtual GiSTPredicate compress(const GiSTPredicate& predicate) const
    {
        return predicate; // Default: no compression
    }

    /**
     * Decompress - Decompress a stored predicate
     *
     * @param predicate The compressed predicate
     * @return Decompressed predicate
     */
    virtual GiSTPredicate decompress(const GiSTPredicate& predicate) const
    {
        return predicate; // Default: no decompression
    }

    /**
     * Distance - Calculate distance for nearest-neighbor queries
     *
     * @param predicate The predicate from the index
     * @param query The query point
     * @return Distance value (lower means closer)
     */
    virtual double distance(const GiSTPredicate& predicate,
                           const std::vector<uint8_t>& query) const
    {
        return 0.0; // Default: not supported
    }
};

/**
 * GiST index implementation
 */
class GiSTIndex : public IndexGCInterface
{
public:
    /**
     * Constructor - requires database and index configuration
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param table_uuid Table UUID
     * @param column_ids Columns being indexed
     * @param opclass Operator class to use
     */
    GiSTIndex(Database* db,
              const ID& index_uuid,
              const ID& table_uuid,
              const std::vector<ID>& column_ids,
              std::shared_ptr<GiSTOperatorClass> opclass);

    /**
     * Create a new GiST index
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
                        std::shared_ptr<GiSTOperatorClass> opclass,
                        GPID root_gpid,
                        ErrorContext* ctx = nullptr);
    static Status create(Database* db,
                        const ID& index_uuid,
                        const ID& table_uuid,
                        const std::vector<ID>& column_ids,
                        std::shared_ptr<GiSTOperatorClass> opclass,
                        uint16_t tablespace_id,
                        uint32_t *root_page_out,
                        ErrorContext* ctx = nullptr);
    static Status create(Database* db,
                        const ID& index_uuid,
                        const ID& table_uuid,
                        const std::vector<ID>& column_ids,
                        std::shared_ptr<GiSTOperatorClass> opclass,
                        uint32_t *root_page_out,
                        ErrorContext* ctx = nullptr);

    /**
     * Open an existing GiST index
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
    static std::unique_ptr<GiSTIndex> open(Database* db,
                                           const ID& index_uuid,
                                           const ID& table_uuid,
                                           const std::vector<ID>& column_ids,
                                           std::shared_ptr<GiSTOperatorClass> opclass,
                                           GPID root_gpid,
                                           ErrorContext* ctx = nullptr);

    /**
     * Open an existing GiST index (minimal interface for executor template)
     * Loads metadata from catalog and operator class from registry
     *
     * @param db Database instance
     * @param index_uuid Index UUID
     * @param root_gpid Root page GPID
     * @param ctx Error context
     * @return Unique pointer to opened index, or nullptr on error
     */
    static std::unique_ptr<GiSTIndex> open(Database* db,
                                           const ID& index_uuid,
                                           GPID root_gpid,
                                           ErrorContext* ctx = nullptr);

    ~GiSTIndex();

    // Disable copy/move
    GiSTIndex(const GiSTIndex&) = delete;
    GiSTIndex& operator=(const GiSTIndex&) = delete;

    /**
     * Initialize the GiST index (create root page)
     *
     * @param ctx Error context
     * @return Status code
     */
    Status initialize(ErrorContext* ctx);

    /**
     * Insert an entry into the GiST index
     *
     * @param predicate The predicate value to index
     * @param tid The tuple ID being indexed
     * @param current_xid Current transaction ID (for MGA visibility)
     * @param ctx Error context
     * @return Status code
     */
    Status insert(const GiSTPredicate& predicate,
                  const TID& tid,
                  uint64_t current_xid,
                  ErrorContext* ctx);

    /**
     * Search the GiST index
     *
     * @param query The query value
     * @param strategy The search strategy
     * @param current_xid Current transaction ID (for MGA visibility)
     * @param results Output: matching tuple IDs
     * @param ctx Error context
     * @return Status code
     */
    Status search(const std::vector<uint8_t>& query,
                  GiSTStrategy strategy,
                  uint64_t current_xid,
                  std::vector<TID>* results,
                  ErrorContext* ctx);

    /**
     * Delete an entry from the GiST index (logical deletion)
     *
     * @param predicate The predicate to delete
     * @param tid The tuple ID to delete
     * @param current_xid Current transaction ID (sets xmax)
     * @param ctx Error context
     * @return Status code
     */
    Status remove(const GiSTPredicate& predicate,
                  const TID& tid,
                  uint64_t current_xid,
                  ErrorContext* ctx);

    /**
     * Nearest neighbor search (k-NN)
     *
     * @param query The query point
     * @param k Number of nearest neighbors to return
     * @param current_xid Current transaction ID (for MGA visibility)
     * @param results Output: nearest tuple IDs sorted by distance
     * @param ctx Error context
     * @return Status code
     */
    Status nearestNeighbor(const std::vector<uint8_t>& query,
                           size_t k,
                           uint64_t current_xid,
                           std::vector<TID>& results,
                           ErrorContext* ctx);

    // IndexGCInterface implementation
    Status removeDeadEntries(const std::vector<TID>& dead_tids,
                            uint64_t* entries_removed_out = nullptr,
                            uint64_t* pages_modified_out = nullptr,
                            ErrorContext* ctx = nullptr) override;

    /**
     * Update TIDs after table migration
     *
     * Updates leaf entry TIDs based on the provided mapping (old GPID -> new GPID).
     */
    Status updateTIDsAfterMigration(const std::unordered_map<TID, TID>& tid_mapping,
                                   uint64_t* tids_updated_out = nullptr,
                                   uint64_t* pages_modified_out = nullptr,
                                   ErrorContext* ctx = nullptr);

    const char* indexTypeName() const override { return "GiST"; }

    // Additional GC methods
    uint64_t getDeadEntryCount() const;

    // Index metadata
    const ID& getIndexUUID() const { return index_uuid_; }
    const ID& getTableUUID() const { return table_uuid_; }
    uint64_t getRootPage() const { return root_page_; }
    uint16_t getHeight() const { return height_; }
    uint64_t getEntryCount() const { return entry_count_; }

private:
    // Core tree operations
    Status insertRecursive(uint64_t page_num,
                          const GiSTPredicate& predicate,
                          const TID& tid,
                          uint64_t current_xid,
                          uint64_t* new_right_page,
                          GiSTPredicate* new_right_pred,
                          ErrorContext* ctx);

    Status searchRecursive(uint64_t page_num,
                          const std::vector<uint8_t>& query,
                          GiSTStrategy strategy,
                          uint64_t current_xid,
                          std::vector<TID>& results,
                          ErrorContext* ctx);

    Status splitPage(uint64_t page_num,
                    GiSTPredicate* left_pred,
                    GiSTPredicate* right_pred,
                    uint64_t* new_right_page,
                    ErrorContext* ctx);

    Status chooseSubtree(uint64_t page_num,
                        const GiSTPredicate& predicate,
                        uint64_t* chosen_child,
                        ErrorContext* ctx);

    Status removeRecursive(uint64_t page_num,
                          const GiSTPredicate& predicate,
                          const TID& tid,
                          uint64_t current_xid,
                          ErrorContext* ctx);

    Status removeDeadEntriesRecursive(uint64_t page_num,
                                      const std::set<TID>& dead_tid_set,
                                      uint64_t oit,
                                      uint64_t* removed_count,
                                      uint64_t* pages_modified,
                                      ErrorContext* ctx);

    // Helper methods
    bool isEntryVisible(uint64_t xmin, uint64_t xmax, uint64_t current_xid) const;
    Status loadPage(uint64_t page_num, SBGiSTPage** page, ErrorContext* ctx);
    Status allocatePage(uint64_t* page_num, ErrorContext* ctx);
    GPID indexGPID(uint64_t page_num) const;
    Status pinIndexPage(uint64_t page_num, void **buffer, ErrorContext* ctx);
    Status unpinIndexPage(uint64_t page_num, bool dirty, ErrorContext* ctx);

    // Member variables
    Database* db_;
    BufferPool* buffer_pool_;
    TransactionManager* txn_manager_;

    ID index_uuid_;
    ID table_uuid_;
    std::vector<ID> column_ids_;
    std::shared_ptr<GiSTOperatorClass> opclass_;

    uint64_t root_page_;
    uint16_t tablespace_id_;
    uint16_t height_;
    uint64_t entry_count_;
    uint64_t deleted_count_;

    mutable std::shared_mutex mutex_; // Protects concurrent access
};

/**
 * GiST operator class registry
 *
 * Manages built-in and custom operator classes
 */
class GiSTOperatorClassRegistry
{
public:
    static GiSTOperatorClassRegistry& instance();

    /**
     * Register an operator class
     *
     * @param opclass Operator class to register
     */
    void registerOperatorClass(std::shared_ptr<GiSTOperatorClass> opclass);

    /**
     * Get an operator class by ID
     *
     * @param opclass_id Operator class ID
     * @return Operator class, or nullptr if not found
     */
    std::shared_ptr<GiSTOperatorClass> getOperatorClass(uint32_t opclass_id) const;

    /**
     * Get an operator class by name
     *
     * @param name Operator class name
     * @return Operator class, or nullptr if not found
     */
    std::shared_ptr<GiSTOperatorClass> getOperatorClass(const std::string& name) const;

private:
    GiSTOperatorClassRegistry() = default;

    std::map<uint32_t, std::shared_ptr<GiSTOperatorClass>> opclasses_by_id_;
    std::map<std::string, std::shared_ptr<GiSTOperatorClass>> opclasses_by_name_;
    mutable std::shared_mutex mutex_;
};

} // namespace scratchbird::core
