# Scheduler and Job Runner Canonical Specification

**Document:** SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md  
**Status:** Canonical Specification (Alpha + Beta)  
**Version:** 1.0  
**Date:** 2026-01-09  
**Authority:** Chief Architect

---

## Table of Contents

1. Executive Summary
2. Scope and Status
3. Design Principles
4. Terminology
5. Architecture
6. Catalog Schema
7. SQL and DDL Surface
8. Scheduling Semantics
9. Execution Semantics
10. Security Model
11. Observability and Monitoring
12. Cluster Extension (Beta)
13. Compatibility and Migration
14. Implementation Notes and Pointers

---

## 1. Executive Summary

This document defines the canonical job scheduler and job runner for ScratchBird.
It consolidates the Alpha standalone scheduler design and the Beta cluster scheduler
design into one coherent specification so that implementation is secure, consistent,
and forward-compatible with cluster deployment.

Event notifications (Firebird-style POST_EVENT) are a separate feature and are
defined in `ScratchBird/docs/specifications/ddl/DDL_EVENTS.md`.

This spec is the primary reference for implementation. Other scheduler documents
remain supporting references.

---

## 2. Scope and Status

**Alpha (single node, embedded):**
- Scheduler runs as a background thread inside the engine process.
- Job definitions are stored in system catalog tables.
- Jobs run locally on the same node.

**Beta (cluster):**
- Scheduler control plane is Raft-coordinated.
- Per-node scheduler agents execute jobs assigned by the control plane.
- Job classes and partition rules govern where jobs run.

**Out of scope:**
- Cluster orchestration details (covered by SBCLUSTER-01/02/05).
- External workflow engines (Airflow, cron) integrations.

---

## 3. Design Principles

1. **Single source of truth**: sys.jobs and sys.job_runs are authoritative.
2. **Secure by default**: external jobs are disabled unless explicitly enabled.
3. **Forward-compatible**: Alpha definitions migrate to Beta without change.
4. **Deterministic scheduling**: time zone, catch-up, and dependencies are explicit.
5. **Least privilege**: job execution runs under explicit user context.

---

## 4. Terminology

- **Job**: A scheduled unit of work (SQL, procedure, or external command).
- **Job run**: A single execution of a job.
- **Scheduler**: Component that decides when jobs should run.
- **Job runner**: Component that executes jobs and records results.
- **Agent**: Beta-only per-node runner managed by control plane.
- **Schedule kind**: CRON, AT, or EVERY (interval).
- **Job class**: LOCAL_SAFE, LEADER_ONLY, QUORUM_REQUIRED (Beta).
- **Partition rule**: ALL_SHARDS, SINGLE_SHARD, SHARD_SET, DYNAMIC (Beta).

---

## 5. Architecture

### 5.1 Alpha (single node)

```
ScratchBird Engine
  |
  +-- Scheduler Thread
  |    - Polls due jobs
  |    - Creates sys.job_runs
  |    - Enqueues execution
  |
  +-- Job Runner Pool
  |    - Executes jobs
  |    - Writes sys.job_runs
  |
  +-- Catalog (sys.jobs, sys.job_runs)
```

### 5.2 Beta (cluster)

```
Raft Control Plane
  |
  +-- Scheduler Leader
        - Computes schedule
        - Assigns job runs
        - Writes sys.job_runs

Node Agents (per node)
  |
  +-- Job Runner Pool
        - Executes assigned runs
        - Updates sys.job_runs
```

### 5.3 Components and Responsibilities

| Component | Responsibility | Alpha | Beta |
|-----------|----------------|-------|------|
| Scheduler | Create job runs from job definitions | Yes | Control plane |
| Job Runner | Execute jobs and update results | Yes | Agent |
| Catalog | Persist jobs and runs | Yes | Yes |
| Security | Privilege checks and audit | Yes | Yes |

---

## 6. Catalog Schema

### 6.1 sys.jobs (canonical)

```sql
CREATE TABLE sys.jobs (
    job_uuid UUID PRIMARY KEY,
    job_name VARCHAR(255) NOT NULL UNIQUE,
    description TEXT,

    job_class VARCHAR(20),                  -- LOCAL_SAFE | LEADER_ONLY | QUORUM_REQUIRED
    job_type VARCHAR(20) NOT NULL,          -- SQL | PROCEDURE | EXTERNAL
    job_sql TEXT,                           -- SQL to execute (SQL jobs)
    procedure_uuid UUID,                    -- Procedure to call (PROCEDURE jobs)
    external_command TEXT,                  -- Command to run (EXTERNAL jobs)

    schedule_kind VARCHAR(20) NOT NULL,     -- CRON | AT | EVERY
    cron_expression VARCHAR(100),           -- For CRON
    interval_seconds BIGINT,                -- For EVERY
    starts_at BIGINT,                       -- Optional start time (ms)
    ends_at BIGINT,                         -- Optional end time (ms)
    schedule_tz VARCHAR(64),                -- Time zone name (optional, default UTC)
    next_run_time BIGINT,                   -- Next scheduled execution (ms)

    on_completion VARCHAR(20) DEFAULT 'PRESERVE', -- PRESERVE | DROP (AT jobs)

    partition_strategy VARCHAR(20),         -- ALL_SHARDS | SINGLE_SHARD | SHARD_SET | DYNAMIC
    partition_shard_uuid UUID,              -- For SINGLE_SHARD
    partition_expression TEXT,              -- For DYNAMIC

    max_retries INTEGER DEFAULT 3,
    retry_backoff_seconds INTEGER DEFAULT 60,
    timeout_seconds INTEGER DEFAULT 3600,

    created_by_user_uuid UUID NOT NULL,
    run_as_role_uuid UUID,                  -- Optional role override
    created_at BIGINT NOT NULL,
    state VARCHAR(20) NOT NULL DEFAULT 'ENABLED', -- ENABLED | DISABLED | PAUSED

    FOREIGN KEY (created_by_user_uuid) REFERENCES sys.users(user_uuid)
);

CREATE INDEX idx_jobs_next_run_time ON sys.jobs(next_run_time) WHERE state = 'ENABLED';
CREATE INDEX idx_jobs_state ON sys.jobs(state);
```

### 6.2 sys.job_runs (canonical)

```sql
CREATE TABLE sys.job_runs (
    job_run_uuid UUID PRIMARY KEY,
    job_uuid UUID NOT NULL,

    assigned_node_uuid UUID,                -- Beta only (NULL in Alpha)
    shard_uuid UUID,                        -- Beta only (NULL in Alpha)

    scheduled_time BIGINT NOT NULL,
    started_at BIGINT,
    completed_at BIGINT,

    state VARCHAR(20) NOT NULL,             -- PENDING | RUNNING | COMPLETED | FAILED | CANCELLED
    retry_count INTEGER DEFAULT 0,

    result_message TEXT,
    rows_affected BIGINT,
    result_data BYTEA,
    error_code INTEGER,

    FOREIGN KEY (job_uuid) REFERENCES sys.jobs(job_uuid) ON DELETE CASCADE
);

CREATE INDEX idx_job_runs_job_uuid ON sys.job_runs(job_uuid);
CREATE INDEX idx_job_runs_state ON sys.job_runs(state);
CREATE INDEX idx_job_runs_started_at ON sys.job_runs(started_at DESC);
```

### 6.3 sys.job_dependencies (canonical)

```sql
CREATE TABLE sys.job_dependencies (
    job_uuid UUID NOT NULL,
    depends_on_job_uuid UUID NOT NULL,

    PRIMARY KEY (job_uuid, depends_on_job_uuid),
    FOREIGN KEY (job_uuid) REFERENCES sys.jobs(job_uuid) ON DELETE CASCADE,
    FOREIGN KEY (depends_on_job_uuid) REFERENCES sys.jobs(job_uuid) ON DELETE CASCADE
);

CREATE INDEX idx_job_deps_job_uuid ON sys.job_dependencies(job_uuid);
```

### 6.4 sys.job_secrets (optional, for external jobs)

```sql
CREATE TABLE sys.job_secrets (
    job_uuid UUID NOT NULL,
    secret_key VARCHAR(128) NOT NULL,
    secret_value BLOB,                      -- Encrypted
    PRIMARY KEY (job_uuid, secret_key),
    FOREIGN KEY (job_uuid) REFERENCES sys.jobs(job_uuid) ON DELETE CASCADE
);
```

---

## 7. SQL and DDL Surface

### 7.1 CREATE JOB

```sql
CREATE JOB job_name
  [CLASS = LOCAL_SAFE | LEADER_ONLY | QUORUM_REQUIRED]
  [PARTITION BY ALL_SHARDS | SINGLE_SHARD 'shard_uuid' | SHARD_SET (...) | DYNAMIC expr]
  SCHEDULE = CRON 'cron_expression'
           | AT 'timestamp'
           | EVERY interval [STARTS 'timestamp'] [ENDS 'timestamp']
  [DEPENDS ON job_name [, ...]]
  [MAX_RETRIES = n]
  [RETRY_BACKOFF = duration]
  [TIMEOUT = duration]
  [ON COMPLETION PRESERVE | DROP]
  [RUN AS role_name]
  [DESCRIPTION = 'description']
  [ENABLED | DISABLED]
  AS 'sql_statement' | CALL procedure_name() | EXEC 'external_command';

CREATE OR ALTER JOB job_name
  ...same clause set as CREATE JOB...

RECREATE JOB job_name
  ...same clause set as CREATE JOB...
```

### 7.2 ALTER JOB

```sql
ALTER JOB job_name
  [SET SCHEDULE = CRON '...' | AT '...' | EVERY interval ...]
  [SET STATE = ENABLED | DISABLED | PAUSED]
  [SET MAX_RETRIES = n]
  [SET RETRY_BACKOFF = duration]
  [SET TIMEOUT = duration]
  [SET RUN AS role_name]
  [SET DESCRIPTION = '...']
  [SET ON COMPLETION PRESERVE | DROP]
  [SET DEPENDS ON job_name [, ...] | NONE]
  [SET CLASS = class_name]
  [SET PARTITION BY ALL_SHARDS | SINGLE_SHARD 'shard_uuid' | SHARD_SET (...) | DYNAMIC expr]
  [SET AS 'sql_statement' | CALL procedure_name() | EXEC 'external_command'];
```

### 7.3 DROP JOB

```sql
DROP JOB job_name [KEEP HISTORY];
```

### 7.4 EXECUTE JOB (manual run)

```sql
EXECUTE JOB job_name;
```

### 7.5 CANCEL JOB RUN

```sql
CANCEL JOB RUN job_run_uuid;
```

### 7.6 Status Queries

```sql
SELECT job_name, state, next_run_time FROM sys.jobs ORDER BY next_run_time;
SELECT * FROM sys.job_runs WHERE job_uuid = '...';
```

---

## 8. Scheduling Semantics

### 8.1 CRON
- Standard 5-field cron format.
- Default timezone: UTC unless schedule_tz is set.
- DST transitions must be deterministic:
  - "Spring forward": skip missing times.
  - "Fall back": run once (not twice).

### 8.2 AT
- One-time schedule.
- `ON COMPLETION DROP` removes the job definition after completion.
- `ON COMPLETION PRESERVE` keeps the job definition and disables it.

### 8.3 EVERY (interval)
- Interval schedules are defined in seconds/minutes/hours/days.
- If STARTS is not set, STARTS = current time.
- ENDS limits scheduling after that timestamp.

### 8.4 Catch-up Policy
- Default: run once when resuming and skip missed occurrences.
- Optional config: catch_up = none | last | all.

---

## 9. Execution Semantics

### 9.1 Job Runs
- Scheduler creates one sys.job_runs entry per execution.
- Job runner updates state RUNNING -> COMPLETED/FAILED/CANCELLED.

### 9.2 Transaction Context
- SQL jobs run in a dedicated transaction with creator privileges.
- Procedure jobs run in the procedure engine with the same context.
- External jobs run outside the DB transaction but still record a job_run row.

### 9.3 Dependencies
- A dependent job is eligible only if the latest dependency run completed
  successfully within the current schedule window.
- Circular dependencies are rejected at CREATE/ALTER time.

### 9.4 Retries and Backoff
- Failures retry with exponential backoff: base * (2 ^ retry_count).
- When retries are exhausted, job_run is marked FAILED and no further retries occur.

### 9.5 Timeouts and Cancellation
- Job runs exceeding timeout_seconds are cancelled.
- CANCEL JOB RUN triggers cooperative cancellation where possible.
- For external jobs, cancellation sends SIGTERM then SIGKILL after grace period.

---

## 10. Security Model

### 10.1 Privileges

Required privileges:
- CREATE JOB
- ALTER JOB (owner or admin)
- DROP JOB (owner or admin)
- EXECUTE JOB
- VIEW JOB HISTORY
- EXECUTE EXTERNAL JOB (separate privilege)

### 10.2 Execution Identity
- Jobs run as the creator by default.
- Optional RUN AS role requires grant and is audited.

### 10.3 External Job Hardening
External jobs are disabled by default and require explicit configuration:

Required controls:
- allowlist of commands or directories
- fixed working directory
- environment allowlist
- output size cap
- CPU/memory/time limits
- optional chroot/namespace isolation

### 10.4 Secrets Handling
- Job secrets stored in sys.job_secrets are encrypted with the DB master key.
- Secrets are never exposed via sys.jobs or regular SELECT access.

### 10.5 Audit Events
Add audit event types:
- JOB_CREATED, JOB_MODIFIED, JOB_DELETED
- JOB_EXECUTED, JOB_FAILED, JOB_CANCELLED

Each event records job_uuid, job_run_uuid, user, and outcome.

---

## 11. Observability and Monitoring

Minimum monitoring surfaces:
- sys.job_runs (history)
- sys.jobs (current state)
- sys.performance extensions for scheduler metrics
- CLI: sb_admin job list, sb_admin job runs

Suggested metrics:
- scheduler_queue_depth
- jobs_running
- jobs_failed_total
- job_run_latency_ms

---

## 12. Cluster Extension (Beta)

### 12.1 Job Classes
- LOCAL_SAFE: any node, no coordination
- LEADER_ONLY: only Raft leader
- QUORUM_REQUIRED: requires quorum acknowledgment

### 12.2 Partition Rules
- ALL_SHARDS: one run per shard
- SINGLE_SHARD: one run on a specific shard
- SHARD_SET: runs on listed shards
- DYNAMIC: runtime shard selection from expression

### 12.3 Control Plane Integration
- Scheduler policy hash stored in SBCLUSTER-01 CCE.
- Agents poll for assigned runs and report completion.

---

## 13. Compatibility and Migration

### 13.1 Event notifications
Event notifications are not scheduler jobs. Firebird-style event posting and listener
registration are specified in `ScratchBird/docs/specifications/ddl/DDL_EVENTS.md`.

### 13.2 Previous Scheduler Docs
- `ALPHA_SCHEDULER_SPECIFICATION.md` and `SBCLUSTER-09-SCHEDULER.md`
  remain valid references but this document is canonical and resolves conflicts.

### 13.3 Migration to Beta
- Alpha jobs migrate without schema or DDL changes.
- Beta enables job classes and partition rules without breaking Alpha jobs.

---

## 14. Implementation Notes and Pointers

- This spec is aligned with the scheduler audit and consolidates all gaps.
- For current code-truth status and missing implementation items, see:
  `ScratchBird/docs/findings/SCHEDULER_JOB_RUNNER_AUDIT.md`

Key implementation building blocks:
- Catalog tables and CRUD in CatalogManager
- Scheduler loop and cron/interval engine
- Job runner execution and cancellation
- Security privilege checks and audit logging

---

**End of Specification**
