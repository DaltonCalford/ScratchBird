# Listener and Parser Pool Metrics (Authoritative)

Version: 1.1
Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define Prometheus metrics for network listeners and parser pools. These metrics
are required for acceptance, backpressure visibility, and parser lifecycle
monitoring.

## Scope

- Listener accept/reject and queue metrics
- Parser pool size, spawn, recycle, and health
- Per-protocol labeling

## Naming Conventions

- Prefix: `scratchbird_`
- Units: seconds for durations, bytes for sizes
- Labels: `protocol`, `listener`, `reason`, `pool`

## Listener Metrics

### Counters

- `scratchbird_listener_connections_total{protocol,listener}`
- `scratchbird_listener_accept_total{protocol,listener}`
- `scratchbird_listener_reject_total{protocol,listener,reason}`
  - reason: `max_connections` | `queue_full` | `auth_required` | `protocol_unsupported` | `timeout` | `error`

### Gauges

- `scratchbird_listener_open_connections{protocol,listener}`
- `scratchbird_listener_queue_depth{protocol,listener}`

### Histograms

- `scratchbird_listener_handoff_seconds{protocol,listener}`
  - measured from accept to `HANDOFF_ACK`
- `scratchbird_listener_queue_wait_seconds{protocol,listener}`
  - measured from accept to worker assignment

## Parser Pool Metrics

### Counters

- `scratchbird_parser_spawn_total{protocol,pool}`
- `scratchbird_parser_recycle_total{protocol,pool,reason}`
  - reason: `max_requests` | `max_age` | `error` | `manual`
- `scratchbird_parser_errors_total{protocol,pool,category}`

### Gauges

- `scratchbird_parser_pool_size{protocol,pool}`
- `scratchbird_parser_pool_idle{protocol,pool}`
- `scratchbird_parser_pool_busy{protocol,pool}`

### Histograms

- `scratchbird_parser_session_seconds{protocol,pool}`
- `scratchbird_parser_healthcheck_seconds{protocol,pool}`

## Export Requirements

- Metrics MUST be emitted by listener and pool manager.
- Parser errors MUST increment `scratchbird_parser_errors_total` with a
  stable `category` label.
- Handoff latency MUST be measured from accept to `HANDOFF_ACK`.

## Related Specs

- `docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md`
- `docs/specifications/parser/v3/network/CONTROL_PLANE_PROTOCOL_SPEC.md`
