# ScratchBird SQL Monitoring Views

Version: 1.0  
Status: Draft (Alpha)  
Last Updated: January 2026

## Purpose

Define the SQL-visible monitoring views used by native ScratchBird tools (e.g. ScratchRobin)
and by emulated dialects that must surface monitoring data. These views are read-only and
expose live runtime state (sessions, transactions, locks, statements, and performance counters).

This spec is separate from Prometheus metrics. Prometheus is for external observability,
while these views are for interactive SQL inspection.

## Scope

In scope:
- Session and statement visibility
- Transaction and lock inspection
- Performance counters suitable for UI dashboards
- Table statistics and I/O counters for dialect parity

Out of scope:
- Historical time-series retention (Prometheus handles this)
- Cluster-wide aggregation (Beta)
- Audit/event log storage (see Security specs)

## Schema and Access Control

All monitoring views live in the `sys` schema and are read-only.  
Default visibility rules:
- Superuser can see all rows.
- Non-superuser can only see rows for their own session and owned transactions.
- Role membership does not automatically grant visibility unless explicitly granted.

Implementations may add a `MONITOR` privilege in the future; for Alpha, treat
superuser-only access as the default.

## View Definitions

### 1) sys.sessions

Current client sessions and connection state.

| Column | Type | Description |
| --- | --- | --- |
| session_id | UUID | Unique session identifier |
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

Notes:
- `current_query` should be redacted if policy requires.
- `state=idle_in_txn` indicates an open transaction with no active statement.

### 2) sys.transactions

Active and recent transactions (implementation may include active only).

| Column | Type | Description |
| --- | --- | --- |
| transaction_id | BIGINT | Transaction id |
| transaction_uuid | UUID | Transaction UUID |
| session_id | UUID | Owning session id |
| state | TEXT | active / committed / rolledback / waiting |
| isolation_level | TEXT | read_committed / repeatable_read / serializable |
| read_only | BOOLEAN | Read-only flag |
| start_time | TIMESTAMPTZ | Transaction start time |
| duration_ms | BIGINT | Elapsed time in milliseconds |
| current_query | TEXT | Current SQL text (nullable) |
| wait_event | TEXT | Wait event name (nullable) |
| locks_held | INT | Locks held by this transaction |
| pages_modified | INT | Pages modified in this transaction |
| distributed | BOOLEAN | Distributed transaction flag |
| coordinator_uuid | UUID | Coordinator node (nullable) |

Source mapping:
- `TransactionViewEntry` in `TRANSACTION_MAIN.md` is the primary source.

### 3) sys.locks

Current granted and waiting locks.

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

Source mapping:
- `LockViewEntry` in `TRANSACTION_LOCK_MANAGER.md` is the primary source.

### 4) sys.statements

Active statements (optionally include recently finished statements).

| Column | Type | Description |
| --- | --- | --- |
| statement_id | BIGINT | Statement id |
| session_id | UUID | Owning session id |
| transaction_id | BIGINT | Owning transaction id |
| state | TEXT | running / waiting / idle |
| sql_text | TEXT | SQL text |
| start_time | TIMESTAMPTZ | Statement start |
| elapsed_ms | BIGINT | Elapsed time in milliseconds |
| rows_processed | BIGINT | Rows processed so far |
| wait_event | TEXT | Wait event name (nullable) |
| wait_resource | TEXT | Wait resource (nullable) |

Long-running statements are derived:
```
SELECT * FROM sys.statements
WHERE elapsed_ms > 5000
ORDER BY elapsed_ms DESC;
```

### 5) sys.performance

Aggregated performance counters, exposed as key/value rows.

| Column | Type | Description |
| --- | --- | --- |
| metric | TEXT | Metric name (snake_case) |
| value | DOUBLE | Metric value |
| unit | TEXT | Unit (count, ms, bytes, ratio, percent) |
| scope | TEXT | engine / database |
| database_name | TEXT | Database name (nullable) |
| updated_at | TIMESTAMPTZ | Last update time |

Required minimum metrics (native):
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

Labeled metrics:
`sys.performance` has no separate label column. Metrics that carry labels
must encode them into the metric name using a Prometheus-style suffix.
Example:
- buffer_pool_reads_total{source=disk}
- query_rows_affected_total{type=insert}

Extended metrics for emulation (required for parity views):
- connections_total (counter; from scratchbird_connections_established_total)
- queries_total{type=select|insert|update|delete|ddl|other}
- query_rows_returned_total (counter; from scratchbird_query_rows_returned_sum)
- query_rows_affected_total{type=insert|update|delete} (counter; from scratchbird_query_rows_affected_sum)
- buffer_pool_reads_total{source=cache|disk}
- copy_rows_total{direction=from|to}
- copy_bytes_total{direction=from|to}
- copy_errors_total
- uptime_seconds
- page_size_bytes
- ods_major
- ods_minor
- allocated_pages
- page_buffers
- next_transaction
- oldest_transaction (from scratchbird_mga_oldest_active_transaction)
- query_progress_rows
- query_progress_bytes
- query_progress_last_update_micros

Mapping guidance:
- Values should be sourced from the in-process metrics registry described in
  `operations/PROMETHEUS_METRICS_REFERENCE.md`.

### 6) sys.table_stats

Per-table runtime statistics used for pg_stat_all_tables parity.

| Column | Type | Description |
| --- | --- | --- |
| table_id | UUID | Table identifier |
| schema_name | TEXT | Schema name |
| table_name | TEXT | Table name |
| seq_scan_count | BIGINT | Sequential scans started |
| last_seq_scan_at | TIMESTAMPTZ | Last sequential scan time (nullable) |
| seq_rows_read | BIGINT | Rows read by sequential scans |
| idx_scan_count | BIGINT | Index scans started |
| last_idx_scan_at | TIMESTAMPTZ | Last index scan time (nullable) |
| idx_rows_fetch | BIGINT | Rows fetched by index scans |
| rows_inserted | BIGINT | Rows inserted (committed) |
| rows_updated | BIGINT | Rows updated (committed) |
| rows_deleted | BIGINT | Rows deleted (committed) |
| rows_hot_updated | BIGINT | HOT updates (if supported) |
| rows_newpage_updated | BIGINT | Updates that moved to new page |
| live_rows_estimate | BIGINT | Estimated live rows |
| dead_rows_estimate | BIGINT | Estimated dead rows |
| mod_since_analyze | BIGINT | Modifications since last analyze |
| ins_since_vacuum | BIGINT | Inserts since last sweep/GC |
| last_vacuum_at | TIMESTAMPTZ | Last sweep/GC completion (nullable) |
| last_autovacuum_at | TIMESTAMPTZ | Last background GC completion (nullable) |
| last_analyze_at | TIMESTAMPTZ | Last manual analyze (nullable) |
| last_autoanalyze_at | TIMESTAMPTZ | Last auto analyze (nullable) |
| vacuum_count | BIGINT | Sweep/GC count (PostgreSQL alias) |
| autovacuum_count | BIGINT | Background GC count (PostgreSQL alias) |
| analyze_count | BIGINT | Manual analyze count |
| autoanalyze_count | BIGINT | Auto analyze count |
| total_vacuum_time_ms | BIGINT | Cumulative sweep/GC time (ms) |
| total_autovacuum_time_ms | BIGINT | Cumulative background GC time (ms) |
| total_analyze_time_ms | BIGINT | Cumulative manual analyze time (ms) |
| total_autoanalyze_time_ms | BIGINT | Cumulative auto analyze time (ms) |

Instrumentation notes:
- Counters increment on commit to avoid inflating stats for rolled back work.
- seq_rows_read / idx_rows_fetch count rows read from heap, not rows returned.
- last_* timestamps update when a scan/maintenance action completes.
- seq_scan_count/idx_scan_count increment when the corresponding scan node starts.
- rows_* counters increment after a successful row modification is visible.

### 7) sys.io_stats

Per-object I/O counters used for Firebird MON$IO_STATS parity.

| Column | Type | Description |
| --- | --- | --- |
| stat_id | BIGINT | 0 for database; connection_id/transaction_id/statement_id for others |
| stat_group | SMALLINT | 0=database, 1=connection, 2=transaction, 3=statement, 4=call |
| session_id | UUID | Owning session (nullable) |
| transaction_id | BIGINT | Transaction id (nullable) |
| statement_id | BIGINT | Statement id (nullable) |
| page_reads | BIGINT | Pages read from disk |
| page_writes | BIGINT | Pages written to disk |
| page_fetches | BIGINT | Pages fetched from cache |
| page_marks | BIGINT | Pages marked dirty |

Instrumentation notes:
- page_reads/page_fetches map to buffer pool reads (disk/cache).
- page_marks increments on first dirtying of a page within the stat scope.
- stat_group=2/3 are required for MON$IO_STATS groups 2/3 parity.
- Buffer manager must attribute page operations to the current statement and
  transaction via execution context (statement_id/transaction_id).

### 8) sys.tablespace_migrations (Beta)

Online tablespace migration state (Beta). Backed by `pg_tablespace_migrations`.

| Column | Type | Description |
| --- | --- | --- |
| table_id | UUID | Table identifier |
| source_tablespace_id | SMALLINT | Source tablespace id |
| target_tablespace_id | SMALLINT | Target tablespace id |
| shadow_table_id | UUID | Shadow table identifier |
| delta_log_id | UUID | Delta log identifier |
| state | TEXT | PREPARE/COPY/CATCH_UP/CUTOVER/CLEANUP/DONE/FAILED/CANCELED |
| migration_start_xid | BIGINT | Migration start transaction id |
| cutover_xid | BIGINT | Cutover transaction id |
| rows_copied | BIGINT | Rows copied so far |
| rows_total_est | BIGINT | Estimated total rows |
| rows_per_sec | DOUBLE | Smoothed rows per second |
| eta_seconds | BIGINT | Estimated seconds remaining |
| last_lag_ms | BIGINT | Last observed catch-up lag (ms) |
| last_lag_sample_at | TIMESTAMPTZ | Lag sample timestamp |
| throttle_state | TEXT | NONE/LAG/IO/MANUAL |
| throttle_sleep_ms | INT | Current throttle delay (ms) |
| last_progress_at | TIMESTAMPTZ | Last progress update |
| last_error_code | INT | Last error code (nullable) |
| created_at | TIMESTAMPTZ | Migration start time |
| updated_at | TIMESTAMPTZ | Last state update time |

Notes:
- This view is Beta-only and requires online tablespace migration support.
- When `state=DONE`, the old storage is eligible for sweep/GC after pre-cutover snapshots finish.

### 9) sys.shard_migrations (Beta, Cluster)

Cluster shard migration state (Beta). Backed by `pg_shard_migrations`.

| Column | Type | Description |
| --- | --- | --- |
| migration_id | UUID | Migration identifier |
| source_shard_id | UUID | Source shard uuid |
| target_shard_id | UUID | Target shard uuid |
| state | TEXT | PREPARE/COPY/CATCH_UP/CUTOVER/CLEANUP/DONE/FAILED/CANCELED |
| migration_start_xid | BIGINT | Migration start transaction id |
| cutover_xid | BIGINT | Cutover transaction id |
| bytes_copied | BIGINT | Bytes copied so far |
| rows_copied | BIGINT | Rows copied so far |
| bytes_per_sec | DOUBLE | Smoothed bytes per second |
| rows_per_sec | DOUBLE | Smoothed rows per second |
| eta_seconds | BIGINT | Estimated seconds remaining |
| last_lag_ms | BIGINT | Last observed catch-up lag (ms) |
| last_lag_sample_at | TIMESTAMPTZ | Lag sample timestamp |
| throttle_state | TEXT | NONE/LAG/IO/MANUAL |
| throttle_sleep_ms | INT | Current throttle delay (ms) |
| last_progress_at | TIMESTAMPTZ | Last progress update |
| last_error_code | INT | Last error code (nullable) |
| created_at | TIMESTAMPTZ | Migration start time |
| updated_at | TIMESTAMPTZ | Last state update time |

Notes:
- This view is Beta-only and requires sharding.
- Rows are visible only to superusers or cluster administrators.

## Emulation Mapping (High-Level)

Emulated dialects should map their monitoring views to these ScratchBird views:

| Emulated View | ScratchBird Source |
| --- | --- |
| pg_stat_activity | sys.sessions + sys.statements |
| pg_locks | sys.locks |
| pg_stat_database | sys.performance |
| pg_stat_bgwriter | sys.performance |
| pg_stat_all_tables | sys.table_stats |
| information_schema.PROCESSLIST | sys.sessions |
| performance_schema.data_locks | sys.locks |
| performance_schema.global_status | sys.performance |
| MON$ATTACHMENTS | sys.sessions |
| MON$LOCKS | sys.locks |
| MON$STATEMENTS | sys.statements |
| MON$DATABASE | sys.performance |
| MON$TRANSACTIONS | sys.transactions |
| MON$IO_STATS | sys.io_stats |

Dialect-specific column mapping is defined in
`operations/MONITORING_DIALECT_MAPPINGS.md` (with parser requirements in
`parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md`).

## Examples

List active sessions:
```
SELECT session_id, user_name, state, current_query
FROM sys.sessions
ORDER BY connected_at DESC;
```

Show waiting locks:
```
SELECT lock_type, lock_mode, relation_name, wait_start
FROM sys.locks
WHERE lock_state = 'waiting'
ORDER BY wait_start DESC;
```

Performance counters:
```
SELECT metric, value, unit
FROM sys.performance
ORDER BY metric;
```

## Related Specs

- docs/specifications/operations/PROMETHEUS_METRICS_REFERENCE.md
- docs/specifications/operations/MONITORING_DIALECT_MAPPINGS.md
- docs/specifications/transaction/TRANSACTION_MAIN.md
- docs/specifications/transaction/TRANSACTION_LOCK_MANAGER.md
- docs/specifications/parser/EMULATED_DATABASE_PARSER_SPECIFICATION.md
