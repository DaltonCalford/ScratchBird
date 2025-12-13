# TODO: Task Scheduler (Local/Cluster-Controlled)

Goal: Add a scheduler to launch tasks/processes/procedures at configured times/intervals, controllable locally or via cluster coordination.

## Requirements
- Scheduling: cron-like expressions and simple intervals/delays; start/stop/pause; time zone awareness.
- Targets: stored procedures/functions or external hooks (as allowed), with arguments; execute within a transaction context.
- Scoping: local-only mode and cluster-coordinated mode (single active runner in cluster; failover/rebalance).
- Control: enable/disable per job; on-demand trigger; per-job concurrency limits and retry/backoff.
- Visibility: catalog of jobs, next/last run, status, error logs, runtime stats.
- Security: only privileged roles manage jobs; execution under specified role/user.
- Isolation: ensure transactional execution and cleanup; configurable commit/rollback behavior on failure.

## Work Items
- Catalog schema for jobs, schedules, args, ownership, status, and history.
- Scheduler service (thread) with pluggable timing engine; cluster-mode leader election/lease mechanism.
- Executor integration to run stored code under specified role and transaction settings.
- Configuration to switch between local-only and cluster-coordinated scheduling.
- APIs/commands to create/update/delete/pause/resume/trigger jobs; list status/history.
- Tests: scheduling accuracy, pause/resume, retries, failure handling, cluster failover (when cluster added).
- Monitoring: metrics for runs, failures, durations; optional logging per job.

## Constraints
- ScratchBird feature; emulated dialects unaffected unless explicitly exposed via admin interfaces.
- Keep runtime overhead low; allow disabling scheduler entirely.
