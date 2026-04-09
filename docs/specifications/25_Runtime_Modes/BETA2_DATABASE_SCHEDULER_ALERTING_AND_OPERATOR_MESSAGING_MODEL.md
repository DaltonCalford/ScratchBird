# Beta 2 Database Scheduler Alerting And Operator Messaging Model

## Purpose

Define the native database-resident job scheduler, alert policy, retry model,
and operator messaging sinks used for maintenance, automation, and queue
activation support.

## Governing rules

1. Scheduled jobs are cataloged first-class objects.
2. Jobs run through the ordinary runtime scheduler and service classes.
3. Alert evaluation and message delivery are auditable.
4. Message sinks are pluggable and policy-gated.
5. Scheduler state may not be hidden only in listener or manager memory.

## Canonical metadata

- `sb_job`
  - `job_uuid`
  - `job_name`
  - `job_kind`
  - `schedule_text`
  - `service_class`
  - `payload_locator`
  - `retry_policy`
  - `enabled`
- `sb_job_run`
  - `run_uuid`
  - `job_uuid`
  - `scheduled_at`
  - `started_at`
  - `completed_at`
  - `status`
  - `failure_code`
  - `result_locator`
- `sb_alert_policy`
  - `alert_uuid`
  - `alert_name`
  - `metric_selector`
  - `threshold_rule`
  - `cooldown_policy`
  - `sink_uuid`
  - `enabled`
- `sb_message_sink`
  - `sink_uuid`
  - `sink_name`
  - `sink_kind`
  - `config_locator`
  - `enabled`

## Admitted job kinds

- `INTERNAL_PROCEDURE`
- `UDR_CALL`
- `QUEUE_ACTIVATION`
- `MAINTENANCE_ACTION`
- `REPLAY_OR_REHEARSAL_ACTION`

## Scheduler flow

1. Scheduler scans enabled jobs with schedule eligibility.
2. Eligible runs are materialized as `sb_job_run`.
3. Admission checks service class, dependencies, and concurrency policy.
4. Job runs publish status transitions:
   - `QUEUED`
   - `RUNNING`
   - `SUCCEEDED`
   - `FAILED`
   - `CANCELLED`
   - `QUARANTINED`
5. Retry policy controls re-enqueue behavior on admitted failure classes.

## Alert flow

1. Metric or event source is sampled.
2. Alert policy evaluates threshold or rule.
3. On trigger, one alert event is emitted.
4. Message sink delivery occurs through governed sink policy.
5. Cooldown prevents duplicate flood.

## Sink kinds

- `LOCAL_AUDIT`
- `QUEUE_PUBLICATION`
- `CONNECTOR_DELIVERY`
- `INSTALLER_OR_MANAGER_NOTICE`

No sink kind is mandatory. A deployment may operate with `LOCAL_AUDIT` only.

## Refusal rules

- `JOB_SCHEDULE_INVALID`
- `JOB_KIND_UNSUPPORTED`
- `JOB_CONCURRENCY_REFUSED`
- `ALERT_SINK_DISABLED`
- `ALERT_RULE_INVALID`

## Metrics

- jobs scheduled
- jobs running
- job success rate
- retry count
- quarantined job count
- alerts fired
- sink delivery failures

## Example

```sql
create job nightly_stats_refresh
schedule '0 2 * * *'
as call sb_maint.refresh_stats('sales');
create alert queue_backlog_high
when metric 'queue.visible_backlog' > 100000
send to local_audit_sink;
```

## Cross-section requirements

- section `25` owns scheduler runtime and admission
- section `20` owns operator messaging diagnostics
- section `31` owns scheduler correctness and retry certification
