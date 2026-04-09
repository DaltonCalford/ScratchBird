# Generalized Search and Spatial Planner Spec

## Purpose
Define the planner contract for:

- `GIST`
- `SPGIST`
- `RTREE`

## Hard Invariants
1. `GIST` and `SPGIST` are opclass-driven families; searchable predicates come
   from bound strategy maps, not the family name alone.
2. `RTREE` is a narrower built-in spatial family in first-wave delivery.
3. Lossy search is legal only with explicit `needs_recheck`.
4. Ordered nearest-path planning is legal only for validated lower-bound
   support functions.

## Canonical Paths
- `GIST_SCAN`
- `SPGIST_SCAN`
- `RTREE_SCAN`
- `GIST_NEAREST_SCAN`
- `SPGIST_NEAREST_SCAN`
- `RTREE_NEAREST_SCAN`

## Metadata Contract
Per indexed key column, persist:

- `opclass_id`
- `opclass_version`
- `strategy_map_version`
- `storage_format_version`
- `family_options_version`
- `compatibility_state_version`
- `partial_predicate_id` when the family is filtered or conditional
- `scope_descriptor` when wildcard or encoded-space scope changes legality

Open must refuse queryability if these do not match the runtime.

## Strategy and Search Descriptor
Planner and executor must exchange a typed search descriptor containing:

- family method
- column ordinal
- strategy number
- serialized query datum
- optional order-by strategy
- optional distance limit
- `allow_lossy`
- `want_recheck_flag`
- `want_distance`
- snapshot or visibility context
- encoded-space bounds where the family uses one
- optional bitset or prefilter handle

## Metrics Packet
- `overlap_ratio`
- `penalty_growth_factor`
- `all_the_same_fraction`
- `branch_skew`
- `candidate_amplification`
- `nearest_lb_tightness`
- `distance_recheck_ratio`

## Path Legality

### `GIST_SCAN` and `SPGIST_SCAN`
Legal when:

- a bound opclass maps the predicate to a stable strategy number
- required support functions are present and validated

### `RTREE_SCAN`
Legal when:

- key encoding is within declared `RTREE` scope
- predicate is one of `overlaps`, `contains`, `contained_by`, or `equal`

### `*_NEAREST_SCAN`
Legal when:

- lower-bound correctness is validated
- final exact ordering can be restored after recheck when needed

## Costing

### Generalized predicate search
`cost_generalized = C_node * nodes_visited_est + C_candidate * rows_candidate_est + C_recheck * recheck_ratio_est + C_overlap * overlap_ratio`

### Nearest search
`cost_nearest = C_queue * queue_growth_est + C_node * nodes_visited_est + C_rerank * rows_candidate_est + C_lb * (1 - nearest_lb_tightness)`

### SP-GiST partition skew
`cost_partition = cost_generalized + C_skew * branch_skew + C_same * all_the_same_fraction`

`cost_encoded_space = cost_generalized - C_encoded_prune * encoded_space_prune_gain`

## Exactness and Recheck
- ordinary `GIST_SCAN`, `SPGIST_SCAN`, `RTREE_SCAN`: usually
  `CANDIDATE_REGION`, exactness depends on opclass support
- nearest paths: `LOWER_BOUND_ORDERED` unless the family proves exact distance
  ordering at access time

## Planner Selection Rules
1. Planner must route SQL operators through bound strategy maps.
2. Planner may not default generalized search to a hardcoded overlap predicate.
3. Index-only behavior is legal only when the opclass or family can reconstruct
   exact values.
4. If lower-bound certification is missing, nearest paths are forbidden.
5. Compatibility filtering for multikey, partial, wildcard, sparse, or scoped
   states is mandatory before costing.
6. Family-specific access operators may not be collapsed into one generic scan
   surface if that would hide legality, scope, or distance semantics.

## Donor-Derived Requirements
This document incorporates the normalized generalized-search and spatial
requirements traced in
`../../planning/SPECIFICATIONS_WORK_PLANNING/INDEX_OPTIMIZER_REFERENCE_TRACE_MATRIX_2026-03-16.md`.

## Cross-Section References
- `GIST_SPEC.md`
- `SPGIST_SPEC.md`
- `SPATIAL_SPEC.md`
- `OPCLASS_DEFINITIONS.md`
- `INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md`
