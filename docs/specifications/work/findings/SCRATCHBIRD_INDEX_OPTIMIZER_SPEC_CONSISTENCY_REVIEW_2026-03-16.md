# ScratchBird Index Optimizer Spec Consistency Review

Date: 2026-03-16

Status: Active

## Purpose

Review the newly integrated donor-derived optimizer and index requirements
against:
- the existing canonical section 18 and section 23 specification set
- the current live `ScratchBird` implementation surfaces

The goal is to isolate real contradictions and schema mismatches before a full
execution workplan is derived.

## Executive Summary

The new borrowed material is directionally sound and does not introduce any
conflict with ScratchBird’s core invariants around SBLR execution, MGA, UUID
identity, or parser boundaries.

However, six concrete consistency issues remain:

1. planner front-door request/result schemas are incomplete relative to the new
   donor-derived obligations
2. access-path and runtime-plan field schemas are split across docs and no
   longer align cleanly
3. index lifecycle and publication-state mappings are incomplete
4. one ordered-family cost formula references an undefined metric
5. join-search now partially owns base-relation family composition logic that
   should stay anchored in access-path enumeration contracts
6. live implementation is materially behind the expanded canonical contracts

These are fixable specification and migration issues, not architectural
failures.

## Findings

### 1. Planner Front Door Schema Is Incomplete

Severity: High

Problem:
- The canonical front door now requires storage-layer topology,
  publication-state inputs, and collector or execution-intent classes, but the
  normative request and result schemas do not actually contain those fields.

Evidence:
- Request schema fields stop at `what_if_context` in
  [PLANNER_FRONT_DOOR_AND_STATEMENT_PLANNING_API.md](../../23_SBLR_VM_Compiler_and_Executor/PLANNER_FRONT_DOOR_AND_STATEMENT_PLANNING_API.md#L67).
- Result schema fields stop at fallback and rejection stream in
  [PLANNER_FRONT_DOOR_AND_STATEMENT_PLANNING_API.md](../../23_SBLR_VM_Compiler_and_Executor/PLANNER_FRONT_DOOR_AND_STATEMENT_PLANNING_API.md#L90).
- The donor-derived obligations that require more are in
  [PLANNER_FRONT_DOOR_AND_STATEMENT_PLANNING_API.md](../../23_SBLR_VM_Compiler_and_Executor/PLANNER_FRONT_DOOR_AND_STATEMENT_PLANNING_API.md#L190).

Why this matters:
- An implementation plan cannot safely introduce these new planning concerns if
  the authoritative request/result schema does not declare them explicitly.

Required fix:
- Extend PF02 and PF03 with the missing fields rather than leaving them in the
  donor-derived prose only.

### 2. Access-Path Descriptor, Candidate Bundle, and RuntimePlan Schemas No Longer Align

Severity: High

Problem:
- The path-property model now diverges across section 18 taxonomy, section 23
  access-path ordering, join-search inputs, and runtime-plan integration.
- Some properties are required in one place but absent in another.

Evidence:
- Every path is required to carry `storage order class`, `early-stop
  capability`, pruning class, and projection layout in
  [ACCESS_PATH_ORDERING_AND_UPPER_STAGE_PLANNING.md](../../23_SBLR_VM_Compiler_and_Executor/ACCESS_PATH_ORDERING_AND_UPPER_STAGE_PLANNING.md#L61).
- Candidate bundles and runtime plans require `native_trust_class`,
  `locator_granularity`, `maintenance_state_class`, `storage_layer_shape`, and
  `collector_specialization_id` in
  [INDEX_FAMILY_ACCESS_PATH_AND_RUNTIMEPLAN_INTEGRATION.md](../../23_SBLR_VM_Compiler_and_Executor/INDEX_FAMILY_ACCESS_PATH_AND_RUNTIMEPLAN_INTEGRATION.md#L40).
- Join-search candidate bundles additionally require
  `clustered-vs-secondary lookup shape` in
  [JOIN_SEARCH_AND_METHOD_ENUMERATION.md](../../23_SBLR_VM_Compiler_and_Executor/JOIN_SEARCH_AND_METHOD_ENUMERATION.md#L64).
- Section 18 taxonomy only defines `native_trust_class`,
  `locator_granularity`, and `maintenance_state_class` on the access-path
  descriptor, not `storage_layer_shape`, `collector_specialization_id`,
  `storage order class`, or `early-stop capability`, in
  [INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md](../../18_Index_Framework/INDEX_PLANNER_PATH_TAXONOMY_AND_EXACTNESS.md#L22).

Why this matters:
- Different implementation teams could satisfy different documents and still
  produce mutually incompatible payloads.
- There is also no explicit legal-combination matrix for
  `exactness_class x native_trust_class x locator_granularity x
  visibility_enforcement`.

Required fix:
- Establish one canonical descriptor schema and one legal-combination matrix,
  then make section 18 and section 23 reference that same schema verbatim.

### 3. Index Lifecycle and Publication-State Mapping Is Incomplete

Severity: Medium

Problem:
- New lifecycle states were added to both the high-level index architecture and
  the MGA publication model, but the short-term mapping was not extended to
  cover them.

Evidence:
- Index architecture now defines `growing_mutable`, `sealed_pending_index`,
  `indexed_pending_publish`, `stale_queryable`, `invalid`, and `dropping` in
  [INDEX_ARCHITECTURE.md](../../18_Index_Framework/INDEX_ARCHITECTURE.md#L74).
- MGA publication defines `GROWING`, `SEALED_PENDING_INDEX`,
  `INDEXED_PENDING_PUBLISH`, and `STALE_QUERYABLE`, but the short-term mapping
  only covers `BUILDING`, `PUBLISHED`, `RETIRING`, and `FAILED` in
  [INDEX_MGA_PUBLICATION_AND_RECLAIM.md](../../18_Index_Framework/INDEX_MGA_PUBLICATION_AND_RECLAIM.md#L79).

Why this matters:
- A planner- or catalog-facing implementation cannot tell how the new states
  map into actual persisted catalog shapes.
- `invalid` and `dropping` also remain outside the new publication-state
  mapping.

Required fix:
- Add a complete mapping table between catalog states, conceptual publication
  states, planner trust classes, and queryability states.

### 4. Ordered-Family Costing References an Undefined Metric

Severity: Medium

Problem:
- The ordered-family cost model uses `prefetchable_page_fraction`, but the
  shared metrics spec does not define that metric.

Evidence:
- `cost_prefetch = cost_range - C_prefetch_gain * prefetchable_page_fraction - C_early_stop * early_stop_gain_est`
  in [ORDERED_EXACT_AND_RANGE_PLANNER_SPEC.md](../../18_Index_Framework/ORDERED_EXACT_AND_RANGE_PLANNER_SPEC.md#L128).
- The metrics packet for ordered exact families in
  [INDEX_FAMILY_METRICS_AND_CALIBRATION.md](../../18_Index_Framework/INDEX_FAMILY_METRICS_AND_CALIBRATION.md#L56)
  does not include `prefetchable_page_fraction`.

Why this matters:
- This is a direct formula/schema contradiction. A cost implementation cannot be
  fully spec-compliant because the required metric is not defined.

Required fix:
- Either add `prefetchable_page_fraction` to the ordered-family metrics packet
  or remove it from the formula and express prefetch gain through existing
  metrics.

### 5. Join Search Now Overlaps Ownership With Base Access-Path Enumeration

Severity: Medium

Problem:
- The join-search spec now owns single-relation access-family compositions such
  as native intersections, OR-union, filter-probe hybrids, ANN plus bitset, and
  mutable-overlay plus published-generation union access.
- Those are base-relation access composition problems before they are
  join-order problems.

Evidence:
- Composition candidates are mandated in
  [JOIN_SEARCH_AND_METHOD_ENUMERATION.md](../../23_SBLR_VM_Compiler_and_Executor/JOIN_SEARCH_AND_METHOD_ENUMERATION.md#L112).
- The fixed pass pipeline still says `P08_ACCESS_PATH_ANNOTATE` derives the
  candidate bundles and `P09_JOIN_ORDER_PLAN` consumes them in
  [OPTIMIZER_PASS_PIPELINE.md](../../23_SBLR_VM_Compiler_and_Executor/OPTIMIZER_PASS_PIPELINE.md#L48).

Why this matters:
- This creates duplicated ownership between access-path enumeration and join
  search. Different implementations could place the same logic in different
  phases and still think they are following the spec.

Required fix:
- Move single-relation family composition ownership back into access-path
  enumeration and reserve join-search for consuming those already-formed
  candidates.

### 6. Live Implementation Is Still Behind the Expanded Contracts

Severity: Medium

Problem:
- The new specs are target-state coherent enough to continue, but they are much
  broader than the current implementation surface.

Evidence:
- `planQuery(...)` still returns `nullptr` and explicitly tells callers to use
  `buildSelectPlan(...)` in
  [query_planner.cpp](src/optimizer/query_planner.cpp#L4800).
- `planAnalyze(...)` still returns `nullptr` in
  [query_planner.cpp](src/optimizer/query_planner.cpp#L4816).
- `AccessPathDescriptor` does not yet carry the new trust, locator,
  maintenance-state, storage-layer, or collector-specialization fields in
  [path.h](include/scratchbird/optimizer/path.h#L434).
- `RuntimePlanRelation` likewise does not yet carry the expanded field set in
  [plan_payload.h](include/scratchbird/optimizer/plan_payload.h#L30).

Why this matters:
- This is not a contradiction inside the spec tree, but it means the next
  execution workplan must clearly treat these contracts as target-state
  migrations, not as if they were already implemented.

Required fix:
- Build the next implementation workplan as a migration plan with schema
  expansion, adapter compatibility, and payload-version steps, not as a
  feature-only plan.

## Non-Findings

The consistency pass did not find any contradiction with these core invariants:
- MGA remains the governing visibility and reclaim model.
- The engine still executes SBLR and internal procedures only.
- Parser dialect behavior remains outside the engine core.
- The new donor-derived material does not introduce WAL-centered recovery
  assumptions.

## Recommended Next Step

Before generating the full execution workplan:
1. repair findings 1 through 5 in the canonical specs
2. then derive a migration-oriented implementation workplan for finding 6

That sequence is safer than planning directly from the current mixed schema
state.
