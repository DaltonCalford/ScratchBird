# Count-Min Sketch Index Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
**Features:** Page-size agnostic, MGA compliant, approximate frequency

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Data Model](#data-model)
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

Count-Min Sketch (CMS) provides approximate frequency counts with sublinear memory. It is useful for heavy-hitter queries and optimizer statistics.

### Primary Use Cases

- Approximate `TOP K` queries
- Streaming frequency estimation
- Optimizer cardinality hints for skewed distributions

---

## Architecture Decision

### Design Choice

Implement CMS as an **auxiliary index** attached to a column. It stores a hash-based counter matrix. CMS is not exact; it should be used for approximate queries or planner hints.

---

## Data Model

### Parameters

- `width` (w): number of counters per row
- `depth` (d): number of hash rows
- Error bound: `epsilon = e / w`
- Confidence: `delta = e^-d`

### Counter Update (Conservative Update)

```
for each row i:
  idx = hash_i(value) % width
  counters[i][idx] = max(counters[i][idx], min_estimate + 1)
```

---

## On-Disk Structures

### Meta Page

```cpp
#pragma pack(push, 1)

struct SBCMSIndexMetaPage {
    PageHeader cms_header;

    uint8_t cms_index_uuid[16];
    uint8_t cms_table_uuid[16];

    uint16_t cms_column_id;
    uint16_t cms_depth;           // d
    uint32_t cms_width;           // w

    uint32_t cms_counter_bits;    // 16 or 32
    uint32_t cms_seed_base;       // hash seed

    uint64_t cms_total_inserts;
    uint64_t cms_total_updates;

    uint32_t cms_matrix_first_page;
    uint32_t cms_matrix_page_count;

    uint8_t cms_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Counter Matrix Page

```cpp
#pragma pack(push, 1)

struct SBCMSMatrixPage {
    PageHeader cm_header;
    uint32_t cm_next_page;
    uint32_t cm_row_index;        // which depth row
    uint32_t cm_count;            // counters in this page
    uint32_t cm_counters[];        // variable length
} __attribute__((packed));

#pragma pack(pop)
```

---

## Page Size Considerations

- Width and depth chosen to fit in memory; on-disk matrix stored in pages
- 8K page can hold about 2000 32-bit counters

---

## MGA Compliance

- CMS updates are approximate and can be applied on commit
- Per-transaction deltas are accumulated and merged at commit
- Rollback discards deltas

---

## Core API

```cpp
Status cms_update(UUID index_uuid, const void* key, size_t key_len, uint32 count);
uint64 cms_estimate(UUID index_uuid, const void* key, size_t key_len);
```

---

## DML Integration

- INSERT: update CMS with increment 1
- DELETE: optional decrement if negative counters enabled; otherwise ignore and rebuild periodically
- UPDATE: decrement old value and increment new value if enabled

---

## Garbage Collection

Count-Min Sketch does not store per-row TIDs and cannot remove dead entries
precisely during sweep. GC is implemented as a **rebuild**:

- `removeDeadEntries()` triggers a background rebuild when:
  - delete/update volume exceeds a configured threshold, or
  - GC runs after OIT advances.
- Rebuild scans the base table under a consistent snapshot and regenerates
  the counter matrix from live rows only.
- The rebuilt matrix replaces the old one atomically by swapping the
  meta root pointer and incrementing `cms_epoch`.

Optional accuracy controls:

- Maintain per-transaction deltas and merge on commit.
- Track delete/update counts to schedule rebuilds before error grows.

See `INDEX_GC_PROTOCOL.md` for the GC contract.

---

## Query Planner Integration

- Planner can use CMS to estimate selectivity for `WHERE column = value`
- Approximate `TOP K` can be implemented using CMS with heavy-hitter tracking

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_clicks_cms
ON clicks(user_id)
USING count_min_sketch
WITH (width = 100000, depth = 5, counter_bits = 32, conservative = true);
```

### Catalog Additions

- `sys.indexes.index_type = 'COUNT_MIN_SKETCH'`
- `sys.indexes.index_options` stores width, depth, counter_bits

---

## Implementation Steps

1. Implement CMS counter matrix and hashing
2. Add per-transaction delta buffers
3. Add merge on commit and rebuild logic
4. Add planner integration for selectivity estimation

---

## Testing Requirements

- Error bound validation with synthetic data
- Insert/update/delete behavior with rollback
- Planner estimates vs actual counts

---

## Performance Targets

- 5M updates/sec per core
- Memory overhead < 1% of raw data size
- Error within configured epsilon

---

## Future Enhancements

- Decay or time-windowed counts
- Count-Min Sketch with conservative update and aging
- Shared CMS per table for global stats
