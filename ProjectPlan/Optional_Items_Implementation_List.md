# Optional and Deferred Items — Implementation List

This document consolidates optional, deferred, or stretch-scope items identified across project phases. Use it to plan follow-on implementation work after core completion.

## Phase 9 — Advanced Index Families (Future Enhancements)

- Bloom filters for LSM-Tree false-positive reduction
- Parallel compaction for LSM-Tree
- Additional compression codecs (Zstd, Snappy) for Columnstore
- Online index rebuild (zero-downtime)
- Index partitioning and partition-wise operations
- Automatic index selection recommendations (ML-assisted)
- Cross-index/global statistics and cardinality models

## Phase 10 — PostgreSQL FDW (Optional Auth Extensions)

- Certificate-based authentication for libpq connections
- Kerberos (GSSAPI) authentication (basic)

## Phase 11 — Server/Protocol/Auth (Advanced/Deferred)

- TLS advanced features: client cert auth, CRL checking, PFS, session resumption
- Role attributes and deep security context management (RLS enforcement, BYPASSRLS semantics)
- Server monitoring/diagnostics: per-connection metrics, profiling, tracing surfaces
- Administrative interfaces: start/stop/restart, config reload, connection management commands

### Phase 11.7 — Performance and Scalability (Not Yet Implemented)

- Server-side connection pool (process/thread-based), health checks, dynamic sizing
- Network buffer configuration and auto-tuning
- Shared buffer pool with replacement policy (clock-sweep), statistics, observability
- Background writer for dirty page flushing with batching and scheduling
- Fast-path locks and partitioned lock manager
- Wire protocol compression with negotiation and adaptive levels
- Network message batching and flow control
- Adaptive plan cache with generic vs custom plans and invalidation policies

## Phase 12 — Backup/Restore and PITR

- Executable backup/restore operations (beyond parser acceptance)
- PITR tooling and WAL integration for to-now recovery
- SHOW BACKUP HISTORY metadata and utilities

## Phase 13 — Logical Replication

- WAL shipper (file/remote/Kafka), batching and compression
- Idempotent replayer with checkpoints and consistency markers
- Publications/subscriptions DDL execution and control (pause/resume)

## Phase 14 — Tablespaces and Secondary Files

- CREATE/ALTER/DROP TABLESPACE execution
- Object placement/move tooling and rebalance flows

## Phase 16 — Security and RLS

- GRANT/REVOKE full lifecycle coverage
- RLS policies (USING/WITH CHECK; FORCE) enforcement
- Metadata visibility vs operation permissions separation

## Phase 17 — JSON, Spatial, Collations

- Native JSON/JSONB types and operators
- Deterministic collations via ICU surfaces
- Spatial types and ST_* operations (library/extension integration)

## Phase 18 — Partitioning and Materialized Views

- RANGE/LIST/HASH partitioning with pruning
- Global/local index support on partitions
- CREATE/REFRESH MATERIALIZED VIEW (on-demand/incremental where possible)

## Phase 19 — Tooling and UX

- isql meta-commands: SHOW HEADER, EXPLAIN [ANALYZE], ANALYZE, VACUUM, admin surfaces
- Admin CLIs: backup/restore, analyze/vacuum, trace/audit, replication, index tools
- Perf microbenchmarks and catalog inspectors

## Phase 20 — QA, Perf Gates, Hardening

- Concurrency/soak tests; fuzzing and fault-injection; chaos testing (WAL/replication)
- Performance CI gates with baselines; memory/CPU regression detection

## Phase 21 — Packaging and Documentation

- Packages/containers and configuration samples
- Migration/compatibility guides, API docs, Doxygen
- Admin/operations manuals and quickstart

## Cross-Cutting

- Telemetry/monitoring counters and MON$*/RDB$* compatibility views expansion
- Config knob surfaces for I/O, cache, prefetch, WAL, planner
- Error code standardization and diagnostics unification

