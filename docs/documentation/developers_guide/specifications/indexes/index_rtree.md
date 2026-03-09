# Specification: R-Tree Spatial Index

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/rtree.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/rtree_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/rtree.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/rtree_index.cpp:1`


## Synopsis

R-Tree (Rectangle Tree) is a balanced tree structure for spatial indexing of multi-dimensional data. It groups nearby objects using minimum bounding rectangles (MBRs), enabling efficient queries for intersection, containment, and nearest neighbors in 2D/3D space.

## Scope

### In Scope

- MBR-based node structure
- Quadratic and linear split algorithms
- Insert with reinsertion (R*-Tree variant)
- Search: intersection, containment, within distance
- Nearest neighbor queries
- GiST integration layer

### Out of Scope

- Grid-based spatial indexes
- Z-order curve indexing
- GPU-accelerated spatial queries

## Background

R-Tree organizes spatial objects by:
- **Leaf nodes**: Store object MBRs with TID references
- **Internal nodes**: Store MBRs covering child nodes
- **Properties**: All leaves at same level, M parameter (max entries)

R*-Tree optimizations:
- Forced reinsert on overflow
- Reduced coverage and overlap
- Better split heuristics

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:665
enum class IndexType : uint8_t {
    RTREE = 7,        // R-tree spatial index
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_RTREE_NODE` | 0x29 | R-tree node page |
| `PAGE_TYPE_RTREE_META` | 0x2A | Metadata page |

### MBR Structure

```cpp
// Source: scratchbird/core/rtree.h:45
struct MBR2D {
    double xmin;      // Minimum X coordinate
    double ymin;      // Minimum Y coordinate
    double xmax;      // Maximum X coordinate
    double ymax;      // Maximum Y coordinate
    
    // Helper methods
    double area() const {
        return (xmax - xmin) * (ymax - ymin);
    }
    
    double union_area(const MBR2D& other) const {
        double uxmin = std::min(xmin, other.xmin);
        double uymin = std::min(ymin, other.ymin);
        double uxmax = std::max(xmax, other.xmax);
        double uymax = std::max(ymax, other.ymax);
        return (uxmax - uxmin) * (uymax - uymin);
    }
    
    bool intersects(const MBR2D& other) const {
        return !(xmin > other.xmax || xmax < other.xmin ||
                 ymin > other.ymax || ymax < other.ymin);
    }
    
    bool contains(const MBR2D& other) const {
        return xmin <= other.xmin && xmax >= other.xmax &&
               ymin <= other.ymin && ymax >= other.ymax;
    }
    
    double margin() const {
        return 2 * ((xmax - xmin) + (ymax - ymin));
    }
};

struct MBR3D : MBR2D {
    double zmin;
    double zmax;
};
```

### R-Tree Node Entry

```cpp
// Source: scratchbird/core/rtree.h:89
struct RTreeEntry {
    MBR2D mbr;              // Bounding rectangle
    
    union {
        TID tid;            // For leaf: pointer to row
        uint32_t child_page; // For internal: child page
    };
    
    bool is_leaf;           // Entry type flag
    
    // MGA compliance
    uint64_t xmin;          // Creating transaction
    uint64_t xmax;          // Deleting transaction
};
```

### Node Structure

```cpp
// Source: scratchbird/core/rtree.h:112
struct RTreeNode {
    PageHeader header;      // Standard page header
    
    uint16_t level;         // 0 = leaf, increases upward
    uint16_t count;         // Number of entries
    uint32_t parent_page;   // Parent page (0 for root)
    
    RTreeEntry entries[];   // Variable-length entries
    // Capacity: (page_size - header) / sizeof(RTreeEntry)
};
```

### R-Tree Parameters

```cpp
// Source: scratchbird/core/rtree_index.h
struct RTreeParams {
    uint32_t max_entries;       // M (default 50)
    uint32_t min_entries;       // m = 0.4 * M (default 20)
    uint32_t reinsert_p;        // Reinsert percentage (default 30%)
    bool use_rstar;             // Use R*-Tree optimizations
    uint8_t dimensions;         // 2 or 3
};
```

## Algorithms

### Algorithm: Search (Intersection)

```
Input:  query_mbr, current_xid
Output: TIDs with intersecting MBRs

1. results = []
2. Stack = [root_page]

3. While Stack not empty:
   a. page = Stack.pop()
   
   b. For each entry in page:
      - Skip if entry.xmax visible as deleted
      
      c. If entry.mbr.intersects(query_mbr):
         - If entry.is_leaf:
           * Add entry.tid to results
         - Else:
           * Stack.push(entry.child_page)

4. Return results
```

### Algorithm: Choose Subtree (R*-Tree)

```
Input:  new_mbr, level
Output: Best leaf node for insertion

1. Set N = root node

2. While N is not at target level:
   a. If N is leaf (but we need higher level):
      - Error: tree structure corrupt
   
   b. If child pointers are leaves:
      // Choose based on overlap increase
      For each entry E in N:
        - Compute overlap increase if new_mbr added
      - Choose entry with minimum overlap increase
   
   c. Else:
      // Choose based on area increase
      For each entry E in N:
        - Compute area increase = E.mbr.union_area(new_mbr) - E.mbr.area()
      - Choose entry with minimum area increase
      - Break ties by smallest area
   
   d. N = child node of chosen entry

3. Return N
```

### Algorithm: Insert

```
Input:  new_mbr, TID, current_xid
Output: Status

1. Choose leaf L = choose_subtree(new_mbr, level=0)

2. If L has space:
   - Add entry to L
   - Update parent MBRs
   - Return OK

3. Else (overflow):
   a. If use_rstar and not reinserted yet:
      - Sort entries by distance from center
      - Remove p% farthest entries
      - Reinsert them (calling this algorithm again)
      - Then insert new entry
   
   b. Else:
      - Split node L into L and LL
      - Distribute entries using split algorithm
      - Adjust parent (propagate if needed)

4. Return OK
```

### Algorithm: Quadratic Split

```
Input:  Overfull node with M+1 entries
Output: Two nodes with redistributed entries

1. Pick seeds:
   - Find pair of entries with maximum wasted area
   - Wasted = area(union) - area(E1) - area(E2)
   - Assign E1 to group 1, E2 to group 2

2. While entries remain:
   a. If one group must receive all remaining to meet min:
      - Assign all to that group
   
   b. For next entry E:
      - Compute d1 = area increase for group 1
      - Compute d2 = area increase for group 2
      - If d1 < d2: assign to group 1
      - If d2 < d1: assign to group 2
      - If equal: assign to smaller area group

3. Create two nodes from groups
```

### Algorithm: Linear Split

```
Input:  Overfull node with M+1 entries
Output: Two nodes with redistributed entries

1. Pick seeds:
   - Find pair with largest normalized separation
   - Along each dimension:
     * Find entry with highest low side
     * Find entry with lowest high side
     * Compute separation
   - Choose dimension with max separation

2. Distribute remaining entries as in quadratic split
```

### Algorithm: Nearest Neighbor

```
Input:  query_point, k, current_xid
Output: k nearest TIDs

1. priority_queue = min-heap ordered by min_distance
2. results = max-heap with capacity k

3. priority_queue.push((root, 0))

4. While priority_queue not empty:
   a. (node, min_dist) = priority_queue.pop()
   
   b. If results full and min_dist > max in results:
      - Break (pruning)
   
   c. For each entry in node:
      - Compute min_dist from query to entry.mbr
      
      - If entry.is_leaf:
        * Compute actual distance to object
        * If results not full or distance better:
          - Add to results
      
      - Else:
        * priority_queue.push((child, min_dist))

5. Return results
```

### Algorithm: Delete

```
Input:  TID to delete, current_xid
Output: Status

1. Find leaf L containing entry with matching TID
   - Search using MBR as key

2. If found:
   - Set entry.xmax = current_xid (soft delete)

3. Note: Physical removal during GC or reinsert

4. Return OK
```

## Spatial Operators

| Operator | Meaning | Usage |
|----------|---------|-------|
| && | Overlaps | `geom1 && geom2` |
| @> | Contains | `geom1 @> geom2` |
| <@ | Contained by | `geom1 <@ geom2` |
| << | Left of | `geom1 << geom2` |
| >> | Right of | `geom1 >> geom2` |
| <-> | Distance | `geom1 <-> geom2` |

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | All leaves at same level | Insert check |
| I2 | Node entries between m and M | Split/Merge |
| I3 | Parent MBR contains all children | Update propagation |
| I4 | No overlapping siblings (approximate) | R*-Tree optimization |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `rtree.max_entries` | 50 | Maximum entries per node (M) |
| `rtree.fillfactor` | 0.4 | Minimum fill ratio (m/M) |
| `rtree.reinsert_pct` | 30 | Reinsert percentage for R* |
| `rtree.variant` | RSTAR | R-Tree variant |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_rtree_nn.cpp` | Nearest neighbor queries |
| `test_rtree_gist.cpp` | GiST integration |

## Related Specifications

- [index_gist.md](./index_gist.md) - GiST framework (R-tree uses GiST)
- [index_spgist.md](./index_spgist.md) - Space-partitioned alternative

## References

- Guttman, A. (1984). R-Trees: A Dynamic Index Structure for Spatial Searching.
- Beckmann, N. et al. (1990). The R*-Tree: An Efficient and Robust Access Method.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
