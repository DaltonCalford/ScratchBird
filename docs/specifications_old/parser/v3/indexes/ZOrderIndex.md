# Z-Order (Morton) Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)



Status: Authoritative (V3)
Last Updated: 2026-02-08
Features: Page-size agnostic, MGA compliant, B-tree backed

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
15. [Reserved Enhancements (Not Supported in V3)](#reserved-enhancements-not-supported-in-v3)

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

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Encoding (Morton / Z‑Order)

1. Normalize each dimension to `[0, 2^b - 1]` (fixed `b` bits per dimension).  
2. Interleave bits from each dimension to form the Morton code.  
3. Store Morton code in B‑tree as the computed key.  

### Query (Bounding Box)

1. Convert the query box into a set of Z‑order ranges using recursive partitioning.  
2. For each range `[zmin, zmax]`, perform B‑tree range scan.  
3. Recheck original coordinates to remove false positives.  

### Updates / Deletes

1. Insert computed Morton code with `record_uuid`.  
2. Delete by computed key + `record_uuid`.  

### MGA / Versioning

- Standard index versioning rules apply.  

### Complexity Targets

- Insert: `O(log N)`  
- Range: `O(k log N + candidates)` where `k` is number of Z‑ranges.  

### References (for algorithmic definitions)

- Morton order / Z‑order curve (bit interleaving).  

---

## Architecture Decision

### Design Choice

Implement as a **computed-key B-tree index** with a Z-order encoding layer.

- Leverages existing B-tree implementation and tooling
- Allows index-only scans where computed key and `record_uuid` are sufficient
- All spatial logic happens in the planner and key encoder

### Supported Dimensions

- 2D (default)
- 3D support is not supported in V3; MUST reject with `ERR_FEATURE_DISABLED`

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


**Logical Fields:**

- `hi` (uint64)
- `lo` (uint64)


### Bounding-Box to Morton Ranges

Bounding boxes are decomposed into a minimal set of Z-order ranges using recursive quad/oct decomposition. This set is used as an index range predicate. Exact spatial predicates are applied as a post-filter.

---

## On-Disk Structures

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.

### Meta Page


**Logical Fields:**

- `zo_header` (PageHeader): Page header (see `../storage/PAGE_TYPES_AND_LAYOUTS.md`)
- `zo_index_uuid[16]` (uint8_t): Index UUID
- `zo_table_uuid[16]` (uint8_t): Table UUID
- `zo_dimensions` (uint8_t): 2 or 3
- `zo_bits_per_dim` (uint8_t): 16..32
- `zo_key_bytes` (uint8_t): 8 or 16
- `zo_reserved1` (uint8_t)
- `zo_column_ids[3]` (uint16_t): Column IDs per dimension
- `zo_column_count` (uint16_t): 2 or 3
- `zo_min_values[3]` (double): Min per dimension
- `zo_max_values[3]` (double): Max per dimension
- `zo_root_page` (uint32_t): B-tree root page
- `zo_reserved2` (uint32_t)
- `zo_total_keys` (uint64_t)
- `zo_total_ranges` (uint64_t)
- `zo_total_scans` (uint64_t)
- `zo_reserved3` (uint64_t)
- `zo_range_cover_max` (uint32_t): Max ranges for planner
- `zo_range_cover_gran` (uint32_t): Granularity for cover
- `zo_padding[]` (uint8_t)


### Key Layout

```
ZOrderKey {
    uint8_t morton_key[8 or 16];
    UUID record_uuid;             // Record identity tie-breaker
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
Status zorder_insert(UUID index_uuid, const Morton128* key, UUID record_uuid);
Status zorder_delete(UUID index_uuid, const Morton128* key, UUID record_uuid);
IndexScan zorder_range_scan(UUID index_uuid, const MortonRange* ranges, size_t count);
```

---

## DML Integration

- INSERT: compute Morton key from coordinate columns and insert into B-tree
- UPDATE: if any indexed dimension changes, delete old key and insert new key
- DELETE: insert tombstone for key + record_uuid

---

## Garbage Collection

Z-order indexes are implemented as computed-key B-tree indexes and use the
standard GC contract:

Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
not permitted in v2 on-disk formats.

- `removeDeadEntries()` scans leaf pages and removes entries whose `record_uuid`
  is dead.
- If tombstones are used, GC drops tombstones for dead record versions.
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

## Reserved Enhancements (Not Supported in V3)

- Hilbert curve option for improved locality
- optional KNN search using iterative range expansion
- Hybrid with BRIN/zone maps for large partitions

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
