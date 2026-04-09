# Beta 2 Serverless Autosuspend Autoscale And Warm Resume Model

## Purpose

Define the native service-tier model for autosuspend, resume, warm cache
retention, and bounded autoscale behavior.

## Governing rules

1. Suspend and resume are policy decisions, not silent process exits.
2. Protected sessions, queue backlogs, and replication duties may block suspend.
3. Resume must preserve catalog and MGA truth before accepting writes.
4. Warm-state retention is bounded and optional.

## Canonical metadata

- `sb_service_tier`
  - `tier_uuid`
  - `tier_name`
  - `compute_floor`
  - `compute_ceiling`
  - `suspend_policy`
  - `resume_policy`
- `sb_suspend_state`
  - `db_uuid`
  - `tier_uuid`
  - `state`
  - `last_transition_at`
  - `block_reason`
- `sb_warm_resume_capsule`
  - `capsule_uuid`
  - `db_uuid`
  - `cache_scope`
  - `expires_at`
  - `status`

## Suspend states

- `RUNNING`
- `QUIESCING`
- `SUSPENDED`
- `RESUMING`
- `FAILED_RESUME`

## Suspend flow

1. Idle policy threshold is reached.
2. Runtime checks block conditions.
3. If clear, runtime quiesces new work and emits suspend state.
4. Admitted warm-state capsules are persisted.
5. Instance enters `SUSPENDED`.

## Resume flow

1. Connection or policy trigger requests resume.
2. Runtime restores core state and validates catalog epochs.
3. Warm capsule is loaded if still valid.
4. Service returns to `RUNNING`.

## Refusal rules

- `SERVERLESS_SUSPEND_BLOCKED`
- `SERVERLESS_RESUME_INVALID`
- `SERVERLESS_WARM_STATE_EXPIRED`

## Metrics

- suspend count
- resume latency
- warm-hit rate
- blocked suspend count

## Cross-section requirements

- section `25` owns state machine and admission
- section `31` owns latency and availability certification
