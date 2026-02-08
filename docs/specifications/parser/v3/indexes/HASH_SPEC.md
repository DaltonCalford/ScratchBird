# Hash Index Specification for ScratchBird


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

ScratchBird hash index uses **extendible hashing**. It supports equality lookups only and is MGA‑compliant using record UUIDs.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### On-Disk Layout

**Storage Layout Authority:** On-disk page headers, slot arrays, free-space rules, and page-type layouts are authoritative in `../storage/PAGE_TYPES_AND_LAYOUTS.md`. Any structs here are logical field groupings; do not infer byte-accurate layout from this file.

- Directory pages: array of bucket pointers (`global_depth`).
- Bucket pages: fixed-size array of entries + local depth.


**Logical Fields:**

- `base` (SBIndexPageHeader)
- `global_depth` (uint16_t)
- `count` (uint16_t)
- `bucket_ptrs[]` (uint64_t): 2^global_depth entries
- `base` (SBIndexPageHeader)
- `local_depth` (uint16_t)
- `count` (uint16_t)
- `entries[]` (HashEntry)
- `hash` (uint64_t)
- `meta` (SBIndexEntryMeta): record_uuid + record_txn


### Insert

1. Compute hash.
2. Use `global_depth` bits to pick bucket.
3. If bucket has space, insert entry.
4. If full:
   - Split bucket: increment local depth, redistribute entries.
   - If local depth exceeds global depth, double directory.

### Search

1. Compute hash.
2. Lookup bucket.
3. Scan entries with matching hash.
4. Filter by MGA visibility.

### Delete

- Create deleted record version in heap.
- Insert deletion entry (logical delete) or mark entry flags (depending on implementation).

---

## MGA Compliance

- Each entry stores `SBIndexEntryMeta`.
- Visibility via TIP.

---

## Locking / Concurrency

- Directory lock for global changes (split/double).
- Bucket lock for inserts/deletes.
- Latch order: directory → bucket.

---

## GC

- `removeDeadEntries` scans buckets and removes dead record UUID entries.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
