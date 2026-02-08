# GIN Index Specification for ScratchBird

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

GIN maps keys → posting lists of record UUIDs. Posting lists are compressed and can spill into posting trees for large sets.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Structures

- Entry tree: B-tree of key → posting list pointer.
- Posting list: compressed array of record UUIDs (delta + varint).
- Posting tree: B-tree when list exceeds threshold.

### Insert

1. Extract keys from value.
2. For each key:
   - find posting list.
   - append record_uuid.
3. If posting list too large, convert to posting tree.

### Search

1. For each key, get postings.
2. Intersect/union postings per query.
3. For each record_uuid, validate visibility via TIP.

### Delete

- Deletions insert tombstones in posting lists (no physical removal).
- GC removes tombstones after OIT advances.

---

## MGA Compliance

- Postings store record UUIDs.
- Visibility must check record header/TIP.

---

## Locking

- Entry tree uses B-tree latch coupling.
- Posting list append uses list lock.
- Posting tree uses B-tree locking.

---

## GC

- `removeDeadEntries` removes dead UUIDs from postings.
- Posting trees may be compacted/rebuilt when sparse.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
