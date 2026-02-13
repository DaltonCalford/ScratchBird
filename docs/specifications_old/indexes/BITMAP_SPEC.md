# Bitmap Index Specification for ScratchBird


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

Bitmap indexes store bitsets per value. ScratchBird bitmap uses row ordinal mapping and must validate record visibility.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Layout

- Dictionary: value → bitmap id
- Bitmap pages: compressed bitsets (RLE/WAH)

### Insert

1. Resolve value → bitmap id.
2. Set bit for record_uuid’s ordinal in bitmap.

### Search

1. Fetch bitmap for value.
2. Return candidate record UUIDs; verify MGA visibility.

### Delete

- Clear bit when record version becomes dead during GC.

---

## MGA Compliance

- Bitmap is auxiliary; never used without visibility validation.

---

## GC

- `removeDeadEntries` clears bits for dead record UUIDs.
- Bitmap can be rebuilt from visible rows when stale.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
