# Specification: BIN_IVF_FLAT Index (Binary IVF)

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

BIN_IVF_FLAT combines IVF clustering with binary vector storage. Optimized for large-scale binary vector search using Hamming distance on packed bit representations.

## Scope

### In Scope

- Binary vector clustering
- Packed bit storage in inverted lists
- Hamming distance computation
- Fast POPCNT-based comparison

### Out of Scope

- Float vector support
- Product quantization for binary

## Background

Binary IVF:
- **Memory**: d/8 bytes per vector (vs 4d for float)
- **Distance**: POPCNT(XOR) in hardware
- **Clustering**: Hamming k-means

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:677
enum class IndexType : uint8_t {
    BIN_IVF_FLAT = 0x13,      // IVF flat binary variant
    // ... other types
};
```

### Structure

```cpp
struct BinIVFFlatIndex {
    uint32_t dimension;        // Bits
    uint32_t bytes_per_vector; // ceil(dimension/8)
    uint32_t nlist;
    
    // Binary centroids
    std::vector<uint8_t> centroids;  // nlist x bytes_per_vector
    
    // Binary inverted lists
    struct InvertedList {
        uint64_t vector_count;
        std::vector<uint8_t> vectors;  // Packed bits
    };
};
```

## Algorithms

### Hamming K-Means

```
Similar to standard k-means but:
- Distance: Hamming instead of Euclidean
- Centroid: Component-wise majority vote (mode)
```

## Related Specifications

- [index_vector_bin_flat.md](./index_vector_bin_flat.md) - Brute-force binary
- [index_ivf_flat.md](./index_ivf_flat.md) - Float IVF

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
