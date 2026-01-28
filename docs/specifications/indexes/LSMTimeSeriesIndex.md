# LSM-Tree with TTL (Time-Series) Index Specification

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
**Features:** Page-size agnostic, MGA compliant, time-based retention

---

**Scope Note:** "WAL" references in this spec refer to a per-index write-after log (WAL, optional post-gold) and do not imply a global recovery log.

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

```cpp
#pragma pack(push, 1)

struct SBLSMTtlMetaPage {
    PageHeader lt_header;

    uint8_t lt_index_uuid[16];
    uint8_t lt_table_uuid[16];

    uint16_t lt_ts_column_id;      // timestamp column
    uint16_t lt_pk_column_id;      // optional key

    uint64_t lt_ttl_seconds;       // retention window
    uint32_t lt_segment_seconds;   // target SSTable time span

    uint32_t lt_level0_max_tables;
    uint32_t lt_compaction_mode;   // leveled or tiered

    uint64_t lt_total_keys;
    uint64_t lt_total_segments;

    uint8_t lt_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### SSTable Footer (per segment)

```cpp
#pragma pack(push, 1)

struct SBLSMSegmentFooter {
    uint64 seg_min_ts;
    uint64 seg_max_ts;
    uint64 seg_expire_ts;  // min_ts + ttl
    uint64 seg_row_count;
    uint32 seg_level;
    uint32 seg_reserved;
} __attribute__((packed));

#pragma pack(pop)
```

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
Status lsm_ttl_insert(UUID index_uuid, const Key* key, const Value* value, TID tid);
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

TID references use `TID { gpid, slot }`. Legacy packed TID encodings are
not permitted in v2 on-disk formats.

- Dead TIDs supplied by heap sweep are filtered during merge.
- Tombstones with `xmax < OIT` are dropped when no shadowed versions remain.
- Fully expired SSTables (by `seg_expire_ts`) are dropped when OIT allows.
- Partially expired segments are rewritten with only live rows.

GC scheduling:

- Regular compaction handles both TTL and dead TID removal.
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
