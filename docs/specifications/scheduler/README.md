# Job Scheduler Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains job scheduler specifications for ScratchBird's automated job execution system.

## Overview

ScratchBird implements a comprehensive job scheduler for automated database maintenance, ETL workflows, and scheduled operations. The scheduler supports both standalone (Alpha) and distributed cluster (Beta) deployments with a forward-compatible design.

## Specifications in this Directory

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
CREATE JOB daily_vacuum
  CLASS = LOCAL_SAFE              -- Alpha: Accepted, ignored
  PARTITION BY ALL_SHARDS         -- Alpha: Accepted, ignored
  SCHEDULE = '0 2 * * *'          -- Alpha: Fully supported
  AS 'VACUUM ANALYZE';            -- Alpha: Fully supported
```

## Catalog Schema

Jobs are stored in system catalog:

```sql
-- Job definitions
CREATE TABLE sys.jobs (
    job_uuid UUID PRIMARY KEY,
    job_name VARCHAR(255) NOT NULL UNIQUE,
    job_class VARCHAR(20),              -- Alpha: stored but unused
    job_type VARCHAR(20) NOT NULL,      -- SQL | PROCEDURE | EXTERNAL
    job_sql TEXT,
    schedule_type VARCHAR(20) NOT NULL, -- ONE_TIME | RECURRING
    cron_expression VARCHAR(100),
    next_run_time BIGINT,
    partition_strategy VARCHAR(20),     -- Alpha: stored but unused
    max_retries INTEGER DEFAULT 3,
    retry_backoff_seconds INTEGER DEFAULT 60,
    created_by_user_uuid UUID NOT NULL,
    state VARCHAR(20) NOT NULL DEFAULT 'ENABLED'
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
    state VARCHAR(20) NOT NULL,         -- PENDING | RUNNING | COMPLETED | FAILED
    retry_count INTEGER DEFAULT 0,
    result_message TEXT,
    rows_affected BIGINT,
    FOREIGN KEY (job_uuid) REFERENCES sys.jobs(job_uuid)
);

-- Job dependencies (DAG support)
CREATE TABLE sys.job_dependencies (
    parent_job_uuid UUID NOT NULL,
    child_job_uuid UUID NOT NULL,
    PRIMARY KEY (parent_job_uuid, child_job_uuid),
    FOREIGN KEY (parent_job_uuid) REFERENCES sys.jobs(job_uuid),
    FOREIGN KEY (child_job_uuid) REFERENCES sys.jobs(job_uuid)
);
```

## SQL Syntax

### CREATE JOB
```sql
CREATE JOB job_name
  [CLASS = LOCAL_SAFE | LEADER_ONLY | QUORUM_REQUIRED]
  [PARTITION BY ALL_SHARDS | SINGLE_SHARD | DYNAMIC]
  SCHEDULE = 'cron_expression'
  [MAX_RETRIES = n]
  [RETRY_BACKOFF = n]
  AS 'sql_statement';
```

### ALTER JOB
```sql
ALTER JOB job_name
  [SET SCHEDULE = 'cron_expression']
  [SET STATE = ENABLED | DISABLED | PAUSED]
  [SET MAX_RETRIES = n];
```

### DROP JOB
```sql
DROP JOB [IF EXISTS] job_name [CASCADE | RESTRICT];
```

### Manual Execution
```sql
EXECUTE JOB job_name;
```

## Example Jobs

### Daily Vacuum
```sql
CREATE JOB daily_vacuum
  SCHEDULE = '0 2 * * *'  -- Run at 2:00 AM daily
  AS 'VACUUM ANALYZE';
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

## Related Specifications

- [Cluster Scheduler](../Cluster%20Specification%20Work/SBCLUSTER-09-SCHEDULER.md) - Beta distributed scheduler
- [Catalog](../catalog/) - System catalog schema
- [Parser](../parser/) - Job DDL parsing
- [Transaction System](../transaction/) - Transaction management for jobs
- [Security](../Security%20Design%20Specification/) - Job execution privileges

## Critical Reading

Before working on scheduler implementation:

1. **MUST READ:** [../../MGA_RULES.md](../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../IMPLEMENTATION_STANDARDS.md](../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ:** [ALPHA_SCHEDULER_SPECIFICATION.md](ALPHA_SCHEDULER_SPECIFICATION.md) - Alpha scheduler design
4. **READ:** [../Cluster%20Specification%20Work/SBCLUSTER-09-SCHEDULER.md](../Cluster%20Specification%20Work/SBCLUSTER-09-SCHEDULER.md) - Beta scheduler design

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Related:** [Cluster Specifications](../Cluster%20Specification%20Work/README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
