# Index Garbage Collection Protocol


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)


**Version**: 2.0
**Date**: 2026-02-07
**Status**: Authoritative

---

## Overview

This specification defines how ScratchBird removes dead **index entries** after heap sweep determines that the referenced **record versions** are permanently invisible under Firebird MGA rules. The protocol is heap-driven, TIP-based, and non-blocking.

**Key idea:** index entries are tied to **record versions**, not to logical rows. They are removed only when the referenced record version is dead by MGA rules.

---

## Definitions

- **Record version**: A physical record with `SBRecordHeader` containing `rhd_transaction`, `rhd_back_version`, and `rhd_flags`.
- **Dead record version**: A version that cannot be visible to any active or future transaction (see MGA rules).
- **Index entry**: Key + `SBIndexEntryMeta` that references a specific record version.
- **OIT**: Oldest Interesting Transaction (minimum active transaction ID).

---

## Dead Record Version Rule (Authoritative)

A record version is **dead** when all of the following are true:

1. `rhd_transaction` is **COMMITTED** in TIP.
2. `rhd_transaction < OIT`.
3. The version is either:
   - marked `RHD_DELETED`, or
   - superseded by a newer committed version that is visible to all transactions `>= OIT`.

**Result:** dead versions can be physically removed, and index entries referencing them can be purged.

---

## Index GC Phases

### Phase 1: Heap Sweep (Producer)

The heap sweep process is authoritative for identifying dead record versions.

**Output:** a stream of `RecordGCItem` entries:


**Logical Fields:**

- `record_uuid` (UUID): Stable record identity
- `record_ptr` (SBRecordPtr): Physical locator (page, slot)
- `record_txn` (uint64_t): rhd_transaction of the version
- `record_flags` (uint32_t): RHD_DELETED, etc.


**Guarantee:** each `RecordGCItem` represents a dead record version.

### Phase 2: Index GC Dispatch (Coordinator)

The sweep process emits GC batches to the index subsystem:

```cpp
void index_gc_submit(const IndexGCBatch& batch);
```

`IndexGCBatch` contains:
- `database_uuid`
- `table_uuid`
- `vector<RecordGCItem>`
- `gc_epoch` (monotonic sequence)

Dispatch is **fire-and-forget**. The sweep does not block on index GC.

### Phase 3: Index Cleanup (Consumer)

Each index implementation:
1. Receives the `IndexGCBatch`.
2. Filters entries by `table_uuid` and indexed columns.
3. Removes entries referencing the dead record version.

**Index-only validation rule:** the index must validate entry identity by UUID before removal to avoid removing entries for a different record that reused a locator.

---

## Index GC API (Required)


**Logical Fields:**

- `database_uuid` (UUID)
- `table_uuid` (UUID)
- `gc_epoch` (uint64_t)


**Guarantees:**
- `gc_apply_batch` must be idempotent.
- `gc_apply_batch` must tolerate missing entries (already cleaned or never existed).

---

## Index-Specific Removal Rules

All index types must implement the following removal semantics:

1. **Exact key indexes (B-Tree, Hash, ART):**
   - Locate entry by key + `record_uuid` (or `record_ptr` if validated).
   - Remove entry from leaf/bucket.

2. **Posting-list indexes (GIN, Inverted, Fulltext):**
   - Remove `record_uuid` (or pointer) from posting list.
   - If posting list becomes empty, remove the key entry.

3. **Range/Spatial indexes (R-Tree, Z-Order, Quadtree/Octree):**
   - Remove leaf entry matching `record_uuid`.
   - Rebalance if underflow rules are met.

4. **LSM/Log-structured indexes:**
   - Insert a GC tombstone into memtable if immediate removal is unsafe.
   - Ensure compaction drops entries for dead versions.

5. **Approximate/summary indexes (Bloom, ZoneMap, HLL, CMS):**
   - If deletions are not supported, mark the summary segment for rebuild.
   - Rebuild during maintenance or when segment-level GC threshold is exceeded.

---

## Concurrency and Locking

- Index GC uses **low-priority locks** and must not block foreground reads/writes.
- Deadlock avoidance: GC must **yield** if it cannot acquire a page/node lock within its budget.
- All lock modes and compatibility follow `TRANSACTION_LOCK_MANAGER.md`.

---

## Failure Handling

- If GC fails partway, it can be retried with the same `gc_epoch`.
- Missing entries are not errors.
- Index corruption triggers a rebuild of that index only (table remains online if possible).

---

## Testing Requirements

1. **Visibility correctness:**
   - Insert/update/delete, advance OIT, ensure dead entries are removed.
2. **Idempotence:**
   - Apply the same GC batch twice; results must be identical.
3. **Concurrency:**
   - Heavy concurrent writes while GC runs must not block readers.
4. **Locator reuse:**
   - Ensure `record_uuid` validation prevents accidental removal.

---

## Columnstore Index GC

Columnstore GC is segment-based:
- Dead versions invalidate segment rows by `record_uuid`.
- Segments are compacted when the invalid ratio crosses a threshold.

---

## LSM-Tree Index GC

LSM GC is compaction-based:
- GC generates tombstones for dead `record_uuid`s.
- Compaction drops entries where record versions are dead per MGA rules.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
