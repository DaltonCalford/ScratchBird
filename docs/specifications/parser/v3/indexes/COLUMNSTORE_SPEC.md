# Columnstore Specification for ScratchBird

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.



**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)

Status: Authoritative (V3)
Last Updated: 2026-02-08

---

## Overview

ScratchBird columnstore is optimized for OLAP scans and compression while respecting Firebird MGA. Columnstore stores **record versions** and uses MGA visibility rules at query time.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Storage Model

- Data is stored in **segments** (column chunks) grouped by row group.
- Each row group has:
  - `row_group_uuid`
  - `row_count`
  - per-column segments

### Record Version Metadata

Each row has a stable `record_uuid`. Each column segment stores values plus **version metadata**:


**Logical Fields:**

- `record_uuid` (UUID)
- `record_txn` (uint64_t): rhd_transaction of this version
- `record_flags` (uint32_t): RHD_DELETED, etc.


### Insert

1. Assign `record_uuid`.
2. Append values to each column segment.
3. Append `ColumnRowMeta` with `record_txn` and flags.

### Update

- If updated column is in columnstore:
  - append a **new record version** with same `record_uuid` and new values.
  - old version remains until sweep.

### Delete

- Create a new record version with `RHD_DELETED`.
- No physical removal until sweep.

### Visibility

Row visibility is determined by TIP:
- Use `record_txn` and MGA rules to check if a version is visible.
- Skip versions marked `RHD_DELETED`.

### Record Identity Requirements

If any columnstore structure stores row references outside the row group, it must
use `record_uuid` with optional `SBRecordPtr` cache hints. Legacy TID encodings are not permitted.

### Garbage Collection

1. Sweep identifies dead record versions (record_txn committed and < OIT).
2. Columnstore GC compacts segments to remove dead versions.
3. Row groups with high dead ratio are rewritten.

---

## Data Structures


**Logical Fields:**

- `column_uuid` (UUID)
- `row_group_uuid` (UUID)
- `row_count` (uint64_t)
- `visible_count` (uint64_t)
- `dead_count` (uint64_t)
- `min_txn` (uint64_t): oldest record_txn in segment
- `max_txn` (uint64_t): newest record_txn in segment


---

## Core API

```cpp
Status columnstore_insert(UUID table_uuid, const Row& row);
Status columnstore_scan(UUID table_uuid, const Predicate& p,
                        const SBTransactionSnapshot* snap,
                        SBTransactionManager* tm,
                        ResultSet* out);
Status columnstore_gc(UUID table_uuid);
```

---

## DML Integration

- INSERT: append new row version.
- UPDATE: append new row version; old version remains.
- DELETE: append deleted version.

---

## Query Planner Integration

- Prefer columnstore for wide scans and aggregations.
- Always apply MGA visibility filter on row versions.

---

## Testing Requirements

1. Visibility correctness with concurrent updates.
2. GC compaction removes dead versions only after OIT.
3. Compression correctness after GC rewrite.

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
