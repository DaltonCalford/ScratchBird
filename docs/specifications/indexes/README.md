# Index System Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains index implementation specifications for ScratchBird's comprehensive index system, supporting 11+ index types.

## Overview

ScratchBird implements a sophisticated multi-index architecture supporting traditional B-tree indexes alongside specialized index types for full-text search, vector similarity, columnar analytics, and more. All indexes integrate with the MGA transaction system.

## Specifications in this Directory

### Core Index Architecture

- **[INDEX_ARCHITECTURE.md](INDEX_ARCHITECTURE.md)** (983 lines) - Overall index architecture and design principles
- **[INDEX_IMPLEMENTATION_GUIDE.md](INDEX_IMPLEMENTATION_GUIDE.md)** (1,135 lines) - Implementation guide for index developers
- **[INDEX_IMPLEMENTATION_SPEC.md](INDEX_IMPLEMENTATION_SPEC.md)** (915 lines) - Detailed implementation specifications
- **[INDEX_GC_PROTOCOL.md](INDEX_GC_PROTOCOL.md)** (622 lines) - Index garbage collection protocol

### Advanced Index Types

- **[AdvancedIndexes.md](AdvancedIndexes.md)** (1,283 lines) - Overview of advanced index capabilities

### Specialized Indexes

#### Full-Text & Text Search
- **[InvertedIndex.md](InvertedIndex.md)** (2,333 lines) - Inverted index for full-text search

#### Vector & Similarity Search
- **[IVFIndex.md](IVFIndex.md)** (2,243 lines) - IVF (Inverted File) index for vector similarity search

#### Filtering & Membership
- **[BloomFilterIndex.md](BloomFilterIndex.md)** (1,529 lines) - Bloom filter index for set membership tests

#### Analytics & Column Stores
- **[COLUMNSTORE_SPEC.md](COLUMNSTORE_SPEC.md)** (712 lines) - Columnar storage for OLAP workloads
- **[ZoneMapsIndex.md](ZoneMapsIndex.md)** (2,222 lines) - Zone maps (min/max pruning) for analytics

#### Write-Optimized Storage
- **[LSM_TREE_SPEC.md](LSM_TREE_SPEC.md)** (1,596 lines) - LSM tree specification for write-heavy workloads
- **[LSM_TREE_ARCHITECTURE.md](LSM_TREE_ARCHITECTURE.md)** (493 lines) - LSM tree architecture details

## Index Types Summary

| Index Type | Use Case | Status |
|------------|----------|--------|
| **B-tree** | General-purpose ordered index | ✅ Specified |
| **Hash** | Equality lookups | ✅ Specified |
| **GiST** | Extensible index framework | ✅ Specified |
| **GIN** | Inverted index (full-text, arrays) | ✅ Specified |
| **BRIN** | Block range index (large tables) | ✅ Specified |
| **Bloom Filter** | Set membership tests | ✅ Specified |
| **Inverted** | Full-text search | ✅ Specified |
| **IVF** | Vector similarity search | ✅ Specified |
| **Zone Maps** | Min/max pruning (analytics) | ✅ Specified |
| **LSM Tree** | Write-optimized storage | ✅ Specified |
| **Columnstore** | OLAP workloads | ✅ Specified |

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
| Inverted | O(k) | O(k) | O(m) | 2.0x | Full-text search |
| IVF | O(1) | O(k) | N/A | 1.5x | Vector search |
| Zone Maps | O(1) | O(b) | O(n) | 0.05x | Analytics pruning |

## Related Specifications

- [Storage Engine](../storage/) - Page management and buffer pool
- [Transaction System](../transaction/) - MGA integration and versioning
- [Query Optimization](../query/) - Index selection and cost estimation
- [Types System](../types/) - Data type indexing requirements

## Critical Reading

Before working on index implementation:

1. **MUST READ:** [../../MGA_RULES.md](../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../IMPLEMENTATION_STANDARDS.md](../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ IN ORDER:**
   - [INDEX_ARCHITECTURE.md](INDEX_ARCHITECTURE.md) - Core architecture
   - [INDEX_IMPLEMENTATION_GUIDE.md](INDEX_IMPLEMENTATION_GUIDE.md) - Implementation guide
   - [INDEX_GC_PROTOCOL.md](INDEX_GC_PROTOCOL.md) - Garbage collection

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
