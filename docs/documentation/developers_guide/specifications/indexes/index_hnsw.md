# Specification: HNSW Vector Index

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage/indexes |
| **Spec Version** | 1.0.0 |
| **Status** | 🟡 Review |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Development Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/hnsw_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/hnsw_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/vector.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_hnsw_index.cpp:1`

## Synopsis

HNSW (Hierarchical Navigable Small World) provides efficient approximate nearest neighbor (ANN) search for high-dimensional vectors. It builds a multi-layer graph structure enabling O(log n) search time with 95%+ recall for semantic search, image similarity, and recommendation systems.

## Scope

### In Scope

- Multi-layer graph structure
- Node storage with neighbor connections
- Greedy search with beam expansion
- Probabilistic layer assignment
- Distance metrics (Euclidean, Cosine, Inner Product)
- MGA-compliant visibility tracking

### Out of Scope

- Exact vector search (use brute-force)
- GPU acceleration (see GPU_CAGRA)
- Product quantization (see IVF_PQ)
- Binary vectors (see VECTOR_BIN_FLAT)

## Background

HNSW builds a hierarchy of proximity graphs where:
- Layer 0 (base): Contains all vectors with dense connections
- Higher layers: Sparse graphs for coarse navigation
- Search starts at top layer, descends to refine results

Key parameters:
- **M**: Max connections per node (default: 16)
- **ef_construction**: Beam width during build (default: 200)
- **ef_search**: Beam width during search (default: 100)

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:659-660
enum class IndexType : uint8_t {
    HNSW = 2,         // Vector similarity index (renamed from VECTOR)
    VECTOR = 2,       // Alias for HNSW (backward compatibility)
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_HNSW_META` | 0x40 | Metadata page with entry point |
| `PAGE_TYPE_HNSW_LAYER` | 0x41 | Layer-specific graph pages |

### Meta Page Layout

```cpp
// Source: scratchbird/core/hnsw_index.h:195
struct SBHnswIndex {
    ID idx_uuid;                    // Index UUID v7 (16 bytes)
    ID idx_table_uuid;              // Table UUID (16 bytes)
    std::vector<ID> idx_column_uuids; // Indexed columns
    uint32_t idx_root_page;         // Root page number (4 bytes)
    uint16_t idx_tablespace_id;     // Tablespace ID (2 bytes)
    uint32_t idx_m;                 // Max connections (4 bytes)
    uint32_t idx_ef_construction;   // Build beam width (4 bytes)
    uint32_t idx_ef_search;         // Search beam width (4 bytes)
    uint32_t idx_dimensions;        // Vector dimensions (4 bytes)
    uint64_t idx_total_nodes;       // Total nodes (8 bytes)
    uint64_t idx_creation_xid;      // Creation transaction (8 bytes)
    uint8_t idx_distance_metric;    // Distance metric enum (1 byte)
    uint8_t idx_vector_type;        // Vector type enum (1 byte)
};
```

### HNSW Page Structure (208 bytes header)

```cpp
// Source: scratchbird/core/hnsw_index.h:106
#pragma pack(push, 1)
struct SBHnswPage {
    // Standard page header (80 bytes)
    PageHeader hnsw_header;

    // Index identification (32 bytes)
    ID hnsw_index_uuid;         // Index UUID v7 (16 bytes)
    ID hnsw_table_uuid;         // Table UUID (16 bytes)

    // HNSW metadata (20 bytes)
    uint16_t hnsw_flags;        // Page flags (see HnswFlags)
    uint16_t hnsw_count;        // Number of nodes on page
    uint16_t hnsw_free_space;   // Free space in bytes
    uint16_t hnsw_layer;        // Layer this page belongs to
    uint32_t hnsw_m;            // Max connections per node
    uint32_t hnsw_dimensions;   // Vector dimensions

    // Sibling navigation (16 bytes)
    uint64_t hnsw_left_sibling;     // Left sibling page
    uint64_t hnsw_right_sibling;    // Right sibling page

    // MGA compliance (24 bytes)
    uint64_t hnsw_xmin;         // Page creation transaction
    uint64_t hnsw_xmax;         // Page deletion transaction
    uint64_t hnsw_lsn;          // Last LSN

    // Statistics (16 bytes)
    uint64_t hnsw_total_nodes;      // Total nodes in index
    uint64_t hnsw_deleted_nodes;    // Deleted nodes count

    uint8_t hnsw_distance_metric;   // Distance metric
    uint8_t hnsw_padding[63];       // Reserved (64 bytes)

    // Nodes follow immediately after header
    // Variable-size SBHnswNode structures
};
#pragma pack(pop)
```

### Node Structure (Variable Size)

```cpp
// Source: scratchbird/core/hnsw_index.h:149
struct SBHnswNode {
    GPID node_gpid;                 // Heap GPID (8 bytes)
    uint16_t node_slot;             // Heap slot (2 bytes)
    uint16_t node_flags;            // Node flags (2 bytes)
    uint16_t node_layer;            // Highest layer (2 bytes)
    uint16_t node_num_neighbors;    // Number of neighbors (2 bytes)
    uint16_t node_vector_len;       // Vector data length (2 bytes)

    // MGA compliance (16 bytes)
    uint64_t node_xmin;             // Creating transaction
    uint64_t node_xmax;             // Deleting transaction

    // Variable data follows:
    // - HnswNeighbor neighbors[node_num_neighbors] (16 bytes each)
    // - uint8_t vector_data[node_vector_len]
};

// Neighbor pointer (16 bytes)
struct HnswNeighbor {
    GPID neighbor_gpid;             // Neighbor GPID (8 bytes)
    uint16_t neighbor_slot;         // Neighbor slot (2 bytes)
    uint8_t neighbor_padding[6];    // Padding (6 bytes)

    TID getTID() const { return TID(neighbor_gpid, neighbor_slot); }
};

static_assert(sizeof(HnswNeighbor) == 16, "HnswNeighbor must be 16 bytes");
```

### Node Size Calculation

```
Total node size = 32 (fixed) + (node_num_neighbors * 16) + node_vector_len

Example for 768-dim float32 vector, M=16:
- Fixed header: 32 bytes
- Neighbors: 16 * 16 = 256 bytes
- Vector: 768 * 4 = 3072 bytes
- Total: ~3.4 KB per node
```

## Algorithms

### Algorithm: Layer Selection

```
Input:  None (uses random generation)
Output: Selected layer for new node

1. Generate random value r in [0, 1)

2. layer = floor(-ln(r) * ML)
   Where ML = 1 / ln(M) = ~1.4427 for M=2

3. Return min(layer, max_layer)

Properties:
- Layer 0 has ~50% of nodes
- Each higher layer has ~1/M fewer nodes
- Expected max layer = log_M(N)
```

### Algorithm: Insert

```
Input:  vector, TID, current_xid
Output: Status

1. Select layer L for new node using layer selection algorithm

2. Find entry point:
   - Start at highest layer with existing nodes
   - Descend to layer L

3. For each layer l from current_max_layer down to 0:
   
   a. If l > L:
      - Only traverse to find entry point for next layer
      - ef = 1 (greedy descent)
   
   b. Else (l <= L):
      // Search for neighbors at this layer
      ef = (l == 0) ? ef_construction : max(ef_construction, M)
      
      candidates = beam_search(entry_point, vector, ef, l, current_xid)
      
      // Select M nearest neighbors
      neighbors = select_nearest(candidates, M, vector)
      
      // Create bidirectional links
      For each neighbor in neighbors:
         - Add link: new_node -> neighbor
         - Add link: neighbor -> new_node
      
      // Prune connections if needed
      For each neighbor in neighbors:
         - If neighbor.degree > M:
           * Prune to M nearest using heuristic

4. Create node with:
   - node_xmin = current_xid
   - node_xmax = 0
   - Store vector data
   - Store neighbor links

5. Update entry point if new node at higher layer

6. Return OK
```

### Algorithm: Beam Search

```
Input:  entry_point, query_vector, ef, layer, current_xid
Output: List of nearest neighbors

1. Initialize:
   - visited = {entry_point}
   - candidates = min-heap ordered by distance to query
   - results = max-heap of size ef

2. Push entry_point to candidates with distance(query, entry_point)

3. While candidates not empty:
   a. current = pop nearest from candidates
   
   b. If current is farther than farthest in results:
      - Break (early termination)
   
   c. For each neighbor of current at this layer:
      - If neighbor not in visited:
        * Add to visited
        * Compute distance(query, neighbor)
        * If distance < farthest in results OR results.size < ef:
          - Push to candidates
          - Push to results (evict farthest if needed)

4. Filter results by MGA visibility:
   - Remove nodes where node_xmax set and visible as deleted

5. Return results
```

### Algorithm: K-NN Search

```
Input:  query_vector, k, current_xid
Output: k nearest neighbors with distances

1. entry = find_entry_point()

2. For layer from max_layer down to 1:
   a. // Coarse search for better entry
   b. candidates = beam_search(entry, query_vector, 1, layer, current_xid)
   c. entry = nearest from candidates

3. // Final search at layer 0
   results = beam_search(entry, query_vector, ef_search, 0, current_xid)

4. Return top k from results with distances
```

### Algorithm: Remove (Logical Delete)

```
Input:  TID, current_xid
Output: Status

1. Find node by TID:
   - Scan all layers or use auxiliary index

2. If found:
   - Set node_xmax = current_xid
   - Increment hnsw_deleted_nodes

3. Note: Links are kept for snapshot isolation
   Physical removal happens during GC

4. Return OK
```

### Algorithm: GC Compaction

```
Input:  OIT, dead_tids
Output: Number of nodes removed

1. For each layer from 0 to max_layer:
   
   a. For each page in layer:
      - Pin page
      
      b. For each node on page:
         - If node_xmax != 0 AND node_xmax < OIT:
           * Remove node (compact page)
           * For each neighbor link:
             - Remove reverse link from neighbor
           * Increment removed_count
         - Else if node in dead_tids:
           * Same removal process
      
      c. Mark dirty, unpin

2. Update hnsw_total_nodes

3. If entry point removed:
   - Find new entry point (highest layer with nodes)

4. Return removed_count
```

### Algorithm: Heuristic Pruning (Select Neighbors)

```
Input:  candidates, M, query_vector
Output: Selected M neighbors

// HNSW uses a heuristic to preserve graph connectivity
// Simple version: select M nearest
// Advanced version: diversity-aware selection

1. Sort candidates by distance to query

2. selected = []

3. For each candidate in sorted order:
   a. If selected.size < M:
      - Add to selected
   b. Else:
      // Check if improves graph connectivity
      - For each s in selected:
        If distance(candidate, s) < distance(query, s):
          // Candidate is closer to s than query is
          - Replace s with candidate (improves connectivity)
          - Break

4. Return selected
```

## Distance Metrics

```cpp
// Source: scratchbird/core/vector.h
enum class DistanceMetric : uint8_t {
    EUCLIDEAN = 0,      // L2 distance: sqrt(sum((a-b)^2))
    COSINE = 1,         // 1 - dot(a,b) / (|a|*|b|)
    INNER_PRODUCT = 2,  // -dot(a,b) (for MIPS)
    MANHATTAN = 3,      // L1 distance: sum(|a-b|)
    HAMMING = 4,        // For binary vectors
    DOT = 5             // Raw dot product
};
```

**Distance Computations:**

```cpp
// Euclidean (L2)
double euclidean_distance(const float* a, const float* b, size_t dim) {
    double sum = 0;
    for (size_t i = 0; i < dim; i++) {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

// Cosine (1 - similarity)
double cosine_distance(const float* a, const float* b, size_t dim) {
    double dot = 0, norm_a = 0, norm_b = 0;
    for (size_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return 1.0 - (dot / (sqrt(norm_a) * sqrt(norm_b)));
}

// Inner Product (for Maximum Inner Product Search)
double inner_product_distance(const float* a, const float* b, size_t dim) {
    double dot = 0;
    for (size_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
    }
    return -dot;  // Negative for min-heap ordering
}
```

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Each node at layer l also exists at all lower layers | Insert check |
| I2 | Node degree <= M * 2 (bidirectional) | Link pruning |
| I3 | Entry point is node with highest layer | Meta page check |
| I4 | All links are bidirectional | Consistency check |
| I5 | Vector dimensions match index config | Insert validation |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_VECTOR_DIMENSION_MISMATCH` | Vector dims != index dims | Reject insert |
| `SB_ERR_INVALID_DISTANCE_METRIC` | Unsupported metric | Reject at creation |
| `SB_ERR_HNSW_GRAPH_DISCONNECTED` | Entry point lost | Rebuild from layer 0 |
| `SB_ERR_OUT_OF_MEMORY` | Cannot allocate page | Return error |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `hnsw.m` | 16 | Max connections per node |
| `hnsw.ef_construction` | 200 | Beam width during build |
| `hnsw.ef_search` | 100 | Beam width during search |
| `hnsw.max_layer` | 16 | Maximum layer limit |
| `hnsw.distance_metric` | EUCLIDEAN | Default distance function |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_hnsw_index.cpp` | Core HNSW operations |
| `test_hnsw_recall.cpp` | Recall rate validation |
| `test_hnsw_concurrent.cpp` | Concurrent insert/search |

## Related Specifications

- [index_ivf.md](./index_ivf.md) - IVF for large-scale ANN
- [index_vector_flat.md](./index_vector_flat.md) - Brute-force search
- [index_rhnsw_pq.md](./index_rhnsw_pq.md) - HNSW with quantization

## References

- Malkov, Y., & Yashunin, D. (2018). Efficient and robust approximate nearest neighbor search using Hierarchical Navigable Small World graphs. IEEE TPAMI.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification with source anchors |
