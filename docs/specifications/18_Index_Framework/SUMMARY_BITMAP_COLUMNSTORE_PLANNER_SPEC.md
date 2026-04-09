# Summary Bitmap Columnstore Planner Spec

## Purpose
Define the planner contract for summary, bitmap, bloom, and columnar families:

- `BRIN`
- zonemap-like summaries
- `BLOOM`
- `BITMAP`
- `COLUMNSTORE`

## Hard Invariants
1. Stored bitmap access and synthetic bitmap combination are distinct path
   families.
2. Summary families return candidate regions unless exactness is explicitly
   proven.
3. `COLUMNSTORE` is a real planner family with projection and late
   materialization semantics, not a disguised row-store scan.
4. `BLOOM` is filter-only in first-wave planning.

## Canonical Paths
- `BRIN_SCAN`
- `SUMMARY_FILTER_SCAN`
- `BITMAP_STORAGE_SCAN`
- `BITMAP_COMBINE_SCAN`
- `COLUMNSTORE_SCAN`

## Family Semantics

### `BRIN` and zonemap summaries
- planner family: `SUMMARY`
- path result: candidate ranges or page intervals
- exactness: `CANDIDATE_REGION`
- required recheck: residual predicate plus visibility

### `BLOOM`
- planner family: `FILTER_ONLY`
- path result: filter reduction only
- exactness: `CANDIDATE_REGION`
- required recheck: predicate and visibility

### `BITMAP`
- planner family: `COMPRESSED_CANDIDATE`
- path result: candidate row identifiers or candidate containers
- exactness: exact or lossy depending on container mode

### `COLUMNSTORE`
- planner family: `COLUMNAR`
- path result: projected rows or candidate row groups
- exactness: depends on projection and segment metadata
- alternate layouts and projection-local ordering are legal planner-visible
  choices when published metadata proves them

## Metrics Packet
- `pages_per_range`
- `prune_ratio_est`
- `unsummarized_range_fraction`
- `summary_staleness_fraction`
- `bitmap_density`
- `bitmap_false_positive_ratio`
- `lossy_container_fraction`
- `column_bytes_pruned_ratio`
- `row_groups_touched_ratio`
- `chunk_prune_ratio`
- `projection_layout_count`
- `bulk_filter_gain_est`
- `mutable_buffer_fraction`
- `late_materialization_gain_est`
- `projection_width_bytes`
- `delta_fraction`

## Path Legality

### `BRIN_SCAN`
Legal when:

- range predicates align with summarized columns
- summary state is `QUERYABLE` or explicitly costed `STALE`

### `SUMMARY_FILTER_SCAN`
Legal when:

- the family reduces scan scope but cannot return exact row identifiers

### `BITMAP_STORAGE_SCAN`
Legal when:

- the stored bitmap family exists and row-identifier semantics are defined

### `BITMAP_COMBINE_SCAN`
Legal when:

- two or more exact index predicates can be combined into a synthetic bitmap
- planner keeps it distinct from physical family `BITMAP`

### `COLUMNSTORE_SCAN`
Legal when:

- projection width and pruning benefit exceed row-store fallback thresholds
- required columns are available from the published columnstore generation
- any mutable buffer plus published-segment union contract is published and
  queryable for the active snapshot

## Costing

### Summary pruning
`cost_summary = C_summary_read * ranges_touched + C_recheck * rows_candidate_est + C_unsum * unsummarized_range_fraction`

### Bitmap storage
`cost_bitmap_storage = C_bitmap_build * bitmap_density + C_heap_page * heap_pages_touched + C_lossy * lossy_container_fraction`

### Synthetic bitmap combine
`cost_bitmap_combine = sum(child_index_costs) + C_bitmap_merge * bitmap_operand_count + C_heap_page * heap_pages_touched`

### Columnstore
`cost_column = C_column_read * bytes_read_est + C_reconstruct * rows_materialized + C_delta * delta_fraction - C_prune_gain * column_bytes_pruned_ratio`

`cost_projection = cost_column - C_layout_gain * projection_layout_count - C_bulk_filter * bulk_filter_gain_est`

## Exactness and Recheck
- `BRIN_SCAN`: always `CANDIDATE_REGION`
- `SUMMARY_FILTER_SCAN`: always `CANDIDATE_REGION`
- `BITMAP_STORAGE_SCAN`: `EXACT_KEY` only for exact containers; otherwise
  `CANDIDATE_REGION`
- `BITMAP_COMBINE_SCAN`: `CANDIDATE_REGION`
- `COLUMNSTORE_SCAN`: `EXACT_ROW` only when projection and visibility can be
  satisfied directly from the published generation, else `CANDIDATE_REGION`

Families that only prune page, granule, row-group, or chunk regions must
publish `native_trust_class = PRUNING_ONLY`; they may not claim row-locator
trust.

## Planner Selection Rules
1. Prefer `BRIN_SCAN` only when `prune_ratio_est` yields a real scan reduction.
2. Do not substitute `BITMAP_COMBINE_SCAN` for physical `BITMAP_STORAGE_SCAN`
   in reporting or costing.
3. Prefer `COLUMNSTORE_SCAN` when byte-pruning and projection savings outweigh
   reconstruction and visibility cost.
4. Treat `unsummarized_range_fraction`, `lossy_container_fraction`, and
   `delta_fraction` as first-class penalties.
5. Projection-aware layouts, row-group pruning, chunk pruning, and bulk filter
   APIs must remain visible in plan evidence when they affect the chosen path.

## Donor-Derived Requirements
This document incorporates the normalized summary, bitmap, and columnar
requirements traced in
`../../planning/SPECIFICATIONS_WORK_PLANNING/INDEX_OPTIMIZER_REFERENCE_TRACE_MATRIX_2026-03-16.md`.

## Cross-Section References
- `BRIN_SPEC.md`
- `BITMAP_SPEC.md`
- `BLOOM_SPEC.md`
- `COLUMNSTORE_SPEC.md`
- `INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
- `INDEX_FAMILY_METRICS_AND_CALIBRATION.md`
