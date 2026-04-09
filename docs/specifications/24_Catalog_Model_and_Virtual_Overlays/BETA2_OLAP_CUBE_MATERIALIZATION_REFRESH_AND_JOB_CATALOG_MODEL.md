# Beta 2 OLAP Cube Materialization Refresh And Job Catalog Model

Status: reconstructed_required_beta2

## Purpose

Define the Beta 2 catalog rows that govern cube materialization freshness,
refresh jobs, rewrite eligibility, and operator-visible job lineage.

## Governing rules

1. Cube materializations are derivative tables with explicit freshness and
   lineage identity.
2. Refresh jobs must be durable catalog state, not scheduler-local memory only.
3. Rewrite eligibility depends on current freshness rows and coverage rows.

## Canonical row families

- `sb_cube_materialization_binding`
  - cube uuid, materialization uuid, storage table uuid, coverage digest,
    freshness class
- `sb_cube_refresh_window`
  - cube uuid, mode, watermark column uuid nullable, last successful watermark,
    max staleness ms
- `sb_cube_refresh_job`
  - job uuid, cube uuid, job class, state, requested at, started at,
    completed at, error code nullable
- `sb_cube_refresh_step`
  - step uuid, job uuid, step index, step class, state, row count, byte count
- `sb_cube_freshness_watermark`
  - cube uuid, source table uuid, last source txid, last applied txid,
    last refresh time
- `sb_cube_rewrite_contract`
  - cube uuid, required dimensions digest, required measures digest, security
    digest, freshness class

## Refresh modes

- `FULL_REBUILD`
- `INCREMENTAL_APPEND`
- `WINDOW_RECOMPUTE`

## Refresh publication

1. Create `sb_cube_refresh_job`.
2. Acquire source snapshot and refresh contract.
3. Build or update the target materialization.
4. Update freshness watermarks.
5. Publish the refreshed materialization binding.

## Refusal rules

- `CUBE_REFRESH_CONTRACT_MISSING`
- `CUBE_REFRESH_SOURCE_SNAPSHOT_STALE`
- `CUBE_REFRESH_WATERMARK_INVALID`
- `CUBE_REWRITE_CONTRACT_MISSING`

## Cross-section requirements

- section `24` owns the catalog rows and overlay visibility
- section `18` owns physical acceleration classes
- section `36` consumes rewrite-contract and freshness rows
- section `31` certifies refresh behavior and analytical benchmarks
