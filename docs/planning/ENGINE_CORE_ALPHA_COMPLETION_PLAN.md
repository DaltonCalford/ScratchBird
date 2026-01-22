# Engine Core Alpha Completion Plan

## Purpose
Provide a single tracked plan for completing the Alpha core engine gaps
identified in the unified spec and implementation audits.

## Scope
Core engine only: storage, catalog, transactions, executor, indexes, security,
monitoring, backup/restore, scheduler. Parsers, network/listeners, drivers,
cluster/sharding, and tooling are out of scope.

## Inputs
- `ScratchBird/docs/specifications/core/ENGINE_CORE_UNIFIED_SPEC.md`
- `ScratchBird/docs/findings/ENGINE_CORE_IMPLEMENTATION_AUDIT.md`
- `ScratchBird/docs/findings/INDEX_IMPLEMENTATION_GAPS.md`
- `ScratchBird/docs/findings/TABLESPACE_IMPLEMENTATION_AUDIT.md`
- `ScratchBird/docs/findings/SCHEDULER_JOB_RUNNER_AUDIT.md`
- `ScratchBird/docs/findings/MGA_GC_THREAD_AUDIT.md`
- `ScratchBird/docs/findings/CACHE_AND_BUFFER_IMPLEMENTATION_REVIEW.md`
- `ScratchBird/docs/planning/TABLESPACE_REMEDIATION_PLAN.md`
- `ScratchBird/docs/planning/CACHE_AND_BUFFER_REMEDIATION_PLAN.md`

## Execution Order (recommended)
1. Catalog bootstrap schema roots.
2. Tablespace routing + GPID wiring (follow tablespace plan).
3. Index TID updates + migration safety for all index types.
4. Scheduler/job system.
5. Constraint enforcement (PK/FK/UNIQUE/CHECK/NOT NULL).
6. Security enforcement gaps (view definer checks + RLS SELECT).
7. Monitoring view parity (sys.* and MON$ sources).
8. Backup/restore validation for multi-tablespace coverage.

## Workstreams

### WS-1 Catalog Bootstrap and Schema Roots
- [ ] Remove `/emulated` root; use `/remote/emulation`.
- [ ] Move `/public` under `/users/public`.
- [ ] Add migration/repair logic for existing catalogs if needed.
- [ ] Update any catalog bootstrap tests to assert canonical roots.

### WS-2 Tablespace Routing + GPID Wiring
- [ ] Execute `ScratchBird/docs/planning/TABLESPACE_REMEDIATION_PLAN.md` phases.
- [ ] Confirm CREATE TABLE/INDEX placement honors TABLESPACE and schema defaults.
- [ ] Ensure GPID-aware DML across heap/index/TOAST paths.

### WS-3 Index Migration Safety
- [ ] Implement TID updates for all non-BTree/Hash index types listed in
      `ScratchBird/docs/findings/INDEX_IMPLEMENTATION_GAPS.md`.
- [ ] Fix GiST cache cleanup once type integration is complete.
- [ ] Add tests covering migrations and index integrity rebuilds.

### WS-4 Scheduler and Job System
- [ ] Implement secure job runner and scheduling model per scheduler specs.
- [ ] Add maintenance jobs (sweep, GC, stats, index rebuild) as first-class jobs.
- [ ] Add permissions and audit hooks for job creation/execution.

### WS-5 Constraint Enforcement
- [ ] Enforce PK/UNIQUE on insert/update.
- [ ] Enforce FK with CASCADE/RESTRICT handling and dependency validation.
- [ ] Enforce CHECK constraints.
- [ ] Confirm NOT NULL enforcement in all DML paths.
- [ ] Add tests for constraint failures and dependency rules.

### WS-6 Security Enforcement (AuthZ + RLS)
- [ ] Implement view definer permission checks (table/column) in view security.
- [ ] Add RLS SELECT enforcement in executor.
- [ ] Add tests for authz + RLS policies across DML/SELECT.

### WS-7 Monitoring View Parity
- [ ] Implement sys.sessions, sys.transactions, sys.locks, sys.statements,
      sys.performance in analyzer + executor.
- [ ] Replace MON$ placeholders with real engine state in Firebird catalog.
- [ ] Align view columns with `MONITORING_SQL_VIEWS.md`.

### WS-8 Backup/Restore Coverage
- [ ] Validate backup includes all tablespace datafiles and catalog objects.
- [ ] Validate restore rejects missing tablespace datafiles (unless recovery mode).
- [ ] Add integration tests for multi-tablespace backup/restore.

### WS-9 Cache/Buffer Plan (parallel stream)
- [ ] Execute `ScratchBird/docs/planning/CACHE_AND_BUFFER_REMEDIATION_PLAN.md`.

## Tracking Table
Update this table as work progresses.

| Workstream | Status | Owner | Milestone | Notes |
| --- | --- | --- | --- | --- |
| WS-1 Catalog bootstrap | Done |  | Alpha Core | Root paths updated; migration/repair pass added |
| WS-2 Tablespace routing | In Progress |  | Alpha Core | Tablespace ID allocation aligned with reserved ID=1; root_gpid wired in catalog records; tablespace header v2 write + v1 read support |
| WS-3 Index migration safety | TODO |  | Alpha Core |  |
| WS-4 Scheduler/job system | TODO |  | Alpha Core |  |
| WS-5 Constraint enforcement | TODO |  | Alpha Core |  |
| WS-6 Security enforcement | TODO |  | Alpha Core |  |
| WS-7 Monitoring parity | TODO |  | Alpha Core |  |
| WS-8 Backup/restore | TODO |  | Alpha Core |  |
| WS-9 Cache/buffer plan | TODO |  | Alpha Core |  |

## Acceptance Criteria
- All workstreams complete with tests passing.
- Core engine parity matches `ENGINE_CORE_UNIFIED_SPEC.md`.
- Gap list in `ENGINE_CORE_IMPLEMENTATION_AUDIT.md` is fully resolved.

## Recent Progress
- 2026-01-21: Full `ctest` run (with `SCRATCHBIRD_TEST_NETWORK=1`) completed cleanly after stabilizing TCP integration port selection and rebuilding listener/parser binaries.
- 2026-01-21: Temporary debug logging removed from listener/parser/native adapter paths after validation.
