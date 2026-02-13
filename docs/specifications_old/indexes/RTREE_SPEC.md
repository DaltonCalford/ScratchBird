# R-Tree Index Specification for ScratchBird


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

R-Tree indexes rectangles. ScratchBird R-Tree stores record UUIDs and uses MGA visibility.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Node Layout

- Internal node: list of (MBR, child_page)
- Leaf node: list of (MBR, SBIndexEntryMeta)

### Insert

1. Choose subtree minimizing area enlargement.
2. Insert entry into leaf.
3. Split node if overflow (quadratic split default).
4. Propagate split upward.

### Search

1. Traverse nodes whose MBR intersects query.
2. Filter leaves by MGA visibility.

### Delete

- Logical delete via record version.
- Condense tree on GC.

---

## Locking

- Latch coupling root→leaf.
- Exclusive latch for insert/split.

---

## GC

- Remove dead record UUIDs.
- Condense tree and merge underfull nodes.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
