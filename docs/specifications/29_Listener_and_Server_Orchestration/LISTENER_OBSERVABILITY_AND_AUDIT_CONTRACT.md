# Listener Observability and Audit Contract

## Current metrics

The shipped listener registers these metrics:

### Listener edge
- `scratchbird_listener_connections_total`
- `scratchbird_listener_accept_total`
- `scratchbird_listener_reject_total`
- `scratchbird_listener_open_connections`
- `scratchbird_listener_queue_depth`
- `scratchbird_listener_handoff_seconds`
- `scratchbird_listener_queue_wait_seconds`

### Parser pool
- `scratchbird_parser_spawn_total`
- `scratchbird_parser_recycle_total`
- `scratchbird_parser_errors_total`
- `scratchbird_parser_pool_size`
- `scratchbird_parser_pool_idle`
- `scratchbird_parser_pool_busy`
- `scratchbird_parser_session_seconds`
- `scratchbird_parser_healthcheck_seconds`

## Metric labels

Current metric labels include combinations of:
- `protocol`
- `listener`
- `reason`
- `pool`
- `category`

## Required rejection reasons

Current shipped rejection accounting includes:
- `draining`
- `queue_full`
- `error`

## Current status visibility

The management `STATUS` command must expose:
- `draining`
- `owner_database`
- `active_sessions`
- `warm_workers`
- `pool_min`
- `pool_max`

When the listener owns or proxies an engine with derivative-lane diagnostics,
the management status surface must also expose summary values for:
- derivative backpressure class
- derivative queue profile count
- shadow-group state
- shadow-group readiness counts
- last restore boundary identifier when present
- last failback boundary identifier when present

## Managed-mode audit events

Managed-mode validation paths emit audit lines through the managed audit event
path.

Current event family:
- `MANAGED_PREFACE_DECISION`

Audit payload may include:
- event name
- success flag
- reason
- `dbbt_id` when available

If a management request surfaces derivative-lane or shadow-group degradation,
the emitted audit or diagnostic event must preserve:
- degradation class
- affected database identity
- observed backpressure class when present
- shadow-group state when present

## Debug surfaces

Current listener and engine IPC code also emits debug diagnostics to stderr for:
- worker registration
- control-plane hello
- attach begin and attach completion
- catalog attach steps

These debug lines are diagnostic aids, not stability contracts.

## Hard boundaries

- Current observability authority is local listener metrics, management status,
  and managed audit event emission.
- No append-only cross-host listener audit store is part of current authority.
- No universal migration or mirror observability model is part of current
  authority.
- Listener-side derivative and shadow-group visibility is a summary projection
  over engine truth, not a separate source of operational authority.
