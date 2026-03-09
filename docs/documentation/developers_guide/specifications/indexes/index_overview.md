# Index Types Overview

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

This document provides an overview of all 28+ index types supported by ScratchBird, including their use cases, performance characteristics, and selection guidance.

## Index Categories

### Traditional Indexes (OLTP-Optimized)

| Index | Type | Best For | Time Complexity | Space |
|-------|------|----------|-----------------|-------|
| [B-Tree](./index_btree.md) | Balanced tree | Range, order, unique | O(log n) | Medium |
| [Hash](./index_hash.md) | Hash table | Exact match | O(1) avg | Medium |
| [GIN](./index_gin.md) | Inverted | Multi-value, arrays | O(log n) | Large |
| [GiST](./index_gist.md) | Extensible | Custom types, spatial | O(log n) | Medium |
| [BRIN](./index_brin.md) | Block summary | Large sequential tables | O(n/block_size) | Tiny |
| [R-Tree](./index_rtree.md) | Spatial | 2D/3D geometric data | O(log n) | Medium |
| [SP-GiST](./index_spgist.md) | Partitioned | Quadtrees, radix trees | O(log n) | Medium |
| [Bitmap](./index_bitmap.md) | Bitmap | Low cardinality columns | O(1) per bit | Small |

### Vector Indexes (Similarity Search)

| Index | Type | Best For | Recall | Memory |
|-------|------|----------|--------|--------|
| [HNSW/VECTOR](./index_hnsw.md) | Graph | General ANN | 95%+ | High |
| [IVF](./index_ivf.md) | Clustering | Uniform distributions | 85-95% | Medium |
| [VECTOR_FLAT](./index_vector_flat.md) | Brute-force | Exact, small datasets | 100% | Very High |
| [VECTOR_BIN_FLAT](./index_bin_flat.md) | Binary | Binary embeddings | 100% | High |
| [IVF_FLAT](./index_ivf_flat.md) | IVF+Flat | High recall, medium scale | 95%+ | High |
| [BIN_IVF_FLAT](./index_bin_ivf_flat.md) | Binary IVF | Binary ANN | 90%+ | Medium |
| [IVF_PQ](./index_ivf_pq.md) | IVF+PQ | Large scale (1B+) | 80-90% | Low |
| [IVF_SQ8](./index_ivf_sq8.md) | IVF+SQ8 | Balanced speed/memory | 85-92% | Low |
| [IVF_SQ8_HYBRID](./index_ivf_sq8_hybrid.md) | Hybrid routing | Predictable performance | 85-92% | Low |
| [RHNSW_PQ](./index_rhnsw_pq.md) | HNSW+PQ | Graph with compression | 90%+ | Low |
| [RHNSW_SQ](./index_rhnsw_sq.md) | HNSW+SQ | Graph with SQ | 92%+ | Low |
| [ANNOY](./index_annoy.md) | Random trees | Static datasets | 70-85% | Medium |
| [NSG](./index_nsg.md) | Proximity graph | Navigation-optimized | 90%+ | High |
| [DISKANN](./index_diskann.md) | SSD graph | Billion-scale | 90%+ | SSD |

### Specialized Indexes

| Index | Type | Best For | Key Feature |
|-------|------|----------|-------------|
| [Full-Text](./index_fulltext.md) | Inverted | Text search | TsVector/TsQuery |
| [Columnstore](./index_columnstore.md) | Columnar | OLAP analytics | Compression |
| [LSM-Tree](./index_lsm.md) | Log-structured | Write-heavy | Sequential writes |
| [Zone Map](./index_zonemap.md) | Statistics | I/O elimination | Min/max pruning |
| [ART](./index_art.md) | Radix trie | In-memory strings | Adaptive nodes |
| [Bloom](./index_bloom.md) | Probabilistic | Membership test | Zero FNs |

## Selection Guide

### By Workload Type

**OLTP (Transactional)**
- Primary keys: B-Tree
- Foreign keys: B-Tree, Hash
- Unique constraints: B-Tree
- Text search: Full-Text (GIN-based)
- Arrays/JSON: GIN
- Geospatial: GiST (R-Tree), SP-GiST

**OLAP (Analytical)**
- Fact tables: Columnstore
- Large tables: BRIN, Zone Map
- Bitmap filters: Bitmap
- Time-series: BRIN, Zone Map

**Vector Search**
- General purpose: HNSW
- Billion-scale: DiskANN
- Memory-constrained: IVF_PQ, IVF_SQ8
- Exact results (small): VECTOR_FLAT
- Binary vectors: VECTOR_BIN_FLAT

### By Data Characteristics

| Data Type | Recommended Indexes |
|-----------|-------------------|
| Integer PK | B-Tree, Hash |
| Timestamp | B-Tree (sorted), BRIN (time-series) |
| Text (exact) | Hash, B-Tree |
| Text (search) | Full-Text (GIN) |
| Arrays | GIN |
| JSONB | GIN |
| Geometry | GiST (R-Tree), SP-GiST |
| Embeddings (float) | HNSW, IVF_FLAT |
| Embeddings (binary) | VECTOR_BIN_FLAT |
| Low cardinality | Bitmap |
| High cardinality | B-Tree, Hash |

## Index Type Enum Reference

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:655-717

enum class IndexType : uint8_t {
    // Core indexes (0-13)
    BTREE = 0,           // B-tree index (default)
    HASH = 1,            // Hash index
    HNSW = 2,            // Vector similarity index
    VECTOR = 2,          // Alias for HNSW
    FULLTEXT = 3,        // Full-text search index
    GIN = 4,             // Generalized Inverted Index
    GIST = 5,            // Generalized Search Tree
    BRIN = 6,            // Block Range Index
    RTREE = 7,           // R-tree spatial index
    SPGIST = 8,          // Space-Partitioned GiST
    BITMAP = 9,          // Bitmap index
    COLUMNSTORE = 10,    // Columnstore index
    LSM = 11,            // LSM-Tree
    IVF = 12,            // IVF vector index
    ZONEMAP = 13,        // Zone map index
    
    // Extended indexes (14-27)
    ART = 0x0E,          // Adaptive radix tree
    BLOOM = 0x0F,        // Bloom filter range
    VECTOR_FLAT = 0x10,  // Brute-force float vector
    VECTOR_BIN_FLAT = 0x11, // Brute-force binary vector
    IVF_FLAT = 0x12,     // IVF flat variant
    BIN_IVF_FLAT = 0x13, // IVF binary variant
    IVF_PQ = 0x14,       // IVF product quantization
    IVF_SQ8 = 0x15,      // IVF scalar quantization
    IVF_SQ8_HYBRID = 0x16, // IVF SQ8 hybrid routing
    RHNSW_PQ = 0x17,     // HNSW with PQ
    RHNSW_SQ = 0x18,     // HNSW with SQ
    ANNOY = 0x19,        // ANNOY random projection
    NSG = 0x1A,          // NSG graph ANN
    DISKANN = 0x1B,      // DiskANN graph ANN
    
    // Additional indexes defined (0x1C+)
    SCANN = 0x1C,
    GPU_CAGRA = 0x1D,
    MINHASH_LSH = 0x1E,
    // ... (see catalog_manager.h for full list)
};
```

## Comparison Matrix

### Traditional Indexes

| Feature | B-Tree | Hash | GIN | GiST | BRIN | R-Tree |
|---------|--------|------|-----|------|------|--------|
| Exact match | ✓ | ✓ | ✓ | ✓ | - | - |
| Range queries | ✓ | - | - | ✓ | ✓ | ✓ |
| Ordering | ✓ | - | - | - | - | - |
| Multi-value | - | - | ✓ | ✓ | - | - |
| Spatial | - | - | - | ✓ | - | ✓ |
| Low maintenance | ✓ | ✓ | - | ✓ | ✓ | ✓ |
| Small size | ✓ | ✓ | - | ✓ | ✓✓ | ✓ |

### Vector Indexes

| Feature | HNSW | IVF_FLAT | IVF_PQ | IVF_SQ8 | DISKANN |
|---------|------|----------|--------|---------|---------|
| Recall@10 | 95%+ | 95%+ | 85% | 88% | 92%+ |
| Build time | Medium | Fast | Slow | Fast | Medium |
| Memory | High | High | Low | Low | Low* |
| Add vectors | Yes | Yes | Yes | Yes | No |
| Billion-scale | No | No | Yes | Yes | Yes |

*Uses SSD storage

## MGA Compliance

All indexes in ScratchBird support Multi-Generational Architecture:

- **xmin/xmax**: Transaction IDs for visibility
- **Soft delete**: Logical deletion with physical cleanup during GC
- **Snapshot isolation**: Queries see consistent view
- **Garbage collection**: Dead entries removed after OIT advances

See [index_dml_integration.md](./index_dml_integration.md) for details.

## Related Specifications

- [index_dml_integration.md](./index_dml_integration.md) - DML operations on indexes
- Individual index specifications (linked above)

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive index overview |
