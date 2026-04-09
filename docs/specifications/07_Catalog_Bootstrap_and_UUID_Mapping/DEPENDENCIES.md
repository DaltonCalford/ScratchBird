# Dependencies - 07_Catalog_Bootstrap_and_UUID_Mapping

## Status
`current_authority_with_reconstructed_expansion`

## Last code-audit date
`2026-03-27`

## Upstream dependencies
- section `05`: shared page-header, checksum, and fixed page-type validation rules used by database-open and catalog-root checks
- section `06`: fixed placement of page `2` as the catalog root and fixed bootstrap-page validation
- section `00`: subsystem ownership and durable identity vocabulary

## Runtime implementation dependencies
- `ScratchBird/include/scratchbird/core/ondisk.h:560`: fixed catalog-root page constant used by the create/open flow
- `ScratchBird/src/core/database.cpp:3999`: database bootstrap writes the fixed catalog-root shell
- `ScratchBird/src/core/database.cpp:5866`: database open validates the fixed catalog-root location
- `ScratchBird/include/scratchbird/core/catalog_manager.h:2925`: catalog bootstrap state machine authority
- `ScratchBird/src/core/catalog_manager.cpp:21345`: catalog root persistence path
- `ScratchBird/src/core/catalog_manager.cpp:21684`: catalog root reload path
- `ScratchBird/src/core/catalog_manager.cpp:21841`: fail-closed missing `object_name` dependency
- `ScratchBird/include/scratchbird/core/uuidv7.h:94`: shared durable identity type alias `ID`
- `ScratchBird/src/core/heap_page.cpp:87`: heap row UUID generation depends on the same UUID subsystem

## Downstream consumers
- section `24`: full catalog model, schema objects, and metadata overlays depend on the root and name-authority contracts documented here
- parser, binder, and executor surfaces ultimately depend on catalog-root and name-resolution state, but this section does not overclaim those higher-level semantics
- transaction, audit, and incident catalog rows depend on the same UUID identity contract

## Known dependency drift
- the old prose understated the dependency on `CatalogManager` as the real owner of bootstrap materialization
- the old prose overstated a separate name-registry physical subsystem dependency that current code does not prove
- a machine-readable dependency matrix is still missing

## Non-blocking expansion candidates
- publish one machine-readable dependency matrix for catalog bootstrap, object-name authority, and UUID-bearing structures
- tighten downstream traceability from section `07` into section `24` and any future security or protocol identity sections
