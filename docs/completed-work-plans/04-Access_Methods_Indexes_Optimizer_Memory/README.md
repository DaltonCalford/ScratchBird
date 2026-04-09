# 04-Access_Methods_Indexes_Optimizer_Memory

Status: completed_workplan

## Purpose

This work-plan closes Beta 1 access methods, index parity, optimizer behavior, memory and accelerator use, and workload governance. It is the plan that ensures every index is a primary optimizer candidate with usable metrics and that Beta 1 acceleration behavior is explicit rather than implied.

## Prerequisite Status

- docs/completed-work-plans/00-Beta1_Tasks/README.md is complete
- this package is part of the ordered Beta 1 implementation program
- no implementation work may start until B1-04-001 closes specification sufficiency for this package

## Scope

- close the assigned Beta 1 sections: 18,33,34,36,38
- begin with a specification sufficiency closure pass over the assigned canon
- use docs/reference first and web research only when local authority is insufficient
- normalize search-key-based implementation audit anchors for the assigned scope
- drive the implementation, gate, benchmark, and evidence model for this lane

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
- ORDERED_TASK_TICKETS.csv
- DEPENDENCY_GRAPH.csv
- GATE_EVIDENCE_MATRIX.csv
- EVIDENCE_EXPECTATIONS.md
- RISK_DECISION_LOG.md
- evidence/README.md
- gates/README.md

## Primary Canonical Targets

- docs/specifications/18_Index_Framework/README.md
- docs/specifications/18_Index_Framework/INDEX_METRICS_AND_COSTING.md
- docs/specifications/33_Memory_Management/README.md
- docs/specifications/34_Table_Storage_and_Access_Methods/README.md
- docs/specifications/36_Query_Rewrite_and_Planner/PRIMARY_INDEX_FAMILY_PARITY_AND_METRICS_MANDATE.md
- docs/specifications/38_Workload_Governance_and_Parallelism/README.md

## Source Planning Inputs

- docs/completed-work-plans/00-Beta1_Tasks/README.md
- docs/completed-work-plans/00-Beta1_Tasks/WORKPLAN_GENERATION_INPUT.md
- docs/completed-work-plans/00-Beta1_Tasks/DEFINITIVE_SPECSET_INDEX.md
- docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md
- docs/reference/README.md

## Current Execution Point

- B1-04-001 is completed
- B1-04-002 is completed
- B1-04-003 is completed
- B1-04-004 is completed
- B1-04-005 is completed
- B1-04-006 is completed
- package `04` canon now fixes parity at the admitted named-family layer,
  requires full persisted canonical family fields per index, activates
  accelerator-capable families for Beta 1 governance and fallback work, and
  derives effective resource envelopes from the environment ceiling plus clamped
  configuration
- live lane ownership and audit anchors are now frozen to catalog, storage,
  buffer, planner, statistics, and workload-governance seams
- lane A now persists canonical family identity through the existing
  `index_params_oid` TOAST carrier, validates create/open metadata against the
  `IndexFactory` registry, and proves admitted ANN-family create paths through
  focused V3 executor contracts
- lane B now preserves shared-backend sibling families as distinct primary
  planner candidates, publishes canonical named-family metrics identity through
  statistics refresh, extends workload admission rows with accelerator policy
  and device state, and records environment-clamped effective buffer budgets
- package `04` now preserves its bounded section `31` benchmark surface through
  the repo-local cache-buffer benchmark, optimizer cost-calibration benchmark,
  and ordered-index proof corpus; the external `ScratchBird-Benchmarks` matrix
  remains non-applicable for ScratchBird-target execution at current authority
- this package is now archived under
  `docs/completed-work-plans/04-Access_Methods_Indexes_Optimizer_Memory/`

## Success Standard

This work-plan is complete only when:

1. B1-04-001 proves the assigned specifications are detailed enough to implement without guessing
2. every later ticket in this package closes with updated canonical specs, audit anchors, and evidence
3. the assigned Beta 1 sections are implementation-complete to their canonical standard
4. the required gate and benchmark evidence for this lane exists
5. this package can move to docs/completed-work-plans without leaving unresolved scope ambiguity
