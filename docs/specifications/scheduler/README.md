# Job Scheduler Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains job scheduler specifications for ScratchBird's automated job execution system.

## Overview

ScratchBird implements a comprehensive job scheduler for automated database maintenance, ETL workflows, and scheduled operations. The scheduler supports both standalone (Alpha) and distributed cluster (Beta) deployments with a forward-compatible design.

## Specifications in this Directory

- **[SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md](SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md)** - Canonical scheduler and job runner spec (Alpha + Beta)
  - Consolidates Alpha and Beta scheduler behavior
  - Clarifies scheduler vs event notifications
  - Security-hardening and cluster-ready rules
- **[ALPHA_SCHEDULER_SPECIFICATION.md](ALPHA_SCHEDULER_SPECIFICATION.md)** - Alpha standalone scheduler specification
  - Single-threaded scheduler embedded in engine process
  - Non-clustered, single-node operation
  - Forward-compatible with Beta cluster scheduler
  - Cron-based scheduling
  - Job dependencies and DAG support
  - Retry with exponential backoff

## Key Features

### Job Types

1. **SQL Jobs** - Execute SQL statements
2. **Procedure Jobs** - Call stored procedures
3. **External Jobs** - Execute shell commands/scripts

### Scheduling

- **Cron expressions** - Standard 5-field cron format (`minute hour day month weekday`)
- **One-time jobs** - Execute once at specified time
- **Recurring jobs** - Execute on schedule indefinitely
- **Dependencies** - DAG-based job dependencies for ETL pipelines

### Failure Handling

- **Retry logic** - Automatic retry with exponential backoff
- **Configurable retries** - Set max retry count per job
- **Failure notifications** - Log job failures to audit trail
- **Job state tracking** - Track job runs, success/failure history

## Alpha vs Beta Scheduler

| Feature | Alpha | Beta |
|---------|-------|------|
| Architecture | Embedded thread | Distributed (Raft + agents) |
| Deployment | Single node | Multi-node cluster |
| Job Classes | Ignored | LOCAL_SAFE, LEADER_ONLY, QUORUM_REQUIRED |
| Partition Rules | Ignored | ALL_SHARDS, SINGLE_SHARD, DYNAMIC |
| Coordination | None | Raft consensus |
| Catalog Schema | ✅ Identical | ✅ Identical |
| SQL Syntax | ✅ Same | ✅ Same |
| Code Reuse | 80% reused in Beta | ✅ |

### Forward Compatibility

Alpha scheduler accepts Beta syntax but ignores cluster-specific features:

```sql
-- Works in both Alpha and Beta
CREATE JOB daily_sweep
  CLASS = LOCAL_SAFE              -- Alpha: Accepted, ignored
  PARTITION BY ALL_SHARDS         -- Alpha: Accepted, ignored
  SCHEDULE = '0 2 * * *'          -- Alpha: Fully supported
  AS 'SWEEP ANALYZE';             -- Native sweep/GC command (VACUUM alias available)
```

Note: VACUUM is a PostgreSQL-compatibility command that maps to Firebird-style sweep/GC
in ScratchBird. It does not imply PostgreSQL VACUUM phases.

### Event Notifications

Event notifications (Firebird-style POST_EVENT) are a separate feature and are
specified in `ScratchBird/docs/specifications/ddl/DDL_EVENTS.md`.

## Catalog Schema

Jobs are stored in system catalog:

```sql
-- Job definitions
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
    run_as_role_uuid UUID,
    created_at BIGINT NOT NULL,
    state VARCHAR(20) NOT NULL DEFAULT 'ENABLED' -- ENABLED | DISABLED | PAUSED
);

-- Job execution history
CREATE TABLE sys.job_runs (
    job_run_uuid UUID PRIMARY KEY,
    job_uuid UUID NOT NULL,
    assigned_node_uuid UUID,            -- Alpha: always NULL
    shard_uuid UUID,                    -- Alpha: always NULL
    scheduled_time BIGINT NOT NULL,
    started_at BIGINT,
    completed_at BIGINT,
    state VARCHAR(20) NOT NULL,         -- PENDING | RUNNING | COMPLETED | FAILED | CANCELLED
    retry_count INTEGER DEFAULT 0,
    result_message TEXT,
    rows_affected BIGINT,
    FOREIGN KEY (job_uuid) REFERENCES sys.jobs(job_uuid)
);

-- Job dependencies (DAG support)
CREATE TABLE sys.job_dependencies (
    job_uuid UUID NOT NULL,
    depends_on_job_uuid UUID NOT NULL,
    PRIMARY KEY (job_uuid, depends_on_job_uuid),
    FOREIGN KEY (job_uuid) REFERENCES sys.jobs(job_uuid),
    FOREIGN KEY (depends_on_job_uuid) REFERENCES sys.jobs(job_uuid)
);
```

## SQL Syntax

### CREATE JOB
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
```

### ALTER JOB
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

### DROP JOB
```sql
DROP JOB job_name [KEEP HISTORY];
```

### Manual Execution
```sql
EXECUTE JOB job_name;
```

### Cancel a Job Run
```sql
CANCEL JOB RUN job_run_uuid;
```

## Example Jobs

### Daily Sweep/GC
```sql
CREATE JOB daily_sweep
  SCHEDULE = '0 2 * * *'  -- Run at 2:00 AM daily
  AS 'SWEEP ANALYZE';
```

### Hourly Statistics Update
```sql
CREATE JOB update_stats
  SCHEDULE = '0 * * * *'  -- Run at the start of every hour
  AS 'CALL refresh_statistics()';
```

### ETL Pipeline with Dependencies
```sql
-- Step 1: Extract
CREATE JOB extract_data
  SCHEDULE = '0 1 * * *'
  AS 'CALL extract_from_source()';

-- Step 2: Transform (depends on extract)
CREATE JOB transform_data
  DEPENDS ON extract_data
  AS 'CALL transform_staging_data()';

-- Step 3: Load (depends on transform)
CREATE JOB load_data
  DEPENDS ON transform_data
  AS 'CALL load_to_warehouse()';
```

## Implementation Status

**Alpha:** Specified, not yet implemented
**Beta:** Specified (see [Cluster Specification Work](../Cluster%20Specification%20Work/SBCLUSTER-09-SCHEDULER.md))
**Canonical:** [SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md](SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md)

## Related Specifications

- [Cluster Scheduler](../Cluster%20Specification%20Work/SBCLUSTER-09-SCHEDULER.md) - Beta distributed scheduler
- [Catalog](../catalog/) - System catalog schema
- [Parser](../parser/) - Job DDL parsing
- [Transaction System](../transaction/) - Transaction management for jobs
- [Security](../Security%20Design%20Specification/) - Job execution privileges

## Critical Reading

Before working on scheduler implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ:** [ALPHA_SCHEDULER_SPECIFICATION.md](ALPHA_SCHEDULER_SPECIFICATION.md) - Alpha scheduler design
4. **READ:** [../Cluster%20Specification%20Work/SBCLUSTER-09-SCHEDULER.md](../Cluster%20Specification%20Work/SBCLUSTER-09-SCHEDULER.md) - Beta scheduler design

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Related:** [Cluster Specifications](../Cluster%20Specification%20Work/README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
