# Section 11: TOAST and LOB Storage

Status: current_authority

This section is authoritative for ScratchBird's current oversized-value storage contract.

The current implementation is TOAST-first. The normative runtime surface is ToastManager, ToastVisibility, heap tuple TOAST-pointer handling, storage-engine TOAST orchestration, and heap and TOAST and LOB diagnostics. ScratchBird does not currently define a separate operator-visible standalone LOB subsystem in this section.

## Current implemented truth

Oversized values are stored through per-table TOAST tables.

The authoritative pointer contract is the packed 32-byte ToastPointer.

Chunk visibility is MGA-aware and TIP-backed through ToastVisibility.

Delete and update paths use MGA-safe soft deletion and deferred cleanup rather than immediate physical removal.

LOB-named diagnostics and page-family references may appear in runtime and diagnostic code, but they do not widen the current authoritative feature surface beyond the TOAST-first contract defined here.

## Primary audit entry points

ScratchBird/include/scratchbird/core/toast.h

ScratchBird/include/scratchbird/core/toast_visibility.h

ScratchBird/include/scratchbird/core/heap_page.h

ScratchBird/include/scratchbird/core/storage_engine.h

ScratchBird/src/core/toast.cpp

ScratchBird/src/core/toast_visibility.cpp

ScratchBird/src/core/heap_page.cpp

ScratchBird/src/core/storage_engine.cpp

ScratchBird/src/core/oversized_value_lifecycle.cpp

ScratchBird/src/core/heap_toast_lob_diagnostics.cpp

## Search-key audit anchors

- `src/core/toast.cpp` search `ToastManager::toastValue` for TOAST write and
  pointer materialization.
- `src/core/toast_visibility.cpp` search
  `ToastVisibility::evaluateChunkLifecycle` for TIP-backed chunk visibility and
  retireability.
- `src/core/storage_engine.cpp` search
  `StorageEngine::getOrCreateToastManager` for storage-engine TOAST
  orchestration.

## Capability boundary

| Capability | Status | Current authority | Non-guarantee |
| --- | --- | --- | --- |
| Table-owned TOAST storage | implemented | ToastManager plus heap and storage integration | none |
| ToastPointer contract | implemented | toast.h plus heap tuple integration | none |
| TOAST chunk visibility and reclaim classification | implemented | ToastVisibility | none |
| Heap and storage integrated TOAST read and write and delete and retire | implemented | toast.cpp, heap_page.cpp, storage_engine.cpp | none |
| Chunk diagnostics and sequence validation | implemented | heap_toast_lob_diagnostics.cpp | none |
| Encrypted pointer flag bit | implemented | ToastPointer::TOAST_ENCRYPTED | no standalone encryption policy implied |
| Standalone LOB streaming API | unsupported | none | no generic seek and read and write handle API exists |
| Standalone LOB relocation | unsupported | none | no relocate, resume, abort, or validate contract exists |
| Standalone LOB control surfaces | unsupported | none | no parser, admin, or runtime contract exists |

## Cross-section boundary rules

Section 11 owns oversized-value pointer semantics, TOAST chunk storage, TOAST chunk I/O, lifecycle classification, and oversized-value diagnostics.

Section 11 does not own filespace relocation guarantees beyond what section 02 proves.

Section 11 does not own page-header or general binary integrity rules beyond what section 05 proves.

Section 11 does not own MGA visibility rules beyond what section 08 proves.

Section 11 does not own reclaim ordering beyond what section 10 proves.

Section 11 does not own chunk-lookup index semantics beyond what section 18 proves.

## Section rule

Any future standalone LOB subsystem shall be additive. It shall not be backfilled into the current TOAST-first contract by implication or by LOB-named diagnostics alone.

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [LOB_FILESPACE_RELOCATION.md](LOB_FILESPACE_RELOCATION.md)
- [LOB_IO_SEMANTICS.md](LOB_IO_SEMANTICS.md)
- [LOB_PAGE_LAYOUTS.md](LOB_PAGE_LAYOUTS.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [TOAST_POINTER_FORMAT.md](TOAST_POINTER_FORMAT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
