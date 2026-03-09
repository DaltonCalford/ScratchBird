# Specification: DiskANN Index

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

DiskANN enables billion-scale vector search on SSD by combining in-memory graph navigation with on-disk vector storage. Uses compressed in-memory graph and fetches full vectors from disk only for promising candidates.

## Scope

### In Scope

- SSD-aware graph structure
- PQ-compressed in-memory graph
- Paged vector storage
- Async prefetching
- Beam search with disk I/O

### Out of Scope

- Pure in-memory search (see HNSW)
- Network-distributed search

## Background

DiskANN design:
- **Graph**: PQ-compressed vectors in RAM
- **Vectors**: Full-precision on SSD
- **Search**: Navigate graph in RAM, verify on disk
- **Prefetching**: Overlap computation and I/O

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:685
enum class IndexType : uint8_t {
    DISKANN = 0x1B,           // DiskANN graph ANN index
    // ... other types
};
```

### Architecture

```
Memory (RAM):
┌─────────────────────────────────────────┐
│ PQ-Compressed Graph (1 byte/dim)        │
│ - Graph structure                       │
│ - Approximate distances                 │
│ - Fits billions of vectors              │
└─────────────────────────────────────────┘
                    │
                    ▼ Read full vectors
Disk (SSD):
┌─────────────────────────────────────────┐
│ Full-Precision Vectors (4 bytes/dim)    │
│ - Organized by graph locality           │
│ - Paged access pattern                  │
└─────────────────────────────────────────┘
```

### Structure

```cpp
struct DiskANNIndex {
    // In-memory PQ graph
    uint32_t pq_m;
    std::vector<float> pq_codebook;
    
    struct GraphNode {
        std::vector<uint8_t> pq_code;
        std::vector<uint32_t> neighbors;
        uint64_t vector_offset;  // Disk location
    };
    std::vector<GraphNode> graph;
    
    // Disk-backed storage
    int vector_file_fd;
    uint32_t vector_dim;
};
```

## Algorithms

### Algorithm: Search

```
Input:  query, k, beam_size
Output: k nearest neighbors

1. Precompute PQ distance table for query

2. Initialize beam with entry point

3. While beam not converged:
   a. Expand beam using PQ distances
   b. Select candidates for disk fetch
   c. Async prefetch full vectors
   d. Compute exact distances
   e. Update beam with exact distances

4. Return top k from beam
```

## References

- Subramanya, S. J. et al. (2019). DiskANN: Fast Accurate Billion-point Nearest Neighbor Search on a Single Node.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
