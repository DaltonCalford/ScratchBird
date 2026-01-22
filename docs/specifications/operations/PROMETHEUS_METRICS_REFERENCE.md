# Prometheus Metrics Reference

## 1. Overview

ScratchBird exposes metrics in Prometheus format on the metrics endpoint (default: `http://localhost:9090/metrics`). All metrics use the `scratchbird_` prefix.

**Metrics Endpoint:** `/metrics`
**Default Port:** 9090 (configurable)
**Format:** Prometheus text exposition format

---

## 2. Connection Metrics

### 2.1 Connection Counts

```prometheus
# HELP scratchbird_connections_total Total number of connections
# TYPE scratchbird_connections_total gauge
scratchbird_connections_total{state="active"} 45
scratchbird_connections_total{state="idle"} 120
scratchbird_connections_total{state="idle_in_transaction"} 5
scratchbird_connections_total{state="waiting"} 2

# HELP scratchbird_connections_max Maximum allowed connections
# TYPE scratchbird_connections_max gauge
scratchbird_connections_max 500

# HELP scratchbird_connections_used_ratio Connection utilization ratio
# TYPE scratchbird_connections_used_ratio gauge
scratchbird_connections_used_ratio 0.344
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `state` | active, idle, idle_in_transaction, waiting | Connection state |

### 2.2 Connection Lifecycle

```prometheus
# HELP scratchbird_connections_established_total Total connections established
# TYPE scratchbird_connections_established_total counter
scratchbird_connections_established_total{protocol="postgresql"} 15234
scratchbird_connections_established_total{protocol="mysql"} 8901
scratchbird_connections_established_total{protocol="firebird"} 123
scratchbird_connections_established_total{protocol="native"} 7890

# HELP scratchbird_connections_closed_total Total connections closed
# TYPE scratchbird_connections_closed_total counter
scratchbird_connections_closed_total{reason="normal"} 14500
scratchbird_connections_closed_total{reason="timeout"} 234
scratchbird_connections_closed_total{reason="error"} 56
scratchbird_connections_closed_total{reason="admin_kill"} 12

# HELP scratchbird_connections_rejected_total Rejected connection attempts
# TYPE scratchbird_connections_rejected_total counter
scratchbird_connections_rejected_total{reason="max_connections"} 45
scratchbird_connections_rejected_total{reason="auth_failed"} 123
scratchbird_connections_rejected_total{reason="ssl_required"} 8
scratchbird_connections_rejected_total{reason="database_not_found"} 15
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `protocol` | postgresql, mysql, firebird, native (tds post-gold) | Wire protocol |
| `reason` | normal, timeout, error, admin_kill, max_connections, auth_failed, ssl_required, database_not_found | Close/reject reason |

### 2.3 Connection Timing

```prometheus
# HELP scratchbird_connection_duration_seconds Connection duration histogram
# TYPE scratchbird_connection_duration_seconds histogram
scratchbird_connection_duration_seconds_bucket{le="1"} 1234
scratchbird_connection_duration_seconds_bucket{le="10"} 5678
scratchbird_connection_duration_seconds_bucket{le="60"} 8901
scratchbird_connection_duration_seconds_bucket{le="300"} 9500
scratchbird_connection_duration_seconds_bucket{le="3600"} 9800
scratchbird_connection_duration_seconds_bucket{le="+Inf"} 10000
scratchbird_connection_duration_seconds_sum 45678.9
scratchbird_connection_duration_seconds_count 10000

# HELP scratchbird_connection_setup_seconds Connection setup time
# TYPE scratchbird_connection_setup_seconds histogram
scratchbird_connection_setup_seconds_bucket{protocol="postgresql",le="0.001"} 5000
scratchbird_connection_setup_seconds_bucket{protocol="postgresql",le="0.005"} 9000
scratchbird_connection_setup_seconds_bucket{protocol="postgresql",le="0.01"} 9800
scratchbird_connection_setup_seconds_bucket{protocol="postgresql",le="+Inf"} 10000
```

---

## 3. Query Metrics

### 3.1 Query Counts

```prometheus
# HELP scratchbird_queries_total Total queries executed
# TYPE scratchbird_queries_total counter
scratchbird_queries_total{type="select"} 1234567
scratchbird_queries_total{type="insert"} 456789
scratchbird_queries_total{type="update"} 234567
scratchbird_queries_total{type="delete"} 12345
scratchbird_queries_total{type="ddl"} 567
scratchbird_queries_total{type="other"} 890

# HELP scratchbird_queries_failed_total Failed queries
# TYPE scratchbird_queries_failed_total counter
scratchbird_queries_failed_total{error_class="syntax"} 123
scratchbird_queries_failed_total{error_class="permission"} 45
scratchbird_queries_failed_total{error_class="constraint"} 234
scratchbird_queries_failed_total{error_class="timeout"} 12
scratchbird_queries_failed_total{error_class="deadlock"} 5
scratchbird_queries_failed_total{error_class="other"} 34
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `type` | select, insert, update, delete, ddl, other | Query type |
| `error_class` | syntax, permission, constraint, timeout, deadlock, serialization, other | Error category |

### 3.2 Query Timing

```prometheus
# HELP scratchbird_query_duration_seconds Query execution time
# TYPE scratchbird_query_duration_seconds histogram
scratchbird_query_duration_seconds_bucket{type="select",le="0.001"} 500000
scratchbird_query_duration_seconds_bucket{type="select",le="0.01"} 900000
scratchbird_query_duration_seconds_bucket{type="select",le="0.1"} 1100000
scratchbird_query_duration_seconds_bucket{type="select",le="1"} 1200000
scratchbird_query_duration_seconds_bucket{type="select",le="10"} 1230000
scratchbird_query_duration_seconds_bucket{type="select",le="+Inf"} 1234567
scratchbird_query_duration_seconds_sum{type="select"} 12345.67
scratchbird_query_duration_seconds_count{type="select"} 1234567

# HELP scratchbird_query_rows_returned Rows returned by queries
# TYPE scratchbird_query_rows_returned histogram
scratchbird_query_rows_returned_bucket{le="1"} 800000
scratchbird_query_rows_returned_bucket{le="10"} 1000000
scratchbird_query_rows_returned_bucket{le="100"} 1150000
scratchbird_query_rows_returned_bucket{le="1000"} 1200000
scratchbird_query_rows_returned_bucket{le="+Inf"} 1234567

# HELP scratchbird_query_rows_affected Rows affected by DML
# TYPE scratchbird_query_rows_affected histogram
scratchbird_query_rows_affected_bucket{type="insert",le="1"} 400000
scratchbird_query_rows_affected_bucket{type="insert",le="100"} 450000
scratchbird_query_rows_affected_bucket{type="insert",le="1000"} 455000
scratchbird_query_rows_affected_bucket{type="insert",le="+Inf"} 456789
```

### 3.3 Slow Queries

```prometheus
# HELP scratchbird_slow_queries_total Queries exceeding slow threshold
# TYPE scratchbird_slow_queries_total counter
scratchbird_slow_queries_total{threshold="1s"} 1234
scratchbird_slow_queries_total{threshold="5s"} 234
scratchbird_slow_queries_total{threshold="30s"} 45

# HELP scratchbird_query_currently_running Currently executing queries
# TYPE scratchbird_query_currently_running gauge
scratchbird_query_currently_running 23
```

---

## 4. Transaction Metrics

### 4.1 Transaction Counts

```prometheus
# HELP scratchbird_transactions_total Total transactions
# TYPE scratchbird_transactions_total counter
scratchbird_transactions_total{status="committed"} 567890
scratchbird_transactions_total{status="rolled_back"} 12345
scratchbird_transactions_total{status="aborted"} 234

# HELP scratchbird_transactions_active Currently active transactions
# TYPE scratchbird_transactions_active gauge
scratchbird_transactions_active 45

# HELP scratchbird_transactions_idle_in_transaction Idle in transaction
# TYPE scratchbird_transactions_idle_in_transaction gauge
scratchbird_transactions_idle_in_transaction 5
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `status` | committed, rolled_back, aborted | Transaction outcome |

### 4.2 Transaction Timing

```prometheus
# HELP scratchbird_transaction_duration_seconds Transaction duration
# TYPE scratchbird_transaction_duration_seconds histogram
scratchbird_transaction_duration_seconds_bucket{le="0.01"} 400000
scratchbird_transaction_duration_seconds_bucket{le="0.1"} 500000
scratchbird_transaction_duration_seconds_bucket{le="1"} 550000
scratchbird_transaction_duration_seconds_bucket{le="10"} 565000
scratchbird_transaction_duration_seconds_bucket{le="+Inf"} 580469

# HELP scratchbird_transaction_oldest_seconds Age of oldest transaction
# TYPE scratchbird_transaction_oldest_seconds gauge
scratchbird_transaction_oldest_seconds 45.6
```

### 4.3 MGA/MVCC Metrics

```prometheus
# HELP scratchbird_mga_versions_created Record versions created
# TYPE scratchbird_mga_versions_created counter
scratchbird_mga_versions_created 12345678

# HELP scratchbird_mga_versions_garbage Garbage versions pending cleanup
# TYPE scratchbird_mga_versions_garbage gauge
scratchbird_mga_versions_garbage 45678

# HELP scratchbird_mga_oldest_active_transaction Oldest active transaction ID
# TYPE scratchbird_mga_oldest_active_transaction gauge
scratchbird_mga_oldest_active_transaction 1234567890

# HELP scratchbird_mga_conflicts_total Transaction conflicts detected
# TYPE scratchbird_mga_conflicts_total counter
scratchbird_mga_conflicts_total{type="update_conflict"} 234
scratchbird_mga_conflicts_total{type="serialization_failure"} 56
```

---

## 5. Memory Metrics

### 5.1 Buffer Pool

```prometheus
# HELP scratchbird_buffer_pool_size_bytes Buffer pool size
# TYPE scratchbird_buffer_pool_size_bytes gauge
scratchbird_buffer_pool_size_bytes 4294967296

# HELP scratchbird_buffer_pool_used_bytes Buffer pool used
# TYPE scratchbird_buffer_pool_used_bytes gauge
scratchbird_buffer_pool_used_bytes 3221225472

# HELP scratchbird_buffer_pool_hit_ratio Buffer pool hit ratio
# TYPE scratchbird_buffer_pool_hit_ratio gauge
scratchbird_buffer_pool_hit_ratio 0.9945

# HELP scratchbird_buffer_pool_reads_total Buffer pool reads
# TYPE scratchbird_buffer_pool_reads_total counter
scratchbird_buffer_pool_reads_total{source="cache"} 98765432
scratchbird_buffer_pool_reads_total{source="disk"} 543210

# HELP scratchbird_buffer_pool_writes_total Buffer pool writes
# TYPE scratchbird_buffer_pool_writes_total counter
scratchbird_buffer_pool_writes_total 234567

# HELP scratchbird_buffer_pool_evictions_total Pages evicted
# TYPE scratchbird_buffer_pool_evictions_total counter
scratchbird_buffer_pool_evictions_total{type="clean"} 123456
scratchbird_buffer_pool_evictions_total{type="dirty"} 45678
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `source` | cache, disk | Read source |
| `type` | clean, dirty | Eviction type |

### 5.2 Memory Allocation

```prometheus
# HELP scratchbird_memory_allocated_bytes Total memory allocated
# TYPE scratchbird_memory_allocated_bytes gauge
scratchbird_memory_allocated_bytes{area="buffer_pool"} 4294967296
scratchbird_memory_allocated_bytes{area="query_memory"} 536870912
scratchbird_memory_allocated_bytes{area="connection_memory"} 268435456
scratchbird_memory_allocated_bytes{area="cache"} 134217728
scratchbird_memory_allocated_bytes{area="other"} 67108864

# HELP scratchbird_memory_limit_bytes Memory limit
# TYPE scratchbird_memory_limit_bytes gauge
scratchbird_memory_limit_bytes 8589934592
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `area` | buffer_pool, query_memory, connection_memory, cache, sort, hash, other | Memory area |

---

## 6. Storage Metrics

### 6.1 Disk I/O

```prometheus
# HELP scratchbird_disk_read_bytes_total Bytes read from disk
# TYPE scratchbird_disk_read_bytes_total counter
scratchbird_disk_read_bytes_total 12345678901234

# HELP scratchbird_disk_write_bytes_total Bytes written to disk
# TYPE scratchbird_disk_write_bytes_total counter
scratchbird_disk_write_bytes_total 9876543210987

# HELP scratchbird_disk_read_operations_total Disk read operations
# TYPE scratchbird_disk_read_operations_total counter
scratchbird_disk_read_operations_total 12345678

# HELP scratchbird_disk_write_operations_total Disk write operations
# TYPE scratchbird_disk_write_operations_total counter
scratchbird_disk_write_operations_total 2345678

# HELP scratchbird_disk_read_latency_seconds Disk read latency
# TYPE scratchbird_disk_read_latency_seconds histogram
scratchbird_disk_read_latency_seconds_bucket{le="0.0001"} 10000000
scratchbird_disk_read_latency_seconds_bucket{le="0.001"} 12000000
scratchbird_disk_read_latency_seconds_bucket{le="0.01"} 12300000
scratchbird_disk_read_latency_seconds_bucket{le="+Inf"} 12345678

# HELP scratchbird_disk_write_latency_seconds Disk write latency
# TYPE scratchbird_disk_write_latency_seconds histogram
scratchbird_disk_write_latency_seconds_bucket{le="0.0001"} 2000000
scratchbird_disk_write_latency_seconds_bucket{le="0.001"} 2300000
scratchbird_disk_write_latency_seconds_bucket{le="0.01"} 2340000
scratchbird_disk_write_latency_seconds_bucket{le="+Inf"} 2345678
```

### 6.2 Database Size

```prometheus
# HELP scratchbird_database_size_bytes Database size
# TYPE scratchbird_database_size_bytes gauge
scratchbird_database_size_bytes{database="mydb"} 10737418240
scratchbird_database_size_bytes{database="analytics"} 53687091200

# HELP scratchbird_table_size_bytes Table size (top tables)
# TYPE scratchbird_table_size_bytes gauge
scratchbird_table_size_bytes{database="mydb",table="orders"} 2147483648
scratchbird_table_size_bytes{database="mydb",table="users"} 536870912

# HELP scratchbird_index_size_bytes Index size
# TYPE scratchbird_index_size_bytes gauge
scratchbird_index_size_bytes{database="mydb",index="idx_orders_date"} 268435456

# HELP scratchbird_disk_usage_bytes Total disk usage
# TYPE scratchbird_disk_usage_bytes gauge
scratchbird_disk_usage_bytes{type="data"} 64424509440
scratchbird_disk_usage_bytes{type="wal"} 1073741824  # optional post-gold
scratchbird_disk_usage_bytes{type="temp"} 536870912
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `database` | (database name) | Database name |
| `table` | (table name) | Table name |
| `index` | (index name) | Index name |
| `type` | data, wal (optional post-gold), temp, backup | Storage type |

---

## 7. Write-after log (WAL, optional post-gold)/Recovery Metrics

**Scope Note:** MGA does not use write-after log (WAL) for recovery. These metrics apply only if an optional write-after log is introduced (post-gold).

```prometheus
# HELP scratchbird_wal_bytes_written_total Write-after log (WAL, optional post-gold) bytes written
# TYPE scratchbird_wal_bytes_written_total counter
scratchbird_wal_bytes_written_total 9876543210

# HELP scratchbird_wal_segments_total Write-after log (WAL, optional post-gold) segments
# TYPE scratchbird_wal_segments_total gauge
scratchbird_wal_segments_total{state="active"} 3
scratchbird_wal_segments_total{state="archived"} 1234
scratchbird_wal_segments_total{state="pending_archive"} 2

# HELP scratchbird_wal_segment_size_bytes Write-after log (WAL, optional post-gold) segment size
# TYPE scratchbird_wal_segment_size_bytes gauge
scratchbird_wal_segment_size_bytes 16777216

# HELP scratchbird_checkpoint_duration_seconds Checkpoint duration
# TYPE scratchbird_checkpoint_duration_seconds histogram
scratchbird_checkpoint_duration_seconds_bucket{le="1"} 900
scratchbird_checkpoint_duration_seconds_bucket{le="5"} 990
scratchbird_checkpoint_duration_seconds_bucket{le="30"} 999
scratchbird_checkpoint_duration_seconds_bucket{le="+Inf"} 1000

# HELP scratchbird_checkpoint_total Checkpoints completed
# TYPE scratchbird_checkpoint_total counter
scratchbird_checkpoint_total{type="scheduled"} 876
scratchbird_checkpoint_total{type="requested"} 124

# HELP scratchbird_seconds_since_last_checkpoint Time since last checkpoint
# TYPE scratchbird_seconds_since_last_checkpoint gauge
scratchbird_seconds_since_last_checkpoint 234.5
```

---

## 8. Replication Metrics

```prometheus
# HELP scratchbird_replication_lag_seconds Replication lag
# TYPE scratchbird_replication_lag_seconds gauge
scratchbird_replication_lag_seconds{standby="standby1.example.com"} 0.5
scratchbird_replication_lag_seconds{standby="standby2.example.com"} 1.2

# HELP scratchbird_replication_lag_bytes Replication lag in bytes
# TYPE scratchbird_replication_lag_bytes gauge
scratchbird_replication_lag_bytes{standby="standby1.example.com"} 8388608
scratchbird_replication_lag_bytes{standby="standby2.example.com"} 16777216

# HELP scratchbird_replication_state Replication state (1=streaming, 0=other)
# TYPE scratchbird_replication_state gauge
scratchbird_replication_state{standby="standby1.example.com",state="streaming"} 1
scratchbird_replication_state{standby="standby2.example.com",state="streaming"} 1

# HELP scratchbird_replication_sent_bytes_total Bytes sent to standbys
# TYPE scratchbird_replication_sent_bytes_total counter
scratchbird_replication_sent_bytes_total{standby="standby1.example.com"} 123456789012
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `standby` | (hostname) | Standby server |
| `state` | streaming, catchup, backup, startup | Replication state |

---

## 9. Lock Metrics

```prometheus
# HELP scratchbird_locks_held Current locks held
# TYPE scratchbird_locks_held gauge
scratchbird_locks_held{mode="AccessShare"} 45
scratchbird_locks_held{mode="RowShare"} 12
scratchbird_locks_held{mode="RowExclusive"} 23
scratchbird_locks_held{mode="ShareUpdateExclusive"} 5
scratchbird_locks_held{mode="Share"} 2
scratchbird_locks_held{mode="ShareRowExclusive"} 1
scratchbird_locks_held{mode="Exclusive"} 3
scratchbird_locks_held{mode="AccessExclusive"} 0

# HELP scratchbird_locks_waiting Locks waiting
# TYPE scratchbird_locks_waiting gauge
scratchbird_locks_waiting 3

# HELP scratchbird_deadlocks_total Deadlocks detected
# TYPE scratchbird_deadlocks_total counter
scratchbird_deadlocks_total 12

# HELP scratchbird_lock_wait_time_seconds Lock wait time
# TYPE scratchbird_lock_wait_time_seconds histogram
scratchbird_lock_wait_time_seconds_bucket{le="0.001"} 50000
scratchbird_lock_wait_time_seconds_bucket{le="0.01"} 55000
scratchbird_lock_wait_time_seconds_bucket{le="0.1"} 56000
scratchbird_lock_wait_time_seconds_bucket{le="1"} 56500
scratchbird_lock_wait_time_seconds_bucket{le="+Inf"} 56789
```

**Labels:**
| Label | Values | Description |
|-------|--------|-------------|
| `mode` | AccessShare, RowShare, RowExclusive, ShareUpdateExclusive, Share, ShareRowExclusive, Exclusive, AccessExclusive | Lock mode |

---

## 10. Connection Pool Metrics

```prometheus
# HELP scratchbird_pool_connections Pool connections
# TYPE scratchbird_pool_connections gauge
scratchbird_pool_connections{pool="default",state="active"} 45
scratchbird_pool_connections{pool="default",state="idle"} 55
scratchbird_pool_connections{pool="default",state="waiting"} 2

# HELP scratchbird_pool_size_max Maximum pool size
# TYPE scratchbird_pool_size_max gauge
scratchbird_pool_size_max{pool="default"} 100

# HELP scratchbird_pool_acquires_total Pool acquires
# TYPE scratchbird_pool_acquires_total counter
scratchbird_pool_acquires_total{pool="default"} 1234567

# HELP scratchbird_pool_acquire_duration_seconds Pool acquire time
# TYPE scratchbird_pool_acquire_duration_seconds histogram
scratchbird_pool_acquire_duration_seconds_bucket{pool="default",le="0.0001"} 1200000
scratchbird_pool_acquire_duration_seconds_bucket{pool="default",le="0.001"} 1230000
scratchbird_pool_acquire_duration_seconds_bucket{pool="default",le="0.01"} 1234000
scratchbird_pool_acquire_duration_seconds_bucket{pool="default",le="+Inf"} 1234567

# HELP scratchbird_pool_timeouts_total Pool acquire timeouts
# TYPE scratchbird_pool_timeouts_total counter
scratchbird_pool_timeouts_total{pool="default"} 23
```

---

## 11. Cache Metrics

### 11.1 Statement Cache

```prometheus
# HELP scratchbird_stmt_cache_size Statement cache entries
# TYPE scratchbird_stmt_cache_size gauge
scratchbird_stmt_cache_size 5000

# HELP scratchbird_stmt_cache_hits_total Statement cache hits
# TYPE scratchbird_stmt_cache_hits_total counter
scratchbird_stmt_cache_hits_total 9876543

# HELP scratchbird_stmt_cache_misses_total Statement cache misses
# TYPE scratchbird_stmt_cache_misses_total counter
scratchbird_stmt_cache_misses_total 123456

# HELP scratchbird_stmt_cache_evictions_total Statement cache evictions
# TYPE scratchbird_stmt_cache_evictions_total counter
scratchbird_stmt_cache_evictions_total 45678
```

### 11.2 Result Cache

```prometheus
# HELP scratchbird_result_cache_size_bytes Result cache size
# TYPE scratchbird_result_cache_size_bytes gauge
scratchbird_result_cache_size_bytes 134217728

# HELP scratchbird_result_cache_entries Result cache entries
# TYPE scratchbird_result_cache_entries gauge
scratchbird_result_cache_entries 12345

# HELP scratchbird_result_cache_hits_total Result cache hits
# TYPE scratchbird_result_cache_hits_total counter
scratchbird_result_cache_hits_total 567890

# HELP scratchbird_result_cache_misses_total Result cache misses
# TYPE scratchbird_result_cache_misses_total counter
scratchbird_result_cache_misses_total 234567

# HELP scratchbird_result_cache_invalidations_total Cache invalidations
# TYPE scratchbird_result_cache_invalidations_total counter
scratchbird_result_cache_invalidations_total{reason="ttl"} 12345
scratchbird_result_cache_invalidations_total{reason="table_modified"} 5678
scratchbird_result_cache_invalidations_total{reason="manual"} 123
```

---

## 12. Garbage Collection Metrics

```prometheus
# HELP scratchbird_gc_runs_total Garbage collection runs
# TYPE scratchbird_gc_runs_total counter
scratchbird_gc_runs_total 12345

# HELP scratchbird_gc_duration_seconds GC duration
# TYPE scratchbird_gc_duration_seconds histogram
scratchbird_gc_duration_seconds_bucket{le="0.1"} 12000
scratchbird_gc_duration_seconds_bucket{le="1"} 12300
scratchbird_gc_duration_seconds_bucket{le="10"} 12340
scratchbird_gc_duration_seconds_bucket{le="+Inf"} 12345

# HELP scratchbird_gc_records_cleaned Records cleaned by GC
# TYPE scratchbird_gc_records_cleaned counter
scratchbird_gc_records_cleaned 98765432

# HELP scratchbird_gc_bytes_freed Bytes freed by GC
# TYPE scratchbird_gc_bytes_freed counter
scratchbird_gc_bytes_freed 12345678901

# HELP scratchbird_gc_lag_records Pending garbage records
# TYPE scratchbird_gc_lag_records gauge
scratchbird_gc_lag_records 45678
```

---

## 13. Backup Metrics

```prometheus
# HELP scratchbird_backup_last_success_timestamp Last successful backup
# TYPE scratchbird_backup_last_success_timestamp gauge
scratchbird_backup_last_success_timestamp{type="full"} 1709251200
scratchbird_backup_last_success_timestamp{type="incremental"} 1709337600

# HELP scratchbird_backup_duration_seconds Backup duration
# TYPE scratchbird_backup_duration_seconds gauge
scratchbird_backup_duration_seconds{type="full"} 3600.5
scratchbird_backup_duration_seconds{type="incremental"} 120.3

# HELP scratchbird_backup_size_bytes Last backup size
# TYPE scratchbird_backup_size_bytes gauge
scratchbird_backup_size_bytes{type="full"} 10737418240
scratchbird_backup_size_bytes{type="incremental"} 536870912

# HELP scratchbird_backup_running Is backup running (1=yes, 0=no)
# TYPE scratchbird_backup_running gauge
scratchbird_backup_running 0
```

---

## 13.1 Migration Metrics (Beta)

### 13.1.1 Tablespace Migration

```prometheus
# HELP scratchbird_tablespace_migration_rows_copied_total Rows copied during tablespace migration
# TYPE scratchbird_tablespace_migration_rows_copied_total counter
scratchbird_tablespace_migration_rows_copied_total{table="orders"} 987654

# HELP scratchbird_tablespace_migration_rows_per_sec Rows copied per second
# TYPE scratchbird_tablespace_migration_rows_per_sec gauge
scratchbird_tablespace_migration_rows_per_sec{table="orders"} 12500

# HELP scratchbird_tablespace_migration_eta_seconds Estimated seconds remaining
# TYPE scratchbird_tablespace_migration_eta_seconds gauge
scratchbird_tablespace_migration_eta_seconds{table="orders"} 180

# HELP scratchbird_tablespace_migration_lag_ms Catch-up lag in milliseconds
# TYPE scratchbird_tablespace_migration_lag_ms gauge
scratchbird_tablespace_migration_lag_ms{table="orders"} 250

# HELP scratchbird_tablespace_migration_throttle_state Current throttle state
# TYPE scratchbird_tablespace_migration_throttle_state gauge
scratchbird_tablespace_migration_throttle_state{table="orders",state="none"} 1

# HELP scratchbird_tablespace_migration_state Migration state
# TYPE scratchbird_tablespace_migration_state gauge
scratchbird_tablespace_migration_state{table="orders",state="copy"} 1
```

### 13.1.2 Shard Migration (Cluster)

```prometheus
# HELP scratchbird_shard_migration_bytes_copied_total Bytes copied during shard migration
# TYPE scratchbird_shard_migration_bytes_copied_total counter
scratchbird_shard_migration_bytes_copied_total{shard="shard_01"} 10737418240

# HELP scratchbird_shard_migration_rows_copied_total Rows copied during shard migration
# TYPE scratchbird_shard_migration_rows_copied_total counter
scratchbird_shard_migration_rows_copied_total{shard="shard_01"} 1234567

# HELP scratchbird_shard_migration_bytes_per_sec Bytes copied per second
# TYPE scratchbird_shard_migration_bytes_per_sec gauge
scratchbird_shard_migration_bytes_per_sec{shard="shard_01"} 104857600

# HELP scratchbird_shard_migration_rows_per_sec Rows copied per second
# TYPE scratchbird_shard_migration_rows_per_sec gauge
scratchbird_shard_migration_rows_per_sec{shard="shard_01"} 20000

# HELP scratchbird_shard_migration_eta_seconds Estimated seconds remaining
# TYPE scratchbird_shard_migration_eta_seconds gauge
scratchbird_shard_migration_eta_seconds{shard="shard_01"} 240

# HELP scratchbird_shard_migration_lag_ms Catch-up lag in milliseconds
# TYPE scratchbird_shard_migration_lag_ms gauge
scratchbird_shard_migration_lag_ms{shard="shard_01"} 500

# HELP scratchbird_shard_migration_throttle_state Current throttle state
# TYPE scratchbird_shard_migration_throttle_state gauge
scratchbird_shard_migration_throttle_state{shard="shard_01",state="lag"} 1

# HELP scratchbird_shard_migration_state Migration state
# TYPE scratchbird_shard_migration_state gauge
scratchbird_shard_migration_state{shard="shard_01",state="catch_up"} 1
```

Labels:
| Label | Values | Description |
|-------|--------|-------------|
| `table` | table name or UUID | Tablespace migration target |
| `shard` | shard name or UUID | Shard migration target |
| `state` | copy, catch_up, cutover, cleanup, done, failed | Migration state |

---

## 13.2 COPY Metrics (Alpha)

```prometheus
# HELP scratchbird_copy_rows_total Rows processed by COPY
# TYPE scratchbird_copy_rows_total counter
scratchbird_copy_rows_total{direction="from"} 1000000
scratchbird_copy_rows_total{direction="to"} 500000

# HELP scratchbird_copy_bytes_total Bytes processed by COPY
# TYPE scratchbird_copy_bytes_total counter
scratchbird_copy_bytes_total{direction="from"} 104857600
scratchbird_copy_bytes_total{direction="to"} 52428800

# HELP scratchbird_copy_errors_total COPY errors
# TYPE scratchbird_copy_errors_total counter
scratchbird_copy_errors_total 12

# HELP scratchbird_copy_duration_seconds COPY duration
# TYPE scratchbird_copy_duration_seconds histogram
scratchbird_copy_duration_seconds_bucket{le="0.1"} 5
scratchbird_copy_duration_seconds_bucket{le="1"} 15
scratchbird_copy_duration_seconds_bucket{le="5"} 40
scratchbird_copy_duration_seconds_bucket{le="+Inf"} 50
scratchbird_copy_duration_seconds_sum 123.4
scratchbird_copy_duration_seconds_count 50
```

Labels:
| Label | Values | Description |
|-------|--------|-------------|
| `direction` | from, to | COPY FROM or COPY TO |

---

## 14. Server Info Metrics

```prometheus
# HELP scratchbird_info Server information
# TYPE scratchbird_info gauge
scratchbird_info{version="1.0.0",build="abc123",go_version="1.21"} 1

# HELP scratchbird_uptime_seconds Server uptime
# TYPE scratchbird_uptime_seconds gauge
scratchbird_uptime_seconds 604800

# HELP scratchbird_start_time_seconds Server start time
# TYPE scratchbird_start_time_seconds gauge
scratchbird_start_time_seconds 1708646400

# HELP scratchbird_config_reload_total Config reloads
# TYPE scratchbird_config_reload_total counter
scratchbird_config_reload_total{status="success"} 5
scratchbird_config_reload_total{status="failed"} 0
```

---

## 15. Alert Thresholds

### 15.1 Prometheus Alerting Rules

```yaml
# /etc/prometheus/rules/scratchbird.yml
groups:
  - name: scratchbird
    rules:
      # Connection alerts
      - alert: ScratchBirdConnectionsHigh
        expr: scratchbird_connections_used_ratio > 0.8
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High connection usage ({{ $value | humanizePercentage }})"

      - alert: ScratchBirdConnectionsCritical
        expr: scratchbird_connections_used_ratio > 0.95
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Critical connection usage ({{ $value | humanizePercentage }})"

      # Query alerts
      - alert: ScratchBirdSlowQueries
        expr: rate(scratchbird_slow_queries_total{threshold="5s"}[5m]) > 1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High rate of slow queries (>5s)"

      - alert: ScratchBirdQueryErrors
        expr: rate(scratchbird_queries_failed_total[5m]) > 10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High query error rate"

      # Replication alerts
      - alert: ScratchBirdReplicationLag
        expr: scratchbird_replication_lag_seconds > 10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Replication lag > 10s on {{ $labels.standby }}"

      - alert: ScratchBirdReplicationLagCritical
        expr: scratchbird_replication_lag_seconds > 60
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Critical replication lag on {{ $labels.standby }}"

      # Memory alerts
      - alert: ScratchBirdBufferPoolLow
        expr: scratchbird_buffer_pool_hit_ratio < 0.9
        for: 15m
        labels:
          severity: warning
        annotations:
          summary: "Buffer pool hit ratio low ({{ $value | humanizePercentage }})"

      - alert: ScratchBirdMemoryHigh
        expr: sum(scratchbird_memory_allocated_bytes) / scratchbird_memory_limit_bytes > 0.9
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Memory usage > 90%"

      # Disk alerts
      - alert: ScratchBirdDiskUsageHigh
        expr: scratchbird_disk_usage_bytes{type="data"} / scratchbird_disk_total_bytes > 0.8
        for: 15m
        labels:
          severity: warning
        annotations:
          summary: "Disk usage > 80%"

      - alert: ScratchBirdDiskUsageCritical
        expr: scratchbird_disk_usage_bytes{type="data"} / scratchbird_disk_total_bytes > 0.95
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Disk usage > 95%"

      # Lock alerts
      - alert: ScratchBirdDeadlocks
        expr: rate(scratchbird_deadlocks_total[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "Deadlocks detected"

      - alert: ScratchBirdLockWaiting
        expr: scratchbird_locks_waiting > 10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High number of locks waiting"

      # Backup alerts
      - alert: ScratchBirdBackupStale
        expr: time() - scratchbird_backup_last_success_timestamp{type="full"} > 86400
        for: 1h
        labels:
          severity: warning
        annotations:
          summary: "No successful full backup in 24 hours"

      - alert: ScratchBirdBackupStaleCritical
        expr: time() - scratchbird_backup_last_success_timestamp{type="full"} > 172800
        for: 1h
        labels:
          severity: critical
        annotations:
          summary: "No successful full backup in 48 hours"

      # GC alerts
      - alert: ScratchBirdGCLag
        expr: scratchbird_gc_lag_records > 1000000
        for: 30m
        labels:
          severity: warning
        annotations:
          summary: "GC falling behind ({{ $value }} pending records)"

      # Server alerts
      - alert: ScratchBirdDown
        expr: up{job="scratchbird"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "ScratchBird server is down"

      - alert: ScratchBirdCheckpointLag
        expr: scratchbird_seconds_since_last_checkpoint > 900
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "No checkpoint in 15 minutes"
```

### 15.2 Threshold Summary Table

| Metric | Warning | Critical | Duration |
|--------|---------|----------|----------|
| Connection usage | >80% | >95% | 5m/1m |
| Buffer pool hit ratio | <90% | <80% | 15m |
| Replication lag | >10s | >60s | 5m/1m |
| Disk usage | >80% | >95% | 15m/5m |
| Memory usage | >90% | >95% | 5m |
| Slow queries (>5s) rate | >1/min | >10/min | 5m |
| Deadlock rate | >0.1/min | >1/min | 5m |
| Lock waits | >10 | >50 | 5m |
| Backup age (full) | >24h | >48h | 1h |
| GC lag | >1M records | >10M records | 30m |
| Checkpoint lag | >15min | >30min | 5m |

---

## 16. Grafana Dashboard

### 16.1 Dashboard JSON ID

```
Dashboard ID: scratchbird-overview
Folder: ScratchBird
```

### 16.2 Panel Recommendations

| Panel | Metrics | Visualization |
|-------|---------|---------------|
| Connections | connections_total, connections_max | Gauge + Time series |
| Query Rate | queries_total | Rate graph |
| Query Latency | query_duration_seconds | Heatmap |
| Buffer Pool | buffer_pool_hit_ratio | Gauge |
| Replication Lag | replication_lag_seconds | Time series per standby |
| Disk I/O | disk_read/write_bytes_total | Rate graph |
| Transactions | transactions_total | Rate by status |
| Locks | locks_held, locks_waiting | Stacked bar |
| Memory | memory_allocated_bytes | Stacked area |
| Top Tables | table_size_bytes | Bar chart |
