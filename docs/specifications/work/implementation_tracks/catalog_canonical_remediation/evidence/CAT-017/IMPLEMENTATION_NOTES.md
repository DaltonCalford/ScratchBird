# Implementation Notes

Status: `Completed`

## Completed in this pass
- Added CAT-017 catalog root fields and persistence wiring:
  - `index_stats_page`
  - `index_usage_page`
  - `index_contention_page`
  - `index_storage_page`
  - `index_health_page`
- Added bootstrap allocation and legacy backfill allocation for all CAT-017 families.
- Added on-disk record contracts in `CatalogManager`:
  - `IndexStatsRecord`
  - `IndexUsageRecord`
  - `IndexContentionRecord`
  - `IndexStorageRecord`
  - `IndexHealthRecord`
- Added full CAT-017 CRUD/public APIs for:
  - `index_stats`
  - `index_usage`
  - `index_contention`
  - `index_storage`
  - `index_health`
- Enforced deterministic constraints:
  - index UUID required and must resolve.
  - `index_stats`: bounded `null_frac`, `bloat_ratio`, `correlation`; `distinct_count_est <= row_count_est` when `row_count_est > 0`.
  - `index_storage`: `bytes_used <= bytes_allocated`; `fragmentation_ratio` in `[0,1]`; non-zero filespace UUID must resolve.
  - `index_health`: `light_status` cannot be `CORRUPT`; both statuses validated against allowed enum values.
- Added/updated CAT-017 bootstrap persistence and index metrics contract tests.
- Corrected CAT-017 contract helper invariant to permit default primary filespace represented by zero UUID.
