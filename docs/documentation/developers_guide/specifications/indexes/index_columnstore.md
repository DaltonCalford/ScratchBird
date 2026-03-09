# Specification: Columnstore Index

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/columnstore_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/columnstore.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/columnstore_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/columnstore.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_columnstore_index.cpp:1`

## Synopsis

Columnstore index organizes data in column-major format, enabling high compression ratios and efficient analytical query processing. Optimized for OLAP workloads with aggregation, filtering, and scan-heavy operations on large datasets.

## Scope

### In Scope

- Column-major page organization
- Rowgroup and segment structure
- Dictionary encoding for strings
- Run-length encoding (RLE)
- Bitpacking for integers
- Column-wise compression codecs
- Batch mode execution
- Zone maps (min/max per segment)

### Out of Scope

- Row-oriented updates (requires delta store)
- Point lookups (use B-tree)
- Single-row inserts (bulk load only)

## Background

Columnstore benefits:
- **Compression**: 5-10x better than rowstore (similar values adjacent)
- **Vectorized execution**: Process values in SIMD batches
- **Projection pushdown**: Read only needed columns
- **Better cache utilization**: Sequential column access

Organization:
- **Rowgroup**: ~1M rows (unit of compression)
- **Segment**: Column data within rowgroup
- **Dictionary**: String encoding lookup table
- **Zone map**: Min/max per segment for pruning

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:668
enum class IndexType : uint8_t {
    COLUMNSTORE = 10, // Columnstore index
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_COLSTORE_META` | 0x32 | Metadata page |
| `PAGE_TYPE_COLSTORE_ROWGROUP` | 0x33 | Rowgroup directory |
| `PAGE_TYPE_COLSTORE_SEGMENT` | 0x34 | Column segment data |
| `PAGE_TYPE_COLSTORE_DICTIONARY` | 0x35 | String dictionary |

### Columnstore Structure

```
Table organized by rowgroups:
┌─────────────────────────────────────────────────────┐
│ Rowgroup 0 (rows 0-999,999)                         │
│ ┌──────────┬──────────┬──────────┬──────────┐      │
│ │Col 0 Seg │Col 1 Seg │Col 2 Seg │Col 3 Seg │      │
│ │(Int)     │(String)  │(Date)    │(Decimal) │      │
│ │[encoded] │[dict idx]│[RLE]     │[bitpack] │      │
│ └──────────┴──────────┴──────────┴──────────┘      │
├─────────────────────────────────────────────────────┤
│ Rowgroup 1 (rows 1,000,000-1,999,999)               │
│ ┌──────────┬──────────┬──────────┬──────────┐      │
│ │Col 0 Seg │Col 1 Seg │Col 2 Seg │Col 3 Seg │      │
│ └──────────┴──────────┴──────────┴──────────┘      │
└─────────────────────────────────────────────────────┘
```

### Meta Page Layout

```cpp
// Source: scratchbird/core/columnstore.h
struct ColumnstoreMetaPage {
    PageHeader header;
    ID index_uuid;
    ID table_uuid;
    
    uint32_t rowgroup_count;
    uint32_t rowgroup_size;        // Rows per rowgroup (default 1M)
    uint32_t columns_count;
    
    // Rowgroup directory (root of B-tree or array)
    uint32_t rowgroup_dir_root;
    
    // Compression settings per column
    struct ColumnInfo {
        uint16_t column_id;
        uint16_t data_type;
        uint8_t encoding;          // DICTIONARY, RLE, BITPACK, NONE
        uint8_t codec;             // LZ4, ZSTD, NONE
    } column_info[];
};
```

### Rowgroup Structure

```cpp
// Source: scratchbird/core/columnstore.h
struct Rowgroup {
    uint64_t start_row_id;         // First row in rowgroup
    uint32_t row_count;            // Actual rows (may be < rowgroup_size)
    
    // Segment pointers for each column
    struct SegmentPointer {
        uint32_t page_count;       // Number of pages for segment
        uint32_t first_page;       // Starting page
        uint32_t compressed_size;  // Bytes after compression
        uint32_t uncompressed_size;// Bytes before compression
    } segments[];
    
    // Zone maps for segment elimination
    struct ZoneMap {
        uint8_t min_value[64];     // Min value (type-dependent size)
        uint8_t max_value[64];     // Max value
        uint64_t null_count;       // NULL values in segment
    } zone_maps[];
};
```

### Segment Encoding Formats

**Dictionary Encoding (for strings/low cardinality):**
```cpp
struct DictionarySegment {
    uint32_t dictionary_id;        // Reference to dictionary page
    uint32_t distinct_count;       // Number of distinct values
    uint8_t bits_per_value;        // log2(distinct_count) rounded up
    
    // Data: packed array of dictionary indices
    // e.g., 12-bit values packed into bytes
    uint8_t packed_indices[];
};
```

**RLE Encoding (for sorted/repetitive data):**
```cpp
struct RLESegment {
    uint32_t run_count;
    struct Run {
        uint32_t count;            // Run length
        uint8_t value[];           // Value bytes (type-dependent)
    } runs[];
};
```

**Bitpacking (for integers):**
```cpp
struct BitpackedSegment {
    uint8_t bits_per_value;        // Based on value range
    uint64_t value_count;
    
    // Data: tightly packed values
    // min_value is subtracted before packing
    uint8_t packed_data[];
};
```

## Algorithms

### Algorithm: Bulk Load

```
Input:  Row data in batches
Output: Populated columnstore

1. Initialize new rowgroup

2. For each batch of rows:
   a. For each column:
      - Extract column values from rows
      - Collect into column buffer
   
   b. If rowgroup full or input exhausted:
      i.   For each column buffer:
           - Determine best encoding
           - Sort if beneficial for RLE
           - Encode and compress
           - Write segment pages
      
      ii.  Build zone maps from segments
      
      iii. Write rowgroup metadata
      
      iv.  Start new rowgroup

3. Finalize metadata and write to catalog
```

### Algorithm: Scan with Predicates

```
Input:  Column projections, WHERE predicates
Output: Matching rows

1. Identify candidate rowgroups using zone maps:
   For each predicate on indexed column:
   - Check zone map min/max
   - If predicate outside range: skip rowgroup

2. For each candidate rowgroup:
   a. Load required column segments
   
   b. Decompress segments to vector batches
   
   c. Apply predicates using vectorized operations:
      - SIMD comparisons
      - Bitmap result generation
   
   d. Project selected columns for qualifying rows

3. Return results
```

### Algorithm: Encode Segment (Dictionary)

```
Input:  Array of string values
Output: Dictionary-encoded segment

1. Build dictionary:
   a. Collect unique values
   b. Sort by frequency (most frequent first)
   c. Assign indices 0, 1, 2, ...

2. Calculate bits_per_value = ceil(log2(dict_size))

3. Encode values:
   For each value:
   - Look up dictionary index
   - Pack index into output (bits_per_value bits each)

4. Write dictionary to dictionary page

5. Return segment with packed indices
```

### Algorithm: Encode Segment (RLE)

```
Input:  Array of values
Output: RLE-encoded segment

1. Sort input values (if order doesn't matter)
   This maximizes run lengths

2. Generate runs:
   runs = []
   current = values[0]
   count = 1
   
   For i = 1 to values.length:
     If values[i] == current:
       count++
     Else:
       runs.append((count, current))
       current = values[i]
       count = 1
   
   runs.append((count, current))

3. If runs smaller than raw: use RLE
   Else: use bitpacking or raw

4. Return encoded segment
```

### Algorithm: Decode Segment (Vectorized)

```
Input:  Compressed segment
Output: Decompressed value array

1. Determine encoding from segment header

2. Dispatch to decoder:
   - Dictionary: Lookup indices in dictionary
   - RLE: Expand runs
   - Bitpack: Unpack values, add min_value offset

3. Return decompressed array for vectorized processing
```

## Compression Comparison

| Encoding | Best For | Compression Ratio |
|----------|----------|-------------------|
| Dictionary | Low cardinality strings | 10-20x |
| RLE | Sorted/repetitive data | 20-100x |
| Bitpack | Integer ranges | 2-4x |
| None | High cardinality unique | 1x |

## Zone Map Pruning

```
Example: Query WHERE date >= '2024-01-01' AND date < '2024-02-01'

Rowgroup 0: zone_map = [2023-06-01, 2023-12-31]
  -> Pruned (entirely before range)

Rowgroup 1: zone_map = [2023-12-15, 2024-01-20]
  -> Scan (overlaps range)

Rowgroup 2: zone_map = [2024-01-15, 2024-03-01]
  -> Scan (overlaps range)

Rowgroup 3: zone_map = [2024-03-01, 2024-06-01]
  -> Pruned (entirely after range)
```

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Row count matches sum of rowgroup counts | Consistency check |
| I2 | Segments in rowgroup have same row count | Load validation |
| I3 | Zone map min <= all values <= max | Build check |
| I4 | Dictionary indices fit in bits_per_value | Encode validation |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `columnstore.rowgroup_size` | 1048576 | Rows per rowgroup (1M) |
| `columnstore.encoding_threshold` | 1000 | Distinct values for dictionary |
| `columnstore.codec` | ZSTD | Compression codec |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_columnstore_index.cpp` | Columnstore operations |
| `test_columnstore_encoding.cpp` | Encoding/decoding |
| `test_columnstore_scan.cpp` | Vectorized scans |

## Related Specifications

- [index_zonemap.md](./index_zonemap.md) - Zone map details
- [index_btree.md](./index_btree.md) - Rowstore alternative

## References

- Microsoft SQL Server Columnstore documentation
- Abadi, D. et al. (2013). The Vertica Analytic Database.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
