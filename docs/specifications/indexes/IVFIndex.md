# IVF (Inverted File Index) for Vector Search Specification for ScratchBird

**Version:** 1.0
**Date:** November 20, 2025
**Status:** Implementation Ready (Requires Vector Type Support)
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Alpha - Phase 3 (Future Enhancement)
**Features:** Page-size agnostic, Approximate Nearest Neighbor (ANN) search

---

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Architecture Decision](#architecture-decision)
4. [External Dependencies](#external-dependencies)
5. [Vector Data Type](#vector-data-type)
6. [On-Disk Structures](#on-disk-structures)
7. [Page Size Considerations](#page-size-considerations)
8. [K-Means Clustering](#k-means-clustering)
9. [Product Quantization](#product-quantization)
10. [Inverted Lists](#inverted-lists)
11. [Training Phase](#training-phase)
12. [Search Algorithm](#search-algorithm)
13. [MGA Compliance](#mga-compliance)
14. [Core API](#core-api)
15. [DML Integration](#dml-integration)
16. [Query Processing](#query-processing)
17. [Query Planner Integration](#query-planner-integration)
18. [Bytecode Integration](#bytecode-integration)
19. [Implementation Steps](#implementation-steps)
20. [Testing Requirements](#testing-requirements)
21. [Performance Targets](#performance-targets)
22. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

IVF (Inverted File) indexes enable **fast approximate nearest neighbor (ANN) search** on high-dimensional vector embeddings. This allows sub-millisecond similarity search across millions or billions of vectors.

### Key Characteristics

- **Type:** Auxiliary index structure for vector similarity search
- **Granularity:** Cluster-based inverted file with Product Quantization
- **Space efficiency:** 50-200x compression vs full vectors
- **Query acceleration:** 10-100x speedup vs brute-force search
- **Best for:** ML embeddings, image search, recommendation systems
- **Page-size aware:** Adapts to database page size (8K/16K/32K/64K/128K)
- **Approximate:** Trades accuracy for speed (configurable recall)

### Classic Use Case

```sql
-- Create embeddings table
CREATE TABLE images (
    image_id BIGINT PRIMARY KEY,
    image_path VARCHAR(255),
    embedding VECTOR(128),  -- 128-dimensional embedding
    created_at TIMESTAMP
);

-- Create IVF index for ANN search
CREATE INDEX idx_embedding_ivf ON images USING ivf(embedding)
WITH (
    nlist = 4096,      -- Number of clusters
    nprobe = 20,       -- Number of clusters to search
    pq_m = 16          -- Product Quantization: 16 subvectors
);

-- Similarity search query (find 10 nearest neighbors)
SELECT image_id, image_path, embedding <-> query_vector AS distance
FROM images
ORDER BY embedding <-> '[0.1, 0.2, ..., 0.5]'::VECTOR(128)
LIMIT 10;

-- Without IVF index: Brute force O(N×d) → 1000ms for 1M vectors
-- With IVF index: O(nprobe/nlist × N × 1) → 10-50ms for 1M vectors
```

### Integration Strategy

**Design Decision:** IVF indexes are **standalone index types** optimized for vector similarity search, using Faiss library for core algorithms.

```
Table: images
├── Heap Pages (row data)
└── IVF Index
    ├── Centroids (nlist cluster centers)
    ├── PQ Codebooks (for compression)
    └── Inverted Lists
        ├── List 0: [vec_id, pq_code]...
        ├── List 1: [vec_id, pq_code]...
        └── List nlist-1: [vec_id, pq_code]...
```

---

## Prerequisites

### Critical Requirements

**IMPORTANT:** IVF indexes require foundational features that may not yet exist in ScratchBird:

1. **VECTOR Data Type**
   - Fixed-dimension vector type: `VECTOR(d)` where d = dimensions
   - Storage: `d × sizeof(float)` bytes (e.g., 512 bytes for 128-dim)
   - Operations: distance metrics (L2, cosine, inner product)

2. **Training Data Requirement**
   - IVF indexes MUST be trained before use
   - Requires 30K-256K representative vectors
   - Training happens at index creation time
   - Cannot add vectors before training

3. **External Library Integration**
   - Faiss library (C++ API) for k-means and PQ
   - Significant compilation and linking complexity

### Implementation Recommendation

**Phase 1:** Implement VECTOR data type and basic operations
**Phase 2:** Integrate Faiss library
**Phase 3:** Implement IVF index (this specification)

**Alternative:** Skip IVF index in ScratchBird Alpha, add in future release when vector workloads become common.

---

## Architecture Decision

### Faiss-Based Implementation

Following industry best practices, ScratchBird uses **Faiss (Facebook AI Similarity Search)** as the core engine for IVF indexes.

**Rationale:**
- **Production-proven:** Used by Meta, Pinterest, Spotify at billion-scale
- **Optimized algorithms:** Highly optimized k-means, PQ, and search
- **GPU support:** Optional GPU acceleration for training/search
- **Comprehensive:** Supports IVF, HNSW, PQ, OPQ, and combinations
- **Well-maintained:** Active development, excellent documentation

#### IVF Architecture

```cpp
struct IVFIndex {
    uint32_t nlist;                      // Number of clusters (e.g., 4096)
    uint32_t dimension;                  // Vector dimension
    uint32_t pq_m;                       // PQ subquantizers (e.g., 16)
    uint32_t pq_nbits;                   // Bits per code (typically 8)

    std::vector<float> centroids;        // nlist × d cluster centers
    std::vector<std::vector<float>> pq_codebooks;  // m × 256 × (d/m) codebooks
    std::vector<InvertedList> inverted_lists;      // nlist inverted lists

    bool is_trained;                     // Training status
};

struct InvertedList {
    std::vector<uint64_t> vector_ids;    // Original row IDs
    std::vector<uint8_t> pq_codes;       // PQ-compressed vectors (m bytes each)
};
```

#### Query Flow

```
Query Vector (128-dim)
    ↓
1. Coarse Quantization: Find nprobe nearest centroids
    ↓
2. Scan Inverted Lists: For each of nprobe clusters
    ↓
3. Distance Computation: Using asymmetric distance with PQ
    ↓
4. Merge Results: Collect candidates from all lists
    ↓
5. Re-rank Top K: (optional) with full vectors
    ↓
Return K nearest neighbors
```

---

## External Dependencies

### 1. Faiss Library (MIT License)

**Purpose:** Core vector search algorithms (k-means, PQ, IVF, search)

**Integration:**
- **Library:** Faiss (C++ library with Python bindings)
- **License:** MIT License (permissive, compatible with ScratchBird)
- **Repository:** https://github.com/facebookresearch/faiss
- **Size:** ~50 MB compiled library (CPU-only), ~200 MB (with GPU support)
- **Dependencies:** BLAS/LAPACK (for matrix operations)

**Installation:**
```bash
# Install Faiss (CPU-only)
git clone https://github.com/facebookresearch/faiss.git
cd faiss
cmake -B build -DFAISS_ENABLE_GPU=OFF -DFAISS_ENABLE_PYTHON=OFF .
cmake --build build
sudo cmake --install build
```

**API Usage:**
```cpp
#include <faiss/IndexIVFPQ.h>
#include <faiss/IndexFlat.h>

// Create index
faiss::IndexFlatL2 quantizer(dimension);
faiss::IndexIVFPQ index(&quantizer, dimension, nlist, pq_m, 8);

// Train
index.train(n_train, train_vectors);

// Add vectors
index.add(n, vectors);

// Search
index.nprobe = 20;
float* distances = new float[k];
faiss::idx_t* labels = new faiss::idx_t[k];
index.search(1, query_vector, k, distances, labels);
```

**Why Faiss:**
- Industry standard for vector search
- Battle-tested at billion-scale
- Highly optimized (SIMD, multithreading)
- Active development and support
- No need to reimplement complex algorithms

### 2. BLAS/LAPACK (Optional but Recommended)

**Purpose:** Fast linear algebra operations (matrix multiplications for distance computations)

**Options:**
- OpenBLAS (open source, good performance)
- Intel MKL (proprietary, best performance on Intel CPUs)
- Apple Accelerate (macOS, optimized for ARM)

**Installation:**
```bash
# Ubuntu/Debian
sudo apt-get install libopenblas-dev liblapack-dev

# macOS
# Accelerate framework is built-in
```

---

## Vector Data Type

### Type Definition

**SQL Syntax:**
```sql
CREATE TABLE embeddings (
    id BIGINT PRIMARY KEY,
    embedding VECTOR(128)  -- 128-dimensional vector
);
```

### Internal Representation

```cpp
struct VectorType {
    uint16_t dimension;          // Number of dimensions
    float* values;               // Array of float32 values

    // Total size: 2 + (dimension × 4) bytes
    // Example: VECTOR(128) = 2 + 512 = 514 bytes
};

// Storage in heap tuple
struct VectorValue {
    uint16_t dimension;
    float values[];              // Flexible array member
} __attribute__((packed));
```

### Distance Metrics

```cpp
enum class DistanceMetric {
    L2,              // Euclidean distance: sqrt(sum((a - b)^2))
    COSINE,          // Cosine similarity: 1 - (a·b)/(||a||×||b||)
    INNER_PRODUCT    // Inner product: a·b
};

// L2 (Euclidean) distance
float l2_distance(const float* a, const float* b, uint32_t d) {
    float sum = 0.0f;
    for (uint32_t i = 0; i < d; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrt(sum);
}

// Cosine distance (1 - cosine similarity)
float cosine_distance(const float* a, const float* b, uint32_t d) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < d; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    float cosine_sim = dot / (sqrt(norm_a) * sqrt(norm_b));
    return 1.0f - cosine_sim;
}
```

### SQL Operators

```sql
-- Distance operators
embedding <-> query    -- L2 distance
embedding <#> query    -- Inner product (negative for similarity)
embedding <=> query    -- Cosine distance

-- Example queries
SELECT * FROM images
WHERE embedding <-> '[0.1, 0.2, ...]'::VECTOR(128) < 0.5;  -- Distance threshold

SELECT * FROM images
ORDER BY embedding <-> query_embedding
LIMIT 10;  -- K nearest neighbors
```

---

## On-Disk Structures

### Page Layout Overview

```
IVF Index
├── Meta Page
│   └── Index metadata, configuration, training status
├── Centroids Pages
│   └── nlist × d cluster centers (full precision floats)
├── PQ Codebook Pages
│   └── m × 256 × (d/m) codebooks for Product Quantization
└── Inverted List Pages
    ├── List 0 Header + Entries
    ├── List 1 Header + Entries
    └── List nlist-1 Header + Entries
```

### 1. IVF Index Meta Page

```cpp
#pragma pack(push, 1)

struct SBIVFIndexMetaPage {
    PageHeader ivf_header;          // Standard page header (64 bytes)

    // Index metadata (64 bytes)
    uint8_t ivf_index_uuid[16];     // Index UUID (16 bytes)
    uint32_t ivf_table_id;          // Parent table ID (4 bytes)
    uint16_t ivf_column_id;         // Indexed column (2 bytes)
    uint16_t ivf_dimension;         // Vector dimension (2 bytes)
    uint32_t ivf_nlist;             // Number of clusters (4 bytes)
    uint32_t ivf_nprobe;            // Default nprobe (4 bytes)
    uint8_t ivf_pq_m;               // PQ subquantizers (1 byte)
    uint8_t ivf_pq_nbits;           // Bits per code (1 byte, typically 8)
    uint8_t ivf_metric;             // Distance metric (1 byte, 0=L2, 1=Cosine)
    uint8_t ivf_is_trained;         // Training status (1 byte, 0=not trained, 1=trained)
    uint64_t ivf_total_vectors;     // Total vectors indexed (8 bytes)
    uint64_t ivf_training_time_ms;  // Training time (8 bytes)
    uint32_t ivf_reserved1[3];      // Reserved (12 bytes)

    // Page pointers (32 bytes)
    uint32_t ivf_centroids_first_page;   // First centroids page (4 bytes)
    uint32_t ivf_centroids_num_pages;    // Centroids page count (4 bytes)
    uint32_t ivf_codebook_first_page;    // First codebook page (4 bytes)
    uint32_t ivf_codebook_num_pages;     // Codebook page count (4 bytes)
    uint32_t ivf_invlists_first_page;    // First inverted list page (4 bytes)
    uint32_t ivf_invlists_num_pages;     // Inverted list page count (4 bytes)
    uint64_t ivf_reserved2;              // Reserved (8 bytes)

    // Statistics (32 bytes)
    uint64_t ivf_total_queries;          // Total queries (8 bytes)
    uint64_t ivf_avg_query_time_us;      // Average query time (8 bytes)
    uint64_t ivf_total_list_scans;       // Total lists scanned (8 bytes)
    uint64_t ivf_reserved3;              // Reserved (8 bytes)

    // Padding to fill page
    uint8_t ivf_padding[];               // Flexible array member
} __attribute__((packed));

// Fixed header size: 64 + 64 + 32 + 32 = 192 bytes

// Distance metric types
constexpr uint8_t IVF_METRIC_L2 = 0;
constexpr uint8_t IVF_METRIC_COSINE = 1;
constexpr uint8_t IVF_METRIC_INNER_PRODUCT = 2;

#pragma pack(pop)
```

### 2. Centroids Page

Centroids are the cluster centers from k-means clustering.

```cpp
#pragma pack(push, 1)

struct SBIVFCentroidsPage {
    PageHeader cent_header;         // Standard page header (64 bytes)
    uint32_t cent_next_page;        // Next centroids page (0 if last) (4 bytes)
    uint32_t cent_start_index;      // First centroid index on this page (4 bytes)
    uint16_t cent_dimension;        // Vector dimension (2 bytes)
    uint16_t cent_num_centroids;    // Number of centroids on page (2 bytes)
    uint32_t cent_reserved;         // Reserved (4 bytes)

    // Centroids data: Each centroid is dimension × sizeof(float) bytes
    // Flexible array member - size varies by page size
    uint8_t cent_data[];            // Variable-length centroid data
} __attribute__((packed));

// Fixed header size: 64 + 4 + 4 + 2 + 2 + 4 = 80 bytes

// Calculate max centroids per page
inline uint32_t maxCentroidsPerPage(uint32_t page_size, uint16_t dimension) {
    constexpr uint32_t CENTROIDS_PAGE_HEADER_SIZE = 80;
    uint32_t available_bytes = page_size - CENTROIDS_PAGE_HEADER_SIZE;
    uint32_t bytes_per_centroid = dimension * sizeof(float);  // dimension × 4
    return available_bytes / bytes_per_centroid;
}

// Examples for VECTOR(128):
// 8KB pages: (8192 - 80) / 512 = 15 centroids per page
// 16KB pages: (16384 - 80) / 512 = 31 centroids per page
// 32KB pages: (32768 - 80) / 512 = 63 centroids per page

#pragma pack(pop)
```

### 3. PQ Codebook Page

Product Quantization codebooks for vector compression.

```cpp
#pragma pack(push, 1)

struct SBIVFCodebookPage {
    PageHeader cb_header;           // Standard page header (64 bytes)
    uint32_t cb_next_page;          // Next codebook page (0 if last) (4 bytes)
    uint8_t cb_pq_m;                // Number of subquantizers (1 byte)
    uint8_t cb_pq_nbits;            // Bits per code (1 byte, typically 8)
    uint16_t cb_dimension;          // Vector dimension (2 bytes)
    uint16_t cb_subvec_dim;         // Subvector dimension (d/m) (2 bytes)
    uint16_t cb_start_subquantizer; // First subquantizer on page (2 bytes)
    uint16_t cb_num_subquantizers;  // Subquantizers on page (2 bytes)
    uint32_t cb_reserved;           // Reserved (4 bytes)

    // Codebook data: Each subquantizer has 256 centroids (for 8-bit)
    // Each centroid is subvec_dim × sizeof(float) bytes
    // Total per subquantizer: 256 × subvec_dim × 4 bytes
    uint8_t cb_data[];              // Flexible array member
} __attribute__((packed));

// Fixed header size: 64 + 4 + 1 + 1 + 2 + 2 + 2 + 2 + 4 = 82 bytes

// Calculate max subquantizers per page
inline uint32_t maxSubquantizersPerPage(uint32_t page_size, uint16_t subvec_dim) {
    constexpr uint32_t CODEBOOK_PAGE_HEADER_SIZE = 82;
    constexpr uint32_t NUM_CENTROIDS_PER_SUBQ = 256;  // For 8-bit PQ
    uint32_t available_bytes = page_size - CODEBOOK_PAGE_HEADER_SIZE;
    uint32_t bytes_per_subq = NUM_CENTROIDS_PER_SUBQ * subvec_dim * sizeof(float);
    return available_bytes / bytes_per_subq;
}

// Example for VECTOR(128) with PQ_M=16:
// subvec_dim = 128 / 16 = 8
// bytes_per_subq = 256 × 8 × 4 = 8192 bytes
// 8KB pages: (8192 - 82) / 8192 = 0 subquantizers → Need ~1 page per subquantizer
// 16KB pages: (16384 - 82) / 8192 = 1 subquantizer per page
// 32KB pages: (32768 - 82) / 8192 = 3 subquantizers per page

#pragma pack(pop)
```

### 4. Inverted List Page

Each cluster has an inverted list of vectors assigned to it.

```cpp
#pragma pack(push, 1)

struct SBIVFInvertedListPage {
    PageHeader inv_header;          // Standard page header (64 bytes)
    uint32_t inv_next_page;         // Next list page (0 if last) (4 bytes)
    uint32_t inv_list_id;           // Cluster/list ID (4 bytes)
    uint8_t inv_pq_m;               // PQ subquantizers (1 byte)
    uint8_t inv_reserved1[3];       // Alignment (3 bytes)
    uint32_t inv_num_vectors;       // Vectors on this page (4 bytes)
    uint32_t inv_reserved2;         // Reserved (4 bytes)

    // Inverted list entries: Each entry is (vector_id + pq_code)
    // vector_id: uint64_t (8 bytes)
    // pq_code: pq_m bytes (e.g., 16 bytes for pq_m=16)
    // Total per entry: 8 + pq_m bytes
    uint8_t inv_data[];             // Flexible array member
} __attribute__((packed));

// Fixed header size: 64 + 4 + 4 + 1 + 3 + 4 + 4 = 84 bytes

// Calculate max vectors per list page
inline uint32_t maxVectorsPerListPage(uint32_t page_size, uint8_t pq_m) {
    constexpr uint32_t INV_LIST_PAGE_HEADER_SIZE = 84;
    uint32_t available_bytes = page_size - INV_LIST_PAGE_HEADER_SIZE;
    uint32_t bytes_per_vector = sizeof(uint64_t) + pq_m;  // 8 + pq_m
    return available_bytes / bytes_per_vector;
}

// Examples for PQ_M=16:
// bytes_per_vector = 8 + 16 = 24 bytes
// 8KB pages: (8192 - 84) / 24 = 337 vectors per page
// 16KB pages: (16384 - 84) / 24 = 679 vectors per page
// 32KB pages: (32768 - 84) / 24 = 1361 vectors per page

#pragma pack(pop)
```

---

## Page Size Considerations

IVF indexes in ScratchBird adapt to the database page size configured at creation time.

### Supported Page Sizes

- 8 KB (8,192 bytes)
- 16 KB (16,384 bytes)
- 32 KB (32,768 bytes)
- 64 KB (65,536 bytes)
- 128 KB (131,072 bytes)

### Centroids Per Page (for VECTOR(128))

| Page Size | Available Bytes | Centroids Per Page |
|-----------|-----------------|---------------------|
| 8 KB      | 8,112           | 15                  |
| 16 KB     | 16,304          | 31                  |
| 32 KB     | 32,688          | 63                  |
| 64 KB     | 65,456          | 127                 |
| 128 KB    | 130,992         | 255                 |

**Note:** For nlist=4096 and dimension=128, you need ~273 pages (8KB) or ~133 pages (16KB) to store all centroids.

### Inverted List Vectors Per Page (PQ_M=16)

| Page Size | Available Bytes | Vectors Per Page |
|-----------|-----------------|------------------|
| 8 KB      | 8,108           | 337              |
| 16 KB     | 16,300          | 679              |
| 32 KB     | 32,684          | 1,361            |
| 64 KB     | 65,452          | 2,727            |
| 128 KB    | 130,988         | 5,457            |

### Trade-offs by Page Size

#### Small Pages (8KB)
**Pros:**
- Lower memory footprint per page load
- Fine-grained I/O

**Cons:**
- More pages needed for large inverted lists
- Higher page overhead percentage
- More page chains for large clusters

**Best for:** Small vector collections (< 100K vectors)

#### Large Pages (64KB-128KB)
**Pros:**
- Fewer pages for large inverted lists
- Better sequential scan performance
- More vectors cached per page load
- Fewer page chains

**Cons:**
- Higher memory usage per page load
- Potential waste for small clusters

**Best for:** Large vector collections (> 1M vectors), high-dimensional vectors

### Page Size Recommendations

**For embeddings with dimension ≤ 128:**
- Use **16KB or 32KB pages** for balanced performance

**For embeddings with dimension > 512:**
- Use **32KB or 64KB pages** for better centroid storage

**For billion-scale collections:**
- Use **64KB or 128KB pages** to minimize page count

---

## K-Means Clustering

K-means partitions the vector space into `nlist` Voronoi cells (clusters).

### Algorithm Overview

```cpp
Status IVFIndex::trainKMeans(const std::vector<float*>& train_vectors,
                            uint32_t nlist,
                            uint32_t dimension,
                            std::vector<float>& centroids_out,
                            ErrorContext* ctx) {
    // Use Faiss for k-means clustering
    faiss::Clustering clustering(dimension, nlist);

    // Configure k-means parameters
    clustering.niter = 25;          // Number of iterations
    clustering.verbose = false;
    clustering.spherical = false;   // Standard k-means (not on sphere)

    // Create index for clustering
    faiss::IndexFlatL2 index(dimension);

    // Prepare training data
    size_t n_train = train_vectors.size();
    std::vector<float> train_data_flat(n_train * dimension);
    for (size_t i = 0; i < n_train; i++) {
        memcpy(&train_data_flat[i * dimension], train_vectors[i], dimension * sizeof(float));
    }

    // Run k-means
    clustering.train(n_train, train_data_flat.data(), index);

    // Extract centroids
    centroids_out.resize(nlist * dimension);
    memcpy(centroids_out.data(), clustering.centroids.data(), nlist * dimension * sizeof(float));

    return Status::OK;
}
```

### nlist Selection

The number of clusters (`nlist`) affects both accuracy and speed.

**Formula:** `nlist = 4*sqrt(N)` to `16*sqrt(N)` where N = dataset size

**Recommended values:**
- 10K-100K vectors: nlist = 1,024
- 100K-1M vectors: nlist = 4,096
- 1M-10M vectors: nlist = 16,384
- 10M-100M vectors: nlist = 65,536
- 100M-1B vectors: nlist = 262,144

**Trade-offs:**
- **Larger nlist:** Faster search (smaller lists), but requires more training data
- **Smaller nlist:** Slower search (larger lists), but easier to train

### Training Data Requirements

**Minimum:** 30K vectors
**Recommended:** 30K-256K vectors
**Rule of thumb:** 30 × nlist to 256 × nlist training vectors

**Example:**
- For nlist=4096: Need 122K to 1M training vectors
- For nlist=16384: Need 491K to 4M training vectors

**Sampling strategy:**
```cpp
std::vector<float*> sampleTrainingVectors(Table* table,
                                          uint16_t column_id,
                                          uint32_t num_samples,
                                          ErrorContext* ctx) {
    // Random sampling from table
    std::vector<float*> samples;

    // Get total row count
    uint64_t total_rows = table->getRowCount();

    if (total_rows <= num_samples) {
        // Use all rows
        // ... scan entire table ...
    } else {
        // Random sampling
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, total_rows - 1);

        // Sample num_samples rows
        for (uint32_t i = 0; i < num_samples; i++) {
            uint64_t row_idx = dis(gen);
            // ... fetch vector from row_idx ...
            samples.push_back(vector_data);
        }
    }

    return samples;
}
```

---

## Product Quantization

Product Quantization (PQ) compresses vectors for compact storage and fast distance computation.

### Algorithm Overview

**Step 1: Split** vector into `m` subvectors
```
Vector: [v0, v1, v2, ..., v127]  (128-dim)
Split into m=16 subvectors of dim=8 each:
  subvec[0] = [v0, v1, v2, v3, v4, v5, v6, v7]
  subvec[1] = [v8, v9, v10, v11, v12, v13, v14, v15]
  ...
  subvec[15] = [v120, ..., v127]
```

**Step 2: Quantize** each subvector using k-means (256 centroids for 8-bit)
```
For each subvector:
  - Run k-means with k=256
  - Each subvector → nearest centroid ID (0-255)
  - Store as 1 byte
```

**Step 3: Encode** vector as m-byte code
```
Original vector: 128 × 4 = 512 bytes
PQ code (m=16): 16 bytes
Compression: 32x
```

### Implementation

```cpp
Status IVFIndex::trainProductQuantization(const std::vector<float*>& train_vectors,
                                         uint32_t dimension,
                                         uint8_t pq_m,
                                         std::vector<std::vector<float>>& codebooks_out,
                                         ErrorContext* ctx) {
    if (dimension % pq_m != 0) {
        return Status::INVALID_ARGUMENT;  // Dimension must be divisible by m
    }

    uint32_t subvec_dim = dimension / pq_m;
    size_t n_train = train_vectors.size();

    codebooks_out.resize(pq_m);

    // Train each subquantizer
    for (uint8_t i = 0; i < pq_m; i++) {
        // Extract subvectors
        std::vector<float> subvectors(n_train * subvec_dim);
        for (size_t j = 0; j < n_train; j++) {
            memcpy(&subvectors[j * subvec_dim],
                   &train_vectors[j][i * subvec_dim],
                   subvec_dim * sizeof(float));
        }

        // Run k-means with k=256
        faiss::Clustering clustering(subvec_dim, 256);
        clustering.niter = 25;

        faiss::IndexFlatL2 index(subvec_dim);
        clustering.train(n_train, subvectors.data(), index);

        // Store codebook
        codebooks_out[i].resize(256 * subvec_dim);
        memcpy(codebooks_out[i].data(),
               clustering.centroids.data(),
               256 * subvec_dim * sizeof(float));
    }

    return Status::OK;
}
```

### Encoding Vectors

```cpp
std::vector<uint8_t> encodePQ(const float* vector,
                             uint32_t dimension,
                             uint8_t pq_m,
                             const std::vector<std::vector<float>>& codebooks) {
    std::vector<uint8_t> pq_code(pq_m);
    uint32_t subvec_dim = dimension / pq_m;

    for (uint8_t i = 0; i < pq_m; i++) {
        // Extract subvector
        const float* subvec = &vector[i * subvec_dim];

        // Find nearest centroid in codebook[i]
        float min_dist = std::numeric_limits<float>::max();
        uint8_t best_code = 0;

        for (uint16_t j = 0; j < 256; j++) {
            const float* centroid = &codebooks[i][j * subvec_dim];

            // Compute L2 distance
            float dist = 0.0f;
            for (uint32_t k = 0; k < subvec_dim; k++) {
                float diff = subvec[k] - centroid[k];
                dist += diff * diff;
            }

            if (dist < min_dist) {
                min_dist = dist;
                best_code = j;
            }
        }

        pq_code[i] = best_code;
    }

    return pq_code;
}
```

### Asymmetric Distance Computation

Fast distance computation between query vector and PQ-encoded vectors:

```cpp
// Pre-compute distance table (once per query)
std::vector<std::vector<float>> computeDistanceTable(
    const float* query,
    uint32_t dimension,
    uint8_t pq_m,
    const std::vector<std::vector<float>>& codebooks) {

    uint32_t subvec_dim = dimension / pq_m;
    std::vector<std::vector<float>> distance_table(pq_m, std::vector<float>(256));

    for (uint8_t i = 0; i < pq_m; i++) {
        const float* query_subvec = &query[i * subvec_dim];

        for (uint16_t j = 0; j < 256; j++) {
            const float* centroid = &codebooks[i][j * subvec_dim];

            // Compute L2 distance
            float dist = 0.0f;
            for (uint32_t k = 0; k < subvec_dim; k++) {
                float diff = query_subvec[k] - centroid[k];
                dist += diff * diff;
            }

            distance_table[i][j] = dist;
        }
    }

    return distance_table;
}

// Compute distance to PQ-encoded vector (very fast)
float computePQDistance(const uint8_t* pq_code,
                       uint8_t pq_m,
                       const std::vector<std::vector<float>>& distance_table) {
    float total_dist = 0.0f;
    for (uint8_t i = 0; i < pq_m; i++) {
        total_dist += distance_table[i][pq_code[i]];
    }
    return sqrt(total_dist);
}
```

---

## Inverted Lists

### Structure

Each of the `nlist` clusters has an inverted list containing all vectors assigned to that cluster.

```cpp
struct InvertedList {
    uint32_t list_id;                    // Cluster ID
    uint64_t num_vectors;                // Total vectors in list
    std::vector<uint64_t> vector_ids;    // Original row IDs
    std::vector<uint8_t> pq_codes;       // PQ codes (num_vectors × pq_m bytes)
    uint32_t first_page;                 // First page for this list
    uint32_t num_pages;                  // Pages used by this list
};
```

### Insertion into Inverted Lists

```cpp
Status IVFIndex::assignVectorToList(uint64_t vector_id,
                                   const float* vector,
                                   uint32_t dimension,
                                   ErrorContext* ctx) {
    // 1. Find nearest centroid (coarse quantization)
    uint32_t nearest_list = findNearestCentroid(vector, dimension);

    // 2. Encode vector with Product Quantization
    std::vector<uint8_t> pq_code = encodePQ(vector, dimension, pq_m_, codebooks_);

    // 3. Add to inverted list
    InvertedList& list = inverted_lists_[nearest_list];
    list.vector_ids.push_back(vector_id);
    list.pq_codes.insert(list.pq_codes.end(), pq_code.begin(), pq_code.end());
    list.num_vectors++;

    // 4. Mark list as dirty for optional write-after log (WAL)
    markListDirty(nearest_list);

    return Status::OK;
}

uint32_t IVFIndex::findNearestCentroid(const float* vector, uint32_t dimension) {
    float min_dist = std::numeric_limits<float>::max();
    uint32_t best_list = 0;

    for (uint32_t i = 0; i < nlist_; i++) {
        const float* centroid = &centroids_[i * dimension];
        float dist = computeL2Distance(vector, centroid, dimension);

        if (dist < min_dist) {
            min_dist = dist;
            best_list = i;
        }
    }

    return best_list;
}
```

### List Distribution

In real-world datasets, inverted lists have **unbalanced sizes**:

- **Popular clusters:** May contain 10x average vectors
- **Sparse clusters:** May contain very few vectors
- **Zipf distribution:** Common in natural data

**Example for 1M vectors with nlist=4096:**
- Average: 244 vectors per list
- Typical range: 50-1000 vectors per list
- Some lists may have 5K+ vectors (hot spots)

**Page allocation strategy:**
```cpp
// Allocate pages dynamically as lists grow
uint32_t calculatePagesNeeded(uint64_t num_vectors, uint8_t pq_m, uint32_t page_size) {
    uint32_t vectors_per_page = maxVectorsPerListPage(page_size, pq_m);
    return (num_vectors + vectors_per_page - 1) / vectors_per_page;
}
```

---

## Training Phase

### Training Workflow

IVF indexes **must be trained** before they can accept vectors.

```cpp
Status IVFIndex::train(Table* table, uint16_t column_id, ErrorContext* ctx) {
    // 1. Validate training data availability
    uint64_t total_rows = table->getRowCount();
    if (total_rows < MIN_TRAINING_VECTORS) {
        return Status::ERROR("Need at least 30K vectors for training");
    }

    // 2. Sample training vectors
    uint32_t n_train = std::min(total_rows, (uint64_t)RECOMMENDED_TRAINING_VECTORS);
    std::vector<float*> train_vectors = sampleTrainingVectors(table, column_id, n_train, ctx);

    auto start_time = std::chrono::steady_clock::now();

    // 3. Train k-means clustering (centroids)
    Status status = trainKMeans(train_vectors, nlist_, dimension_, centroids_, ctx);
    if (!status.ok()) {
        return status;
    }

    // 4. Train Product Quantization (codebooks)
    status = trainProductQuantization(train_vectors, dimension_, pq_m_, codebooks_, ctx);
    if (!status.ok()) {
        return status;
    }

    auto end_time = std::chrono::steady_clock::now();
    training_time_ms_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();

    // 5. Mark as trained
    is_trained_ = true;

    // 6. Persist training data to disk
    status = persistTrainingData(ctx);
    if (!status.ok()) {
        return status;
    }

    // 7. Free training vectors
    for (auto vec : train_vectors) {
        delete[] vec;
    }

    return Status::OK;
}
```

### Training Time Estimates

**K-means clustering:**
- Formula: O(n_train × nlist × dimension × num_iterations)
- Example: 100K vectors, nlist=4096, dim=128, 25 iterations
  - ~100,000 × 4096 × 128 × 25 = 13B operations
  - Time: 30-60 seconds (CPU), 2-5 seconds (GPU)

**Product Quantization:**
- Formula: O(pq_m × n_train × 256 × subvec_dim × num_iterations)
- Example: 100K vectors, pq_m=16, dim=128, 25 iterations
  - ~16 × 100,000 × 256 × 8 × 25 = 8.2B operations
  - Time: 20-40 seconds (CPU), 1-3 seconds (GPU)

**Total training time:**
- CPU: 50-100 seconds for 100K training vectors
- GPU: 3-8 seconds with CUDA

### Persisting Training Data

```cpp
Status IVFIndex::persistTrainingData(ErrorContext* ctx) {
    // 1. Write meta page
    SBIVFIndexMetaPage meta_page;
    meta_page.ivf_is_trained = 1;
    meta_page.ivf_nlist = nlist_;
    meta_page.ivf_dimension = dimension_;
    meta_page.ivf_pq_m = pq_m_;
    meta_page.ivf_training_time_ms = training_time_ms_;
    writeMetaPage(&meta_page);

    // 2. Write centroids pages
    persistCentroids(centroids_, nlist_, dimension_, ctx);

    // 3. Write codebook pages
    persistCodebooks(codebooks_, pq_m_, dimension_, ctx);

    return Status::OK;
}
```

---

## Search Algorithm

### Overview

IVF search is **approximate nearest neighbor (ANN)** search with configurable accuracy/speed trade-off.

### Algorithm

```cpp
std::vector<SearchResult> IVFIndex::search(const float* query_vector,
                                          uint32_t k,
                                          uint32_t nprobe,
                                          ErrorContext* ctx) {
    if (!is_trained_) {
        return {};  // Index not trained
    }

    // Step 1: Coarse quantization - find nprobe nearest centroids
    std::vector<uint32_t> probe_lists = findNearestCentroids(query_vector, nprobe);

    // Step 2: Compute distance table for asymmetric PQ distance
    auto distance_table = computeDistanceTable(query_vector, dimension_, pq_m_, codebooks_);

    // Step 3: Scan inverted lists and collect candidates
    std::vector<SearchResult> candidates;
    for (uint32_t list_id : probe_lists) {
        scanInvertedList(list_id, distance_table, candidates, ctx);
    }

    // Step 4: Sort candidates by distance
    std::sort(candidates.begin(), candidates.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.distance < b.distance;
              });

    // Step 5: Return top k
    if (candidates.size() > k) {
        candidates.resize(k);
    }

    return candidates;
}
```

### Finding Nearest Centroids

```cpp
std::vector<uint32_t> IVFIndex::findNearestCentroids(const float* query, uint32_t nprobe) {
    // Compute distances to all centroids
    std::vector<std::pair<float, uint32_t>> centroid_distances;
    centroid_distances.reserve(nlist_);

    for (uint32_t i = 0; i < nlist_; i++) {
        const float* centroid = &centroids_[i * dimension_];
        float dist = computeL2Distance(query, centroid, dimension_);
        centroid_distances.emplace_back(dist, i);
    }

    // Partial sort to get nprobe nearest
    std::partial_sort(centroid_distances.begin(),
                     centroid_distances.begin() + nprobe,
                     centroid_distances.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    // Extract list IDs
    std::vector<uint32_t> probe_lists;
    probe_lists.reserve(nprobe);
    for (uint32_t i = 0; i < nprobe; i++) {
        probe_lists.push_back(centroid_distances[i].second);
    }

    return probe_lists;
}
```

### Scanning Inverted Lists

```cpp
void IVFIndex::scanInvertedList(uint32_t list_id,
                               const std::vector<std::vector<float>>& distance_table,
                               std::vector<SearchResult>& candidates,
                               ErrorContext* ctx) {
    InvertedList& list = inverted_lists_[list_id];

    // Iterate through all vectors in the list
    for (uint64_t i = 0; i < list.num_vectors; i++) {
        uint64_t vector_id = list.vector_ids[i];
        const uint8_t* pq_code = &list.pq_codes[i * pq_m_];

        // Compute approximate distance using PQ
        float dist = computePQDistance(pq_code, pq_m_, distance_table);

        candidates.push_back({vector_id, dist});
    }
}
```

### nprobe Parameter Trade-off

The `nprobe` parameter controls the **accuracy vs. speed** trade-off:

| nprobe | Lists Scanned | Vectors Scanned (1M, nlist=4096) | Recall | Query Time |
|--------|---------------|-----------------------------------|--------|------------|
| 1      | 1             | ~244                              | 40-60% | 0.5 ms     |
| 10     | 10            | ~2,440                            | 80-90% | 3 ms       |
| 20     | 20            | ~4,880                            | 90-95% | 5 ms       |
| 50     | 50            | ~12,200                           | 95-98% | 10 ms      |
| 100    | 100           | ~24,400                           | 98-99% | 18 ms      |
| 4096   | All           | 1,000,000                         | 100%   | 50-100 ms  |

**Recommendations:**
- **High accuracy:** nprobe = 50-100 (95-99% recall)
- **Balanced:** nprobe = 20 (90-95% recall)
- **Fast:** nprobe = 5-10 (80-90% recall)

---

## MGA Compliance

### Transaction Visibility for Vector Indexes

IVF indexes must respect **Firebird MGA (Multi-Generation Architecture)** for transaction isolation.

### Strategy

**Approach:** Store transaction metadata alongside vector entries in inverted lists.

```cpp
struct InvertedListEntry {
    uint64_t vector_id;          // Row ID (8 bytes)
    uint8_t pq_code[PQ_M];       // PQ-encoded vector (pq_m bytes)
    uint32_t insert_txn;         // Transaction that inserted this entry (4 bytes)
    uint32_t delete_txn;         // Transaction that deleted (0 if active) (4 bytes)
} __attribute__((packed));

// Total size: 8 + pq_m + 4 + 4 = 16 + pq_m bytes
// Example for pq_m=16: 32 bytes per entry
```

### Visibility Check

```cpp
bool IVFIndex::isVectorVisible(const InvertedListEntry& entry,
                              uint32_t query_txn,
                              TransactionManager* txn_mgr) {
    // Check if vector was inserted before our transaction
    if (!txn_mgr->isVisible(entry.insert_txn, query_txn)) {
        return false;  // Inserted by a future or uncommitted transaction
    }

    // Check if vector was deleted
    if (entry.delete_txn != 0) {
        if (txn_mgr->isVisible(entry.delete_txn, query_txn)) {
            return false;  // Deleted by a committed transaction
        }
    }

    return true;  // Visible to our transaction
}
```

### Search with MGA

```cpp
std::vector<SearchResult> IVFIndex::searchWithMGA(const float* query_vector,
                                                  uint32_t k,
                                                  uint32_t nprobe,
                                                  uint32_t query_txn,
                                                  TransactionManager* txn_mgr,
                                                  ErrorContext* ctx) {
    // ... same coarse quantization and distance table setup ...

    // Scan lists with visibility checks
    std::vector<SearchResult> candidates;
    for (uint32_t list_id : probe_lists) {
        InvertedList& list = inverted_lists_[list_id];

        for (uint64_t i = 0; i < list.num_vectors; i++) {
            const InvertedListEntry& entry = list.entries[i];

            // MGA visibility check
            if (!isVectorVisible(entry, query_txn, txn_mgr)) {
                continue;  // Skip invisible vectors
            }

            // Compute distance
            float dist = computePQDistance(entry.pq_code, pq_m_, distance_table);
            candidates.push_back({entry.vector_id, dist});
        }
    }

    // Sort and return top k
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.distance < b.distance; });

    if (candidates.size() > k) {
        candidates.resize(k);
    }

    return candidates;
}
```

### Sweep/GC and Cleanup

```cpp
Status IVFIndex::vacuum(TransactionManager* txn_mgr, ErrorContext* ctx) {
    uint32_t oldest_active_txn = txn_mgr->getOldestActiveTransaction();

    for (auto& list : inverted_lists_) {
        std::vector<InvertedListEntry> visible_entries;

        for (const auto& entry : list.entries) {
            // Keep if delete_txn is 0 (not deleted)
            if (entry.delete_txn == 0) {
                visible_entries.push_back(entry);
                continue;
            }

            // Keep if delete_txn is newer than oldest active transaction
            if (entry.delete_txn >= oldest_active_txn) {
                visible_entries.push_back(entry);
            }
            // Otherwise, entry can be purged
        }

        // Replace list entries with cleaned version
        list.entries = std::move(visible_entries);
        list.num_vectors = list.entries.size();
    }

    return Status::OK;
}
```

### Page Size Impact with MGA

With MGA metadata, entry size increases:
- **Without MGA:** 8 + pq_m bytes
- **With MGA:** 16 + pq_m bytes

**Example for pq_m=16:**
- Without MGA: 24 bytes → 337 entries per 8KB page
- With MGA: 32 bytes → 253 entries per 8KB page
- Overhead: ~25% reduction in entries per page

---

## Tablespace + TID/GPID Requirements

- **Row identity:** Store row references as `TID` (full `GPID + slot`) rather than legacy 32-bit page IDs.
- **Tablespace routing:** IVF metadata pages and inverted lists must allocate/pin via `root_gpid` and `tablespace_id`.
- **Heap fetch:** Result resolution must pin heap pages with `pinPageGlobal` using the `GPID` embedded in `TID`.
- **Migration:** `updateTIDsAfterMigration` must rewrite row references across all inverted lists (including PQ code paths).

---

## Core API

### IVFIndex Class

```cpp
class IVFIndex {
public:
    // Constructor
    IVFIndex(uint32_t nlist,
            uint32_t dimension,
            uint8_t pq_m,
            DistanceMetric metric,
            uint32_t page_size);

    // Training
    Status train(Table* table, uint16_t column_id, ErrorContext* ctx);
    bool isTrained() const { return is_trained_; }

    // Index operations
    Status insert(uint64_t vector_id, const float* vector, uint32_t txn_id, ErrorContext* ctx);
    Status remove(uint64_t vector_id, uint32_t txn_id, ErrorContext* ctx);
    Status update(uint64_t vector_id, const float* old_vector, const float* new_vector,
                 uint32_t txn_id, ErrorContext* ctx);

    // Search
    std::vector<SearchResult> search(const float* query_vector,
                                    uint32_t k,
                                    uint32_t nprobe,
                                    uint32_t query_txn,
                                    TransactionManager* txn_mgr,
                                    ErrorContext* ctx);

    // Maintenance
    Status vacuum(TransactionManager* txn_mgr, ErrorContext* ctx);
    Status rebalance(ErrorContext* ctx);  // Optional: redistribute vectors if lists unbalanced

    // Statistics
    IndexStatistics getStatistics() const;

    // Persistence
    Status persist(ErrorContext* ctx);
    Status load(uint32_t meta_page_id, ErrorContext* ctx);

private:
    // Index metadata
    uint32_t nlist_;                     // Number of clusters
    uint32_t dimension_;                 // Vector dimension
    uint8_t pq_m_;                       // PQ subquantizers
    uint8_t pq_nbits_;                   // Bits per code (typically 8)
    DistanceMetric metric_;              // Distance metric
    uint32_t page_size_;                 // Database page size
    bool is_trained_;                    // Training status
    uint64_t training_time_ms_;          // Training time

    // Index data
    std::vector<float> centroids_;       // nlist × dimension cluster centers
    std::vector<std::vector<float>> codebooks_;  // pq_m × 256 × (dimension/pq_m)
    std::vector<InvertedList> inverted_lists_;   // nlist inverted lists

    // Faiss integration
    faiss::IndexIVFPQ* faiss_index_;     // Underlying Faiss index

    // Helper methods
    uint32_t findNearestCentroid(const float* vector, uint32_t dimension);
    std::vector<uint32_t> findNearestCentroids(const float* query, uint32_t nprobe);
    std::vector<uint8_t> encodePQ(const float* vector);
    Status trainKMeans(const std::vector<float*>& train_vectors,
                      std::vector<float>& centroids_out,
                      ErrorContext* ctx);
    Status trainProductQuantization(const std::vector<float*>& train_vectors,
                                   std::vector<std::vector<float>>& codebooks_out,
                                   ErrorContext* ctx);
};

struct SearchResult {
    uint64_t vector_id;      // Row ID
    float distance;          // Distance to query
};

struct IndexStatistics {
    uint64_t total_vectors;
    uint32_t num_lists;
    uint64_t total_pages;
    float avg_list_size;
    float max_list_size;
    uint64_t total_queries;
    uint64_t avg_query_time_us;
};
```

---

## DML Integration

### INSERT

```cpp
Status Table::insertVector(uint64_t row_id, const VectorValue* vector, uint32_t txn_id) {
    // 1. Insert into heap
    Status status = heap_->insert(row_id, vector, txn_id);
    if (!status.ok()) return status;

    // 2. Update IVF index
    if (ivf_index_ && ivf_index_->isTrained()) {
        status = ivf_index_->insert(row_id, vector->values, txn_id, &error_ctx);
        if (!status.ok()) {
            // Rollback heap insert
            heap_->remove(row_id, txn_id);
            return status;
        }
    }

    return Status::OK;
}
```

### DELETE

```cpp
Status Table::deleteVector(uint64_t row_id, uint32_t txn_id) {
    // 1. Mark deleted in heap
    Status status = heap_->markDeleted(row_id, txn_id);
    if (!status.ok()) return status;

    // 2. Mark deleted in IVF index
    if (ivf_index_) {
        status = ivf_index_->remove(row_id, txn_id, &error_ctx);
        if (!status.ok()) {
            // Continue - index can be rebuilt if needed
            logWarning("IVF index update failed for DELETE");
        }
    }

    return Status::OK;
}
```

### UPDATE

```cpp
Status Table::updateVector(uint64_t row_id,
                          const VectorValue* old_vector,
                          const VectorValue* new_vector,
                          uint32_t txn_id) {
    // For IVF: UPDATE = DELETE + INSERT
    // Reason: Vector may move to different cluster

    // 1. Update heap
    Status status = heap_->update(row_id, new_vector, txn_id);
    if (!status.ok()) return status;

    // 2. Update IVF index
    if (ivf_index_ && ivf_index_->isTrained()) {
        status = ivf_index_->update(row_id, old_vector->values, new_vector->values,
                                   txn_id, &error_ctx);
        if (!status.ok()) {
            // Rollback heap update
            heap_->update(row_id, old_vector, txn_id);
            return status;
        }
    }

    return Status::OK;
}
```

### Bulk Loading

For initial index creation on existing table:

```cpp
Status IVFIndex::bulkLoad(Table* table, uint16_t column_id, ErrorContext* ctx) {
    // 1. Train index first
    Status status = train(table, column_id, ctx);
    if (!status.ok()) return status;

    // 2. Scan entire table and insert vectors
    auto scanner = table->createScanner();
    uint64_t vectors_inserted = 0;

    while (scanner->hasNext()) {
        auto row = scanner->next();
        const VectorValue* vector = row->getVector(column_id);

        status = insert(row->id, vector->values, BULK_LOAD_TXN, ctx);
        if (!status.ok()) {
            return status;
        }

        vectors_inserted++;

        if (vectors_inserted % 10000 == 0) {
            // Periodic progress reporting
            logInfo("IVF index: inserted %lu vectors", vectors_inserted);
        }
    }

    return Status::OK;
}
```

---

## Query Processing

### SQL Query

```sql
-- Vector similarity search
SELECT product_id, product_name, embedding <-> query_embedding AS distance
FROM products
WHERE category_id = 5
ORDER BY embedding <-> '[0.1, 0.2, ..., 0.9]'::VECTOR(128)
LIMIT 10;
```

### Query Execution Plan

```
TopK (k=10)
  ↓
Sort by distance
  ↓
IVFIndexScan (nprobe=20)
  ↓
Filter (category_id = 5)
  ↓
Heap Fetch (get full rows)
```

### QueryPlanNode for IVF Scan

```cpp
class IVFIndexScanNode : public QueryPlanNode {
public:
    IVFIndexScanNode(IVFIndex* index,
                    const float* query_vector,
                    uint32_t k,
                    uint32_t nprobe,
                    uint32_t query_txn,
                    TransactionManager* txn_mgr)
        : index_(index),
          query_vector_(query_vector),
          k_(k),
          nprobe_(nprobe),
          query_txn_(query_txn),
          txn_mgr_(txn_mgr) {}

    Status execute(ExecutionContext* ctx) override {
        // Execute IVF search
        results_ = index_->search(query_vector_, k_, nprobe_, query_txn_, txn_mgr_, ctx);

        // Convert to row IDs for heap fetch
        for (const auto& result : results_) {
            output_row_ids_.push_back(result.vector_id);
        }

        return Status::OK;
    }

    std::vector<uint64_t> getOutputRowIds() const override {
        return output_row_ids_;
    }

private:
    IVFIndex* index_;
    const float* query_vector_;
    uint32_t k_;
    uint32_t nprobe_;
    uint32_t query_txn_;
    TransactionManager* txn_mgr_;
    std::vector<SearchResult> results_;
    std::vector<uint64_t> output_row_ids_;
};
```

---

## Query Planner Integration

### Cost Estimation

```cpp
double QueryPlanner::estimateIVFIndexScanCost(IVFIndex* index,
                                             uint32_t k,
                                             uint32_t nprobe,
                                             uint64_t table_size) {
    // Formula: Cost = (nprobe / nlist) × table_size × PQ_DISTANCE_COST + SORTING_COST

    double selectivity = (double)nprobe / index->getNList();
    double vectors_scanned = selectivity * table_size;
    double pq_distance_cost = 0.01;  // PQ distance is very fast
    double sorting_cost = vectors_scanned * log2(vectors_scanned) * 0.001;

    double total_cost = vectors_scanned * pq_distance_cost + sorting_cost;

    return total_cost;
}

double QueryPlanner::estimateSequentialScanCost(uint64_t table_size,
                                               uint32_t dimension) {
    // Formula: Cost = table_size × FULL_VECTOR_DISTANCE_COST
    double full_distance_cost = dimension * 0.001;  // L2 distance on full vector
    return table_size * full_distance_cost;
}

bool QueryPlanner::shouldUseIVFIndex(IVFIndex* index,
                                    uint32_t k,
                                    uint32_t nprobe,
                                    uint64_t table_size) {
    double ivf_cost = estimateIVFIndexScanCost(index, k, nprobe, table_size);
    double seq_cost = estimateSequentialScanCost(table_size, index->getDimension());

    // Use IVF if it's cheaper
    return ivf_cost < seq_cost;
}
```

### Plan Selection

```cpp
QueryPlanNode* QueryPlanner::planVectorSimilarityQuery(
    const VectorSimilarityQuery& query,
    Table* table,
    ErrorContext* ctx) {

    IVFIndex* ivf_index = table->getIVFIndex(query.column_id);

    // Check if IVF index exists and is trained
    if (!ivf_index || !ivf_index->isTrained()) {
        // Fall back to sequential scan
        return new SequentialScanNode(table, query);
    }

    // Cost-based decision
    uint64_t table_size = table->getRowCount();
    if (shouldUseIVFIndex(ivf_index, query.k, query.nprobe, table_size)) {
        // Use IVF index
        return new IVFIndexScanNode(ivf_index, query.vector, query.k,
                                   query.nprobe, query.txn_id, txn_mgr_);
    } else {
        // Sequential scan is cheaper (small table)
        return new SequentialScanNode(table, query);
    }
}
```

---

## Bytecode Integration

### SQL Syntax Extensions

```sql
-- Create IVF index
CREATE INDEX idx_embedding_ivf ON images USING ivf(embedding)
WITH (
    nlist = 4096,      -- Number of clusters
    nprobe = 20,       -- Default search parameter
    pq_m = 16,         -- Product Quantization subquantizers
    metric = 'L2'      -- Distance metric: L2, Cosine, or InnerProduct
);

-- Vector similarity operators
embedding <-> query    -- L2 distance
embedding <#> query    -- Inner product (negative for max similarity)
embedding <=> query    -- Cosine distance

-- Query with custom nprobe
SELECT * FROM images
ORDER BY embedding <-> query_vec
LIMIT 10
OPTIONS (nprobe = 50);  -- Override default nprobe for this query
```

### Bytecode Instructions

```cpp
// Vector similarity search bytecode
enum class VectorOpcode {
    VECTOR_L2_DISTANCE,       // Compute L2 distance between two vectors
    VECTOR_COSINE_DISTANCE,   // Compute cosine distance
    VECTOR_INNER_PRODUCT,     // Compute inner product
    IVF_INDEX_SEARCH,         // Execute IVF index search
    VECTOR_NORMALIZE,         // Normalize vector (for cosine similarity)
};

struct IVFSearchInstruction {
    uint16_t index_id;           // IVF index ID
    uint16_t query_vector_reg;   // Register holding query vector
    uint16_t k_reg;              // Register holding k value
    uint16_t nprobe_reg;         // Register holding nprobe value
    uint16_t result_reg;         // Register to store result row IDs
};
```

### Bytecode Execution

```cpp
Status VirtualMachine::executeIVFSearch(const IVFSearchInstruction& instr,
                                       ExecutionContext* ctx) {
    // 1. Load operands from registers
    const VectorValue* query_vector = ctx->getRegister(instr.query_vector_reg).as_vector;
    uint32_t k = ctx->getRegister(instr.k_reg).as_uint32;
    uint32_t nprobe = ctx->getRegister(instr.nprobe_reg).as_uint32;

    // 2. Get IVF index
    IVFIndex* index = ctx->getIndex(instr.index_id)->as_ivf_index;

    // 3. Execute search
    std::vector<SearchResult> results = index->search(
        query_vector->values, k, nprobe,
        ctx->transaction_id, ctx->txn_manager, &ctx->error_ctx);

    // 4. Store results in register
    ctx->setRegister(instr.result_reg, results);

    return Status::OK;
}
```

---

## Implementation Steps

### Phase 1: VECTOR Data Type (4-6 weeks)

**Week 1-2: Core Type Implementation**
- [ ] Define `VectorValue` struct and storage format
- [ ] Implement VECTOR(d) SQL type parser
- [ ] Add heap tuple support for variable-length vectors
- [ ] Implement serialization/deserialization

**Week 3-4: Distance Operators**
- [ ] Implement L2 distance function (`<->`)
- [ ] Implement cosine distance (`<=>`)
- [ ] Implement inner product (`<#>`)
- [ ] Add SIMD optimizations for distance computations

**Week 5-6: SQL Integration**
- [ ] Add VECTOR type to type system
- [ ] Implement vector literal parsing: `'[1.0, 2.0, ...]'::VECTOR(128)`
- [ ] Add distance operators to query planner
- [ ] Write comprehensive tests

**Estimated effort:** 200-250 hours

---

### Phase 2: Faiss Integration (2-3 weeks)

**Week 1: Build System**
- [ ] Add Faiss as external dependency
- [ ] Configure CMake for Faiss linking
- [ ] Add optional GPU support flags
- [ ] Test Faiss installation on target platforms

**Week 2: Wrapper Classes**
- [ ] Create C++ wrapper for `faiss::IndexIVFPQ`
- [ ] Implement training interface
- [ ] Implement search interface
- [ ] Add error handling and status codes

**Week 3: Testing**
- [ ] Unit tests for Faiss integration
- [ ] Benchmark k-means training performance
- [ ] Benchmark search performance
- [ ] Validate accuracy (recall@k metrics)

**Estimated effort:** 80-120 hours

---

### Phase 3: IVF Index Implementation (8-10 weeks)

**Week 1-2: On-Disk Structures**
- [ ] Implement `SBIVFIndexMetaPage`
- [ ] Implement `SBIVFCentroidsPage`
- [ ] Implement `SBIVFCodebookPage`
- [ ] Implement `SBIVFInvertedListPage`
- [ ] Page allocation and management

**Week 3-4: Training Phase**
- [ ] Implement training data sampling
- [ ] Integrate Faiss k-means for centroids
- [ ] Integrate Faiss PQ for codebooks
- [ ] Implement training data persistence

**Week 5-6: Index Operations**
- [ ] Implement `IVFIndex::insert()`
- [ ] Implement `IVFIndex::remove()`
- [ ] Implement `IVFIndex::update()`
- [ ] Implement `IVFIndex::search()`

**Week 7: MGA Compliance**
- [ ] Add transaction metadata to inverted list entries
- [ ] Implement visibility checks in search
- [ ] Implement sweep/GC for deleted entries
- [ ] Test with concurrent transactions

**Week 8-9: DML Integration**
- [ ] Hook IVF index into INSERT operations
- [ ] Hook IVF index into DELETE operations
- [ ] Hook IVF index into UPDATE operations
- [ ] Implement bulk loading for existing tables

**Week 10: Query Planner**
- [ ] Add IVF index scan plan node
- [ ] Implement cost estimation
- [ ] Add plan selection logic
- [ ] Test query plans

**Estimated effort:** 350-420 hours

---

### Phase 4: Testing and Optimization (3-4 weeks)

**Week 1: Functional Testing**
- [ ] Unit tests for all IVF operations
- [ ] Integration tests with DML
- [ ] MGA visibility tests
- [ ] Edge case testing (empty lists, large lists, etc.)

**Week 2: Performance Testing**
- [ ] Benchmark search latency (varying nlist, nprobe, k)
- [ ] Benchmark training time
- [ ] Benchmark index build time
- [ ] Memory usage profiling

**Week 3: Accuracy Testing**
- [ ] Measure recall@k for different nprobe values
- [ ] Test with different datasets (SIFT, GloVe, etc.)
- [ ] Validate distance computations
- [ ] Compare with ground truth (brute force)

**Week 4: Optimization**
- [ ] SIMD optimizations for distance computations
- [ ] Optimize page I/O patterns
- [ ] Prefetching for inverted lists
- [ ] Multithreading for search (optional)

**Estimated effort:** 150-200 hours

---

### Total Implementation Estimate

**Total time:** 17-23 weeks (4-6 months)
**Total effort:** 780-990 hours

**Team recommendation:**
- 2 engineers full-time: 4-5 months
- 1 engineer full-time: 8-10 months

---

## Testing Requirements

### Unit Tests

```cpp
TEST(IVFIndexTest, TrainingKMeans) {
    // Test k-means clustering
    std::vector<float*> train_vectors = generateRandomVectors(100000, 128);
    std::vector<float> centroids;

    Status status = IVFIndex::trainKMeans(train_vectors, 4096, 128, centroids, &ctx);
    EXPECT_OK(status);
    EXPECT_EQ(centroids.size(), 4096 * 128);

    // Verify centroids are not NaN or Inf
    for (float val : centroids) {
        EXPECT_FALSE(std::isnan(val));
        EXPECT_FALSE(std::isinf(val));
    }
}

TEST(IVFIndexTest, ProductQuantization) {
    // Test PQ encoding/decoding
    std::vector<float*> train_vectors = generateRandomVectors(100000, 128);
    std::vector<std::vector<float>> codebooks;

    Status status = IVFIndex::trainProductQuantization(train_vectors, 128, 16, codebooks, &ctx);
    EXPECT_OK(status);
    EXPECT_EQ(codebooks.size(), 16);

    // Test encoding
    float test_vector[128];
    fillRandomVector(test_vector, 128);
    std::vector<uint8_t> pq_code = encodePQ(test_vector, 128, 16, codebooks);
    EXPECT_EQ(pq_code.size(), 16);
}

TEST(IVFIndexTest, SearchAccuracy) {
    // Test search recall
    IVFIndex index(4096, 128, 16, DistanceMetric::L2, 16384);

    // Train with dataset
    std::vector<float*> train_vectors = loadSIFTDataset(100000);
    index.train(train_vectors);

    // Insert vectors
    for (size_t i = 0; i < train_vectors.size(); i++) {
        index.insert(i, train_vectors[i], TXN_BULK_LOAD, &ctx);
    }

    // Query and measure recall
    float query_vector[128];
    fillRandomVector(query_vector, 128);

    // Ground truth (brute force)
    auto ground_truth = bruteForceKNN(query_vector, train_vectors, 10);

    // IVF search
    auto results = index.search(query_vector, 10, 20, TXN_READ, txn_mgr, &ctx);

    // Measure recall@10
    double recall = computeRecall(ground_truth, results);
    EXPECT_GT(recall, 0.90);  // Expect >90% recall with nprobe=20
}
```

### Integration Tests

```cpp
TEST(IVFIndexIntegrationTest, DMLOperations) {
    // Create table with vector column
    Table table("embeddings");
    table.addColumn("id", DataType::BIGINT);
    table.addColumn("embedding", DataType::VECTOR, 128);

    // Create IVF index
    IVFIndex* index = table.createIVFIndex("embedding", 4096, 16, DistanceMetric::L2);

    // Train index
    std::vector<float*> train_vectors = generateRandomVectors(100000, 128);
    index->train(&table, 1, &ctx);

    // Insert vectors
    for (uint64_t i = 0; i < 10000; i++) {
        float vector[128];
        fillRandomVector(vector, 128);
        table.insert(i, vector, txn_id);
    }

    // Query
    float query[128];
    fillRandomVector(query, 128);
    auto results = index->search(query, 10, 20, txn_id, txn_mgr, &ctx);
    EXPECT_EQ(results.size(), 10);

    // Update
    float new_vector[128];
    fillRandomVector(new_vector, 128);
    table.update(5000, new_vector, txn_id);

    // Delete
    table.deleteRow(6000, txn_id);

    // Query again
    results = index->search(query, 10, 20, txn_id, txn_mgr, &ctx);
    EXPECT_EQ(results.size(), 10);
}

TEST(IVFIndexIntegrationTest, MGAVisibility) {
    // Test transaction isolation
    IVFIndex index(1024, 128, 16, DistanceMetric::L2, 16384);
    index.train(train_vectors);

    // Transaction 1: Insert vectors
    uint32_t txn1 = txn_mgr->beginTransaction();
    for (uint64_t i = 0; i < 1000; i++) {
        index.insert(i, vectors[i], txn1, &ctx);
    }

    // Transaction 2: Query before commit
    uint32_t txn2 = txn_mgr->beginTransaction();
    auto results = index.search(query, 10, 20, txn2, txn_mgr, &ctx);
    EXPECT_EQ(results.size(), 0);  // Should not see uncommitted inserts

    // Commit transaction 1
    txn_mgr->commit(txn1);

    // Transaction 3: Query after commit
    uint32_t txn3 = txn_mgr->beginTransaction();
    results = index.search(query, 10, 20, txn3, txn_mgr, &ctx);
    EXPECT_GT(results.size(), 0);  // Should see committed inserts
}
```

### Performance Tests

```cpp
TEST(IVFIndexPerfTest, SearchLatency) {
    // Benchmark search latency
    IVFIndex index(16384, 128, 16, DistanceMetric::L2, 32768);

    // Load 1M vectors
    loadVectorsFromFile("sift1M.bin", &index);

    // Benchmark with varying nprobe
    for (uint32_t nprobe : {1, 5, 10, 20, 50, 100}) {
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 1000; i++) {
            float query[128];
            fillRandomVector(query, 128);
            auto results = index.search(query, 10, nprobe, txn_id, txn_mgr, &ctx);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double avg_latency_us = duration.count() / 1000.0;

        std::cout << "nprobe=" << nprobe << ": " << avg_latency_us << " μs/query\n";
    }
}

TEST(IVFIndexPerfTest, TrainingTime) {
    // Benchmark training time
    std::vector<float*> train_vectors = generateRandomVectors(256000, 128);

    auto start = std::chrono::high_resolution_clock::now();

    IVFIndex index(16384, 128, 16, DistanceMetric::L2, 32768);
    index.train(train_vectors);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

    std::cout << "Training time: " << duration.count() << " seconds\n";
    EXPECT_LT(duration.count(), 300);  // Should complete within 5 minutes
}
```

---

## Performance Targets

### Search Latency

| Dataset Size | nprobe | Target Latency (p50) | Target Latency (p99) |
|--------------|--------|----------------------|----------------------|
| 100K vectors | 10     | < 2 ms               | < 5 ms               |
| 100K vectors | 20     | < 3 ms               | < 8 ms               |
| 1M vectors   | 10     | < 5 ms               | < 15 ms              |
| 1M vectors   | 20     | < 8 ms               | < 25 ms              |
| 10M vectors  | 20     | < 20 ms              | < 60 ms              |
| 10M vectors  | 50     | < 40 ms              | < 100 ms             |

### Recall (Accuracy)

| nprobe | Target Recall@10 | Target Recall@100 |
|--------|------------------|-------------------|
| 1      | > 50%            | > 60%             |
| 10     | > 85%            | > 90%             |
| 20     | > 92%            | > 95%             |
| 50     | > 96%            | > 98%             |
| 100    | > 98%            | > 99%             |

### Training Time

| Dataset Size | nlist  | Target Training Time (CPU) | Target Training Time (GPU) |
|--------------|--------|----------------------------|----------------------------|
| 100K vectors | 4096   | < 60 seconds               | < 10 seconds               |
| 256K vectors | 16384  | < 180 seconds              | < 30 seconds               |
| 1M vectors   | 65536  | < 600 seconds (10 min)     | < 90 seconds               |

### Memory Usage

| Dataset Size | nlist  | PQ m | Centroids | Codebooks | Inverted Lists (compressed) | Total |
|--------------|--------|------|-----------|-----------|----------------------------|-------|
| 1M vectors   | 4096   | 16   | 2 MB      | 0.5 MB    | 16 MB                      | ~19 MB |
| 10M vectors  | 16384  | 16   | 8 MB      | 0.5 MB    | 160 MB                     | ~169 MB |
| 100M vectors | 65536  | 16   | 32 MB     | 0.5 MB    | 1.6 GB                     | ~1.6 GB |

**Note:** Without PQ compression, 100M × 128-dim vectors = 51.2 GB

### Index Build Time

| Dataset Size | Target Build Time |
|--------------|-------------------|
| 100K vectors | < 2 minutes       |
| 1M vectors   | < 15 minutes      |
| 10M vectors  | < 2 hours         |

---

## Future Enhancements

### 1. GPU Acceleration

**Faiss GPU Support:**
```cpp
#include <faiss/gpu/GpuIndexIVFPQ.h>

// Use GPU for training and search
faiss::gpu::GpuIndexIVFPQ gpu_index(&gpu_resources, cpu_index);
gpu_index.train(n_train, train_vectors);
gpu_index.search(n_query, query_vectors, k, distances, labels);
```

**Benefits:**
- 10-50x faster training
- 5-20x faster search
- Enables real-time updates

**Challenges:**
- Requires CUDA/ROCm
- GPU memory limits
- Host-device transfers

---

### 2. Hybrid HNSW-IVF Index

**Concept:** Combine HNSW graph for coarse quantization with IVF for fine-grained search.

**Benefits:**
- Better accuracy than pure IVF
- Faster than pure HNSW for large datasets
- Adaptive routing

**Implementation:**
```cpp
class HNSWIVFIndex {
    HNSW hnsw_coarse_;       // For finding nearest centroids
    IVF ivf_fine_;           // For inverted lists
};
```

---

### 3. Optimized Product Quantization (OPQ)

**Optimized PQ:** Learn a rotation matrix to better align subspaces before quantization.

**Benefits:**
- 10-20% better recall for same compression
- Minimal search overhead

**Formula:**
```
OPQ: Encode(R × vector) where R is learned rotation matrix
```

---

### 4. Multi-Vector Queries

**Use case:** Find images similar to multiple query images.

```sql
SELECT * FROM images
ORDER BY embedding <-> ANY(ARRAY[query1, query2, query3])
LIMIT 10;
```

**Implementation:** Merge results from multiple IVF searches.

---

### 5. Range Queries

**Use case:** Find all vectors within distance threshold.

```sql
SELECT * FROM images
WHERE embedding <-> query_vec < 0.5;
```

**Challenge:** Requires dynamic nprobe based on threshold.

---

### 6. Incremental Training

**Problem:** Current IVF requires full retraining when adding new data.

**Solution:** Online k-means updates to centroids and codebooks.

**Benefits:**
- No downtime for retraining
- Adapts to data drift

---

## Conclusion

### Feature Checklist

This specification provides a **complete, implementation-ready design** for IVF vector search indexes in ScratchBird:

✅ **Page-size agnostic design** (8KB-128KB)
✅ **Faiss integration** for production-quality algorithms
✅ **Product Quantization** for 32-64x compression
✅ **Configurable accuracy/speed trade-off** (nprobe parameter)
✅ **MGA compliance** for transaction isolation
✅ **Complete DML integration** (INSERT/UPDATE/DELETE)
✅ **Query planner integration** with cost-based optimization
✅ **Comprehensive testing strategy**
✅ **Detailed implementation plan** (17-23 weeks)
✅ **Performance targets** for all operations

### Prerequisites Reminder

**CRITICAL:** IVF indexes require:
1. ✅ VECTOR(d) data type implementation
2. ✅ Faiss library integration
3. ✅ 30K-256K training vectors at index creation time

### Recommendation

**For ScratchBird Alpha:**
- Implement Bloom Filter, Zone Maps, and Inverted Index first (no external dependencies)
- Consider IVF index as **Phase 2** or future release
- IVF is powerful but adds significant complexity (Faiss dependency, training requirement)

**For Production Vector Search:**
- IVF with PQ provides excellent balance of accuracy, speed, and memory efficiency
- Industry-proven architecture (used by Meta, Pinterest, Spotify)
- Suitable for 100K-1B+ vector datasets

### References

- Faiss: https://github.com/facebookresearch/faiss
- Product Quantization paper: Jégou et al., "Product Quantization for Nearest Neighbor Search" (2011)
- IVF-PQ: "Billion-scale similarity search with GPUs" (2017)
- Recall benchmarks: http://ann-benchmarks.com

---

**End of IVF Index Specification v1.0**
