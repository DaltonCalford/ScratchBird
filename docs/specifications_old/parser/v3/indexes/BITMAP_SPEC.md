# Bitmap Index Specification for ScratchBird

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

Bitmap indexes store bitsets per value. ScratchBird bitmap uses **stable heap
slot ordinals** (page_id, slot_id) and must validate record visibility via MGA.
This index is **auxiliary** and never bypasses visibility checks.

---

## Authoritative Algorithm (Normative, 2026-02-07)

### Layout

- Dictionary: value → bitmap id
- Bitmap pages: compressed bitsets (RLE/WAH)
- Bitmap bit position: `(heap_page_id, heap_slot_id)`
- Each bitmap page covers a fixed page-id range (`pages_per_bitmap`)

**Heap Slot Ordinal Rule (Normative):**
- The bitmap index uses the **physical heap page id + slot id** as the stable
  bitmap position for a record version chain.
- When a record version is updated, the **new version must reuse the same
  slot** when possible; if a new slot is allocated, the bitmap must be updated
  to reflect the new slot.
- GC removes bitmap bits only after the version is no longer visible to any
  transaction.

### Insert (Normative)

1. Resolve value → bitmap id (create if missing).
2. Resolve heap physical location `(page_id, slot_id)` for the record version.
3. Compute bitmap segment id:
   - `segment_id = page_id / pages_per_bitmap`
4. Load or create bitmap segment page.
5. Set bit at `(page_id, slot_id)` in the segment bitmap.
6. Record MGA metadata for the record version in the index entry:
   - `creator_xid`, `deleter_xid` (or equivalent MGA visibility markers).

### Search (Normative)

1. Resolve value → bitmap id.
2. Read bitmap segments and collect `(page_id, slot_id)` positions where bit=1.
3. For each position:
   - Load heap record version at `(page_id, slot_id)`.
   - Apply MGA visibility rules from `TRANSACTION_MGA_CORE.md`.
   - If visible, return record UUID.

### Delete / Update (Normative)

- **Delete**: mark record as deleted (new version with deleter_xid).
  - Do **not** clear bitmap immediately.
  - GC clears bit when the version is no longer visible.
- **Update**:
  - Write new version (prefer same slot).
  - If slot changes, set bit for new slot and leave old slot bit until GC.

---

## MGA Compliance

- Bitmap is auxiliary; never used without visibility validation.
- For each bitmap position, executor must validate visibility using
  `creator_xid`/`deleter_xid` rules.
- Bitmap may return false positives; executor must filter.

---

## GC

- `removeDeadEntries` clears bits for dead record versions by scanning bitmap
  segments and verifying MGA visibility.
- Bitmap can be rebuilt from visible rows when stale or corrupted.


## See Also

- `INDEX_IMPLEMENTATION_REFERENCE.md` — authoritative algorithm map (includes per-index specs).
- `INDEX_IMPLEMENTATION_SPEC.md` — global MGA/UUID requirements.
- `INDEX_GC_PROTOCOL.md` — index GC contract.
