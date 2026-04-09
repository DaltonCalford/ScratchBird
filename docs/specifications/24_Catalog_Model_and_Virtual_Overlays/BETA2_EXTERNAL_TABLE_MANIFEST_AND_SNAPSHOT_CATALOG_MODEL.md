# Beta 2 External Table Manifest And Snapshot Catalog Model

## Purpose

Define the catalog rows and overlay surfaces required to represent object-store
table snapshots, manifests, schemas, partitions, and attached external-table
state inside ScratchBird.

## Governing rules

1. External snapshot metadata is catalog-backed, not parser-local state.
2. Local catalog rows record imported or attached snapshot identity even when
   the source family uses its own metadata files.
3. Refresh and commit depend on one explicit snapshot pointer row.

## Canonical row families

- `sb_external_table`
  - table uuid, family class, attach mode, object store root
- `sb_external_snapshot`
  - snapshot uuid, sequence id, committed at, parent snapshot uuid, status
- `sb_external_manifest`
  - manifest uuid, snapshot uuid, manifest class, object locator, row count,
    byte count, stats digest
- `sb_external_schema_version`
  - schema version uuid, snapshot uuid, compatibility class, column digest
- `sb_external_partition_spec`
  - partition spec uuid, transform digest, field mapping digest
- `sb_external_snapshot_pointer`
  - table uuid, current snapshot uuid, local generation, refresh state

## Overlay rules

1. Virtual catalog views shall expose family-neutral rows and family-specific
   detail overlays.
2. Imported metadata remains filtered to the owning attached table root.
3. Quarantined snapshots remain visible to operators but hidden from ordinary
   query planning.

## Refresh publication

1. Import manifests and schema rows into staging state.
2. Validate consistency and compatibility.
3. Update `sb_external_snapshot_pointer`.
4. Commit the local generation.

## Refusal rules

- `EXTERNAL_SNAPSHOT_POINTER_MISSING`
- `EXTERNAL_MANIFEST_IMPORT_FAILED`
- `EXTERNAL_SCHEMA_COMPATIBILITY_REFUSED`
- `EXTERNAL_SNAPSHOT_QUARANTINED`

## Cross-section requirements

- section 24 owns the catalog rows and overlay visibility
- section 39 owns object-store table commit and refresh workflows
- section 20 owns operator diagnostics on quarantined snapshots
