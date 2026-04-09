# Table Storage Modes and Row Movement Model

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27

## Current status
The reviewed implementation proves a movement-aware heap-centric model with stable slot identity and online tablespace migration resolution. The older prose claimed a broader storage-mode taxonomy than this audit could prove uniformly in code.

## Current implementation
### Proven row-movement authority
- Stable slot identity is represented through `ItemPointer`.
- Tuple identity and movement history are represented through CTID, back-version GPID or slot fields, row UUID, and movement flags in `TupleHeader`.
- Storage-engine tuple APIs are movement-aware and consume tablespace-resolution logic during migration.
- Live relocation currently depends on tuple-level source or target tablespace resolution rather than a separate generic row-remap subsystem.

### Storage-mode maturity matrix
- `heap_rowstore`: implemented and code-backed
- `temp_heap_rowstore`: partial, exact runtime and lifecycle closure not completed in this pass
- `append_overlay`: not proven by reviewed code in this pass
- `columnar_overlay`: partial at the family level, but row-movement closure not uniformly proven here
- `external_virtual`: not proven by reviewed code in this pass

### Beta 1 ownership boundary
- `heap_rowstore` is the authoritative storage-mode contract for this work-plan.
- `temp_heap_rowstore` and `columnar_overlay` inherit filespace placement and
  lifecycle rules only where their own owning sections make those rules
  authoritative; this file does not invent missing family semantics.
- `append_overlay` and `external_virtual` are not required implementation
  targets for this work-plan unless later canonical promotion assigns them here
  explicitly.

## Implementation code map
| Repo | File | Symbol or area | Line | Coverage note |
| --- | --- | --- | --- | --- |
| ScratchBird | `include/scratchbird/core/heap_page.h` | `ItemPointer` | `37` | Stable slot identity authority |
| ScratchBird | `include/scratchbird/core/heap_page.h` | `TupleHeader` | `91` | CTID, back-version, row UUID, and movement flags |
| ScratchBird | `include/scratchbird/core/storage_engine.h` | `StorageEngine::getTuple(uint32_t, uint16_t, ...)` | `176` | Direct tuple access path |
| ScratchBird | `include/scratchbird/core/storage_engine.h` | `StorageEngine::getTuple(const ID&, const TID&, ...)` | `181` | Movement-aware lookup and update path |
| ScratchBird | `src/core/tid_resolver.cpp` | `TIDResolver::resolveTablespace` | `216` | Runtime source or target tablespace resolution |
| ScratchBird | `src/core/transaction_manager.cpp` | `TransactionManager::flushTransactionPublicationState` | `3800` | Movement-sensitive publication ordering |

## Drift and contradictions
- Older prose described a broader, cleaner storage-mode taxonomy than the reviewed code proves.
- The current implementation is clearly strongest in heap rowstore and movement-aware migration logic. Uniform mode closure for overlays, virtual storage, and temporary variants was not proven in this pass.
- Older prose implied a generic row-remap journal. The reviewed code instead grounds movement in heap tuple identity and resolver-based relocation.

## Deferred beyond current canonical scope
- A single catalog-backed storage-mode registry with explicit maturity states
- A single row-movement legality vocabulary shared by storage engine, GC, and
  index cleanup
- Exact closure for temporary, overlay, and external storage-mode semantics
- Remaining exact closure for non-heap storage-mode call sites and maturity
  transitions

## Suggestions
- Treat heap rowstore as the canonical current authority and describe other modes conservatively until they are separately audited.
- Keep movement legality tied to stable identity and relocation resolver behavior, not to abstract planned journals.
- Reuse the newer oversized-value and index relocation specs when extending this file.
