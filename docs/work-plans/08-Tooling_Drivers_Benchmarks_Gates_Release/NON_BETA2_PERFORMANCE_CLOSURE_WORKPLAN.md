# Non-Beta2 Performance Closure Workplan

Status: active_package_expansion

Owning tickets:
- B1-08-004
- B1-08-005

## Goal

Implement every package-owned canonical item that:

- materially affects runtime or benchmark performance
- is not explicitly Beta 2 only
- does not appear fully implemented in the current codebase

This workplan expands package `08` from a bounded benchmark-fix lane into the
full non-Beta2 performance-closure lane now required before Beta 1 closeout.

## Inclusion Rule

Include only items from package-owned canon that are:

- `current_authority_beta1`
- `current_authority_with_reconstructed_expansion`
- `reconstructed_required`
- `reconstructed_required_with_current_substrate`

Exclude:

- explicitly Beta 2 only named extensions
- target-state-only notes
- surfaces that canon intentionally keeps unsupported and fail-closed unless an
  admitted Beta 1 subset is explicitly required

## Ordering Rules

The implementation order is dependency-driven:

1. close correctness and state publication before cost-based consumption
2. close write and maintenance paths before bulk-ingest and online-build paths
3. close publication and invalidation before planner-parity work
4. close memory admission before final join, sort, and aggregate tuning
5. close runtime and trace surfaces before section `31` rerun evidence

## Canonical Item Inventory

### Phase 1: Exact-family write path and maintained-index closure

Primary canon:

- docs/specifications/18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md
- docs/specifications/33_Memory_Management/BUFFER_POOL_DOMAIN_BUDGET_AND_RESIDENCY_MODEL.md

Items to close:

- exact-family same-key update suppression
- reclaim-driven exact cleanup debt
- transaction-local index delta buffering for admitted families
- narrow cold-page secondary delta buffering and merge-before-read behavior
- commit-group batch apply and its reloadable runtime controls
- hot-leaf or right-edge mitigation for sustained exact writes
- durable write-path observability for suppression, batching, cleanup debt, and
  hot-page pressure
- any remaining sustained-load correctness faults exposed by the maintained
  write substrate

Why first:

- bulk ingest, online build, family metrics invalidation, and benchmark load
  lanes all depend on a stable maintained write path

Exit criteria:

- bounded load and update paths are correct under sustained benchmark pressure
- exact-family write-path features required by Beta 1 are no longer missing or
  silently bypassed on the active runtime

### Phase 2: Bulk-ingest lane closure

Primary canon:

- docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md
- docs/specifications/18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md

Items to close:

- `RETAIL_MICRO_BATCH` lane selection, execution, and observability
- `SORTED_EXACT_BULK` lane selection and execution for admitted exact loads
- `SHADOW_LOAD_CUTOVER` lane selection and cutover behavior
- durable `bulk_load_plan`
- durable `bulk_load_event`
- durable `bulk_load_progress`
- durable `bulk_load_cutover_guard`
- benchmark and gate harness integration so preserved load runs exercise the
  canonical ingest lanes rather than ordinary retail DML only

Why second:

- the release benchmark load path is one of the package-owned blockers and it
  builds directly on the maintained exact-family write substrate

Exit criteria:

- large additive and rebuild-style loads no longer rely exclusively on the
  ordinary retail write path when canon requires a bulk lane
- load-path evidence can distinguish retail, sorted-bulk, and shadow-cutover
  execution

### Phase 3: Online build, visibility-state, and heavy-family publication closure

Primary canon:

- docs/specifications/37_Statistics_Metadata_and_Schema_DDL/ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md
- docs/specifications/18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md

Items to close:

- durable `index_build_plan`
- durable `index_build_progress`
- durable `index_build_cutover_guard`
- admitted online-build visibility states, including `VISIBLE_CANDIDATE`
- resumable backfill and cutover behavior for admitted online index builds
- explicit publish, retire, and failure-state transitions for index families
- heavy-family pending lanes where Beta 1 canon admits them
- immutable heavy-family generation publication
- durable generation or manifest tracking needed for published heavy families
- shared cutover and retirement semantics between shadow-load and online-build
  publication paths

Why third:

- metrics freshness and optimizer parity depend on concrete build, publish,
  retire, and invalidation events

Exit criteria:

- online index publication no longer relies on implicit or partial state
- admitted heavy-family publication paths are durable and observable

### Phase 4: Family-native metrics, freshness, and invalidation closure

Primary canon:

- docs/specifications/18_Index_Framework/INDEX_FAMILY_NATIVE_METRICS_PACKET_CONTRACT.md
- docs/specifications/18_Index_Framework/INDEX_METRICS_AND_COSTING.md
- docs/specifications/37_Statistics_Metadata_and_Schema_DDL/INDEX_FAMILY_METRICS_PUBLICATION_FRESHNESS_AND_INVALIDATION_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/INDEX_FAMILY_STATISTICS_CONSUMPTION_AND_STALENESS_PENALTY_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/FAMILY_METRICS_REFRESH_STALENESS_AND_REPLAN_TRIGGER_MODEL.md
- docs/specifications/37_Statistics_Metadata_and_Schema_DDL/STATISTICS_COLLECTION_AND_FRESHNESS.md

Items to close:

- family-native metrics packet publication for every implemented family
- required shared metrics envelope fields for every family
- family-specific native payload fields where Beta 1 canon requires them
- explicit freshness classes `CURRENT`, `AGED`, `STALE_DEGRADED`, and
  `UNUSABLE`
- explicit invalidation state and invalidation reasons
- shared publication epoch and refresh timestamp handling
- refresh, invalidate, and replan triggers wired to real maintenance and build
  events
- fail-closed behavior when a family cannot yet publish a required metric
- typed packet loading and explain or diagnostics visibility for benchmarked
  families

Why fourth:

- planner parity cannot be closed safely until the planner is consuming a real
  publication and invalidation model instead of implicit freshness assumptions

Exit criteria:

- every implemented family visible to the optimizer has an explicit published
  metrics state
- metrics freshness and invalidation are no longer inferred ad hoc

### Phase 5: Memory residency, operator grants, and bounded spill-admission closure

Primary canon:

- docs/specifications/33_Memory_Management/BUFFER_POOL_DOMAIN_BUDGET_AND_RESIDENCY_MODEL.md
- docs/specifications/33_Memory_Management/MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md
- docs/specifications/12_Temporary_Tables/TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md

Items to close:

- any remaining divergence in shared buffer-pool policy-domain and residency
  budgeting behavior on the benchmark-visible runtime paths
- operator and statement grant reservation behavior
- durable `memory_grant_feedback` row model and required feedback fields
- percentile-based right-sizing and oscillation handling
- explicit spill-first, grant-denial, and cancellation behavior for admitted
  operators
- explicit refusal and diagnostics when an operator requires unsupported spill
  behavior
- planner and runtime agreement on spill expectation versus runtime admission
  outcome

Why fifth:

- final join, aggregate, sort, and upper-stage planning closure should not be
  tuned against a nonexistent or implicit admission layer

Exit criteria:

- memory admission is explicit, bounded, and observable
- unsupported spill behavior remains fail-closed rather than silently invented

### Phase 6: Planner parity, candidate enumeration, and refusal-model closure

Primary canon:

- docs/specifications/36_Query_Rewrite_and_Planner/PRIMARY_INDEX_FAMILY_PARITY_AND_METRICS_MANDATE.md
- docs/specifications/36_Query_Rewrite_and_Planner/ALL_IMPLEMENTED_INDEX_FAMILIES_PRIMARY_CLASS_PLANNING_AND_REFUSAL_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/NO_SECONDARY_INDEX_CLASS_HEURISTIC_AND_COMPLETE_CANDIDATE_ENUMERATION_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/IMPLEMENTED_FAMILY_WINNER_OBLIGATION_AND_REFUSAL_EXPLANATION_MODEL.md
- docs/specifications/18_Index_Framework/ORDERED_EXACT_AND_RANGE_PLANNER_SPEC.md
- docs/specifications/18_Index_Framework/SUMMARY_BITMAP_COLUMNSTORE_PLANNER_SPEC.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/ACCESS_PATH_ORDERING_AND_UPPER_STAGE_PLANNING.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/JOIN_SEARCH_AND_METHOD_ENUMERATION.md

Items to close:

- removal of any remaining silent secondary-family downgrade behavior
- complete candidate enumeration for implemented and semantically legal
  families
- structured refusal records for legal families that are not admitted further
- cached-plan preservation of refusal reasons
- winner-or-refusal coverage for every implemented family
- ordered exact and range path legality, covering, recheck, and costing closure
- summary, bitmap, bloom, and columnstore candidate-path closure where the
  family is implemented and admitted by Beta 1 canon
- access-path ordering and upper-stage planning closure for sort avoidance and
  ordered delivery
- join-search and method-enumeration closure for the admitted join families
- trace and explain coverage naming winner choice and refusal causes

Why sixth:

- this phase depends on the real metrics, invalidation, and memory-admission
  surfaces from phases `4` and `5`

Exit criteria:

- implemented families no longer disappear from candidate generation without an
  explicit reason
- planner output and diagnostics can explain both winner choice and family
  absence

### Phase 7: Query-runtime and analytical-path closure

Primary canon:

- docs/specifications/34_Table_Storage_and_Access_Methods/COLUMNSTORE_ANALYTICAL_STORAGE_AND_SEGMENT_MODEL.md
- docs/specifications/18_Index_Framework/SUMMARY_BITMAP_COLUMNSTORE_PLANNER_SPEC.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/ACCESS_PATH_ORDERING_AND_UPPER_STAGE_PLANNING.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/JOIN_SEARCH_AND_METHOD_ENUMERATION.md

Items to close:

- columnstore late materialization on the runtime path
- any remaining admitted columnstore projection, predicate-pruning, and
  covering-path gaps required by Beta 1 canon
- runtime support needed to make planner-admitted summary, bitmap, and
  columnstore winners real instead of purely cost-model placeholders
- remaining benchmark-visible join, aggregate, sort, and insert-select hot-path
  closures after the earlier substrate phases land
- explain or trace parity between chosen plans and actual execution behavior

Why seventh:

- runtime operator tuning should happen after write, publication, metrics, and
  memory-admission semantics are stable

Exit criteria:

- benchmark-visible analytical and join-heavy paths are backed by real runtime
  closure rather than planner-only intent
- known hot scenarios are measured against the donor matrix after the canonical
  substrates are in place

### Phase 8: Section 31 rerun and release-evidence closure

Primary canon:

- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/README.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_EXECUTION_AND_FAILURE_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_CATEGORY_AND_STEP_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/FULL_CLEAN_BUILD_TEST_AND_BENCHMARK_ARTIFACT_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/SCRATCHBIRD_BENCHMARKS_PROJECT_AND_MATRIX_MODEL.md
- docs/TEST.md

Items to close:

- remove superseded benchmark roots before official reruns when the benchmark
  contract has changed
- rerun the transaction-aware donor comparison matrix
- rerun the clean full build, test, and benchmark aggregate from scratch
- preserve before-or-after evidence for each major optimization phase
- update package evidence, risk notes, and gate summaries against the expanded
  closure scope

Why last:

- section `31` evidence is only meaningful after the runtime and planner
  closure work above has landed

Exit criteria:

- package `08` owns a clean current proof set for the full non-Beta2
  performance closure scope
- remaining limits, if any, are explicitly documented and no longer represent
  hidden implementation debt

## Deliverable Map

The workplan closes these outstanding canonical categories:

- exact-family maintained write-path optimization
- bulk-ingest lane implementation
- online index publication and heavy-family publish semantics
- family-native metrics publication and freshness discipline
- buffer-pool residency and operator memory-grant admission
- planner primary-class parity, enumeration, refusal, and winner explanation
- analytical runtime closure, including late materialization
- section `31` benchmark and gate rerun evidence

## Package-State Implication

Package `08` is no longer in simple rerun-only closure mode. The active work is
the expanded non-Beta2 performance-closure program above. `B1-08-005` may not
close until this workplan is materially executed and the official rerun
evidence is regenerated on top of the resulting implementation state.
