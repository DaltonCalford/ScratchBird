# Operations & Monitoring Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains operational monitoring and observability specifications for ScratchBird.

## Overview

ScratchBird provides comprehensive operational monitoring with Prometheus metrics, health checks, and observability features for production deployments.

## Specifications in this Directory

- **[PROMETHEUS_METRICS_REFERENCE.md](PROMETHEUS_METRICS_REFERENCE.md)** (824 lines) - Complete Prometheus metrics reference
- **[LISTENER_POOL_METRICS.md](LISTENER_POOL_METRICS.md)** - Listener and parser pool metrics
- **[MONITORING_SQL_VIEWS.md](MONITORING_SQL_VIEWS.md)** - SQL-visible monitoring views for sessions/locks/statements/perf
- **[MONITORING_DIALECT_MAPPINGS.md](MONITORING_DIALECT_MAPPINGS.md)** - Column-level mapping to pg_stat_*/MON$/performance_schema
- **[OID_MAPPING_STRATEGY.md](OID_MAPPING_STRATEGY.md)** - PostgreSQL OID mapping strategy

## Key Features

### Metrics Categories

- **Database Metrics** - Connection count, transaction rate, query latency
- **Storage Metrics** - Buffer pool hit rate, disk I/O, page reads/writes
- **Index Metrics** - Index usage, index scans, index size
- **Transaction Metrics** - Transaction duration, commit/rollback rate, snapshot age
- **Query Metrics** - Query execution time, slow queries, query cache hit rate
- **Replication Metrics** - Replication lag, streaming status (Beta)
- **Cluster Metrics** - Node health, Raft status, shard distribution (Beta)

### Prometheus Integration

ScratchBird exports metrics in Prometheus format:

```
# HELP scratchbird_connections_total Total number of client connections
# TYPE scratchbird_connections_total counter
scratchbird_connections_total{database="mydb"} 1234

# HELP scratchbird_buffer_pool_hit_ratio Buffer pool cache hit ratio
# TYPE scratchbird_buffer_pool_hit_ratio gauge
scratchbird_buffer_pool_hit_ratio 0.95

# HELP scratchbird_transaction_duration_seconds Transaction duration in seconds
# TYPE scratchbird_transaction_duration_seconds histogram
scratchbird_transaction_duration_seconds_bucket{le="0.001"} 100
scratchbird_transaction_duration_seconds_bucket{le="0.01"} 500
```

### Health Checks

- **Liveness probe** - Server is running
- **Readiness probe** - Server is ready to accept connections
- **Health endpoint** - HTTP health check endpoint

## Related Specifications

- [Deployment](../deployment/) - Deployment integration
- [Admin](../admin/) - Administration tools
- [Cluster Observability](../Cluster%20Specification%20Work/SBCLUSTER-10-OBSERVABILITY.md) - Cluster-specific observability (Beta)

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
