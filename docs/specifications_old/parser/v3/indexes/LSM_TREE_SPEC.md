# LSM-Tree Index Specification for ScratchBird

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

LSM (Log-Structured Merge) is the **write-optimized** index for ScratchBird. It stores multiple versions of entries and relies on compaction + MGA rules to remove dead versions.

Entries are keyed by index key and carry **record-version metadata**:


**Logical Fields:**

- `key` (Key)
- `record_uuid` (UUID): Stable record identity
- `record_txn` (uint64_t): rhd_transaction for this version
- `record_flags` (uint32_t): RHD_DELETED, etc.
- `seq` (uint64_t): monotonic sequence for tie-break
- `tombstone` (bool): tombstone for delete


---

## Authoritative Algorithm (Normative, 2026-02-07)

### Write Path

1. Normalize key.
2. Insert `LSMEntry` into memtable (skiplist or std::map).
3. If memtable size exceeds threshold, freeze and flush to SSTable.
4. Append new SSTable to manifest (atomic publish).

### Read Path

1. Check active memtable.
2. Check immutable memtables (newest → oldest).
3. Check Level-0 SSTables (newest → oldest).
4. Check lower levels in order (L1..Ln).
5. For each candidate key, select the **visible record version** via MGA:
   - Resolve record header by `record_uuid`.
   - Use `sb_find_visible_version` with TIP.

### Delete

- A delete inserts a **tombstone entry** for the key+record_uuid.
- Tombstone remains until compaction and MGA rules permit removal.

### Compaction

1. Select overlapping SSTables.
2. Merge sorted runs.
3. For identical keys, keep the newest entry by sequence.
4. Drop entries whose record versions are dead:
   - `record_txn` committed
   - `record_txn < OIT`
   - record version deleted or superseded
5. Drop tombstones older than OIT when no visible versions remain.

---

## MGA Compliance

- LSM stores entries for record versions, not tuple headers.
- Visibility checks are TIP-based.
- Dead versions are removed **only** after OIT advances.

---

## Core API

```cpp
Status lsm_put(const Key& key, const SBIndexEntryMeta& meta);
Status lsm_get(const Key& key, const SBTransactionSnapshot* snap,
               SBTransactionManager* tm, std::vector<UUID>* results);
Status lsm_remove(const Key& key, const UUID& record_uuid);
Status lsm_compact();
```

---

## DML Integration

- INSERT: add new entry for record version.
- UPDATE: if key changes, add entry for new version; old entry remains.
- DELETE: insert tombstone for record_uuid.

---

## Garbage Collection

- Compaction is the GC mechanism.
- `removeDeadEntries(dead_record_uuids)` inserts tombstones or marks entries for drop.
- Dead entries are physically removed during compaction.

---

## Tablespace / Migration

- Record identity is `record_uuid`.
- `SBRecordPtr` is a cache hint and must be refreshed during migration.

---

## Testing Requirements

1. MGA visibility correctness under concurrent updates.
2. Tombstone drop after OIT advances.
3. Compaction correctness with overlapping SSTables.
4. Rebuild after crash (manifest integrity).

## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
