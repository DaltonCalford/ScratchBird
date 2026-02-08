# ScratchBird SQL Monitoring Views (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

## Purpose

Define the SQL-visible monitoring views used by native ScratchBird tools and by
emulated dialects that must surface monitoring data. These views are read-only
and expose live runtime state (sessions, transactions, locks, statements, and
performance counters).

Prometheus metrics are defined separately in `PROMETHEUS_METRICS_REFERENCE.md`.

## Scope

In scope:
- Session, statement, transaction, and lock visibility
- Performance counters suitable for SQL inspection
- Table statistics and I/O counters for dialect parity

Out of scope:
- Historical time-series retention (Prometheus only)
- Cluster-wide aggregation (rejected in V3)
- Audit/event log storage (security specs)

## Access Control (Authoritative)

- Superuser can see all rows.
- Non-superuser can only see rows for their own session and owned transactions.
- Emulated dialect views MUST apply the same row filters as the underlying `sys.*`
  views; no extra rows may be exposed.

## View Definitions

### 1) sys.sessions

| Column | Type | Description |
| --- | --- | --- |
| session_id | UUID | Unique session identifier (UUID v7) |
| connection_id | BIGINT | Monotonic connection id |
| user_name | TEXT | Authenticated user name |
| role_name | TEXT | Effective role for the session |
| database_name | TEXT | Database name |
| protocol | TEXT | scratchbird / postgresql / mysql / firebird |
| client_addr | TEXT | Client IP or host |
| client_port | INT | Client port |
| state | TEXT | idle / active / idle_in_txn / waiting |
| connected_at | TIMESTAMPTZ | Session start time |
| last_activity_at | TIMESTAMPTZ | Last activity timestamp |
| transaction_id | BIGINT | Current transaction id (nullable) |
| statement_id | BIGINT | Current statement id (nullable) |
| current_query | TEXT | Current SQL text (nullable) |
| wait_event | TEXT | Wait event name (nullable) |
| wait_resource | TEXT | Wait resource (nullable) |

### 2) sys.transactions

| Column | Type | Description |
| --- | --- | --- |
| transaction_id | BIGINT | Transaction id |
| transaction_uuid | UUID | Transaction UUID (UUID v7) |
| session_id | UUID | Owning session id |
| state | TEXT | active / committed / rolledback / waiting |
| isolation_level | TEXT | read_committed / repeatable_read / serializable |
| read_only | BOOLEAN | Read-only flag |
| start_time | TIMESTAMPTZ | Transaction start time |
| duration_ms | BIGINT | Elapsed time in ms |
| current_query | TEXT | Current SQL text (nullable) |
| wait_event | TEXT | Wait event name (nullable) |
| locks_held | INT | Locks held by this transaction |
| pages_modified | INT | Pages modified in this transaction |
| distributed | BOOLEAN | Distributed transaction flag |
| coordinator_uuid | UUID | Coordinator node (nullable) |

### 3) sys.locks

| Column | Type | Description |
| --- | --- | --- |
| lock_id | BIGINT | Unique lock id |
| lock_type | TEXT | Object type (table/page/tuple/etc.) |
| lock_mode | TEXT | Lock mode (shared/exclusive/etc.) |
| granted | BOOLEAN | True if granted |
| lock_state | TEXT | granted / waiting |
| database_uuid | UUID | Database UUID |
| relation_uuid | UUID | Relation UUID (nullable) |
| relation_name | TEXT | Relation name (nullable) |
| page | BIGINT | Page number (nullable) |
| tuple | BIGINT | Tuple/row id (nullable) |
| transaction_id | BIGINT | Owning transaction id |
| session_id | UUID | Owning session id |
| virtual_xid | TEXT | Virtual transaction id (nullable) |
| grant_time | TIMESTAMPTZ | When lock granted (nullable) |
| wait_start | TIMESTAMPTZ | When wait started (nullable) |

### 4) sys.statements

| Column | Type | Description |
| --- | --- | --- |
| statement_id | BIGINT | Statement id |
| session_id | UUID | Owning session id |
| transaction_id | BIGINT | Owning transaction id |
| state | TEXT | running / waiting / idle |
| sql_text | TEXT | SQL text |
| start_time | TIMESTAMPTZ | Statement start |
| elapsed_ms | BIGINT | Elapsed time in ms |
| rows_processed | BIGINT | Rows processed so far |
| wait_event | TEXT | Wait event name (nullable) |
| wait_resource | TEXT | Wait resource (nullable) |

### 5) sys.performance

| Column | Type | Description |
| --- | --- | --- |
| metric | TEXT | Metric name (snake_case) |
| value | DOUBLE | Metric value |
| unit | TEXT | count / ms / bytes / ratio / percent |
| scope | TEXT | engine / database |
| database_name | TEXT | Database name (nullable) |
| updated_at | TIMESTAMPTZ | Last update time |

Required minimum metrics:
- connections_active
- transactions_active
- transactions_committed_total
- transactions_rolled_back_total
- query_latency_avg_ms
- lock_waits_total
- deadlocks_total
- buffer_pool_hit_ratio
- cache_hits_total
- cache_misses_total

Extended metrics required for emulation views:
- connections_total
- queries_total{type=select|insert|update|delete|ddl|other}
- query_rows_returned_total
- query_rows_affected_total{type=insert|update|delete}
- buffer_pool_reads_total{source=cache|disk}
- copy_rows_total{direction=from|to}
- copy_bytes_total{direction=from|to}
- copy_errors_total
- uptime_seconds

## Related Specs

- `docs/specifications/parser/v3/operations/MONITORING_DIALECT_MAPPINGS.md`
- `docs/specifications/parser/v3/operations/PROMETHEUS_METRICS_REFERENCE.md`
