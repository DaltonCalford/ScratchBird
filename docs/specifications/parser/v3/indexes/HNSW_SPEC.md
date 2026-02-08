# HNSW Index Specification for ScratchBird

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

HNSW is an approximate nearest neighbor graph. ScratchBird stores record UUIDs for vector nodes and validates MGA visibility.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Node Layout


**Logical Fields:**

- `record_uuid` (UUID)
- `level` (uint32_t)
- `vector` (float*)
- `neighbor_offsets[]` (uint32_t): adjacency list per level
- `meta` (SBIndexEntryMeta)


### Insert

1. Choose level via random exponential distribution.
2. For each level, search entry point with ef_construction.
3. Select neighbors (heuristic) and link bidirectionally.

### Search

1. Traverse upper layers with greedy search.
2. At level 0, perform ef_search to return k nearest.
3. Filter visible nodes via MGA.

### Delete

- Mark node deleted (record version deleted); neighbor links remain until GC.

---

## Locking

- Coarse-grained graph lock for insert/delete.
- Read-only search uses shared lock.

---

## GC

- Remove nodes whose record versions are dead.
- Rewire neighbors during GC compaction (optional optimization; correctness invariant).


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
