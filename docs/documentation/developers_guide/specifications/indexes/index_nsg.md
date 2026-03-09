# Specification: NSG Index (Navigating Spreading-out Graph)

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

NSG is a proximity graph optimized for approximate nearest neighbor search. Builds a directed graph with carefully selected edges to ensure navigation efficiency, reducing the number of distance computations during search.

## Scope

### In Scope

- Proximity graph construction
- Edge selection heuristics
- Navigation-efficient routing
- Search on proximity graph

### Out of Scope

- Dynamic graph updates
- Distributed graph construction

## Background

NSG improvements over k-NN graph:
- **Degree limitation**: Control out-degree for efficiency
- **Navigation-guided**: Ensure graph connectivity
- **Edge pruning**: Remove redundant edges

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:684
enum class IndexType : uint8_t {
    NSG = 0x1A,               // NSG graph ANN index
    // ... other types
};
```

### Graph Structure

```cpp
struct NSGNode {
    uint32_t id;
    std::vector<uint32_t> neighbors;  // Outgoing edges
    std::vector<float> vector;
};

struct NSGIndex {
    uint32_t entry_point;
    uint32_t max_degree;
    std::vector<NSGNode> nodes;
};
```

## Algorithms

### Algorithm: Build NSG

```
1. Build initial k-NN graph (brute-force or NN-descent)

2. Select entry point (centrality-based)

3. For each node:
   a. Search from entry to find candidate neighbors
   b. Select diverse subset for out-edges
   c. Ensure bidirectional connectivity

4. Prune edges:
   - Remove edges that can be shortcut via other paths
```

## References

- Fu, C. et al. (2019). Fast Approximate Nearest Neighbor Search with Navigating Spreading-out Graphs.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
