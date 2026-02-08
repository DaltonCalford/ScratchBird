# Zone Maps Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)

**Version:** 2.0
**Date:** 2026-02-07
**Status:** Authoritative

---

## Overview

Zone maps store min/max (and optional extra stats) for data **segments** to allow fast pruning of scans. Zone maps are **auxiliary** and never used for correctness without validating rows via MGA visibility.

**Key property:** a zone map can only safely **exclude** a segment if the predicate cannot match any visible row in the segment.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Segment Definition

A **segment** is a contiguous run of heap pages or columnstore rows. Segment boundaries are fixed at load time or maintained by compaction.

### Zone Map Record

For each segment and column:
- `min_value`
- `max_value`
- `null_count`
- `row_count`
- optional: `bloom` or `distinct_estimate`

### Build

1. Scan segment row versions.
2. For each row version, check MGA visibility (TIP, `rhd_transaction`).
3. If visible, update min/max/null_count/row_count.
4. Persist zone map entry.

### Prune

Given predicate P:
1. Evaluate P against `min_value` and `max_value`.
2. If P cannot be satisfied by any value in the range, **skip segment**.
3. Otherwise, scan segment and validate row versions.

### Update

- On INSERT into segment: update min/max as needed.
- On UPDATE: if updated column is in zone map, update min/max as needed.
- On DELETE: no direct min/max update; segment may become stale.

### GC / Rebuild

Zone maps cannot precisely remove deleted values without a scan. Rebuild when:
- delete/update ratio exceeds threshold, or
- sweep advances OIT, or
- segment compaction occurs.

---

## MGA Compliance

- Zone maps are **hints only**.
- All query results must be verified via record visibility (TIP).
- Rebuild uses MGA-visible versions only.

## Record Identity Requirements

If any zone map stores row references (optional), it must use `record_uuid` with
optional `SBRecordPtr` cache hints. Legacy TID encodings are not permitted.

---

## Data Structures


**Logical Fields:**

- `table_uuid` (UUID)
- `segment_id` (uint64_t)
- `column_id` (uint16_t)
- `min_value` (Value)
- `max_value` (Value)
- `null_count` (uint64_t)
- `row_count` (uint64_t)
- `epoch` (uint64_t): bump on rebuild


---

## Core API

```cpp
Status zonemap_build(UUID table_uuid, uint64_t segment_id);
Status zonemap_update(UUID table_uuid, uint64_t segment_id, uint16_t column_id, const Value& v);
bool   zonemap_can_prune(UUID table_uuid, uint64_t segment_id, const Predicate& p);
Status zonemap_rebuild(UUID table_uuid, uint64_t segment_id);
```

---

## DML Integration

- INSERT: update zone map if segment open for append.
- UPDATE: update min/max for affected columns if new value extends range.
- DELETE: no immediate change; rely on rebuild/compaction.

---

## Garbage Collection

- `removeDeadEntries()` schedules rebuild for segments with high dead ratio.
- Rebuild is segment-local and uses MGA visibility.

---

## Query Planner Integration

- Prefer zone map pruning for large scans.
- Always perform row visibility checks after pruning.

---

## Testing Requirements

1. Pruning correctness (no false negatives).
2. Rebuild restores correct min/max after deletes.
3. Large segment handling (performance).

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
