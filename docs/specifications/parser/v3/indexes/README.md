# Index System Specifications

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**[← Back to Specifications Index](../README.md)**

This directory contains index implementation specifications for ScratchBird's comprehensive index system, supporting 28 core index types.

## Overview

ScratchBird implements a sophisticated multi-index architecture supporting traditional B-tree indexes alongside specialized index types for full-text search, vector similarity, columnar analytics, and more. All indexes integrate with the MGA transaction system.

## Specifications in this Directory

### Core Index Architecture

- **[INDEX_ARCHITECTURE.md](INDEX_ARCHITECTURE.md)** (983 lines) - Overall index architecture and design principles
- **[INDEX_IMPLEMENTATION_GUIDE.md](INDEX_IMPLEMENTATION_GUIDE.md)** (1,135 lines) - Implementation guide for index developers
- **[INDEX_IMPLEMENTATION_SPEC.md](INDEX_IMPLEMENTATION_SPEC.md)** (915 lines) - Detailed implementation specifications
- **[INDEX_GC_PROTOCOL.md](INDEX_GC_PROTOCOL.md)** (622 lines) - Index garbage collection protocol
- **[INDEX_IMPLEMENTATION_REFERENCE.md](INDEX_IMPLEMENTATION_REFERENCE.md)** - Authoritative algorithm links by index type
- **[BTREE_SPEC.md](BTREE_SPEC.md)** - Authoritative B-Tree algorithm and on-disk layout
- **[HASH_SPEC.md](HASH_SPEC.md)** - Authoritative Hash index algorithm and layout
- **[GIN_SPEC.md](GIN_SPEC.md)** - Authoritative GIN algorithm and posting structures
- **[GIST_SPEC.md](GIST_SPEC.md)** - Authoritative GiST algorithm and split policies
- **[SPGIST_SPEC.md](SPGIST_SPEC.md)** - Authoritative SP-GiST algorithm and partitioning
- **[BRIN_SPEC.md](BRIN_SPEC.md)** - Authoritative BRIN algorithm and summaries
- **[BITMAP_SPEC.md](BITMAP_SPEC.md)** - Authoritative Bitmap algorithm and compression
- **[RTREE_SPEC.md](RTREE_SPEC.md)** - Authoritative R-Tree algorithm and split/merge
- **[HNSW_SPEC.md](HNSW_SPEC.md)** - Authoritative HNSW algorithm and graph layout

### Advanced Index Types

- **[AdvancedIndexes.md](AdvancedIndexes.md)** (1,283 lines) - Overview of advanced index capabilities

### Specialized Indexes

#### Spatial & Multi-Dimensional
- **[ZOrderIndex.md](ZOrderIndex.md)** - Z-order (Morton) index for multi-dimensional keys
- **[GeohashS2Index.md](GeohashS2Index.md)** - Geohash and S2 cell index
- **[QuadtreeOctreeIndex.md](QuadtreeOctreeIndex.md)** - Quadtree/Octree spatial index

#### Full-Text & Text Search
- **[InvertedIndex.md](InvertedIndex.md)** (2,333 lines) - Inverted index for full-text search
- **[FSTIndex.md](FSTIndex.md)** - Finite State Transducer (prefix/autocomplete)
- **[SuffixIndex.md](SuffixIndex.md)** - Suffix array/tree for substring search

#### Document & Semi-Structured
- **[JSONPathIndex.md](JSONPathIndex.md)** - JSON/JSONB path index (core)

#### Vector & Similarity Search
- **[IVFIndex.md](IVFIndex.md)** (2,243 lines) - IVF (Inverted File) index for vector similarity search

#### Filtering & Membership
- **[BloomFilterIndex.md](BloomFilterIndex.md)** (1,529 lines) - Bloom filter index for set membership tests
- **[CountMinSketchIndex.md](CountMinSketchIndex.md)** - Count-Min Sketch for frequency estimation
- **[HyperLogLogIndex.md](HyperLogLogIndex.md)** - HyperLogLog for approximate distinct counts

#### Modern In-Memory & Learned Indexes
- **[AdaptiveRadixTreeIndex.md](AdaptiveRadixTreeIndex.md)** - Adaptive Radix Tree (ART)
- **[LearnedIndex.md](LearnedIndex.md)** - Learned index (RMI)

#### Analytics & Column Stores
- **[COLUMNSTORE_SPEC.md](COLUMNSTORE_SPEC.md)** (712 lines) - Columnar storage for OLAP workloads
- **[ZoneMapsIndex.md](ZoneMapsIndex.md)** (2,222 lines) - Zone maps (min/max pruning) for analytics

#### Write-Optimized Storage
- **[LSM_TREE_SPEC.md](LSM_TREE_SPEC.md)** (1,596 lines) - LSM tree specification for write-heavy workloads
- **[LSM_TREE_ARCHITECTURE.md](LSM_TREE_ARCHITECTURE.md)** (493 lines) - LSM tree architecture details
- **[LSMTimeSeriesIndex.md](LSMTimeSeriesIndex.md)** - LSM with TTL for time-series workloads

## Index Types Summary

| Index Type | Use Case | Status |
|------------|----------|--------|
| **B-tree** | General-purpose ordered index | ✅ Specified |
| **Hash** | Equality lookups | ✅ Specified |
| **GiST** | Extensible index framework | ✅ Specified |
| **GIN** | Inverted index (full-text, arrays) | ✅ Specified |
| **BRIN** | Block range index (large tables) | ✅ Specified |
| **Bloom Filter** | Set membership tests | ✅ Specified |
| **Fulltext (Inverted)** | Full-text search | ✅ Specified |
| **IVF** | Vector similarity search | ✅ Specified |
| **Zone Maps** | Min/max pruning (analytics) | ✅ Specified |
| **LSM Tree** | Write-optimized storage | ✅ Specified |
| **Columnstore** | OLAP workloads | ✅ Specified |
| **JSON Path** | JSON/JSONB path predicates | ✅ Specified |
| **Z-Order (Morton)** | Multi-dimensional range pruning | ✅ Specified |
| **Geohash / S2** | Geo cell indexing | ✅ Specified |
| **Quadtree / Octree** | Spatial partitioning | ✅ Specified |
| **FST** | Prefix search, autocomplete | ✅ Specified |
| **Suffix Array / Tree** | Substring search | ✅ Specified |
| **Count-Min Sketch** | Frequency estimation | ✅ Specified |
| **HyperLogLog** | Approx distinct counts | ✅ Specified |
| **ART** | In-memory key lookup | ✅ Specified |
| **Learned Index** | Model-based lookup | ✅ Specified |
| **LSM TTL** | Time-series retention | ✅ Specified |

## Canonical Index Type Names (Parser/Catalog)

**Core (V3 scope):** BTREE, HASH, FULLTEXT, GIN, GIST, SPGIST, BRIN, BITMAP, RTREE, HNSW, COLUMNSTORE, LSM

**Core (All Index Types):** BTREE, HASH, FULLTEXT, GIN, GIST, SPGIST, BRIN, BITMAP, RTREE, HNSW, COLUMNSTORE, LSM, IVF, ZONEMAP, ZORDER, GEOHASH, S2, QUADTREE, OCTREE, FST, SUFFIX_ARRAY, SUFFIX_TREE, COUNT_MIN_SKETCH, HYPERLOGLOG, ART, LEARNED, LSM_TTL, JSON_PATH

## Key Concepts

### MGA Integration

All indexes must respect Multi-Generational Architecture:

- **Index versioning** - Multiple versions of index entries for different transactions
- **Non-blocking reads** - Index reads don't block writes
- **Garbage collection** - Old index entries removed when versions are swept

### Index Garbage Collection

See [INDEX_GC_PROTOCOL.md](INDEX_GC_PROTOCOL.md) for:

- Cooperative garbage collection between heap and indexes
- Tombstone handling and removal
- Vacuum integration

### Performance Characteristics

| Index Type | Insert | Lookup | Scan | Space | Best For |
|------------|--------|--------|------|-------|----------|
| B-tree | O(log n) | O(log n) | O(n) | 1.0x | General purpose |
| Hash | O(1) | O(1) | N/A | 0.8x | Equality only |
| LSM Tree | O(1)* | O(log n) | O(n) | 0.6x | Write-heavy |
| Bloom Filter | O(1) | O(1) | N/A | 0.1x | Membership tests |
| Fulltext (Inverted) | O(k) | O(k) | O(m) | 2.0x | Full-text search |
| IVF | O(1) | O(k) | N/A | 1.5x | Vector search |
| Zone Maps | O(1) | O(b) | O(n) | 0.05x | Analytics pruning |

## Related Specifications

- [Storage Engine](../storage/) - Page management and buffer pool
- [Transaction System](../transaction/) - MGA integration and versioning
- [Query Optimization](../query/) - Index selection and cost estimation
- [Types System](../types/) - Data type indexing requirements

## Critical Reading

Before working on index implementation:

1. **MUST READ:** [/docs/specifications/parser/v3/MGA_RULES.md](/docs/specifications/parser/v3/MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [/docs/specifications/parser/v3/IMPLEMENTATION_STANDARDS.md](/docs/specifications/parser/v3/IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ IN ORDER:**
   - [INDEX_ARCHITECTURE.md](INDEX_ARCHITECTURE.md) - Core architecture
   - [INDEX_IMPLEMENTATION_GUIDE.md](INDEX_IMPLEMENTATION_GUIDE.md) - Implementation guide
   - [INDEX_GC_PROTOCOL.md](INDEX_GC_PROTOCOL.md) - Garbage collection

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
