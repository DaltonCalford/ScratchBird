# Prometheus Metrics Reference (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the required Prometheus metrics emitted by ScratchBird for operational
monitoring. These metrics are for external observability; SQL-visible metrics
are defined in `MONITORING_SQL_VIEWS.md`.

## Global Rules

- All metrics use the `scratchbird_` prefix.
- Metrics MUST be emitted in Prometheus text exposition format.
- Labels are fixed and MUST NOT change without a major version bump.
- Units are encoded in metric names when required (e.g., `_seconds`).

## Required Metrics

### Connections

- `scratchbird_connections_total{state}` (gauge)
  - state: active | idle | idle_in_transaction | waiting
- `scratchbird_connections_established_total{protocol}` (counter)
- `scratchbird_connections_closed_total{reason}` (counter)
  - reason: normal | timeout | error | admin_kill
- `scratchbird_connections_rejected_total{reason}` (counter)
  - reason: max_connections | auth_failed | ssl_required | database_not_found | protocol_unsupported
- `scratchbird_connection_duration_seconds` (histogram)
- `scratchbird_connection_setup_seconds{protocol}` (histogram)

### Queries

- `scratchbird_queries_total{type}` (counter)
  - type: select | insert | update | delete | ddl | other
- `scratchbird_queries_failed_total{error_class}` (counter)
  - error_class: syntax | permission | constraint | timeout | deadlock | serialization | other
- `scratchbird_query_duration_seconds{type}` (histogram)
- `scratchbird_query_rows_returned` (histogram)
- `scratchbird_query_rows_affected{type}` (histogram)
- `scratchbird_slow_queries_total{threshold}` (counter)
- `scratchbird_query_currently_running` (gauge)

### Transactions

- `scratchbird_transactions_total{status}` (counter)
  - status: committed | rolled_back | aborted
- `scratchbird_transactions_active` (gauge)
- `scratchbird_transactions_idle_in_transaction` (gauge)
- `scratchbird_transaction_duration_seconds` (histogram)
- `scratchbird_transaction_conflicts_total{type}` (counter)
  - type: deadlock | serialization | timeout

### Locks

- `scratchbird_lock_waits_total` (counter)
- `scratchbird_deadlocks_total` (counter)
- `scratchbird_lock_wait_seconds` (histogram)

### Storage

- `scratchbird_buffer_pool_hit_ratio` (gauge)
- `scratchbird_buffer_pool_reads_total{source}` (counter)
  - source: cache | disk
- `scratchbird_buffer_pool_writes_total` (counter)
- `scratchbird_page_reads_total` (counter)
- `scratchbird_page_writes_total` (counter)

### COPY

- `scratchbird_copy_rows_total{direction}` (counter)
- `scratchbird_copy_bytes_total{direction}` (counter)
- `scratchbird_copy_errors_total` (counter)

### Uptime

- `scratchbird_uptime_seconds` (gauge)

## Related Specs

- `docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md`
- `docs/specifications/parser/v3/operations/LISTENER_POOL_METRICS.md`
