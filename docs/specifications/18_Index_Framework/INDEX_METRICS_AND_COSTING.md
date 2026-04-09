# Index Metrics and Costing

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the required planner-facing metrics packet for every active index family and the rules that prevent secondary indexes from being silently ignored by the optimizer.

## Current code-backed authority

The current optimizer path already exposes a typed planner metrics envelope centered on `IndexFamilyMetricsPacket` with planner path classes including:
- `ORDERED_EXACT`
- `SUMMARY_CANDIDATE`
- `GENERALIZED_SPATIAL`
- `TEXT_SEARCH`
- `ANN`

Current packet and routing surfaces already carry or require:
- `runtime_family`
- `alias_surface`
- `native_metrics_mode`
- `semantic_contract_state`
- `requires_fail_closed_stronger_semantics`
- family payload data used by the planner and advisor surfaces
- canonical named-family identity and variation-safe packet fields even when
  multiple admitted families share one runtime backend or one calibration
  substrate

Current Beta 1 proof also covers shared-backend sibling retention:
- planner candidate formation keeps compatible sibling families visible as
  distinct primary candidates
- statistics publication preserves the admitted named-family identity instead
  of collapsing the packet to runtime-family identity alone

## Non-negotiable rule: no ignored active indexes

No active, admitted, catalog-visible index may be dropped from candidate enumeration merely because it is not the primary or historically preferred family.

The optimizer must do one of the following for every active index that can legally satisfy the predicate, order, or proximity requirement:
1. enumerate it as a candidate with family-native metrics
2. enumerate it with conservative fallback metrics and mark degraded confidence
3. refuse the surface as non-conforming if the family contract is declared supported but does not supply the minimum metrics needed for safe candidate ranking

Silent omission is non-conforming.

## Required common metrics for every family

Every family must provide a planner-visible packet with at least:
- `entry_count`
- `distinct_estimate`
- `null_fraction` where applicable
- `selectivity_estimate`
- `heap_recheck_rate`
- `visibility_reject_rate`
- `maintenance_debt`
- `stats_freshness_epoch`
- `stats_confidence`
- `supports_order_delivery`
- `supports_exact_lookup`
- `supports_range_lookup`
- `supports_k_nearest` where applicable
- `supports_bitmap_or_summary_probe` where applicable
- `cold_load_cost`
- `resident_memory_bytes`
- `estimated_cpu_cost`
- `estimated_io_cost`

## Family-specific required extensions

### Ordered exact families
- fanout or depth
- clustering or locality factor
- split and merge churn
- duplicate-key or posting-list density
- prefix compression effectiveness

### Summary families
- false positive rate or pruning effectiveness
- segment or range coverage density
- summary granularity
- heap recheck amplification

### Spatial and generalized families
- overlap or covering ratio
- bounding-box or penalty distribution
- internal node selectivity confidence
- lossy recheck ratio

### Text and inverted families
- token cardinality
- posting-list size distribution
- lexicon coverage
- ranking support mode
- stemming or normalization profile identity where relevant

### ANN and vector families
- dimension count
- distance metric
- recall estimate at configured probe depth
- graph or centroid connectivity depth
- resident index state
- accelerator residency state if present
- candidate expansion cost at configured `k`

## Metrics freshness and confidence

Every metrics packet must declare one of:
- `fresh_native`
- `fresh_derived`
- `stale_but_usable`
- `missing_conservative`
- `non_conforming`

Planner behavior by state:
- `fresh_native`: normal competitive ranking
- `fresh_derived`: normal ranking with modest confidence penalty
- `stale_but_usable`: candidate retained with explicit stale penalty
- `missing_conservative`: candidate retained with worst-case conservative penalty if semantic contract still allows enumeration
- `non_conforming`: family is fail-closed for optimizer use until metrics are repaired

## Alias and routed family rule

An alias surface may route through another runtime family, but it must still publish:
- the alias identity exposed to DDL and catalog
- the actual runtime family
- whether metrics are native, translated, or heuristic
- whether stronger family-native metrics are still missing

## Beta 2 emulation-family closure

The Beta 2 emulation families added for donor-engine parity must not be forced
into generic heuristics merely because they share a runtime backend.

Required rule for these families:

- `SPATIAL`
- `VECTOR`
- `COLUMNAR`
- `MONGODB_COLUMN`
- `YBGIN`
- `MILVUS_AUTOINDEX`
- `MILVUS_IVF_RABITQ`
- `MILVUS_IVF_HNSW`
- `MILVUS_GPU_IVF_FLAT`
- `MILVUS_GPU_IVF_PQ`
- `MILVUS_GPU_BRUTE_FORCE`
- `CLICKHOUSE_SET`
- `CLICKHOUSE_TOKENBF_V1`
- `CLICKHOUSE_SPARSE_GRAMS`
- `CLICKHOUSE_TEXT`
- `CLICKHOUSE_HYPOTHESIS`
- `CLICKHOUSE_VECTOR_SIMILARITY`

Each must publish:

- `named_family`
- `resolved_runtime_family`
- `family_mode`
- family-native fields listed in its canonical spec

Specific required native payload additions:

- `CLICKHOUSE_SET`:
  - `overflowed_granule_fraction`
  - `avg_distinct_values_per_granule`
  - `hyperrectangle_prune_ratio`
- `CLICKHOUSE_TOKENBF_V1` and `CLICKHOUSE_SPARSE_GRAMS`:
  - `tokenizer_mode`
  - `false_positive_ratio_est`
  - `granule_elimination_gain_est`
  - `extractor_cpu_cost_est`
- `CLICKHOUSE_TEXT`:
  - `dictionary_block_count`
  - `posting_block_count`
  - `embedded_posting_fraction`
  - `front_coding_gain_est`
- `CLICKHOUSE_HYPOTHESIS`:
  - `supported_predicate_class`
  - `met_granule_fraction`
  - `deterministic_skip_ratio`
- `CLICKHOUSE_VECTOR_SIMILARITY`:
  - `scalar_quantization`
  - `graph_connectivity`
  - `graph_expansion_add`
  - `bytes_per_vector`
- `YBGIN`:
  - `opclass_family`
  - `posting_density_est`
  - `fast_update_supported`
  - `backfill_progress_fraction`
- `MONGODB_COLUMN` and `COLUMNAR`:
  - `path_projection_cardinality`
  - `late_materialization_gain_est`
  - `delta_fraction`
- `VECTOR` and `MILVUS_*` families:
  - `resolved_runtime_family`
  - `distance_metric`
  - `vector_dimension`
  - `accelerator_policy`
  - `recall_estimate_at_k`

The optimizer must use those fields in ranking and diagnostics. It is
non-conforming to publish them only for operator inspection while continuing to
rank the family with generic fallback costs forever.

## Named-family parity over shared backends

Every admitted named `IndexType` is independently primary for planner use even
when multiple names share one runtime backend or one base metrics calibration
family.

That means:
- each admitted named family publishes its own planner-visible packet and
  observability identity
- sibling families may reuse one family metrics substrate only when the packet
  also carries the named family identity and any variation-specific extensions,
  penalties, or credits
- shared calibration never authorizes the optimizer to skip, hide, or
  hint-gate a named family
- family variation fields must be sufficient for operator inspection,
  maintenance, and deterministic costing

## Candidate generation algorithm

1. enumerate all catalog-visible active indexes for the relation
2. discard only indexes that are semantically incapable of serving the query shape
3. request metrics for every remaining candidate
4. convert stale or missing metrics to explicit conservative penalties
5. keep the candidate unless semantic incapability or fail-closed non-conformance requires exclusion
6. rank by cost, confidence, and semantic exactness
7. emit diagnostics when a family is retained only under degraded metrics

## Observability requirement

The same typed metrics packet consumed by the optimizer must be queryable through observability and operator surfaces. Planner-visible metrics are not allowed to exist only in private memory.

## Required reconstructed behavior

The rebuilt commercial-grade contract requires every supported family to converge on family-native metrics rather than indefinite generic heuristics. If a family is marketed or documented as supported, its planner packet must reach family-native quality.

## Audit lookup anchors

Representative audit anchors for this file are:
- `shared_metrics_envelope`
- `metricsTypeForLowering(`
