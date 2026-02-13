# GiST Index Specification for ScratchBird

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

GiST is a balanced search tree with pluggable predicates. ScratchBird GiST stores record UUIDs and uses MGA visibility.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Node Layout

- Internal node: (predicate key, child_page)
- Leaf node: (predicate key, SBIndexEntryMeta)

### Insert

1. Descend using `consistent()` predicate.
2. On leaf, insert entry.
3. If overflow, call `picksplit()` to split node.
4. Propagate split upward.

### Search

1. DFS from root using `consistent()` to prune.
2. At leaves, filter by MGA visibility.

### Delete

- Logical delete by inserting deleted record version; old entries remain.

---

## Locking

- Latch coupling root→leaf.
- Exclusive latch for node modification.

---

## GC

- Remove dead record UUID entries.
- Merge nodes if underfull (optional optimization; correctness invariant).


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
