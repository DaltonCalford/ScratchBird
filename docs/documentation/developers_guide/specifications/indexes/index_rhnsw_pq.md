# Specification: RHNSW_PQ Index (HNSW with PQ)

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

RHNSW_PQ combines HNSW graph navigation with Product Quantization compression. Graph edges use PQ codes for compact neighbor storage, enabling large-scale approximate nearest neighbor search with reduced memory footprint.

## Scope

### In Scope

- HNSW graph structure
- PQ-compressed node vectors
- Refinement with reranking

### Out of Scope

- Raw vector storage in graph
- Exact search without reranking

## Background

HNSW + PQ:
- **Graph**: HNSW structure for navigation
- **Storage**: PQ codes for vectors
- **Search**: Navigate using approximate distances, rerank top candidates
- **Memory**: Significantly less than full HNSW

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:681
enum class IndexType : uint8_t {
    RHNSW_PQ = 0x17,          // HNSW with PQ payload variant
    // ... other types
};
```

### Structure

```cpp
struct RHNSW_PQ {
    // HNSW parameters
    uint32_t M;
    uint32_t ef_construction;
    
    // PQ parameters
    uint32_t m;                // Subspaces
    uint32_t ksub;             // 256 typically
    std::vector<float> pq_centroids;
    
    // Nodes with PQ codes
    struct Node {
        std::vector<uint8_t> pq_code;  // m bytes
        std::vector<uint32_t> neighbors;
    };
    std::vector<Node> nodes;
};
```

## Algorithms

### Search with Reranking

```
1. Navigate graph using PQ approximate distances
2. Collect top 2*k candidates
3. Rerank using exact distance computation
4. Return top k
```

## Related Specifications

- [index_hnsw.md](./index_hnsw.md) - Base HNSW
- [index_ivf_pq.md](./index_ivf_pq.md) - PQ details

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
