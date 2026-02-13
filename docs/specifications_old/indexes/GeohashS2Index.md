# Geohash / S2 Cell Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)



**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Core Index Type)
**Features:** Page-size agnostic, MGA compliant, B-tree backed

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Encoding Model](#encoding-model)
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

Geohash and S2 are hierarchical spatial indexing schemes that divide the globe into cells. They map lat/lon to a discrete cell ID, which can be stored in a B-tree for fast coverage queries.

### Primary Use Cases

- Radius search and "find nearby" queries
- Bounding-box filtering with hierarchical pruning
- Fast tile-based retrieval for maps and location search

### Key Characteristics

- Hierarchical representation supports prefix searches
- Can cover regions with a compact set of cells
- Works with B-tree or ART indexes

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Geohash Encoding

1. Normalize latitude in `[-90, 90]` and longitude in `[-180, 180]`.  
2. Interleave lat/lon bits (lon first) to produce a binary string.  
3. Group into 5‑bit chunks and map to base32 alphabet.  
4. Precision `p` controls depth (cell size); higher `p` = smaller cells.  
5. For B‑tree storage, pack the base32 string into a 64‑bit integer (fixed length).

### S2 Cell ID Encoding

1. Project `(lat, lon)` onto a unit cube face (S2 uses six cube faces).  
2. Convert to `(u,v)` coordinates, then to `(s,t)` and `(i,j)` grid coordinates.  
3. Encode `(face, i, j)` via a Hilbert curve to produce a 64‑bit `CellId`.  
4. Cell level (0..30) is encoded in the ID; lower level = larger cell.  

### Covering (Bounding Box / Polygon)

1. Compute a region covering with a maximum cell count `covering_max_cells`.  
2. Return the set of cell IDs or ranges to scan.  
3. Use prefix/range scans on the B‑tree to retrieve candidates.  

### Query

1. Convert geometry predicate into a covering set.  
2. Lookup candidate rows via `cell_id IN (...)` or range scans.  
3. Recheck exact geometry in the executor to remove false positives.  

### MGA / Versioning

- Index entries are versioned like any other index (no special rules).  

### Complexity Targets

- Encoding: `O(precision)`.  
- Query: `O(covering_cells + candidate_rows)`.  

### References (for algorithmic definitions)

- Geohash specification (bit interleaving, base32).  
- S2 Geometry library documentation (cell id / Hilbert curve encoding).  

---

## Architecture Decision

### Modes

- **GEOHASH**: base32 string or 64-bit packed value (5-bit chunks)
- **S2**: 64-bit cell ID (Hilbert curve on a cube)

### Design Choice

Implement as a **cell-ID B-tree index** with a covering algorithm that converts bounding boxes or polygons into a list of cell IDs or ID ranges.

---

## Encoding Model

### Geohash Encoding

- Convert lat/lon to base32 geohash string
- Store as 64-bit packed value for fixed-length keys
- Precision is configured as `geohash_precision` (1-12 chars)

```
char* geohash_encode(double lat, double lon, int precision);
uint64 geohash_pack(const char* geohash, int precision);
```

### S2 Encoding

- Convert lat/lon to S2 cell ID (uint64)
- Cell ID already encodes level (0..30)

```
uint64 s2_cell_id(double lat, double lon, int level);
```

### Covering Strategy

- Bounding boxes and polygons are decomposed into a covering set of cell IDs
- `covering_max_cells` limits expansion
- Planner can use either `IN (cell_id list)` or range scans on sorted IDs

---

## On-Disk Structures

### Meta Page


**Logical Fields:**

- `geo_header` (PageHeader)
- `geo_index_uuid[16]` (uint8_t)
- `geo_table_uuid[16]` (uint8_t)
- `geo_mode` (uint8_t): SBGeoMode
- `geo_precision` (uint8_t): Geohash chars or S2 level
- `geo_key_bytes` (uint8_t): 8 for packed geohash or S2
- `geo_reserved1` (uint8_t)
- `geo_lat_col_id` (uint16_t): Latitude column
- `geo_lon_col_id` (uint16_t): Longitude column
- `geo_root_page` (uint32_t): B-tree root
- `geo_reserved2` (uint32_t)
- `geo_covering_max` (uint32_t): Max covering cells
- `geo_covering_level` (uint32_t): Max cover level expansion
- `geo_total_keys` (uint64_t)
- `geo_total_covers` (uint64_t)
- `geo_reserved3` (uint64_t)
- `geo_padding[]` (uint8_t)


### Key Layout

```
GeoKey {
    uint64 cell_id;    // Packed geohash or S2 cell ID
    UUID record_uuid;  // Record identity tie-breaker
}
```

---

## Page Size Considerations

- 8-byte keys allow high fan-out
- Geohash precision up to 12 chars fits in 60 bits (base32)
- S2 cell ID uses full 64 bits

---

## MGA Compliance

- Versioned entries with standard MGA visibility
- Deletes create tombstones to be swept by GC

---

## Core API

```cpp
uint64 geo_encode_geohash(double lat, double lon, int precision);
uint64 geo_encode_s2(double lat, double lon, int level);

size_t geo_cover_bbox(const SBGeoIndexMetaPage* meta,
                      const double* min_values,
                      const double* max_values,
                      uint64* out_cell_ids,
                      size_t max_cells);

Status geo_insert(UUID index_uuid, uint64 cell_id, UUID record_uuid);
Status geo_delete(UUID index_uuid, uint64 cell_id, UUID record_uuid);
IndexScan geo_scan_cells(UUID index_uuid, const uint64* cells, size_t count);
```

---

## DML Integration

- INSERT: compute cell ID and insert
- UPDATE: recompute on lat/lon change
- DELETE: tombstone entry

---

## Garbage Collection

Geohash/S2 indexes implement `IndexGCInterface`:

- `removeDeadEntries()` scans cell posting lists and removes dead record UUIDs.
- Tombstones for deleted record versions are removed during GC.
- Empty cell lists are dropped to keep the index compact.

Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
not permitted in v2 on-disk formats.

See `INDEX_GC_PROTOCOL.md` for the shared GC contract.

---

## Query Planner Integration

- For `WHERE ST_Within(point, polygon)` or bbox filters, compute covering cells.
- Choose index if covering cell count < `geo_covering_max`.
- Apply exact geometry predicate as post-filter.

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_places_geohash
ON places(lat, lon)
USING geohash
WITH (precision = 8, covering_max = 1024);

CREATE INDEX idx_places_s2
ON places(lat, lon)
USING s2
WITH (level = 14, covering_max = 512);
```

### Catalog Additions

- `sys.indexes.index_type = 'GEOHASH'` or `'S2'`
- `sys.indexes.index_options` stores precision/level and covering settings

---

## Implementation Steps

1. Implement geohash and S2 encoding helpers.
2. Add covering algorithm for bbox and polygon.
3. Extend index factory for `GEOHASH` and `S2`.
4. Add planner hooks and post-filtering.
5. Add catalog persistence and stats.

---

## Testing Requirements

- Encode/decode round-trips for geohash and S2
- Covering correctness for bbox and polygon
- Planner selection thresholds
- MGA visibility under concurrent updates

---

## Performance Targets

- 1M encodes/sec per core
- Covering under 1 ms for typical queries
- 10x pruning vs full scan on selective regions

---

## Future Enhancements

- Support S2 region covering for complex polygons
- Optional ART backend for in-memory geohash
- Hybrid with Z-order index for large partitions

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
