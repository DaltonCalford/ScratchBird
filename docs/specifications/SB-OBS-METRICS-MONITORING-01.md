# SB-OBS-METRICS-MONITORING-01
## ScratchBird Observability Specification
### Metrics, Monitoring, Introspection, and Alerting Baseline

**Status:** Draft – Beta Observability Baseline  
**Applies To:** ScratchBird Engine (Native v3), Drivers, Cluster (SB-CLUSTER-SWS-MGA-01), Migration/Compatibility Layer  

---

# 1. Purpose

This document defines the **mandatory observability requirements** for ScratchBird.

It standardizes:

- Metrics naming and cardinality rules
- Export interfaces (OpenMetrics + SQL views + event logs)
- Required metric sets for engine, cluster, auth, drivers, storage, and migration
- Health checks and readiness signals
- Baseline alert rules (reference set)

This specification is intended to:

- Enable production-grade debugging and operational confidence
- Prevent inconsistent ad-hoc instrumentation
- Provide deterministic insight into cluster correctness (fencing, replication, MGA horizons)
- Support beta tester evaluation

---

# 2. Observability Surfaces

ScratchBird MUST expose observability through three canonical surfaces.

## 2.1 OpenMetrics / Prometheus Endpoint

- An HTTP endpoint MUST exist (default: `/metrics`).
- Output MUST conform to OpenMetrics or Prometheus text format.
- Endpoint MUST support scraping without authentication in developer mode and SHOULD support authentication/ACL in production.

## 2.2 SQL Introspection Views

- ScratchBird MUST expose system views under `sys.metrics.*` and `sys.cluster.metrics.*`.
- Views MUST be queryable using native v3 and emulated protocols.
- Views MUST be stable across minor releases.

## 2.3 Structured Event Streams

- Security and operationally significant events MUST be emitted to:
  - Audit log (durable)
  - Optional structured event stream (JSON lines)
- Events MUST contain stable IDs and epoch context.

---

# 3. Global Naming and Cardinality Rules

## 3.1 Metric Naming

- All metrics MUST be prefixed with `sb_`.
- Subsystem second token MUST be one of:
  - `engine`, `cluster`, `auth`, `storage`, `driver`, `migration`, `udr`, `net`, `cache`, `planner`, `exec`.

## 3.2 Metric Types

- Counters MUST end in `_total`.
- Gauges SHOULD avoid suffixes.
- Histograms MUST end in `_seconds`, `_bytes`, or `_rows`.

## 3.3 Label Cardinality Policy (Mandatory)

Allowed labels:
- `db` (db_uuid or db_alias, configurable)
- `shard`
- `node`
- `protocol` (native, pg, mysql, fb)
- `driver` (go, python, jdbc, odbc, etc.)
- `result` (ok, error)
- `reason` (low-cardinality enum)

Forbidden labels (default):
- raw SQL text
- session_id
- user_id
- client_ip (unless explicitly enabled)
- object UUIDs (table_id/index_id) unless aggregated

---

# 4. Health, Readiness, and Liveness

ScratchBird MUST support:

## 4.1 Liveness
- Process running.
- Event loop responding.

## 4.2 Readiness
- Database opened.
- Catalog available.
- Cluster epoch loaded.
- Listener/parser pool available.

## 4.3 Cluster Readiness
- Control-plane log reachable.
- Leader leases valid.
- Shard map loaded.

Readiness MUST be exposed as:
- `/healthz` (liveness)
- `/readyz` (readiness)
- `sys.metrics.health_status` view

---

# 5. Required Metrics by Subsystem

This section defines mandatory metrics.

---

## 5.1 Engine Metrics

### Connections and Sessions
- `sb_engine_connections_active` (gauge)
- `sb_engine_connections_total` (counter)
- `sb_engine_sessions_active` (gauge)
- `sb_engine_sessions_total` (counter)

### Query Execution
- `sb_engine_queries_total{result}` (counter)
- `sb_engine_query_duration_seconds` (histogram)
- `sb_engine_rows_returned_total` (counter)
- `sb_engine_rows_affected_total` (counter)

### Bytecode
- `sb_engine_bytecode_validations_total{result}` (counter)
- `sb_engine_bytecode_validation_seconds` (histogram)

### Errors
- `sb_engine_errors_total{sqlstate}` (counter, sqlstate must be bucketed to low-cardinality classes)

---

## 5.2 Authentication and Security Metrics

- `sb_auth_success_total{method}` (counter)
- `sb_auth_failure_total{method,reason}` (counter)
- `sb_auth_lockouts_total{reason}` (counter)
- `sb_auth_active_tokens` (gauge)

If token auth exists:
- `sb_auth_token_issued_total{scope}` (counter)
- `sb_auth_token_revoked_total{reason}` (counter)

---

## 5.3 Cluster Metrics (Single Writer per Shard)

### Leadership / Fencing
- `sb_cluster_leader_term{db,shard}` (gauge)
- `sb_cluster_lease_seconds_remaining{db,shard}` (gauge)
- `sb_cluster_fencing_rejections_total{db,shard,reason}` (counter)

### Routing
- `sb_cluster_routing_requests_total{db,protocol,result}` (counter)
- `sb_cluster_routing_epoch{db}` (gauge)

### Replication
- `sb_cluster_replication_lag_txn{db,shard}` (gauge)
- `sb_cluster_replication_lag_seconds{db,shard}` (gauge)
- `sb_cluster_replication_apply_total{db,shard,result}` (counter)
- `sb_cluster_replication_apply_seconds{db,shard}` (histogram)

### MGA Horizons
- `sb_cluster_cwm_txn{db,shard}` (gauge)
- `sb_cluster_ost_txn{db,shard}` (gauge)
- `sb_cluster_rwm_txn{db,shard}` (gauge)
- `sb_cluster_gc_safe_horizon_txn{db,shard}` (gauge)

### Snapshot Registry
- `sb_cluster_snapshots_active{db,shard}` (gauge)
- `sb_cluster_snapshot_heartbeats_total{db,shard}` (counter)

---

## 5.4 Storage Metrics

### Buffer / IO
- `sb_storage_buffer_pool_pages` (gauge)
- `sb_storage_buffer_pool_hit_total` (counter)
- `sb_storage_buffer_pool_miss_total` (counter)
- `sb_storage_page_reads_total` (counter)
- `sb_storage_page_writes_total` (counter)
- `sb_storage_io_seconds{op}` (histogram)

### MGA Versioning
- `sb_storage_record_versions_created_total` (counter)
- `sb_storage_record_versions_reclaimed_total` (counter)

### Sweep / GC
- `sb_storage_sweep_runs_total{result}` (counter)
- `sb_storage_sweep_seconds` (histogram)
- `sb_storage_gc_blocked_seconds_total{reason}` (counter)

---

## 5.5 Driver and Protocol Metrics

### Per-driver
- `sb_driver_connections_total{driver,result}` (counter)
- `sb_driver_query_total{driver,result}` (counter)
- `sb_driver_tls_handshakes_total{driver,result}` (counter)

### Wire-level
- `sb_net_bytes_in_total{protocol}` (counter)
- `sb_net_bytes_out_total{protocol}` (counter)
- `sb_net_messages_total{protocol,type}` (counter)

---

## 5.6 Cache and Compilation Metrics

- `sb_cache_translation_hits_total{dialect}` (counter)
- `sb_cache_translation_misses_total{dialect}` (counter)
- `sb_cache_statement_hits_total{driver}` (counter)
- `sb_cache_statement_misses_total{driver}` (counter)

---

## 5.7 Migration / Dual-Write Metrics

For live migration and pass-through:

- `sb_migration_passthrough_queries_total{dialect,type,result}` (counter)
- `sb_migration_mirrored_writes_total{dialect,result}` (counter)
- `sb_migration_mirror_lag_seconds{dialect}` (gauge)
- `sb_migration_validation_mismatches_total{dialect,reason}` (counter)
- `sb_migration_cutover_events_total{dialect}` (counter)

---

# 6. SQL Introspection Views

ScratchBird MUST expose the following views.

## 6.1 `sys.metrics.runtime`
Columns:
- `metric_name`
- `metric_type`
- `value`
- `labels_json`
- `updated_at`

## 6.2 `sys.metrics.health`
Columns:
- `component`
- `status` (OK/WARN/FAIL)
- `message`
- `updated_at`

## 6.3 `sys.cluster.metrics.shards`
Columns:
- `db_uuid`
- `shard_id`
- `leader_node_id`
- `leader_term`
- `lease_expires_at`
- `cwm_txn`
- `ost_txn`
- `rwm_txn`
- `gc_safe_txn`
- `replication_lag_txn`
- `replication_lag_seconds`

## 6.4 `sys.cluster.metrics.snapshots`
Columns:
- `session_id`
- `db_uuid`
- `shard_id`
- `snapshot_boundary`
- `start_time`
- `last_heartbeat`

---

# 7. Structured Events

The following MUST emit structured events:

- Cluster leadership changes
- Fencing token rejection
- Node join/rejoin
- Replication apply failure
- GC blocked due to snapshot
- Migration validation mismatch
- Cutover approved

Event schema MUST include:
- `event_type`
- `event_id` (UUIDv7)
- `timestamp`
- `db_uuid`
- `node_id`
- `shard_id` (optional)
- `cluster_config_epoch`
- `schema_epoch`
- `security_epoch`
- `details_json`

---

# 8. Alerting Reference Rules (Baseline)

This section defines recommended alerts.

## 8.1 Leader Lease Low
Trigger if `sb_cluster_lease_seconds_remaining < 5`.

## 8.2 Fencing Rejections Spike
Trigger if `rate(sb_cluster_fencing_rejections_total[5m]) > threshold`.

## 8.3 Replication Lag
Trigger if `sb_cluster_replication_lag_seconds > threshold`.

## 8.4 Snapshot Stuck
Trigger if `sb_cluster_snapshots_active > 0` AND `sb_cluster_gc_safe_horizon_txn` not advancing.

## 8.5 Auth Failure Spike
Trigger if `rate(sb_auth_failure_total[5m]) > threshold`.

---

# 9. Acceptance Criteria

ScratchBird observability is considered beta-ready when:

1. OpenMetrics endpoint exports all mandatory metrics.
2. SQL introspection views exist and match schema.
3. Key cluster safety metrics (fencing, horizons, lag) are present.
4. Structured events emit for major state transitions.
5. Reference alert rules can be applied without high-cardinality labels.

---

# 10. Non-Goals

- Full distributed tracing spec (future)
- Per-query SQL text metrics (forbidden by default)
- High-cardinality per-object metrics

---

# 11. Conclusion

This specification establishes a consistent, safe observability baseline for ScratchBird beta, ensuring that:

- Cluster correctness is measurable
- MGA GC safety is visible
- Migration correctness is auditable
- Driver and protocol health is verifiable

This document is authoritative for instrumentation and monitoring requirements.

