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
- `ScratchBird/docs/findings/INDEX_IMPLEMENTATION_GAPS-completed.md`
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
- [x] Confirm CREATE TABLE/INDEX placement honors TABLESPACE and schema defaults.
- [x] Allocate table root pages via tablespace-aware APIs in catalog manager.
- [x] Ensure GPID-aware DML across heap/index/TOAST paths.

### WS-3 Index Migration Safety

- [x] Implement TID updates for all non-BTree/Hash index types listed in
  
      `ScratchBird/docs/findings/INDEX_IMPLEMENTATION_GAPS-completed.md`.
- [x] Fix GiST cache cleanup once type integration is complete.
- [ ] Add tests covering migrations and index integrity rebuilds.

### WS-4 Scheduler and Job System

- [x] Implement secure job runner and scheduling model per scheduler specs.
- [x] Add maintenance jobs (sweep, GC, stats, index rebuild) as first-class jobs.
- [x] Add permissions and audit hooks for job creation/execution.

#### WS-4 Gap Checklist (spec-driven)

- [x] WS4-GAP-01 Persist job_class in catalog records and surface it in JobInfo (`include/scratchbird/core/catalog_manager.h`, `src/core/catalog_manager.cpp`, `src/sblr/executor.cpp`).
- [x] WS4-GAP-02 Persist SINGLE_SHARD UUID for partitioned jobs (`src/sblr/executor.cpp`, `include/scratchbird/core/catalog_manager.h`, `src/core/catalog_manager.cpp`).
- [x] WS4-GAP-03 Parse and store PARTITION BY SHARD_SET/DYNAMIC lists/expressions per spec (`src/parser/parser_v2.cpp`).
- [x] WS4-GAP-04 Expose sys.jobs/sys.job_runs/sys.job_dependencies as queryable sys.* tables (`src/catalog/sys_catalog.cpp`, `include/scratchbird/catalog/sys_catalog.h`, `include/scratchbird/catalog/virtual_catalog.h`, `src/catalog/virtual_catalog.cpp`).
- [x] WS4-GAP-05 Add catalog indexes for jobs/runs lookup and scheduling windows (`src/core/catalog_manager.cpp`, `include/scratchbird/core/catalog_manager.h`).
- [x] WS4-GAP-06 Persist job_run result_data payloads (`include/scratchbird/core/catalog_manager.h`, `src/core/catalog_manager.cpp`, `src/catalog/sys_catalog.cpp`).
- [x] WS4-GAP-07 Represent PENDING job runs before execution starts (`src/core/job_scheduler.cpp`).
- [x] WS4-GAP-08 Implement catch_up = all scheduling behavior (`src/core/job_scheduler.cpp`).
- [x] WS4-GAP-09 Enforce SQL job timeouts during execution (not just post-run) (`src/core/job_scheduler.cpp`).
- [x] WS4-GAP-10 Add sys.performance scheduler metrics surface (`src/catalog/sys_catalog.cpp`).
- [x] WS4-GAP-11 Add SQL/DDL surface for job secrets management (`src/parser/parser_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/executor.cpp`).

### WS-5 Constraint Enforcement

- [x] Enforce PK/UNIQUE on insert/update.
- [x] Enforce FK with CASCADE/RESTRICT handling and dependency validation.
- [x] Enforce CHECK constraints.
- [x] Confirm NOT NULL enforcement in all DML paths.
- [x] Add tests for constraint failures and dependency rules.

### WS-6 Security Enforcement (AuthZ + RLS)

- [x] Implement view definer permission checks (table/column) in view security.
- [x] Add RLS SELECT enforcement in executor.
- [x] Add tests for authz + RLS policies across DML/SELECT.

### WS-7 Monitoring View Parity

- [x] Implement sys.sessions, sys.transactions, sys.locks, sys.statements,
  
      sys.performance in analyzer + executor.
- [x] Replace remaining MON$ placeholders with real engine state in Firebird catalog.
- [x] Align view columns with `MONITORING_SQL_VIEWS.md`.
- [x] WS7-GAP-01 Add MON$LOCKS table support and map to sys.locks + sys.sessions. `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-02 Add MON$SYSTEM_FLAG (constant 0) to MON$ATTACHMENTS output. `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-03 Align MON$DATABASE column naming/metrics to spec (MON$ALLOCATED_PAGES; use sys.performance for oldest/next transaction). `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-04 Align MON$TRANSACTIONS isolation mapping and prefer sys.performance OIT/OAT when available. `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-05 Replace MON$IO_STATS snapshot path with sys.io_stats via VirtualCatalogRouter. `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-06 Implement MON$CALL_STACK real data (define mapping once available). `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-07 Implement MON$RECORD_STATS real data (likely from sys.table_stats counters). `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-08 Implement MON$MEMORY_USAGE real data (requires sys.performance memory metrics). `src/catalog/firebird_catalog.cpp`, `src/catalog/sys_catalog.cpp`
- [x] WS7-GAP-09 Implement MON$CONTEXT_VARIABLES real data (session/txn context variables). `src/catalog/firebird_catalog.cpp`, `src/catalog/sys_catalog.cpp`
- [x] WS7-GAP-10 Expand MON$TABLE_STATS beyond stat_id/table_name if required; align spec. `src/catalog/firebird_catalog.cpp`, `docs/specifications/operations/MONITORING_DIALECT_MAPPINGS.md`
- [x] WS7-GAP-11 Expand MON$RECORD_STATS columns to match Firebird spec; map seq/idx reads and row mutations. `src/catalog/firebird_catalog.cpp`, `include/scratchbird/catalog/emulation_view_generator.h`
- [x] WS7-GAP-12 Expand MON$CALL_STACK columns to match Firebird spec (call metadata placeholders). `src/catalog/firebird_catalog.cpp`, `include/scratchbird/catalog/emulation_view_generator.h`
- [x] WS7-GAP-13 Expand MON$STATEMENTS columns to match Firebird spec (plan/timeout placeholders). `src/catalog/firebird_catalog.cpp`
- [x] WS7-GAP-14 Implement sys.io_stats and wire MON$IO_STATS mapping. `src/catalog/sys_catalog.cpp`, `include/scratchbird/catalog/sys_catalog.h`
- [x] WS7-GAP-15 Implement MON$COMPILED_STATEMENTS (compiled statement metadata placeholders). `src/catalog/firebird_catalog.cpp`, `include/scratchbird/catalog/emulation_view_generator.h`
- [x] WS7-GAP-16 Populate MON$CALL_STACK caller IDs/object names/types using security stack. `src/catalog/firebird_catalog.cpp`, `include/scratchbird/core/connection_context.h`
- [x] WS7-GAP-17 Expand MON$CALL_STACK to include per-connection stacks across sessions. `src/catalog/firebird_catalog.cpp`, `src/core/database.cpp`, `include/scratchbird/core/database.h`
- [x] WS7-GAP-18 Wire MON$CALL_STACK timestamps/source line/column from executor tracking. `src/catalog/firebird_catalog.cpp`, `src/core/connection_context.cpp`, `src/core/database.cpp`
- [x] WS7-GAP-19 Emit bytecode debug spans and consume them in executor for source line/column updates. `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/executor.cpp`
- [x] WS7-GAP-20 Emit debug spans from protocol-specific parsers (PG/MySQL) for source line/column tracking. `src/parser/postgresql/pg_parser.cpp`, `src/parser/mysql/mysql_parser.cpp`

### WS-8 Backup/Restore Coverage

- [x] Validate backup includes all tablespace datafiles and catalog objects.
- [x] Validate restore rejects missing tablespace datafiles (unless recovery mode).
- [x] Add tests for multi-tablespace backup/restore manifest/restore behavior.
- [x] Restore multi-file tablespaces from all file_paths (per-file ranges honored).

### WS-9 Cache/Buffer Plan (parallel stream)

- [ ] Execute `ScratchBird/docs/planning/CACHE_AND_BUFFER_REMEDIATION_PLAN.md`.

## Tracking Table

Update this table as work progresses.

| Workstream                  | Status      | Owner | Milestone  | Notes                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                               |
| --------------------------- | ----------- | ----- | ---------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| WS-1 Catalog bootstrap      | Done        |       | Alpha Core | Root paths updated; migration/repair pass added                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| WS-2 Tablespace routing     | Done        |       | Alpha Core | Tablespace ID allocation aligned with reserved ID=1; root_gpid wired in catalog records; tablespace_files catalog wiring added; tablespace table/index counts maintained; CREATE TABLE/INDEX honors TABLESPACE and schema defaults; GPID-aware DML wired                                                                                                                                                                                                                                                                                                                                                                                                                     |
| WS-3 Index migration safety | Done        |       | Alpha Core | Index migration TID updates + GiST cache cleanup verified                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                           |
| WS-4 Scheduler/job system   | Done        |       | Alpha Core | Job DDL parsing/bytecode/executor + scheduler thread complete; cron/dependency unit tests added; maintenance jobs + audit/permission hooks added; CANCEL JOB RUN support added; maintenance/cancel tests added; scheduler.enabled + pre_execute_delay_ms config wiring added; ALTER SYSTEM runtime scheduler config + job timeout handling added; scheduler max_concurrent_jobs/job_timeout_seconds config defaults wired; concurrent job execution + job DDL coverage expanded; SHOW JOBS/JOB RUNS parser + job GRANT/REVOKE support added; CREATE OR ALTER/RECREATE/expanded ALTER support for job body/dependencies/on-completion/partitioning; scheduler TSAN teardown join fix |
| WS-5 Constraint enforcement | Done        |       | Alpha Core | PK/UNIQUE enforcement now covers unique indexes and COPY composite constraints; FK validation now enforces referenced PK/UNIQUE keys; CHECK/NOT NULL enforcement covers MERGE and FK actions; constraint failure/dependency tests added and passing                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| WS-6 Security enforcement   | Done        |       | Alpha Core | RLS SELECT filtering added to row/aggregate scans; view security definer override wired into permission checks; integration tests cover RLS filtering and DML WITH CHECK enforcement                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  |
| WS-7 Monitoring parity      | Done        |       | Alpha Core | sys.sessions/sys.transactions/sys.locks/sys.statements/sys.performance now available via sys catalog; Firebird MON$ database/attachments/transactions/statements now map to sys.* views; COPY + query running metrics wired                                                                                                                                                                                                                                                                                                                                                                                                                                                          |
| WS-8 Backup/restore         | Done        |       | Alpha Core | Tablespace manifest in backup header; restore enforces missing tablespace files unless allow-create; per-file range restore honored                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   |
| WS-9 Cache/buffer plan      | Done        |       | Alpha Core | Result cache get path stabilized; full sequential `ctest` pass completed (2406 tests, 0 failures)                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                    |

## Acceptance Criteria

- All workstreams complete with tests passing.
- Core engine parity matches `ENGINE_CORE_UNIFIED_SPEC.md`.
- Gap list in `ENGINE_CORE_IMPLEMENTATION_AUDIT.md` is fully resolved.

## Recent Progress

- 2026-01-23: Closed INDEX_IMPLEMENTATION_GAPS findings; index migration + GC/regression tests pass.
- 2026-01-23: Enabled GiST index cache deletion now that GiST type is integrated.
- 2026-01-23: Added Parser V2 SWEEP DATABASE support (AST/semantic/bytecode) and enabled sweep parser tests.
- 2026-01-24: Started WS-4 scheduler core: job DDL plumbing (parser/semantic/bytecode/executor) and minimal scheduler thread.
- 2026-01-24: Expanded sys.performance to include broader engine metrics for monitoring parity.
- 2026-01-24: Wired SQL job timeouts into executor query limits for runtime enforcement (WS4-GAP-09).
- 2026-01-24: Create job runs in PENDING state and transition to RUNNING at execution start (WS4-GAP-07).
- 2026-01-24: Persist job_run result_data payloads in TOAST and expose via sys.job_runs (WS4-GAP-06).
- 2026-01-24: Added job scheduler validation tests for pending transitions, dependencies, and result payloads.
- 2026-01-25: Added cron parsing/dependency gating helpers and unit tests for scheduler.
- 2026-01-25: Seeded maintenance jobs and added job audit logging + permission checks.
- 2026-01-25: Added CANCEL JOB RUN parsing/bytecode/executor path and parser test coverage.
- 2026-01-25: Added maintenance seed validation and cancel-before-run scheduler tests.
- 2026-01-25: Added scheduler.enabled and pre_execute_delay_ms config wiring; documented scheduler config keys in sb_server.conf.
- 2026-01-26: Added ALTER SYSTEM SET runtime config wiring for scheduler keys and job timeout enforcement in scheduler runs.
- 2026-01-29: Closed WS-6 with RLS SELECT enforcement, view definer permission override wiring, and RLS runtime tests.
- 2026-01-29: Full rebuild + full test sweep passed (CTEST_PARALLEL_LEVEL=1, SCRATCHBIRD_TEST_NETWORK=1): 2347 tests, 0 failures.
- 2026-01-30: Implemented sys.sessions/sys.transactions/sys.locks/sys.statements via sys virtual catalog and aligned columns to monitoring spec; wired MON$ database stats to live transaction markers/metrics and attachment name to database path.
- 2026-01-31: Wired COPY + query running metrics into sys.performance; mapped Firebird MON$ attachments/transactions/statements to sys.* catalog; full build + sequential `ctest` pass completed (2347 tests, 0 failures).
- 2026-02-01: Added backup tablespace manifest + restore tablespace gating with unit tests for manifest/restore behavior.
- 2026-02-01: Full rebuild + sequential `ctest` pass completed (2355 tests, 0 failures).
- 2026-02-01: Wired buffer pool telemetry counters (hits/misses/reads/writes + dirty/pages gauges).
- 2026-02-01: Full rebuild + sequential `ctest` pass completed (2355 tests, 0 failures).
- 2026-02-01: Fixed columnstore metadata root/segment tracking and LSM SSTable footer parsing; full sequential `ctest` pass completed (2353 tests, 0 failures).
- 2026-01-26: Wired scheduler.max_concurrent_jobs and job_timeout_seconds defaults into runtime config application.
- 2026-01-26: Implemented concurrent scheduler job execution and expanded Parser V2 job DDL coverage (state/keep history).
- 2026-01-26: Added SHOW JOBS/JOB RUNS parser support and job-level GRANT/REVOKE permissions with EXECUTE checks.
- 2026-01-27: Implemented EXECUTE JOB real execution, cooperative cancellation with interrupt, timeout cancel handling, and added manual execution/cancel unit tests.
- 2026-01-27: Added CREATE/VIEW/EXECUTE EXTERNAL job privileges, session variable injection, RUN AS role validation, and dependency cycle checks.
- 2026-01-27: Wired schedule_tz cron evaluation, catch_up policy handling, and dependency window gating.
- 2026-01-28: Expanded job DDL to support CREATE OR ALTER/RECREATE and ALTER job body/dependencies/on-completion/partitioning updates; added parser coverage for new clauses.
- 2026-01-28: Fixed scheduler TSAN teardown race by joining job execution threads during shutdown; full `ctest` run completed (2 JSON tests skipped).
- 2026-01-28: Full build + sequential `ctest` run completed cleanly (2 JSON tests skipped).
- 2026-01-28: Marked WS-4 scheduler/job system complete; moving on to WS-5 constraint enforcement.
- 2026-01-28: Enforced PK/UNIQUE for unique indexes and COPY composite constraints in executor.
- 2026-01-28: Added FK definition validation (referenced columns must be PK/UNIQUE, type match).
- 2026-01-28: Enforced CHECK/NOT NULL in MERGE and FK actions; added runtime constraint tests.
- 2026-01-28: Closed WS-5 with constraint failure/dependency tests and full test sweep.
- 2026-01-28: Persisted job_class and SINGLE_SHARD UUID for jobs (WS4-GAP-01/02).
- 2026-01-28: Added SHARD_SET list parsing and DYNAMIC expression capture for job partitioning (WS4-GAP-03).
- 2026-01-28: Added sys.jobs/sys.job_runs/sys.job_dependencies virtual tables via sys catalog handler (WS4-GAP-04).
- 2026-01-28: Implemented catch_up=all scheduling advancement without collapsing to current time (WS4-GAP-08).
- 2026-01-28: Added in-memory job/job_run indexes to accelerate lookup and due-job scheduling (WS4-GAP-05).
- 2026-01-28: Added sys.performance scheduler metrics surface via sys catalog handler (WS4-GAP-10).
- 2026-01-28: Added ALTER JOB secret set/drop DDL wiring and catalog persistence (WS4-GAP-11).
- 2026-01-27: Wired heap GC to clean indexes post-prune with table_id scoping; added Columnstore/LSM index GC removal paths.
- 2026-01-25: Fixed buffer pool TSAN race by guarding flush writes with content mutex and skipping pinned pages; catalog heap updates now lock page content; full `ctest` pass completed (2 JSON tests skipped).
- 2026-01-21: Full `ctest` run (with `SCRATCHBIRD_TEST_NETWORK=1`) completed cleanly after stabilizing TCP integration port selection and rebuilding listener/parser binaries.
- 2026-01-21: Temporary debug logging removed from listener/parser/native adapter paths after validation.
- 2026-01-21: Wired sb_tablespace_files catalog page allocation/backfill plus load/persist helpers for tablespace file paths.
- 2026-01-21: Maintained tablespace table_count/index_count on table/index create/drop and table migration.
- 2026-01-21: Storage engine now allocates/pins heap pages by tablespace (GPID-aware insert/read/update/delete paths and allocation helpers).
- 2026-01-21: Heap scans iterate GPID-based pages for non-primary tablespaces and emit TIDs with the correct tablespace.
- 2026-01-21: Legacy delete-by-TID path now honors tablespace ID overrides when pinning pages.
- 2026-01-21: B-tree and Hash index storage now use root_gpid tablespace routing for pin/unpin and allocation.
- 2026-02-01: Implemented sys.context_variables and MON$CONTEXT_VARIABLES mapping to session variables; updated Firebird emulation view.
- 2026-02-01: Expanded MON$TABLE_STATS columns (stat_group, record_stat_id) to align with Firebird spec and emulation view.
- 2026-02-01: Expanded MON$RECORD_STATS columns to align with Firebird spec; mapped seq/idx reads and row mutations.
- 2026-02-01: Expanded MON$CALL_STACK columns to match Firebird spec; placeholders wired to sys.statements.
- 2026-02-01: Expanded MON$STATEMENTS columns to match Firebird spec; plan/timeout placeholders wired.
- 2026-02-01: Added sys.io_stats and wired MON$IO_STATS to live per-connection/transaction/statement counters.
- 2026-02-01: Added MON$COMPILED_STATEMENTS mapping (compiled statement placeholders) from sys.statements.
- 2026-02-01: Populated MON$CALL_STACK caller IDs/object metadata from security context stack.
- 2026-02-01: Extended MON$CALL_STACK to include per-connection stacks across sessions.
- 2026-02-01: Wired MON$CALL_STACK timestamps/source line/column from executor statement tracking.
- 2026-02-02: Stabilized result cache integration and confirmed full sequential `ctest` pass (2406 tests, 0 failures).
- 2026-02-01: Added bytecode debug spans to update executor source line/column during execution.
- 2026-02-02: Full rebuild + sequential `ctest` pass completed (2355 tests, 0 failures).



Gaps found in audit of WS-1 to WS-4



I reviewed WS‑1 through WS‑4 against the current ScratchBird code (read‑only). Here’s the status with concrete evidence and gaps.  

 WS‑1 Catalog bootstrap and schema roots  

 - Complete: root hierarchy defines /users/public and /remote/emulation/* as canonical roots in ScratchBird/src/core/  
   catalog_manager.cpp:2115.  
 - Complete: migration/repair moves legacy /public and /emulation under the new parents in ScratchBird/src/core/catalog_manager.cpp:2830.  
 - Outstanding: none found.  

 WS‑2 Tablespace routing + GPID wiring  

 - Implemented: DML uses tablespace-aware GPIDs for inserts and TIDs in ScratchBird/src/core/storage_engine.cpp:527.  
 - Implemented: heap page allocation uses tablespace-aware global allocation in ScratchBird/src/core/storage_engine.cpp:1520.  
 - Implemented: TOAST tables/indices inherit parent tablespace in ScratchBird/src/core/toast.cpp:329.  
 - Implemented: index root allocation uses tablespace-aware allocation in ScratchBird/src/core/catalog_manager.cpp:7250.  
 - Outstanding: table root pages still allocated via primary‑tablespace API (allocatePage) instead of a tablespace‑aware path in ScratchBird/  
   src/core/catalog_manager.cpp:6601.  
 - Outstanding: CREATE TABLE/INDEX default tablespace does not honor schema defaults or table tablespace when clause omitted; tablespace_id  
   defaults to 0 in ScratchBird/src/sblr/executor.cpp:5593 and ScratchBird/src/sblr/executor.cpp:6141.  

 WS‑3 Index migration safety  

 - Implemented: updateIndexTIDs covers BTREE/HASH/HNSW/GIN/GIST/BRIN/RTREE in ScratchBird/src/core/catalog_manager.cpp:11941 with matching  
   updateTIDsAfterMigration implementations in ScratchBird/src/core/btree.cpp, ScratchBird/src/core/hash_index.cpp, ScratchBird/src/core/  
   hnsw_index.cpp, ScratchBird/src/core/gin_index.cpp, ScratchBird/src/core/gist_index.cpp, ScratchBird/src/core/brin_index.cpp, ScratchBird/  
   src/core/rtree.cpp.  
 - Outstanding: SPGIST/BITMAP/COLUMNSTORE/LSM aren’t handled in updateIndexTIDs and have no updateTIDsAfterMigration implementations; they  
   currently fall into the default case in ScratchBird/src/core/catalog_manager.cpp:12232.  

 WS‑4 Scheduler/job system  

 - Complete: job DDL parsing (CREATE/ALTER/DROP/SHOW/EXECUTE/CANCEL + secrets) in ScratchBird/src/parser/parser_v2.cpp:447.  
 - Complete: bytecode generation for job flags/secret payloads in ScratchBird/src/sblr/bytecode_generator_v2.cpp:2605.  
 - Complete: executor job update + secret store/drop in ScratchBird/src/sblr/executor.cpp:35640.  
 - Complete: job/run/dependency/secret records in catalog in ScratchBird/src/core/catalog_manager.cpp:938.  
 - Complete: sys.jobs/sys.job_runs/sys.job_dependencies in ScratchBird/src/catalog/sys_catalog.cpp:1.  
 - Complete: scheduler PENDING, catch_up handling, concurrency in ScratchBird/src/core/job_scheduler.cpp:637.  
 - Outstanding: none found against the WS‑4 checklist.
