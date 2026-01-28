# Geohash / S2 Cell Index Specification for ScratchBird

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

```cpp
#pragma pack(push, 1)

enum SBGeoMode : uint8_t {
    GEO_MODE_GEOHASH = 1,
    GEO_MODE_S2 = 2
};

struct SBGeoIndexMetaPage {
    PageHeader geo_header;

    uint8_t geo_index_uuid[16];
    uint8_t geo_table_uuid[16];

    uint8_t geo_mode;              // SBGeoMode
    uint8_t geo_precision;         // Geohash chars or S2 level
    uint8_t geo_key_bytes;          // 8 for packed geohash or S2
    uint8_t geo_reserved1;

    uint16_t geo_lat_col_id;        // Latitude column
    uint16_t geo_lon_col_id;        // Longitude column

    uint32_t geo_root_page;         // B-tree root
    uint32_t geo_reserved2;

    uint32_t geo_covering_max;      // Max covering cells
    uint32_t geo_covering_level;    // Max cover level expansion

    uint64_t geo_total_keys;
    uint64_t geo_total_covers;
    uint64_t geo_reserved3;

    uint8_t geo_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Key Layout

```
GeoKey {
    uint64 cell_id;    // Packed geohash or S2 cell ID
    TID tid;           // TID tie-breaker
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

Status geo_insert(UUID index_uuid, uint64 cell_id, TID tid);
Status geo_delete(UUID index_uuid, uint64 cell_id, TID tid);
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

- `removeDeadEntries()` scans cell posting lists and removes dead TIDs.
- Tombstones with `xmax < OIT` are removed during GC.
- Empty cell lists are dropped to keep the index compact.

TID references use `TID { gpid, slot }`. Legacy packed TID encodings are
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
