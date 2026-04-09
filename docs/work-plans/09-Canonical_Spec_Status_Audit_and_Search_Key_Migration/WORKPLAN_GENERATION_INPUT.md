# Workplan Execution Input

## Intent

This directory is the active implementation-audit package for full canonical
specification status verification and search-key migration.

## Required Input Read Order

1. README.md
2. CODE_TRUTH_AUDIT_MAINTENANCE_RULES.md
3. DEFINITIVE_SPECSET_INDEX.md
4. CANONICAL_GAP_REGISTER.md
5. BOUNDED_TICKET_SET.md
6. CODE_AREA_OWNERSHIP_MAP.md
7. ORDERED_TASK_TICKETS.csv
8. DEPENDENCY_GRAPH.csv
9. GATE_EVIDENCE_MATRIX.csv
10. EVIDENCE_EXPECTATIONS.md
11. RISK_DECISION_LOG.md
12. `docs/specifications/AUTHORITATIVE_SPEC_INVENTORY.md`

## Execution Rules

1. `SV-09-001` must execute first and freeze the status vocabulary,
   implementation-proof rules, and audit scope for the package.
2. `SV-09-002` must expand `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv` from its
   bootstrap state to a row-complete inventory that covers every authoritative
   spec file in scope.
3. Use live implementation first when determining current behavior. Use
   `docs/reference` and official web sources only when code truth is
   insufficient to classify a status claim.
4. Never preserve a line-number implementation anchor when an owned search key
   can exist instead.
5. If no durable search key exists in an owned implementation file, create one
   as part of the current ticket and record it in the audit matrix and the
   migration log.
6. Every updated canonical spec must point auditors to
   `implementation_path + unique_search_key`, not to line numbers.
7. When code and canon disagree, record the discrepancy first, then correct the
   canon or status label explicitly. Do not silently let the implementation win
   without a documented audit result.
8. Final rollup status classes are:
   - `finished`
   - `partial`
   - `outstanding`
9. Intermediate audit statuses may be richer, including:
   - `implemented`
   - `partial`
   - `reconstructed_required`
   - `drift`
   - `unsupported_boundary`
   - `unknown_pending_audit`
10. The final rollups must map intermediate statuses deterministically:
    - `finished` := implemented and canon aligned
    - `partial` := partial, drift, implemented_but_doc_stale, or partially
      fail-closed
    - `outstanding` := reconstructed required, unsupported, unimplemented, or
      still unknown at closeout
11. Keep `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`,
    `SPEC_STATUS_CLASSIFICATION.csv`, and `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv`
    current throughout execution.
12. Preserve MGA-first, UUID-first, parser-boundary, and anti-WAL invariants
    while auditing and correcting status claims.

## Required Output From Execution

- a row-complete `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
- a complete `SPEC_STATUS_CLASSIFICATION.csv`
- a complete `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv`
- updated canonical specs with search-key implementation anchors
- `FINISHED_SPECIFICATIONS.md`
- `PARTIAL_SPECIFICATIONS.md`
- `OUTSTANDING_SPECIFICATIONS.md`
- corrected status claims wherever canon drift is proven
- package trackers updated to a move-ready closeout state

## Non-Negotiable Constraints

- do not use line numbers as durable implementation anchors
- do not mark a surface implemented without source-backed proof
- do not preserve known overclaims once they are identified
- do not insert audit identifiers into third-party or preserved-tree code; use
  owned wrappers or owned integration files instead
