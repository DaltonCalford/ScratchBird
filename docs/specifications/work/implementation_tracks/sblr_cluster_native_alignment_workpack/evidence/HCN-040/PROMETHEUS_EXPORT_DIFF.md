# Prometheus Export Diff - HCN-040

The telemetry registry now includes canonical SB-OBS metric families (`sb_engine_*`, `sb_cluster_*`) in addition to legacy `scratchbird_*` metrics.

Observed canonical additions (representative):
- `sb_engine_queries_total`
- `sb_engine_query_duration_seconds`
- `sb_engine_connections_active`
- `sb_cluster_leader_term`
- `sb_cluster_replication_apply_total`
- `sb_cluster_gc_safe_horizon_txn`

Migration behavior:
- legacy metrics are not removed in this phase;
- canonical `sb_*` metrics are registered in parallel to support controlled migration.
