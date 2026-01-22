# Dialect Monitoring View Mappings

Version: 1.0
Status: Draft (Alpha)
Last Updated: January 2026

## Purpose

Define column-level mappings from ScratchBird monitoring views (`sys.sessions`,
`sys.transactions`, `sys.locks`, `sys.statements`, `sys.performance`) to
emulated dialect monitoring views. This is the parity contract for native
monitoring data exposed through PostgreSQL, MySQL, and Firebird compatibility
layers.

## Scope

In scope:
- PostgreSQL: `pg_stat_activity`, `pg_locks`, `pg_stat_database`,
  `pg_stat_bgwriter`, `pg_stat_all_tables`
- MySQL: `information_schema.PROCESSLIST`, `performance_schema.data_locks`,
  `performance_schema.global_status`
- Firebird: `MON$ATTACHMENTS`, `MON$LOCKS`, `MON$STATEMENTS`, `MON$DATABASE`,
  `MON$TRANSACTIONS`, `MON$IO_STATS`

Out of scope:
- Dialect-specific historical stats reset timestamps
- Cluster-wide aggregation (Beta)

## General Mapping Rules

- Base row set for sessions comes from `sys.sessions`.
- Statement columns join from `sys.statements` on `session_id`.
- Transaction columns join from `sys.transactions` on `session_id`.
- Lock columns join from `sys.locks` on `session_id` or `transaction_id`.
- Performance counters pivot from `sys.performance` where `scope` matches
  database rows (if `scope=engine` only, values may be reused for all databases).
- Unavailable columns must return `NULL` (or documented constant) to preserve
  schema shape and types.
- Integer identity columns (pid, processlist ID, attachment id) should use
  `sys.sessions.connection_id`, cast to the target integer size if required.
- PostgreSQL OID mapping follows `docs/specifications/operations/OID_MAPPING_STRATEGY.md`.
  If OID mapping is disabled or unavailable, return `NULL`.

### State Mapping (Common)

| sys.sessions.state | PostgreSQL pg_stat_activity.state | MySQL PROCESSLIST.COMMAND | Firebird MON$STATE |
| --- | --- | --- | --- |
| idle | idle | Sleep | 0 |
| active | active | Query | 1 |
| idle_in_txn | idle in transaction | Sleep | 1 |
| waiting | active (with wait_event) | Query | 2 |

Firebird `MON$STATE` values follow Firebird semantics: 0 idle, 1 active, 2 stalled.

## PostgreSQL Mappings

### pg_stat_activity

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| datid | NULL | No OID mapping in Alpha |
| datname | sys.sessions.database_name |  |
| pid | sys.sessions.connection_id | Cast to INT if needed |
| usesysid | NULL | OID mapping not implemented |
| usename | sys.sessions.user_name |  |
| application_name | NULL | Populate from client metadata when available |
| client_addr | sys.sessions.client_addr |  |
| client_port | sys.sessions.client_port |  |
| backend_start | sys.sessions.connected_at |  |
| xact_start | sys.transactions.start_time | Join on session_id |
| query_start | sys.statements.start_time | Join on session_id |
| state_change | sys.sessions.last_activity_at |  |
| state | sys.sessions.state | See state mapping table |
| wait_event_type | CASE wait_event != NULL -> 'Lock' | Optional classification |
| wait_event | sys.sessions.wait_event |  |
| backend_xid | sys.transactions.transaction_id | Join on session_id |
| backend_xmin | NULL | Not tracked |
| query | COALESCE(sys.statements.sql_text, sys.sessions.current_query) |  |
| backend_type | 'client backend' | Constant |

Columns not listed above should return NULL or documented PostgreSQL defaults.

### pg_locks

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| locktype | sys.locks.lock_type |  |
| database | NULL | OID mapping not implemented |
| relation | NULL | OID mapping not implemented |
| page | sys.locks.page |  |
| tuple | sys.locks.tuple |  |
| virtualxid | sys.locks.virtual_xid |  |
| transactionid | sys.locks.transaction_id |  |
| classid | NULL |  |
| objid | NULL |  |
| objsubid | NULL |  |
| virtualtransaction | sys.locks.session_id | UUID rendered as text |
| pid | sys.sessions.connection_id | Join on session_id |
| mode | sys.locks.lock_mode |  |
| granted | sys.locks.granted |  |
| fastpath | false | Constant |

### pg_stat_database

| Column | sys.performance metric | Notes |
| --- | --- | --- |
| datid | NULL | OID mapping not implemented |
| datname | sys.performance.database_name | One row per database |
| numbackends | connections_active | Per database if available |
| xact_commit | transactions_committed_total | Counter |
| xact_rollback | transactions_rolled_back_total | Counter |
| blks_read | buffer_pool_reads_total{source=disk} | Counter |
| blks_hit | buffer_pool_reads_total{source=cache} | Counter |
| tup_returned | query_rows_returned_total | Counter |
| tup_fetched | NULL | Not tracked |
| tup_inserted | query_rows_affected_total{type=insert} | Counter |
| tup_updated | query_rows_affected_total{type=update} | Counter |
| tup_deleted | query_rows_affected_total{type=delete} | Counter |
| conflicts | 0 | Not tracked |
| temp_files | 0 | Not tracked |
| temp_bytes | 0 | Not tracked |
| deadlocks | deadlocks_total | Counter |
| blk_read_time | NULL | Not tracked |
| blk_write_time | NULL | Not tracked |
| stats_reset | NULL | Not tracked |

### pg_stat_bgwriter

`pg_stat_bgwriter` is a single-row view. Until bgwriter-specific counters exist,
map to overall buffer pool counters and return NULL for unavailable fields.

| Column | sys.performance metric | Notes |
| --- | --- | --- |
| buffers_clean | buffer_pool_writes_total | Total writes (not bgwriter-specific) |
| maxwritten_clean | NULL | Not tracked |
| buffers_alloc | page_buffers | Current buffer count (approx) |
| stats_reset | NULL | Not tracked |

### pg_stat_all_tables

Row set is derived from `sys.table_stats`.

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| relid | sys.table_stats.table_id | Stable 32-bit hash (e.g., lower 32 bits of UUIDv7) |
| schemaname | sys.table_stats.schema_name |  |
| relname | sys.table_stats.table_name |  |
| seq_scan | sys.table_stats.seq_scan_count |  |
| last_seq_scan | sys.table_stats.last_seq_scan_at |  |
| seq_tup_read | sys.table_stats.seq_rows_read |  |
| idx_scan | sys.table_stats.idx_scan_count |  |
| last_idx_scan | sys.table_stats.last_idx_scan_at |  |
| idx_tup_fetch | sys.table_stats.idx_rows_fetch |  |
| n_tup_ins | sys.table_stats.rows_inserted |  |
| n_tup_upd | sys.table_stats.rows_updated |  |
| n_tup_del | sys.table_stats.rows_deleted |  |
| n_tup_hot_upd | sys.table_stats.rows_hot_updated |  |
| n_tup_newpage_upd | sys.table_stats.rows_newpage_updated |  |
| n_live_tup | sys.table_stats.live_rows_estimate |  |
| n_dead_tup | sys.table_stats.dead_rows_estimate |  |
| n_mod_since_analyze | sys.table_stats.mod_since_analyze |  |
| n_ins_since_vacuum | sys.table_stats.ins_since_vacuum | Since last sweep/GC |
| last_vacuum | sys.table_stats.last_vacuum_at | Sweep/GC completion time |
| last_autovacuum | sys.table_stats.last_autovacuum_at | Background GC completion time |
| last_analyze | sys.table_stats.last_analyze_at |  |
| last_autoanalyze | sys.table_stats.last_autoanalyze_at |  |
| vacuum_count | sys.table_stats.vacuum_count | Sweep/GC runs (PostgreSQL alias) |
| autovacuum_count | sys.table_stats.autovacuum_count | Background GC runs (PostgreSQL alias) |
| analyze_count | sys.table_stats.analyze_count |  |
| autoanalyze_count | sys.table_stats.autoanalyze_count |  |
| total_vacuum_time | sys.table_stats.total_vacuum_time_ms | Sweep/GC time; convert ms to seconds |
| total_autovacuum_time | sys.table_stats.total_autovacuum_time_ms | Background GC time; convert ms to seconds |
| total_analyze_time | sys.table_stats.total_analyze_time_ms | Convert ms to seconds |
| total_autoanalyze_time | sys.table_stats.total_autoanalyze_time_ms | Convert ms to seconds |

## MySQL Mappings

### information_schema.PROCESSLIST

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| ID | sys.sessions.connection_id |  |
| USER | sys.sessions.user_name |  |
| HOST | sys.sessions.client_addr + ':' + client_port |  |
| DB | sys.sessions.database_name |  |
| COMMAND | sys.sessions.state | Map via state table |
| TIME | elapsed seconds in state | Derive from last_activity_at or statement start |
| STATE | sys.sessions.state |  |
| INFO | COALESCE(sys.statements.sql_text, sys.sessions.current_query) |  |

### performance_schema.data_locks

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| ENGINE | 'SCRATCHBIRD' | Constant |
| LOCK_ID | sys.locks.lock_id |  |
| ENGINE_LOCK_ID | sys.locks.lock_id | String form |
| ENGINE_TRANSACTION_ID | sys.locks.transaction_id |  |
| THREAD_ID | sys.sessions.connection_id | Join on session_id |
| EVENT_ID | NULL |  |
| OBJECT_SCHEMA | Parsed from relation_name | Split schema.table when possible |
| OBJECT_NAME | sys.locks.relation_name |  |
| PARTITION_NAME | NULL |  |
| SUBPARTITION_NAME | NULL |  |
| INDEX_NAME | NULL |  |
| OBJECT_INSTANCE_BEGIN | NULL |  |
| LOCK_TYPE | sys.locks.lock_type |  |
| LOCK_MODE | sys.locks.lock_mode |  |
| LOCK_STATUS | sys.locks.lock_state |  |
| LOCK_DATA | tuple/page identifier | Use tuple or page when available |

### performance_schema.global_status

Expose one row per variable name from `sys.performance` metrics:

| VARIABLE_NAME | sys.performance metric | Notes |
| --- | --- | --- |
| Connections | connections_total | Counter |
| Threads_connected | connections_active | Gauge |
| Threads_running | query_currently_running | Gauge |
| Com_select | queries_total{type=select} | Counter |
| Com_insert | queries_total{type=insert} | Counter |
| Com_update | queries_total{type=update} | Counter |
| Com_delete | queries_total{type=delete} | Counter |
| Innodb_buffer_pool_read_requests | buffer_pool_reads_total{source=cache} | Counter |
| Innodb_buffer_pool_reads | buffer_pool_reads_total{source=disk} | Counter |
| Innodb_row_lock_waits | lock_waits_total | Counter |
| Uptime | uptime_seconds | Counter |

Variables not listed above should return NULL or 0 (as appropriate) until
ScratchBird exposes the metric.

## Firebird Mappings

### MON$ATTACHMENTS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$ATTACHMENT_ID | sys.sessions.connection_id | Join on session_id when needed |
| MON$ATTACHMENT_NAME | sys.sessions.database_name |  |
| MON$USER | sys.sessions.user_name |  |
| MON$ROLE | sys.sessions.role_name |  |
| MON$REMOTE_PROTOCOL | sys.sessions.protocol |  |
| MON$REMOTE_ADDRESS | sys.sessions.client_addr |  |
| MON$STATE | sys.sessions.state | Map via state table |
| MON$TIMESTAMP | sys.sessions.connected_at |  |
| MON$SYSTEM_FLAG | 0 | Constant |

### MON$LOCKS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$LOCK_ID | sys.locks.lock_id |  |
| MON$LOCK_TYPE | sys.locks.lock_type |  |
| MON$LOCK_MODE | sys.locks.lock_mode |  |
| MON$LOCK_STATE | sys.locks.lock_state | Map to Firebird lock state codes |
| MON$ATTACHMENT_ID | sys.sessions.connection_id | Join on session_id |
| MON$TRANSACTION_ID | sys.locks.transaction_id |  |
| MON$OBJECT_NAME | sys.locks.relation_name |  |

### MON$STATEMENTS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$STATEMENT_ID | sys.statements.statement_id |  |
| MON$ATTACHMENT_ID | sys.sessions.connection_id | Join on session_id |
| MON$TRANSACTION_ID | sys.statements.transaction_id |  |
| MON$STATE | sys.statements.state | Map running/waiting/idle to Firebird state codes |
| MON$TIMESTAMP | sys.statements.start_time |  |
| MON$SQL_TEXT | sys.statements.sql_text |  |

### MON$DATABASE

| Column | sys.performance metric | Notes |
| --- | --- | --- |
| MON$DATABASE_NAME | database_name | One row per database |
| MON$PAGE_SIZE | page_size_bytes | Gauge |
| MON$ODS_MAJOR | ods_major | Gauge |
| MON$ODS_MINOR | ods_minor | Gauge |
| MON$OLDEST_TRANSACTION | oldest_transaction | Gauge |
| MON$NEXT_TRANSACTION | next_transaction | Gauge |
| MON$PAGE_BUFFERS | page_buffers | Gauge |
| MON$ALLOCATED_PAGES | allocated_pages | Gauge |

### MON$TRANSACTIONS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$TRANSACTION_ID | sys.transactions.transaction_id |  |
| MON$ATTACHMENT_ID | sys.sessions.connection_id | Join on session_id |
| MON$STATE | sys.transactions.state | Map idle/active to Firebird state codes |
| MON$TIMESTAMP | sys.transactions.start_time |  |
| MON$TOP_TRANSACTION | sys.transactions.transaction_id | Nested txn not tracked |
| MON$OLDEST_TRANSACTION | sys.performance.oldest_transaction | OIT |
| MON$OLDEST_ACTIVE | sys.performance.oldest_transaction | Use OAT metric when available |
| MON$ISOLATION_MODE | sys.transactions.isolation_level | Map to Firebird mode codes |
| MON$LOCK_TIMEOUT | NULL | Not tracked |
| MON$READ_ONLY | sys.transactions.read_only | 1/0 |
| MON$AUTO_COMMIT | NULL | Derive from session when available |
| MON$AUTO_UNDO | 0 | Not tracked |
| MON$STAT_ID | sys.transactions.transaction_id | Surrogate for join to MON$IO_STATS |

Isolation mode mapping:
- consistency (serializable) -> 0
- concurrency (repeatable_read) -> 1
- read committed record version -> 2
- read committed no record version -> 3
- read committed read consistency -> 4

### MON$IO_STATS

`MON$IO_STATS` returns cumulative counters per statistics group.

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$STAT_ID | sys.io_stats.stat_id |  |
| MON$STAT_GROUP | sys.io_stats.stat_group | 0=database, 1=connection, 2=transaction, 3=statement, 4=call |
| MON$PAGE_READS | sys.io_stats.page_reads |  |
| MON$PAGE_WRITES | sys.io_stats.page_writes |  |
| MON$PAGE_FETCHES | sys.io_stats.page_fetches |  |
| MON$PAGE_MARKS | sys.io_stats.page_marks |  |

Columns not listed above should return NULL or documented defaults until
ScratchBird exposes the required data.

Firebird state code mapping for statements should follow the attachment mapping:
0 idle, 1 active, 2 stalled.
