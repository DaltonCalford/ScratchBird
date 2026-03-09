# Specification: IVF_FLAT Index

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

IVF_FLAT combines Inverted File Index with flat (uncompressed) vector storage. Provides approximate nearest neighbor search with exact distance computation on candidate vectors.

## Scope

### In Scope

- K-means clustering (coarse quantizer)
- Full vector storage in inverted lists
- Asymmetric distance computation
- Configurable nprobe parameter

### Out of Scope

- Product quantization (see IVF_PQ)
- Scalar quantization (see IVF_SQ8)

## Background

IVF_FLAT:
- **Recall**: High (exact distances on candidates)
- **Memory**: Full vector storage (4*d bytes per vector)
- **Speed**: Faster than brute-force by nprobe/nlist factor
- **Trade-off**: nprobe controls speed/accuracy

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:676
enum class IndexType : uint8_t {
    IVF_FLAT = 0x12,          // IVF flat vector variant
    // ... other types
};
```

### Structure

```cpp
struct IVFFlatIndex {
    // From base IVF
    uint32_t dimension;
    uint32_t nlist;
    uint32_t nprobe;
    std::vector<float> centroids;  // nlist x dimension
    
    // Inverted lists with full vectors
    struct InvertedList {
        uint64_t vector_count;
        std::vector<float> vectors;  // Flat storage: count x dimension
        std::vector<uint64_t> ids;
    };
    std::vector<InvertedList> lists;
};
```

## Algorithms

See [index_ivf.md](./index_ivf.md) for base IVF algorithms.

### Search with Exact Distance

```
1. Find nprobe nearest centroids to query
2. For each selected list:
   - Load all vectors
   - Compute exact L2/cosine distance
   - Keep k nearest in heap
3. Return results
```

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| Search | O(nprobe * (N/nlist) * d) | O(N * d * 4 bytes) |

## Related Specifications

- [index_ivf.md](./index_ivf.md) - Base IVF structure
- [index_ivf_pq.md](./index_ivf_pq.md) - PQ compression variant

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
