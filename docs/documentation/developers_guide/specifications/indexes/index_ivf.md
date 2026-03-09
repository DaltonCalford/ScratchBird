# Specification: IVF Index (Inverted File)

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

IVF (Inverted File) is a vector indexing method that partitions the vector space into Voronoi cells using k-means clustering. Query vectors are compared only to vectors in the nearest cells, enabling efficient approximate nearest neighbor search for large-scale datasets.

## Scope

### In Scope

- K-means clustering for centroids
- Voronoi cell assignment
- Inverted list structure
- Coarse quantizer
- Asymmetric distance computation
- Probe factor configuration

### Out of Scope

- Product quantization (see IVF_PQ)
- Hierarchical clustering
- Graph-based refinement (see HNSW)

## Background

IVF structure:
- **Coarse quantizer**: k centroids from k-means
- **Inverted lists**: Vectors assigned to each centroid
- **Query**: Find nprobe nearest centroids, search their lists

Advantages:
- Simple and fast indexing
- Memory efficient
- Good for uniform data distributions

Trade-offs:
- Clustering quality affects recall
- Edge cases (vectors near cell boundaries)

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:670
enum class IndexType : uint8_t {
    IVF = 12,         // IVF (Inverted File) vector index
    // ... other types
};
```

### IVF Structure

```
┌─────────────────────────────────────────┐
│ Coarse Quantizer: k Centroids           │
│ C0, C1, C2, ..., C(k-1)                 │
│ (k = nlist, typically sqrt(N))          │
└─────────────────────────────────────────┘
                    │
    ┌───────────────┼───────────────┐
    ▼               ▼               ▼
┌───────┐      ┌───────┐      ┌───────┐
│List 0 │      │List 1 │      │List k-1│
│(near  │      │(near  │      │(near   │
│ C0)   │      │ C1)   │      │C(k-1)) │
├───────┤      ├───────┤      ├───────┤
│V0     │      │V5     │      │V12    │
│V3     │      │V7     │      │V15    │
│V8     │      │V9     │      │V20    │
└───────┘      └───────┘      └───────┘

Query flow:
1. Compute distance from query to all centroids
2. Select nprobe nearest centroids
3. Search only those inverted lists
4. Return k nearest from searched lists
```

### IVF Index Layout

```cpp
// IVF index metadata
struct IVFIndex {
    uint32_t dimension;           // Vector dimension
    uint32_t nlist;               // Number of centroids (lists)
    uint32_t nprobe;              // Lists to search per query
    
    // Centroids: nlist x dimension float matrix
    std::vector<float> centroids;
    
    // Inverted lists
    struct InvertedList {
        uint32_t centroid_id;
        uint64_t vector_count;
        std::vector<uint32_t> vector_ids;  // Original vector IDs
        std::vector<float> vectors;        // Flat storage or codes
    };
    std::vector<InvertedList> lists;
};
```

### K-Means Clustering

```
Algorithm: K-Means Training

Input:  Vectors, k (nlist)
Output: k centroids

1. Initialize centroids:
   - Random selection of k vectors, OR
   - K-means++ initialization

2. Repeat until convergence:
   a. Assignment step:
      For each vector v:
        - Find nearest centroid: argmin ||v - c||
        - Assign to that centroid's list
   
   b. Update step:
      For each centroid c:
        - c = mean of all vectors assigned to c

3. Return final centroids

Convergence criteria:
- Max iterations reached (e.g., 25)
- Centroid movement < threshold
- Or cluster assignment stable
```

## Algorithms

### Algorithm: Build Index

```
Input:  Vectors, nlist, metric
Output: IVF index

1. Train coarse quantizer:
   centroids = kmeans(vectors, nlist)

2. Initialize nlist empty inverted lists

3. For each vector v with id:
   a. Compute distance to all centroids
   b. Find nearest centroid c
   c. Append (id, v) to list[c]

4. Optionally sort vectors within lists
   (for better cache locality)

5. Return index structure
```

### Algorithm: Search

```
Input:  query_vector, k, nprobe
Output: k nearest neighbors

1. Compute distances from query to all centroids

2. Select nprobe nearest centroids
   (use heap for efficiency if nprobe << nlist)

3. candidates = []

4. For each selected centroid:
   a. Load inverted list
   
   b. For each vector in list:
      - Compute exact distance(query, vector)
      - Maintain max-heap of size k for results

5. Return top k from candidates
```

### Algorithm: Add Vector

```
Input:  new_vector, vector_id
Output: Status

1. Compute distances to all centroids

2. Find nearest centroid c

3. Append (vector_id, new_vector) to list[c]

4. Return OK

Note: Over time, may need re-clustering if distribution changes
```

### Algorithm: Asymmetric Distance

```
For IVF with encoded vectors (IVF_PQ, IVF_SQ8):

Distance estimate = ||q - c||^2 + ||c - v_encoded||^2
                  ≈ ||q - c||^2 + codebook_distance

Where:
- ||q - c|| is precomputed per centroid
- ||c - v_encoded|| uses precomputed tables

This avoids decoding v during search.
```

## Complexity Analysis

| Operation | Time | Space |
|-----------|------|-------|
| Build | O(N * k * I) | O(k*d + N*d) |
| Search | O(k*d + nprobe * (N/k)*d) | O(k*d) |
| Add | O(k*d) | - |

Where:
- N = total vectors
- k = nlist (number of centroids)
- d = dimension
- I = k-means iterations
- nprobe = lists searched per query

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ivf.nlist` | sqrt(N) | Number of centroids |
| `ivf.nprobe` | 1 | Lists to search |
| `ivf.kmeans_iters` | 25 | K-means iterations |
| `ivf.kmeans_min_points` | 39 | Min vectors per centroid |

## Recall vs Speed Trade-off

| nprobe | Search Time | Recall@10 |
|--------|-------------|-----------|
| 1 | 1x | 30% |
| 4 | 4x | 65% |
| 16 | 16x | 90% |
| 64 | 64x | 98% |
| 256 | 256x | 99%+ |

## Variants

| Variant | Description |
|---------|-------------|
| IVF_FLAT | Store full vectors in lists |
| IVF_PQ | Product quantization compression |
| IVF_SQ8 | 8-bit scalar quantization |
| IVF_HNSW | Use HNSW for coarse quantizer |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_ivf_index.cpp` | Core IVF operations |
| `test_ivf_recall.cpp` | Recall vs nprobe |
| `test_ivf_kmeans.cpp` | Clustering quality |

## Related Specifications

- [index_ivf_flat.md](./index_ivf_flat.md) - Flat storage variant
- [index_ivf_pq.md](./index_ivf_pq.md) - Product quantization
- [index_hnsw.md](./index_hnsw.md) - Graph-based alternative

## References

- Jégou, H. et al. (2011). Product Quantization for Nearest Neighbor Search.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
