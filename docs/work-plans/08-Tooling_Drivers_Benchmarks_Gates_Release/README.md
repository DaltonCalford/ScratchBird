# 08-Tooling_Drivers_Benchmarks_Gates_Release

Status: active_workplan
Active frontier: phase 7 full rerun and release evidence closure

## Purpose

This work-plan closes the Beta 1 outer release lane: client tooling, drivers,
admin SQL surfaces, benchmark generation, gate execution, artifact
preservation, final release-readiness evidence, and the cross-section Beta 1
optimization closure now required to make the bounded release benchmark surface
practical.

## Prerequisite Status

- docs/completed-work-plans/00-Beta1_Tasks/README.md is complete
- this package is part of the ordered Beta 1 implementation program
- no implementation work may start until B1-08-001 closes specification sufficiency for this package

## Scope

- close the assigned Beta 1 sections: 30(current-authority release subset),31
- begin with a specification sufficiency closure pass over the assigned canon
- use docs/reference first and web research only when local authority is insufficient
- normalize search-key-based implementation audit anchors for the assigned scope
- drive the implementation, gate, benchmark, and evidence model for this lane
- include maintained `ScratchBird-driver` release-facing lanes required by the
  section `30` current-authority subset
- include `ScratchBird-Benchmarks` native ScratchBird target activation,
  release-matrix integration, and preserved benchmark artifacts
- consume and close the explicitly elevated Beta 1 optimization authorities
  from sections `12`, `18`, `23`, `33`, `34`, `36`, `37`, and `39` where they
  are now required to make the package `08` release benchmark surface
  practical
- end with a full clean build, test, gate, and benchmark cycle that serves as
  the Beta 1 release-readiness evidence pack

## Non-Goals

- no explicit Beta 2 or Beta 3 work unless the canonical spec is updated first
- no direct takeover of sections owned by another downstream Beta 1 plan
- no line-number-based implementation anchors

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
- PERFORMANCE_REMEDIATION_PLAN.md
- NON_BETA2_PERFORMANCE_CLOSURE_WORKPLAN.md
- ORDERED_TASK_TICKETS.csv
- DEPENDENCY_GRAPH.csv
- GATE_EVIDENCE_MATRIX.csv
- EVIDENCE_EXPECTATIONS.md
- RISK_DECISION_LOG.md
- evidence/README.md
- gates/README.md

## Primary Canonical Targets

- docs/specifications/30_Client_Tooling/README.md
- docs/specifications/30_Client_Tooling/TOOL_COMMAND_SURFACE_CONTRACTS.md
- docs/specifications/30_Client_Tooling/CLIENT_ERROR_AND_RESULT_MODEL.md
- docs/specifications/30_Client_Tooling/INSTALLER_PROFILES_AND_ARTIFACTS.md
- docs/specifications/30_Client_Tooling/DRIVER_CPP_BASELINE_SPECIFICATION.md
- docs/specifications/30_Client_Tooling/DRIVER_CLI_BASELINE_SPECIFICATION.md
- docs/specifications/30_Client_Tooling/REMOTE_ADMIN_AND_DEPLOYMENT_CONTROL_SURFACE.md
- docs/specifications/30_Client_Tooling/REMOTE_MANAGEMENT_ADMIN_SQL_COMMAND_SURFACE.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/README.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_EXECUTION_AND_FAILURE_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/PUBLIC_BETA_REQUIRED_GATE_CATEGORY_AND_STEP_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/TEST_CONTRACT.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/SCRATCHBIRD_BENCHMARKS_PROJECT_AND_MATRIX_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/FULL_CLEAN_BUILD_TEST_AND_BENCHMARK_ARTIFACT_MODEL.md
- docs/specifications/31_Conformance_Performance_and_Reliability_Gates/CLIENT_API_AND_TOOLING_GATES.md
- docs/TEST.md

## Expanded Beta 1 Optimization Closure Authorities

The following cross-section canonical authorities are now explicitly consumed by
package `08` because they were elevated to Beta 1 requirements during active
execution and they materially affect the release benchmark surface:

- docs/specifications/12_Temporary_Tables/TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md
- docs/specifications/18_Index_Framework/DML_WRITE_PATH_AND_INDEX_OPTIMIZATION_MODEL.md
- docs/specifications/18_Index_Framework/INDEX_FAMILY_NATIVE_METRICS_PACKET_CONTRACT.md
- docs/specifications/18_Index_Framework/INDEX_METRICS_AND_COSTING.md
- docs/specifications/18_Index_Framework/ORDERED_EXACT_AND_RANGE_PLANNER_SPEC.md
- docs/specifications/18_Index_Framework/SUMMARY_BITMAP_COLUMNSTORE_PLANNER_SPEC.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/ACCESS_PATH_ORDERING_AND_UPPER_STAGE_PLANNING.md
- docs/specifications/23_SBLR_VM_Compiler_and_Executor/JOIN_SEARCH_AND_METHOD_ENUMERATION.md
- docs/specifications/33_Memory_Management/BUFFER_POOL_DOMAIN_BUDGET_AND_RESIDENCY_MODEL.md
- docs/specifications/33_Memory_Management/MEMORY_GRANT_FEEDBACK_AND_OPERATOR_RESERVATION_MODEL.md
- docs/specifications/34_Table_Storage_and_Access_Methods/COLUMNSTORE_ANALYTICAL_STORAGE_AND_SEGMENT_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/PRIMARY_INDEX_FAMILY_PARITY_AND_METRICS_MANDATE.md
- docs/specifications/36_Query_Rewrite_and_Planner/ALL_IMPLEMENTED_INDEX_FAMILIES_PRIMARY_CLASS_PLANNING_AND_REFUSAL_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/NO_SECONDARY_INDEX_CLASS_HEURISTIC_AND_COMPLETE_CANDIDATE_ENUMERATION_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/INDEX_FAMILY_STATISTICS_CONSUMPTION_AND_STALENESS_PENALTY_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/FAMILY_METRICS_REFRESH_STALENESS_AND_REPLAN_TRIGGER_MODEL.md
- docs/specifications/36_Query_Rewrite_and_Planner/IMPLEMENTED_FAMILY_WINNER_OBLIGATION_AND_REFUSAL_EXPLANATION_MODEL.md
- docs/specifications/37_Statistics_Metadata_and_Schema_DDL/INDEX_FAMILY_METRICS_PUBLICATION_FRESHNESS_AND_INVALIDATION_MODEL.md
- docs/specifications/37_Statistics_Metadata_and_Schema_DDL/ONLINE_SCHEMA_CHANGE_AND_BACKFILL_MODEL.md
- docs/specifications/37_Statistics_Metadata_and_Schema_DDL/STATISTICS_COLLECTION_AND_FRESHNESS.md
- docs/specifications/39_Backup_Restore_and_Bulk_Data_Paths/BULK_INGEST_LANES_AND_SHADOW_LOAD_CUTOVER_MODEL.md

## Source Planning Inputs

- docs/completed-work-plans/00-Beta1_Tasks/README.md
- docs/completed-work-plans/00-Beta1_Tasks/WORKPLAN_GENERATION_INPUT.md
- docs/completed-work-plans/00-Beta1_Tasks/DEFINITIVE_SPECSET_INDEX.md
- docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md
- docs/reference/README.md

## Current Execution Point

- B1-08-004 is active
- B1-08-001 is closed by the restored Beta 1 release canon for section `30`,
  full admin-SQL scope, active `ScratchBird-Benchmarks` ScratchBird targets,
  and full clean-build-test-benchmark expectations
- B1-08-004 has been expanded from a narrow benchmark rerun ticket into the
  package-owned Beta 1 optimization closure lane required to make the release
  benchmark surface practical
- the ordered closure sequence under B1-08-004 is: exact-family write and
  primary-tablespace correctness, bulk ingest lanes, online build and
  heavy-family publication, metrics freshness and optimizer parity, memory
  grant and spill admission, then query-path operator optimization before the
  final rerun gates
- the bounded `small` stress benchmark blocker is now closed on the active
  default runtime; preserved no-override reruns now match the earlier
  `256MB` workaround closely and the full `15`-test bounded stress suite passes
- the fresh transaction-aware donor comparison is now preserved in
  `ScratchBird-Benchmarks/results/txmode-matrix-20260403T152011Z`; this root
  supersedes the earlier deleted mixed-transaction stress artifacts
- the current package focus is the expanded non-Beta2 performance-closure
  program: the package has been re-expanded to close every non-Beta2
  performance-affecting canonical item that still appears open; the active
  execution order is now recorded in
  NON_BETA2_PERFORMANCE_CLOSURE_WORKPLAN.md and
  PERFORMANCE_REMEDIATION_PLAN.md before the final clean aggregate rerun and
  evidence normalization
- B1-08-005 and B1-08-006 remain queued behind the performance remediation and
  rerun evidence

## Success Standard

This work-plan is complete only when:

1. B1-08-001 proves the assigned specifications are detailed enough to implement without guessing
2. every later ticket in this package closes with updated canonical specs, audit anchors, and evidence
3. the assigned Beta 1 sections are implementation-complete to their canonical standard
4. the required gate and benchmark evidence for this lane exists
5. the external benchmark project contains an active native ScratchBird target and preserved Beta 1 matrix artifacts
6. this package can move to docs/completed-work-plans without leaving unresolved scope ambiguity
7. the expanded cross-section Beta 1 optimization authorities consumed by this package are either closed in implementation or explicitly fail-closed with preserved evidence
