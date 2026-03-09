# Specification: IVF_PQ Index (Product Quantization)

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

IVF_PQ combines Inverted File Index with Product Quantization for extreme compression. Divides vectors into sub-vectors, quantizes each with a small codebook, achieving 16-32x memory reduction with maintained search accuracy.

## Scope

### In Scope

- Product quantization (PQ)
- Sub-vector splitting
- Codebook training (k-means per subspace)
- Asymmetric distance computation (ADC)
- Compressed inverted lists

### Out of Scope

- OPQ (Optimized PQ) rotation
- LSH-based quantization

## Background

Product Quantization:
- **Split**: Vector into m sub-vectors (d/m dimensions each)
- **Quantize**: Each sub-vector to 8-bit code (256 centroids)
- **Storage**: m bytes per vector (vs 4d bytes raw)
- **ADC**: Precompute distance tables for fast lookup

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:678
enum class IndexType : uint8_t {
    IVF_PQ = 0x14,            // IVF product quantization variant
    // ... other types
};
```

### PQ Structure

```cpp
struct IVFPQIndex {
    uint32_t dimension;        // d
    uint32_t m;                // Number of subspaces
    uint32_t dsub;             // Sub-vector dimension (d/m)
    uint32_t ksub;             // Centroids per subspace (typically 256)
    
    // Coarse quantizer (from IVF)
    uint32_t nlist;
    std::vector<float> coarse_centroids;
    
    // Product quantizer codebooks
    // m x ksub x dsub float matrix
    std::vector<float> pq_centroids;
    
    // Compressed vectors in inverted lists
    struct InvertedList {
        uint64_t code_count;
        // Each vector: m bytes (one code per subspace)
        std::vector<uint8_t> codes;
    };
    std::vector<InvertedList> lists;
};
```

## Algorithms

### Algorithm: PQ Training

```
Input:  Training vectors, m (subspaces), ksub
Output: PQ codebooks

1. Split vectors into m sub-vectors of dsub dimensions

2. For each subspace j = 0 to m-1:
   a. Collect all j-th sub-vectors from training set
   b. Run k-means with k = ksub centroids
   c. Store ksub centroids as codebook for subspace j

3. Return m codebooks
```

### Algorithm: Encode Vector

```
Input:  vector x, PQ codebooks
Output: m-byte code

1. Split x into m sub-vectors: x_0, x_1, ..., x_{m-1}

2. For each subspace j:
   a. Find nearest centroid: 
      code[j] = argmin_i ||x_j - c_{j,i}||

3. Return code[0..m-1]
```

### Algorithm: Asymmetric Distance Computation (ADC)

```
Input:  query q, PQ codebooks
Output: Distance lookup tables

1. Split q into m sub-vectors: q_0, ..., q_{m-1}

2. For each subspace j:
   For each centroid i in subspace j:
     table[j][i] = ||q_j - c_{j,i}||^2

3. To compute distance(query, encoded_vector):
   dist = sum(table[j][code[j]] for j in 0..m-1)
   
   No decompression needed!
```

### Algorithm: Search

```
Input:  query, k, nprobe
Output: k nearest neighbors

1. Precompute ADC tables for query

2. Find nprobe nearest coarse centroids

3. For each selected inverted list:
   For each compressed vector in list:
   a. Compute distance using ADC tables
   b. Maintain k nearest in heap

4. Return results
```

## Compression Ratios

| Dimension | PQ (m=8, ksub=256) | Raw Float | Ratio |
|-----------|-------------------|-----------|-------|
| 128 | 8 bytes | 512 bytes | 64x |
| 768 | 96 bytes | 3072 bytes | 32x |
| 1536 | 192 bytes | 6144 bytes | 32x |

## Related Specifications

- [index_ivf.md](./index_ivf.md) - Base IVF structure
- [index_ivf_sq8.md](./index_ivf_sq8.md) - Scalar quantization

## References

- Jégou, H. et al. (2011). Product Quantization for Nearest Neighbor Search.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
