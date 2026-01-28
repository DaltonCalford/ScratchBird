# Z-Order (Morton) Index Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
**Features:** Page-size agnostic, MGA compliant, B-tree backed

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Data Model and Encoding](#data-model-and-encoding)
4. [On-Disk Structures](#on-disk-structures)
5. [Page Size Considerations](#page-size-considerations)
6. [MGA Compliance](#mga-compliance)
7. [Core API](#core-api)
8. [DML Integration](#dml-integration)
9. [Garbage Collection](#garbage-collection)
10. [Query Planner Integration](#query-planner-integration)
11. [DDL and Catalog](#ddl-and-catalog)
12. [Implementation Steps](#implementation-steps)
13. [Testing Requirements](#testing-requirements)
14. [Performance Targets](#performance-targets)
15. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

A Z-order (Morton) index maps multi-dimensional values (2D or 3D) into a single sortable key by interleaving bits. This preserves spatial locality well enough to use a standard B-tree or LSM tree for bounding-box queries and nearby searches.

### Primary Use Cases

- Geospatial point searches (lat, lon)
- Bounding-box filters on 2D/3D coordinates
- Approximate nearest-neighbor candidate retrieval
- Multi-dimensional telemetry (time, x, y) when fast pruning is needed

### Key Characteristics

- Index key is 64-bit or 128-bit Morton code derived from normalized coordinates
- Uses existing B-tree infrastructure (no new leaf layout required)
- Bounding-box queries convert into a set of Morton key ranges
- Requires post-filtering with exact predicates

---

## Architecture Decision

### Design Choice

Implement as a **computed-key B-tree index** with a Z-order encoding layer.

- Leverages existing B-tree implementation and tooling
- Allows index-only scans where computed key and TID are sufficient
- All spatial logic happens in the planner and key encoder

### Supported Dimensions

- 2D (default)
- 3D (optional)

### Supported Input Types

- INTEGER, BIGINT
- DECIMAL, DOUBLE
- GEOMETRY(POINT) and GEOMETRY(MULTIPOINT) using centroid extraction

---

## Data Model and Encoding

### Normalization

Each dimension is normalized to an unsigned integer range.

```
normalized = clamp((value - min) / (max - min), 0.0, 1.0)
quantized = floor(normalized * ((1 << bits_per_dim) - 1))
```

### Morton Interleaving (2D)

```
uint64 morton_encode_2d(uint32 x, uint32 y) {
    uint64 interleaved = 0;
    for (int i = 0; i < 32; i++) {
        interleaved |= ((uint64)((x >> i) & 1) << (2 * i));
        interleaved |= ((uint64)((y >> i) & 1) << (2 * i + 1));
    }
    return interleaved;
}
```

### Morton Interleaving (3D)

For 3D, the key uses 96 or 128 bits (three bit lanes). Store as two uint64 values.

```
struct Morton128 {
    uint64 hi;
    uint64 lo;
};
```

### Bounding-Box to Morton Ranges

Bounding boxes are decomposed into a minimal set of Z-order ranges using recursive quad/oct decomposition. This set is used as an index range predicate. Exact spatial predicates are applied as a post-filter.

---

## On-Disk Structures

### Meta Page

```cpp
#pragma pack(push, 1)

struct SBZOrderIndexMetaPage {
    PageHeader zo_header;            // Standard page header (64 bytes)

    // Identity
    uint8_t zo_index_uuid[16];       // Index UUID
    uint8_t zo_table_uuid[16];       // Table UUID

    // Dimensions
    uint8_t zo_dimensions;           // 2 or 3
    uint8_t zo_bits_per_dim;         // 16..32
    uint8_t zo_key_bytes;            // 8 or 16
    uint8_t zo_reserved1;

    // Column mapping
    uint16_t zo_column_ids[3];       // Column IDs per dimension
    uint16_t zo_column_count;        // 2 or 3

    // Normalization bounds (double-precision)
    double zo_min_values[3];         // Min per dimension
    double zo_max_values[3];         // Max per dimension

    // Root page
    uint32_t zo_root_page;           // B-tree root page
    uint32_t zo_reserved2;

    // Statistics
    uint64_t zo_total_keys;
    uint64_t zo_total_ranges;
    uint64_t zo_total_scans;
    uint64_t zo_reserved3;

    // Options
    uint32_t zo_range_cover_max;     // Max ranges for planner
    uint32_t zo_range_cover_gran;    // Granularity for cover

    uint8_t zo_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Key Layout

```
ZOrderKey {
    uint8_t morton_key[8 or 16];
    TID tid;                      // TID tie-breaker
}
```

Keys are stored in the existing B-tree leaf format with Morton key as the primary sort key.

---

## Page Size Considerations

- 8K pages support both 64-bit and 128-bit keys.
- 128-bit keys reduce fan-out; use 2D unless 3D is required.
- Recommend 32 bits per dimension for lat/lon precision (approx 1e-7).

---

## MGA Compliance

- Index entries are versioned like other B-tree entries.
- Deletes create tombstones tied to transaction ID.
- Visibility checks use standard MGA rules (TIP-based visibility).

---

## Core API

```cpp
// Encoding
Morton128 zorder_encode(const double* values, const SBZOrderIndexMetaPage* meta);
void zorder_decode(const Morton128* key, const SBZOrderIndexMetaPage* meta, double* out_values);

// Range cover
size_t zorder_cover_bbox(const SBZOrderIndexMetaPage* meta,
                         const double* min_values,
                         const double* max_values,
                         MortonRange* out_ranges,
                         size_t max_ranges);

// Index operations
Status zorder_insert(UUID index_uuid, const Morton128* key, TID tid);
Status zorder_delete(UUID index_uuid, const Morton128* key, TID tid);
IndexScan zorder_range_scan(UUID index_uuid, const MortonRange* ranges, size_t count);
```

---

## DML Integration

- INSERT: compute Morton key from coordinate columns and insert into B-tree
- UPDATE: if any indexed dimension changes, delete old key and insert new key
- DELETE: insert tombstone for key + TID

---

## Garbage Collection

Z-order indexes are implemented as computed-key B-tree indexes and use the
standard GC contract:

TID references use `TID { gpid, slot }`. Legacy packed TID encodings are
not permitted in v2 on-disk formats.

- `removeDeadEntries()` scans leaf pages and removes entries whose TID
  is dead.
- If tombstones are used, GC drops tombstones with `xmax < OIT`.
- Leaf-level cleanup may trigger page merge/rebalance following the
  B-tree GC rules.

See `INDEX_GC_PROTOCOL.md` for the shared behavior.

---

## Query Planner Integration

### Predicate Support

- `WHERE x BETWEEN a AND b AND y BETWEEN c AND d`
- `WHERE ST_Within(point, bbox)` for GEOMETRY types

### Planner Behavior

1. Convert bounding box into Morton ranges.
2. Choose Z-order index if range count is below `range_cover_max`.
3. Perform range scans and post-filter with exact predicate.

### Fallback

If range decomposition exceeds threshold, planner falls back to BRIN/zone map or heap scan.

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_geo_zorder
ON places(lat, lon)
USING zorder
WITH (
    dims = 2,
    bits_per_dim = 32,
    min = '[-90,-180]',
    max = '[90,180]',
    range_cover_max = 512,
    range_cover_gran = 4
);
```

### Catalog Additions

- `sys.indexes.index_type = 'ZORDER'`
- `sys.indexes.index_options` stores normalized bounds, bits, dims, cover settings
- `sys.index_stats` adds: `range_cover_calls`, `avg_ranges_per_query`

---

## Implementation Steps

1. Add Z-order key encoder and range cover algorithm.
2. Extend index factory with `ZORDER` type.
3. Wire planner rule for bounding-box predicates.
4. Add index option parsing and catalog persistence.
5. Add visibility-aware scans and post-filter path.

---

## Testing Requirements

- Deterministic encoding/decoding tests for 2D and 3D
- Range cover correctness vs brute-force scanning
- Planner selection tests (range count threshold)
- MGA visibility tests under concurrent updates

---

## Performance Targets

- Encode 1M points/sec per core
- Range cover < 100 microseconds for typical bbox
- Range scan with post-filter 5-20x faster than heap for selective bbox

---

## Future Enhancements

- Hilbert curve option for improved locality
- Optional KNN search using iterative range expansion
- Hybrid with BRIN/zone maps for large partitions
