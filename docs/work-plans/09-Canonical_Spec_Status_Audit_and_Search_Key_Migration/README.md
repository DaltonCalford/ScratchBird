# 09-Canonical_Spec_Status_Audit_and_Search_Key_Migration

Status: active_workplan

## Purpose

This work-plan verifies the current truth of the entire canonical
specification tree against the live implementation, replaces fragile
line-number implementation references with stable search-key anchors, and
produces the definitive finished, partial, and outstanding specification
inventories for the current ScratchBird codebase.

## Prerequisite Status

- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md` is the scope controller
  for this package.
- `docs/specifications/00_Governance_and_Invarients/WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md`
  is the governing standard for this package.
- the recent Beta 2 canonical expansion work is in scope for verification; this
  package does not assume those status markers are already correct
- no implementation status claim is trusted until it is re-audited against live
  code and tests

## Scope

- audit every authoritative specification file listed in
  `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- verify every status claim that matters for implementation reality:
  implemented, partial, fail-closed boundary, reconstructed, drift, or
  unsupported
- detect both directions of drift:
  - implementation exists but the documentation is incomplete or stale
  - the documentation overclaims implementation or marks a surface partial or
    implemented when code truth does not support it
- replace line-number implementation references in canonical specs with
  `implementation_path + unique_search_key` anchors
- insert stable unique identifiers into implementation files when no durable
  search key exists yet
- keep a machine-readable audit matrix current while the verification proceeds
- produce final rollups:
  - `FINISHED_SPECIFICATIONS.md`
  - `PARTIAL_SPECIFICATIONS.md`
  - `OUTSTANDING_SPECIFICATIONS.md`
  - `SPEC_STATUS_CLASSIFICATION.csv`
  - `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv`

## Non-Goals

- no new feature design work unless the audit proves the current canon is
  insufficient and that insufficiency itself becomes a recorded gap
- no behavior-changing code work beyond the minimum safe insertion of stable
  audit identifiers or equivalent owned search keys
- no line-number-based implementation anchors in any updated work-plan output
- no silent promotion of checklist-only material to implemented status without
  source-backed proof

## Contents

- README.md
- WORKPLAN_GENERATION_INPUT.md
- DEFINITIVE_SPECSET_INDEX.md
- CANONICAL_GAP_REGISTER.md
- BOUNDED_TICKET_SET.md
- CODE_AREA_OWNERSHIP_MAP.md
- CODE_TRUTH_AUDIT_MAINTENANCE_RULES.md
- SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv
- MASTER_TRACKER.md
- MASTER_TRACKER.csv
- ORDERED_TASK_TICKETS.csv
- DEPENDENCY_GRAPH.csv
- GATE_EVIDENCE_MATRIX.csv
- EVIDENCE_EXPECTATIONS.md
- RISK_DECISION_LOG.md
- SPEC_STATUS_CLASSIFICATION.csv
- LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv
- FINISHED_SPECIFICATIONS.md
- PARTIAL_SPECIFICATIONS.md
- OUTSTANDING_SPECIFICATIONS.md
- evidence/README.md
- gates/README.md

## Primary Canonical Targets

- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- `docs/specifications/Reference_Documentation_specification.md`
- every authoritative spec file listed in the inventory across sections
  `00_Governance_and_Invarients` through
  `42_Failure_Model_and_Fault_Tolerance`
- section README and `TEST_CONTRACT.md` files where they define status or
  closure expectations
- completed and active work-plan material only where it currently owns search
  key rules, audit practices, or bounded closure evidence

## Implementation Roots To Audit

- `ScratchBird/include/`
- `ScratchBird/src/`
- `ScratchBird/tests/`
- `ScratchBird/scripts/`
- `ScratchBird/docs/` when repo-local docs encode current implementation entry
  points
- `../ScratchBird-driver/` when canonical specs point to maintained driver
  implementation
- `../ScratchBird-Benchmarks/` when canonical specs point to maintained
  benchmark or gate implementation
- `../ScratchRobin/` when canonical specs point to maintained client or UI
  implementation

## Source Planning Inputs

- `docs/specifications/00_Governance_and_Invarients/WORK_PLAN_MANAGEMENT_STANDARD_AND_LIFECYCLE.md`
- `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`
- `docs/specifications/README.md`
- `docs/work-plans/08-Tooling_Drivers_Benchmarks_Gates_Release/README.md`
- `docs/reference/README.md`

## Current Execution Point

- `SV-09-001` is the active starting ticket
- the package has not yet frozen the authoritative file census into a
  row-complete audit matrix
- the package has not yet converted existing line-number implementation
  references into search-key references
- the final finished, partial, and outstanding lists remain intentionally empty
  until the section-by-section audit closes

## Success Standard

This work-plan is complete only when:

1. every authoritative specification file has at least one current audit row in
   `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
2. no canonical spec in scope still relies on a line-number implementation
   reference where an owned search key can be used instead
3. every required new implementation anchor inserted for audit stability is
   recorded in `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv`
4. every implementation-status claim in scope is either confirmed and aligned
   or corrected in canon with preserved evidence
5. `FINISHED_SPECIFICATIONS.md`, `PARTIAL_SPECIFICATIONS.md`, and
   `OUTSTANDING_SPECIFICATIONS.md` are generated from the same current audit
   matrix and are mutually consistent
6. the package can move to `docs/completed-work-plans/` without leaving scope
   ambiguity about which authoritative specs remain unfinished
