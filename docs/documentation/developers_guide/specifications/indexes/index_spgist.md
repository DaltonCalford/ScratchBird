# Specification: SP-GiST Index (Space-Partitioned GiST)

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/spgist_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/spgist_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/spgist_quad_ops.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/spgist_text_ops.h:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_spgist_index.cpp:1`

## Synopsis

SP-GiST (Space-Partitioned Generalized Search Tree) provides an extensible framework for implementing non-balanced disk-based data structures like quadtrees, k-d trees, and radix trees. Unlike GiST which uses balanced trees, SP-GiST supports unbalanced partitioning schemes.

## Scope

### In Scope

- Space partitioning tree structure
- Non-balanced node organization
- Leaf tuple storage
- Inner tuple with node pointers
- Choose, Picksplit, Config methods
- Built-in opclasses: quadtree, k-d tree, radix tree

### Out of Scope

- Balanced tree guarantees
- Range partitioning
- Hash-based partitioning

## Background

SP-GiST partitions space recursively:
- **Inner tuples**: Contain partition definition and child pointers
- **Leaf tuples**: Contain actual data (predicates + TIDs)
- **Nodes**: Can be stored across multiple pages
- **Leaf pages**: Store leaf tuples in lists

Common use cases:
- 2D point indexing (quadtree, k-d tree)
- Text indexing (radix/prefix tree)
- IP address indexing

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:666
enum class IndexType : uint8_t {
    SPGIST = 8,       // Space-Partitioned GiST
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_SPGIST_META` | 0x2B | Metadata page |
| `PAGE_TYPE_SPGIST_INNER` | 0x2C | Inner node page |
| `PAGE_TYPE_SPGIST_LEAF` | 0x2D | Leaf node page |
| `PAGE_TYPE_SPGIST_OVERFLOW` | 0x2E | Overflow chain page |

### SP-GiST Structure

```
                 [Root Inner Node]
                      │
        ┌─────────────┼─────────────┐
        ▼             ▼             ▼
   [NW Quadrant] [NE Quadrant] [Inner Node]
        │             │              │
        ▼             ▼         ┌────┴────┐
   [Leaf Page]   [Leaf Page]   ▼         ▼
   T1,T2,T3      T4,T5       [Leaf]   [Leaf]
```

### Meta Page Layout

```cpp
// Source: scratchbird/core/spgist_index.h
struct SPGiSTMetaPage {
    PageHeader header;
    ID index_uuid;
    ID table_uuid;
    uint32_t root_page;           // Root inner node page
    uint32_t leaf_pages_list;     // First leaf page
    uint16_t opclass_id;          // Operator class
    
    // Configuration from opclass
    uint16_t leaf_tuples_per_page;
    uint16_t nodes_per_inner_tuple;
    bool long_values_ok;
};
```

### Inner Tuple Structure

```cpp
// Source: scratchbird/core/spgist_index.h
struct SPGiSTInnerTuple {
    uint16_t tuple_size;          // Total tuple size
    uint8_t tuple_type;           // INNER_TUPLE = 1
    uint8_t node_count;           // Number of child nodes
    
    // Prefix (optional compression)
    uint16_t prefix_size;
    uint8_t prefix[prefix_size];
    
    // Node array
    SPGiSTNode nodes[node_count];
};

struct SPGiSTNode {
    uint32_t child_page;          // Child page number (0 = no child)
    uint16_t label_size;          // Size of node label
    uint8_t label[label_size];    // Label distinguishing this node
    
    // MGA
    uint64_t xmin;
    uint64_t xmax;
};
```

### Leaf Tuple Structure

```cpp
// Source: scratchbird/core/spgist_index.h
struct SPGiSTLeafTuple {
    uint16_t tuple_size;
    uint8_t tuple_type;           // LEAF_TUPLE = 2
    uint8_t flags;
    
    TID tid;                      // Heap tuple reference
    
    // Value (optional, if not in heap)
    uint16_t value_size;
    uint8_t value[value_size];
    
    // MGA
    uint64_t xmin;
    uint64_t xmax;
};
```

### Operator Class Interface

```cpp
// Source: scratchbird/core/spgist_index.h
class SPGiSTOpClass {
public:
    // Configuration
    struct Config {
        uint16_t leaf_tuples_per_page;
        uint16_t nodes_per_inner_tuple;
        bool long_values_ok;
    };
    virtual Config getConfig() const = 0;

    /**
     * Choose - Decide how to insert value into tree
     * 
     * @param inner_tuple Current inner tuple being traversed
     * @param leaf_tuple Candidate leaf tuple (for splits)
     * @param value Value to insert
     * @return Decision: which node to descend, or need to split
     */
    virtual ChooseResult choose(
        const SPGiSTInnerTuple& inner_tuple,
        const SPGiSTLeafTuple* leaf_tuple,
        const std::vector<uint8_t>& value) const = 0;

    /**
     * Picksplit - Split set of leaf tuples into partitions
     * 
     * @param leaf_tuples Tuples to partition
     * @return Partition assignments and new inner tuple prefix
     */
    virtual PickSplitResult pickSplit(
        const std::vector<SPGiSTLeafTuple>& leaf_tuples) const = 0;

    /**
     * Consistent - Check if scan should descend into node
     * 
     * @param node_label Label of node being checked
     * @param query Query value
     * @param strategy Query strategy
     * @return true if scan should explore this node
     */
    virtual bool consistent(
        const std::vector<uint8_t>& node_label,
        const std::vector<uint8_t>& query,
        uint16_t strategy) const = 0;

    /**
     * Compare - Compare two values for ordering
     * (Optional, for ordered scans)
     */
    virtual int compare(
        const std::vector<uint8_t>& a,
        const std::vector<uint8_t>& b) const = 0;
};
```

## Algorithms

### Algorithm: Insert

```
Input:  value, TID, current_xid
Output: Status

1. Start at root inner node

2. While current node is inner tuple:
   a. result = opclass.choose(current, null, value)
   
   b. If result.action == DESCEND:
      - Move to child_page indicated by result.node
   
   c. If result.action == SPLIT:
      - Call split_inner_node()
      - Retry from parent

3. At leaf page:
   a. If space available:
      - Create leaf tuple with xmin = current_xid
      - Add to page
   
   b. Else if leaf page full:
      - Split leaf page using pickSplit
      - Create new inner node
      - Redistribute tuples

4. Return OK
```

### Algorithm: Search

```
Input:  query, strategy, current_xid
Output: Matching TIDs

1. results = []
2. Stack = [root_page]

3. While Stack not empty:
   a. page_num = Stack.pop()
   b. Load page
   
   c. If inner node:
      For each node in inner tuple:
        - If opclass.consistent(node.label, query, strategy):
          * If node.child_page != 0:
            - Stack.push(node.child_page)
   
   d. If leaf page:
      For each leaf tuple:
        - Check visibility using xmin/xmax
        - If visible and opclass.consistent(tuple.value, query, strategy):
          * Add TID to results

4. Return results
```

### Algorithm: Split Inner Node

```
Input:  Overfull inner node
Output: New partition

1. Collect all leaf tuples from this subtree

2. result = opclass.pickSplit(leaf_tuples)

3. Create new inner tuple:
   - Set prefix = result.prefix
   - Create nodes for each partition

4. Redistribute leaf tuples to child pages

5. Update parent to point to new structure

6. Return OK
```

## Built-in Operator Classes

### Quadtree Ops (2D Points)

```cpp
// Source: scratchbird/core/spgist_quad_ops.h
class SPGiSTQuadtreeOpClass : public SPGiSTOpClass {
    // Partitions space into 4 quadrants:
    // 0: NW, 1: NE, 2: SW, 3: SE
    
    Config getConfig() const override {
        return {100, 4, false};  // 4 nodes (quadrants)
    }
    
    ChooseResult choose(...) const override {
        // Based on point (x,y), select quadrant
        double cx = (inner_tuple.xmin + inner_tuple.xmax) / 2;
        double cy = (inner_tuple.ymin + inner_tuple.ymax) / 2;
        
        if (value.x < cx && value.y >= cy) return DESCEND(0);  // NW
        if (value.x >= cx && value.y >= cy) return DESCEND(1); // NE
        if (value.x < cx && value.y < cy) return DESCEND(2);   // SW
        return DESCEND(3);  // SE
    }
};
```

### Text Prefix Ops (Radix Tree)

```cpp
// Source: scratchbird/core/spgist_text_ops.h
class SPGiSTTextOpClass : public SPGiSTOpClass {
    // Partitions by character prefix
    
    Config getConfig() const override {
        return {100, 256, true};  // Up to 256 nodes (byte values)
    }
    
    ChooseResult choose(...) const override {
        // Compare value with prefix, choose by next character
        size_t prefix_len = inner_tuple.prefix_size;
        if (value.size() <= prefix_len) {
            return SPLIT;  // Value is prefix of existing
        }
        uint8_t next_char = value[prefix_len];
        return DESCEND(next_char);
    }
};
```

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | All paths from root to leaf have same prefix concatenation | Insert check |
| I2 | No two leaf tuples have same value (for unique) | Constraint check |
| I3 | Inner tuple node labels are unique within tuple | Split validation |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `spgist.leaf_tuples_per_page` | 100 | Target leaf count |
| `spgist.nodes_per_inner_tuple` | (opclass) | Partition count |
| `spgist.fillfactor` | 90 | Page fill target |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_spgist_index.cpp` | Core SP-GiST operations |
| `test_spgist_quadtree.cpp` | Quadtree opclass |
| `test_spgist_text.cpp` | Text/radix opclass |

## Related Specifications

- [index_gist.md](./index_gist.md) - Balanced tree alternative
- [index_rtree.md](./index_rtree.md) - R-tree for spatial

## References

- PostgreSQL SP-GiST documentation
- Böhm, C. et al. (2001). XXL - A Library Approach to Supporting Efficient Implementations.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
