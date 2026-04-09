Status: reconstructed_required_with_current_substrate

# Index Family Native Metrics Packet Contract

## Purpose

This document defines the canonical metrics packet every admitted index family must publish so the optimizer treats all implemented index families as primary planning candidates rather than as secondary or advisory-only surfaces.

## Canonical Rule

Every implemented index family shall publish a family-native metrics packet. Absence of metrics is not a reason to demote an implemented index family to secondary planning status. It is an implementation defect to be corrected.

## Required Packet Envelope

Each metrics packet shall include:

- logical index identity
- runtime family
- alias surface if the public name routes to another runtime family
- metrics freshness epoch
- metrics confidence classification
- semantic contract state
- native metrics mode

## Required Cost Fields

Each family shall publish at least:

- entry count or posting count
- page or segment footprint
- resident-memory footprint where applicable
- selectivity estimate inputs
- ordering capability
- exact-match capability
- range capability
- visibility reject rate
- stale or dead-entry rate where applicable
- maintenance debt indicator

## Family-Specific Extensions

Families may add native fields such as:

- graph-layer depth and recall estimate for ANN
- posting-list density for inverted families
- summary precision and false-positive rate for BRIN-like families
- spatial bounding density and split fanout for spatial families
- hash skew and overflow density for hash families

## Beta 2 required named-family extensions

For the Beta 2 emulation families, the packet must also expose the named-family
identity and resolved lowering:

- `named_family`
- `resolved_runtime_family`
- `family_mode`

Required family-native fields:

- `CLICKHOUSE_SET`:
  - `max_rows_limit`
  - `avg_distinct_values_per_granule`
  - `overflowed_granule_fraction`
  - `hyperrectangle_prune_ratio`
- `CLICKHOUSE_TOKENBF_V1` and `CLICKHOUSE_SPARSE_GRAMS`:
  - `tokenizer_mode`
  - `filter_size_bytes`
  - `hash_function_count`
  - `false_positive_ratio_est`
- `CLICKHOUSE_TEXT`:
  - `dictionary_block_count`
  - `posting_block_count`
  - `embedded_posting_fraction`
  - `rare_token_cache_hit_ratio`
- `CLICKHOUSE_HYPOTHESIS`:
  - `supported_predicate_class`
  - `met_granule_fraction`
  - `planner_refusal_count`
- `CLICKHOUSE_VECTOR_SIMILARITY`:
  - `distance_metric`
  - `scalar_quantization`
  - `graph_connectivity`
  - `graph_expansion_add`
- `YBGIN`:
  - `opclass_family`
  - `posting_density_est`
  - `fast_update_supported`
- `MONGODB_COLUMN` and `COLUMNAR`:
  - `path_projection_cardinality`
  - `late_materialization_gain_est`
  - `delta_fraction`
- `VECTOR` and `MILVUS_*`:
  - `distance_metric`
  - `vector_dimension`
  - `accelerator_policy`
  - `recall_estimate_at_k`

## Optimizer Requirement

The optimizer shall consume the typed packet through the shared family-metrics envelope. It shall not silently ignore an implemented family merely because the family exposes different native metrics than ordered B-tree surfaces.

## Freshness Rule

Metrics shall be marked as:

- `CURRENT`
- `STALE_ACCEPTABLE`
- `STALE_DEGRADED`
- `UNUSABLE`

`UNUSABLE` metrics may force conservative planning, but they do not erase the family from candidate generation when the family remains the only conforming access path.

## Visibility Rule

Index metrics never override MGA visibility. Any family may identify candidate tuples only. Final acceptance remains heap or version-truth based.

## Fail-Closed Rule

If a family cannot currently publish one or more native fields, the runtime shall:

- publish the omission explicitly
- keep the family in the candidate set
- mark confidence accordingly
- avoid fabricating false precision

## Non-Guarantees

This file does not require identical metrics across families. It requires parity of optimizer admission and a typed packet with explicit native capability fields.
