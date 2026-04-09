# ANN_ACCESS_PATH_SELECTION_AND_INDEX_PARITY_MODEL

## Status

Current code-backed authority with required reconstructed parity rule.

## Purpose

This document defines the actual ANN and vector index access-path model in the current planner and the required parity rule that prevents ANN families from becoming secondary or silently ignored optimizer surfaces.

## Query admission trigger

The current planner recognizes vector-nearest-order requests when the query ordering contains at least one of:

1. `VECTOR_L2_DISTANCE(`
2. `VECTOR_COSINE_DISTANCE(`
3. `<->`

This trigger is part of the access-path admission logic for ANN and vector families.

## ANN family inventory recognized by the planner

The current planner recognizes the following catalog index families as ANN or vector-nearest candidates:

1. `VECTOR_FLAT`
2. `VECTOR_BIN_FLAT`
3. `HNSW`
4. `RHNSW_PQ`
5. `RHNSW_SQ`
6. `IVF`
7. `IVF_FLAT`
8. `BIN_IVF_FLAT`
9. `IVF_PQ`
10. `IVF_SQ8`
11. `IVF_SQ8_HYBRID`
12. `ANNOY`
13. `NSG`
14. `DISKANN`
15. `SCANN`
16. `GPU_CAGRA`
17. `NEO4J_VECTOR`

## Planner access-family lowering

Current vector and ANN candidates lower into one of the following planner families:

1. `VECTOR_FLAT_SCAN`
2. `HNSW_SCAN`
3. `IVF_SCAN`
4. `ANN_RERANK_SCAN`
5. `ANN_HYBRID_FALLBACK_SCAN`

The final physical path and plan-node type are derived from this lowered family, not directly from the raw index-type enum.

## Shared metrics packet contract

All ANN-family costing and path comparison must flow through the shared `IndexFamilyMetricsPacket` contract.

The shared metrics envelope currently includes at least:

1. physical family
2. runtime family
3. planner family
4. alias surface flag
5. native metrics mode
6. semantic contract state
7. queryability state
8. metrics confidence class
9. leaf pages
10. height
11. row count estimate
12. live entry count estimate
13. dead fraction
14. bloat ratio
15. recheck ratio estimate
16. coverage fraction
17. maintenance backlog ops
18. publish lag
19. reclaim lag

## ANN family metrics payload

The current planner and statistics path use ANN-specific family metrics that include at least:

1. `candidate_budget_default`
2. `avg_candidates_scanned`
3. `deleted_node_fraction`
4. `orphan_link_fraction`
5. `recall_estimate_at_k`
6. `rerank_fraction`
7. `stale_training_fraction`
8. `segment_coverage_fraction`
9. `bytes_per_live_vector`
10. `growing_fraction`
11. `segment_merge_cost_est`

When available, `native_runtime_metrics` are merged into the family payload and shall be consumed as family-native evidence rather than replaced with generic heuristics.

## Current costing behavior

For ANN-family candidates, the planner currently incorporates at least:

1. average candidates scanned
2. rerank fraction
3. candidate budget
4. recall estimate
5. deleted-node fraction

This means ANN planning is already packet-driven rather than hard-coded as one blanket "vector index" cost.

## Required parity rule

The canonical optimizer rule is:

1. no ANN or vector family may be silently ignored if it produces a valid lowering result and a valid metrics packet
2. ANN families shall be compared through the same shared planner packet contract, with family-native metrics attached where available
3. alias families are allowed only when their packet truthfully marks:
   - runtime family
   - alias surface
   - native metrics mode
   - semantic contract state
   - stronger fail-closed requirements if applicable

This rule exists to prevent a fallback of "optimizer only really uses one preferred vector index family" unless the other families are explicitly non-queryable or fail-closed.

## Exact-truth versus approximate families

The planner differentiates exact and approximate vector behavior.

Current code-backed implications include:

1. `VECTOR_FLAT_SCAN` serves as the exact vector truth lane
2. approximate families rely on recall-oriented metrics and candidate/rerank behavior
3. hybrid fallback lanes exist to reconcile approximate family behavior with exact vector semantics where required

## GPU family boundary

`GPU_CAGRA` exists in the planner family inventory, but current source-backed runtime evidence does not prove a full GPU execution backend in the ScratchBird core.

Therefore:

1. the planner may recognize the family name
2. the family is not allowed to become an ungrounded execution promise
3. admission must remain fail-closed until a real accelerator runtime capability path exists

## Required implementer interpretation

Another agent implementing or extending ANN planning shall:

1. keep every ANN family on the shared packet contract
2. prefer family-native runtime metrics over generic heuristics
3. avoid hard-coding one privileged ANN family as the only optimizer-visible vector index
4. fail closed for any family whose queryability, semantic contract state, or runtime capability is not proven
