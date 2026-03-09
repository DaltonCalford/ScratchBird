# Specification: Page Types

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | storage |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird Alpha |
| **Authors** | Dalton Calford |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/ondisk.h:23`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_page_types.cpp`

## Synopsis

This specification defines all page types used in ScratchBird, organized by functional category. Each page type has a unique 16-bit identifier and specific layout requirements.

## Scope

### In Scope

- Core system page types
- Heap and storage page types
- Index page types (all variants)
- vNext multi-model page types
- Page type validation

### Out of Scope

- Specific page layouts (see individual specs)
- Page type migration/upgrade
- Custom page types

## Background

Page types are defined in `ondisk.h` and follow these conventions:
- **0x0000-0x00FF**: Core system pages
- **0x0100-0x01FF**: Index pages
- **0x0200-0x02FF**: Columnstore and LSM
- **0x0300-0x03FF**: Vector and ANN indexes
- **0x0400-0x04FF**: Emulation and Redis
- **0x2000-0x20FF**: vNext multi-model (reserved)

## Specification

### PageType Enum

```cpp
// From include/scratchbird/core/ondisk.h:23
enum PageType : uint16_t {
    // Core and system (0x0000-0x000F)
    PAGE_TYPE_DATABASE_HEADER = 0x0000,
    PAGE_TYPE_SYSTEM_STATE = 0x0001,
    PAGE_TYPE_CATALOG_ROOT = 0x0002,
    PAGE_TYPE_CATALOG_PAGE = 0x0003,
    PAGE_TYPE_FSM_ROOT = 0x0004,
    PAGE_TYPE_FSM_PAGE = 0x0005,
    PAGE_TYPE_TRANSACTION_MAP = 0x0006,
    PAGE_TYPE_HEAP = 0x0007,
    PAGE_TYPE_TOAST_META = 0x0008,
    PAGE_TYPE_TOAST_CHUNK = 0x0009,
    PAGE_TYPE_LOB_META = 0x000A,
    PAGE_TYPE_LOB_CHUNK = 0x000B,
    PAGE_TYPE_TEMP_HEAP = 0x000C,
    PAGE_TYPE_NAME_REGISTRY = 0x000D,
    PAGE_TYPE_BOOTSTRAP_RESERVED = 0x000E,
    PAGE_TYPE_FILESPACE_HEADER = 0x000F,
    
    // Index types (0x0100-0x01FF)
    PAGE_TYPE_BTREE_META = 0x0100,
    PAGE_TYPE_BTREE_INTERNAL = 0x0101,
    PAGE_TYPE_BTREE_LEAF = 0x0102,
    PAGE_TYPE_HASH_META = 0x0110,
    // ... (see full list below)
};
```

### Core System Pages (0x0000-0x000F)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0000 | DATABASE_HEADER | Primary database header (page 0) |
| 0x0001 | SYSTEM_STATE | Clean shutdown state (page 1) |
| 0x0002 | CATALOG_ROOT | Catalog B-tree root (page 2) |
| 0x0003 | CATALOG_PAGE | Catalog data pages |
| 0x0004 | FSM_ROOT | Free Space Map root (page 3) |
| 0x0005 | FSM_PAGE | Additional FSM pages |
| 0x0006 | TRANSACTION_MAP | Transaction map root (page 4) |
| 0x0007 | HEAP | Heap table data pages |
| 0x0008 | TOAST_META | TOAST metadata page |
| 0x0009 | TOAST_CHUNK | TOAST data chunks |
| 0x000A | LOB_META | Large Object metadata |
| 0x000B | LOB_CHUNK | Large Object data chunks |
| 0x000C | TEMP_HEAP | Temporary table data |
| 0x000D | NAME_REGISTRY | Object name registry |
| 0x000E | BOOTSTRAP_RESERVED | Reserved bootstrap page |
| 0x000F | FILESPACE_HEADER | Tablespace file header |

### Index Pages (0x0100-0x01FF)

#### B-tree Indexes (0x0100-0x0102)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0100 | BTREE_META | B-tree metadata |
| 0x0101 | BTREE_INTERNAL | B-tree internal nodes |
| 0x0102 | BTREE_LEAF | B-tree leaf pages |

#### Hash Indexes (0x0110-0x0113)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0110 | HASH_META | Hash index metadata |
| 0x0111 | HASH_BUCKET | Hash bucket pages |
| 0x0112 | HASH_OVERFLOW | Hash overflow pages |
| 0x0113 | HASH_BITMAP | Hash bitmap pages |

#### GIN Indexes (0x0120-0x0123)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0120 | GIN_META | GIN metadata |
| 0x0121 | GIN_ENTRY | GIN entry pages |
| 0x0122 | GIN_DATA | GIN posting tree |
| 0x0123 | GIN_PENDING | GIN pending list |

#### GiST Indexes (0x0130-0x0131)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0130 | GIST_INTERNAL | GiST internal nodes |
| 0x0131 | GIST_LEAF | GiST leaf pages |

#### SP-GiST Indexes (0x0140-0x0142)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0140 | SPGIST_META | SP-GiST metadata |
| 0x0141 | SPGIST_INNER | SP-GiST inner nodes |
| 0x0142 | SPGIST_LEAF | SP-GiST leaf pages |

#### BRIN Indexes (0x0150-0x0152)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0150 | BRIN_META | BRIN metadata |
| 0x0151 | BRIN_REVMAP | BRIN reverse map |
| 0x0152 | BRIN_DATA | BRIN summary data |

#### Bitmap Indexes (0x0160-0x0162)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0160 | BITMAP_META | Bitmap index metadata |
| 0x0161 | BITMAP_DICT | Bitmap dictionary |
| 0x0162 | BITMAP_CONTAINER | Bitmap containers |

#### Additional Index Types

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0170-0x0172 | INVERTED_* | Inverted index |
| 0x0180-0x0182 | SPARSE_* | Sparse index |
| 0x0190-0x0192 | FTS_* | Full-text search |
| 0x01A0-0x01A1 | TRIE_* | Trie index |
| 0x01A8 | ART_NODE | Adaptive Radix Tree |
| 0x01B0-0x01B1 | SPATIAL_* | Spatial/R-tree index |
| 0x01C0-0x01C1 | MINHASH_* | MinHash index |
| 0x01D0-0x01D1 | BLOOM_* | Bloom filter index |
| 0x01E0-0x01E4 | SAI_* | SAI (Storage-Attached Index) |
| 0x01F0-0x01F3 | SASI_* | SASI (Cassandra-style) |

### Columnstore and LSM (0x0200-0x02FF)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0200 | COLUMNSTORE_META | Columnstore metadata |
| 0x0201 | COLUMNSTORE_SEGMENT | Column segment |
| 0x0202 | COLUMNSTORE_DICT | Column dictionary |
| 0x0203 | COLUMNSTORE_RLE | RLE-encoded data |
| 0x0204 | COLUMNSTORE_BITPACK | Bit-packed data |
| 0x0210 | LSM_META | LSM tree metadata |
| 0x0211 | LSM_INDEX | LSM index pages |
| 0x0212 | LSM_SSTABLE | LSM SSTable data |
| 0x0213 | LSM_FILTER | LSM bloom filters |
| 0x0220 | SORT_META | External sort metadata |
| 0x0221 | SORT_RUN | Sort run data |

### Vector and ANN Indexes (0x0300-0x03FF)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0300 | HNSW_META | HNSW metadata |
| 0x0301 | HNSW_NODE | HNSW graph nodes |
| 0x0310 | IVF_META | IVF index metadata |
| 0x0311 | IVF_CENTROID | IVF centroids |
| 0x0312 | IVF_LIST | IVF posting lists |
| 0x0320 | DISKANN_META | DiskANN metadata |
| 0x0321 | DISKANN_GRAPH | DiskANN graph |
| 0x0322 | DISKANN_VECTOR_BLOCK | DiskANN vectors |
| 0x0330 | SCANN_META | ScaNN metadata |
| 0x0331 | SCANN_CENTROID | ScaNN centroids |
| 0x0332 | SCANN_PARTITION | ScaNN partitions |
| 0x0340 | CAGRA_META | CAGRA metadata |
| 0x0341 | CAGRA_NODE | CAGRA nodes |
| 0x0350 | ANNOY_META | Annoy metadata |
| 0x0351 | ANNOY_NODE | Annoy nodes |
| 0x0360 | NSG_META | NSG metadata |
| 0x0361 | NSG_NODE | NSG nodes |
| 0x0370 | VECTOR_FLAT_META | Flat vector metadata |
| 0x0371 | VECTOR_FLAT_SEGMENT | Flat vector segments |

### Emulation and Redis (0x0400-0x04FF)

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x0400 | DOC_META | Document store metadata |
| 0x0401 | DOC_DATA | Document data |
| 0x0410 | KV_META | Key-value metadata |
| 0x0411 | KV_DATA | Key-value data |
| 0x0420 | WIDE_META | Wide-column metadata |
| 0x0421 | WIDE_ROW | Wide-column rows |
| 0x0430 | GRAPH_META | Graph metadata |
| 0x0431 | GRAPH_NODE | Graph nodes |
| 0x0432 | GRAPH_EDGE | Graph edges |
| 0x0440 | VECTOR_META | Vector metadata |
| 0x0441 | VECTOR_DATA | Vector data |
| 0x0450-0x0458 | REDIS_* | Redis data types |

### vNext Multi-Model (0x2000-0x20FF)

Reserved for vNext engine multi-model support:

| Type Code | Name | Description |
|-----------|------|-------------|
| 0x2000 | DOC_COLLECTION_ROOT | Document collection |
| 0x2001 | DOC_HEAP | Document heap |
| 0x2002 | DOC_PATH_DICTIONARY | Path dictionary |
| 0x2003 | DOC_PATH_POSTINGS | Path postings |
| 0x2004 | TS_MEASUREMENT_ROOT | Time series measurement |
| 0x2005 | TS_SERIES_INDEX | Time series index |
| 0x2006 | TS_CHUNK | Time series chunks |
| 0x2007 | TS_AGG_CACHE | Aggregated cache |
| 0x2008 | COL_SEGMENT_HEADER | Column segment header |
| 0x2009 | COL_SEGMENT_DATA | Column segment data |
| 0x200A | COL_ZONE_MAP | Zone maps |
| 0x200B | COL_DICTIONARY | Column dictionaries |
| 0x200C | SEARCH_TERM_DICT | Search term dictionary |
| 0x200D | SEARCH_POSTINGS | Search postings |
| 0x200E | SEARCH_DOCVALUES | Doc values |
| 0x200F | VECTOR_GRAPH | Vector graph |
| 0x2010 | VECTOR_QUANTIZER | Vector quantizer |
| 0x2011 | VECTOR_POSTING | Vector posting |
| 0x2012 | LSM_RUN_MANIFEST | LSM run manifest |
| 0x2013 | LSM_RUN_DATA | LSM run data |
| 0x2014 | LSM_BLOOM | LSM bloom filter |
| 0x2015 | RETENTION_MANIFEST | Retention manifest |

## Page Type Validation

### Function: `isVNextPageTypeRange()`

```cpp
// From include/scratchbird/core/ondisk.h:189
inline bool isVNextPageTypeRange(uint16_t page_type) {
    return page_type >= PAGE_TYPE_VNEXT_RANGE_START &&
           page_type <= PAGE_TYPE_VNEXT_RANGE_END;
}
```

### Function: `isKnownVNextPageType()`

```cpp
// From include/scratchbird/core/ondisk.h:195
inline bool isKnownVNextPageType(uint16_t page_type) {
    switch (page_type) {
        case PAGE_TYPE_DOC_COLLECTION_ROOT:
        case PAGE_TYPE_DOC_HEAP:
        // ... (all vNext types)
            return true;
        default:
            return false;
    }
}
```

### Function: `validateVNextPageTypeKnown()`

```cpp
// From include/scratchbird/core/ondisk.h:228
inline Status validateVNextPageTypeKnown(uint16_t page_type) {
    if (!isVNextPageTypeRange(page_type)) {
        return Status::OK;
    }
    return isKnownVNextPageType(page_type) ? 
           Status::OK : Status::PAGE_CORRUPT;
}
```

### Index Page Type Check

```cpp
// From include/scratchbird/core/ondisk.h:602
inline bool isCanonicalIndexPageType(uint16_t page_type) {
    switch (page_type) {
        case PAGE_TYPE_BTREE_META:
        case PAGE_TYPE_BTREE_INTERNAL:
        case PAGE_TYPE_BTREE_LEAF:
        case PAGE_TYPE_HASH_META:
        // ... (all index types)
            return true;
        default:
            return false;
    }
}
```

## Invariants

1. **Unique Type Codes**: Each page type has unique 16-bit code
   - Verification: Static assertions in ondisk.h
   
2. **Type Ranges**: Types organized by functional category
   - Verification: Code review, range checks
   
3. **vNext Validation**: Unknown types in vNext range rejected
   - Verification: validateVNextPageTypeKnown() called on page read

## Error Handling

| Error Code | Condition | Recovery Action |
|------------|-----------|-----------------|
| `PAGE_CORRUPT` | Unknown page type in vNext range | Reject page read |
| `INVALID_ARGUMENT` | Page type validation failed | Return error |

## Related Specifications

- [Page Layout](./page_layout.md) - Physical page structure
- [Heap Format](./heap_format.md) - HEAP page layout
- [File Layout](./file_layout.md) - File organization

## Appendix

### Page Type Ranges Summary

| Range | Category |
|-------|----------|
| 0x0000-0x00FF | Core system pages |
| 0x0100-0x01FF | Index pages |
| 0x0200-0x02FF | Columnstore and LSM |
| 0x0300-0x03FF | Vector and ANN indexes |
| 0x0400-0x04FF | Emulation and Redis |
| 0x0500-0x1FFF | Reserved |
| 0x2000-0x20FF | vNext multi-model |
| 0x2100-0xFFFF | Reserved |

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | AI Agent |
