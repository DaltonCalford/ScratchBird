# SP-GiST Index Specification for ScratchBird

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

SP-GiST partitions space into non-overlapping regions. It is suitable for tries, quadtrees, and IP prefix trees. ScratchBird SP-GiST stores record UUIDs and MGA metadata.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Node Layout

- Inner node: partitioning prefix + child pointers
- Leaf node: key + SBIndexEntryMeta

### Insert

1. Use `choose()` to select partition.
2. Create new node/leaf when partition missing.
3. Split when node exceeds capacity.

### Search

1. Traverse only partitions consistent with query.
2. Filter leaves by MGA visibility.

### Delete

- Logical delete via record version; GC removes stale entries.

---

## Locking

- Latch coupling root→leaf.
- Exclusive latch for split/insert.

---

## GC

- Remove dead record UUID entries.
- Condense nodes if empty.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
