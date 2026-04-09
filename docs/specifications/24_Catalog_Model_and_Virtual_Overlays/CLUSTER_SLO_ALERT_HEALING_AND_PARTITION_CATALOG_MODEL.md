# Cluster SLO, Alert, Healing, and Partition Catalog Model

## Scope

This file defines the current code-backed catalog substrate for:

- SLO profiles and bindings
- SLO measurement windows
- SLO burn events
- alert rules, targets, routes, events, acknowledgements, and silences
- network partition events and members
- healing policies, actions, parameters, runs, and steps
- autoscale policies tied to burn and queue thresholds

This is current persisted authority, not a target-state-only cluster design.

## SLO profile

### Current row rules

The current code enforces that an SLO profile must have:

- `slo_profile_id`
- `profile_name`
- valid node `role`
- non-zero latency targets
- `latency_p95_target_ms <= latency_p99_target_ms`
- non-zero window sizes
- `short_burn_window_minutes <= long_burn_window_minutes`
- monotone burn thresholds:
  - `moderate <= high <= critical`

Duplicate `profile_name` is rejected.

### Persisted meaning

The profile currently persists:

- availability target
- latency targets
- error-rate target
- short and long burn windows
- moderate, high, and critical burn thresholds
- active flag
- version

Canonical rule:

- SLO posture is persisted policy, not only metrics-server configuration

## SLO bindings

### Current row rules

An SLO binding requires:

- `slo_binding_id`
- `slo_profile_id`
- valid node `role`
- `effective_from_time`

Optional:

- `node_id`
- `effective_to_time`

The current code rejects:

- missing referenced SLO profile
- missing referenced node when `node_id` is present
- `effective_to_time <= effective_from_time`
- duplicate binding for same `(profile, role, effective_from_time, node)`

### Meaning

SLO profiles can apply:

- globally by role
- specifically to one node

This is a real current substrate for role-specific and node-specific reliability policy.

## SLO windows

### Current row rules

An SLO window requires:

- `slo_window_id`
- `node_id`
- valid node `role`
- `window_start_time`
- `window_end_time > window_start_time`

The code rejects windows where:

- `success_count + error_count > request_count`

### Persisted meaning

The current window family already stores:

- request count
- success count
- error count
- `latency_p95_ms`
- `latency_p99_ms`
- availability SLI
- error-rate SLI

This is the persisted measurement substrate for later burn evaluation and autoscale decisions.

## SLO burn events

### Current row rules

The current code requires:

- `slo_burn_event_id`
- `node_id`
- `slo_profile_id`
- valid node `role`
- valid burn severity
- valid action plan
- `event_time`

Optional:

- `resolved_time`, which must be greater than or equal to `event_time`

### Persisted meaning

The current burn-event row persists:

- short burn rate
- long burn rate
- burn severity
- selected action plan
- event and optional resolution times

Canonical rule:

- burn-rate escalation is persisted reliability state, not just ephemeral alert text

## Alerting substrate

### Alert rules

The current tests prove:

- a rule may not carry both condition text and condition SBLR UUID at the same time for the same contract lane
- invalid mixed condition state is rejected

Current persisted rule shape includes:

- `rule_id`
- `rule_name`
- `rule_kind`
- `severity`
- bounded condition representation
- throttle interval

### Alert targets

Current persisted targets include at least:

- `target_id`
- `target_name`
- `target_kind`
- endpoint

### Alert routes

Current persisted routes include:

- `route_id`
- `rule_id`
- `target_id`
- route kind
- severity band

### Alert events

Current persisted events include:

- `event_id`
- `rule_id`
- severity
- event state
- event time

### Alert acknowledgements

Current persisted acknowledgements include:

- `ack_id`
- `event_id`
- `user_id`
- `ack_time`
- optional comment

### Alert silences

Current persisted silences include:

- `silence_id`
- scope kind
- optional scope UUID
- start and end time
- creator identity
- optional reason

Canonical rule:

- alerts are not log-only
- acknowledgement and silence state are persisted control-plane truth

## Network partition substrate

### Partition events

The current code-backed row contract requires:

- `partition_id`
- `cluster_id`
- `local_node_id`
- valid `partition_state`
- `opened_time`

Optional:

- `resolved_time >= opened_time`
- `description`

Persisted row fields include:

- `quorum_reachable`
- optional toast-backed description

### Partition members

The current tests prove persisted partition-member rows exist and bind:

- member
- partition
- node
- side id
- reachable flag

Canonical rule:

- network split state is persisted cluster state
- quorum reachability is first-class and durable enough for healing and audit

## Healing substrate

### Healing policy

Current persisted healing policy includes:

- `policy_id`
- `policy_name`
- trigger kind
- minimum severity

### Healing action

Current persisted healing action includes:

- `action_id`
- `policy_id`
- action kind
- action order
- max retries
- cooldown

### Healing action parameters

The current tests prove:

- parameter type discipline is enforced
- invalid type/value combinations are rejected

### Healing runs and steps

Current persisted run and step rows include:

- run id
- policy id
- optional trigger event id
- run state
- start timing
- step id
- action id
- step index
- step state
- timing

Canonical rule:

- healing is represented as persisted policy plus persisted execution history, not a transient-only job queue

## Autoscale policy

The current code-backed autoscale substrate already includes:

- role-scoped policy
- min and max nodes
- scale out and scale in step
- cooldowns
- CPU and queue thresholds
- SLO burn scale-out threshold
- SLO recovery scale-in threshold

Canonical rule:

- autoscale policy is already tied to SLO burn and queue pressure at the catalog level

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- persisted SLO profile and burn substrate
- persisted alerting substrate
- persisted partition and member substrate
- persisted healing substrate
- persisted autoscale substrate

### Required reconstructed behavior

The rebuilt manager and cluster-control lane must use these persisted families for:

- operator inspection
- queued healing
- policy rollout
- drift detection
- burn-driven autoscale decisions

### Drift rule

No rebuilt control-plane or external tool may bypass these persisted rows with a competing transient-only source of truth for:

- alert acknowledgement
- silence state
- healing run state
- partition event state
- burn-event state
