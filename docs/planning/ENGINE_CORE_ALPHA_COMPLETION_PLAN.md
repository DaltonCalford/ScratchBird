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
| WS-2 Tablespace routing | In Progress |  | Alpha Core | Tablespace ID allocation aligned with reserved ID=1; root_gpid wired in catalog records; tablespace header v2 write + v1 read support; tablespace_files catalog wiring added; tablespace table/index counts maintained |
| WS-3 Index migration safety | Done |  | Alpha Core | Index migration TID updates + GiST cache cleanup verified |
| WS-4 Scheduler/job system | In Progress |  | Alpha Core | Job DDL parsing/bytecode/executor + scheduler thread in progress; cron/dependency unit tests added; maintenance jobs + audit/permission hooks added; CANCEL JOB RUN support added; maintenance/cancel tests added |
| WS-5 Constraint enforcement | TODO |  | Alpha Core |  |
| WS-6 Security enforcement | TODO |  | Alpha Core |  |
| WS-7 Monitoring parity | TODO |  | Alpha Core |  |
| WS-8 Backup/restore | TODO |  | Alpha Core |  |
| WS-9 Cache/buffer plan | In Progress |  | Alpha Core | Buffer pool flush path now skips pinned pages and guards writes with content mutex; catalog heap updates lock page content |

## Acceptance Criteria
- All workstreams complete with tests passing.
- Core engine parity matches `ENGINE_CORE_UNIFIED_SPEC.md`.
- Gap list in `ENGINE_CORE_IMPLEMENTATION_AUDIT.md` is fully resolved.

## Recent Progress
- 2026-01-23: Closed INDEX_IMPLEMENTATION_GAPS findings; index migration + GC/regression tests pass.
- 2026-01-23: Enabled GiST index cache deletion now that GiST type is integrated.
- 2026-01-23: Added Parser V2 SWEEP DATABASE support (AST/semantic/bytecode) and enabled sweep parser tests.
- 2026-01-24: Started WS-4 scheduler core: job DDL plumbing (parser/semantic/bytecode/executor) and minimal scheduler thread.
- 2026-01-25: Added cron parsing/dependency gating helpers and unit tests for scheduler.
- 2026-01-25: Seeded maintenance jobs and added job audit logging + permission checks.
- 2026-01-25: Added CANCEL JOB RUN parsing/bytecode/executor path and parser test coverage.
- 2026-01-25: Added maintenance seed validation and cancel-before-run scheduler tests.
- 2026-01-25: Fixed buffer pool TSAN race by guarding flush writes with content mutex and skipping pinned pages; catalog heap updates now lock page content; full `ctest` pass completed (2 JSON tests skipped).
- 2026-01-21: Full `ctest` run (with `SCRATCHBIRD_TEST_NETWORK=1`) completed cleanly after stabilizing TCP integration port selection and rebuilding listener/parser binaries.
- 2026-01-21: Temporary debug logging removed from listener/parser/native adapter paths after validation.
- 2026-01-21: Wired sb_tablespace_files catalog page allocation/backfill plus load/persist helpers for tablespace file paths.
- 2026-01-21: Maintained tablespace table_count/index_count on table/index create/drop and table migration.
- 2026-01-21: Storage engine now allocates/pins heap pages by tablespace (GPID-aware insert/read/update/delete paths and allocation helpers).
- 2026-01-21: Heap scans iterate GPID-based pages for non-primary tablespaces and emit TIDs with the correct tablespace.
- 2026-01-21: Legacy delete-by-TID path now honors tablespace ID overrides when pinning pages.
- 2026-01-21: B-tree and Hash index storage now use root_gpid tablespace routing for pin/unpin and allocation.
