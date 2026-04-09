# Cluster Clock Policy and Failure Detector Catalog Model

## Scope

This file defines the current code-backed catalog model for:

- clock policy
- clock source
- node clock state
- clock violation events
- failure detector policy
- node, role binding, and service rows that participate in heartbeat and health evaluation

This is the canonical persisted substrate for cluster heartbeat evaluation and skew policy. It is not a speculative future-only design.

## `failure_detector`

### Current code-backed row rules

The `failure_detector` catalog family currently enforces:

- `detector_id` is required.
- `cluster_id` is required and must resolve to a valid cluster row.
- `detector_kind` must be valid.
- `heartbeat_interval_ms` must be non-zero.
- `grace_startup_ms` must be non-zero.
- there can be at most one valid row for a given `(cluster_id, detector_kind)`.

Optional thresholds currently supported:

- `phi_threshold`
- `miss_threshold`
- `suspect_threshold`
- `fail_threshold`

### Canonical meaning

The persisted failure-detector row is policy, not event history. It defines the cluster's expected liveness evaluation contract.

`heartbeat_interval_ms` is authoritative timing configuration for expected heartbeat cadence.

`grace_startup_ms` is authoritative for startup suppression of failure classification.

## Clock policy

### Current code-backed row rules

The current clock-policy tests prove that a valid clock policy must satisfy an ordered threshold model. Policies with inconsistent skew thresholds are rejected.

Persisted clock policy currently carries:

- `policy_name`
- `warn_skew_ms`
- `soft_skew_ms`
- `hard_skew_ms`
- `max_jitter_ms`
- `sample_interval_ms`
- `stale_after_ms`
- `skew_guard_ms`
- `node_quarantine_on_hard_skew`

Duplicate `policy_name` rows are rejected.

### Canonical meaning

Clock policy is not advisory prose. It is a persisted control contract for:

- warning threshold
- soft skew threshold
- hard skew threshold
- sampling cadence
- staleness timeout
- quarantine policy

## Clock source

### Current code-backed row rules

Clock sources are persisted under a specific clock policy and currently include:

- `clock_source_id`
- `clock_policy_id`
- `source_kind`
- `endpoint`
- `priority_rank`
- `is_enabled`

The tests prove that duplicate `priority_rank` use within a policy is rejected.

### Meaning

Clock-source rows define source inventory and order, not current health verdict. Live health is derived from clock-state and violation-event material.

## Node catalog heartbeat substrate

### Current code-backed row rules

The node catalog already carries cluster-visible liveness material:

- `node_id`
- `cluster_id`
- `node_name`
- role
- host
- port
- transport
- state
- optional `last_heartbeat_time`

The unit tests prove:

- duplicate node names in the same cluster lane are rejected
- persisted `last_heartbeat_time` is legal and retrieved back through catalog accessors

### Role bindings and services

The current catalog also persists:

- node-role bindings
- node-service rows

Duplicate node-role bindings are rejected.

Duplicate service tuples are rejected.

These row families are the correct persisted home for the rebuilt manager heartbeat and remote control topology, not an auxiliary configuration file.

## Node clock state

### Current code-backed row rules

The `node_clock_state` family already persists:

- node
- clock policy
- clock state label
- offset
- jitter
- sample count
- last sync time
- last transition time
- logical counter

The unit tests prove that duplicate node-clock-state rows for the same node and policy are rejected.

### Meaning

This row family is the current durable source for cluster-visible skew posture. It is not derived only from logs or metrics output.

## Clock violation event

### Current code-backed row rules

The clock-violation family persists:

- violation event id
- node
- clock policy
- clock state
- offset
- jitter
- action taken
- event time

The unit tests prove that an event in `HEALTHY` state with no action is rejected as invalid violation material.

### Canonical meaning

Clock-violation rows are event history. They are distinct from steady-state clock policy and node clock state.

## Snapshot heartbeat observability

The current code-backed observability contract already exports:

- `sb_cluster_snapshot_heartbeats_total`

and the SQL observability row builder already materializes:

- `session_id`
- `db_uuid`
- `shard_id`
- `snapshot_boundary`
- `start_time`
- `last_heartbeat`

Canonical rule:

- heartbeat-backed cluster or snapshot freshness must be visible through both metrics and SQL-introspection surfaces
- observability is derivative evidence of persisted or runtime health state, not a substitute for catalog truth

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- persisted failure-detector policy
- persisted clock policy
- persisted clock source inventory
- persisted node heartbeat timestamps
- persisted node clock state
- persisted violation events
- observability exposure of snapshot heartbeat activity

### Required reconstructed behavior

The rebuilt cluster layer must preserve this split:

- policy rows define expected cadence and thresholds
- heartbeat events update freshness state
- clock violation rows record notable threshold crossings and actions
- manager heartbeat or cluster agent heartbeat must write into these persisted families or directly adjacent canonical families

### Drift rule

No reconstructed manager or cluster heartbeat design may invent a parallel liveness truth source that bypasses:

- failure detector policy
- node heartbeat timestamps
- clock state
- clock violation event history

## MGA boundary

All of these cluster-health and heartbeat rows are ordinary MGA-governed database state.

They are:

- transaction-scoped
- visible according to MGA rules
- recovered as database state, not from replay of an external log

This section is therefore subordinate to the MGA truth model in sections `08`, `35`, and `42`.
