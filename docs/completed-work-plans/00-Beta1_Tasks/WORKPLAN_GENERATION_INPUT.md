# Workplan Execution Input

## Intent

This directory is the active planning-generator package for the Beta 1 program.

Use it to generate the downstream standardized work-plans that will drive the
actual Beta 1 implementation work.

## Required Input Read Order

1. `README.md`
2. `CODE_TRUTH_AUDIT_MAINTENANCE_RULES.md`
3. `DEFINITIVE_SPECSET_INDEX.md`
4. `CANONICAL_GAP_REGISTER.md`
5. `BOUNDED_TICKET_SET.md`
6. `CODE_AREA_OWNERSHIP_MAP.md`
7. `BENCHMARK_AND_LOAD_SHAPE_INPUTS.md`
8. `ORDERED_TASK_TICKETS.csv`
9. `DEPENDENCY_GRAPH.csv`
10. `GATE_EVIDENCE_MATRIX.csv`
11. `EVIDENCE_EXPECTATIONS.md`
12. `RISK_DECISION_LOG.md`

## Execution Rules

1. Treat Beta 1 as the default task class. Unless a canonical file or clause is
   explicitly marked Beta 2 or Beta 3, it belongs in the Beta 1 planning set.
2. Treat mixed files as partially in scope. Only the explicit Beta 2 or Beta 3
   clauses are deferred.
3. Generate exactly the downstream work-plan set frozen in `README.md` unless
   live canonical drift creates a real blocker.
4. Every downstream work-plan must be created under `docs/work-plans/` using
   the standardized package model.
5. The first task in every generated downstream work-plan must be a
   specification sufficiency closure task for that plan's assigned scope.
6. That first task must:
   - read the assigned specifications and consumed cross-section dependencies
   - determine every process, algorithm, state machine, and plan that must be
     performed
   - identify grey areas, undefined behavior, missing elements, missing
     algorithms, or weakly defined execution steps
   - consume `docs/reference/` first
   - use web research when the required authority is not already available in
     `docs/reference/`
   - update the canonical specifications until the assigned scope is explicit
     enough to implement without guessing
7. Every downstream work-plan must include a current
   `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv` that uses `implementation_path` plus
   `unique_search_key` and never line numbers.
8. Every canonical section from `01` through `42` that is not explicitly Beta 2
   or Beta 3 must appear exactly once in primary downstream ownership, except
   for the explicit split-owner rules frozen in this package.
9. Section `00` is consumed by every generated plan as governance authority and
   does not become a standalone Beta 1 implementation lane.
10. Do not generate a downstream work-plan whose purpose is only “investigate”
   or “research”; every downstream work-plan must be implementation-driving.
11. Preserve the public-beta gate and Beta 1 cloud support authorities as the
   minimum release and operational baseline.
12. No product code changes belong to this work-plan. Its only valid output is
    the downstream work-plan program and its navigation updates.

## Required Output From Execution

Execution of this work-plan must produce:

- downstream work-plan packages `01` through `08`
- each generated downstream package with a first ticket that closes
  specification sufficiency before implementation begins
- updated `docs/work-plans/README.md` listing the active downstream plans in
  numeric order
- a complete primary-ownership map for all Beta 1 sections
- downstream evidence and gate expectations aligned to the Beta 1 release goal
- downstream audit matrices that preserve search-key-based implementation
  anchors

## Non-Negotiable Constraints

1. Do not move Beta 2 or Beta 3 work into Beta 1 unless the canonical spec is
   updated first.
2. Do not leave unowned Beta 1 sections or orphan implementation areas.
3. Do not use line-number-based audit anchors in any generated work-plan.
4. Do not collapse distinct implementation lanes just to reduce the plan count
   if that creates unsafe ownership overlap.
5. Do not split a canonical section across multiple downstream plans unless the
   split is explicitly frozen here or required by later verified drift.
6. Do not allow downstream implementation work to start before the generated
   plan's specification sufficiency closure task is complete.
7. Do not let downstream plan generation weaken MGA-first or anti-WAL
   invariants already established in canon.
