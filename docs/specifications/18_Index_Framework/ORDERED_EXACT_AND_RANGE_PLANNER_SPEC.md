# Ordered Exact and Range Planner Spec

## Purpose
Define the planner contract for ordered exact and range families:

- `BTREE`
- `HASH`
- `LSM`
- ordered aliases that lower to those families

## Hard Invariants
1. `HASH` is equality-only and must never claim ordered output.
2. `HASH` must always carry key recheck semantics unless the runtime persists
   enough key material to prove collision-free equality for the concrete mode.
3. `LSM` equality and range behavior are distinct path families.
4. Skip-scan is not universal; first-wave legality is `BTREE` only.

## Canonical Paths
- `BTREE_EQ_SCAN`
- `BTREE_RANGE_SCAN`
- `BTREE_ORDERED_SCAN`
- `BTREE_SKIP_SCAN`
- `HASH_EQ_SCAN`
- `LSM_EQ_SCAN`
- `LSM_RANGE_SCAN`
- `LSM_ORDERED_RANGE_SCAN`

## Capability Object
Every ordered exact family must publish:

- `supports_exact`
- `supports_range`
- `supports_ordered_output`
- `supports_reverse_order`
- `supports_covering_payload`
- `supports_prefix_search`
- `supports_skip_scan`
- `supports_interval_descriptors`
- `supports_cluster_locality_model`
- `supports_prefetch_or_multi_range`
- `supports_encoded_space_pruning`
- `recheck_mode`
- `ordering_comparator_id`

`recheck_mode` values:

- `VISIBILITY_ONLY`
- `KEY_AND_VISIBILITY`
- `RESIDUAL_QUALS_ONLY`
- `KEY_VISIBILITY_AND_RESIDUAL`

## Predicate Interval Model
Range-capable families shall consume a formal interval descriptor rather than
raw predicate fragments. The descriptor must preserve:
- ordered key column sequence
- exact lower and upper bounds
- open or closed endpoint semantics
- disjoint interval unions
- partition-pruning consequences
- encoded-space bounds when the family uses a score or encoded-order space

## Path Legality

### `BTREE_EQ_SCAN`
Legal when:

- equality predicates bind a leading prefix
- comparator and null-order semantics match
- partial-index predicate implication is satisfied

### `BTREE_RANGE_SCAN`
Legal when:

- lower and or upper bounds are representable by the key comparator
- range predicates preserve column ordering semantics

### `BTREE_ORDERED_SCAN`
Legal when:

- requested order matches key order or exact reverse order
- no later executor phase destroys order before consumption

### `BTREE_SKIP_SCAN`
Legal when:

- leading key is unbound
- suffix predicate selectivity is strong enough
- `skip_group_count` and `records_per_group` statistics are available or a
  conservative fallback threshold passes

### `HASH_EQ_SCAN`
Legal when:

- all searchable predicates are equality on the hashed key
- key recheck cost remains within accepted bounds

### `LSM_*`
Legal when:

- the published run set is queryable
- duplicate suppression and tombstone semantics are available for the current
  snapshot
- ordered variants are enabled only when the runtime can merge published runs
  into stable key order

## Metrics Packet
- `avg_probe_pages`
- `avg_range_pages_per_row`
- `duplicate_density`
- `prefix_selectivity`
- `skip_group_count`
- `overflow_chain_depth`
- `run_count`
- `level_count`
- `tombstone_fraction`
- `L0_run_count`
- `sort_avoidance_gain_est`

## Costing

### Point equality
`cost_eq = C_start + C_probe * avg_probe_pages + C_recheck * recheck_ratio_est + C_heap * heap_fetch_est`

When clustered-primary and secondary-plus-row-fetch alternatives both exist, the
cost model shall expose that distinction explicitly rather than hiding it inside
one generic equality probe term.

### Ordered or range access
`cost_range = C_start + C_page * pages_touched_est + C_row * rows_candidate_est + C_sort_avoid * sort_saved_rows`

`cost_prefetch = cost_range - C_prefetch_gain * prefetchable_page_fraction - C_early_stop * early_stop_gain_est`

### Skip-scan penalty
`cost_skip = cost_eq * skip_group_count + C_skip_restart * skip_group_count`

### Hash overflow penalty
`cost_hash = cost_eq + C_overflow * overflow_chain_depth + C_hash_recheck * recheck_ratio_est`

`cost_secondary_lookup = cost_eq + C_secondary_row_fetch * secondary_lookup_fraction`

### LSM merge penalty
`cost_lsm = C_probe * run_count + C_merge * log2(1 + level_count) + C_tomb * tombstone_fraction + C_l0 * L0_run_count`

`cost_mrr = cost_secondary_lookup - C_mrr_gain * cluster_locality_gain_est`

## Exactness and Recheck
- `BTREE_*`: `EXACT_KEY`, `requires_recheck = visibility only`
- `HASH_EQ_SCAN`: `EXACT_KEY`, `requires_recheck = key and visibility`
- `LSM_EQ_SCAN`: `EXACT_KEY`, `requires_recheck = visibility only`
- `LSM_RANGE_SCAN`: `EXACT_KEY`, `requires_recheck = visibility only`
- `LSM_ORDERED_RANGE_SCAN`: may satisfy order only after duplicate-suppression
  stability is proven

## Covering and Index-Only Rules
Index-only behavior is legal only when:

- covering payload is persisted
- visibility checks can be satisfied without contradictory stale-entry debt
- family contract declares exact reconstructability

`HASH` is not first-wave index-only.

## Planner Selection Rules
1. Preserve both `BTREE` and `HASH` equality candidates when their cost, order,
   or recheck properties differ materially.
2. Prefer `BTREE_ORDERED_SCAN` or `LSM_ORDERED_RANGE_SCAN` over sort-plus-scan
   only when `sort_avoidance_gain_est` is positive and ordered semantics are
   validated.
3. Use conservative thresholds when metrics are stale or confidence is low.
4. Interval-descriptor legality and partition pruning must be resolved before
   cost comparison begins.
5. If the family can exploit encoded-order or sparse-primary bounds for early
   termination, that behavior must be explicit in both costing and runtime-plan
   evidence.

## Donor-Derived Requirements
This document incorporates the normalized ordered-family requirements traced in
`../../planning/SPECIFICATIONS_WORK_PLANNING/INDEX_OPTIMIZER_REFERENCE_TRACE_MATRIX_2026-03-16.md`.

## Cross-Section References
- `BTREE_SPEC.md`
- `HASH_SPEC.md`
- `LSM_TREE_SPEC.md`
- `INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
- `INDEX_FAMILY_METRICS_AND_CALIBRATION.md`
