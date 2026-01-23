# Alpha Standalone Job Scheduler Specification

**Document**: ALPHA_SCHEDULER_SPECIFICATION.md
**Status**: ALPHA IMPLEMENTATION SPECIFICATION
**Version**: 1.0
**Date**: 2026-01-09
**Authority**: Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Design Principles](#design-principles)
3. [Alpha vs Beta Feature Matrix](#alpha-vs-beta-feature-matrix)
4. [Alpha Architecture](#alpha-architecture)
5. [Data Structures](#data-structures)
6. [Job Execution](#job-execution)
7. [SQL Syntax](#sql-syntax)
8. [Catalog Schema](#catalog-schema)
9. [Implementation Plan](#implementation-plan)
10. [Beta Migration Path](#beta-migration-path)

---

## Executive Summary

**Canonical spec:** This document is a supporting Alpha reference. The canonical scheduler
spec that consolidates Alpha + Beta behavior and resolves DDL differences is:
`ScratchBird/docs/specifications/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md`.

### Purpose

This specification defines a **standalone job scheduler for Alpha** (single-node, non-clustered) that is **forward-compatible** with the full Beta cluster scheduler (SBCLUSTER-09). The Alpha scheduler provides essential scheduling capabilities while avoiding cluster-specific features that don't apply to single-node deployments.

### Key Requirements

1. **Standalone**: Works without cluster features (no Raft, no distributed coordination)
2. **Forward-Compatible**: Catalog schema, SQL syntax, job execution engine reusable in Beta
3. **Full-Featured**: Cron scheduling, job dependencies, retry logic, SQL/procedure/external jobs
4. **Zero Breaking Changes**: Alpha jobs migrate seamlessly to Beta

### Scope

**Alpha Includes**:
- ✅ Job scheduling (cron expressions, one-time jobs, recurring jobs)
- ✅ Job execution (SQL, stored procedures, external commands)
- ✅ Failure handling (retry with exponential backoff)
- ✅ Job dependencies (ETL pipelines)
- ✅ Catalog tables (`sys.jobs`, `sys.job_runs`)
- ✅ SQL syntax (`CREATE JOB`, `ALTER JOB`, `DROP JOB`)
- ✅ Security (job execution with creator privileges, audit logging)

**Alpha Defers** (Beta only):
- ⏭️ Job classes (LOCAL_SAFE, LEADER_ONLY, QUORUM_REQUIRED) - Alpha treats all as LOCAL_SAFE
- ⏭️ Partition rules (ALL_SHARDS, SINGLE_SHARD) - Alpha has no shards
- ⏭️ Distributed execution (scheduler agents, Raft control plane)
- ⏭️ Cluster-wide coordination

---

## Design Principles

### 1. **Subset, Not Fork**

Alpha scheduler is a **subset** of Beta scheduler, not a separate implementation.

**Implication**:
- Reuse data structures (Job, JobRun) from SBCLUSTER-09
- Reuse SQL syntax (`CREATE JOB` format) from SBCLUSTER-09
- Reuse job execution engine (SQL execution, retry logic)

**Result**: Zero code duplication, clean migration path to Beta.

### 2. **Ignore, Don't Reject**

Alpha parser accepts Beta syntax but ignores cluster-specific features.

**Example**:
```sql
-- Beta syntax (with job class and partition rule)
CREATE JOB daily_sweep
  CLASS = LOCAL_SAFE              -- Accepted, ignored (Alpha has no classes)
  PARTITION BY ALL_SHARDS         -- Accepted, ignored (Alpha has no shards)
  SCHEDULE = CRON '0 2 * * *'
  AS 'SWEEP ANALYZE';             -- Native sweep/GC command (VACUUM alias available)

-- Result: Job created, executes on single Alpha node, no errors
```

**Rationale**: Users can write Beta-compatible job definitions in Alpha, then migrate to Beta without changes.

### 3. **Single-Threaded Scheduler**

Alpha scheduler runs as a **single background thread** in the ScratchBird engine process.

**Architecture**:
```
ScratchBird Engine (Alpha, single-node)
  ├─ Query Executor Thread
  ├─ Transaction Manager Thread
  ├─ Storage Engine Thread
  └─ Scheduler Thread (NEW)
      ├─ Scheduling Loop (every 10s)
      └─ Job Execution Loop (up to 10 concurrent jobs)
```

**No separate process**: Scheduler embedded in engine, not a separate daemon.

### 4. **No Cluster Dependencies**

Alpha scheduler does NOT depend on:
- ❌ Raft consensus
- ❌ Cluster Configuration Epoch (CCE)
- ❌ Node membership (SBCLUSTER-02)
- ❌ Shard map (SBCLUSTER-05)

Alpha scheduler DOES use:
- ✅ Catalog tables (`sys.jobs`, `sys.job_runs`)
- ✅ Transaction manager (for SQL job execution)
- ✅ Security context (for privilege enforcement)
- ✅ Audit chain (for job execution logging)

---

## Alpha vs Beta Feature Matrix

| Feature | Alpha | Beta | Notes |
|---------|-------|------|-------|
| **Cron Scheduling** | ✅ | ✅ | Identical implementation |
| **One-Time Jobs** | ✅ | ✅ | Identical implementation |
| **Recurring Jobs** | ✅ | ✅ | Identical implementation |
| **SQL Jobs** | ✅ | ✅ | Identical implementation |
| **Procedure Jobs** | ✅ | ✅ | Identical implementation |
| **External Jobs** | ✅ | ✅ | Identical implementation |
| **Job Dependencies** | ✅ | ✅ | Identical implementation |
| **Retry with Backoff** | ✅ | ✅ | Identical implementation |
| **Privilege Enforcement** | ✅ | ✅ | Identical implementation |
| **Audit Logging** | ✅ | ✅ | Identical implementation |
| **Job Classes** | ⏭️ Ignored | ✅ | Alpha: All jobs execute locally |
| **Partition Rules** | ⏭️ Ignored | ✅ | Alpha: No shards |
| **Distributed Execution** | ❌ | ✅ | Alpha: Single node only |
| **Raft Control Plane** | ❌ | ✅ | Alpha: No Raft |
| **Scheduler Agents** | ❌ | ✅ | Alpha: Embedded thread |

### Compatibility Guarantee

**Alpha Job → Beta Migration**:
```sql
-- Alpha: Create job
CREATE JOB hourly_report
  SCHEDULE = CRON '0 * * * *'
  AS 'CALL generate_hourly_sales_report()';

-- Beta: Same job definition works unchanged
-- (Beta adds cluster coordination, but job definition identical)
```

**No Breaking Changes**: All Alpha job definitions are valid Beta job definitions.

---

## Alpha Architecture

### Component Diagram

```
┌─────────────────────────────────────────────────────────┐
│         ScratchBird Engine (Alpha, Single-Node)         │
│                                                           │
│  ┌────────────────────────────────────────────────────┐ │
│  │           Scheduler Thread                         │ │
│  │                                                    │ │
│  │  ┌──────────────────────────────────────────────┐ │ │
│  │  │  Scheduling Loop (every 10 seconds)          │ │ │
│  │  │  1. Query sys.jobs for due jobs              │ │ │
│  │  │  2. Create JobRun entries                    │ │ │
│  │  │  3. Add to execution queue                   │ │ │
│  │  └──────────────────────────────────────────────┘ │ │
│  │                                                    │ │
│  │  ┌──────────────────────────────────────────────┐ │ │
│  │  │  Job Execution Loop (concurrent)             │ │ │
│  │  │  1. Pop job from queue                       │ │ │
│  │  │  2. Execute job (SQL/procedure/external)     │ │ │
│  │  │  3. Handle success/failure                   │ │ │
│  │  │  4. Retry on failure (exponential backoff)   │ │ │
│  │  └──────────────────────────────────────────────┘ │ │
│  │                                                    │ │
│  └────────────────────────────────────────────────────┘ │
│                                                           │
│  ┌────────────────────────────────────────────────────┐ │
│  │           Catalog Tables                           │ │
│  │  - sys.jobs (job definitions)                     │ │
│  │  - sys.job_runs (job execution history)           │ │
│  └────────────────────────────────────────────────────┘ │
│                                                           │
└─────────────────────────────────────────────────────────┘
```

### Threading Model

**Main Engine Threads** (existing):
- Query executor
- Transaction manager
- Storage engine (LSM compaction, GC)
- Network listener

**New Scheduler Thread**:
- Background thread created at engine startup
- Runs scheduling loop every 10 seconds
- Spawns worker threads for job execution (up to `max_concurrent_jobs`)

**Configuration**:
```yaml
scheduler:
  enabled: true                    # Enable scheduler (default: true)
  polling_interval_seconds: 10     # Scheduling loop frequency
  max_concurrent_jobs: 10          # Max concurrent job executions
  job_timeout_seconds: 3600        # Max job execution time (1 hour)
```

---

## Data Structures

### Job

**Alpha uses SBCLUSTER-09 Job struct** (with cluster fields ignored):

```cpp
struct Job {
    UUID job_uuid;                       // UUIDv7 identifier
    string job_name;                     // Human-readable name
    string description;                  // Job description

    JobClass job_class;                  // Alpha: Accepted, ignored (always LOCAL_SAFE)
    JobType job_type;                    // SQL | PROCEDURE | EXTERNAL
    string job_sql;                      // SQL to execute (for SQL jobs)
    UUID procedure_uuid;                 // Procedure to call (for PROCEDURE jobs)
    string external_command;             // Command to run (for EXTERNAL jobs)

    ScheduleKind schedule_kind;          // CRON | AT | EVERY
    string cron_expression;              // For CRON
    int64 interval_seconds;              // For EVERY
    timestamp_t starts_at;               // Optional start time (ms)
    timestamp_t ends_at;                 // Optional end time (ms)
    string schedule_tz;                  // Time zone name (optional, default UTC)
    timestamp_t next_run_time;           // Next scheduled execution

    OnCompletion on_completion;          // PRESERVE | DROP (AT jobs)

    PartitionRule partition_rule;        // Alpha: Accepted, ignored (no shards)

    uint32_t max_retries;                // Max retry attempts (default: 3)
    duration_t retry_backoff;            // Backoff between retries (default: 60s)
    duration_t timeout_seconds;          // Timeout (default: 3600s)

    UUID created_by_user_uuid;           // Job creator (for privilege enforcement)
    UUID run_as_role_uuid;               // Optional role override
    timestamp_t created_at;
    JobState state;                      // ENABLED | DISABLED | PAUSED
};
```

**Alpha Simplifications**:
- `job_class`: Parsed, stored, but not used (all jobs execute locally)
- `partition_rule`: Parsed, stored, but not used (no shards in Alpha)

### JobRun

**Alpha uses SBCLUSTER-09 JobRun struct** (with cluster fields set to null):

```cpp
struct JobRun {
    UUID job_run_uuid;                   // Unique run identifier (UUIDv7)
    UUID job_uuid;                       // Parent job

    UUID assigned_node_uuid;             // Alpha: Always null (no cluster)
    UUID shard_uuid;                     // Alpha: Always null (no shards)

    timestamp_t scheduled_time;          // When run was scheduled
    timestamp_t started_at;              // When execution started
    timestamp_t completed_at;            // When execution completed

    JobRunState state;                   // PENDING | RUNNING | COMPLETED | FAILED | CANCELLED
    uint32_t retry_count;                // Number of retries so far

    string result_message;               // Success/error message
    uint64_t rows_affected;              // Rows affected (for SQL jobs)
    bytes result_data;                   // Optional result data

    ErrorCode error_code;                // Error code (if failed)
};
```

**Alpha Simplifications**:
- `assigned_node_uuid`: Always null (single-node, no assignment needed)
- `shard_uuid`: Always null (no shards in Alpha)

---

## Job Execution

### Scheduling Loop (Alpha)

```cpp
void SchedulerThread::schedulingLoop() {
    while (running_) {
        try {
            // 1. Get current time
            timestamp_t now = currentTimestamp();

            // 2. Query enabled jobs due to run
            vector<Job> due_jobs = catalog_->queryJobs(
                "SELECT * FROM sys.jobs WHERE state = 'ENABLED' AND next_run_time <= ?",
                now
            );

            // 3. Create JobRun entries
            for (const Job& job : due_jobs) {
                JobRun run = createJobRun(job);
                execution_queue_.push(run);  // Add to execution queue

                // Update next run time (CRON/EVERY) or finalize AT jobs
                if (job.schedule_kind == ScheduleKind::CRON) {
                    timestamp_t next_run = calculateNextRunTime(job.cron_expression, now);
                    catalog_->updateJob(job.job_uuid, {{"next_run_time", next_run}});
                } else if (job.schedule_kind == ScheduleKind::EVERY) {
                    timestamp_t next_run = now + seconds(job.interval_seconds);
                    catalog_->updateJob(job.job_uuid, {{"next_run_time", next_run}});
                } else {
                    // AT job: disable or drop after scheduling
                    if (job.on_completion == OnCompletion::DROP) {
                        catalog_->deleteJob(job.job_uuid);
                    } else {
                        catalog_->updateJob(job.job_uuid, {{"state", JobState::DISABLED}});
                    }
                }
            }

        } catch (const Exception& e) {
            LOG_ERROR("Scheduling loop error: {}", e.message());
        }

        // 4. Sleep until next poll
        std::this_thread::sleep_for(std::chrono::seconds(polling_interval_seconds_));
    }
}
```

### Job Execution Loop (Alpha)

```cpp
void SchedulerThread::jobExecutionLoop() {
    while (running_) {
        // 1. Check if we can execute more jobs
        if (active_jobs_.size() >= max_concurrent_jobs_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        // 2. Pop job from queue (blocking, 1-second timeout)
        JobRun run;
        if (!execution_queue_.try_pop(run, std::chrono::seconds(1))) {
            continue;  // Queue empty, try again
        }

        // 3. Execute job asynchronously
        std::thread job_thread([this, run]() {
            executeJobRun(run);
        });
        job_thread.detach();

        // Track active job
        active_jobs_.insert(run.job_run_uuid);
    }
}
```

### Job Execution (Reusable in Beta)

```cpp
void SchedulerThread::executeJobRun(JobRun run) {
    // 1. Mark as RUNNING
    run.state = JobRunState::RUNNING;
    run.started_at = currentTimestamp();
    catalog_->updateJobRun(run);

    try {
        // 2. Get job definition
        Job job = catalog_->getJob(run.job_uuid);

        // 3. Check job dependencies (if any)
        if (!dependenciesSatisfied(job)) {
            // Dependencies not met, reschedule
            run.state = JobRunState::PENDING;
            run.result_message = "Waiting for dependencies";
            catalog_->updateJobRun(run);
            execution_queue_.push(run);  // Re-queue
            return;
        }

        // 4. Execute based on job type
        switch (job.job_type) {
            case JobType::SQL:
                executeSqlJob(job, run);
                break;
            case JobType::PROCEDURE:
                executeProcedureJob(job, run);
                break;
            case JobType::EXTERNAL:
                executeExternalJob(job, run);
                break;
        }

        // 5. Mark as COMPLETED
        run.state = JobRunState::COMPLETED;
        run.completed_at = currentTimestamp();
        catalog_->updateJobRun(run);

        LOG_INFO("Job run {} completed successfully", run.job_run_uuid);

    } catch (const Exception& e) {
        // 6. Handle failure
        handleJobFailure(run, job, e);
    }

    // 7. Remove from active jobs
    active_jobs_.erase(run.job_run_uuid);
}
```

### SQL Job Execution (Reusable in Beta)

```cpp
void SchedulerThread::executeSqlJob(const Job& job, JobRun& run) {
    // 1. Get job creator's security context
    SecurityContext creator_ctx = security_manager_->getUserContext(job.created_by_user_uuid);

    // 2. Begin transaction (with job creator's privileges)
    Transaction txn = transaction_manager_->beginTransaction(creator_ctx);

    // 3. Set job execution context (session variables)
    txn.setSessionVar("job_uuid", job.job_uuid.toString());
    txn.setSessionVar("job_run_uuid", run.job_run_uuid.toString());
    txn.setSessionVar("job_name", job.job_name);

    // 4. Execute SQL
    ResultSet result = txn.executeQuery(job.job_sql);

    // 5. Commit transaction
    txn.commit();

    // 6. Record results
    run.rows_affected = result.rowsAffected();
    run.result_message = fmt::format("Query executed successfully, {} rows affected",
                                     result.rowsAffected());

    // 7. Audit log
    audit_chain_->logEvent(AuditEventType::JOB_EXECUTED, {
        {"job_uuid", job.job_uuid.toString()},
        {"job_run_uuid", run.job_run_uuid.toString()},
        {"rows_affected", run.rows_affected}
    });
}
```

---

## SQL Syntax

### CREATE JOB (Alpha Subset)

**Alpha Syntax** (subset of Beta):

```sql
CREATE JOB job_name
  [CLASS = job_class]              -- Optional, ignored in Alpha
  [PARTITION BY partition_rule]    -- Optional, ignored in Alpha
  SCHEDULE = CRON 'cron_expression' | AT 'timestamp' | EVERY interval [STARTS 'timestamp'] [ENDS 'timestamp']
  [DEPENDS ON job_name [, ...]]    -- Optional, job dependencies
  [MAX_RETRIES = n]                -- Optional, default: 3
  [RETRY_BACKOFF = duration]       -- Optional, default: 60s
  [TIMEOUT = duration]             -- Optional
  [ON COMPLETION PRESERVE | DROP]  -- Optional (AT jobs)
  [RUN AS role_name]               -- Optional
  [DESCRIPTION = 'description']    -- Optional
  [ENABLED | DISABLED]             -- Optional
  AS 'sql_statement' | CALL procedure_name() | EXEC 'external_command';
```

**Alpha Examples**:

```sql
-- Daily sweep/GC (Beta syntax, works in Alpha)
CREATE JOB daily_sweep
  CLASS = LOCAL_SAFE              -- Accepted, ignored in Alpha
  PARTITION BY ALL_SHARDS         -- Accepted, ignored in Alpha
  SCHEDULE = CRON '0 2 * * *'
  DESCRIPTION = 'Daily sweep/GC + analyze'
  AS 'SWEEP ANALYZE';             -- Native sweep/GC command (VACUUM alias available)

-- Hourly report (Alpha-only syntax)
CREATE JOB hourly_sales_report
  SCHEDULE = CRON '0 * * * *'
  AS 'CALL generate_hourly_sales_report()';

-- One-time job (run at specific time)
CREATE JOB migrate_customer_data
  SCHEDULE = AT '2026-01-15 10:00:00'
  MAX_RETRIES = 5
  RETRY_BACKOFF = 120s
  AS 'CALL migrate_customers_to_new_schema()';

-- ETL pipeline with dependencies
CREATE JOB etl_extract
  SCHEDULE = CRON '0 1 * * *'
  AS 'CALL extract_source_data()';

CREATE JOB etl_transform
  DEPENDS ON etl_extract
  SCHEDULE = CRON '0 2 * * *'
  AS 'CALL transform_extracted_data()';

CREATE JOB etl_load
  DEPENDS ON etl_transform
  SCHEDULE = CRON '0 3 * * *'
  AS 'CALL load_transformed_data()';
```

### ALTER JOB

```sql
-- Pause job
ALTER JOB job_name SET STATE = PAUSED;

-- Resume job
ALTER JOB job_name SET STATE = ENABLED;

-- Disable job
ALTER JOB job_name SET STATE = DISABLED;

-- Change schedule
ALTER JOB job_name SET SCHEDULE = CRON '0 */2 * * *';  -- Every 2 hours

-- Change retry policy
ALTER JOB job_name SET MAX_RETRIES = 5;
```

### DROP JOB

```sql
-- Delete job (also deletes job run history)
DROP JOB job_name;

-- Delete job, keep history
DROP JOB job_name KEEP HISTORY;
```

### Query Job Status

```sql
-- List all jobs
SELECT job_uuid, job_name, schedule_kind, cron_expression, next_run_time, state
FROM sys.jobs
ORDER BY next_run_time;

-- View job run history
SELECT job_run_uuid, job_name, scheduled_time, started_at, completed_at,
       state, result_message, rows_affected
FROM sys.job_runs
WHERE job_uuid = 'job_uuid_here'
ORDER BY started_at DESC
LIMIT 10;

-- View currently running jobs
SELECT job_uuid, job_name, started_at, CURRENT_TIMESTAMP - started_at AS duration
FROM sys.job_runs
WHERE state = 'RUNNING'
ORDER BY started_at;
```

---

## Catalog Schema

### sys.jobs

**Alpha uses SBCLUSTER-09 schema** (unchanged):

```sql
CREATE TABLE sys.jobs (
    job_uuid UUID PRIMARY KEY,
    job_name VARCHAR(255) NOT NULL UNIQUE,
    description TEXT,

    job_class VARCHAR(20),                  -- Alpha: Stored, not used
    job_type VARCHAR(20) NOT NULL,          -- 'SQL' | 'PROCEDURE' | 'EXTERNAL'
    job_sql TEXT,                           -- SQL to execute (for SQL jobs)
    procedure_uuid UUID,                    -- Procedure to call (for PROCEDURE jobs)
    external_command TEXT,                  -- Command to run (for EXTERNAL jobs)

    schedule_kind VARCHAR(20) NOT NULL,     -- CRON | AT | EVERY
    cron_expression VARCHAR(100),           -- For CRON
    interval_seconds BIGINT,                -- For EVERY
    starts_at BIGINT,                       -- Optional start time (ms)
    ends_at BIGINT,                         -- Optional end time (ms)
    schedule_tz VARCHAR(64),                -- Time zone name (optional, default UTC)
    next_run_time BIGINT,                   -- Next scheduled execution (ms)

    on_completion VARCHAR(20) DEFAULT 'PRESERVE', -- PRESERVE | DROP (AT jobs)

    partition_strategy VARCHAR(20),         -- Alpha: Stored, not used
    partition_shard_uuid UUID,              -- Alpha: Always null
    partition_expression TEXT,              -- Alpha: Stored, not used

    max_retries INTEGER DEFAULT 3,
    retry_backoff_seconds INTEGER DEFAULT 60,
    timeout_seconds INTEGER DEFAULT 3600,

    created_by_user_uuid UUID NOT NULL,
    run_as_role_uuid UUID,
    created_at BIGINT NOT NULL,
    state VARCHAR(20) NOT NULL DEFAULT 'ENABLED',  -- ENABLED | DISABLED | PAUSED

    FOREIGN KEY (created_by_user_uuid) REFERENCES sys.users(user_uuid)
);

CREATE INDEX idx_jobs_next_run_time ON sys.jobs(next_run_time) WHERE state = 'ENABLED';
CREATE INDEX idx_jobs_state ON sys.jobs(state);
```

### sys.job_runs

**Alpha uses SBCLUSTER-09 schema** (unchanged):

```sql
CREATE TABLE sys.job_runs (
    job_run_uuid UUID PRIMARY KEY,
    job_uuid UUID NOT NULL,

    assigned_node_uuid UUID,                -- Alpha: Always null
    shard_uuid UUID,                        -- Alpha: Always null

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

### sys.job_dependencies

**Alpha uses SBCLUSTER-09 schema** (unchanged):

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

### Compatibility Notes

**Alpha → Beta Migration**:
- Catalog schema identical (no migration needed)
- `assigned_node_uuid`, `shard_uuid` columns exist in Alpha (always null)
- Beta populates these columns during migration
- Zero downtime migration possible

---

## Implementation Plan

### Phase 1: Catalog Schema (Week 1)

**Files to Create**:
- `src/core/scheduler/catalog_schema.sql` (CREATE TABLE statements)
- `src/core/scheduler/catalog_manager.cpp` (CRUD operations for sys.jobs, sys.job_runs)

**Tasks**:
- [ ] Create `sys.jobs`, `sys.job_runs`, `sys.job_dependencies` tables
- [ ] Bootstrap schema during engine initialization
- [ ] Implement catalog CRUD operations
  - `createJob(Job) -> Status`
  - `getJob(job_uuid) -> Job`
  - `updateJob(job_uuid, fields) -> Status`
  - `deleteJob(job_uuid) -> Status`
  - `queryJobs(sql_filter) -> vector<Job>`
  - `createJobRun(JobRun) -> Status`
  - `updateJobRun(JobRun) -> Status`
  - `getJobRuns(job_uuid) -> vector<JobRun>`

**Acceptance Criteria**:
- [ ] Schema created on engine startup
- [ ] Can create job via catalog API
- [ ] Can query jobs from `sys.jobs`

**Estimated Duration**: 1 week

### Phase 2: Scheduler Thread (Week 2)

**Files to Create**:
- `include/scratchbird/core/scheduler/scheduler_thread.h`
- `src/core/scheduler/scheduler_thread.cpp`

**Tasks**:
- [ ] Implement `SchedulerThread` class (background thread)
- [ ] Implement scheduling loop (every 10 seconds)
- [ ] Cron expression parser (reuse from SBCLUSTER-09 spec)
- [ ] Next run time calculation
- [ ] Job execution queue (thread-safe)

**Acceptance Criteria**:
- [ ] Scheduler thread starts on engine startup
- [ ] Scheduling loop runs every 10 seconds
- [ ] Can parse cron expressions
- [ ] Can calculate next run time

**Estimated Duration**: 1 week

### Phase 3: Job Execution Engine (Week 3)

**Files to Modify**:
- `src/core/scheduler/scheduler_thread.cpp` (add execution logic)

**Tasks**:
- [ ] Implement `executeSqlJob()`
- [ ] Implement `executeProcedureJob()`
- [ ] Implement `executeExternalJob()`
- [ ] Transaction context (execute with job creator's privileges)
- [ ] Session variable injection (job_uuid, job_run_uuid)

**Acceptance Criteria**:
- [ ] Can execute SQL job
- [ ] Can execute stored procedure job
- [ ] Can execute external command job
- [ ] Job executes with creator's privileges

**Estimated Duration**: 1 week

### Phase 4: Failure Handling (Week 4)

**Files to Modify**:
- `src/core/scheduler/scheduler_thread.cpp` (add retry logic)

**Tasks**:
- [ ] Implement `handleJobFailure()`
- [ ] Retry with exponential backoff
- [ ] Max retries enforcement
- [ ] Error logging and alerting

**Acceptance Criteria**:
- [ ] Failed job retries automatically
- [ ] Exponential backoff: 60s, 120s, 240s, ...
- [ ] Max retries enforced (default: 3)
- [ ] Error logged to audit chain

**Estimated Duration**: 1 week

### Phase 5: Job Dependencies (Week 5)

**Files to Modify**:
- `src/core/scheduler/scheduler_thread.cpp` (add dependency checking)
- `src/core/scheduler/catalog_manager.cpp` (add dependency queries)

**Tasks**:
- [ ] Implement `dependenciesSatisfied()` check
- [ ] Query `sys.job_dependencies` table
- [ ] Verify all dependencies completed successfully
- [ ] Detect circular dependencies (at job creation time)

**Acceptance Criteria**:
- [ ] Job with dependencies waits for dependencies to complete
- [ ] Circular dependencies rejected at creation time
- [ ] ETL pipeline example works (extract → transform → load)

**Estimated Duration**: 1 week

### Phase 6: SQL Syntax (Week 6)

**Files to Modify**:
- `src/parser/parser_v2.cpp` (add job DDL parsing)
- `include/scratchbird/parser/ast_v2.h` (add job AST nodes)
- `src/sblr/bytecode_generator_v2.cpp` (add job DDL bytecode)
- `src/sblr/executor.cpp` (execute job DDL)

**Tasks**:
- [ ] Parser: `CREATE JOB` statement
- [ ] Parser: `ALTER JOB` statement
- [ ] Parser: `DROP JOB` statement
- [ ] AST nodes: `CreateJobStatement`, `AlterJobStatement`, `DropJobStatement`
- [ ] Bytecode generation for job DDL
- [ ] Executor: Call catalog APIs to create/update/delete jobs

**Acceptance Criteria**:
- [ ] Can execute `CREATE JOB` SQL
- [ ] Can execute `ALTER JOB SET STATE = PAUSED`
- [ ] Can execute `DROP JOB`
- [ ] Syntax errors reported correctly

**Estimated Duration**: 1 week

### Phase 7: Integration & Testing (Week 7)

**Files to Create**:
- `tests/unit/test_scheduler_catalog.cpp`
- `tests/unit/test_scheduler_execution.cpp`
- `tests/unit/test_cron_parsing.cpp`
- `tests/integration/test_scheduler_jobs.cpp`

**Tasks**:
- [ ] Unit tests: Catalog CRUD operations
- [ ] Unit tests: Cron expression parsing
- [ ] Unit tests: Next run time calculation
- [ ] Unit tests: Job execution (SQL, procedure, external)
- [ ] Unit tests: Retry logic
- [ ] Integration tests: Job scheduling end-to-end
- [ ] Integration tests: Job dependencies (ETL pipeline)
- [ ] Integration tests: Failure handling

**Acceptance Criteria**:
- [ ] All unit tests pass (100% code coverage)
- [ ] All integration tests pass
- [ ] Example jobs from spec work (daily_sweep, hourly_report, ETL pipeline)

**Estimated Duration**: 1 week

### Phase 8: Documentation (Week 8)

**Files to Create**:
- `docs/user_guide/JOB_SCHEDULER.md` (user-facing documentation)
- `docs/operations/SCHEDULER_OPERATIONS.md` (operational guide)

**Tasks**:
- [ ] User guide: How to create jobs
- [ ] User guide: SQL syntax reference
- [ ] User guide: Job state management (pause, resume, disable)
- [ ] User guide: Viewing job history
- [ ] Operations guide: Scheduler configuration
- [ ] Operations guide: Troubleshooting failed jobs
- [ ] Operations guide: Monitoring scheduler health

**Acceptance Criteria**:
- [ ] User guide complete with examples
- [ ] Operations guide complete with runbooks
- [ ] Documentation reviewed by 2 engineers

**Estimated Duration**: 1 week

**Total Alpha Implementation**: 8 weeks

---

## Beta Migration Path

### Zero Downtime Migration

**Alpha Job → Beta Job** (no changes required):

1. **Alpha job definition**:
```sql
CREATE JOB hourly_report
  SCHEDULE = CRON '0 * * * *'
  AS 'CALL generate_hourly_sales_report()';
```

2. **Alpha to Beta upgrade**:
   - Upgrade ScratchBird binaries (Alpha → Beta)
   - Catalog schema unchanged (compatible)
   - Beta reads `sys.jobs`, continues scheduling

3. **Beta execution**:
   - Beta Raft leader becomes scheduler control plane
   - Beta scheduler agents execute jobs (instead of embedded Alpha thread)
   - Job definition unchanged, execution context identical

4. **User experience**:
   - No SQL changes needed
   - No job downtime (jobs continue running during upgrade)
   - Job history preserved (`sys.job_runs` table unchanged)

### Beta Enhancements (Opt-In)

**After upgrading to Beta**, users can **optionally** enhance jobs with cluster features:

```sql
-- Alpha job (still works in Beta)
CREATE JOB daily_sweep
  SCHEDULE = CRON '0 2 * * *'
  AS 'SWEEP ANALYZE';             -- Native sweep/GC command (VACUUM alias available)

-- Beta enhancement: Add job class and partition rule
ALTER JOB daily_sweep
  SET CLASS = LOCAL_SAFE
  SET PARTITION BY ALL_SHARDS;

-- Result: Job now runs on all 16 shards (instead of single node)
```

**Opt-In, Not Breaking**:
- Alpha jobs work unchanged in Beta (single-node execution)
- Users add cluster features when ready (no rush)
- Gradual migration (job-by-job)

### Implementation Reuse

**Shared Code** (Alpha → Beta):
- ✅ Catalog schema (`sys.jobs`, `sys.job_runs`, `sys.job_dependencies`)
- ✅ Job execution engine (`executeSqlJob()`, `executeProcedureJob()`, `executeExternalJob()`)
- ✅ Retry logic (`handleJobFailure()`)
- ✅ Cron parsing (`parseCronExpression()`, `calculateNextRunTime()`)
- ✅ Job dependency resolution (`dependenciesSatisfied()`)
- ✅ SQL syntax (parser, AST nodes, bytecode generation)

**Beta Additions** (new code):
- 🆕 Raft control plane (scheduler leader)
- 🆕 Scheduler agents (per-node daemons)
- 🆕 Job class routing (LOCAL_SAFE, LEADER_ONLY, QUORUM_REQUIRED)
- 🆕 Partition rule evaluation (ALL_SHARDS, SINGLE_SHARD, DYNAMIC)

**Code Reuse**: ~80% of Alpha scheduler code reused in Beta.

---

## Alpha Configuration

### scratchbird.yaml

```yaml
scheduler:
  enabled: true                     # Enable scheduler (default: true)
  polling_interval_seconds: 10      # Scheduling loop frequency (default: 10)
  max_concurrent_jobs: 10           # Max concurrent job executions (default: 10)
  job_timeout_seconds: 3600         # Max job execution time (default: 1 hour)
  job_history_retention_days: 30    # Retain job run history (default: 30 days)

  # Retry policy defaults (overridable per job)
  default_max_retries: 3
  default_retry_backoff_seconds: 60
```

### Runtime Control

```sql
-- Disable scheduler (stops scheduling new jobs, running jobs continue)
ALTER SYSTEM SET scheduler.enabled = false;

-- Enable scheduler
ALTER SYSTEM SET scheduler.enabled = true;

-- Change polling interval
ALTER SYSTEM SET scheduler.polling_interval_seconds = 30;

-- Change max concurrent jobs
ALTER SYSTEM SET scheduler.max_concurrent_jobs = 20;
```

---

## Testing Requirements

### Unit Tests

**Test: Cron Parsing**:
```cpp
TEST(Scheduler, CronParsing) {
    auto expr = parseCronExpression("0 2 * * *");
    EXPECT_EQ(expr.minute, 0);
    EXPECT_EQ(expr.hour, 2);
    EXPECT_TRUE(expr.day_wildcard);
}
```

**Test: Next Run Time Calculation**:
```cpp
TEST(Scheduler, NextRunTime) {
    auto now = parseTimestamp("2026-01-09 10:00:00");
    auto next = calculateNextRunTime("0 * * * *", now);  // Every hour at :00
    EXPECT_EQ(next, parseTimestamp("2026-01-09 11:00:00"));
}
```

**Test: Job Execution**:
```cpp
TEST(Scheduler, SqlJobExecution) {
    Database db;
    db.execute("CREATE TABLE test_job_result (value INT)");

    Job job{
        .job_type = JobType::SQL,
        .job_sql = "INSERT INTO test_job_result VALUES (42)",
        .created_by_user_uuid = admin_user_uuid
    };

    JobRun run = createJobRun(job);
    executeSqlJob(job, run);

    EXPECT_EQ(run.state, JobRunState::COMPLETED);
    EXPECT_EQ(run.rows_affected, 1);

    auto result = db.query("SELECT value FROM test_job_result");
    EXPECT_EQ(result[0].value, 42);
}
```

### Integration Tests

**Test: Job Scheduling End-to-End**:
```cpp
TEST(SchedulerIntegration, JobSchedulingE2E) {
    Database db;

    // Create job scheduled for 5 seconds from now
    auto now = currentTimestamp();
    db.execute(fmt::format(
        "CREATE JOB test_job SCHEDULE = AT '{}' AS 'SELECT 1'",
        formatTimestamp(now + 5s)
    ));

    // Wait for job to execute
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Verify job ran
    auto runs = db.query("SELECT * FROM sys.job_runs WHERE job_name = 'test_job'");
    ASSERT_EQ(runs.size(), 1);
    EXPECT_EQ(runs[0].state, "COMPLETED");
}
```

**Test: Job Dependencies**:
```cpp
TEST(SchedulerIntegration, JobDependencies) {
    Database db;

    // Create dependency chain
    db.execute("CREATE JOB job_a SCHEDULE = AT '2026-01-09 10:00:00' AS 'SELECT 1'");
    db.execute("CREATE JOB job_b DEPENDS ON job_a SCHEDULE = AT '2026-01-09 10:01:00' AS 'SELECT 2'");
    db.execute("CREATE JOB job_c DEPENDS ON job_b SCHEDULE = AT '2026-01-09 10:02:00' AS 'SELECT 3'");

    // Fast-forward time, trigger scheduling
    advanceTimeTo("2026-01-09 10:03:00");

    // Verify execution order
    auto runs = db.query("SELECT job_name, started_at FROM sys.job_runs ORDER BY started_at");
    EXPECT_EQ(runs[0].job_name, "job_a");
    EXPECT_EQ(runs[1].job_name, "job_b");
    EXPECT_EQ(runs[2].job_name, "job_c");
}
```

---

## Security Model

### Privilege Enforcement

**Jobs execute with creator's privileges**:

```cpp
void executeSqlJob(const Job& job, JobRun& run) {
    // Get job creator's security context
    SecurityContext creator_ctx = security_manager_->getUserContext(job.created_by_user_uuid);

    // Execute SQL with creator's privileges
    Transaction txn = transaction_manager_->beginTransaction(creator_ctx);
    ResultSet result = txn.executeQuery(job.job_sql);
    txn.commit();
}
```

**Implication**: If creator's privileges are revoked, job execution fails.

### Access Control

**CREATE JOB**:
- Requires: `CREATE JOB` privilege

**ALTER JOB**:
- Requires: Job ownership OR `cluster_admin` role

**DROP JOB**:
- Requires: Job ownership OR `cluster_admin` role

**VIEW JOB HISTORY**:
- Job creator can view their own jobs
- `cluster_admin` can view all jobs
- Users with `VIEW JOB HISTORY` privilege can view all jobs

### Audit Logging

**Audit Events**:
- `JOB_CREATED`: When job is created
- `JOB_EXECUTED`: When job run completes (success or failure)
- `JOB_MODIFIED`: When job is altered
- `JOB_DELETED`: When job is dropped

**Audit Chain Integration**:
```cpp
audit_chain_->logEvent(AuditEventType::JOB_EXECUTED, {
    {"job_uuid", job.job_uuid.toString()},
    {"job_run_uuid", run.job_run_uuid.toString()},
    {"job_creator_uuid", job.created_by_user_uuid.toString()},
    {"state", jobRunStateToString(run.state)},
    {"rows_affected", run.rows_affected}
});
```

---

## Performance Characteristics

### Alpha Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Scheduling overhead | < 5 ms per job | Time to evaluate cron, create JobRun |
| Job execution latency | < 10 ms | Overhead to start job execution |
| Max concurrent jobs | 10 | Configurable (default: 10) |
| Polling interval | 10 seconds | Configurable (default: 10s) |
| Cron parsing | < 1 ms | Per cron expression |

### Scalability

**Alpha Limitations** (single-node):
- Max concurrent jobs: 10 (CPU-bound)
- Throughput: ~3,600 jobs/hour (assuming 1-second avg job duration)

**Beta Improvements** (cluster):
- Max concurrent jobs: 10 × N nodes (e.g., 100 jobs for 10-node cluster)
- Throughput: ~36,000 jobs/hour (10 nodes, 10 concurrent jobs/node)

---

## Examples

### Daily Sweep/GC

```sql
CREATE JOB daily_sweep
  SCHEDULE = CRON '0 2 * * *'
  DESCRIPTION = 'Daily sweep/GC + analyze'
  AS 'SWEEP ANALYZE';             -- Native sweep/GC command (VACUUM alias available)
```

### Hourly Report

```sql
CREATE JOB hourly_sales_report
  SCHEDULE = CRON '0 * * * *'
  DESCRIPTION = 'Generate hourly sales report'
  AS 'CALL generate_hourly_sales_report()';
```

### ETL Pipeline

```sql
CREATE JOB etl_extract
  SCHEDULE = CRON '0 1 * * *'
  AS 'CALL extract_source_data()';

CREATE JOB etl_transform
  DEPENDS ON etl_extract
  SCHEDULE = CRON '0 2 * * *'
  AS 'CALL transform_extracted_data()';

CREATE JOB etl_load
  DEPENDS ON etl_transform
  SCHEDULE = CRON '0 3 * * *'
  AS 'CALL load_transformed_data()';
```

### One-Time Migration

```sql
CREATE JOB migrate_users
  SCHEDULE = AT '2026-01-15 10:00:00'
  MAX_RETRIES = 5
  RETRY_BACKOFF = 300s  -- 5 minutes
  DESCRIPTION = 'Migrate users to new schema'
  AS 'CALL migrate_users_to_new_schema()';
```

---

## References

### Internal Documents

- **SBCLUSTER-09-SCHEDULER.md**: Full Beta scheduler specification
- **SCHEDULER_STATUS_REPORT.md**: Scheduler status assessment

### External References

- **Cron Expression Format**: https://en.wikipedia.org/wiki/Cron
- **PostgreSQL pg_cron**: https://github.com/citusdata/pg_cron

---

**Document Status**: ALPHA IMPLEMENTATION SPECIFICATION
**Last Updated**: 2026-01-09
**Implementation Target**: Alpha Polish Phase (before Beta)
**Estimated Duration**: 8 weeks

---

**End of Specification**
