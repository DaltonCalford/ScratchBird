# Listener and Parser Pool Metrics

Version: 1.0
Status: Draft (Alpha IP layer)
Last Updated: January 2026

## Purpose

Define Prometheus metrics for network listeners and parser pools.
These metrics provide visibility into accept rates, queueing,
parser churn, and handoff latency.

## Scope

- Listener accept/reject and queue metrics
- Parser pool size, spawn, recycle, and health
- Per-protocol labeling

## Naming Conventions

- Prefix: scratchbird_
- Units: seconds for durations, bytes for sizes
- Labels: protocol, listener, reason, database (where applicable)

## Listener Metrics

### Counters

- scratchbird_listener_connections_total{protocol,listener}
- scratchbird_listener_accept_total{protocol,listener}
- scratchbird_listener_reject_total{protocol,listener,reason}
  - reason: max_connections | queue_full | auth_required | error

### Gauges

- scratchbird_listener_open_connections{protocol,listener}
- scratchbird_listener_queue_depth{protocol,listener}

### Histograms

- scratchbird_listener_handoff_seconds{protocol,listener}
- scratchbird_listener_queue_wait_seconds{protocol,listener}

## Parser Pool Metrics

### Counters

- scratchbird_parser_spawn_total{protocol,pool}
- scratchbird_parser_recycle_total{protocol,pool,reason}
  - reason: max_requests | max_age | error | manual
- scratchbird_parser_errors_total{protocol,pool,category}

### Gauges

- scratchbird_parser_pool_size{protocol,pool}
- scratchbird_parser_pool_idle{protocol,pool}
- scratchbird_parser_pool_busy{protocol,pool}

### Histograms

- scratchbird_parser_session_seconds{protocol,pool}
- scratchbird_parser_healthcheck_seconds{protocol,pool}

## Export Requirements

- Metrics must be emitted by listener and pool manager.
- Parser errors should increment parser_errors_total.
- Handoff latency measured from accept -> HANDOFF_ACK.

## Related Specs

- docs/specifications/operations/PROMETHEUS_METRICS_REFERENCE.md
- docs/specifications/network/CONTROL_PLANE_PROTOCOL_SPEC.md
