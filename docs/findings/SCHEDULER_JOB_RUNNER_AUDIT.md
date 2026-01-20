# Scheduler and Job Runner Audit

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

## Implemented (code-truth)
- General-purpose thread pool supports delayed/repeating tasks with a scheduler loop (in-memory only).
  - Code: `ScratchBird/src/network/thread_pool.cpp:123-466`
- Asynchronous TRUNCATE TABLE job runner with job_id tracking and background thread execution.
  - Code: `ScratchBird/src/core/catalog_manager.cpp:17581-17755`
  - Code (caller and async job ID output): `ScratchBird/src/sblr/executor.cpp:10020-10106`
- Background task runners exist for internal subsystems (LSM compaction, parallel executor), but are not tied to job scheduling.
  - Code: `ScratchBird/src/core/lsm_thread_pool.cpp:33-366`
  - Code: `ScratchBird/src/executor/parallel_executor.cpp:46-163`

## Missing or Partial vs Spec

### F-SCHED-001 sys.jobs/sys.job_runs/sys.job_dependencies catalog is not implemented
- Spec: `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` (Catalog Schema),
  `ScratchBird/docs/specifications/scheduler/README.md` (Catalog Schema).
- Code: No references to `sys.jobs`, `sys.job_runs`, or `sys.job_dependencies` in the source tree
  (search returned no matches).
- Status: Missing

### F-SCHED-002 Scheduler thread and execution loop are not implemented
- Spec: `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` (Alpha Architecture, Job Execution).
- Code: No scheduler_thread implementation; only a generic task scheduler in the network thread pool
  without persistence or catalog integration.
  `ScratchBird/src/network/thread_pool.cpp:123-466`
- Status: Missing

### F-SCHED-003 Cron parsing/next-run calculation not implemented
- Spec: `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` (Cron parsing, next-run).
- Code: No cron parsing or next-run logic in source tree (search returned no matches for "cron").
- Status: Missing

### F-SCHED-004 Job DDL (CREATE/ALTER/DROP/EXECUTE JOB) is not implemented
- Spec: `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` (SQL Syntax),
  `ScratchBird/docs/specifications/scheduler/README.md` (SQL Syntax, EXECUTE JOB).
- Code: No parser/executor support for CREATE/ALTER/DROP/EXECUTE JOB in V2 or adapters
  (search returned no matches for CREATE JOB or sys.jobs).
- Status: Missing

### F-SCHED-005 Job dependency tracking (DAG) not implemented
- Spec: `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` (Job Dependencies).
- Code: No sys.job_dependencies table or dependency evaluation in scheduler (not present in code).
- Status: Missing

### F-SCHED-006 Security privileges and audit events for jobs are not implemented
- Spec: `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` (Security Model).
- Code: No CREATE JOB privilege or JOB_* audit event types exist.
  `ScratchBird/include/scratchbird/core/catalog_manager.h:944-1009`,
  `ScratchBird/include/scratchbird/core/audit_logger.h:25-90`
- Status: Missing

### F-SCHED-007 Scheduler configuration keys and ALTER SYSTEM controls not implemented
- Spec: `ScratchBird/docs/specifications/scheduler/ALPHA_SCHEDULER_SPECIFICATION.md` (Alpha Configuration).
- Code: No references to scheduler.* config keys or ALTER SYSTEM scheduling controls in source tree.
- Status: Missing

### F-SCHED-008 Event scheduler spec is separate and unimplemented
- Spec: `ScratchBird/docs/specifications/ddl/DDL_EVENTS.md` (CREATE/ALTER/DROP EVENT, event scheduler thread).
- Code: No CREATE/ALTER/DROP EVENT support in parser/executor; no event scheduler thread.
- Status: Missing
- Note: This is a parallel scheduling model to CREATE JOB; see "Spec conflicts" below.

### F-SCHED-009 Job cancellation/timeouts not wired
- Spec: Alpha spec defines `job_timeout_seconds` and implies cancellation.
- Code: ThreadPool cancellation is stubbed and cannot cancel by task ID.
  `ScratchBird/src/network/thread_pool.cpp:264-283`
- Status: Missing/Partial

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
The scheduler subsystem is fully specified (Alpha + Beta) but not implemented in code.
There is partial job-runner infrastructure (thread pools and an async TRUNCATE job),
yet no persistent scheduler, catalog schema, DDL surface, or security model exists.
Documentation conflicts (CREATE JOB vs CREATE EVENT, schema naming, SQL syntax drift)
must be resolved before implementation to ensure security and cluster-ready behavior.
