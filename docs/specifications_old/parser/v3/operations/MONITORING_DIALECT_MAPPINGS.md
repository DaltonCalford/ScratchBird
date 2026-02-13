# Dialect Monitoring View Mappings (Authoritative)

Status: Authoritative (V3)
Last Updated: 2026-02-08

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

Not covered in this document:
- Dialect-specific historical stats reset timestamps (rejected in V3)
- Cluster-wide aggregation (rejected in V3)

## General Mapping Rules

- Base row set for sessions comes from `sys.sessions`.
- Statement columns join from `sys.statements` on `session_id`.
- Transaction columns join from `sys.transactions` on `session_id`.
- Lock columns join from `sys.locks` on `session_id` or `transaction_id`.
- Performance counters pivot from `sys.performance` where `scope` matches
  database rows (if `scope=engine` only, values may be reused for all databases).
- Unavailable columns MUST return `NULL` (or a documented constant) to preserve
  schema shape and types.
- PostgreSQL OID mapping follows `operations/OID_MAPPING_STRATEGY.md`.
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
| datid | sys.oid_map | Map database UUID → OID per `OID_MAPPING_STRATEGY.md`; if mapping disabled, return NULL |
| datname | sys.sessions.database_name |  |
| pid | sys.sessions.connection_id | Cast to INT if needed |
| usesysid | NULL | User/role OID mapping is not defined in V3; MUST return NULL |
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
| database | sys.oid_map | Map database UUID → OID per `OID_MAPPING_STRATEGY.md`; if mapping disabled, return NULL |
| relation | sys.oid_map | Map relation UUID → OID per `OID_MAPPING_STRATEGY.md`; if mapping disabled, return NULL |
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
| datid | sys.oid_map | Map database UUID → OID per `OID_MAPPING_STRATEGY.md`; if mapping disabled, return NULL |
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
| n_ins_since_vacuum | sys.table_stats.ins_since_vacuum | Sweep/GC count since last sweep |
| last_vacuum | sys.table_stats.last_vacuum_at | Sweep/GC completion time |
| last_autovacuum | sys.table_stats.last_autovacuum_at | Background GC completion time |
| last_analyze | sys.table_stats.last_analyze_at |  |
| last_autoanalyze | sys.table_stats.last_autoanalyze_at |  |
| vacuum_count | sys.table_stats.vacuum_count | Sweep/GC runs (PostgreSQL alias) |
| autovacuum_count | sys.table_stats.autovacuum_count | Background GC runs (PostgreSQL alias) |
| analyze_count | sys.table_stats.analyze_count |  |
| autoanalyze_count | sys.table_stats.autoanalyze_count |  |
| total_vacuum_time | sys.table_stats.total_vacuum_time_ms | Convert ms to seconds |
| total_autovacuum_time | sys.table_stats.total_autovacuum_time_ms | Convert ms to seconds |
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
| ENGINE_LOCK_ID | sys.locks.lock_id | String form |
| OBJECT_SCHEMA | sys.locks.schema_name |  |
| OBJECT_NAME | sys.locks.object_name |  |
| LOCK_TYPE | sys.locks.lock_type |  |
| LOCK_MODE | sys.locks.lock_mode |  |
| LOCK_STATUS | sys.locks.granted | Map to GRANTED/WAITING |
| THREAD_ID | sys.sessions.connection_id | Join on session_id |

### performance_schema.global_status

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| VARIABLE_NAME | sys.performance.metric_name |  |
| VARIABLE_VALUE | sys.performance.metric_value | Cast to string |

## Firebird Mappings

### MON$ATTACHMENTS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$ATTACHMENT_ID | sys.sessions.connection_id |  |
| MON$USER | sys.sessions.user_name |  |
| MON$ROLE | sys.sessions.role_name |  |
| MON$REMOTE_ADDRESS | sys.sessions.client_addr |  |
| MON$REMOTE_PID | NULL | Not tracked |
| MON$STATE | sys.sessions.state | Map via state table |

### MON$LOCKS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$LOCK_ID | sys.locks.lock_id |  |
| MON$LOCK_TYPE | sys.locks.lock_type |  |
| MON$LOCK_STATE | sys.locks.granted | Map to 0/1 |
| MON$LOCK_OWNER | sys.locks.session_id |  |

### MON$STATEMENTS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$STATEMENT_ID | sys.statements.statement_id |  |
| MON$ATTACHMENT_ID | sys.statements.session_id |  |
| MON$SQL_TEXT | sys.statements.sql_text |  |
| MON$STATE | sys.statements.state | Map to Firebird codes |

### MON$DATABASE

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$DATABASE_NAME | sys.sessions.database_name |  |
| MON$PAGE_SIZE | sys.database.page_size |  |
| MON$ODS_VERSION | sys.database.ods_version |  |
| MON$OLDEST_TRANSACTION | sys.transactions.oldest_xid |  |
| MON$OLDEST_ACTIVE | sys.transactions.oldest_active_xid |  |
| MON$OLDEST_SNAPSHOT | sys.transactions.oldest_snapshot_xid |  |

### MON$TRANSACTIONS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$TRANSACTION_ID | sys.transactions.transaction_id |  |
| MON$STATE | sys.transactions.state | Map to Firebird codes |
| MON$TIMESTAMP | sys.transactions.start_time |  |

### MON$IO_STATS

| Column | ScratchBird source | Notes |
| --- | --- | --- |
| MON$PAGE_READS | sys.performance.page_reads_total |  |
| MON$PAGE_WRITES | sys.performance.page_writes_total |  |
| MON$PAGE_FETCHES | sys.performance.page_fetches_total |  |
| MON$PAGE_MARKS | sys.performance.page_marks_total |  |

## Related Specs

- `docs/specifications/parser/v3/operations/MONITORING_SQL_VIEWS.md`
- `docs/specifications/parser/v3/operations/OID_MAPPING_STRATEGY.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_MGA_CORE.md`
- `docs/specifications/parser/v3/transaction/TRANSACTION_LOCK_MANAGER.md`
- `docs/specifications/parser/v3/storage/MGA_IMPLEMENTATION.md`
- `docs/specifications/parser/v3/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`
- `docs/specifications/parser/v3/transaction/FIREBIRD_CONSTANTS_REFERENCE.md`
