# Beta 2 Open Table Format And Object Store Table Model

## Purpose

Define ScratchBird support for object-store-backed table families with
snapshot-manifest semantics comparable to Iceberg, Delta, and Hudi-class
systems.

## Governing rules

1. Metadata commit and data-file production are separate phases.
2. Snapshot identity, manifest identity, and schema identity are first-class
   catalog concepts.
3. Local ScratchBird catalog truth owns attachment, imported snapshot identity,
   and optimistic commit fencing.

## Admitted families

- `ICEBERG_CLASS`
- `DELTA_CLASS`
- `HUDI_CLASS`
- `SCRATCHBIRD_NATIVE_OBJECT_TABLE`

## Table states

- `ATTACHED_READ_ONLY`
- `ATTACHED_READ_WRITE`
- `REFRESH_PENDING`
- `COMMIT_PENDING`
- `QUARANTINED`

## Common commit flow

1. Produce or stage new object files.
2. Build next manifest and statistics set.
3. Validate optimistic conflict rules against the current snapshot pointer.
4. Publish the next snapshot pointer.
5. Mark old manifests and files for retention according to policy.

## Common refresh flow

1. Load current external snapshot pointer.
2. Compare with local attached snapshot pointer.
3. Import new snapshot metadata into the local catalog overlay.
4. Publish the new attached snapshot generation.

## Required capabilities

- snapshot time travel by snapshot id
- schema evolution with explicit compatibility rules
- partition transform metadata
- file-level statistics for pruning and pushdown
- retention and orphan-file tracking

## Refusal rules

- `OBJECT_TABLE_SNAPSHOT_CONFLICT`
- `OBJECT_TABLE_MANIFEST_INVALID`
- `OBJECT_TABLE_SCHEMA_INCOMPATIBLE`
- `OBJECT_TABLE_REFRESH_QUARANTINED`

## Metrics

- attached snapshot age
- refresh lag
- manifest import duration
- file pruning ratio
- optimistic commit conflict count

## Cross-section requirements

- section 24 owns external manifest and snapshot catalog rows
- section 39 owns commit, refresh, restore, and retention workflows
- section 36 consumes statistics for pruning and pushdown
