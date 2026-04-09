# Work Plan Management Standard and Lifecycle

Status: current_authority

## 1. Purpose

Define the mandatory structure, naming, placement, lifecycle, and navigation
rules for all new standardized ScratchBird work-plans.

This standard exists so active execution packages are:

- discoverable
- consistently named
- bounded
- auditable
- easy to hand to another implementation agent without rediscovering scope
- stable under code churn where implementation audit anchors must survive line
  movement

## 2. Scope

This standard applies to all new work-plans created after adoption of this
rule.

This standard does not require migration of legacy planning trees or archived
planning packages that already exist under `local_work`.

## 3. Canonical Locations

### 3.1 Active work-plan root

All new standardized work-plans must be created under:

- `docs/work-plans/`

### 3.2 Completed work-plan root

When a work-plan is complete, the full work-plan directory must be moved to:

- `docs/completed-work-plans/`

### 3.3 Non-work-plan planning locations

The following remain valid for planning material, but they are not the
canonical home for new standardized work-plans:

- `docs/specifications/work/` for findings, audits, migration notes, and
  spec-tied work artifacts
- `../local_work/docs/planning/` for broader planning inventories, archives,
  and historical worktrees

## 4. Naming Standard

### 4.1 Directory naming

Every active or completed work-plan must be a directory named:

- `NN-Descriptive_Name`

Required rules:

- `NN` is the implementation-order prefix
- use zero-padded decimal numbering
- use at least two digits
- widen only if the project exceeds `99` active historical sequence numbers
- `Descriptive_Name` uses ASCII letters, digits, and underscores only
- do not use spaces
- do not use ad hoc suffixes such as `final`, `v2`, `new`, or `fixed`
  inside the canonical directory name

Example:

- `01-Scan_Specifications_Determine_B1_items`

### 4.2 Ordering rule

The numeric prefix is the authoritative execution order for the work-plan
program. New work-plans must receive the next available number in sequence.

### 4.3 Completion move rule

Completed work-plans keep the same directory name when moved to
`docs/completed-work-plans/`.

If follow-on work is needed after completion, create a new work-plan with a
new sequence number. Do not reopen a completed work-plan directory in place.

## 5. Work-Plan Package Model

### 5.1 Package, not single file

A standardized work-plan is a bounded directory package, not a single markdown
file.

### 5.2 Minimum required file set

Every new work-plan directory must contain:

- `README.md`
- `WORKPLAN_GENERATION_INPUT.md`
- `DEFINITIVE_SPECSET_INDEX.md`
- `CANONICAL_GAP_REGISTER.md`
- `BOUNDED_TICKET_SET.md`
- `CODE_AREA_OWNERSHIP_MAP.md`
- `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
- `MASTER_TRACKER.md`
- `MASTER_TRACKER.csv`
- `ORDERED_TASK_TICKETS.csv`
- `DEPENDENCY_GRAPH.csv`
- `GATE_EVIDENCE_MATRIX.csv`
- `EVIDENCE_EXPECTATIONS.md`
- `RISK_DECISION_LOG.md`
- `evidence/README.md`
- `gates/README.md`

### 5.3 Conditionally required control files

Add these when the work-plan needs them:

- `CODE_TRUTH_AUDIT_MAINTENANCE_RULES.md`
  : required when the plan touches code surfaces where local audit legibility
  and code-first proof anchors matter
- `BENCHMARK_AND_LOAD_SHAPE_INPUTS.md`
  : required when performance, pressure shape, benchmark, or gate-load
  behavior is part of the execution program

### 5.4 Allowed additional files

Additional files are allowed only when they serve the bounded work-plan
directly and are listed in `README.md`.

## 6. Required Content Model

### 6.1 `README.md`

The work-plan `README.md` must define the package as an execution control
point and must include, at minimum:

- purpose
- prerequisite status
- scope
- non-goals
- contents
- primary canonical targets
- cross-section or consumed baseline inputs where relevant
- source planning inputs
- current execution point
- status notes

### 6.2 `WORKPLAN_GENERATION_INPUT.md`

This file must tell the implementing agent how to consume the package and must
include, at minimum:

- intent
- required input read order
- execution rules
- required output from implementation work
- non-negotiable constraints

### 6.3 `DEFINITIVE_SPECSET_INDEX.md`

This file must enumerate the authoritative spec set, findings, live code
anchors, and test anchors that define the work-plan scope.

All live code anchors in this file shall use:

- implementation path relative to the project root
- one file-local `unique_search_key`

Line-number anchors are prohibited in work-plan authority because they are too
fragile under active implementation churn.

### 6.4 `CANONICAL_GAP_REGISTER.md`

This file must record the concrete implementation or closure gaps that justify
the work-plan, with explicit gap IDs, code anchors, spec anchors, and closing
tickets.

Every recorded code anchor shall use the canonical search-key format defined by
this standard.

### 6.5 `BOUNDED_TICKET_SET.md`

This file must declare the bounded ticket inventory and the no-uncontrolled-
scope-expansion rule.

Each ticket entry must state:

- ticket ID
- current status
- intended outcome

### 6.6 `CODE_AREA_OWNERSHIP_MAP.md`

This file must map tickets to their primary write scopes, conflict files, and
safe or unsafe parallelization boundaries.

### 6.7 `MASTER_TRACKER.md`

This file must provide the human-readable master tracker table and must use
these columns:

- `Ticket`
- `Title`
- `Status`
- `Depends On`
- `Primary Evidence`

### 6.8 `MASTER_TRACKER.csv`

This file must provide the machine-readable master tracker and must use these
headers:

- `ticket`
- `title`
- `status`
- `depends_on`
- `primary_evidence`

### 6.8A `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`

This file is mandatory for every new standardized work-plan.

It must provide the machine-readable map between canonical spec elements and
their current implementation audit anchors.

It must use these headers:

- `spec_path`
- `spec_element`
- `ticket`
- `implementation_status`
- `implementation_path`
- `unique_search_key`
- `notes`

Required rules:

- `spec_path` points to the canonical specification file being updated or
  audited
- `spec_element` identifies the relevant requirement, subsection, or
  implementation-owned behavior within that file
- `implementation_status` states the current reality, such as
  `implemented`, `partial`, `reconstructed_required`, `drift`, or
  `unsupported_boundary`
- `implementation_path` points to one concrete implementation file
- `unique_search_key` is the exact text the auditor should search for inside
  that file
- `notes` may record scope limits or additional audit guidance

This matrix exists so work-plans remain audit-stable even when line numbers
move.

### 6.9 `ORDERED_TASK_TICKETS.csv`

This file must provide the ordered ticket list and must use these headers:

- `order`
- `ticket`
- `title`
- `phase`
- `status`

### 6.10 `DEPENDENCY_GRAPH.csv`

This file must define ticket dependencies and must use these headers:

- `ticket`
- `depends_on`
- `rationale`

### 6.11 `GATE_EVIDENCE_MATRIX.csv`

This file must define the gate-to-evidence relationship and must use these
headers:

- `gate_id`
- `focus`
- `required_tickets`
- `required_evidence`

### 6.12 `EVIDENCE_EXPECTATIONS.md`

This file must state:

- the general evidence rule for every ticket
- the ticket-specific evidence outputs required for closure

### 6.13 `RISK_DECISION_LOG.md`

This file must capture:

- fixed decisions
- initial or evolving risks
- mitigations
- final closeout note when the program ends

### 6.14 `evidence/README.md`

This file must define how ticket evidence is named, grouped, and stored under
the work-plan package.

### 6.15 `gates/README.md`

This file must define how gate-specific artifacts, results, or certification
bundles are placed under the work-plan package.

## 7. Quality and Style Rules

### 7.1 Bounded execution

Every work-plan must be explicitly bounded. It must not act as an open-ended
research bucket.

### 7.2 Ticket-first execution

Execution packages must be ticket-driven, not prose-driven. A reader must be
able to identify:

- the active ticket
- completed tickets
- remaining tickets
- ticket dependencies

without re-reading the full specification tree.

### 7.3 Evidence-first closure

Completion claims are invalid unless the required tracker, gate, and evidence
artifacts named by the work-plan exist.

### 7.4 Code-truth alignment

If the work-plan governs implementation work, it must identify the live code
anchors and the canonical spec anchors together. It may not rely on prose-only
closure.

### 7.5 One authoritative program per directory

Each work-plan directory must contain one authoritative bounded execution
program. Do not mix multiple unrelated programs into one work-plan package.

### 7.6 Audit-stable implementation anchor rule

All implementation references recorded by a standardized work-plan shall use:

- `path/file`
- one file-local `unique_search_key`

They shall not use:

- `:line`
- `#Lline`
- line ranges
- "nearby" prose without a concrete search key

The `unique_search_key` must be:

- easy to search inside the target file
- narrow enough to identify the intended implementation seam
- stable enough to survive ordinary code insertions and deletions

Good examples include:

- stable function or method names
- enum labels
- metric names
- structured comment tags
- stable string literals or configuration keys that are unique within the file

If no stable search key exists, the work-plan shall treat creation or
identification of one as part of the implementation-audit closure work.

### 7.7 Spec-maintenance rule for implementation anchors

A new standardized work-plan does not merely track ticket progress.

It must also drive canonical spec maintenance so the affected specification
files disclose, as part of their implementation-audit posture:

- current implementation status
- where the implementation can be found
- the `unique_search_key` to use for audit lookup

If a ticket changes the understood implementation anchor, the corresponding
canonical spec entry must be updated before the ticket may be closed.

## 8. Lifecycle Rules

### 8.1 Creation

When a new work-plan is created:

- assign the next available sequence number
- create the full package under `docs/work-plans/`
- update `docs/work-plans/README.md`

### 8.2 Active maintenance

While active:

- keep tracker files current
- keep the current execution point explicit in `README.md`
- keep evidence and gate expectations aligned with the active ticket set
- keep `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv` current
- keep affected canonical specification files updated with current
  implementation status and search-key-based audit anchors where the work-plan
  owns that maintenance

### 8.3 Completion

When complete:

- move the entire directory from `docs/work-plans/` to
  `docs/completed-work-plans/`
- preserve the directory name
- update `docs/work-plans/README.md`
- update `docs/completed-work-plans/README.md`

### 8.4 Post-completion rule

Completed work-plans are historical execution packages. They may receive
clerical link or navigation fixes, but they must not be reopened as active
execution control points.

## 9. Root Navigation Requirements

### 9.1 `docs/work-plans/README.md`

This README must:

- explain the naming rule
- point to this standard
- list active work-plans in numeric order
- state when there are no active work-plans

### 9.2 `docs/completed-work-plans/README.md`

This README must:

- explain that completed plans are historical execution packages
- list completed work-plans in numeric order
- state when there are no completed work-plans

## 10. Non-Migration Rule

Do not retroactively move or normalize older planning trees just to satisfy
this standard.

Legacy worktrees may remain in archive or `local_work` planning locations.
This standard governs new work-plans only.
