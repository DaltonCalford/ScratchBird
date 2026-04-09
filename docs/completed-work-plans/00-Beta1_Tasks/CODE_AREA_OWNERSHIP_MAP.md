# Code Area Ownership Map

## Primary Write Scopes

| Ticket | Primary write scope | Conflict surfaces | Parallelization rule |
| --- | --- | --- | --- |
| `B1P-001` | `docs/work-plans/00-Beta1_Tasks/*` | `README.md`, `DEFINITIVE_SPECSET_INDEX.md`, `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv` | do not parallelize with any other ticket touching the same package |
| `B1P-002` | `docs/work-plans/00-Beta1_Tasks/README.md`, `DEFINITIVE_SPECSET_INDEX.md`, `CODE_AREA_OWNERSHIP_MAP.md` | ownership tables and split rules | may not run in parallel with `B1P-003` |
| `B1P-003` | `docs/work-plans/00-Beta1_Tasks/README.md`, `ORDERED_TASK_TICKETS.csv`, `DEPENDENCY_GRAPH.csv`, `MASTER_TRACKER.*` | downstream numbering and dependency order | may not run in parallel with `B1P-002`, `B1P-004`, or `B1P-005` |
| `B1P-004` | `docs/work-plans/01-*` through `docs/work-plans/04-*` plus `docs/work-plans/README.md` | active numbering allocation, shared README | safe only after `B1P-003` freezes order |
| `B1P-005` | `docs/work-plans/05-*` through `docs/work-plans/08-*` plus `docs/work-plans/README.md` | active numbering allocation, shared README | safe only after `B1P-003` freezes order and should not overlap `B1P-004` on README updates without coordination |
| `B1P-006` | `docs/work-plans/README.md`, `docs/completed-work-plans/README.md`, `docs/work-plans/00-Beta1_Tasks/*` | final navigation, closeout notes | final serial closeout only |

## Unsafe Parallel Boundaries

- any ticket that edits `docs/work-plans/README.md`
- any ticket that allocates or changes downstream numeric prefixes
- any ticket that changes split ownership for section `25` or section `30`
- any ticket that updates gate or evidence expectations shared across all
  generated plans

## Safe Parallel Boundaries After Sequence Freeze

After `B1P-003` is complete, the generated downstream plans may be authored in
separate batches only if:

- their numeric prefixes are already frozen
- the shared `docs/work-plans/README.md` update is serialized
- no two authors change the same split-owner rule or shared evidence model
