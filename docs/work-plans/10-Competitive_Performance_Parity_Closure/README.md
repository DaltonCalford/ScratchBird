# 10-Competitive_Performance_Parity_Closure

Status: active_workplan
Active frontier: phase 0 benchmark discipline and phase 1 write-path parity recovery

## Purpose

This package is the canonical active execution program for donor-competitive
performance parity.

Its goal is strict:

- ScratchBird shall be no more than `3%` slower than the fastest competing
  database in the same benchmark process and transactional guarantee class.
- Faster than the donor best is acceptable.
- Slower than the donor best by more than `3%` is not acceptable.

This package turns the parity specifications and the April 5, 2026 benchmark
audit into a dependency-ordered implementation program with machine-readable
trackers.

## Relationship to package `08`

Package `08` remains the release benchmark and gate consumer. Package `10`
owns the deeper donor-parity implementation queue that package `08` must
consume before final benchmark and release closeout can be treated as complete.

## Scope

- all benchmark-governed write, join, sort, aggregate, distinct, and window
  processes in the current comparable matrix
- all required donor fast-path assimilation work identified by the April 5,
  2026 audit
- prepared-query and query-result-cache performance closure for admitted
  high-performance OLTP shapes
- runtime spill, workfile, vectorization, parallelism, and locality closure for
  benchmark-governed operators
- benchmark discipline and binary-pinning closure so every parity claim is
  reproducible

## Non-Goals

- no weakening of MGA, UUID, parser-boundary, or transactional invariants
- no Beta 2 distributed-execution claims
- no line-number-based implementation anchors
- no benchmark “win” claimed without preserved donor-comparable evidence

## Contents

- README.md
- WORKPLAN_GENERATION_INPUT.md
- DEFINITIVE_SPECSET_INDEX.md
- CANONICAL_GAP_REGISTER.md
- BOUNDED_TICKET_SET.md
- CODE_AREA_OWNERSHIP_MAP.md
- CODE_TRUTH_AUDIT_MAINTENANCE_RULES.md
- BENCHMARK_AND_LOAD_SHAPE_INPUTS.md
- SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- MASTER_TRACKER.md
- MASTER_TRACKER.csv
- PERFORMANCE_PARITY_IMPLEMENTATION_WORKPLAN.md
- ORDERED_TASK_TICKETS.csv
- DEPENDENCY_GRAPH.csv
- PROCESS_PARITY_TARGETS.csv
- LOAD_TABLE_TARGETS.csv
- DONOR_FAST_PATH_ASSIMILATION_TRACKER.csv
- GATE_EVIDENCE_MATRIX.csv
- EVIDENCE_EXPECTATIONS.md
- RISK_DECISION_LOG.md
- evidence/README.md
- gates/README.md

## Primary Canonical Targets

- docs/specifications/work/implementation_tracks/competitive_performance_parity_workpack/README.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/QUERY_PERFORMANCE_ORCHESTRATION_AND_CROSS_LAYER_COORDINATION_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/HIGH_PERFORMANCE_OLTP_PLAN_SHAPES_CONTENTION_AVOIDANCE_AND_PREPARED_EXECUTION_MODEL.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/EXECUTION_CACHE_AND_INVALIDATION.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/PLAN_CACHE_PARALLELISM_AND_OPTIMIZER_FEEDBACK.md
- docs/specifications/36_Query_Rewrite_and_Planner/PLAN_CACHE_AND_INVALIDATION_RULES.md
- docs/specifications/18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md
- docs/specifications/18_Index_Framework/SECONDARY_ACCESS_LOCALITY_PUSHDOWN_AND_COVERING_EXECUTION_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/MEMOIZE_INCREMENTAL_SORT_RUNTIME_FILTER_AND_ADAPTIVE_JOIN_MODEL.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/VECTORIZED_PIPELINED_AND_INTRA_QUERY_PARALLEL_EXECUTION_MODEL.md
- docs/specifications/33_Memory_Management/MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md
- docs/specifications/12_Temporary_Tables/TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md
- docs/specifications/03_Disk_Allocator_and_Free_Space/NUMA_LOCALITY_AND_FRAME_OWNERSHIP.md
- docs/specifications/03_Disk_Allocator_and_Free_Space/ALLOCATION_ALGORITHMS.md
- docs/specifications/02_Filespace_Lifecycle/FILESPACE_OPERATIONS.md
- docs/specifications/34_Table_Storage_and_Access_Methods/HEAP_MULTI_INSERT_AND_HEAP_ONLY_UPDATE_PERFORMANCE_MODEL.md
- docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md

## Source Planning Inputs

- docs/specifications/work/audits/SCRATCHBIRD_BENCHMARK_REGRESSION_AND_DONOR_FAST_PATH_AUDIT_2026-04-05/SCRATCHBIRD_BENCHMARK_REGRESSION_AND_DONOR_FAST_PATH_AUDIT.md
- docs/specifications/work/audits/SCRATCHBIRD_MYSQL_INSERT_FAST_PATH_DELTA_2026-04-03/README.md
- ScratchBird-Benchmarks/results/current-scratchbird-stress-20260405T035510Z
- ScratchBird-Benchmarks/results/clean-rebuild-scratchbird-stress-20260404T023343Z
- ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z
- docs/work-plans/08-Tooling_Drivers_Benchmarks_Gates_Release/README.md
- docs/work-plans/08-Tooling_Drivers_Benchmarks_Gates_Release/PERFORMANCE_REMEDIATION_PLAN.md

## Success Standard

This package is complete only when:

1. every row in [PROCESS_PARITY_TARGETS.csv](PROCESS_PARITY_TARGETS.csv) is
   `met` or carries an explicit waiver with invariant proof
2. every row in [LOAD_TABLE_TARGETS.csv](LOAD_TABLE_TARGETS.csv) is `met` or
   carries an explicit waiver with invariant proof
3. every row in
   [DONOR_FAST_PATH_ASSIMILATION_TRACKER.csv](DONOR_FAST_PATH_ASSIMILATION_TRACKER.csv)
   is `implemented`, `dominated`, or `waived`
4. benchmark artifacts are binary-pinned and reproducible
5. package `08` can consume this package’s final rerun evidence without
   unresolved parity ambiguity
