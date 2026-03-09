# Specification: Zone Map Index

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

Zone Map index stores min/max statistics for contiguous ranges of rows (zones), enabling I/O elimination by skipping zones that cannot contain query results. A lightweight alternative to traditional indexes for analytical workloads.

## Scope

### In Scope

- Zone definition (row range granularity)
- Min/max value tracking per zone
- NULL count per zone
- Zone elimination during scans
- Integration with columnstore

### Out of Scope

- Per-row indexing
- Complex predicates beyond range queries
- Ordering guarantees

## Background

Zone maps are similar to BRIN indexes but:
- Typically larger zones (100K-1M rows)
- Stored separately or with data
- Often built automatically by storage layer
- No index maintenance overhead

Best for:
- Clustered/sorted data (time-series)
- Analytical queries with range predicates
- Large scans where I/O dominates

## Specification

### Index Type Identifier

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:671
enum class IndexType : uint8_t {
    ZONEMAP = 13,     // Zone map (min/max) index
    // ... other types
};
```

### Zone Map Structure

```cpp
// Zone map entry for a column zone
struct ZoneMap {
    uint64_t start_row;        // First row in zone
    uint64_t row_count;        // Rows in this zone
    
    // Min/max values (type-dependent storage)
    uint8_t min_value[64];     // Aligned for any type
    uint8_t max_value[64];
    
    // Statistics
    uint64_t null_count;       // NULL values in zone
    uint64_t distinct_count;   // Approx distinct (HyperLogLog)
    
    // Predicates this zone satisfies
    uint32_t valid_for_predicates;
};

// Zone map set for a table/column
struct ZoneMapSet {
    uint32_t zone_count;
    uint32_t rows_per_zone;    // Configurable granularity
    std::vector<ZoneMap> zones;
};
```

## Algorithms

### Algorithm: Zone Elimination

```
Input:  predicate (col > value), zone_map
Output: Zones that might contain matches

1. candidate_zones = []

2. For each zone in zone_map:
   a. If predicate is col > X:
      - If zone.max_value > X: Add to candidates
      - Else: Skip zone (cannot contain matches)
   
   b. If predicate is col < X:
      - If zone.min_value < X: Add to candidates
      - Else: Skip zone
   
   c. If predicate is col BETWEEN X AND Y:
      - If zone.max >= X AND zone.min <= Y: Add
      - Else: Skip

3. Return candidate_zones
```

### Algorithm: Build Zone Map

```
Input:  Column data
Output: Zone map

1. Determine zones: N / rows_per_zone

2. For each zone:
   a. Initialize:
      min = +inf, max = -inf
      nulls = 0
   
   b. Scan rows in zone:
      - If NULL: nulls++
      - Else: min = MIN(min, value), max = MAX(max, value)
   
   c. Store zone entry

3. Return zone map
```

## Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `zonemap.rows_per_zone` | 262144 | Rows per zone (256K) |
| `zonemap.auto_build` | true | Auto-create on load |

## Related Specifications

- [index_brin.md](./index_brin.md) - Similar block-level indexing
- [index_columnstore.md](./index_columnstore.md) - Zone maps in columnstore

## Changelog

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-08 | Initial specification |
