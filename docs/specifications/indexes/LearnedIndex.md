# Learned Index (RMI) Specification for ScratchBird

**Version:** 1.0
**Date:** January 22, 2026
**Status:** Implementation Ready
**Author:** ScratchBird Architecture Team
**Target:** ScratchBird Beta (Optional Index Type)
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
15. [Future Enhancements](#future-enhancements)

---

## Overview

### Purpose

Learned indexes use a machine-learned model to predict the position of a key within a sorted array. This can reduce lookup time and index size for read-heavy workloads with stable key distributions.

### Primary Use Cases

- Read-heavy, mostly-static tables
- Numeric keys with stable ordering
- Large range scans with predictable distributions

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

### Meta Page

```cpp
#pragma pack(push, 1)

struct SBLearnedIndexMetaPage {
    PageHeader li_header;

    uint8_t li_index_uuid[16];
    uint8_t li_table_uuid[16];

    uint16_t li_column_id;
    uint16_t li_model_type;        // 1 = linear

    uint32_t li_stage1_models;
    uint32_t li_stage2_models;

    uint32_t li_root_page;          // sorted key array root
    uint32_t li_delta_btree_root;   // update buffer

    uint64_t li_total_keys;
    uint64_t li_total_queries;

    uint32_t li_model_page_first;
    uint32_t li_model_page_count;

    uint8_t li_padding[];
} __attribute__((packed));

#pragma pack(pop)
```

### Model Page

```cpp
#pragma pack(push, 1)

struct SBLearnedModelPage {
    PageHeader lm_header;
    uint32_t lm_next_page;
    uint32_t lm_model_count;

    // Linear model: y = a * x + b
    struct LinearModel {
        double a;
        double b;
        uint32_t max_error;
        uint32_t reserved;
    } lm_models[];
} __attribute__((packed));

#pragma pack(pop)
```

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
Status learned_insert_delta(UUID index_uuid, const void* key, size_t key_len, TID tid);
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

TID references use `TID { gpid, slot }`. Legacy packed TID encodings are
not permitted in v2 on-disk formats.

- `removeDeadEntries()` marks dead TIDs in the delta index and schedules
  a rebuild when:
  - dead ratio exceeds `rebuild_threshold`, or
  - OIT advances and delta size is non-trivial.
- Rebuild merges base + delta, removes dead TIDs, and retrains models.
- New model pages and key arrays are swapped atomically with an epoch bump.

This guarantees no TID movement; dead entries are removed only during the
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

## Future Enhancements

- Non-linear models (piecewise or spline)
- GPU-assisted rebuilds
- Adaptive retraining based on drift
