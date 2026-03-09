# Specification: ART Index (Adaptive Radix Tree)

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🟡 Review |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Synopsis

ART (Adaptive Radix Tree) is a memory-optimized trie that adapts node sizes based on the number of children. Provides O(k) lookup for k-byte keys with excellent cache efficiency and prefix compression for string keys.

## Scope

### In Scope

- Adaptive node types (Node4, Node16, Node48, Node256)
- Path compression (lazy/expansion)
- Variable-length key support
- Copy-on-write for concurrency
- Memory-efficient storage

### Out of Scope

- Disk-based persistence (in-memory only)
- Range queries (use B-tree)
- Multi-version support

## Background

ART adapts node structure to fanout:
- **Node4**: 4 children (sparse)
- **Node16**: 16 children (small)
- **Node48**: 48 children (medium)
- **Node256**: 256 children (dense)

Space-efficient for:
- Sparse key spaces (e.g., 64-bit integers)
- String keys with common prefixes
- Variable-length keys

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:672
enum class IndexType : uint8_t {
    ART = 0x0E,       // Adaptive radix tree index
    // ... other types
};
```

### Node Types

```cpp
// Base node header
struct ARTNode {
    uint8_t type;        // Node type (4, 16, 48, 256, LEAF)
    uint8_t prefix_len;  // Compressed prefix length
    uint8_t prefix[8];   // First 8 bytes of prefix
};

// Node4: Up to 4 children
struct Node4 : ARTNode {
    uint8_t keys[4];           // Keys
    ARTNode* children[4];      // Child pointers
};

// Node16: 5-16 children
struct Node16 : ARTNode {
    uint8_t keys[16];          // Sorted keys
    ARTNode* children[16];     // Parallel array
};

// Node48: 17-48 children
struct Node48 : ARTNode {
    uint8_t key_index[256];    // Map byte -> child index
    ARTNode* children[48];     // Dense array
};

// Node256: 49-256 children
struct Node256 : ARTNode {
    ARTNode* children[256];    // Direct index
};

// Leaf node
struct Leaf : ARTNode {
    TID tid;
    uint8_t key[];  // Remaining key bytes
};
```

## Algorithms

### Algorithm: Lookup

```
Input:  key
Output: TID or not found

1. node = root
2. depth = 0

3. While node is not leaf:
   a. Check compressed prefix:
      If key[depth:depth+prefix_len] != node.prefix:
        Return NOT_FOUND
      depth += node.prefix_len
   
   b. Find child for key[depth]:
      - Node4/16: Binary search in keys
      - Node48: key_index[key[depth]]
      - Node256: children[key[depth]]
   
   c. If no child: Return NOT_FOUND
   d. node = child
   e. depth++

4. At leaf: Compare full key
   If match: return leaf.tid
   Else: return NOT_FOUND
```

### Algorithm: Insert

```
Input:  key, TID
Output: Status

1. Traverse to insertion point (as in Lookup)

2. If key differs at compressed prefix:
   - Split node at divergence point
   - Create new intermediate node
   - Redistribute prefix

3. If at leaf with partial match:
   - Replace leaf with internal node
   - Add old and new key as children

4. If node full:
   - Grow: Node4 -> Node16 -> Node48 -> Node256

5. Add new child pointer
```

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| Lookup | O(k) | O(1) |
| Insert | O(k) | Variable |
| Delete | O(k) | O(1) |

Where k = key length in bytes

## Related Specifications

- [index_btree.md](./index_btree.md) - Disk-based alternative
- [index_hash.md](./index_hash.md) - Hash table alternative

## References

- Leis, V. et al. (2013). The Adaptive Radix Tree.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
