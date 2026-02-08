# Catalog Expansion Plan (Alpha)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.

Status: Authoritative (V3)
Last updated: 2026-01-28

## Scope
- Compare current catalog implementation (source of truth) to specs.
- Identify persistence gaps (root page pointers, table allocation, backfill).
- Define a phased expansion plan for missing catalog persistence and doc alignment.

## Source of truth references (code)
- Catalog root page layout and fields: `ScratchBird/src/core/catalog_manager.cpp:291`
- Catalog root page location constant (page 3): `ScratchBird/include/scratchbird/core/catalog_manager.h:3997`
- Schema bootstrap (18-schema hierarchy, PUBLIC default): `ScratchBird/src/core/catalog_manager.cpp:2050`
- Column permissions table allocation + root persistence: `ScratchBird/src/core/catalog_manager.cpp:1772`, `ScratchBird/src/core/catalog_manager.cpp:7810`
- Catalog backfill list (includes column/object/policy pages): `ScratchBird/src/core/catalog_manager.cpp:2423`
- Tablespace file catalog wiring (root pointer + read/write):
  `ScratchBird/include/scratchbird/core/tablespace.h:135`,
  `ScratchBird/src/core/catalog_manager.cpp:7816-7920`, `ScratchBird/src/core/catalog_manager.cpp:10253-10410`

## Current implementation summary (code truth)
- Catalog root page is stored at page 3 (not page 1) and uses a 4KB struct layout
  with a reserved block for growth. See `ScratchBird/src/core/catalog_manager.cpp:291`
  and `ScratchBird/include/scratchbird/core/catalog_manager.h:3997`.
- Catalog bootstrap creates an 18-schema hierarchy, including `public`,
  `emulation.*`, and `emulated.*` trees. See `ScratchBird/src/core/catalog_manager.cpp:2050`.
- Catalog heap pages are allocated for core + phase tables (schemas, tables,
  columns, indexes, constraints, sequences, views, triggers, permissions, stats,
  users, roles, groups, procedures, domains, emulation, FDW, UDR, sessions,
  audit log, etc.) and persisted via root page pointers. Column permissions
  are allocated but not persisted in the root page yet.
- Large fields (ACLs, expressions, definitions, metadata) are persisted via TOAST
  OIDs and loaded through catalog manager helpers.

## Spec drift vs code (docs to reconcile)
The following items show clear divergence between code and
`ScratchBird/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md`:

- Root page location and size: spec says page 1 / 16KB; code uses page 3 and a
  4KB layout (`ScratchBird/src/core/catalog_manager.cpp:291`,
  `ScratchBird/include/scratchbird/core/catalog_manager.h:3997`).
- Schema bootstrap: spec lists 8 schemas; code creates 18 and includes `public`
  plus emulation roots (`ScratchBird/src/core/catalog_manager.cpp:2050`).
- Schema/Table/Column fields: code uses owner UUIDs and name_is_delimited,
  includes RLS and temp table fields, and TOAST OIDs for large text. Specs show
  older layouts and mark some fields as "not implemented."
- Index versioning: spec defines `index_versions_page`, but code embeds version
  and lifecycle fields directly in `IndexRecord` (`ScratchBird/src/core/catalog_manager.cpp:490`).
- Dependencies/comments/object_definitions are implemented and persisted in code
  but listed as missing in spec.

## Persistence gaps to address
Resolved in 2026-01-28:
- Column/object permissions and policy pages are now persisted in the catalog root,
  allocated during init, and backfilled for older databases
  (`ScratchBird/src/core/catalog_manager.cpp:7810`, `ScratchBird/src/core/catalog_manager.cpp:2423`).
- Tablespace file catalog wiring is present; multi-file behavior is still covered
  by the tablespace plan (DDL reachability remains a separate task).

## Expansion plan (phased checklist)
### Phase A - Root page + allocation + backfill
[x] Add root page fields for: column_permissions, object_permissions, and policies.
    (tablespace_files root pointer already present)
[x] Update `writeCatalogRoot` and `readCatalogRoot` to persist those pointers.
[x] Allocate these pages during catalog initialization.
[x] Extend catalog backfill to allocate missing pages for older databases.

### Phase B - Tablespace file persistence
[x] Implement read/write helpers for `SBTablespaceFileCatalog`.
[x] Persist full file list on CREATE/ALTER TABLESPACE and ATTACH.
[x] Load `TablespaceInfo.file_paths` from sb_tablespace_files on startup
    (fallback to primary_path if needed).

### Phase C - Spec alignment (documentation-only)
[x] Update `ScratchBird/docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_STRUCTURE.md`
    with correct root page location/size, schema hierarchy, and record layouts.
[x] Remove stale "NOT IMPLEMENTED" markers for features already persisted.
[x] Add missing tables to the spec (column permissions, object permissions,
    policies, authkeys, sessions, audit log, security policy epoch, migration
    history, dormant/prepared transactions, etc.).

### Phase D - Persistence validation
[x] Add unit tests for column/object permission persistence across restart.
[x] Add unit tests for RLS policy persistence and policy TOAST reload.
[x] Add tablespace file persistence test (multi-file list survives restart).

## Notes / Open questions
- Confirm whether index versioning should remain embedded in IndexRecord
  or require a separate index_versions table for audit/history.
- Decide if schema search paths remain session-only (code truth) or if
  schema-level overrides are desired in future.
