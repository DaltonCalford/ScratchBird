# Specification: BRIN Index (Block Range Index)

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/brin_index.h:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/brin_index.cpp:1`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/brin_minmax_ops.h:1`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_brin_index.cpp:1`

## Synopsis

BRIN (Block Range Index) provides lightweight indexing for very large tables with naturally ordered data. Instead of indexing individual rows, BRIN stores summary information about ranges of pages (min/max values), offering significant space savings with acceptable query performance for correlated data.

## Scope

### In Scope

- Block range summary structure
- Min/max operator classes
- Pages per range configuration
- Scan with range exclusion
- Summarization and revsummarize

### Out of Scope

- B-tree-style per-row indexing
- Unique constraints
- Full table ordering guarantees

## Background

BRIN is ideal for:
- Time-series data (naturally ordered by timestamp)
- Log tables with sequential insertion
- Large tables where B-tree would be too large

Trade-offs:
- **Size**: ~100x smaller than B-tree
- **Speed**: Sequential scan of ranges vs direct lookup
- **Accuracy**: Range-level exclusion, not row-level

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:664
enum class IndexType : uint8_t {
    BRIN = 6,         // Block Range Index
    // ... other types
};
```

### Page Types

| Page Type | Value | Description |
|-----------|-------|-------------|
| `PAGE_TYPE_BRIN_META` | 0x26 | Metadata page |
| `PAGE_TYPE_BRIN_REVMAP` | 0x27 | Reverse mapping page |
| `PAGE_TYPE_BRIN_DATA` | 0x28 | Summary data page |

### BRIN Structure

```
Table Pages:
┌────────┬────────┬────────┬────────┐
│ Range 0│ Range 1│ Range 2│ Range 3│  ... (each range = pages_per_range pages)
│ 0-127  │128-255 │256-383 │384-511 │
└────┬───┴────┬───┴────┬───┴────┬───┘
     │        │        │        │
     ▼        ▼        ▼        ▼
┌─────────┬─────────┬─────────┬─────────┐
│ Summary │ Summary │ Summary │ Summary │
│  (min,  │  (min,  │  (min,  │  (min,  │
│   max)  │   max)  │   max)  │   max)  │
└─────────┴─────────┴─────────┴─────────┘
     ▲        ▲        ▲        ▲
     │        │        │        │
┌────┴────────┴────────┴────────┴───────┐
│      Reverse Mapping (revmap)         │
│  Maps heap page ranges to summaries   │
└───────────────────────────────────────┘
```

### Meta Page Layout

```cpp
// Source: scratchbird/core/brin_index.h
struct BRINMetaPage {
    PageHeader header;              // Standard header
    ID index_uuid;                  // Index UUID
    ID table_uuid;                  // Table UUID
    uint32_t pages_per_range;       // Pages in each range (default 128)
    uint32_t revmap_page_count;     // Number of revmap pages
    uint32_t data_page_count;       // Number of data pages
    uint32_t range_count;           // Total ranges
    uint64_t last_summarized_page;  // Last heap page summarized
    uint16_t opclass_id;            // Operator class
};
```

### Reverse Mapping Entry

```cpp
// Source: scratchbird/core/brin_index.h
struct BRINRevmapEntry {
    uint32_t data_page;     // Page containing summary
    uint16_t data_offset;   // Offset within page
    uint16_t flags;         // BRIN_RANGE_* flags
};

// Flags
enum BRINRangeFlags : uint16_t {
    BRIN_RANGE_EMPTY = 0x01,      // Range has no data
    BRIN_RANGE_UNSUMMARIZED = 0x02, // Range not yet summarized
    BRIN_RANGE_COMPRESSED = 0x04   // Summary is compressed
};
```

### Summary Data Entry (Variable Size)

```cpp
// Source: scratchbird/core/brin_minmax_ops.h
struct BRINMinMaxSummary {
    // Fixed header
    uint16_t summary_size;      // Total size
    uint16_t nulls_count;       // Number of null values
    uint16_t values_count;      // Number of non-null values
    uint8_t has_nulls;          // 1 if range contains NULL
    uint8_t all_nulls;          // 1 if range is all NULL
    
    // Variable data (opclass-specific)
    // For minmax:
    uint8_t min_value[value_size];
    uint8_t max_value[value_size];
};
```

## Algorithms

### Algorithm: Create BRIN Index

```
Input:  table, column, pages_per_range (default 128)
Output: Status

1. Calculate number of ranges:
   range_count = ceil(table_pages / pages_per_range)

2. Allocate revmap pages:
   entries_per_page = page_size / sizeof(BRINRevmapEntry)
   revmap_pages = ceil(range_count / entries_per_page)

3. Initialize revmap:
   For each entry:
   - Set BRIN_RANGE_UNSUMMARIZED flag
   - data_page = 0, data_offset = 0

4. If table has data:
   - Run initial summarization

5. Return OK
```

### Algorithm: Summarize Range

```
Input:  heap page range [start_page, end_page]
Output: Summary entry

1. Initialize summary:
   - min = +infinity
   - max = -infinity
   - nulls_count = 0
   - values_count = 0

2. For each page in range:
   a. For each row in page:
      - If value IS NULL:
        nulls_count++
      - Else:
        values_count++
        min = opclass.min(min, value)
        max = opclass.max(max, value)

3. If values_count == 0:
   - Set all_nulls = true

4. Create summary entry:
   - Store min, max
   - Store nulls_count, values_count

5. Write to data page

6. Update revmap entry:
   - data_page = page_num
   - data_offset = offset
   - Clear UNSUMMARIZED flag

7. Return OK
```

### Algorithm: Search with Range Exclusion

```
Input:  query_predicate, strategy
Output: Matching TIDs (may include false positives)

1. results = []

2. For each range in revmap:
   a. If BRIN_RANGE_EMPTY or BRIN_RANGE_UNSUMMARIZED:
      - Must scan full range
      - Continue to scan

   b. Load summary from data page

   c. Check range applicability:
      - If opclass.satisfies_range(summary, query, strategy):
        * Range might contain matches
        * Scan all pages in range
        * Add matching TIDs to results
      - Else:
        * Range can be skipped (excluded)

3. Return results
```

### Algorithm: Insert (No-Op)

```
Input:  new row
Output: Status

BRIN does not update on INSERT.
Summaries are either:
- Built during CREATE INDEX
- Updated during VACUUM (autosummarize)
- Updated via brin_summarize_new_values()

This is why BRIN is so fast for inserts!
```

### Algorithm: Autosummarize

```
Input:  BRIN index, vacuum threshold
Output: Status

1. Find unsummarized ranges:
   - Scan revmap for BRIN_RANGE_UNSUMMARIZED

2. For each unsummarized range:
   a. If range now has data:
      - Call summarize_range()
   b. Else:
      - Keep UNSUMMARIZED flag

3. Check for new ranges:
   - If table has grown beyond last_summarized_page:
     * Summarize new ranges

4. Return OK
```

## Operator Classes

| Opclass | Type | Strategy | Description |
|---------|------|----------|-------------|
| brin_minmax_ops | Any ordered type | min, max | Basic range summary |
| brin_inclusion_ops | Box, Range | contains | Geometric inclusion |
| brin_bloom_ops | Any | membership | Bloom filter membership |

## Invariants

| Invariant | Description | Verification |
|-----------|-------------|--------------|
| I1 | Summary min <= all values in range | Summarize check |
| I2 | Summary max >= all values in range | Summarize check |
| I3 | Unsummarized ranges must be scanned | Query logic |
| I4 | Revmap entry points to valid summary | Consistency check |

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `brin.pages_per_range` | 128 | Pages per summary range |
| `brin.autosummarize` | on | Auto-summarize on vacuum |
| `brin.summarization_threshold` | 0.3 | Min fraction of range filled |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `SB_ERR_BRIN_UNSUMMARIZED` | Query hits unsummarized range | Scan range |
| `SB_ERR_BRIN_CORRUPT_SUMMARY` | Summary validation failed | Re-summarize |

## Test Coverage

| Test File | Coverage |
|-----------|----------|
| `test_brin_index.cpp` | Basic BRIN operations |
| `test_brin_minmax_ops.cpp` | Minmax operator class |
| `test_brin_autosummarize.cpp` | Auto-summarization |

## Related Specifications

- [index_btree.md](./index_btree.md) - Row-level indexing alternative
- [index_zonemap.md](./index_zonemap.md) - Similar summary approach

## References

- PostgreSQL BRIN documentation
- Alvarez, V. et al. (2015). A Comparison of Log-Structured Merge B-Tree and Streaming B-Tree.

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Comprehensive specification |
