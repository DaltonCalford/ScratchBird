# Specification: GiST Index (Generalized Search Tree)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🟡 Review |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/gist_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/gist_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/gist_box_ops.h:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_gist_index.cpp:1`

## Synopsis

GiST (Generalized Search Tree) is an extensible balanced tree framework for implementing various index types. Unlike fixed-structure indexes, GiST allows custom data types and query strategies through operator classes, supporting R-tree, range types, and geometric indexes.

## Scope

### In Scope

- GiST page and entry structures
- Operator class interface
- Consistent, Union, Penalty, Picksplit methods
- Search with strategy numbers
- Insert with tree descent
- Nearest-neighbor search
- MGA compliance

### Out of Scope

- Specific opclass implementations (see gist_box_ops.h)
- B-tree ordering guarantees
- Exact match optimization

## Background

GiST provides a balanced tree structure where:
- **Predicate**: A value describing a subtree property
- **Consistent**: Tests if predicate satisfies query
- **Union**: Creates predicate covering multiple entries
- **Penalty**: Estimates insertion cost
- **Picksplit**: Divides entries on overflow

Built-in operator classes:
- box_ops: 2D boxes
- circle_ops: Circles
- polygon_ops: Polygons
- range_ops: Range types
- inet_ops: Network addresses

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:663
enum class IndexType : uint8_t {
    GIST = 5,         // Generalized Search Tree
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_GIST_META` | 0x24 | Metadata page |
| `PAGE_TYPE_GIST_NODE` | 0x25 | Internal/leaf node page |

### GiST Page Structure (224 bytes header)

```cpp
// Source: scratchbird/core/gist_index.h:133
#pragma pack(push, 1)
struct SBGiSTPage {
    // Standard page header (80 bytes)
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
    uint8_t gist_reserved[20]; // Reserved padding

    // Sibling navigation (24 bytes)
    uint64_t gist_left_sibling;  // Left sibling page number
    uint64_t gist_right_sibling; // Right sibling page number
    uint64_t gist_parent_page;   // Parent page number

    // MGA compliance (24 bytes)
    uint64_t gist_xmin;            // Page creation transaction
    uint64_t gist_xmax;            // Page deletion transaction
    uint64_t gist_lsn;             // Last LSN that modified this page

    // Statistics (16 bytes)
    uint64_t gist_total_entries;   // Total entries in entire index
    uint64_t gist_deleted_entries; // Deleted entries

    uint8_t gist_padding[16]; // Reserved (total: 224 bytes)

    // Variable-size entries follow
};
#pragma pack(pop)

static_assert(sizeof(SBGiSTPage) == 224, "GiST page header must be 224 bytes");
```

### GiST Entry Structure (Variable Size)

```cpp
// Source: scratchbird/core/gist_index.h:177
#pragma pack(push, 1)
struct SBGiSTEntry {
    // Entry metadata (8 bytes)
    uint16_t entry_size;       // Total size of this entry
    uint16_t entry_flags;      // Entry flags (see GiSTEntryFlags)
    uint16_t entry_pred_size;  // Size of predicate data
    uint16_t entry_reserved;   // Reserved

    // Union for leaf vs internal node (16 bytes)
    union {
        TID entry_row_id;          // For leaf: tuple ID
        uint64_t entry_child_page; // For internal: child page
        uint8_t entry_child_data[16];
    };

    // MGA compliance (16 bytes)
    uint64_t entry_xmin; // Creating transaction
    uint64_t entry_xmax; // Deleting transaction

    // Variable-length predicate data follows
    // uint8_t entry_predicate[entry_pred_size];
};
#pragma pack(pop)

static_assert(sizeof(SBGiSTEntry) == 40, "GiST entry fixed header must be 40 bytes");
```

### GiST Operator Class Interface

```cpp
// Source: scratchbird/core/gist_index.h:227
class GiSTOperatorClass {
public:
    virtual ~GiSTOperatorClass() = default;

    // Identification
    virtual uint32_t getOpClassId() const = 0;
    virtual std::string getOpClassName() const = 0;

    /**
     * Consistent - Test if predicate satisfies query
     * 
     * @param predicate The predicate from index entry
     * @param query The query value
     * @param strategy The query strategy (OVERLAPS, CONTAINS, etc.)
     * @return true if predicate is consistent with query
     */
    virtual bool consistent(const GiSTPredicate& predicate,
                           const std::vector<uint8_t>& query,
                           GiSTStrategy strategy) const = 0;

    /**
     * Union - Create predicate covering all entries
     * 
     * @param entries List of predicates to union
     * @return New predicate covering all inputs
     */
    virtual GiSTPredicate unionPredicates(
        const std::vector<GiSTPredicate>& entries) const = 0;

    /**
     * Penalty - Estimate cost of inserting entry
     * 
     * @param base The existing predicate
     * @param add The predicate to add
     * @return Penalty value (lower is better)
     */
    virtual double penalty(const GiSTPredicate& base,
                          const GiSTPredicate& add) const = 0;

    /**
     * Picksplit - Divide entries when node overflows
     * 
     * @param entries All entries to split
     * @param left_indices Output: indices for left node
     * @param right_indices Output: indices for right node
     */
    virtual void picksplit(const std::vector<GiSTPredicate>& entries,
                          std::vector<size_t>& left_indices,
                          std::vector<size_t>& right_indices) const = 0;

    /**
     * Same - Test if two predicates are equal
     */
    virtual bool same(const GiSTPredicate& a,
                     const GiSTPredicate& b) const = 0;

    /**
     * Compress/Decompress - Optional predicate compression
     */
    virtual GiSTPredicate compress(const GiSTPredicate& p) const {
        return p;
    }
    virtual GiSTPredicate decompress(const GiSTPredicate& p) const {
        return p;
    }

    /**
     * Distance - For nearest-neighbor queries
     */
    virtual double distance(const GiSTPredicate& predicate,
                           const std::vector<uint8_t>& query) const {
        return 0.0;
    }
};
```

### Strategy Numbers

```cpp
// Source: scratchbird/core/gist_index.h:114
enum class GiSTStrategy : uint16_t {
    OVERLAPS = 1,      // && (overlap)
    CONTAINS = 2,      // @> (contains)
    CONTAINED_BY = 3,  // <@ (contained by)
    LEFT_OF = 4,       // << (left of)
    RIGHT_OF = 5,      // >> (right of)
    BELOW = 6,         // <<| (below)
    ABOVE = 7,         // |>> (above)
    EQUALS = 8,        // = (equals)
    ADJACENT = 9,      // -|- (adjacent)
    DISTANCE = 15      // <-> (distance, for k-NN)
};
```

## Algorithms

### Algorithm: Insert

```
Input:  predicate, TID, current_xid
Output: Status

1. Start at root page

2. While not at leaf:
   a. Choose subtree:
      - For each entry on page:
        penalty = opclass.penalty(entry.predicate, new_predicate)
      - Select entry with minimum penalty
   
   b. Descend to child page

3. At leaf page:
   a. If space available:
      - Create entry with entry_xmin = current_xid
      - Store predicate
      - Store TID
   
   b. Else (page full):
      - Split page using picksplit
      - Propagate split upward
      - Retry insert

4. Update parent predicates using union()
```

### Algorithm: Search

```
Input:  query, strategy, current_xid
Output: Matching TIDs

1. results = []
2. Start at root page

3. Recursive search(page):
   a. For each entry on page:
      - If opclass.consistent(entry.predicate, query, strategy):
        * If leaf: Add TID to results (if visible)
        * Else: Recursively search(child_page)

4. Filter results by MGA visibility

5. Return results
```

### Algorithm: Nearest Neighbor Search

```
Input:  query point, k, current_xid
Output: k nearest TIDs

1. Initialize priority queue (max-heap) with capacity k

2. Recursive search(page):
   a. For each entry on page:
      - distance = opclass.distance(entry.predicate, query)
      
   b. If leaf:
      - If queue not full: Add (TID, distance)
      - Else if distance < max in queue: Replace max
   
   c. Else (internal):
      - If queue not full OR distance < max in queue:
        * Recursively search(child_page)

3. Return TIDs from priority queue (sorted by distance)
```

### Algorithm: Page Split (Picksplit)

```
Input:  Full page with N entries
Output: Two pages with redistributed entries

1. Get all entry predicates: preds[0..N-1]

2. Call opclass.picksplit(preds, left_indices, right_indices)
   
   Default algorithm (for geometric types):
   a. Find two entries with maximum distance between centers
   b. Assign each remaining entry to nearer of the two
   c. Result: left_indices, right_indices

3. Create two new pages:
   - Left: entries[left_indices]
   - Right: entries[right_indices]

4. Compute union predicates:
   - left_pred = opclass.unionPredicates(left_entries)
   - right_pred = opclass.unionPredicates(right_entries)

5. Insert separator into parent (or create new root)
```

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Child predicate contained in parent | Insert check |
| I2 | All entries on leaf point to valid TIDs | GC check |
| I3 | Tree remains balanced | Split propagation |
| I4 | Consistent returns true for contained data | Opclass property |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `gist.fillfactor` | 90 | Target page fill |
| `gist.buffering` | on | Use build buffering |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_gist_index.cpp` | Core GiST operations |
| `test_gist_box_ops.cpp` | Box operator class |
| `test_gist_nn.cpp` | Nearest neighbor search |

## Related Specifications

- [index_rtree.md](./index_rtree.md) - R-tree (GiST-based)
- [index_spgist.md](./index_spgist.md) - Space-partitioned variant

## References

- Hellerstein, J. M., Naughton, J. F., & Pfeffer, A. (1995). Generalized Search Trees for Database Systems.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
