# Index Planner Path Taxonomy and Exactness

## Purpose
Define the canonical planner path names, exactness classes, recheck rules, and
lowering contract for all ScratchBird index families.

## Scope
- canonical path names
- access-path descriptor fields
- exactness and recheck classes
- planner lowering rules
- enumeration and fallback rules

## Hard Invariants
1. Generic `INDEX_SCAN` is not sufficient as the long-term planner contract.
2. Synthetic bitmap combination and physical bitmap-family access are distinct
   path families.
3. Ordered output, coverage, parameterization, and exactness must be explicit
   fields, not inferred from family name alone.
4. ANN and generalized nearest paths must declare candidate budget and
   lower-bound semantics before enumeration.
5. Pruning-only and filter-accelerator families are explicit planner contracts;
   they may not masquerade as exact tuple locators.

## Access Path Descriptor
The canonical access-path descriptor schema is owned by this document. All
other canonical specs that consume path properties must reference this schema
instead of defining competing field lists.

Every planner-visible index path must persist:

- `planner_family`
- `path_name`
- `family_tags`
- `taxonomy_version`
- `exactness_class`
- `native_trust_class`
- `requires_recheck`
- `queryability_state`
- `maintenance_state_class`
- `supports_ordering`
- `ordering_keys`
- `ordering_class`
- `ordered_prefix_length`
- `order_complete`
- `storage_order_class`
- `early_stop_capability`
- `supports_covering`
- `supports_parameterization`
- `required_outer_relation_indexes`
- `coverage_fraction`
- `rows_candidate_est`
- `false_positive_ratio`
- `cost_confidence`
- `candidate_budget`
- `visibility_enforcement`
- `locator_granularity`
- `pruning_granularity_class`
- `projection_layout_id`
- `storage_layer_shape`
- `collector_specialization_id`
- `clustered_lookup_shape`
- `parallel_property_signature`
- `family_metrics_version`
- `metrics_confidence_class`
- `native_trust_class`
- `locator_granularity`
- `maintenance_state_class`

`visibility_enforcement` values:

- `INDEX_NATIVE`
- `POST_FILTER`
- `HYBRID`

`native_trust_class` values:

- `NATIVE_EXACT`
- `NATIVE_WITH_RECHECK`
- `PRUNING_ONLY`
- `FILTER_ACCELERATOR`
- `APPROX_CANDIDATE`

`locator_granularity` values:

- `ROW_TID`
- `ROW_ID_SET`
- `POSTING_SET`
- `PAGE_RANGE`
- `ROW_GROUP`
- `SEGMENT`
- `PARTITION`
- `PROJECTION`
- `NONE`

## Exactness Classes
- `EXACT_ROW`: access returns rows that satisfy the indexed predicate subject
  only to MGA visibility
- `EXACT_KEY`: key equality or order is exact, but row visibility still needs
  heap confirmation
- `CANDIDATE_REGION`: access returns candidate pages, row groups, regions, or
  postings that require predicate recheck
- `LOWER_BOUND_ORDERED`: returned order is based on a lower bound and final
  exact ordering requires rerank or recheck
- `APPROX_TOPK`: access returns bounded approximate top-`K` candidates that must
  state recall semantics explicitly

## Canonical Combination Matrix

The following combinations are legal by default:

| `exactness_class` | `native_trust_class` | `locator_granularity` | `visibility_enforcement` |
| --- | --- | --- | --- |
| `EXACT_ROW` | `NATIVE_EXACT` | `ROW_TID`, `ROW_ID_SET`, `PROJECTION` | `INDEX_NATIVE`, `HYBRID` |
| `EXACT_KEY` | `NATIVE_WITH_RECHECK` | `ROW_TID`, `ROW_ID_SET`, `POSTING_SET` | `INDEX_NATIVE`, `HYBRID` |
| `CANDIDATE_REGION` | `PRUNING_ONLY`, `FILTER_ACCELERATOR` | `PAGE_RANGE`, `ROW_GROUP`, `SEGMENT`, `PARTITION`, `POSTING_SET`, `PROJECTION` | `POST_FILTER`, `HYBRID` |
| `LOWER_BOUND_ORDERED` | `APPROX_CANDIDATE` | `ROW_ID_SET`, `POSTING_SET`, `SEGMENT`, `PARTITION` | `POST_FILTER`, `HYBRID` |
| `APPROX_TOPK` | `APPROX_CANDIDATE` | `ROW_ID_SET`, `SEGMENT`, `PARTITION`, `NONE` | `POST_FILTER`, `HYBRID` |

Any combination outside this matrix is forbidden unless a family-specific
section-18 spec explicitly overrides it.

## Canonical Path Families

### Ordered exact and range
- `BTREE_EQ_SCAN`
- `BTREE_RANGE_SCAN`
- `BTREE_ORDERED_SCAN`
- `BTREE_SKIP_SCAN`
- `HASH_EQ_SCAN`
- `LSM_EQ_SCAN`
- `LSM_RANGE_SCAN`
- `LSM_ORDERED_RANGE_SCAN`

### Summary and candidate
- `BRIN_SCAN`
- `SUMMARY_FILTER_SCAN`
- `BITMAP_STORAGE_SCAN`
- `BITMAP_COMBINE_SCAN`
- `COLUMNSTORE_SCAN`

### Generalized and spatial
- `GIST_SCAN`
- `SPGIST_SCAN`
- `RTREE_SCAN`
- `GIST_NEAREST_SCAN`
- `SPGIST_NEAREST_SCAN`
- `RTREE_NEAREST_SCAN`

### Text and inverted
- `GIN_FILTER_SCAN`
- `TEXT_BITMAP_SCAN`
- `TEXT_SCORE_SCAN`
- `TEXT_RECHECK_SCAN`

### Vector and ANN
- `VECTOR_FLAT_SCAN`
- `HNSW_SCAN`
- `IVF_SCAN`
- `ANN_RERANK_SCAN`
- `ANN_HYBRID_FALLBACK_SCAN`

## Lowering Rules
1. Every catalog family must lower into one planner family before enumeration.
2. Lowering must also resolve path candidates, exactness class, and recheck
   defaults.
3. Planner may not collapse all non-`LSM` families into one generic index path.
4. Planner may not infer ordered output only from `BTREE`.
5. Planner may not enumerate an ANN path without a candidate budget.
6. Planner may not enumerate a generalized nearest path unless the family or
   opclass validates lower-bound correctness.
7. Planner may not enumerate any family without explicit trust class, locator
   granularity, and maintenance-state classification.

## Enumeration Rules
1. Families in `FAILED` or non-queryable state are ineligible.
2. `LOW` metric confidence requires conservative penalties or fallback
   thresholds.
3. Multiple candidate paths may remain alive for a relation; one early winner
   may not erase materially different order or exactness properties.
4. Planner-visible alias families are lowered before costing.

## Fallback Rules
- no typed family metrics -> conservative fallback or no plan selection
- no recheck contract -> path not planner-visible
- no order guarantee -> path may not satisfy `ORDER BY`
- no covering proof -> path may not claim index-only behavior
- no candidate budget or recall contract for ANN -> path forbidden
- no trust-class or locator-granularity declaration -> path forbidden
- maintenance state incompatible with declared trust class -> path forbidden

## Donor-Derived Requirements
This document incorporates the normalized trust-model and taxonomy
requirements traced in
`../../planning/SPECIFICATIONS_WORK_PLANNING/INDEX_OPTIMIZER_REFERENCE_TRACE_MATRIX_2026-03-16.md`.

## Cross-Section References
- `INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md`
- `INDEX_FAMILY_METRICS_AND_CALIBRATION.md`
- `INDEX_MGA_PUBLICATION_AND_RECLAIM.md`
- `ORDERED_EXACT_AND_RANGE_PLANNER_SPEC.md`
- `SUMMARY_BITMAP_COLUMNSTORE_PLANNER_SPEC.md`
- `GENERALIZED_SEARCH_AND_SPATIAL_PLANNER_SPEC.md`
- `INVERTED_TEXT_AND_RANKING_PLANNER_SPEC.md`
- `VECTOR_ANN_PLANNER_SPEC.md`

## Update 2026-03-28: planner-proof boundary

This document remains the canonical target planner taxonomy, but the current re-proven code surface is narrower.

Directly re-proven in this pass:
- executor runtime routing distinguishes family behavior instead of treating all indexes as one generic scan shape
- current generic runtime buckets visible in code are:
  - ordered or exact and range-capable: `BTREE` and BTREE-mapped aliases
  - exact equality only: `HASH` and HASH-mapped aliases
  - generalized or spatial: `RTREE`, `GIST`, `SPGIST`
  - summary or block-range: `BRIN`
  - bitmap-set: `BITMAP`
  - specialized columnar: `COLUMNSTORE`
  - inverted or text-specialized: `GIN`, `FULLTEXT`, and other `INVERTED` aliases
  - approximate vector or ANN: `HNSW` and vector aliases
  - file-based merge tree: `LSM`

Current bounded or unproven boundary:
- this pass does not prove a closed planner-family descriptor with all fields listed above
- this pass does not prove one centralized planner enumeration contract matching every taxonomy row in this document
- several exactness and trust-class statements here remain broader than the directly re-proven executor and factory surface

Practical normalization:
- treat this document as the target planner contract
- treat executor routing and `IndexFactory` capability lookup as the current code-backed minimum authority

## Update 2026-03-28: explicit proof-state split

Current proof-state split for section `18` planner claims:
- `proven_now`:
  - executor family routing boundaries
  - specialized-operator fail-closed behavior
  - generic range-support boundaries for B-tree, Hash, BRIN, spatial, and LSM runtime groups
- `partial`:
  - family-to-path grouping
  - exactness implications inferred from executor behavior
- `target_state_only`:
  - full access-path descriptor field closure
  - one unified planner-owned exactness and trust-class registry

This file remains canonical, but its richer taxonomy is a target-state contract unless separately re-proven.
