# Query Performance Orchestration and Cross-Layer Coordination Model

Status: reconstructed_required_with_current_substrate

## Purpose

Define the single authoritative cross-layer procedure for how ScratchBird shall
plan, admit, execute, and feed back performance-sensitive query and mutation
workloads.

This file exists so implementation agents do not have to infer how:

- index access
- heap access
- prepared handles and prepared bundles
- memory grants
- spill and workfile behavior
- vectorization
- intra-query parallelism
- worker locality
- storage growth reservation
- runtime feedback
- replan boundaries

fit together for each workload shape.

This file is the orchestration authority. It does not replace the home
subsystem specifications. It defines when each home subsystem must be consulted,
what information must be handed across subsystem boundaries, and what exact
order of operations is mandatory.

## Scope

This file owns:

- the canonical end-to-end query-performance lifecycle
- the required planning worksheet for all performance-sensitive statements
- the exact ordering of cross-layer admission checks
- the per-query-shape coordination matrix
- the runtime-plan performance contract envelope
- the required execution-time bounded-adaptivity procedure
- the mandatory feedback, cache, and replan boundaries
- the required fail-closed fallback and refusal behavior

This file does not replace:

- section `08` MGA visibility truth
- section `18` index-family legality and maintenance authority
- section `23` bytecode and executor entry-surface authority
- section `33` memory ownership and grant authority
- section `36` planner legality and cost authority
- section `12` workfile and spill authority
- section `03` locality and NUMA authority
- section `02` filespace growth and file operations authority
- section `34` heap or row-store mutation semantics
- section `39` bulk ingest lane authority

## Cross-layer authority rule

### Rule 1: this file controls ordering

If multiple subsystem specifications apply to the same statement, this file
controls:

1. which subsystem is consulted first
2. which fields must be carried from one subsystem to the next
3. which later stages are allowed to reject an otherwise legal earlier choice
4. how a surviving candidate becomes a frozen runtime performance contract

### Rule 2: home specs control local legality

Subsystem-local legality remains controlled by the home files:

- secondary-access locality and covering: section `18`
- memoize, incremental sort, runtime filters, adaptive hash join: section `36`
- vectorization and intra-query parallel execution: section `23`
- memory grants and feedback: section `33`
- workfile and spill: section `12`
- locality and worker affinity: section `03`
- heap mutation behavior: section `34`
- bulk-ingest lanes: section `39`

This file may require a subsystem to be consulted. It may not silently widen a
subsystem-local legality rule.

### Rule 3: no hidden compensation

An implementation shall not skip an earlier mandatory stage on the theory that
a later stage can recover the performance loss.

Examples of forbidden behavior:

- skipping `ICP` eligibility because `MRR` might help later
- skipping `MRR` because a later hash join might hide heap fetch disorder
- skipping incremental-sort eligibility because a full sort also works
- skipping memory admission and assuming spill can always repair the choice
- skipping worker-locality binding because the query can still run serially

## Canonical workload classes

Every planned statement shall be assigned exactly one primary workload class and
zero or more secondary traits.

### Primary workload classes

| Class | Meaning |
| --- | --- |
| `POINT_EXACT_READ` | exact key read expected to touch a small bounded key set |
| `RANGE_READ` | ordered or bounded range read over a searchable key |
| `INDEXED_JOIN_READ` | join whose profitable inner access is expected to use index probing |
| `HASH_JOIN_READ` | join whose profitable path builds hash state |
| `MERGE_JOIN_READ` | join whose profitable path depends on ordered inputs |
| `AGGREGATE_READ` | read dominated by grouping or aggregation |
| `DISTINCT_READ` | read dominated by deduplication |
| `WINDOW_READ` | read dominated by partitioned or ordered window execution |
| `ORDER_LIMIT_READ` | read dominated by ordering with optional top-N semantics |
| `VALUES_INSERT` | direct `VALUES` insert path |
| `BATCH_INSERT` | multi-row client-batched insert |
| `FILE_BULK_LOAD` | `COPY` or file-backed ingest |
| `INSERT_SELECT` | set-sourced insert from a query producer |
| `HOT_ELIGIBLE_UPDATE` | update whose indexed state remains unchanged |
| `INDEXED_UPDATE` | update that changes indexed or physically routed state |
| `DELETE_MUTATION` | delete with optional index maintenance and cleanup debt |

### Secondary traits

Every statement may additionally carry these traits:

- `COVERING_CANDIDATE`
- `ORDER_SENSITIVE`
- `TOP_N_CONTEXT`
- `PARAMETER_REUSE_EXPECTED`
- `PREPARED_EXECUTION`
- `QUERY_RESULT_CACHE_CANDIDATE`
- `PARALLEL_CANDIDATE`
- `WORKFILE_ALLOWED`
- `WORKFILE_DISALLOWED`
- `WRITE_PATH_PREALLOCATION_REQUIRED`
- `LOCALITY_SENSITIVE`
- `BULK_LANE_CANDIDATE`
- `RUNTIME_FILTER_CANDIDATE`
- `ADAPTIVE_BUILD_SIDE_CANDIDATE`

No later planning or execution stage may reclassify the primary workload class.
Later stages may refine only the secondary traits and only by explicit rules
below.

## Canonical planning worksheet

Before any candidate comparison, the planner shall build this logical worksheet:

```cpp
struct QueryPerformanceWorksheet {
  Uuid statement_uuid;
  string primary_workload_class;
  vector<string> secondary_traits;
  bool read_only;
  bool modifies_heap;
  bool modifies_indexed_state;
  bool prepared_execution;
  bool result_cache_candidate;
  bool order_sensitive;
  bool top_n_context;
  bool parameter_reuse_expected;
  bool spill_allowed;
  bool parallel_candidate;
  bool locality_sensitive;
  bool bulk_lane_candidate;
  string result_shape;
  string join_shape;
  string aggregate_shape;
  string window_shape;
  string row_source_shape;
  string predicate_shape;
  string mutation_shape;
  string storage_shape;
  uint64_t estimated_input_rows;
  uint64_t estimated_output_rows;
  uint32_t estimated_row_width;
  vector<Uuid> referenced_relation_uuids;
  vector<Uuid> referenced_index_uuids;
  string prepared_statement_identity;
  string prepared_parameter_regime;
  string prepared_bundle_identity;
  string execution_intent;
  string spill_policy_snapshot;
  string grant_policy_snapshot;
  string plan_cache_mode;
};
```

### Required field derivation

The planner shall populate every field before candidate comparison begins.
Missing field derivation is non-conforming.

Required derivation rules:

1. `primary_workload_class` comes from lowered statement family and dominant
   physical requirement.
2. `order_sensitive` is true if final semantics depend on order, merge
   preconditions, window partition order, or stable top-N ordering.
3. `parameter_reuse_expected` is true only when the lowered plan contains a
   parameterized inner path or repeated parameter-bound rescans.
4. `spill_allowed` comes from the live spill policy snapshot, not from plan
   defaults.
5. `parallel_candidate` is true only when the statement class and admitted
   operators allow single-node parallel execution.
6. `bulk_lane_candidate` is true only for `FILE_BULK_LOAD`, `INSERT_SELECT`,
   and admitted multi-row insert classes.
7. `modifies_indexed_state` is true only when section `34` and section `18`
   determine the mutation changes an indexed or routed column.
8. `prepared_execution` is true only when the front-end is executing a
   prepared statement handle or equivalent canonical bound-statement identity.
9. `result_cache_candidate` is true only for top-level deterministic select
   shapes that the executor result-cache policy admits.

## Canonical cross-layer planning order

The planner and compiler shall apply the following stages in the exact order
shown below.

### Stage 0: prepare and reuse-surface classification

Before semantic shape freeze, the planner or binder shall classify:

- simple execution versus prepared execution
- prepared statement identity when present
- prepared parameter regime when present
- prepared-bundle candidacy
- plan-cache candidacy
- result-cache candidacy

Required outputs:

- `prepared_execution`
- `prepared_statement_identity`
- `prepared_parameter_regime`
- `prepared_bundle_identity` when a bundle already exists
- `result_cache_candidate`

No later stage may retroactively treat a simple execution as prepared.

### Stage 1: semantic shape freeze

Inputs:

- lowered query or mutation shape
- catalog identity and schema epoch
- security and capability context
- policy snapshots

Required outputs:

- complete `QueryPerformanceWorksheet`
- stable statement identity
- stable relation and index identity set

No later stage may change semantics, only physical execution.

### Stage 2: base access-family enumeration

The planner shall enumerate all legal base access families required by section
`18` and section `36`.

This stage shall produce:

- heap or primary scan candidates
- ordered exact and range candidates
- summary, bitmap, columnstore, generalized, ANN, and text candidates when the
  corresponding family is legal
- structured refusals for every implemented family that is not admitted

No upper-stage specialization may be considered before base access enumeration
is complete.

### Stage 3: secondary-access transformations

For every admitted ordered exact candidate, the planner shall consult section
`18` and attempt, in this exact order:

1. `ORDERED_EXACT_INDEX_ONLY_SCAN`
2. `ORDERED_EXACT_ICP_SCAN`
3. `ORDERED_EXACT_MRR_SCAN`
4. `ORDERED_EXACT_BKA_PROBE` when inside indexed join context
5. plain `ORDERED_EXACT_SCAN`

Rules:

1. covering eligibility shall be tested before heap-touch optimization
2. `ICP` shall be tested before `MRR`
3. `BKA` shall be tested only in indexed join context
4. `MRR` may be combined with `BKA`
5. a path rejected by secondary-access transformation shall preserve its
   refusal reason in the candidate bundle

### Stage 4: upper-stage specialization

After base access and secondary-access transformation are known, the planner
shall apply upper-stage specialization in this exact order:

1. `MEMOIZE_WRAP`
2. delivered-order proof and `INCREMENTAL_SORT`
3. runtime-filter producer and consumer wiring
4. bounded adaptive build-side eligibility for legal hash joins
5. join-family selection
6. aggregate or distinct-family selection
7. window-family selection
8. final order or top-N selection

Rules:

1. `MEMOIZE_WRAP` shall be considered before join-family comparison when a
   parameterized inner path exists
2. `INCREMENTAL_SORT` shall be considered before any full sort on the same
   order requirement
3. runtime filters shall be attached before join or scan cost comparison is
   finalized
4. bounded adaptive build-side eligibility shall be decided before hash-join
   memory admission

### Stage 5: vectorization and pipeline segmentation

For every surviving logical candidate, the planner shall derive:

- row-mode family
- vectorized family
- pipeline boundaries
- exchange boundaries

Rules:

1. vectorized legality shall be checked before parallel wrappers are admitted
2. if vectorization is legal, the row-mode path remains as an explicit named
   fallback only
3. if vectorization is illegal, the runtime plan shall preserve the explicit
   row-mode fallback family name

### Stage 6: memory and spill admission

For every surviving candidate, the planner shall compute and admit:

- operator grant floors
- operator grant targets
- worker-aware memory burden
- spill expectation
- spill path requirement
- workfile requirement

The planner shall apply memory and spill admission in this exact order:

1. grant-floor feasibility
2. grant-target feasibility
3. worker-aware total burden
4. spill-path legality
5. spill-policy legality
6. workfile locality and storage legality

If any substep fails, the candidate is rejected at this stage and its refusal
reason shall identify the first failing substep.

### Stage 7: parallel admission and locality binding

Only after memory and spill admission succeeds may the planner consider
parallel execution.

Parallel admission shall apply in this exact order:

1. operator-family parallel legality
2. worker-count planning
3. leader participation
4. exchange-mode selection
5. worker-aware memory recheck
6. locality and NUMA assignment
7. bounded work-stealing legality

No candidate may become parallel if worker-aware memory or locality binding is
still unresolved.

### Stage 8: write-path lane and storage-growth admission

For write classes, after operator and worker admission are known, the planner
or execution binder shall determine:

1. write lane
2. exact-maintenance mode
3. page or extent preallocation window
4. filespace growth reservation
5. deferred merge or batch apply legality

Write-lane admission order:

1. `SHADOW_LOAD_CUTOVER`
2. `SORTED_EXACT_BULK`
3. `RETAIL_MICRO_BATCH`
4. ordinary row-at-a-time path as named fallback only

No bulk or append-heavy write candidate may enter execution without a declared
preallocation strategy.

### Stage 9: final candidate comparison and plan freeze

Only candidates that survive stages `1..8` may enter final cost comparison.

The winner shall then freeze:

- runtime operator family
- access family
- prepared-reuse contract
- vectorization mode
- parallel mode
- memory contract
- spill contract
- locality contract
- storage-growth contract
- feedback identity

No hidden runtime substitution may replace a frozen family with a slower
generic family without explicit runtime refusal evidence.

## Canonical per-workload orchestration matrix

The following matrix is mandatory.

### `POINT_EXACT_READ`

Required planning order:

1. ordered exact candidate enumeration
2. index-only eligibility
3. `ICP` eligibility
4. point-read cost comparison against heap or primary path
5. optional memoize when parameterized rescans exist
6. vectorization eligibility
7. serial versus parallel comparison, though parallel is usually rejected

Required preferred family order:

1. `ORDERED_EXACT_INDEX_ONLY_SCAN`
2. `ORDERED_EXACT_ICP_SCAN`
3. `ORDERED_EXACT_SCAN`
4. heap or primary path

### `RANGE_READ`

Required planning order:

1. ordered range candidate enumeration
2. `ICP`
3. `MRR`
4. delivered-order proof
5. incremental sort if suffix sorting is still needed
6. vectorization
7. parallel scan consideration if range size is large enough

Required preferred family order:

1. index-only range when visibility proof exists
2. `ICP + MRR`
3. `ICP`
4. `MRR`
5. plain ordered range
6. heap or primary scan

### `INDEXED_JOIN_READ`

Required planning order:

1. outer and inner path enumeration
2. parameterized-inner detection
3. `MEMOIZE_WRAP`
4. `BKA`
5. `MRR` on the inner fetch path
6. runtime-filter opportunities
7. vectorized join path
8. parallel admission if both probe and consumer semantics remain legal

Required preferred family order:

1. memoized indexed join with `BKA + MRR` when legal
2. memoized indexed join
3. indexed join with `BKA`
4. indexed join with row-at-a-time probes
5. non-indexed join families

### `HASH_JOIN_READ`

Required planning order:

1. build and probe candidate derivation
2. runtime-filter producer and consumer derivation
3. bounded adaptive build-side eligibility
4. hash-state memory admission
5. spill or partitioned-hash legality
6. vectorized hash join legality
7. parallel repartition or local-broadcast legality

Required preferred family order:

1. vectorized in-memory hash join with runtime filters
2. vectorized spilled or partitioned hash join with runtime filters
3. row-mode hash join only when vectorization is illegal

### `MERGE_JOIN_READ`

Required planning order:

1. ordered child proof for both sides
2. incremental sort on unsatisfied suffixes
3. merge-join legality
4. merge-run memory and spill admission
5. vectorized compare and emit legality
6. `GATHER_MERGE` admission for parallel workers

Required preferred family order:

1. merge join on already ordered children
2. merge join with incremental sort on one or both sides
3. merge join with full sort only when no prefix order exists

### `AGGREGATE_READ`

Required planning order:

1. delivered grouping-order proof
2. hash aggregate versus sort aggregate enumeration
3. aggregate-state memory admission
4. runtime spill or partial-aggregate legality
5. vectorized aggregate legality
6. partial/final aggregate parallel legality

Required preferred family order:

1. vectorized in-memory hash aggregate
2. vectorized spilled hash aggregate
3. vectorized ordered aggregate on already grouped or incrementally sorted input
4. row-mode aggregate only when vectorization is illegal

### `DISTINCT_READ`

Required planning order:

1. delivered distinct-order proof
2. hash distinct versus sort distinct enumeration
3. distinct-state memory admission
4. spill legality
5. vectorized distinct legality
6. partial/final parallel distinct legality

### `WINDOW_READ`

Required planning order:

1. partition and order proof
2. incremental sort eligibility
3. partition-local buffering requirement
4. window-state memory admission
5. workfile spill legality
6. vectorized window legality
7. parallel legality only when whole partitions remain worker-local

Required preferred family order:

1. window on already ordered partition input
2. window with incremental sort
3. window with full sort

### `ORDER_LIMIT_READ`

Required planning order:

1. delivered-order proof
2. incremental sort
3. top-N bounded-heap or bounded-merge path when semantics allow it
4. full sort only if the above are illegal
5. memory and spill admission
6. vectorized sort legality
7. `GATHER_MERGE` legality for parallel order-preserving execution

### `VALUES_INSERT` and `BATCH_INSERT`

Required planning order:

1. row-store lane classification
2. heap multi-insert admission
3. exact-maintenance batch eligibility
4. unchanged-key elision or HOT-like update reuse when applicable
5. filespace preallocation window
6. write-worker locality if parallel producer exists

### `FILE_BULK_LOAD`

Required planning order:

1. lane selection among `SHADOW_LOAD_CUTOVER`, `SORTED_EXACT_BULK`,
   `RETAIL_MICRO_BATCH`
2. batch size and staging contract
3. exact-secondary maintenance route
4. filespace and page-run reservation
5. workerized parse or load staging when legal
6. durable bulk-load plan, progress, event, and cutover state publication

### `INSERT_SELECT`

Required planning order:

1. producer query planning under the normal read rules
2. producer vectorization and parallel admission
3. sink lane selection
4. producer-to-sink batch handoff contract
5. exact-maintenance and preallocation strategy
6. spill legality for the producer independent from the sink lane

### `HOT_ELIGIBLE_UPDATE`

Required planning order:

1. indexed-state change proof
2. heap-only or stable-head-preserving update legality
3. exact-maintenance elision
4. page-local update locality
5. residual cleanup and debt publication when required

### `INDEXED_UPDATE` and `DELETE_MUTATION`

Required planning order:

1. changed-key or routed-state proof
2. exact-maintenance batch and deferral strategy
3. cleanup debt publication
4. filespace and locality behavior for secondary effects
5. feedback and observability publication

## Canonical runtime performance contract

Every frozen runtime plan shall expose this logical contract:

```cpp
struct RuntimePerformanceContract {
  Uuid statement_uuid;
  string primary_workload_class;
  vector<string> secondary_traits;
  vector<AccessStageContract> access_stages;
  vector<OperatorStageContract> operator_stages;
  MemoryAdmissionContract memory_contract;
  SpillAdmissionContract spill_contract;
  ParallelAdmissionContract parallel_contract;
  LocalityBindingContract locality_contract;
  WritePathContract write_contract;
  PrepareReuseContract prepare_contract;
  FeedbackIdentityContract feedback_identity;
  ReplanBoundaryContract replan_boundary;
  vector<StructuredRefusal> material_refusals;
};
```

### `AccessStageContract`

Each access stage shall record:

- `stage_uuid`
- `relation_uuid`
- `chosen_access_family`
- `delivered_order`
- `requires_heap_fetch`
- `uses_icp`
- `uses_mrr`
- `uses_bka`
- `uses_index_only`
- `visibility_proof_kind`
- `residual_recheck_count`

### `OperatorStageContract`

Each operator stage shall record:

- `stage_uuid`
- `operator_family`
- `vector_mode`
- `row_mode_fallback_family`
- `parallel_mode`
- `exchange_mode`
- `delivered_order`
- `consumed_runtime_filters`
- `produced_runtime_filters`
- `uses_memoize`
- `uses_incremental_sort`
- `adaptive_behavior`

### `MemoryAdmissionContract`

The memory contract shall record:

- `grant_floor_bytes`
- `grant_target_bytes`
- `grant_ceiling_bytes`
- `worker_total_bytes`
- `leader_bytes`
- `per_worker_bytes`
- `feedback_source`
- `feedback_epoch`
- `spill_expected`

### `SpillAdmissionContract`

The spill contract shall record:

- `spill_policy`
- `workfile_required`
- `workfile_operator_kind`
- `spill_trigger_metric`
- `spill_partitioning_mode`
- `workfile_locality_class`

### `ParallelAdmissionContract`

The parallel contract shall record:

- `planned_worker_count`
- `actual_worker_cap`
- `leader_participates`
- `exchange_mode`
- `parallel_refusal_reason` when no legal parallel plan survived

### `LocalityBindingContract`

The locality contract shall record:

- `preferred_node_or_socket`
- `preferred_partition_key`
- `morsel_locality_required`
- `allow_work_steal`
- `locality_refusal_reason` when locality-sensitive parallel admission fails

### `WritePathContract`

The write contract shall record:

- `write_lane`
- `exact_maintenance_mode`
- `preallocation_mode`
- `preallocation_window_pages`
- `bulk_plan_uuid` when applicable
- `cleanup_debt_expected`

### `PrepareReuseContract`

The prepare-reuse contract shall record:

- `prepared_execution`
- `prepared_statement_identity`
- `prepared_parameter_regime`
- `prepared_bundle_hit`
- `plan_cache_hit`
- `result_cache_candidate`
- `result_cache_hit`

### `FeedbackIdentityContract`

The feedback identity shall record:

- `plan_profile_identity`
- `execution_intent`
- `spill_policy_snapshot`
- `grant_policy_snapshot`
- `storage_shape_identity`
- `cache_mode_identity`

### `ReplanBoundaryContract`

The replan boundary shall record:

- `schema_epoch`
- `family_statistics_signature`
- `policy_signature`
- `feedback_signature`
- `runtime_capability_signature`

## Execution-time bounded adaptivity

Runtime adaptivity is legal only in the bounded forms explicitly admitted by
subordinate specs.

Allowed bounded-adaptive actions:

- runtime-filter publication and consumption
- bounded adaptive build-side selection for legal hash joins
- worker-count reduction down to the admitted cap
- spill activation on actual pressure
- vector batch-size shrink within the admitted tunable range

Forbidden runtime-adaptive actions:

- switching from one access family to a different unplanned access family
- switching from hash join to merge join or vice versa
- enabling parallelism when the frozen plan was serial
- enabling index-only when the frozen plan required heap fetch
- skipping declared visibility or residual rechecks

If bounded runtime adaptivity changes a physical detail, the executor shall
emit an explicit runtime evidence row rather than silently mutating behavior.

## Fail-closed fallback rules

The executor and planner shall use only named fail-closed fallbacks.

Required fallback families:

- `ROW_MODE_SCAN`
- `ROW_MODE_JOIN`
- `ROW_MODE_AGG`
- `ROW_MODE_SORT`
- `ROW_MODE_WINDOW`
- `SERIAL_FALLBACK`
- `PLAIN_HEAP_SCAN`
- `PLAIN_ORDERED_EXACT_SCAN`
- `ORDINARY_ROW_WRITE_PATH`

Rules:

1. every fallback shall preserve the original refusal or runtime downgrade
   reason
2. every fallback shall remain within the frozen memory, spill, and policy
   envelope
3. an execution-time downgrade that would violate live policy shall fail closed
   with an explicit error instead of silently continuing

## Required structured refusals

Every material rejected candidate shall preserve:

- `candidate_family`
- `refusal_code`
- `refusal_summary`
- `cause_domain`
- `rejected_at_stage`

`rejected_at_stage` shall be one of:

- `BASE_ACCESS`
- `SECONDARY_TRANSFORM`
- `UPPER_STAGE_SPECIALIZATION`
- `VECTORIZATION`
- `MEMORY_ADMISSION`
- `SPILL_ADMISSION`
- `PARALLEL_ADMISSION`
- `LOCALITY_BINDING`
- `WRITE_LANE_ADMISSION`
- `FINAL_COST_COMPARISON`

## Required implementation procedure

An implementation agent shall implement the orchestration in the following
order and may not skip steps:

1. build `QueryPerformanceWorksheet`
2. enumerate all legal base access candidates
3. apply section `18` secondary-access transformations in the required order
4. apply section `36` upper-stage specialization in the required order
5. derive vector and row-mode variants
6. derive memory and spill contracts
7. derive parallel and locality contracts
8. derive write-lane and preallocation contracts for write classes
9. reject any candidate missing a required contract field
10. compare only surviving candidates
11. freeze the complete `RuntimePerformanceContract`
12. execute only within the frozen contract plus bounded-adaptive overlays
13. persist feedback only under the declared `FeedbackIdentityContract`

## Required tests

Before this file is considered closed, tests shall prove all of the following:

1. point exact read prefers index-only over heap fetch when visibility proof is
   present
2. range read preserves `ICP` and `MRR` ordering and restores semantic order
   when required
3. indexed join preserves `MEMOIZE_WRAP`, `BKA`, and `MRR` composition
4. hash join preserves runtime-filter production, adaptive build-side evidence,
   and spill admission
5. merge join uses incremental sort on delivered-prefix inputs instead of full
   sort
6. aggregate and distinct preserve vectorized and spilled execution evidence
7. window execution preserves incremental sort, partition buffering, and spill
   evidence
8. `INSERT ... SELECT` preserves producer and sink contracts independently
9. bulk load preserves lane selection, preallocation, and bulk-state catalog
   evidence
10. HOT-eligible update preserves unchanged-key elision and named cleanup-debt
    publication
11. every rejected material candidate preserves `rejected_at_stage`
12. stale bytecode fails closed when live spill or grant policy invalidates the
    frozen contract
13. feedback does not cross execution-intent, plan-profile, policy, or storage
    shape boundaries

## Cross references

- `VM_EXECUTION_ARCHITECTURE.md`
- `OPTIMIZER_ARCHITECTURE_AND_MAIN_PATH_INTEGRATION.md`
- `OPTIMIZER_PASS_PIPELINE.md`
- `../18_Index_Framework/SECONDARY_ACCESS_LOCALITY_PUSHDOWN_AND_COVERING_EXECUTION_MODEL.md`
- `../36_Query_Rewrite_and_Planner/HIGH_PERFORMANCE_OLTP_PLAN_SHAPES_CONTENTION_AVOIDANCE_AND_PREPARED_EXECUTION_MODEL.md`
- `../36_Query_Rewrite_and_Planner/MEMOIZE_INCREMENTAL_SORT_RUNTIME_FILTER_AND_ADAPTIVE_JOIN_MODEL.md`
- `../36_Query_Rewrite_and_Planner/PLANNER_STRATEGY_AND_PLAN_STABILITY.md`
- `../33_Memory_Management/MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md`
- `../12_Temporary_Tables/TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md`
- `../03_Disk_Allocator_and_Free_Space/NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md`
- `../02_Filespace_Lifecycle/FILESPACE_OPERATIONS.md`
- `../34_Table_Storage_and_Access_Methods/HEAP_MULTI_INSERT_AND_HEAP_ONLY_UPDATE_PERFORMANCE_MODEL.md`
- `../39_Backup_Restore_and_Bulk_Data_Paths/BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md`
