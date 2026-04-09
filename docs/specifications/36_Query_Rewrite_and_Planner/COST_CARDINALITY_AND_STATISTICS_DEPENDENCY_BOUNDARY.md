# Cost Cardinality and Statistics Dependency Boundary

Status: current_authority

## Purpose

This file defines the exact dependency boundary between planning, costing, cardinality, and statistics.

## Dependency tiers

| Dependency tier | Required status | Meaning |
| --- | --- | --- |
| structural heuristics | required | planner may use operator-local heuristics and structural defaults when no richer statistics exist |
| single-column selectivity | supported | equality and predicate selectivity may use per-column statistics |
| NDV and frequency data | supported | distinct-count and remaining-frequency information may influence selectivity |
| multivariate equality and dependency modeling | supported where stats exist | planner may use multivariate equality and dependency strength to adjust independence assumptions |
| join selectivity modeling | supported where stats exist | equi-join selectivity may combine pair estimates and multivariate overlap inputs |
| row-width and spill sensitivity | supported | plan cost may include row width, spill expectations, and resource terms |
| total and startup cost comparison | required | winning path selection must compare startup and total cost rather than only path family labels |

## Required costing procedure

1. Determine structural plan shape and legal path family set.
2. Estimate base selectivity for predicates using available statistics or structural defaults.
3. Refine selectivity using NDV, remaining-frequency, or multivariate statistics when those statistics are present and valid for the referenced columns.
4. Estimate join selectivity using pairwise or multivariate overlap logic when available; otherwise fall back to bounded independence heuristics.
5. Derive row-count and row-width estimates.
6. Compute startup cost and total cost for each candidate path.
7. Include spill-sensitive or parallel-sensitive cost components only when the candidate path actually uses those capabilities.
8. Use the computed cost objects for final path comparison and runtime trace reporting.

## Current cost-profile and calibration identity model

The current cost model does not emit anonymous scalar costs. Each cost object is required to carry:
- `formula_profile_id`
- `formula_profile_version`
- `calibration_profile_id`
- `storage_profile`
- `workload_profile`
- `resource_governance_outcome`
- expanded cost terms
- input estimates

The current default derived profile is a governed profile in the `heap_btree` and `mixed_oltp` family. Index-family-specific costing may derive a stricter family profile whose identity includes:
- planner family
- metrics type name
- family metrics version
- metrics confidence class

The planner is not allowed to compare candidates using opaque total-cost scalars without preserving the profile identity and expanded term breakdown that produced those costs.

## Required structural cost inputs

The current cost model depends on these structural inputs where relevant:
- sequence or random page cost
- CPU tuple, index-tuple, and operator cost
- planner page size
- effective cache size
- work-memory budget
- row-width defaults and calibrated row width
- calibrated heap rows per page
- calibrated index entries per page
- calibrated average probe pages
- calibrated duplicate density
- calibrated dead fraction
- calibrated false-positive ratio
- calibrated visibility tuple cost
- parallel setup and per-tuple cost
- spill page cost and spill CPU tuple cost

If a candidate cannot provide required structural inputs for its current scoring path, the planner must reject or downgrade the candidate rather than synthesizing undocumented defaults beyond the declared model.

## Spill and memory-governance costing

The current cost model includes an explicit spill estimator. For spill-sensitive operators the cost procedure is:

1. compute working-set bytes
2. compute effective budget bytes with a minimum safe budget floor
3. determine whether the working set exceeds the budget
4. if not, mark the candidate `IN_MEMORY`
5. if yes:
- mark `SPILL_EXPECTED`
- derive spill bytes
- derive initial runs
- derive merge fanout from budget and page size
- derive spill passes
- derive spill I/O cost
- derive spill CPU cost
6. publish:
- `memory_bytes`
- `memory_budget_bytes`
- `spill_expected`
- `spill_passes`
- `spill_bytes`
- `resource_governance_outcome`

This is part of current planner truth. Spill-sensitive candidates must not be costed as if all plans were in-memory.

## Histogram and selectivity contract

The current VNext selectivity utilities use a `128` bucket histogram contract for range interpolation and same-column intersection selectivity.

The current rule set is:
- histogram interpolation is legal only when the histogram exists and is initialized
- when a caller marks histogram presence as required and the histogram is absent, the planner must reject with a stable reject path instead of silently inventing a histogram
- range selectivity and intersection selectivity are deterministic for the same histogram and predicate bounds

## Cost-comparison and tie-break contract

Final selection is not “pick the cheapest family label.” The current comparison contract is:

1. reject candidates with missing required inputs
2. reject candidates with invalid numeric inputs
3. filter candidates that violate isolation constraints before final comparison
4. compare total cost after full scoring
5. when equal-cost ties remain, tie-break by lexical plan hash
6. if equal-cost comparison needs a plan hash and no valid plan hash exists, reject rather than pick arbitrarily

This tie-break behavior is current implementation authority and must stay stable for deterministic plan choice.

## Statistics dependency rules

1. Statistics guide plan choice; they do not change query correctness.
2. Missing statistics may lower plan quality, but they must not cause the planner to invent unsupported semantics.
3. Statistics may only be used if the planner can map them to the bound columns or join keys of the current query.
4. Multivariate statistics may refine estimates only for the exact covered column sets.
5. When statistics are absent or stale beyond the trusted revision boundary, the planner must fall back to declared heuristics or force plan invalidation and rebuild according to cache policy.

## Family metrics dependency

The current planner can depend on typed `IndexFamilyMetricsPacket` data and family-native payloads when:
- the family metrics packet exists
- the packet version is compatible
- the family identity is known
- the effective queryability state is not invalid

Family-native metrics may refine:
- probe-page estimates
- coverage posture
- dead-entry burden
- duplicate density
- false-positive burden
- visibility reject expectations

No index family is allowed to disappear from costing just because its metrics are weaker. The planner must either:
- cost it with current family-native inputs
- cost it with the documented fallback model
- or mark it non-queryable with an explicit rejection reason

## Planner or statistics boundary

This file owns the dependency contract, not the full statistics subsystem.
Section 36 may depend on:

1. existence of statistics objects
2. their revision identity
3. their supported estimator classes
4. whether they are valid for the current bound query keys

Section 36 does not own collection scheduling, persistence layout, or DDL for those statistics objects.

## Explicit non-guarantees

- no guarantee that every operator family has a complete cost model
- no guarantee that every query has rich statistics coverage
- no guarantee of globally accurate cardinality estimates
- no permission to treat stale or missing statistics as exact truth
- no permission to break deterministic selection by using undocumented ad hoc tie-breaks
