# HyperLogLog Index Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
**Features:** Page-size agnostic, MGA compliant, approximate distinct counts

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

HyperLogLog (HLL) provides fast, low-memory approximate distinct counts. It is useful for `COUNT(DISTINCT ...)` estimates and analytics.

### Primary Use Cases

- Approximate distinct counts in large datasets
- Optimizer cardinality hints
- Analytics dashboards where exact counts are not required

---

## Architecture Decision

### Design Choice

Implement HLL as an **auxiliary index** attached to a column. Each HLL index stores a register array that can be merged efficiently.

---

## Data Model

### Parameters

- `precision` (p): number of bits used for bucket index
- `m = 2^p` registers
- Expected relative error: `1.04 / sqrt(m)`

### Update

1. Hash value to 64-bit
2. Bucket = first p bits
3. Rank = leading zero count of remaining bits + 1
4. Register[bucket] = max(Register[bucket], rank)

---

## On-Disk Structures

### Meta Page

```cpp
#pragma pack(push, 1)

struct SBHLLIndexMetaPage {
    PageHeader hll_header;

    uint8_t hll_index_uuid[16];
    uint8_t hll_table_uuid[16];

    uint16_t hll_column_id;
    uint8_t hll_precision;        // p
    uint8_t hll_sparse;           // 0/1

    uint32_t hll_register_count;  // m
    uint32_t hll_register_bits;   // 6 or 8

    uint64_t hll_total_inserts;
    uint64_t hll_total_merges;

    uint32_t hll_register_first_page;
    uint32_t hll_register_page_count;

    uint8_t hll_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Register Page

```cpp
#pragma pack(push, 1)

struct SBHLLRegisterPage {
    PageHeader hr_header;
    uint32_t hr_next_page;
    uint32_t hr_count;
    uint8_t hr_registers[];    // packed 6-bit or 8-bit values
} __attribute__((packed));

#pragma pack(pop)
```

---

## Page Size Considerations

- p=14 gives 16,384 registers (error ~0.8%)
- With 8-bit registers, memory is 16KB per HLL
- Sparse mode can compress small-cardinality sets

---

## MGA Compliance

- Per-transaction HLL delta buffers merged on commit
- Rollback discards local buffer
- Snapshot visibility applies to values not yet merged

---

## Core API

```cpp
Status hll_add(UUID index_uuid, const void* key, size_t key_len);
uint64 hll_estimate(UUID index_uuid);
Status hll_merge(UUID index_uuid, const SBHLLBuffer* delta);
```

---

## DML Integration

- INSERT: add value to HLL
- DELETE: no decrement; rebuild if exact accuracy needed
- UPDATE: add new value; optional full rebuild for accuracy

---

## Garbage Collection

HLL does not support precise deletions. GC is handled by **rebuild**:

- `removeDeadEntries()` triggers a rebuild when delete volume or sweep
  frequency exceeds a threshold.
- Rebuild scans the base table under a consistent snapshot and recomputes
  registers from live rows only.
- The rebuilt register array replaces the old one atomically via
  `hll_epoch` and meta root swap.

Optional accuracy controls:

- Maintain per-transaction delta registers and merge on commit.
- Track delete/update counts to schedule rebuilds before error grows.

See `INDEX_GC_PROTOCOL.md` for GC contract expectations.

---

## Query Planner Integration

- `COUNT(DISTINCT col)` can use HLL with explicit `APPROX` hint
- Planner can use HLL to estimate distinct count for joins

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_users_hll
ON events(user_id)
USING hyperloglog
WITH (precision = 14, sparse = true);
```

### Catalog Additions

- `sys.indexes.index_type = 'HYPERLOGLOG'`
- `sys.indexes.index_options` stores precision and mode

---

## Implementation Steps

1. Implement HLL hashing and register update
2. Add sparse mode with threshold switching
3. Add per-transaction buffer and merge
4. Add planner hooks for approximate distinct

---

## Testing Requirements

- Error bound validation across cardinalities
- Merge correctness for distributed segments
- MGA visibility with concurrent inserts

---

## Performance Targets

- 10M updates/sec per core
- < 0.1% of raw data memory footprint
- Error within configured bounds

---

## Future Enhancements

- HLL++ bias correction tables
- Per-partition HLL for rollups
- Automatic fallback to exact counts if needed
