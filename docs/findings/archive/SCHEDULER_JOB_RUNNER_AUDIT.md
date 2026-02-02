# Scheduler and Job Runner Audit
Status: Superseded (implementation verified)
Last Updated: 2026-02-02

Note: All gaps called out here are closed. Track any remaining work in
`docs/planning/TRACKER_OUTSTANDING_MASTER.md`.


## Scope
- Alpha scheduler specification:
  `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md`
- Scheduler directory overview:
  `ScratchBird/docs/specifications/scheduler/README.md`
- Cluster scheduler (Beta):
  `ScratchBird/docs/specifications/Cluster Specification Work/SBCLUSTER-09-SCHEDULER.md`
- Event system (separate scheduling model):
  `ScratchBird/docs/specifications/ddl/DDL_EVENTS.md`
- Source review (read-only):
  `ScratchBird/src/network/thread_pool.cpp`,
  `ScratchBird/src/core/catalog_manager.cpp`,
  `ScratchBird/src/sblr/executor.cpp`,
  `ScratchBird/src/core/lsm_thread_pool.cpp`,
  `ScratchBird/src/executor/parallel_executor.cpp`,
  `ScratchBird/include/scratchbird/core/audit_logger.h`,
  `ScratchBird/include/scratchbird/core/catalog_manager.h`

## Implemented (code-truth, updated)
- Persistent job catalog tables exist and are exposed as `sys.jobs`, `sys.job_runs`,
  `sys.job_dependencies` virtual tables.
  - Code: `ScratchBird/src/catalog/sys_catalog.cpp:4-435`
  - Code: `ScratchBird/src/core/catalog_manager.cpp:383-7900`
- Scheduler thread and execution loop implemented with job queueing, concurrency
  limits, and run state tracking.
  - Code: `ScratchBird/src/core/job_scheduler.cpp:467-1188`
- Cron parsing and next-run calculation implemented (timezone-aware).
  - Code: `ScratchBird/src/core/job_scheduler_utils.cpp:101-273`
  - Code: `ScratchBird/src/core/job_scheduler.cpp:543-624`
- Job DDL and execution surface implemented in parser/semantic/bytecode/executor:
  - Parser: `ScratchBird/src/parser/parser_v2.cpp:389-394`, `ScratchBird/src/parser/parser_v2.cpp:1350-1903`
  - Semantic: `ScratchBird/src/sblr/semantic_analyzer_v2.cpp:3531-3580`
  - Bytecode: `ScratchBird/src/sblr/bytecode_generator_v2.cpp:562-623`
  - Executor: `ScratchBird/src/sblr/executor.cpp:35543-36192`
- Scheduler config keys and runtime update wiring implemented (`scheduler.*`).
  - Code: `ScratchBird/src/core/database.cpp:257-365`
- Security and audit hooks for job DDL/execution implemented.
  - Code: `ScratchBird/src/sblr/executor.cpp:35575-36192`

## Missing or Partial vs Spec (remaining)

### F-SCHED-001 sys.jobs/sys.job_runs/sys.job_dependencies catalog is not implemented
- Status: Resolved (see Implemented section)

### F-SCHED-002 Scheduler thread and execution loop are not implemented
- Status: Resolved (job scheduler loop implemented)

### F-SCHED-003 Cron parsing/next-run calculation not implemented
- Status: Resolved (cron parsing utilities implemented)

### F-SCHED-004 Job DDL (CREATE/ALTER/DROP/EXECUTE JOB) is not implemented
- Status: Resolved (parser/semantic/bytecode/executor implemented)

### F-SCHED-005 Job dependency tracking (DAG) not implemented
- Status: Resolved (job_dependencies table + dependency checks)
  - Code: `ScratchBird/src/core/job_scheduler.cpp:599`
  - Code: `ScratchBird/src/core/catalog_manager.cpp:998-26572`

### F-SCHED-006 Security privileges and audit events for jobs are not implemented
- Status: Resolved (job privileges + audit events implemented)
  - Code: `ScratchBird/src/sblr/executor.cpp:35575-36192`

### F-SCHED-007 Scheduler configuration keys and ALTER SYSTEM controls not implemented
- Status: Resolved (scheduler config keys wired)
  - Code: `ScratchBird/src/core/database.cpp:257-365`

### F-SCHED-008 Event scheduler spec is separate and unimplemented
- Spec: `ScratchBird/docs/specifications/ddl/DDL_EVENTS.md` (CREATE/ALTER/DROP EVENT, event scheduler thread).
- Code: No CREATE/ALTER/DROP EVENT support in parser/executor; no event scheduler thread.
- Status: Missing
- Note: This is a parallel scheduling model to CREATE JOB; see "Spec conflicts" below.

### F-SCHED-009 Job cancellation/timeouts not wired
- Status: Resolved (CANCEL JOB RUN + timeout enforcement implemented)
  - Code: `ScratchBird/src/sblr/executor.cpp:36110-36192`
  - Code: `ScratchBird/src/core/job_scheduler.cpp:900-1063`

## Spec Conflicts and Gaps

### S-SCHED-001 CREATE EVENT scheduling spec corrected
- `DDL_EVENTS.md` is now defined as Firebird-style event notifications (POST_EVENT),
  not a scheduler feature.
- Canonical scheduler spec removes CREATE EVENT aliasing and treats events as separate.
- Impact: Scheduling DDL is now CREATE JOB only; events are notifications.

### S-SCHED-002 sys.job_dependencies schema mismatch
- Alpha spec uses `job_uuid` and `depends_on_job_uuid`.
- Scheduler README uses `parent_job_uuid` and `child_job_uuid`.
- Impact: Catalog schema mismatch between docs; must be normalized before implementation.

### S-SCHED-003 SQL syntax drift between docs
- Alpha spec includes `DROP JOB ... KEEP HISTORY` and `ALTER JOB SET STATE`.
- Scheduler README includes `DROP JOB ... CASCADE|RESTRICT` and `EXECUTE JOB`.
- Impact: Parser/executor requirements unclear; DDL scope inconsistent.

### S-SCHED-004 Job run states differ between Alpha and Beta specs
- Beta spec includes `CANCELLED`; Alpha spec omits it.
- Impact: Migration and audit/event typing mismatch if Alpha tables omit or exclude CANCELLED.

### S-SCHED-005 Cron semantics are underspecified
- No timezone/DST behavior, schedule window definitions for dependency checks, or run-skipping policy
  (catch-up vs skip-on-miss) are specified.
- Impact: Inconsistent job execution across nodes in Beta and edge-case ambiguity in Alpha.

### S-SCHED-006 External job security is underspecified
- No policy for command allowlists, environment isolation, working directory restrictions,
  resource limits, output capture limits, or per-job execution identity.
- Impact: High risk surface for Alpha and Beta; requires explicit safeguards to be secure.

### S-SCHED-007 Job secrets and command storage
- Specs store `job_sql` and `external_command` in catalog tables without defining encryption
  or access controls to prevent disclosure.
- Impact: Sensitive data exposure risk in sys.jobs.

## Notes for Cluster Extension Readiness
- Alpha spec claims forward compatibility but omits how scheduler policy hashes
  (SBCLUSTER-01) and membership/agent roles (SBCLUSTER-02) would be wired at upgrade time.
- Job classes and partition rules are stored but not validated in Alpha; Beta will need
  strict validation to avoid invalid job definitions migrating forward.

## Summary
The Alpha scheduler subsystem is now implemented end-to-end (catalog, DDL, scheduler
thread, cron parsing, security, auditing). Remaining work is primarily around the
separate Firebird-style EVENT notification feature (not a scheduler) and any
documented schema/terminology mismatches between specs.
