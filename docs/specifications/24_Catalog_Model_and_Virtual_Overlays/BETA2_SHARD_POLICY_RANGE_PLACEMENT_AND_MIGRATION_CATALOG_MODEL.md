# Beta 2 Shard Policy Range Placement And Migration Catalog Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 catalog rows required for shard policy, range ownership,
placement, and migration workflows.

## Governing rules

1. Shard policy is durable catalog truth.
2. Placement changes publish new epochs; they do not rewrite history in place.
3. Migration progress rows are derivative progress evidence, not routing truth.

## Canonical row families

- `sb_shard_policy`
  - policy uuid, database uuid, shard kind, split policy, merge policy,
    placement class, read-routing class
- `sb_shard_range`
  - range uuid, table uuid, shard uuid, range min, range max, routing epoch,
    parent range uuid nullable
- `sb_shard_placement`
  - placement uuid, shard uuid, node uuid, role, locality class, state,
    placement epoch
- `sb_shard_split_proposal`
  - proposal uuid, source shard uuid, cut boundary digest, status, created time
- `sb_shard_migration_job`
  - migration uuid, shard uuid, source node uuid, destination node uuid,
    migration class, status, cutover epoch nullable
- `sb_shard_cutover_fence`
  - fence uuid, shard uuid, old placement epoch, new placement epoch, verified
    bool
- `sb_shard_read_route`
  - route uuid, shard uuid, route mode, max staleness ms nullable, preferred
    locality nullable

## Publication rules

1. `sb_shard_range` changes only through split or merge workflows.
2. `sb_shard_placement` changes only through placement publication.
3. `sb_shard_cutover_fence` must exist before old placement retirement.
4. Quarantined or failed migration jobs remain visible to operators.

## Overlay rules

- operator overlays shall join current placement, migration status, and routing
  mode without hiding historical epochs
- donor-emulation overlays may map these rows to donor-specific shard metadata

## Refusal rules

- `SHARD_RANGE_ROW_MISSING`
- `SHARD_PLACEMENT_EPOCH_CONFLICT`
- `SHARD_MIGRATION_JOB_INCONSISTENT`
- `SHARD_CUTOVER_FENCE_MISSING`

## Cross-section requirements

- section `24` owns the row families and overlay visibility
- section `25` consumes them for routing and migration workflows
- section `42` consumes them for failure classification and recovery
