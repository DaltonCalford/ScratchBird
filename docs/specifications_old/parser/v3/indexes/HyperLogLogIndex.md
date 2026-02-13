# HyperLogLog Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


Status: Authoritative (V3)
Last Updated: 2026-02-08
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
15. [Reserved Enhancements (Not Supported in V3)](#reserved-enhancements-not-supported-in-v3)

---

## Overview

### Purpose

HyperLogLog (HLL) provides fast, low-memory approximate distinct counts. It is useful for `COUNT(DISTINCT ...)` estimates and analytics.

### Primary Use Cases

- Approximate distinct counts in large datasets
- Optimizer cardinality hints
- Analytics dashboards where exact counts are not required

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Parameters

- Precision `p` in `[4..16]` (typical).  
- Number of registers `m = 2^p`.  
- Expected relative error `≈ 1.04 / sqrt(m)`.  

### Update

1. Normalize key, hash to 64-bit value `x`.  
2. Bucket index `j` = first `p` bits of `x`.  
3. Remaining bits `w` = lower `64 - p` bits.  
4. Rank `r` = position of first 1-bit in `w` + 1 (i.e., `rho(w)`).  
5. `register[j] = max(register[j], r)`.  

### Estimate

1. Compute harmonic mean:
   - `Z = 1 / sum(2^{-register[j]})`.  
2. Bias-corrected estimate:
   - `E = alpha_m * m^2 * Z`  
   - `alpha_m` constants:
     - `m=16` → 0.673
     - `m=32` → 0.697
     - `m=64` → 0.709
     - `m>=128` → `0.7213 / (1 + 1.079/m)`
3. Small-range correction (linear counting):
   - If `E <= (5/2) * m` and `V` zero-registers exist:
     - `E = m * ln(m / V)`
4. Large-range correction:
   - If `E > (1/30) * 2^32`, use:
     - `E = -2^32 * ln(1 - E/2^32)`

### Merge

To combine HLLs (e.g., for partitions), take per-register maximum:

```
for each j: R[j] = max(R1[j], R2[j])
```

### MGA / Versioning

- HLL is **auxiliary** and may be stale after rollbacks.  
- Use for planner hints and approximate queries only.  

### Complexity Targets

- Update/query: `O(1)` time, `O(m)` space.

### References (for algorithmic definitions)

- Flajolet et al., “HyperLogLog: the analysis of a near-optimal cardinality estimation algorithm,” DMTCS 2007.  

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

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.

### Meta Page


**Logical Fields:**

- `hll_header` (PageHeader)
- `hll_index_uuid[16]` (uint8_t)
- `hll_table_uuid[16]` (uint8_t)
- `hll_column_id` (uint16_t)
- `hll_precision` (uint8_t): p
- `hll_sparse` (uint8_t): 0/1
- `hll_register_count` (uint32_t): m
- `hll_register_bits` (uint32_t): 6 or 8
- `hll_total_inserts` (uint64_t)
- `hll_total_merges` (uint64_t)
- `hll_register_first_page` (uint32_t)
- `hll_register_page_count` (uint32_t)
- `hll_padding[]` (uint8_t)


### Register Page


**Logical Fields:**

- `hr_header` (PageHeader)
- `hr_next_page` (uint32_t)
- `hr_count` (uint32_t)
- `hr_registers[]` (uint8_t): packed 6-bit or 8-bit values


---

## Page Size Considerations

- p=14 gives 16,384 registers (error ~0.8%)
- With 8-bit registers, memory is 16KB per HLL
- Sparse mode can compress small-cardinality sets

---

## MGA Compliance

- HLL is **auxiliary** and must never be used for correctness decisions.
- Per-transaction HLL delta buffers are merged on commit.
- Rollback discards local buffer.
- Rebuild scans **visible record versions** (MGA/TIP rules) only.

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

optional accuracy controls:

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

## Reserved Enhancements (Not Supported in V3)

- HLL++ bias correction tables
- Per-partition HLL for rollups
- Automatic fallback to exact counts if needed

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
