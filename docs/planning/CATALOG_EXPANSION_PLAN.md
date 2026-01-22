# Catalog Expansion Plan (Alpha)
Status: Draft
Last updated: 2026-01-09

## Scope
- Compare current catalog implementation (source of truth) to specs.
- Identify persistence gaps (root page pointers, table allocation, backfill).
- Define a phased expansion plan for missing catalog persistence and doc alignment.

## Source of truth references (code)
- Catalog root page layout and fields: `ScratchBird/src/core/catalog_manager.cpp:291`
- Catalog root page location constant (page 3): `ScratchBird/include/scratchbird/core/catalog_manager.h:3997`
- Schema bootstrap (22-schema hierarchy, PUBLIC default): `ScratchBird/src/core/catalog_manager.cpp:1930`
- Column permissions table allocation (not persisted in root): `ScratchBird/src/core/catalog_manager.cpp:1605`
- Catalog backfill list (no column/object/policy/tablespace_files pages): `ScratchBird/src/core/catalog_manager.cpp:2258`
- Tablespace file catalog struct (not wired): `ScratchBird/include/scratchbird/core/tablespace.h:135`

## Current implementation summary (code truth)
- Catalog root page is stored at page 3 (not page 1) and uses a 4KB struct layout
  with a reserved block for growth. See `ScratchBird/src/core/catalog_manager.cpp:291`
  and `ScratchBird/include/scratchbird/core/catalog_manager.h:3997`.
- Catalog bootstrap creates a 22-schema hierarchy, including `public`,
  `emulation.*`, and `emulated.*` trees. See `ScratchBird/src/core/catalog_manager.cpp:1930`.
- Catalog heap pages are allocated for core + phase tables (schemas, tables,
  columns, indexes, constraints, sequences, views, triggers, permissions, stats,
  users, roles, groups, procedures, domains, emulation, FDW, UDR, sessions,
  audit log, etc.) and persisted via root page pointers.
- Large fields (ACLs, expressions, definitions, metadata) are persisted via TOAST
  OIDs and loaded through catalog manager helpers.

## Spec drift vs code (docs to reconcile)
The following items show clear divergence between code and
`ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`:

- Root page location and size: spec says page 1 / 16KB; code uses page 3 and a
  4KB layout (`ScratchBird/src/core/catalog_manager.cpp:291`,
  `ScratchBird/include/scratchbird/core/catalog_manager.h:3997`).
- Schema bootstrap: spec lists 8 schemas; code creates 22 and includes `public`
  plus emulation roots (`ScratchBird/src/core/catalog_manager.cpp:1930`).
- Schema/Table/Column fields: code uses owner UUIDs and name_is_delimited,
  includes RLS and temp table fields, and TOAST OIDs for large text. Specs show
  older layouts and mark some fields as "not implemented."
- Index versioning: spec defines `index_versions_page`, but code embeds version
  and lifecycle fields directly in `IndexRecord` (`ScratchBird/src/core/catalog_manager.cpp:490`).
- Dependencies/comments/object_definitions are implemented and persisted in code
  but listed as missing in spec.

## Persistence gaps to address
1) Column permissions page is allocated but not persisted in the root page.
   After restart, `column_permissions_table_page_` remains zero and lookups
   are bypassed. Allocation exists at `ScratchBird/src/core/catalog_manager.cpp:1605`,
   but root page has no field for it.

2) Object permissions and row-level security policies are defined in the API
   (`ScratchBird/include/scratchbird/core/catalog_manager.h:4071`) but there is
   no allocation or root-page persistence. Any grant/policy usage will fail
   when page ids are zero.

3) Tablespace file catalog is defined (`SBTablespaceFileCatalog`) but no
   root pointer or read/write wiring exists. Multi-file tablespaces cannot be
   persisted or reloaded (`ScratchBird/include/scratchbird/core/tablespace.h:135`).

4) Backfill list omits column permissions, object permissions, policies, and
   tablespace files. Older databases will never allocate these pages during
   load (`ScratchBird/src/core/catalog_manager.cpp:2258`).

## Expansion plan (phased checklist)
### Phase A - Root page + allocation + backfill
[ ] Add root page fields for: column_permissions, object_permissions,
    policies, and tablespace_files.
[ ] Update `writeCatalogRoot` and `readCatalogRoot` to persist those pointers.
[ ] Allocate these pages during catalog initialization.
[ ] Extend catalog backfill to allocate missing pages for older databases.

### Phase B - Tablespace file persistence
[ ] Implement read/write helpers for `SBTablespaceFileCatalog`.
[ ] Persist full file list on CREATE/ALTER TABLESPACE and ATTACH.
[ ] Load `TablespaceInfo.file_paths` from sb_tablespace_files on startup
    (fallback to primary_path if needed).

### Phase C - Spec alignment (documentation-only)
[ ] Update `ScratchBird/docs/specifications/catalog/SYSTEM_CATALOG_STRUCTURE.md`
    with correct root page location/size, schema hierarchy, and record layouts.
[ ] Remove stale "NOT IMPLEMENTED" markers for features already persisted.
[ ] Add missing tables to the spec (column permissions, object permissions,
    policies, authkeys, sessions, audit log, security policy epoch, migration
    history, dormant/prepared transactions, etc.).

### Phase D - Persistence validation
[ ] Add unit tests for column/object permission persistence across restart.
[ ] Add unit tests for RLS policy persistence and policy TOAST reload.
[ ] Add tablespace file persistence test (multi-file list survives restart).

## Notes / Open questions
- Confirm whether index versioning should remain embedded in IndexRecord
  or require a separate index_versions table for audit/history.
- Decide if schema search paths remain session-only (code truth) or if
  schema-level overrides are desired in future.
