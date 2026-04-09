# 01-Core_MGA_Storage_Recovery_Buffers

Status: completed_workplan

## Purpose

This work-plan closes the Beta 1 MGA storage foundation: record state, page truth, allocator and free-space behavior, buffer and writeback policy, checkpoint and restart, lock semantics, sweep and reclaim, LOB cleanup, time ordering, and engine failure handling.

## Prerequisite Status

- docs/completed-work-plans/00-Beta1_Tasks/README.md is complete
- this package is part of the ordered Beta 1 implementation program
- no implementation work may start until B1-01-001 closes specification sufficiency for this package

## Scope

- close the assigned Beta 1 sections: 02,03,04,05,06,08,09,10,11,35,40,42
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

- docs/specifications/02_Filespace_Lifecycle/README.md
- docs/specifications/03_Disk_Allocator_and_Free_Space/README.md
- docs/specifications/04_Page_Size_Policy/README.md
- docs/specifications/05_Page_Taxonomy_and_Binary_Layouts/README.md
- docs/specifications/06_Fixed_Bootstrap_Page_Map/README.md
- docs/specifications/08_Transaction_Core/MGA_RECORD_STATE_AND_PUBLICATION_MODEL.md
- docs/specifications/09_Lock_Manager_Core/MGA_CONFLICT_AND_LOCKING_POLICY.md
- docs/specifications/10_GC_and_Sweep/README.md
- docs/specifications/11_TOAST_and_LOB_Storage/README.md
- docs/specifications/35_Durability_Crash_Recovery_and_Checkpoint_Model/TRANSACTION_DURABILITY_RECOVERY_OWNERSHIP_AND_MGA_ALIGNMENT_MODEL.md
- docs/specifications/40_Time_Clocks_and_Ordering_Assumptions/README.md
- docs/specifications/42_Failure_Model_and_Fault_Tolerance/FAILURE_MODEL_AND_FAULT_CLASSIFICATION.md

## Source Planning Inputs

- docs/completed-work-plans/00-Beta1_Tasks/README.md
- docs/completed-work-plans/00-Beta1_Tasks/WORKPLAN_GENERATION_INPUT.md
- docs/completed-work-plans/00-Beta1_Tasks/DEFINITIVE_SPECSET_INDEX.md
- docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md
- docs/reference/README.md

## Current Execution Point

- B1-01-001 is completed
- B1-01-002 is completed
- B1-01-003 is completed
- B1-01-004 is completed
- B1-01-005 is completed
- B1-01-006 is completed
- lanes A and B are closed, the bounded section `31` gate bundle is preserved,
  the section `40` direct catalog proof is preserved, and this package is ready
  for historical archive use only

## Success Standard

This work-plan is complete only when:

1. B1-01-001 proves the assigned specifications are detailed enough to implement without guessing
2. every later ticket in this package closes with updated canonical specs, audit anchors, and evidence
3. the assigned Beta 1 sections are implementation-complete to their canonical standard
4. the required gate and benchmark evidence for this lane exists
5. this package can move to docs/completed-work-plans without leaving unresolved scope ambiguity

## Completion Result

- all bounded tickets `B1-01-001` through `B1-01-006` are complete
- the remaining stale section `40` audit rows are promoted to `implemented`
  using the direct catalog-contract proof preserved under
  `evidence/B1-01-006/clock_catalog_contract.log`
- this package is move-complete and archived under
  `docs/completed-work-plans/01-Core_MGA_Storage_Recovery_Buffers/`

## Historical Notes

- this completed package closes the Beta 1 storage, recovery, buffer, and
  failure-model lane assigned to sections `02,03,04,05,06,08,09,10,11,35,40,42`
- any follow-on work for this scope must open a new active work-plan rather
  than reopening this archived package in place
