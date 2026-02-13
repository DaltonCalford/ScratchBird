# LSM-Tree with TTL (Time-Series) Index Specification


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
**Features:** Page-size agnostic, MGA compliant, time-based retention

---

**Scope Note:** "WAL" references in this spec refer to a per-index write-after log (WAL, optional optional extension) and do not imply a global recovery log.

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Decision](#architecture-decision)
3. [Data Model](#data-model)
4. [On-Disk Structures](#on-disk-structures)
5. [Compaction and TTL](#compaction-and-ttl)
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

This specification extends the base LSM-tree to support TTL-based expiration and time-series workloads. Old data is dropped during compaction based on time range metadata.

### Primary Use Cases

- Time-series storage with retention windows
- Efficient queries for recent time windows
- Automatic data aging without explicit deletes

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Key Ordering

1. Normalize index key as `(timestamp, primary_key)` (timestamp first).  
2. Encode timestamps as big-endian 64-bit to preserve chronological order.  

### Write Path

1. Insert into memtable (and optional WAL).  
2. Memtable flush creates SSTables with **min_ts** and **max_ts** recorded in metadata.  

### TTL Enforcement (Compaction-Time)

1. Compute `expire_before = now() - ttl_seconds`.  
2. During compaction:
   - If `max_ts < expire_before`, drop the entire SSTable.  
   - If `min_ts < expire_before <= max_ts`, filter entries with `ts < expire_before`.  
3. Tombstones for expired entries are **not** required if the whole segment is dropped.  

### Query Path (Time Range)

1. Determine query time window `[t_start, t_end]`.  
2. Skip SSTables whose `[min_ts, max_ts]` does not overlap the window.  
3. Merge iterators from memtable + overlapping SSTables.  
4. Apply MGA visibility and TTL filtering for in-window results.

### Late Arrivals / Clock Skew

1. Accept out-of-order inserts (timestamps older than `expire_before`).  
2. If insert timestamp already expired, either:
   - Reject at write time, or  
   - Accept into memtable and allow compaction to drop immediately.  
   (Choose one policy and enforce consistently.)

### MGA / Versioning

- Versions are still tracked per MGA.  
- TTL drops must not delete visible versions that are required for active epochs.  

### Complexity Targets

- Writes: same as base LSM.  
- Range scans: skip with min/max pruning; `O(output + merge overhead)`.  

### References (for algorithmic definitions)

- O’Neil et al., “The Log-Structured Merge-Tree (LSM-tree),” Acta Informatica, 1996.  
- RocksDB Compaction Overview.  

---

## Architecture Decision

### Design Choice

Implement a **time-partitioned LSM** with per-SSTable min/max timestamp metadata and TTL cutoffs.

- Memtable and SSTables sorted by timestamp + primary key
- Compaction drops segments that are fully expired
- Optionally downsample or roll up older data

---

## Data Model

### Keys

Index key is `(timestamp, primary_key)` or `(timestamp, dimension_hash)`.

### TTL Policy

- Global TTL per index or table
- Optional per-row TTL from column
- TTL applied at compaction and sweep

---

## On-Disk Structures

### Meta Page


**Logical Fields:**

- `lt_header` (PageHeader)
- `lt_index_uuid[16]` (uint8_t)
- `lt_table_uuid[16]` (uint8_t)
- `lt_ts_column_id` (uint16_t): timestamp column
- `lt_pk_column_id` (uint16_t): optional key
- `lt_ttl_seconds` (uint64_t): retention window
- `lt_segment_seconds` (uint32_t): target SSTable time span
- `lt_level0_max_tables` (uint32_t)
- `lt_compaction_mode` (uint32_t): leveled or tiered
- `lt_total_keys` (uint64_t)
- `lt_total_segments` (uint64_t)
- `lt_padding[]` (uint8_t)


### SSTable Footer (per segment)


**Logical Fields:**

- `seg_min_ts` (uint64)
- `seg_max_ts` (uint64)
- `seg_expire_ts` (uint64): min_ts + ttl
- `seg_row_count` (uint64)
- `seg_level` (uint32)
- `seg_reserved` (uint32)


---

## Compaction and TTL

- During compaction, segments with `seg_expire_ts < now` are dropped
- Partially expired segments are compacted and filtered
- Tombstones use a shorter retention to avoid unbounded growth

---

## MGA Compliance

- Visibility rules apply to memtable and SSTables
- Expired segments are removed only when no active transaction can see them
- Sweep verifies safe removal based on oldest active transaction

---

## Core API

```cpp
Status lsm_ttl_insert(UUID index_uuid, const Key* key, const Value* value, UUID record_uuid);
Status lsm_ttl_compact(UUID index_uuid);
Status lsm_ttl_drop_expired(UUID index_uuid, uint64 now_ts);
```

---

## DML Integration

- INSERT: write to memtable with timestamp key
- UPDATE: insert new version; old version expires by TTL
- DELETE: optional tombstone with short retention

---

## Garbage Collection

LSM-TTL GC is performed during compaction:

Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
not permitted in v2 on-disk formats.

- Dead record UUIDs supplied by heap sweep are filtered during merge.
- Tombstones for dead record versions are dropped when no shadowed versions remain.
- Fully expired SSTables (by `seg_expire_ts`) are dropped when OIT allows.
- Partially expired segments are rewritten with only live rows.

GC scheduling:

- Regular compaction handles both TTL and dead entry removal.
- GC compaction can be forced when delete volume exceeds a threshold.

Metrics:

- `lsm_ttl_gc_entries_removed`
- `lsm_ttl_gc_segments_dropped`
- `lsm_ttl_gc_bytes_reclaimed`

See `INDEX_GC_PROTOCOL.md` for the shared GC contract.

---

## Query Planner Integration

- Time-range predicates select relevant SSTables by min/max timestamp
- Planner prefers LSM-TTL for recent-window queries

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_metrics_ttl
ON metrics(ts, sensor_id)
USING lsm_ttl
WITH (ttl = '30 days', segment = '1 day', compaction = 'leveled');
```

### Catalog Additions

- `sys.indexes.index_type = 'LSM_TTL'`
- `sys.indexes.index_options` stores ttl and segment duration

---

## Implementation Steps

1. Extend LSM metadata with time ranges
2. Add TTL-based compaction and drop logic
3. Add planner rule for time-range pruning
4. Add visibility checks for safe expiry

---

## Testing Requirements

- TTL expiry correctness
- Compaction drop safety with active transactions
- Time-range query correctness

---

## Performance Targets

- Compaction drop of expired segments in O(1)
- Range query over recent data 10x faster than heap

---

## Future Enhancements

- Downsampling policies for old data
- Tiered storage (cold vs hot)
- Per-tenant TTL policies

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
