# SBCLUSTER-09: Scheduler

## 1. Introduction

**Canonical spec:** This document is the Beta/cluster reference. The canonical scheduler
spec that consolidates Alpha + Beta behavior and resolves DDL differences is:
`ScratchBird/docs/specifications/scheduler/SCHEDULER_JOB_RUNNER_CANONICAL_SPEC.md`.

### 1.1 Purpose
This document specifies the distributed scheduler architecture for ScratchBird clusters. The scheduler enables **cluster-controlled job execution** with distributed scheduler agents, job classes for different execution guarantees, and partition rules for shard-specific jobs.

### 1.2 Scope
- Central control plane for job scheduling
- Distributed scheduler agents on nodes
- Job classes (LOCAL_SAFE, LEADER_ONLY, QUORUM_REQUIRED)
- Partition rules for shard-specific execution
- Cron-like scheduling and recurring jobs
- Job dependencies and execution ordering
- Failure handling and retry policies
- Job execution audit logging

### 1.3 Related Documents
- **SBCLUSTER-00**: Guiding Principles (push compute to data)
- **SBCLUSTER-01**: Cluster Configuration Epoch (scheduler policy in CCE)
- **SBCLUSTER-02**: Membership and Identity (SCHEDULER_AGENT role)
- **SBCLUSTER-05**: Sharding (partition rules route jobs to shards)

### 1.4 Terminology
- **Scheduler Control Plane**: Raft-based central scheduler that manages job definitions
- **Scheduler Agent**: Per-node daemon that executes scheduled jobs
- **Job Class**: Execution guarantee category (LOCAL_SAFE, LEADER_ONLY, QUORUM_REQUIRED)
- **Partition Rule**: Rule determining which shard(s) execute a job
- **Job Run**: Single execution instance of a job
- **Recurring Job**: Job that executes on a schedule (cron-like)

---

## 2. Architecture Overview

### 2.1 Design Principles

1. **Central Control Plane**: Raft leader manages job definitions and schedules
2. **Distributed Execution**: Scheduler agents on each node execute jobs locally
3. **At-Least-Once Execution**: Jobs guaranteed to execute at least once (idempotency required)
4. **Job Classes**: Different execution guarantees for different use cases
5. **Partition-Aware**: Jobs can target specific shards for shard-local computation
6. **Failure Recovery**: Jobs automatically retry on failure with exponential backoff

### 2.2 Scheduler Topology

```
┌─────────────────────────────────────────────────────────────┐
│              SCHEDULER CONTROL PLANE (Raft)                 │
│  - Stores job definitions                                   │
│  - Determines when jobs should run                          │
│  - Assigns jobs to scheduler agents                         │
└─────┬───────────────┬───────────────┬───────────────────────┘
      │               │               │
      ▼               ▼               ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│  NODE A     │ │  NODE B     │ │  NODE C     │
│  Scheduler  │ │  Scheduler  │ │  Scheduler  │
│  Agent      │ │  Agent      │ │  Agent      │
│             │ │             │ │             │
│  Execute    │ │  Execute    │ │  Execute    │
│  jobs       │ │  jobs       │ │  jobs       │
│  assigned   │ │  assigned   │ │  assigned   │
│  to this    │ │  to this    │ │  to this    │
│  node       │ │  node       │ │  node       │
└─────────────┘ └─────────────┘ └─────────────┘
```

### 2.3 Job Lifecycle

```
DEFINED → SCHEDULED → ASSIGNED → RUNNING → COMPLETED/FAILED
   │                                │
   └────────────────────────────────┘
        (for recurring jobs)
```

---

## 3. Data Structures

### 3.1 Job

```cpp
struct Job {
    UUID job_uuid;                       // Unique job identifier (v7)
    string job_name;                     // Human-readable name
    string description;                  // Job description

    JobClass job_class;                  // LOCAL_SAFE | LEADER_ONLY | QUORUM_REQUIRED
    JobType job_type;                    // SQL | PROCEDURE | EXTERNAL

    // Job execution details
    string job_sql;                      // SQL to execute (for SQL jobs)
    UUID procedure_uuid;                 // Procedure to call (for PROCEDURE jobs)
    string external_command;             // Command to run (for EXTERNAL jobs)

    // Scheduling
    JobScheduleType schedule_type;       // ONE_TIME | RECURRING
    string cron_expression;              // Cron expression (for RECURRING)
    timestamp_t next_run_time;           // Next scheduled execution

    // Partition rules
    PartitionRule partition_rule;        // How to partition job across shards

    // Retry policy
    uint32_t max_retries;                // Max retry attempts (default: 3)
    duration_t retry_backoff;            // Backoff between retries (default: 60s)

    // Job metadata
    UUID created_by_user_uuid;
    timestamp_t created_at;
    JobState state;                      // ENABLED | DISABLED | PAUSED
};

enum JobClass {
    LOCAL_SAFE,         // Can run on any node, no coordination needed
    LEADER_ONLY,        // Must run on Raft leader only
    QUORUM_REQUIRED     // Requires quorum acknowledgment before execution
};

enum JobType {
    SQL,                // Execute SQL query
    PROCEDURE,          // Call stored procedure
    EXTERNAL            // Run external command
};

enum JobScheduleType {
    ONE_TIME,           // Run once at specified time
    RECURRING           // Run on cron schedule
};

enum JobState {
    ENABLED,            // Job is active and will be scheduled
    DISABLED,           // Job is disabled (won't run)
    PAUSED              // Job is temporarily paused
};
```

### 3.2 PartitionRule

```cpp
struct PartitionRule {
    PartitionStrategy strategy;          // ALL_SHARDS | SINGLE_SHARD | SHARD_SET

    // For SINGLE_SHARD
    UUID shard_uuid;                     // Specific shard to run on

    // For SHARD_SET
    UUID[] shard_uuids;                  // Set of shards to run on

    // For parameterized partitioning
    string partition_expression;         // SQL expression to determine shard
};

enum PartitionStrategy {
    ALL_SHARDS,         // Run job on all shards
    SINGLE_SHARD,       // Run job on one specific shard
    SHARD_SET,          // Run job on a set of shards
    DYNAMIC             // Determine shard at runtime via expression
};
```

### 3.3 JobRun

```cpp
struct JobRun {
    UUID job_run_uuid;                   // Unique run identifier (v7)
    UUID job_uuid;                       // Parent job

    UUID assigned_node_uuid;             // Node assigned to execute this run
    UUID shard_uuid;                     // Shard this run is for (if partitioned)

    timestamp_t scheduled_time;          // When run was scheduled
    timestamp_t started_at;              // When execution started
    timestamp_t completed_at;            // When execution completed

    JobRunState state;                   // PENDING | RUNNING | COMPLETED | FAILED
    uint32_t retry_count;                // Number of retries so far

    // Results
    string result_message;               // Success/error message
    uint64_t rows_affected;              // Rows affected (for SQL jobs)
    bytes result_data;                   // Optional result data

    ErrorCode error_code;                // Error code (if failed)
};

enum JobRunState {
    PENDING,            // Waiting to be picked up by scheduler agent
    RUNNING,            // Currently executing
    COMPLETED,          // Successfully completed
    FAILED,             // Failed (may retry)
    CANCELLED           // Cancelled by administrator
};
```

### 3.4 SchedulerPolicy

```cpp
struct SchedulerPolicy {
    uint32_t max_concurrent_jobs;        // Max concurrent job runs per node (e.g., 10)
    duration_t job_timeout;              // Max execution time per job (e.g., 1 hour)
    duration_t polling_interval;         // How often agent polls for jobs (e.g., 10s)

    bool enable_job_history;             // Store job run history?
    uint32_t job_history_retention_days; // Retain history for N days (e.g., 30)
};
```

---

## 4. Job Classes

### 4.1 LOCAL_SAFE

**Guarantee**: Job can run on any node without coordination.

**Use Cases**:
- Per-shard maintenance tasks (sweep/GC, ANALYZE)
- Local cache warming
- Log rotation

**Execution**:
- Scheduler assigns job to any available node
- No quorum or leader coordination required
- Multiple instances can run concurrently (if partitioned)

**Example**:
```sql
CREATE JOB sweep_tables
  CLASS = LOCAL_SAFE
  PARTITION BY ALL_SHARDS
  SCHEDULE = '0 2 * * *'  -- Daily at 2 AM
  AS 'SWEEP ANALYZE';             -- Native sweep/GC command (VACUUM alias available)
```

### 4.2 LEADER_ONLY

**Guarantee**: Job runs on Raft leader only (exactly once per schedule).

**Use Cases**:
- Cluster-wide configuration changes
- Backup coordination
- Cluster health checks
- Report generation

**Execution**:
- Only Raft leader schedules and executes this job
- If leader fails, new leader takes over scheduling
- Ensures exactly-once execution per schedule

**Example**:
```sql
CREATE JOB cluster_health_check
  CLASS = LEADER_ONLY
  SCHEDULE = '*/5 * * * *'  -- Every 5 minutes
  AS 'SELECT check_cluster_health()';
```

### 4.3 QUORUM_REQUIRED

**Guarantee**: Job execution requires quorum acknowledgment.

**Use Cases**:
- Critical data migrations
- Compliance-required operations
- Financial transactions

**Execution**:
- Scheduler proposes job execution via Raft
- Waits for quorum to acknowledge before executing
- If quorum not achieved, job is rescheduled

**Example**:
```sql
CREATE JOB monthly_compliance_report
  CLASS = QUORUM_REQUIRED
  SCHEDULE = '0 0 1 * *'  -- First day of month at midnight
  AS 'CALL generate_compliance_report()';
```

---

## 5. Scheduling

### 5.1 Cron Expression Format

ScratchBird uses standard cron format:

```
┌───────────── minute (0 - 59)
│ ┌───────────── hour (0 - 23)
│ │ ┌───────────── day of month (1 - 31)
│ │ │ ┌───────────── month (1 - 12)
│ │ │ │ ┌───────────── day of week (0 - 6) (Sunday to Saturday)
│ │ │ │ │
│ │ │ │ │
* * * * *
```

**Examples**:
- `0 2 * * *` - Daily at 2 AM
- `*/15 * * * *` - Every 15 minutes
- `0 0 * * 0` - Weekly on Sunday at midnight
- `0 0 1 * *` - Monthly on the 1st at midnight

### 5.2 Job Scheduling Algorithm

```cpp
void schedule_jobs() {
    timestamp_t now = current_timestamp();

    // Get all enabled jobs
    vector<Job> jobs = get_enabled_jobs();

    for (Job& job : jobs) {
        // Check if job is due to run
        if (job.next_run_time <= now) {
            // Create job run(s)
            vector<JobRun> runs = create_job_runs(job);

            // Assign runs to scheduler agents
            for (JobRun& run : runs) {
                assign_job_run_to_agent(run);
            }

            // Update next run time (for recurring jobs)
            if (job.schedule_type == JobScheduleType::RECURRING) {
                job.next_run_time = calculate_next_run_time(job.cron_expression, now);
                update_job(job);
            } else {
                // One-time job - disable after scheduling
                job.state = JobState::DISABLED;
                update_job(job);
            }
        }
    }
}
```

### 5.3 Next Run Time Calculation

```cpp
timestamp_t calculate_next_run_time(const string& cron_expr, timestamp_t after) {
    CronExpression cron = parse_cron_expression(cron_expr);

    timestamp_t candidate = after + duration_t::minutes(1);  // Start from next minute

    while (true) {
        if (matches_cron_expression(cron, candidate)) {
            return candidate;
        }
        candidate += duration_t::minutes(1);

        // Safety: don't loop forever
        if (candidate > after + duration_t::days(365)) {
            throw InvalidCronExpressionException("No match found within 1 year");
        }
    }
}
```

---

## 6. Job Execution

### 6.1 Scheduler Agent Loop

Each node runs a scheduler agent that polls for assigned jobs:

```cpp
void scheduler_agent_main_loop() {
    while (true) {
        // 1. Poll for assigned job runs
        vector<JobRun> assigned_runs = poll_for_assigned_jobs(my_node_uuid);

        // 2. Execute jobs concurrently (up to max_concurrent_jobs)
        for (JobRun& run : assigned_runs) {
            if (get_running_job_count() < scheduler_policy.max_concurrent_jobs) {
                // Execute job asynchronously
                async([run]() {
                    execute_job_run(run);
                });
            } else {
                // Agent is at capacity - job will be picked up next poll
                break;
            }
        }

        // 3. Sleep until next poll
        sleep(scheduler_policy.polling_interval);
    }
}
```

### 6.2 Job Run Execution

```cpp
void execute_job_run(JobRun& run) {
    // 1. Mark run as RUNNING
    run.state = JobRunState::RUNNING;
    run.started_at = now();
    update_job_run(run);

    try {
        // 2. Get job definition
        Job job = get_job(run.job_uuid);

        // 3. Execute based on job type
        switch (job.job_type) {
            case JobType::SQL:
                execute_sql_job(job, run);
                break;

            case JobType::PROCEDURE:
                execute_procedure_job(job, run);
                break;

            case JobType::EXTERNAL:
                execute_external_job(job, run);
                break;
        }

        // 4. Mark as COMPLETED
        run.state = JobRunState::COMPLETED;
        run.completed_at = now();
        update_job_run(run);

        log_info("Job run {} completed successfully", run.job_run_uuid);

    } catch (const Exception& e) {
        // 5. Handle failure
        handle_job_failure(run, job, e);
    }
}
```

### 6.3 SQL Job Execution

```cpp
void execute_sql_job(const Job& job, JobRun& run) {
    // 1. Start transaction
    Transaction txn = begin_transaction(TransactionMode::READ_WRITE);

    // 2. Set job execution context
    txn.set_session_var("job_uuid", job.job_uuid.to_string());
    txn.set_session_var("job_run_uuid", run.job_run_uuid.to_string());

    // 3. Execute SQL
    ResultSet result = txn.execute_query(job.job_sql);

    // 4. Commit transaction
    txn.commit();

    // 5. Record results
    run.rows_affected = result.rows_affected;
    run.result_message = fmt::format("Query executed successfully, {} rows affected",
        result.rows_affected);
}
```

### 6.4 Failure Handling

```cpp
void handle_job_failure(JobRun& run, const Job& job, const Exception& e) {
    run.state = JobRunState::FAILED;
    run.error_code = e.code();
    run.result_message = e.message();
    run.completed_at = now();
    update_job_run(run);

    log_error("Job run {} failed: {}", run.job_run_uuid, e.message());

    // Retry if retries remaining
    if (run.retry_count < job.max_retries) {
        // Schedule retry with exponential backoff
        duration_t backoff = job.retry_backoff * (1 << run.retry_count);  // 2^retry_count

        JobRun retry_run = run;
        retry_run.job_run_uuid = UUID::v7();
        retry_run.retry_count = run.retry_count + 1;
        retry_run.scheduled_time = now() + backoff;
        retry_run.state = JobRunState::PENDING;

        create_job_run(retry_run);

        log_info("Scheduled retry {} for job {} in {}s",
            retry_run.retry_count, job.job_uuid, backoff.seconds());
    } else {
        // Max retries exceeded
        log_error("Job {} exceeded max retries ({})", job.job_uuid, job.max_retries);
        alert_administrators("JOB_MAX_RETRIES_EXCEEDED", job.job_uuid);
    }
}
```

---

## 7. Partition Rules

### 7.1 ALL_SHARDS Partitioning

Execute job on all shards:

```sql
CREATE JOB analyze_all_tables
  CLASS = LOCAL_SAFE
  PARTITION BY ALL_SHARDS
  SCHEDULE = '0 3 * * *'
  AS 'ANALYZE TABLES';
```

**Execution**:
- Scheduler creates one JobRun per shard
- Each run assigned to the node owning that shard
- Runs execute concurrently

### 7.2 SINGLE_SHARD Partitioning

Execute job on one specific shard:

```sql
CREATE JOB rebuild_shard_001_indexes
  CLASS = LOCAL_SAFE
  PARTITION BY SHARD 'shard_001'
  SCHEDULE = '0 4 * * 0'  -- Weekly on Sunday
  AS 'REINDEX DATABASE';
```

**Execution**:
- Job runs only on specified shard
- Assigned to node owning that shard

### 7.3 Dynamic Partitioning

Determine shard at runtime:

```sql
CREATE JOB process_customer_orders
  CLASS = LOCAL_SAFE
  PARTITION BY SHARD_KEY(customer_id)
  SCHEDULE = '*/10 * * * *'
  AS 'CALL process_pending_orders(:customer_id)';
```

**Execution**:
- Job extracts `customer_id` parameter
- Routes to shard based on shard key hash
- Useful for parameterized jobs

---

## 8. Job Dependencies

### 8.1 Dependency Graph

Jobs can depend on other jobs completing first:

```sql
CREATE JOB extract_daily_data
  SCHEDULE = '0 1 * * *'
  AS 'CALL extract_daily_sales()';

CREATE JOB transform_daily_data
  DEPENDS ON extract_daily_data
  SCHEDULE = '0 2 * * *'
  AS 'CALL transform_sales_data()';

CREATE JOB load_daily_data
  DEPENDS ON transform_daily_data
  SCHEDULE = '0 3 * * *'
  AS 'CALL load_sales_to_warehouse()';
```

**Execution**:
- `transform_daily_data` waits for `extract_daily_data` to complete
- `load_daily_data` waits for `transform_daily_data` to complete
- Forms a dependency chain (ETL pipeline)

### 8.2 Dependency Resolution

```cpp
bool can_schedule_job(const Job& job, timestamp_t now) {
    // Check if scheduled time has arrived
    if (job.next_run_time > now) {
        return false;
    }

    // Check if all dependencies have completed
    for (const UUID& dep_job_uuid : job.dependency_job_uuids) {
        JobRun last_run = get_last_job_run(dep_job_uuid);

        // Dependency must have completed successfully
        if (last_run.state != JobRunState::COMPLETED) {
            return false;
        }

        // Dependency must have run recently (within same schedule window)
        if (last_run.completed_at < get_schedule_window_start(job)) {
            return false;
        }
    }

    return true;
}
```

---

## 9. Operational Procedures

### 9.1 Create a Job

```sql
-- Create a daily backup job
CREATE JOB daily_cluster_backup
  CLASS = LEADER_ONLY
  SCHEDULE = '0 2 * * *'
  DESCRIPTION = 'Daily cluster backup'
  AS 'BACKUP CLUSTER TO backup_target_s3';
```

### 9.2 List Jobs

```sql
SELECT
    job_uuid,
    job_name,
    job_class,
    schedule_type,
    cron_expression,
    next_run_time,
    state
FROM sys.jobs
ORDER BY next_run_time ASC;
```

### 9.3 View Job Run History

```sql
SELECT
    job_run_uuid,
    job_name,
    scheduled_time,
    started_at,
    completed_at,
    state,
    result_message,
    rows_affected
FROM sys.job_runs
WHERE job_uuid = 'job_uuid_here'
ORDER BY started_at DESC
LIMIT 10;
```

### 9.4 Pause a Job

```sql
ALTER JOB daily_cluster_backup SET STATE = PAUSED;
```

### 9.5 Resume a Job

```sql
ALTER JOB daily_cluster_backup SET STATE = ENABLED;
```

### 9.6 Delete a Job

```sql
DROP JOB daily_cluster_backup;
```

---

## 10. Security Considerations

### 10.1 Job Execution Privileges

Jobs execute with the privileges of the user who created them:

```cpp
void execute_sql_job(const Job& job, JobRun& run) {
    // Get job creator's security context
    SecurityContext job_creator_ctx = get_user_security_context(job.created_by_user_uuid);

    // Execute SQL with job creator's privileges
    Transaction txn = begin_transaction(job_creator_ctx);

    ResultSet result = txn.execute_query(job.job_sql);
    txn.commit();
}
```

**Implication**: If job creator's privileges are revoked, job execution will fail.

### 10.2 Job Definition Access Control

**Who can create jobs?**
- Users with `CREATE JOB` privilege

**Who can modify jobs?**
- Job creator
- Cluster administrators

**Who can view job runs?**
- Job creator
- Cluster administrators
- Users with `VIEW JOB HISTORY` privilege

### 10.3 Audit Logging

All job operations are audited:

```cpp
void log_job_execution_audit(const Job& job, const JobRun& run) {
    AuditRecord record;
    record.event_type = AuditEventType::JOB_EXECUTED;
    record.job_uuid = job.job_uuid;
    record.job_run_uuid = run.job_run_uuid;
    record.job_creator_uuid = job.created_by_user_uuid;
    record.executor_node_uuid = run.assigned_node_uuid;
    record.state = run.state;
    record.rows_affected = run.rows_affected;
    record.timestamp = run.started_at;

    audit_log.write(record);
}
```

---

## 11. Testing Procedures

### 11.1 Functional Tests

**Test: Job Scheduling**
```cpp
TEST(Scheduler, JobScheduling) {
    auto cluster = create_test_cluster(3, 16);

    // Create a job scheduled for 5 seconds from now
    Job job = create_test_job(now() + 5s);

    // Wait for job to execute
    ASSERT_TRUE(cluster.wait_for_job_execution(job.job_uuid, 10s));

    // Verify job ran successfully
    JobRun last_run = cluster.get_last_job_run(job.job_uuid);
    EXPECT_EQ(last_run.state, JobRunState::COMPLETED);
}
```

**Test: Job Retry on Failure**
```cpp
TEST(Scheduler, JobRetry) {
    auto cluster = create_test_cluster(3, 16);

    // Create a job that will fail initially
    Job job = create_failing_job(max_retries: 3);

    // Wait for retries to exhaust
    ASSERT_TRUE(cluster.wait_for_job_max_retries(job.job_uuid, 60s));

    // Verify 4 runs (initial + 3 retries)
    vector<JobRun> runs = cluster.get_job_runs(job.job_uuid);
    EXPECT_EQ(runs.size(), 4);
}
```

**Test: Partition by ALL_SHARDS**
```cpp
TEST(Scheduler, PartitionAllShards) {
    auto cluster = create_test_cluster(3, 16);

    // Create job partitioned by ALL_SHARDS
    Job job = create_test_job(
        partition_rule: PartitionRule{strategy: ALL_SHARDS}
    );

    // Wait for all runs to complete
    ASSERT_TRUE(cluster.wait_for_job_completion(job.job_uuid, 30s));

    // Verify 16 runs (one per shard)
    vector<JobRun> runs = cluster.get_job_runs(job.job_uuid);
    EXPECT_EQ(runs.size(), 16);

    // Verify each shard has one run
    set<UUID> shard_uuids;
    for (const JobRun& run : runs) {
        shard_uuids.insert(run.shard_uuid);
    }
    EXPECT_EQ(shard_uuids.size(), 16);
}
```

---

## 12. Examples

### 12.1 Daily Sweep/GC Job

```sql
CREATE JOB daily_sweep
  CLASS = LOCAL_SAFE
  PARTITION BY ALL_SHARDS
  SCHEDULE = '0 2 * * *'
  DESCRIPTION = 'Daily sweep/GC + analyze on all shards'
  AS 'SWEEP ANALYZE';             -- Native sweep/GC command (VACUUM alias available)
```

### 12.2 Hourly Report Generation

```sql
CREATE JOB hourly_sales_report
  CLASS = LEADER_ONLY
  SCHEDULE = '0 * * * *'  -- Every hour at :00
  DESCRIPTION = 'Generate hourly sales report'
  AS 'CALL generate_hourly_sales_report()';
```

### 12.3 ETL Pipeline with Dependencies

```sql
-- Step 1: Extract
CREATE JOB etl_extract
  CLASS = LEADER_ONLY
  SCHEDULE = '0 1 * * *'
  AS 'CALL extract_source_data()';

-- Step 2: Transform (depends on extract)
CREATE JOB etl_transform
  CLASS = LEADER_ONLY
  DEPENDS ON etl_extract
  SCHEDULE = '0 2 * * *'
  AS 'CALL transform_extracted_data()';

-- Step 3: Load (depends on transform)
CREATE JOB etl_load
  CLASS = LEADER_ONLY
  DEPENDS ON etl_transform
  SCHEDULE = '0 3 * * *'
  AS 'CALL load_transformed_data()';
```

### 12.4 Customer-Specific Maintenance

```sql
CREATE JOB process_customer_reports
  CLASS = LOCAL_SAFE
  PARTITION BY SHARD_KEY(customer_id)
  SCHEDULE = '0 0 * * *'
  AS 'CALL process_customer_monthly_report(:customer_id)';
```

---

## 13. Performance Characteristics

### 13.1 Scheduling Overhead

- **Control Plane**: 1-5 ms per job to determine next run time
- **Agent Polling**: Default 10-second interval (configurable)
- **Job Dispatch**: < 10 ms to assign job to agent

### 13.2 Execution Throughput

| Configuration | Jobs/Hour |
|---------------|-----------|
| 10 nodes, 10 concurrent jobs per node | ~3,600,000 |
| 100 nodes, 10 concurrent jobs per node | ~36,000,000 |

**Assumptions**: 1-second average job duration, 10 concurrent jobs per node.

---

## 14. References

### 14.1 Internal References
- **SBCLUSTER-00**: Guiding Principles
- **SBCLUSTER-01**: Cluster Configuration Epoch
- **SBCLUSTER-02**: Membership and Identity
- **SBCLUSTER-05**: Sharding

### 14.2 External References
- **Cron Expression Format**: https://en.wikipedia.org/wiki/Cron
- **PostgreSQL pg_cron**: https://github.com/citusdata/pg_cron
- **Kubernetes CronJobs**: https://kubernetes.io/docs/concepts/workloads/controllers/cron-jobs/
- **Apache Airflow**: https://airflow.apache.org/

### 14.3 Research Papers
- *"Borg, Omega, and Kubernetes"* - Google, ACM Queue 2016

---

## 15. Revision History

| Version | Date       | Author       | Changes                                    |
|---------|------------|--------------|--------------------------------------------|
| 1.0     | 2026-01-02 | D. Calford   | Initial comprehensive specification        |

---

**Document Status**: DRAFT (Beta Specification Phase)
**Next Review**: Before Beta Implementation Phase
**Approval Required**: Chief Architect, Distributed Systems Lead, Application Services Lead
