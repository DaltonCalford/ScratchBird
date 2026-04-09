# Optimizer Index Family Parity and No Ignored Index Rule

Status: current_authority_with_reconstructed_expansion

## Purpose

This file defines the optimizer-side rule that every supported index family must participate in planning at a parity level appropriate to its semantics, and that no active index may be silently ignored merely because another family historically dominated the optimizer.

It now also defines the concrete metrics envelope, family-lowering path, and planner-admission rules already visible in current code.

## Core rule

If a family is catalog-visible, runtime-supported, and semantically capable of serving a query shape, the optimizer must either:

1. enumerate it as a candidate path
2. enumerate it under a degraded or conservative-cost mode
3. reject it with explicit diagnostics because its queryability or metrics contract is non-conforming

Silent omission is a planner defect.

## Current normalization rule

The optimizer is not allowed to use raw catalog `IndexType` values directly as the final planning vocabulary. It must normalize them through the family-lowering layer into planner families and access-path contracts.

The current normalization model includes at minimum:
- B-tree-like ordered access families
- hash-like exact access families
- BRIN-style summary access families
- bloom or zonemap style summary-filter families
- generalized spatial families such as GiST, SP-GiST, and R-tree-adjacent surfaces
- vector families
- text and inverted families
- alias or routed compatibility families

## Current code-backed metrics envelope

The current optimizer already consumes an `IndexFamilyMetricsPacket` with these shared fields:

- `physical_family`
- `planner_family`
- `queryability_state`
- `metrics_last_refresh_xid`
- `metrics_confidence_class`
- `leaf_pages`
- `height`
- `row_count_est`
- `live_entry_count_est`
- `dead_fraction`
- `bloat_ratio`
- `recheck_ratio_est`
- `correlation`
- `coverage_fraction`
- `maintenance_backlog_ops`
- `publish_lag_xids`
- `reclaim_lag_xids`
- `family_metrics_version`
- `family_metrics_type`
- `family_metrics_payload`

The current packet payload also includes a shared metrics envelope with fields such as:

- `runtime_family`
- `alias_surface`
- `native_metrics_mode`
- `semantic_contract_state`
- `requires_fail_closed_stronger_semantics`
- `queryability_state`
- and a `family_metrics.native_runtime_metrics` subobject when native family counters are available

## Queryability states

The planner must treat `queryability_state` as a first-class admission signal.

Current states are:

- `QUERYABLE`
- `LIMITED`
- `INVALID`
- `UNKNOWN`

Required behavior:

- `QUERYABLE`: enumerate and rank normally
- `LIMITED`: enumerate conservatively and preserve diagnostics explaining the limit
- `INVALID`: reject explicitly with traceable reason
- `UNKNOWN`: either conservatively cost or reject depending on the semantic strength required by the query

## Metrics confidence and family type

The planner must also respect:

- metrics confidence class
- family metrics type
- presence or absence of family-native payload metrics

Required rule:

- weaker confidence or missing family-native metrics cannot be hidden; they must degrade ranking confidence, inflate conservative cost, or force fail-closed rejection when stronger semantics are required

## Current family-lowering detail recovered from code

The family-lowering layer currently uses catalog and opclass evidence, not only family names, to decide:
- exactness class
- visibility-enforcement class
- recheck requirements
- ordering support
- covering support
- parameterization support
- queryability validity

That means optimizer parity is partly a catalog and opclass contract problem. A family is not truly “supported” unless the lowering path and its opclass evidence agree.

## Current planner behavior already visible in code

Current code already proves that the planner:

- loads family metrics packets per index
- caches packet and payload objects per relation during planning
- parses `family_metrics_payload`
- derives access-path queryability from metrics queryability state
- checks for family-metric presence before some ordered-family costing paths
- rejects or degrades candidate bundles when required metrics are missing

This means planner parity is not aspirational only. It is already a live code path and must remain explicit in canon.

## Parity vocabulary

- `semantic_parity`: the family can express the same query class as competing families
- `metrics_parity`: the family publishes enough metrics for safe ranking
- `diagnostic_parity`: the planner can explain why the family won or lost
- `execution_parity`: runtime execution can realize the chosen plan without fallback surprises

A family must not be described as optimizer-supported unless all required parity classes for its advertised query shapes are met.

## Required planner behavior

1. gather all active indexes for the relation and predicate or order shape
2. classify each candidate by planner family and runtime family
3. load the shared index-family metrics packet
4. determine queryability and confidence state
5. obtain family-native or conservative metrics from the payload
6. rank all admissible candidates under the same top-level optimization objective
7. preserve diagnostics explaining rejected, degraded, or fail-closed families

## Secondary index rule

A secondary index remains a first-class planner candidate whenever it can satisfy:

- exact lookup
- range pruning
- ordering delivery
- text or ANN candidate generation
- bitmap or summary pruning
- spatial or geometric candidate generation

The optimizer must not give exclusive structural preference to a primary or traditional B-tree family when another family is better suited to the query.

## Alias-surface rule

If a named index surface is currently routed through another runtime family, the planner must not overclaim parity.

Required rule:

- alias surfaces may be marked `LIMITED`
- stronger semantics may require fail-closed rejection
- routed alias status must remain visible in diagnostics and metrics payloads

## Equality-cost tie-break visibility rule

When multiple admissible families survive to equal-cost comparison, the planner must preserve the deterministic tie-break path in diagnostics rather than exposing only the winning family name.

## Vector-family parity rule

For vector and ANN families, parity requires at minimum:

- candidate generation metrics
- exact-rerank or post-filter semantics when promised
- MGA visibility reject metrics
- cold-load versus warm-resident costing where residency is part of the family design
- accelerator-readiness distinction when GPU acceleration is optional

A vector family with no resident-state visibility or no family-native metrics is not optimizer-complete just because the parser admits the index name.

## Family-specific parity expectations

- ordered exact families: must compete on equality, range, ordering, and partial-order delivery
- summary families: must compete on pruning effectiveness and recheck cost
- spatial families: must compete on geometric predicate support and lossy recheck rate
- text families: must compete on token-selectivity, ranking, and posting traversal cost
- ANN families: must compete on recall, latency, cold-start cost, exact fallback cost, and residency readiness

## Required reconstructed behavior

Commercial-grade ScratchBird canon requires all shipped index families to reach optimizer parity sufficient to avoid implemented-but-never-chosen status.

A family that exists only as storage or DDL surface but lacks planner parity is not complete.

## Non-authority and rejection rules

The following claims are incorrect:

- a catalog-visible index may be silently omitted because another family is historically favored
- alias surfaces may be described as fully optimizer-parity-complete when their runtime family is still routed and limited
- vector families may be costed as warm and accelerator-ready when residency or accelerator readiness is absent
- missing family-native metrics may be hidden behind a generic "index stats available" claim
- unsupported operator strategy or missing opclass support may not be treated as if the index family never existed
