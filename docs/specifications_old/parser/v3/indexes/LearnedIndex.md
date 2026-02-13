# Learned Index (RMI) Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


Status: Authoritative (V3)
Last Updated: 2026-02-08
**Features:** Page-size agnostic, MGA compliant, model-based search

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

Learned indexes use a machine-learned model to predict the position of a key within a sorted array. This can reduce lookup time and index size for read-heavy workloads with stable key distributions.

### Primary Use Cases

- Read-heavy, mostly-static tables
- Numeric keys with stable ordering
- Large range scans with predictable distributions

---

## Authoritative Algorithm (Normative, 2026-02-07)

This section is the implementation source of truth. If any other section in
this document conflicts with the steps below, this section wins.

### Training Data

1. Extract all index keys and sort by binary-comparable order.
2. Map each key `k` to its position `pos` in the sorted array.
3. Train a 2-stage RMI:
   - Stage 1: `M1` linear models predicting which stage-2 model to use.
   - Stage 2: `M2` linear models predicting position.
4. For each stage-2 model, compute **max absolute error** over its training range.
   - Store `max_error` per model for lookup bounds.

### Lookup

1. Normalize key to numeric feature (e.g., 64-bit integer for fixed-width keys;
   for variable-length keys use order-preserving encoding or fallback to delta index).
2. Stage 1: compute model index `i = model1(k)` clamped to `[0, M2-1]`.
3. Stage 2: compute predicted position `p = model2_i(k)` clamped to `[0, N-1]`.
4. Compute search window `[p - err_i, p + err_i]` using stored `max_error`.
5. Binary search within the window for `k`.
6. If not found, check **delta index** (B-tree or LSM) for recent inserts.
7. If still not found, return NOT FOUND.

### Insert / Update

1. Insert into **delta index** (exact structure).
2. If delta size exceeds threshold (e.g., 5-10% of base size), schedule rebuild:
   - Merge base array + delta, retrain RMI, reset delta.

### Delete

1. Record a tombstone in delta index.
2. Rebuild compacts tombstones.

### Range Scan

1. Use RMI to predict range bounds for start/end keys.  
2. Scan base array between predicted bounds, merge with delta index scan.  
3. Apply tombstones.

### MGA / Versioning

- Base array and model are immutable per epoch.  
- Delta index is versioned and visible to MGA readers.  
- Rebuild publishes a new base + model as a new version.

### Complexity Targets

- Lookup: `O(log E)` where `E` is error window size.  
- Insert/delete: `O(log D)` where `D` is delta index size.  
- Rebuild: `O(N log N)` dominated by sort + training.

### References (for algorithmic definitions)

- Kraska et al., “The Case for Learned Index Structures,” SIGMOD 2018 / arXiv.  

---

## Architecture Decision

### Design Choice

Implement a **two-stage RMI (Recursive Model Index)** with a fallback B-tree or local search window:

- Stage 1 model predicts which stage-2 model to use
- Stage 2 model predicts a position
- A bounded local search window validates the result
- Updates accumulate in a delta B-tree until rebuild

---

## Data Model

### RMI Parameters

- Stage1 model count (M1)
- Stage2 model count (M2)
- Max error bound per stage2 model
- Model type: linear regression (default)

### Fallback

If the prediction misses the key, search falls back to:

- local binary search in the bounded window
- delta B-tree for recent updates

---

## On-Disk Structures

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.

### Meta Page


**Logical Fields:**

- `li_header` (PageHeader)
- `li_index_uuid[16]` (uint8_t)
- `li_table_uuid[16]` (uint8_t)
- `li_column_id` (uint16_t)
- `li_model_type` (uint16_t): 1 = linear
- `li_stage1_models` (uint32_t)
- `li_stage2_models` (uint32_t)
- `li_root_page` (uint32_t): sorted key array root
- `li_delta_btree_root` (uint32_t): update buffer
- `li_total_keys` (uint64_t)
- `li_total_queries` (uint64_t)
- `li_model_page_first` (uint32_t)
- `li_model_page_count` (uint32_t)
- `li_padding[]` (uint8_t)


### Model Page


**Logical Fields:**

- `lm_header` (PageHeader)
- `lm_next_page` (uint32_t)
- `lm_model_count` (uint32_t)
- `a` (double)
- `b` (double)
- `max_error` (uint32_t)
- `reserved` (uint32_t)


---

## Page Size Considerations

- Model parameters are small and cache-friendly
- Sorted key arrays use existing B-tree leaf pages for storage

---

## MGA Compliance

- Base array is immutable per build epoch
- Delta B-tree stores changes per transaction
- Merge requires snapshot build of new model

---

## Core API

```cpp
Status learned_build(UUID index_uuid, LearnedBuilder* builder);
LearnedResult learned_lookup(UUID index_uuid, const void* key, size_t key_len);
Status learned_insert_delta(UUID index_uuid, const void* key, size_t key_len, UUID record_uuid);
```

---

## DML Integration

- INSERT: add key to delta B-tree
- UPDATE: update delta B-tree
- DELETE: mark in delta B-tree
- Periodic rebuild merges delta into base and retrains models

---

## Garbage Collection

Learned indexes use a **base array + delta index** model. GC is enforced
by rebuild:

Record locators use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy packed TIDs are
not permitted in v2 on-disk formats.

- `removeDeadEntries()` marks dead record UUIDs in the delta index and schedules
  a rebuild when:
  - dead ratio exceeds `rebuild_threshold`, or
  - OIT advances and delta size is non-trivial.
- Rebuild merges base + delta, removes dead record UUIDs, and retrains models.
- New model pages and key arrays are swapped atomically with an epoch bump.

This guarantees no record locator movement; dead entries are removed only during the
swap boundary.

See `INDEX_GC_PROTOCOL.md` for the GC contract.

---

## Query Planner Integration

- Equality and range predicates can choose learned index when delta size is small
- Planner falls back to B-tree when update rate exceeds threshold

---

## DDL and Catalog

### Syntax

```sql
CREATE INDEX idx_readings_learned
ON readings(ts)
USING learned
WITH (stage1 = 64, stage2 = 2048, max_error = 128, rebuild_threshold = 0.05);
```

### Catalog Additions

- `sys.indexes.index_type = 'LEARNED'`
- `sys.indexes.index_options` stores model parameters and thresholds

---

## Implementation Steps

1. Implement training pipeline for linear models
2. Build sorted key arrays and error bounds
3. Add lookup with bounded search window
4. Add delta B-tree buffering and rebuild

---

## Testing Requirements

- Prediction error bound validation
- Lookup correctness with delta updates
- Rebuild consistency and MGA visibility

---

## Performance Targets

- 2-3x faster point lookup vs B-tree for static data
- Index size 30-50% smaller than B-tree

---

## Reserved Enhancements (Not Supported in V3)

- Non-linear models (piecewise or spline)
- GPU-assisted rebuilds
- Adaptive retraining based on drift

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
