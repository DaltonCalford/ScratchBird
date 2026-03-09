# Specification: ANNOY Index

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

ANNOY (Approximate Nearest Neighbors Oh Yeah) builds a forest of random projection trees. Splits space recursively using random hyperplanes, enabling fast approximate nearest neighbor search with O(log n) tree traversal.

## Scope

### In Scope

- Random projection trees
- Forest of multiple trees
- Hyperplane splits
- Priority queue search

### Out of Scope

- Dynamic updates (read-only after build)
- Exact search guarantees

## Background

ANNOY characteristics:
- **Build**: Fast, creates multiple random trees
- **Search**: Traverse trees, combine results
- **Memory**: Moderate (stores vectors + tree structure)
- **Best for**: Static datasets, read-heavy workloads

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:683
enum class IndexType : uint8_t {
    ANNOY = 0x19,             // ANNOY random projection forest ANN
    // ... other types
};
```

### Tree Structure

```cpp
struct AnnoyNode {
    bool is_leaf;
    
    // For internal node: hyperplane split
    std::vector<float> hyperplane_vector;
    float hyperplane_offset;
    int left_child;
    int right_child;
    
    // For leaf node
    std::vector<int> items;  // Vector indices
};

struct AnnoyIndex {
    int num_trees;
    int num_neighbors;  // Max items per leaf
    std::vector<std::vector<AnnoyNode>> trees;
    std::vector<std::vector<float>> vectors;
};
```

## Algorithms

### Algorithm: Build Tree

```
Input:  Set of vectors
Output: Random projection tree

1. If |vectors| <= num_neighbors:
   - Create leaf with all vectors
   - Return

2. Select two random vectors as samples

3. Compute hyperplane:
   - Midpoint between samples
   - Normal vector = difference of samples

4. Split vectors by hyperplane:
   - side = dot(v, normal) - offset
   - left: side < 0, right: side >= 0

5. Recursively build left and right subtrees

6. Return internal node with hyperplane
```

### Algorithm: Search

```
Input:  query, k, search_k
Output: k nearest neighbors

1. Initialize priority queue with tree roots

2. While queue not empty and results < search_k:
   a. Pop node closest to query
   
   b. If leaf:
      - Compute exact distances to all items
      - Add to result heap
   
   c. If internal:
      - Compute side of query relative to hyperplane
      - Push closer child first
      - Push farther child second

3. Return top k from result heap
```

## References

- Bernhardsson, E. (2015). Annoy: Approximate Nearest Neighbors in C++/Python.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
