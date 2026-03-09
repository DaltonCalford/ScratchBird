# Specification: Bitmap Index

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/bitmap_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/bitmap_index.cpp:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_bitmap_index_gc.cpp:1`

## Synopsis

Bitmap index stores a bitmap for each distinct value, where each bit represents whether a row contains that value. Optimized for low-cardinality columns (boolean, status codes, gender) with efficient bitwise AND/OR operations for complex queries.

## Scope

### In Scope

- B-tree of values to bitmaps
- RLE-compressed bitmap segments
- Bitwise operations (AND, OR, NOT, XOR)
- Row ID to bit position mapping
- Bitmap segment page layout

### Out of Scope

- High-cardinality columns (use B-tree)
- Unique column indexing
- Range queries without value lookup

## Background

Bitmap indexes excel for:
- Columns with few distinct values (< 1000)
- Data warehousing with star schemas
- Complex boolean queries combining multiple conditions
- Read-heavy workloads with occasional bulk loads

Storage benefits:
- 1 bit per row per distinct value (vs ~20 bytes for B-tree)
- Compression via RLE (run-length encoding)
- Efficient bitwise operations for multi-condition queries

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:667
enum class IndexType : uint8_t {
    BITMAP = 9,       // Bitmap index
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_BITMAP_META` | 0x2F | Metadata page |
| `PAGE_TYPE_BITMAP_VALUE` | 0x30 | Value tree page (B-tree) |
| `PAGE_TYPE_BITMAP_SEGMENT` | 0x31 | Bitmap segment page |

### Bitmap Index Structure

```
┌─────────────────────────────────────────┐
│ Value B-Tree (distinct values)          │
│   "Active" -> Bitmap #1                 │
│   "Inactive" -> Bitmap #2               │
│   "Pending" -> Bitmap #3                │
└─────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────┐
│ Bitmap #1: "Active"                     │
│ Seg 0: Row 0-7999   [RLE compressed]    │
│ Seg 1: Row 8000-15999 [RLE compressed]  │
│ ...                                     │
└─────────────────────────────────────────┘
```

### Meta Page Layout

```cpp
// Source: scratchbird/core/bitmap_index.h
struct BitmapMetaPage {
    PageHeader header;
    ID index_uuid;
    ID table_uuid;
    uint32_t value_tree_root;     // Root of value B-tree
    uint64_t rows_per_segment;    // Rows per bitmap segment
    uint64_t total_rows;          // Total rows indexed
    uint32_t distinct_values;     // Number of distinct values
    uint16_t compression_type;    // RLE, Roaring, etc.
};
```

### Value Entry Structure

```cpp
// Source: scratchbird/core/bitmap_index.h
struct BitmapValueEntry {
    // Key: actual value bytes (type-specific)
    uint16_t value_size;
    uint8_t value[value_size];
    
    // Pointer to bitmap segments
    uint32_t first_segment_page;
    uint64_t segment_count;
    uint64_t bit_count;           // Number of set bits (cached)
};
```

### Bitmap Segment Structure

```cpp
// Source: scratchbird/core/bitmap_index.h
struct BitmapSegment {
    uint64_t start_row;           // First row ID in this segment
    uint64_t row_count;           // Number of rows covered
    uint32_t compressed_size;     // Size after compression
    uint32_t uncompressed_size;   // Size before compression
    
    // RLE-compressed data follows
    // Format: [count_1][value_1][count_2][value_2]...
    // value is 0 or 1 (single bit, packed)
    uint8_t rle_data[compressed_size];
};
```

### RLE Compression

```cpp
// Simple RLE for bitmaps
// Input:  [1,1,1,1,0,0,1,1,1,0,0,0,0,0]
// Output: [(4,1), (2,0), (3,1), (5,0)]

struct RLEPair {
    uint32_t count;      // Run length (varint encoded)
    uint8_t value;       // 0 or 1
};

// Optimized encoding:
// - Use bit packing for small counts
// - Separate 0-runs and 1-runs for better compression
// - Store as: [run_count][0-run-1][1-run-1][0-run-2]...
```

## Algorithms

### Algorithm: Insert

```
Input:  value, row_id, current_xid
Output: Status

1. Compute segment number: segment = row_id / rows_per_segment
   Compute bit position: bit = row_id % rows_per_segment

2. Search value B-tree for value:
   a. If value exists:
      - Get value entry with segment list
   
   b. If value not exists:
      - Create new value entry
      - Initialize empty segment list
      - Insert into B-tree

3. Load or create segment page:
   a. If segment exists:
      - Decompress RLE data
      - Set bit at position
      - Recompress
   
   b. If segment not exists:
      - Create new segment with all zeros
      - Set bit at position
      - Add to segment list

4. Update bit_count in value entry

5. Return OK
```

### Algorithm: Search (Single Value)

```
Input:  value
Output: Row IDs with matching value

1. Search value B-tree for value

2. If not found: Return empty result

3. results = []

4. For each segment in value's segment list:
   a. Decompress RLE data
   
   b. Scan runs:
      - For each 1-run:
        * Add row IDs (start_row + offset) to results

5. Return results
```

### Algorithm: Boolean AND

```
Input:  bitmap1 (value A), bitmap2 (value B)
Output: Row IDs where A AND B

1. results = []

2. Align segments by start_row

3. For each pair of aligned segments:
   a. Decompress both RLE streams
   
   b. Perform AND:
      - Walk both runs simultaneously
      - Output 1 only where both inputs are 1
   
   c. Collect row IDs from result runs

4. Return results

Optimization: If one value has few set bits,
iterate only those bits and check other bitmap.
```

### Algorithm: Boolean OR

```
Input:  bitmap1 (value A), bitmap2 (value B)
Output: Row IDs where A OR B

1. Similar to AND, but output 1 where either input is 1

2. Performance: Comparable to single bitmap scan
```

### Algorithm: Compress Segment (RLE)

```
Input:  Uncompressed bitmap (bit array)
Output: RLE-compressed data

1. output = []
2. current_value = bitmap[0]
3. count = 1

4. For i = 1 to bitmap.length:
   a. If bitmap[i] == current_value:
      - count++
   b. Else:
      - output.append((count, current_value))
      - current_value = bitmap[i]
      - count = 1

5. Append final run

6. Encode using varints for compact storage
```

### Algorithm: Decompress Segment

```
Input:  RLE-compressed data
Output: Uncompressed bitmap

1. bitmap = boolean array of size rows_per_segment
2. pos = 0

3. While input not exhausted:
   a. (count, value) = decode next RLE pair
   b. Fill bitmap[pos..pos+count-1] = value
   c. pos += count

4. Return bitmap
```

### Algorithm: GC (Remove Deleted Rows)

```
Input:  Set of deleted row IDs, OIT
Output: Compacted bitmaps

1. For each value in B-tree:
   
   a. For each segment:
      - Decompress RLE
      - Clear bits for deleted rows
      - Recompress
      
   b. If segment all zeros:
      - Remove segment
      - Update segment list
      
   c. Update bit_count

2. If value has zero bits:
   - Remove value entry from B-tree

3. Return count of rows removed
```

## Bitwise Operations Table

| Operation | Description | Complexity |
|-----------|-------------|------------|
| AND | Rows matching both values | O(min(bits_A, bits_B)) |
| OR | Rows matching either value | O(bits_A + bits_B) |
| NOT | Rows NOT matching value | O(total_rows) |
| XOR | Rows matching exactly one | O(bits_A + bits_B) |

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Each row appears in exactly one value's bitmap | Insert check |
| I2 | Segment row ranges don't overlap | Layout check |
| I3 | Bit count matches actual set bits | Consistency check |
| I4 | RLE decodes to correct length | Decode validation |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `bitmap.rows_per_segment` | 8000 | Rows per compressed segment |
| `bitmap.compression` | RLE | Compression algorithm |
| `bitmap.max_distinct` | 1000 | Cardinality limit warning |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_BITMAP_CARDINALITY` | Too many distinct values | Use B-tree instead |
| `SB_ERR_BITMAP_OVERFLOW` | Row ID exceeds capacity | Expand segments |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_bitmap_index_gc.cpp` | Bitmap operations and GC |
| `test_bitmap_rle.cpp` | RLE compression |

## Related Specifications

- [index_btree.md](./index_btree.md) - Alternative for high cardinality
- [gin_index_format.md](./gin_index_format.md) - For multi-value entries

## References

- Oracle Bitmap Index documentation
- O'Neil, P. (1987). Model 204 Architecture and Performance.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
