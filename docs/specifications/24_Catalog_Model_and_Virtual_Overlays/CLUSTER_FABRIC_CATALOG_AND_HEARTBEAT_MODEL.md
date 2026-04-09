# Cluster Fabric Catalog and Heartbeat Model

## Scope

This file defines the current code-backed catalog substrate for server-local manager coordination, future cluster-fabric work, remote execution bookkeeping, and heartbeat-backed control-plane state.

This file is authoritative for:

- `cluster_fabric_link`
- `cluster_fabric_session`
- `cluster_fabric_txn`
- `cluster_fabric_task`
- `cluster_fabric_task_chunk`
- `cluster_fabric_event`
- `cluster_fabric_error`

This file does not claim that the full remote management queue executor is already complete in runtime code. It defines:

- current code-backed persisted catalog truth
- required reconstructed behavior that must be preserved while the cluster lane is rebuilt

## Current code-backed authority

The catalog already persists cluster-fabric runtime rows. The root catalog map allocates fixed storage pages for the seven cluster-fabric table families, and `CatalogManager` already enforces row admission, referential integrity, uniqueness, state validation, and redaction rules.

The current code-backed implementation proves that ScratchBird already carries a persisted substrate for:

- remote links
- remote sessions
- remote transaction identity
- fabric task tracking
- chunked task transfer bookkeeping
- fabric events
- fabric errors

This substrate is not speculative. It is durable catalog state inside the database and survives ordinary restart like other catalog families.

## `cluster_fabric_link`

### Required row identity

Each link row is identified by `cluster_fabric_link_id`.

### Required fields

The current code-backed row contract requires at minimum:

- `cluster_fabric_link_id`
- `link_name`
- `scope_kind`
- `remote_node_id`
- `transport_kind`
- `link_state`
- `mode_version`
- `created_by_id`

The row may also carry:

- `remote_server_id`
- `auth_profile_id`
- `priority_rank`
- `max_sessions`
- `max_tasks`
- `heartbeat_interval_ms`
- `miss_threshold`
- `fail_threshold`
- `last_heartbeat_time`
- `last_ready_time`

### Admission rules

The current code enforces all of the following:

- `link_name` must be non-empty.
- `remote_node_id` must resolve to a valid node row.
- `created_by_id` must resolve to a valid user row.
- `remote_server_id`, when present, must resolve to a valid server-registry row.
- `auth_profile_id`, when present, must resolve to a valid cluster-policy row.
- `scope_kind`, `link_state`, and `transport_kind` must be valid enum members.
- `mode_version` must be non-zero.
- `fail_threshold` must be greater than or equal to `miss_threshold`.
- `last_heartbeat_time`, when present, must be non-zero.
- `last_ready_time`, when present, must be non-zero.

### Uniqueness and concurrency rules

The current code-backed row rules are:

- `link_name` is unique by canonical folded identifier, not by raw byte spelling.
- update-in-place is versioned by `mode_version`
- if a link row already exists, the next accepted update must use `existing.mode_version + 1`
- stale updates are rejected with `CONSTRAINT_VIOLATION`

This is the current authoritative anti-drift rule for manager-side or cluster-side link reconfiguration.

### State meaning

`link_state` is persisted control-plane state, not a derived log-only annotation. The row can transition through states such as `INIT` and `READY`, and readiness timestamps are separately materialized.

`last_heartbeat_time` records the latest accepted heartbeat evidence for the link.

`last_ready_time` records the latest time the link achieved ready state.

## `cluster_fabric_session`

### Purpose

This row family persists the effective identity and lifecycle of a remote or fabric-mediated session attached to a link.

### Admission rules

The current code-backed row rules are:

- `cluster_fabric_session_id` is required.
- `cluster_fabric_link_id` must resolve to a valid link row.
- `session_id` is required.
- `effective_user_id` must resolve to a valid user row.
- `effective_schema_id` must resolve to a valid schema row.
- `effective_role_id`, when present, must resolve to a valid role row.
- `effective_group_id`, when present, must resolve to a valid group row.
- `search_path_profile_id`, when present, must resolve to a valid search-path-profile row.
- `opened_time` is required and non-zero.
- terminal session states require `closed_time`.
- `closed_time`, when present, must be greater than or equal to `opened_time`.
- `last_activity_time`, when present, must be greater than or equal to `opened_time`.
- the tuple `(cluster_fabric_link_id, session_id)` is unique among valid rows.

### Persisted effective identity

The persisted row currently carries:

- effective user
- optional effective role
- optional effective group
- effective schema
- optional search path profile

That means the cluster-fabric substrate already has persisted identity context for future remote-management and passthrough execution lanes.

## `cluster_fabric_txn`

### Purpose

This row family persists fabric transaction identity bound to a cluster-fabric session.

### Current authority

The current code-backed row family proves that the runtime already distinguishes:

- fabric session identity
- fabric transaction identity
- transaction state
- begin and terminal timing

This is aligned with ScratchBird's MGA transaction model. The fabric transaction row tracks remote or fabric execution bookkeeping. It does not replace the local MGA truth source for committed visibility.

## `cluster_fabric_task`

### Purpose

This row family persists unit-of-work tracking for fabric tasks, including passthrough SBLR work.

### Admission rules

The current code-backed row family enforces:

- `cluster_fabric_task_id` is required.
- `cluster_fabric_link_id` must resolve to a valid link row.
- `cluster_fabric_session_id`, when present, must resolve to a valid session row under the same link.
- `cluster_fabric_txn_id`, when present, must resolve to a valid fabric transaction row.
- if both session and transaction are present, the transaction must belong to that session.
- `last_error_id`, when present, must resolve to a valid cluster-fabric error row for the same link.

For `PASSTHROUGH_SBLR_EXECUTE`, the current tests prove that missing required session context is rejected.

### Persisted task shape

The current row family already supports:

- task kind
- task state
- priority
- SBLR artifact identity
- source object identity
- target object identity
- last error identity
- submitted, started, and finished timing

This is enough substrate to support a reconstructed remote-management instruction queue without inventing a parallel persistence model.

## `cluster_fabric_task_chunk`

### Purpose

This row family persists chunked transfer bookkeeping for a fabric task.

### Admission rules

The current code-backed row rules include:

- `cluster_fabric_task_id` must resolve to a valid task row.
- `chunk_seq`
- `chunk_total`
- `chunk_bytes`
- `chunk_checksum`
- `sent_time`
- `is_final_chunk`

The unit tests prove that inconsistent chunk state is rejected. A final chunk with invalid ordering is not accepted.

### Meaning

This row family is authoritative for durable chunk bookkeeping. It is not a transport stream by itself. It records progress and integrity evidence for task transfer.

## `cluster_fabric_event`

### Purpose

This row family persists auditable task or session events tied to a link, and optionally to a session or task.

### Current row shape

The current code-backed family already persists:

- `event_kind`
- `event_time`
- optional event payload
- optional actor identity
- optional session identity
- optional task identity

This is the correct persisted home for future reconstructed remote-management lifecycle events.

## `cluster_fabric_error`

### Purpose

This row family persists fabric error state with sanitization.

### Current code-backed behavior

The unit tests prove:

- errors are persisted under a specific link
- the stored message is sanitized on retrieval
- sensitive tokens are redacted
- endpoint-like material is redacted

So the canonical rule is:

- operator-visible fabric errors must be safe to surface
- raw task tokens or remote endpoint secrets are not canonical user-facing error payload

## Heartbeat semantics

### Current code-backed truth

Heartbeat state is currently persisted at least in:

- cluster-fabric link rows via `last_heartbeat_time`
- node and remote-connector families via last-heartbeat fields
- snapshot observability rows via `last_heartbeat`

That means heartbeat is already a first-class persisted signal, not a transient-only implementation detail.

### Canonical meaning

Heartbeat fields indicate freshness of remote or cluster-visible liveness evidence. They do not by themselves authorize durable state publication, MGA visibility changes, or remote write commitment.

## Current code-backed versus reconstructed-required behavior

### Current code-backed

The current code proves:

- persisted catalog families for link, session, transaction, task, chunk, event, and error
- integrity checks and referential admission rules
- redacted error retrieval
- heartbeat-backed link freshness fields
- enough persisted shape to support manager and cluster rebuilding work

### Required reconstructed behavior

The full remote management plane must be rebuilt on top of this substrate so that:

- remote instructions are represented as stable queued work, not ad hoc controller state
- assessment, dispatch, apply, refuse, quarantine, and acknowledge events all persist through this catalog family or a directly adjacent instruction family
- manager heartbeat and server-agent coordination update the persisted substrate in deterministic state transitions

### Drift rule

Until the cluster queue executor is fully rebuilt, no new specification may bypass these persisted catalog families with a competing control ledger.

## Non-authority boundary

This file does not claim:

- cross-node consensus is complete
- remote instruction queue execution is fully shipped
- cluster-wide heartbeat arbitration is fully implemented

Those lanes remain partly reconstructed. This file defines the persisted substrate they must use.
