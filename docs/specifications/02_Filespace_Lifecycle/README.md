# 02 Filespace Lifecycle

## Status
- Section status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-27
- Primary repo audited: `ScratchBird`

## Current status
- The reviewed implementation uses `tablespace` as the engine term. This section keeps `filespace` as the higher-level specification label and treats `tablespace` as the current implementation authority.
- Tablespace `0` is the primary `.sbdb` database file. Custom tablespaces are `.sbts` files.
- Page allocation, free-space management integration, and autoextend behavior are real in the current `PageManager` implementation.
- Live relocation truth currently exists as TID-aware source or target tablespace resolution during migration. Beta 1 also requires the explicit operator lifecycle, cutover, shrink, split, and durable history model defined by `TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md`.
- `TABLE_STORAGE_AND_ACCESS_METHOD_ARCHITECTURE.md` and `OVERSIZED_VALUE_RETENTION_AND_OVERFLOW_LIFECYCLE.md` were already normalized earlier in this audit wave and remain authoritative inputs for this section.

## Major drift now recorded
- Older prose described the primary database file as if it also carried a trailing `FilespaceHeader` block. The reviewed code instead treats the primary file as a `DatabaseHeader` plus fixed bootstrap-page map authority.
- Older prose described attach, detach, shadow-copy shrink, and partition split procedures more strongly than the reviewed implementation proves. Those surfaces are now explicit Beta 1 required behavior and must be closed through this work-plan rather than treated as optional expansion.
- Row movement is grounded in stable TID, CTID, back-version, and row UUID semantics, not in a separate generic remap-journal subsystem.

## Section file status
- `README.md`: code-backed section summary, current_authority_with_reconstructed_expansion
- `SPEC_OUTLINE.md`: normalized to current implementation depth, current_authority_with_reconstructed_expansion
- `DECISION_RECORD.md`: normalized to code-backed decisions, current_authority_with_reconstructed_expansion
- `DEPENDENCIES.md`: normalized to current subsystem dependencies, current_authority_with_reconstructed_expansion
- `FILESPACE_FILE_LAYOUT.md`: normalized to `DatabaseHeader` plus `TablespaceHeader` truth, current_authority_with_reconstructed_expansion
- `FILESPACE_OPERATIONS.md`: normalized to current substrate plus Beta 1 required lifecycle closure, current_authority_with_reconstructed_expansion
- `PARTITION_BOUNDARY_SPLIT_AND_OBJECT_RELOCATION.md`: expanded to Beta 1 required relocation and cutover orchestration over the current resolver substrate, current_authority_with_reconstructed_expansion
- `TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md`: authoritative Beta 1 lifecycle state machine and operator contract, reconstructed_required_with_current_substrate
- `TABLE_STORAGE_MODES_AND_ROW_MOVEMENT_MODEL.md`: normalized to stable-slot and migration-aware row movement truth, current_authority_with_reconstructed_expansion
- `TABLE_STORAGE_AND_ACCESS_METHOD_ARCHITECTURE.md`: previously backfilled, current_authority_with_reconstructed_expansion
- `OVERSIZED_VALUE_RETENTION_AND_OVERFLOW_LIFECYCLE.md`: previously backfilled, current_authority_with_reconstructed_expansion
- `TEST_CONTRACT.md`: expanded to Beta 1 lifecycle proof obligations, current_authority_with_reconstructed_expansion

## Primary audit lookup anchors
- `src/core/tid_resolver.cpp` search `TIDResolver::resolveTablespace` for
  migration-aware source or target tablespace resolution.
- `src/core/catalog_manager.cpp` search
  `CatalogManager::resolveTablespaceBindings` for catalog-backed tablespace ID
  and UUID ownership.
- `src/core/page_manager.cpp` search `PageManager::allocatePageInTablespace`
  for tablespace-local allocation authority.

## Section file index
<!-- AUTO-GENERATED:FILE-LIST:START -->
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [FILESPACE_FILE_LAYOUT.md](FILESPACE_FILE_LAYOUT.md)
- [FILESPACE_OPERATIONS.md](FILESPACE_OPERATIONS.md)
- [OVERSIZED_VALUE_RETENTION_AND_OVERFLOW_LIFECYCLE.md](OVERSIZED_VALUE_RETENTION_AND_OVERFLOW_LIFECYCLE.md)
- [PARTITION_BOUNDARY_SPLIT_AND_OBJECT_RELOCATION.md](PARTITION_BOUNDARY_SPLIT_AND_OBJECT_RELOCATION.md)
- `SECTION_CLOSURE_MATRIX.csv`
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md](TABLESPACE_DDL_AND_OPERATOR_LIFECYCLE_MODEL.md)
- [TABLE_STORAGE_AND_ACCESS_METHOD_ARCHITECTURE.md](TABLE_STORAGE_AND_ACCESS_METHOD_ARCHITECTURE.md)
- [TABLE_STORAGE_MODES_AND_ROW_MOVEMENT_MODEL.md](TABLE_STORAGE_MODES_AND_ROW_MOVEMENT_MODEL.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->

## Non-blocking expansion candidates
- Remaining exact code-map closure for live attach/detach dispatcher wiring and any later lifecycle call sites not yet tightened in this pass

## Suggestions
- Keep `filespace` only as the section label and standardize `tablespace` as the implementation term throughout the canonical specs.
- Treat attach, detach, migrate, shrink, split, and cutover as explicit Beta 1 lifecycle behavior with fail-closed state-machine semantics rather than as implied or optional current behavior.
- Use this section as the control surface for durable placement, relocation, and storage-mode ownership so later allocator, GC, and index specs stop restating partial truths.
