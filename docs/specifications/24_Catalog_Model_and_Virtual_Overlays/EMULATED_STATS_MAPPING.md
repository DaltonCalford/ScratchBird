# Emulated Engine Statistics Mapping

## Purpose
Ensure ScratchBird meets or exceeds all statistics surfaces of emulated engines by mapping their metrics to canonical ScratchBird stats or storing them as engine-specific metrics.

## Invariants
- Every emulated engine metric MUST be exposed through its expected catalog or command surface.
- If a metric maps directly to canonical ScratchBird stats, the mapping MUST be defined.
- If a metric has no canonical equivalent, it MUST be stored in the emulation stats tables and exposed via overlay views.

## Catalog Tables
### `emulated_stat_def`
- `stat_uuid` UUID
- `engine` enum (firebird, postgresql, mysql, cassandra, milvus, mongodb, neo4j, redis)
- `stat_name` STRING
- `stat_unit` STRING
- `stat_type` enum (int64, uint64, float64, bool, string, blob)
- `scope` enum (instance, database, schema, table, index, filespace, collection, keyspace, namespace, user, role)
- `description` STRING
- `is_required` bool

### `emulated_stat_value`
- `stat_uuid` UUID
- `object_uuid` UUID nullable (scope-dependent)
- `value_i64` INT64
- `value_u64` UINT64
- `value_f64` FLOAT64
- `value_bool` BOOL
- `value_string` STRING
- `value_blob` BLOB
- `updated_txid` UINT64
- `updated_at` TIMESTAMP

### `emulated_stat_map`
- `engine` enum
- `source_name` STRING
- `scope` enum
- `metric_ref` STRING
- `map_kind` enum (direct, scale, ratio, sum, derived)
- `scale_factor` FLOAT64 nullable
- `derived_expr_sblr` BLOB nullable

`metric_ref` uses the form `<table>.<column>`, for example `table_stats.row_count_live`.

## Mapping Rules
1. If a metric is directly represented in a canonical stats table, insert a row in `emulated_stat_map` with `map_kind=direct`.
2. If a metric requires scaling, store `scale_factor` and use `map_kind=scale`.
3. If a metric is derived from multiple canonical metrics, store `derived_expr_sblr` and use `map_kind=derived`.
4. If no canonical metric exists, store the metric in `emulated_stat_value`.
5. Overlay views for each emulated engine must:
   - Check `emulated_stat_map` first and compute mapped values.
   - Fall back to `emulated_stat_value` for unmapped metrics.

## Update Rules
- Canonical stats are updated by `ANALYZE`, `VACUUM`, background stats jobs, and diagnostic scans.
- Emulated stats not mapped to canonical stats are updated by emulated parser routines when available or by background collectors.

## Coverage Requirement
Before enabling an emulated engine, its statistics surface MUST be fully covered by:
- A mapping to canonical stats, or
- Explicit storage in `emulated_stat_value`.

## Error Handling
- Missing mapping for a required metric is `EMULATED_STATS_INCOMPLETE`.

## Firebird 5.x Metric List and Mapping
Required monitoring tables: `MON$DATABASE`, `MON$IO_STATS`, `MON$RECORD_STATS`, `MON$MEMORY_USAGE`, `MON$ATTACHMENTS`, `MON$TRANSACTIONS`, `MON$STATEMENTS`.

### MON$DATABASE (database scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| MON$DATABASE | MON$DATABASE_NAME | database | emulated_stat_value |
| MON$DATABASE | MON$PAGE_SIZE | database | emulated_stat_value |
| MON$DATABASE | MON$ODS_MAJOR | database | emulated_stat_value |
| MON$DATABASE | MON$ODS_MINOR | database | emulated_stat_value |
| MON$DATABASE | MON$OLDEST_TRANSACTION | database | emulated_stat_value |
| MON$DATABASE | MON$OLDEST_ACTIVE | database | emulated_stat_value |
| MON$DATABASE | MON$OLDEST_SNAPSHOT | database | emulated_stat_value |
| MON$DATABASE | MON$NEXT_TRANSACTION | database | emulated_stat_value |
| MON$DATABASE | MON$PAGE_BUFFERS | database | emulated_stat_value |

### MON$IO_STATS (per STAT_GROUP scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| MON$IO_STATS | MON$PAGE_READS | database, connection, transaction, statement, call | emulated_stat_value |
| MON$IO_STATS | MON$PAGE_WRITES | database, connection, transaction, statement, call | emulated_stat_value |
| MON$IO_STATS | MON$PAGE_FETCHES | database, connection, transaction, statement, call | emulated_stat_value |
| MON$IO_STATS | MON$PAGE_MARKS | database, connection, transaction, statement, call | emulated_stat_value |

### MON$RECORD_STATS (per STAT_GROUP scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| MON$RECORD_STATS | MON$RECORD_SEQ_READS | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_IDX_READS | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_INSERTS | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_UPDATES | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_DELETES | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_BACKOUTS | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_PURGES | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_EXPUNGES | database, connection, transaction, statement, call | emulated_stat_value |
| MON$RECORD_STATS | MON$RECORD_LOCKS | database, connection, transaction, statement, call | emulated_stat_value |

### MON$MEMORY_USAGE (per STAT_GROUP scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| MON$MEMORY_USAGE | MON$MEMORY_USED | database, connection, transaction, statement, call | emulated_stat_value |
| MON$MEMORY_USAGE | MON$MEMORY_ALLOCATED | database, connection, transaction, statement, call | emulated_stat_value |
| MON$MEMORY_USAGE | MON$MAX_MEMORY_USED | database, connection, transaction, statement, call | emulated_stat_value |
| MON$MEMORY_USAGE | MON$MAX_MEMORY_ALLOCATED | database, connection, transaction, statement, call | emulated_stat_value |

### MON$ATTACHMENTS (connection scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| MON$ATTACHMENTS | MON$ATTACHMENT_ID | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$SERVER_PID | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$STATE | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$ATTACHMENT_NAME | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$USER | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$ROLE | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$REMOTE_PROTOCOL | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$REMOTE_ADDRESS | connection | emulated_stat_value |
| MON$ATTACHMENTS | MON$REMOTE_PID | connection | emulated_stat_value |

### MON$TRANSACTIONS (transaction scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| MON$TRANSACTIONS | MON$TRANSACTION_ID | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$ATTACHMENT_ID | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$STATE | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$TIMESTAMP | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$TOP_TRANSACTION | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$OLDEST_TRANSACTION | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$OLDEST_ACTIVE | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$ISOLATION_MODE | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$LOCK_TIMEOUT | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$READ_ONLY | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$AUTO_COMMIT | transaction | emulated_stat_value |
| MON$TRANSACTIONS | MON$AUTO_UNDO | transaction | emulated_stat_value |

### MON$STATEMENTS (statement scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| MON$STATEMENTS | MON$STATEMENT_ID | statement | emulated_stat_value |
| MON$STATEMENTS | MON$ATTACHMENT_ID | statement | emulated_stat_value |
| MON$STATEMENTS | MON$TRANSACTION_ID | statement | emulated_stat_value |
| MON$STATEMENTS | MON$STATE | statement | emulated_stat_value |
| MON$STATEMENTS | MON$TIMESTAMP | statement | emulated_stat_value |
| MON$STATEMENTS | MON$SQL_TEXT | statement | emulated_stat_value |
| MON$STATEMENTS | MON$STATEMENT_TIMEOUT | statement | emulated_stat_value |
| MON$STATEMENTS | MON$STATEMENT_TIMER | statement | emulated_stat_value |
| MON$STATEMENTS | MON$EXPLAINED_PLAN | statement | emulated_stat_value |

## PostgreSQL 18.x Metric List and Mapping
Required views: `pg_stat_database`, `pg_stat_all_tables`/`pg_stat_user_tables`, `pg_stat_all_indexes`/`pg_stat_user_indexes`, `pg_statio_all_tables`/`pg_statio_user_tables`, `pg_statio_all_indexes`/`pg_statio_user_indexes`.

### pg_stat_database (database scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| pg_stat_database | numbackends | database | emulated_stat_value |
| pg_stat_database | xact_commit | database | emulated_stat_value |
| pg_stat_database | xact_rollback | database | emulated_stat_value |
| pg_stat_database | blks_read | database | emulated_stat_value |
| pg_stat_database | blks_hit | database | emulated_stat_value |
| pg_stat_database | tup_returned | database | emulated_stat_value |
| pg_stat_database | tup_fetched | database | emulated_stat_value |
| pg_stat_database | tup_inserted | database | emulated_stat_value |
| pg_stat_database | tup_updated | database | emulated_stat_value |
| pg_stat_database | tup_deleted | database | emulated_stat_value |
| pg_stat_database | conflicts | database | emulated_stat_value |
| pg_stat_database | temp_files | database | emulated_stat_value |
| pg_stat_database | temp_bytes | database | emulated_stat_value |
| pg_stat_database | deadlocks | database | emulated_stat_value |
| pg_stat_database | blk_read_time | database | emulated_stat_value |
| pg_stat_database | blk_write_time | database | emulated_stat_value |
| pg_stat_database | stats_reset | database | emulated_stat_value |

### pg_stat_all_tables / pg_stat_user_tables (table scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| pg_stat_all_tables | seq_scan | table | emulated_stat_value |
| pg_stat_all_tables | last_seq_scan | table | emulated_stat_value |
| pg_stat_all_tables | seq_tup_read | table | emulated_stat_value |
| pg_stat_all_tables | idx_scan | table | emulated_stat_value |
| pg_stat_all_tables | idx_tup_fetch | table | emulated_stat_value |
| pg_stat_all_tables | n_tup_ins | table | emulated_stat_value |
| pg_stat_all_tables | n_tup_upd | table | emulated_stat_value |
| pg_stat_all_tables | n_tup_del | table | emulated_stat_value |
| pg_stat_all_tables | n_tup_hot_upd | table | emulated_stat_value |
| pg_stat_all_tables | n_live_tup | table | table_stats.row_count_live |
| pg_stat_all_tables | n_dead_tup | table | table_stats.row_count_dead |
| pg_stat_all_tables | n_mod_since_analyze | table | emulated_stat_value |
| pg_stat_all_tables | last_vacuum | table | emulated_stat_value |
| pg_stat_all_tables | last_autovacuum | table | emulated_stat_value |
| pg_stat_all_tables | last_analyze | table | emulated_stat_value |
| pg_stat_all_tables | last_autoanalyze | table | emulated_stat_value |
| pg_stat_all_tables | vacuum_count | table | emulated_stat_value |
| pg_stat_all_tables | autovacuum_count | table | emulated_stat_value |
| pg_stat_all_tables | analyze_count | table | emulated_stat_value |
| pg_stat_all_tables | autoanalyze_count | table | emulated_stat_value |

### pg_stat_all_indexes / pg_stat_user_indexes (index scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| pg_stat_all_indexes | idx_scan | index | index_usage.scan_count |
| pg_stat_all_indexes | last_idx_scan | index | index_usage.last_used_at |
| pg_stat_all_indexes | idx_tup_read | index | index_usage.tuple_read |
| pg_stat_all_indexes | idx_tup_fetch | index | index_usage.tuple_returned |

### pg_statio_all_tables / pg_statio_user_tables (table scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| pg_statio_all_tables | heap_blks_read | table | emulated_stat_value |
| pg_statio_all_tables | heap_blks_hit | table | emulated_stat_value |
| pg_statio_all_tables | idx_blks_read | table | emulated_stat_value |
| pg_statio_all_tables | idx_blks_hit | table | emulated_stat_value |
| pg_statio_all_tables | toast_blks_read | table | emulated_stat_value |
| pg_statio_all_tables | toast_blks_hit | table | emulated_stat_value |
| pg_statio_all_tables | tidx_blks_read | table | emulated_stat_value |
| pg_statio_all_tables | tidx_blks_hit | table | emulated_stat_value |

### pg_statio_all_indexes / pg_statio_user_indexes (index scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| pg_statio_all_indexes | idx_blks_read | index | index_usage.blocks_read |
| pg_statio_all_indexes | idx_blks_hit | index | index_usage.blocks_hit |

## MySQL 8.x Metric List and Mapping
Required surfaces: `INFORMATION_SCHEMA.TABLES`, `performance_schema.table_io_waits_summary_by_table`, `performance_schema.table_io_waits_summary_by_index_usage`, `performance_schema.table_lock_waits_summary_by_table`.

### INFORMATION_SCHEMA.TABLES (table scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| INFORMATION_SCHEMA.TABLES | TABLE_ROWS | table | table_stats.row_count_est |
| INFORMATION_SCHEMA.TABLES | AVG_ROW_LENGTH | table | table_stats.avg_row_len |
| INFORMATION_SCHEMA.TABLES | DATA_LENGTH | table | emulated_stat_value |
| INFORMATION_SCHEMA.TABLES | INDEX_LENGTH | table | emulated_stat_value |
| INFORMATION_SCHEMA.TABLES | DATA_FREE | table | emulated_stat_value |
| INFORMATION_SCHEMA.TABLES | CREATE_TIME | table | emulated_stat_value |
| INFORMATION_SCHEMA.TABLES | UPDATE_TIME | table | emulated_stat_value |
| INFORMATION_SCHEMA.TABLES | CHECK_TIME | table | emulated_stat_value |
| INFORMATION_SCHEMA.TABLES | TABLE_COLLATION | table | emulated_stat_value |
| INFORMATION_SCHEMA.TABLES | CHECKSUM | table | emulated_stat_value |

### performance_schema.table_io_waits_summary_by_table (table scope)
Columns: `COUNT_STAR`, `SUM_TIMER_WAIT`, `MIN_TIMER_WAIT`, `AVG_TIMER_WAIT`, `MAX_TIMER_WAIT`, `COUNT_READ`, `SUM_TIMER_READ`, `MIN_TIMER_READ`, `AVG_TIMER_READ`, `MAX_TIMER_READ`, `COUNT_WRITE`, `SUM_TIMER_WRITE`, `MIN_TIMER_WRITE`, `AVG_TIMER_WRITE`, `MAX_TIMER_WRITE`, `COUNT_FETCH`, `SUM_TIMER_FETCH`, `MIN_TIMER_FETCH`, `AVG_TIMER_FETCH`, `MAX_TIMER_FETCH`, `COUNT_INSERT`, `SUM_TIMER_INSERT`, `MIN_TIMER_INSERT`, `AVG_TIMER_INSERT`, `MAX_TIMER_INSERT`, `COUNT_UPDATE`, `SUM_TIMER_UPDATE`, `MIN_TIMER_UPDATE`, `AVG_TIMER_UPDATE`, `MAX_TIMER_UPDATE`, `COUNT_DELETE`, `SUM_TIMER_DELETE`, `MIN_TIMER_DELETE`, `AVG_TIMER_DELETE`, `MAX_TIMER_DELETE`.

All metrics in this table map to `emulated_stat_value` with scope `table`.

### performance_schema.table_io_waits_summary_by_index_usage (index scope)
Columns are identical to `table_io_waits_summary_by_table` with an additional `INDEX_NAME` grouping.

All metrics in this table map to `emulated_stat_value` with scope `index`.

### performance_schema.table_lock_waits_summary_by_table (table scope)
Columns: `COUNT_STAR`, `SUM_TIMER_WAIT`, `MIN_TIMER_WAIT`, `AVG_TIMER_WAIT`, `MAX_TIMER_WAIT`, `COUNT_READ`, `SUM_TIMER_READ`, `MIN_TIMER_READ`, `AVG_TIMER_READ`, `MAX_TIMER_READ`, `COUNT_WRITE`, `SUM_TIMER_WRITE`, `MIN_TIMER_WRITE`, `AVG_TIMER_WRITE`, `MAX_TIMER_WRITE`, `COUNT_READ_NORMAL`, `SUM_TIMER_READ_NORMAL`, `MIN_TIMER_READ_NORMAL`, `AVG_TIMER_READ_NORMAL`, `MAX_TIMER_READ_NORMAL`, `COUNT_READ_WITH_SHARED_LOCKS`, `SUM_TIMER_READ_WITH_SHARED_LOCKS`, `MIN_TIMER_READ_WITH_SHARED_LOCKS`, `AVG_TIMER_READ_WITH_SHARED_LOCKS`, `MAX_TIMER_READ_WITH_SHARED_LOCKS`, `COUNT_READ_HIGH_PRIORITY`, `SUM_TIMER_READ_HIGH_PRIORITY`, `MIN_TIMER_READ_HIGH_PRIORITY`, `AVG_TIMER_READ_HIGH_PRIORITY`, `MAX_TIMER_READ_HIGH_PRIORITY`, `COUNT_READ_NO_INSERT`, `SUM_TIMER_READ_NO_INSERT`, `MIN_TIMER_READ_NO_INSERT`, `AVG_TIMER_READ_NO_INSERT`, `MAX_TIMER_READ_NO_INSERT`, `COUNT_READ_EXTERNAL`, `SUM_TIMER_READ_EXTERNAL`, `MIN_TIMER_READ_EXTERNAL`, `AVG_TIMER_READ_EXTERNAL`, `MAX_TIMER_READ_EXTERNAL`, `COUNT_WRITE_ALLOW_WRITE`, `SUM_TIMER_WRITE_ALLOW_WRITE`, `MIN_TIMER_WRITE_ALLOW_WRITE`, `AVG_TIMER_WRITE_ALLOW_WRITE`, `MAX_TIMER_WRITE_ALLOW_WRITE`, `COUNT_WRITE_CONCURRENT_INSERT`, `SUM_TIMER_WRITE_CONCURRENT_INSERT`, `MIN_TIMER_WRITE_CONCURRENT_INSERT`, `AVG_TIMER_WRITE_CONCURRENT_INSERT`, `MAX_TIMER_WRITE_CONCURRENT_INSERT`, `COUNT_WRITE_LOW_PRIORITY`, `SUM_TIMER_WRITE_LOW_PRIORITY`, `MIN_TIMER_WRITE_LOW_PRIORITY`, `AVG_TIMER_WRITE_LOW_PRIORITY`, `MAX_TIMER_WRITE_LOW_PRIORITY`, `COUNT_WRITE_NORMAL`, `SUM_TIMER_WRITE_NORMAL`, `MIN_TIMER_WRITE_NORMAL`, `AVG_TIMER_WRITE_NORMAL`, `MAX_TIMER_WRITE_NORMAL`, `COUNT_WRITE_EXTERNAL`, `SUM_TIMER_WRITE_EXTERNAL`, `MIN_TIMER_WRITE_EXTERNAL`, `AVG_TIMER_WRITE_EXTERNAL`, `MAX_TIMER_WRITE_EXTERNAL`.

All metrics in this table map to `emulated_stat_value` with scope `table`.

## Cassandra 5.x Metric List and Mapping
Required surfaces: `system_views` virtual tables and SAI virtual tables in `system_views`.

### system_views.indexes (index scope, SAI)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| system_views.indexes | cell_count | index | index_stats.entry_count_est |
| system_views.indexes | indexed_sstable_count | index | emulated_stat_value |
| system_views.indexes | per_column_disk_size | index | index_storage.bytes_used |
| system_views.indexes | per_table_disk_size | index | emulated_stat_value |
| system_views.indexes | is_building | index | emulated_stat_value |
| system_views.indexes | is_queryable | index | emulated_stat_value |
| system_views.indexes | is_string | index | emulated_stat_value |
| system_views.indexes | analyzer | index | emulated_stat_value |
| system_views.indexes | column_name | index | emulated_stat_value |
| system_views.indexes | table_name | index | emulated_stat_value |
| system_views.indexes | keyspace_name | index | emulated_stat_value |

### system_views.sstable_indexes (index scope, SAI)
Columns: `keyspace_name`, `index_name`, `sstable_name`, `cell_count`, `column_name`, `end_token`, `format_version`, `max_row_id`, `min_row_id`, `per_column_disk_size`, `per_table_disk_size`, `start_token`, `table_name`.

All metrics in this table map to `emulated_stat_value` with scope `index`.

### system_views virtual table metrics (instance/table scope)
The following virtual tables and their columns are required and map to `emulated_stat_value`:
- `caches` (cache_name, capacity_bytes, entry_count, hit_count, hit_ratio, recent_hit_rate_per_second, recent_request_rate_per_second, request_count, size_bytes)
- `clients` (address, port, client_options, connection_stage, driver_name, driver_version, hostname, protocol_version, request_count, ssl_enabled, ssl_protocol, username)
- `coordinator_read_latency` (keyspace_name, table_name, count, max, median, per_second)
- `coordinator_write_latency` (keyspace_name, table_name, count, max, median, per_second)
- `coordinator_scan` (keyspace_name, table_name, count, max, median, per_second)
- `local_read_latency` (keyspace_name, table_name, count, max, median, per_second)
- `local_write_latency` (keyspace_name, table_name, count, max, median, per_second)
- `local_scan` (keyspace_name, table_name, count, max, median, per_second)
- `disk_usage` (keyspace_name, table_name, disk_space)
- `rows_per_read` (keyspace_name, table_name, count, max, median)
- `tombstones_per_read` (keyspace_name, table_name, count, max, median)
- `sstable_tasks` (keyspace_name, table_name, task_id, kind, progress, total, unit)

## MongoDB 8.x Metric List and Mapping
Required surfaces: `dbStats`, `collStats`, `$indexStats`, and `serverStatus`.

### dbStats (database scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| dbStats | collections | database | emulated_stat_value |
| dbStats | views | database | emulated_stat_value |
| dbStats | objects | database | emulated_stat_value |
| dbStats | avgObjSize | database | emulated_stat_value |
| dbStats | dataSize | database | emulated_stat_value |
| dbStats | storageSize | database | emulated_stat_value |
| dbStats | freeStorageSize | database | emulated_stat_value |
| dbStats | indexes | database | emulated_stat_value |
| dbStats | indexSize | database | emulated_stat_value |
| dbStats | indexFreeStorageSize | database | emulated_stat_value |
| dbStats | totalSize | database | emulated_stat_value |
| dbStats | totalFreeStorageSize | database | emulated_stat_value |
| dbStats | fsUsedSize | database | emulated_stat_value |
| dbStats | fsTotalSize | database | emulated_stat_value |

### collStats (collection scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| collStats | count | collection | table_stats.row_count_live |
| collStats | size | collection | emulated_stat_value |
| collStats | avgObjSize | collection | emulated_stat_value |
| collStats | storageSize | collection | emulated_stat_value |
| collStats | freeStorageSize | collection | emulated_stat_value |
| collStats | totalIndexSize | collection | emulated_stat_value |
| collStats | numOrphanDocs | collection | emulated_stat_value |
| collStats | capped | collection | emulated_stat_value |
| collStats | max | collection | emulated_stat_value |
| collStats | maxSize | collection | emulated_stat_value |

### $indexStats (index scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| $indexStats | accesses.ops | index | index_usage.scan_count |
| $indexStats | accesses.since | index | emulated_stat_value |
| $indexStats | name | index | emulated_stat_value |
| $indexStats | key | index | emulated_stat_value |
| $indexStats | host | index | emulated_stat_value |
| $indexStats | spec | index | emulated_stat_value |

### serverStatus (instance scope)
Required sections and fields:
- `connections` (current, available, totalCreated, rejected, active, threaded, exhaustIsMaster, exhaustHello, awaitingTopologyChanges, loadBalanced, queuedForEstablishment, establishmentRateLimit.*)
- `opcounters` (insert, query, update, delete, getmore, command)
- `mem` (bits, resident, virtual, supported)
- `host`, `version`, `process`, `service`, `pid`, `uptime`, `uptimeMillis`, `uptimeEstimate`, `localTime`
- `locks` (acquireCount.*, acquireWaitCount.*, timeAcquiringMicros.*, deadlockCount.*)

All `serverStatus` fields map to `emulated_stat_value` with scope `instance`.

## Neo4j 5.x Metric List and Mapping
Required surfaces: `SHOW INDEXES` and `db.stats.retrieve(section)`.

### SHOW INDEXES (index scope)
Columns: `id`, `name`, `state`, `populationPercent`, `type`, `entityType`, `labelsOrTypes`, `properties`, `indexProvider`.

Mapping:
- `populationPercent` -> emulated_stat_value
- `state` -> emulated_stat_value
- All other columns stored in `emulated_stat_value` for index scope.

### db.stats.retrieve('GRAPH COUNTS') (database scope)
Required keys:
- `nodeCount`
- `relationshipCount`
- `labelCounts` (map label -> count)
- `relationshipTypeCounts` (map reltype -> count)

All keys map to `emulated_stat_value`.

### db.stats.retrieve('TOKENS') (database scope)
Required keys:
- `labelCount`
- `relationshipTypeCount`
- `propertyKeyCount`

All keys map to `emulated_stat_value`.

### db.stats.retrieve('QUERIES') (database scope)
Required keys:
- `queryCount`
- `failedCount`
- `avgLatencyMs`
- `maxLatencyMs`

All keys map to `emulated_stat_value`.

### db.stats.retrieve('META') (database scope)
Required keys:
- `storeId`
- `storeVersion`
- `createdTime`

All keys map to `emulated_stat_value`.

## Redis Metric List and Mapping
Required surface: `INFO` command sections.

### Required INFO fields
Sections and required fields:
- `server`: `redis_version`, `process_id`, `config_file`, `uptime_in_seconds`, `uptime_in_days`
- `clients`: `connected_clients`, `blocked_clients`
- `memory`: `used_memory`, `mem_fragmentation_ratio`
- `persistence`: `rdb_last_save_time`, `rdb_changes_since_last_save`, `aof_rewrite_in_progress`
- `stats`: `keyspace_hits`, `keyspace_misses`, `expired_keys`, `evicted_keys`, `instantaneous_ops_per_sec`
- `replication`: `master_link_down_since`, `connected_slaves`, `master_last_io_seconds_ago`
- `cpu`: `used_cpu_sys`, `used_cpu_user`
- `cluster`: `cluster_enabled`
- `keyspace`: per-db `keys`, `expires`, `avg_ttl`
- `modules`: per-module `ver`, `options`

All INFO fields map to `emulated_stat_value` with scope `instance`, except `keyspace` entries which use scope `database`.

## Milvus 2.x Metric List and Mapping
Required surfaces: `getCollectionStatistics`, `describe_index`, `getIndexState`, `getLoadingProgress`.

### getCollectionStatistics (collection scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| getCollectionStatistics | row_count | collection | table_stats.row_count_live |

### describe_index (index scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| describe_index | index_type | index | emulated_stat_value |
| describe_index | metric_type | index | emulated_stat_value |
| describe_index | total_rows | index | emulated_stat_value |
| describe_index | indexed_rows | index | emulated_stat_value |
| describe_index | pending_index_rows | index | emulated_stat_value |
| describe_index | state | index | emulated_stat_value |
| describe_index | field_name | index | emulated_stat_value |
| describe_index | index_name | index | emulated_stat_value |

### getIndexState (index scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| getIndexState | state | index | emulated_stat_value |
| getIndexState | fail_reason | index | emulated_stat_value |

### getLoadingProgress (collection scope)
| Source | Metric | Scope | Mapping |
| --- | --- | --- | --- |
| getLoadingProgress | progress | collection | emulated_stat_value |
| getLoadingProgress | total_rows | collection | emulated_stat_value |
