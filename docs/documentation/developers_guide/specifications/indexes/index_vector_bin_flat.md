# Specification: VECTOR_BIN_FLAT Index (Brute-Force Binary)

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

VECTOR_BIN_FLAT stores binary vectors (bits) for efficient Hamming distance computation. Optimized for compact representations like perceptual hashes, binary embeddings, and binarized neural codes.

## Scope

### In Scope

- Binary vector storage (packed bits)
- Hamming distance (XOR + popcount)
- Jaccard distance
- Exact search with bit operations

### Out of Scope

- Float vector support
- Approximate methods
- L2/cosine distance

## Background

Binary vectors:
- **Storage**: 1 bit per dimension (vs 32 for float)
- **Distance**: Hamming = XOR + popcount (fast via POPCNT instruction)
- **Use cases**: Image hashes, fingerprint matching, binarized embeddings

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:675
enum class IndexType : uint8_t {
    VECTOR_BIN_FLAT = 0x11,   // Brute-force binary vector index
    // ... other types
};
```

### Storage Layout

```cpp
struct VectorBinFlatIndex {
    uint32_t dimension;          // Bits per vector
    uint64_t vector_count;
    
    // Vectors packed: ceil(dimension/8) bytes per vector
    uint32_t bytes_per_vector;
    std::vector<uint8_t> vectors;
};
```

## Algorithms

### Algorithm: Hamming Distance

```cpp
// Fast Hamming distance using popcount
int hamming_distance(const uint8_t* a, const uint8_t* b, size_t bytes) {
    int dist = 0;
    for (size_t i = 0; i < bytes; i++) {
        dist += __builtin_popcount(a[i] ^ b[i]);
    }
    return dist;
}
```

## Complexity

| Operation | Time | Space |
|-----------|------|-------|
| Search | O(N * d/8) | O(N * d/8 bytes) |

## Related Specifications

- [index_vector_flat.md](./index_vector_flat.md) - Float version
- [index_bin_ivf_flat.md](./index_bin_ivf_flat.md) - Binary IVF

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
