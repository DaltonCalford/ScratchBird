# GiST Index Specification for ScratchBird


**Authoritative MGA/Lock/GC References:**
- [TRANSACTION_MGA_CORE.md](../transaction/TRANSACTION_MGA_CORE.md)
- [TRANSACTION_LOCK_MANAGER.md](../transaction/TRANSACTION_LOCK_MANAGER.md)
- [MGA_IMPLEMENTATION.md](../storage/MGA_IMPLEMENTATION.md)
- [FIREBIRD_GC_SWEEP_GLOSSARY.md](../transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md)
- [FIREBIRD_CONSTANTS_REFERENCE.md](../transaction/FIREBIRD_CONSTANTS_REFERENCE.md)

**Version:** 1.0
**Date:** 2026-02-07
**Status:** Authoritative

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
- Merge nodes if underfull (optional).


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
