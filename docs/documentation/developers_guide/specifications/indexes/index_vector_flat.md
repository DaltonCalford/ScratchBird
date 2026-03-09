# Specification: VECTOR_FLAT Index (Brute-Force Float)

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

VECTOR_FLAT stores floating-point vectors in a flat array without approximation, providing exact nearest neighbor search with 100% recall. Used for small datasets (<100K vectors) or as a baseline for approximate methods.

## Scope

### In Scope

- Flat vector storage
- Exact distance computation
- Brute-force k-NN search
- Batch query processing
- Multiple distance metrics

### Out of Scope

- Approximate search
- Compression/quantization
- Index construction (none needed)

## Background

Brute-force search:
- **Pros**: 100% recall, simple, no build time
- **Cons**: O(N*d) search time
- **Use when**: N < 100K or exact results required

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:674
enum class IndexType : uint8_t {
    VECTOR_FLAT = 0x10,       // Brute-force float vector index
    // ... other types
};
```

### Storage Layout

```cpp
struct VectorFlatIndex {
    uint32_t dimension;
    uint64_t vector_count;
    DistanceMetric metric;
    
    // Flat array: vector_count x dimension floats
    std::vector<float> vectors;
    
    // Vector IDs (optional, for mapping)
    std::vector<uint64_t> ids;
};
```

## Algorithms

### Algorithm: Search

```
Input:  query (d dimensions), k
Output: k nearest exact neighbors

1. Initialize max-heap of size k

2. For each vector v in index:
   a. Compute exact distance(query, v)
   b. If heap not full or distance < heap.max:
      - Insert (id, distance) into heap

3. Return heap contents sorted by distance
```

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| Build | O(1) | O(N*d*4 bytes) |
| Search | O(N*d) | O(1) |
| Add | O(1) | O(d*4 bytes) |

## Related Specifications

- [index_hnsw.md](./index_hnsw.md) - Approximate alternative
- [index_ivf.md](./index_ivf.md) - Faster approximate search

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
