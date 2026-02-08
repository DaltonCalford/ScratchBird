# Count-Min Sketch Index Specification for ScratchBird


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

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Parameters

- `w` (width): number of counters per row.  
- `d` (depth): number of hash rows.  
- For error bound `ε` and failure probability `δ`:
  - `w = ceil(e / ε)`  
  - `d = ceil(ln(1/δ))`  
- Guarantees: estimated count `ĉ(x) <= c(x) + ε * N` with probability `1 - δ`.

### Hashing

Use `d` pairwise-independent hash functions (or 2-universal + double hashing).

### Insert (Increment)

1. Normalize key to bytes.  
2. For each row `i` in `0..d-1`:
   - `idx = hash_i(key) mod w`
   - `counters[i][idx] += 1`

### Query (Estimate)

1. Normalize key.  
2. For each row `i`:
   - `idx = hash_i(key) mod w`
   - Read `counters[i][idx]`
3. Return `min` of the `d` counters.

### Conservative Update (Optional)

To reduce over-estimation:

1. Compute `min_est = min_i counters[i][idx_i]`.  
2. For each row `i`, set `counters[i][idx_i] = max(counters[i][idx_i], min_est + 1)`.

### Decrement / Deletion

CMS is not safe for general deletions (can undercount). If deletions are required,
use a **counting sketch with periodic rebuild** or pair CMS with a sliding window
mechanism.

### MGA / Versioning

- CMS is **auxiliary** and may be stale after rollbacks.  
- Treat results as hints; never rely on CMS for correctness of query results.  

### Complexity Targets

- Insert/query: `O(d)` time, `O(w * d)` space.

### References (for algorithmic definitions)

- Cormode & Muthukrishnan, “An Improved Data Stream Summary: The Count-Min Sketch and its Applications,” J. Algorithms 2005.  

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


**Logical Fields:**

- `cms_header` (PageHeader)
- `cms_index_uuid[16]` (uint8_t)
- `cms_table_uuid[16]` (uint8_t)
- `cms_column_id` (uint16_t)
- `cms_depth` (uint16_t): d
- `cms_width` (uint32_t): w
- `cms_counter_bits` (uint32_t): 16 or 32
- `cms_seed_base` (uint32_t): hash seed
- `cms_total_inserts` (uint64_t)
- `cms_total_updates` (uint64_t)
- `cms_matrix_first_page` (uint32_t)
- `cms_matrix_page_count` (uint32_t)
- `cms_padding[]` (uint8_t)


### Counter Matrix Page


**Logical Fields:**

- `cm_header` (PageHeader)
- `cm_next_page` (uint32_t)
- `cm_row_index` (uint32_t): which depth row
- `cm_count` (uint32_t): counters in this page
- `cm_counters[]` (uint32_t): variable length


---

## Page Size Considerations

- Width and depth chosen to fit in memory; on-disk matrix stored in pages
- 8K page can hold about 2000 32-bit counters

---

## MGA Compliance

- CMS is **auxiliary** and must never be used for correctness decisions.
- Updates are applied **on commit** via per-transaction deltas.
- Rollback discards deltas.
- Rebuilds must scan only **visible record versions** (MGA/TIP rules).

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

Count-Min Sketch does not store per-row record UUIDs and cannot remove dead entries
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

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
