# Workplan Execution Input

## Intent

This directory is the active implementation package for the Service Stack Local IPC Wire Listeners Manager lane of
Beta 1.

## Required Input Read Order

1. README.md
2. CODE_TRUTH_AUDIT_MAINTENANCE_RULES.md
3. DEFINITIVE_SPECSET_INDEX.md
4. CANONICAL_GAP_REGISTER.md
5. BOUNDED_TICKET_SET.md
6. CODE_AREA_OWNERSHIP_MAP.md
7. BENCHMARK_AND_LOAD_SHAPE_INPUTS.md
8. ORDERED_TASK_TICKETS.csv
9. DEPENDENCY_GRAPH.csv
10. GATE_EVIDENCE_MATRIX.csv
11. EVIDENCE_EXPECTATIONS.md
12. RISK_DECISION_LOG.md

## Execution Rules

1. B1-05-001 must be executed first.
2. B1-05-001 must read the assigned specs and dependencies, determine all
   required processes and plans, and identify grey areas, undefined behavior,
   missing information, missing algorithms, weakly defined payloads, or weakly
   defined step-by-step execution rules.
3. Use docs/reference first. Use web research only when the local reference
   tree does not contain the required authority.
4. Update the canonical specifications until the assigned scope is explicit
   enough that implementation can proceed without guessing or hallucinating.
5. No later ticket may begin until B1-05-001 is closed.
6. Keep SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv current using only
   project-root-relative implementation paths plus file-local unique_search_key
   values.
7. Preserve MGA-first and anti-WAL invariants.
8. Keep work bounded to the sections owned by this package.

## Required Output From Execution

- completed tickets for this package
- updated assigned canonical specs
- current search-key audit matrix for the assigned implementation surfaces
- gate and benchmark evidence required by this lane
- final move-ready closeout state for this package
