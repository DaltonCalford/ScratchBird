# Heap Page Layout

Status: current_authority

## 1. Scope and authority

This document defines the current heap-page binary contract.

The controlling binary authority is:

- `ScratchBird/include/scratchbird/core/heap_page.h`
- `ScratchBird/include/scratchbird/core/ondisk.h`
- current helper semantics in `ScratchBird/src/core/heap_page.cpp`

## 2. Physical page shape

The current heap page is laid out in this order:

1. shared `PageHeader`
2. `ItemPointer` array beginning immediately after the page header
3. tuple payload bodies
4. free-space band between slot array and tuple payload
5. `HeapPageSpecial` anchored at page end

### Required layout consequences

1. slot directory grows forward from `header_bytes`
2. tuple payload grows backward from the special-area boundary
3. `pageSpecial(header)` must equal `page_size - sizeof(HeapPageSpecial)` for a
   valid current heap page
4. compaction may move tuple bodies but must preserve slot identity

## 3. Slot contract

The current slot entry is `ItemPointer`.

### Binary layout

| Field | Type | Meaning |
| --- | --- | --- |
| `offset` | `uint32_t` | byte offset from start of page |
| `length` | `31-bit packed field` | tuple length |
| `flags` | `1-bit packed field` | deleted or unused state |

### Stable constants

- `FLAG_DELETED = 0x80000000`
- `LP_UNUSED = 0`

### Slot legality algorithm

An `ItemPointer` is valid only when:

1. `offset < page_size`
2. `offset + length <= page_size`
3. `offset >= sizeof(PageHeader)`

Unused slot semantics:

- `offset == 0`
- `length == 0`
- `flags == 0`

Deleted slot semantics are owned by the packed flag bit, not by a separate slot
record type.

## 4. Tuple header contract

Every heap tuple begins with the current `TupleHeader`.

### Binary layout

| Order | Field | Type | Meaning |
| --- | --- | --- | --- |
| 1 | `xmin` | `uint64_t` | creating transaction |
| 2 | `xmax` | `uint64_t` | deleting or superseding transaction, or `0` |
| 3 | `back_version_gpid` | `uint64_t` | GPID of prior version |
| 4 | `back_version_slot` | `uint16_t` | slot of prior version |
| 5 | `reserved1` | `uint16_t` | alignment padding |
| 6 | `ctid_gpid` | `GPID` | canonical tuple identity page component |
| 7 | `ctid_slot` | `uint16_t` | canonical tuple identity slot component |
| 8 | `infomask` | `uint16_t` | tuple state mask |
| 9 | `null_bitmap_offset` | `uint16_t` | null-bitmap offset |
| 10 | `padding` | `uint16_t` | alignment padding |
| 11 | `session_id` | `ID` | session lineage identifier |
| 12 | `row_uuid` | `ID` | stable logical row identity |
| 13 | `record_flags` | `uint32_t` | canonical record flags |
| 14 | `record_format` | `uint32_t` | payload format version |
| 15 | `payload_len` | `uint32_t` | stored payload bytes after header |

## 5. Tuple state vocabulary

### Infomask flags

- `HEAP_HAS_NULLS = 0x0001`
- `HEAP_XMIN_COMMITTED = 0x0002`
- `HEAP_XMIN_INVALID = 0x0004`
- `HEAP_XMAX_COMMITTED = 0x0008`
- `HEAP_XMAX_INVALID = 0x0010`
- `HEAP_XMAX_IS_MULTI = 0x0020`
- `HEAP_UPDATED = 0x0040`
- `HEAP_MOVED = 0x0080`
- `HEAP_XMIN_FROZEN = 0x0100`
- `HEAP_HOT_UPDATED = 0x0200`
- `HEAP_CHAIN = 0x0400`

### Record flags

- `RHD_DELETED = 0x0001`
- `RHD_CHAINED = 0x0002`
- `RHD_MOVED = 0x0004`
- `RHD_TOAST_PTR = 0x0008`
- `RECORD_FORMAT_V1 = 1`

These are the only current tuple-state and record-state vocabularies defined by
the heap tuple header.

## 6. Tuple identity and lineage rules

The current heap model requires all of the following:

1. slot identity is stable
2. tuple identity is `ctid_gpid + ctid_slot`
3. stable row identity is `row_uuid`
4. version-chain traversal is `back_version_gpid + back_version_slot`

### Current helper rules

- `hasBackVersion()` is true when `back_version_gpid != INVALID_GPID`
- `getBackVersionTID()` returns the prior-version `TID`
- `getTID()` returns the tuple's current `TID`
- `getCreateTxid()` returns `xmin`
- `getDeleteTxid()` returns `xmax`

No older `back_ptr_page_id` or `back_ptr_slot_id` naming outranks this model.

## 7. Special area contract

The current end-of-page special region is `HeapPageSpecial`.

| Field | Type | Meaning |
| --- | --- | --- |
| `pd_flags` | `uint16_t` | page-local heap flags |
| `reserved` | `uint16_t` | reserved alignment space |
| `table_id` | `ID` | owning table UUID |
| `pd_prune_xid` | `uint64_t` | pruning horizon marker |

### Required legality

1. special area is physically anchored at page end
2. tuple payload must never overlap it
3. `table_id` is current owning-table identity
4. pruning metadata belongs here, not in invented side structures

## 8. Free-space and maximum tuple rules

Current heap implementation computes maximum tuple size as:

`page_size - sizeof(PageHeader) - sizeof(HeapPageSpecial) - sizeof(ItemPointer)`

Heap admission, insertion, repair, and compaction must honor that bound.

## 9. MGA rules

Heap pages obey the ScratchBird MGA model:

1. `xmin` and `xmax` are MGA transaction identifiers
2. visibility derives from MGA transaction state and tuple header state
3. `COMMIT` and `ROLLBACK` both end the current transaction and immediately
   begin the next one
4. `DDL` and `DML` remain transaction-scoped
5. tuple visibility is not WAL-owned

## 10. Corruption and repair classes

Current heap-page audit and repair logic must classify at least:

- invalid slot offset
- invalid slot length
- tuple body overlapping header or special area
- broken back-version target
- malformed tuple identity
- inconsistent row identity across a version chain

Repair tooling may:

- relocate tuple bodies
- rebuild free-space structure
- compact payload bands

Repair tooling must not:

- renumber logical slots
- replace tuple identity fields with guessed values
- reinterpret removed legacy fields as current layout

## 11. Negative requirements

The following are explicitly non-current:

1. four-`u16` slot entries
2. `savepoint_id` as a tuple-header field
3. `back_ptr_page_id` and `back_ptr_slot_id` as canonical field names
4. WAL-owned tuple visibility

## 12. Implementation contract

Any implementation or audit against this file must prove:

1. heap pages use `PageHeader`, `ItemPointer`, `TupleHeader`, and
   `HeapPageSpecial` exactly
2. slot offsets and tuple payload remain within legal page bounds
3. stable slot identity survives compaction
4. MGA visibility derives from tuple transaction fields
5. version-chain traversal uses the current GPID-plus-slot model exactly
