# BRIN Index Specification for ScratchBird


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

BRIN stores min/max summaries per block range. It is lossy and must be combined with MGA visibility checks.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Range Summary

For each range of heap pages:
- `min_value`, `max_value`, `null_count`, `row_count`

### Insert/Update

- Update summaries when new rows are appended to the range.
- If updates are random, mark range as stale and rebuild.

### Search

- For predicate P, compare to min/max.
- If outside range, skip.
- Otherwise scan heap and apply MGA visibility.

---

## Locking

- Range summary page locks for updates.

---

## GC

- Rebuild ranges when delete/update ratio exceeds threshold.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
