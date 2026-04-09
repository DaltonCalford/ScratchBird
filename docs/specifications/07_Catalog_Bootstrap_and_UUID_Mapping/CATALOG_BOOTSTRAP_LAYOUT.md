# Catalog Bootstrap Layout

## Status
`current_authority_with_reconstructed_expansion`

## Last code-audit date
`2026-03-27`

## Purpose
Define the durable bootstrap contract for the catalog root and the catalog-owned initialization flow that turns fixed page `2` into a usable root of catalog state.

## Current implementation status
The reviewed code proves a real catalog bootstrap subsystem:
- page `2` is fixed as `BOOTSTRAP_PAGE_CATALOG_ROOT`, but the root payload is catalog-owned rather than hard-coded entirely inside `Database::create`
- `CatalogManager` owns a bootstrap-state machine with `UNINITIALIZED`, `INITIALIZED`, and `LOCKED`
- the catalog root stores a broad pointer inventory, including the canonical `object_name` table root
- bootstrap schema seeding is real and deterministic, with `43` bootstrap schema nodes in the current implementation
- open or bootstrap flows fail closed when the catalog root is missing required `object_name` state

## Implementation code map
- `ScratchBird/include/scratchbird/core/ondisk.h:560`: fixed page `2` constant `BOOTSTRAP_PAGE_CATALOG_ROOT`
- `ScratchBird/src/core/database.cpp:3999`: create path writes page-id identity for the fixed catalog-root page
- `ScratchBird/src/core/database.cpp:4351`: database header persists `system_catalog_page = BOOTSTRAP_PAGE_CATALOG_ROOT`
- `ScratchBird/src/core/database.cpp:5866`: open path rejects header drift when the system catalog page is not the fixed catalog-root page
- `ScratchBird/include/scratchbird/core/catalog_manager.h:2925`: authoritative `BootstrapState` enum
- `ScratchBird/include/scratchbird/core/catalog_manager.h:10859`: `getBootstrapState`
- `ScratchBird/include/scratchbird/core/catalog_manager.h:10862`: `transitionBootstrapState`
- `ScratchBird/include/scratchbird/core/catalog_manager.h:13409`: `CATALOG_ROOT_PAGE = BOOTSTRAP_PAGE_CATALOG_ROOT`
- `ScratchBird/src/core/catalog_manager.cpp:10966`: allocates the `object_name` table page during bootstrap
- `ScratchBird/src/core/catalog_manager.cpp:12805`: deterministic `kBootstrapSchemas` array with `43` bootstrap schema nodes
- `ScratchBird/src/core/catalog_manager.cpp:21345`: persists `root->object_name_page = object_name_table_page_`
- `ScratchBird/src/core/catalog_manager.cpp:21684`: reads `object_name_table_page_` back from the catalog root
- `ScratchBird/src/core/catalog_manager.cpp:21841`: fail-closed error when catalog root is missing the `object_name` table page
- `ScratchBird/src/core/catalog_manager.cpp:44212`: bootstrap transition to `INITIALIZED` during bootstrap completion path
- `ScratchBird/src/core/catalog_manager.cpp:45024`: lock transition path used by bootstrap coordination

## Known contradictions and drift
- the old section text implied a much smaller static bootstrap table list than current code proves
- `Database::create` only establishes the fixed bootstrap shell; `CatalogManager` later owns most catalog-root materialization
- this pass proves the catalog-root pointer inventory is much broader than the previous prose documented, but it does not yet enumerate every field into one machine-readable manifest
- broader catalog semantics belong downstream to section `24`; this section should not overclaim them

## Non-blocking expansion candidates
- publish one machine-readable catalog-root pointer manifest derived from the real root layout
- add a corruption matrix for bootstrap catalog-root pointer fields, not just high-level open failures
- add a dedicated operator-facing bootstrap diagnostics surface for state lock or retry or repair outcomes
- finish code-backed closure of all catalog-root pointer fields so the code map moves beyond the highest-value anchors above
