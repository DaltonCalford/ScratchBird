# Final Target for Gold Release

## Purpose
Provide a **technical marketing** overview of the **Gold target**: the full
union of **Alpha** (core engine) and **Beta** (cluster, replication, ecosystem)
features as specified. Gold is the point where ScratchBird is a complete,
production-grade successor to Firebird/InterBase with modern scale-out and
integration coverage.

## Gold Definition
Gold is achieved when:
- All Alpha and Beta specifications are implemented and tested.
- Engine, parsers, and ecosystem integrations ship as a cohesive platform.
- Performance, security, and observability targets are met for production use.

## Executive Summary (Gold Differentiators)
- **MGA core without mandatory WAL** for fast commits and non-blocking reads.
- **SBLR bytecode VM** with untrusted parser model and UUID-based object IDs.
- **Multi-dialect parity** (native + FirebirdSQL + PostgreSQL + MySQL) via
  strict parser isolation.
- **Cluster and replication built around MGA**, not bolted-on MVCC.
- **Elastic autoscaling and node lifecycle** as first-class cluster behavior.
- **Multi-model and NoSQL support** as a first-class extension of SQL.
- **Migration-first tooling** for live legacy cutovers and dual-write auditing.
- **Streaming integration (Kafka)** for CDC, audit, and operational events.
- **Enterprise-grade security** from single-node to multi-node deployments.

## Alpha Feature Set (Core Engine)

### Transaction and Concurrency
- Multi-Generational Architecture (MGA) with snapshot visibility.
- Fast commit, reduced reader/writer blocking.
- Sweep/GC with explicit SWEEP semantics (no WAL dependency).
- Strict transaction/session controls and lock management.

### Storage, Catalog, and Indexing
- Heap storage with TOAST support for varlen payloads.
- Catalog persistence for schemas, tables, columns, indexes, permissions.
- Index families implemented across common OLTP and analytics patterns.
- Filespaces/tablespaces with online migration foundations.

### Type System and Domain Enforcement
- Rich core type system: numeric, temporal, binary, JSON, XML, spatial, arrays.
- Domains as first-class objects with constraints, masking, and validation.
- UUID-based identity with deterministic object references.

### SBLR Execution Model
- SQL compiled to SBLR; engine verifies and executes SBLR, not raw SQL.
- Shared SBLR cache across connections for repeat workloads.
- Designed for future native compilation of hot PSQL routines.

### Security (Single Database)
- Authentication providers and SCRAM support.
- GRANT/REVOKE and column-level permission enforcement in executor.
- Row-level security policy hooks and domain-level safeguards.

### Deployment Models
- Embedded (single-user, in-process).
- Local shared server (IPC).
- Network server (INET).

### Operational Tooling (Alpha)
- Native CLI tools: sb_isql, sb_admin, sb_backup, sb_security, sb_verify.
- Monitoring and system views for runtime inspection.
- Job scheduler with cron support and audit/security hooks.
- Integrated Git support for schema and metadata versioning.

## Beta Feature Set (Scale, Ecosystem, and Multi-Model)

### Cluster and Replication (Core Beta)
- Cluster membership, identity, and CA policy enforcement.
- Distributed MGA with UUIDv8-HLC conflict resolution.
- Leaderless quorum replication and time-partitioned Merkle forests.
- Schema-aware colocation and shard-aware routing.
- Autoscaling and elastic node lifecycle with policy-driven scale out/in.

### Distributed Query and Parallel Execution
- Parallel execution architecture for multi-core and multi-node plans.
- Distributed query planning and execution across shards.

### Remote Database UDRs and Live Migration
- UDR connectors for PostgreSQL, MySQL, FirebirdSQL, MSSQL, ODBC/JDBC.
- Pass-through queries and schema introspection.
- Live migration workflow: emulate, dual-write, audit, cutover, archive.

### Streaming and Event Integration
- Kafka broadcaster/client for CDC, audit, and DDL event streams.
- Protocol-first integration to avoid external middleware dependency.

### Multi-Model and NoSQL
- Catalog models for document, graph, key-value, column-family, time-series.
- Dedicated language coverage (AQL, CQL, Cypher, Gremlin, SPARQL, etc.).
- JSON path indexing and vector/graph-ready storage contracts.

### Ecosystem Coverage
- Language drivers (P0/P1/P2) and standard protocols (ODBC/JDBC).
- ORM and framework integrations.
- Tooling and BI integrations (Metabase, DBeaver, pgAdmin, etc.).
- Cloud and container packaging (Docker, Kubernetes, Helm, Terraform).

### Performance, Observability, and Ops
- Columnstore batch execution and vectorized pipelines.
- Query store and regression tracking.
- Expanded monitoring parity and Prometheus metrics.
- Signed runtime bundles and secure build chains.

### Security (Cluster and Enterprise)
- Military-grade cluster security model with trust bundles and enforcement.
- Policy-driven identity, auditability, and operational controls.

## Competitive Positioning (Gold)

### Versus FirebirdSQL
- Preserves MGA strengths while adding modern SBLR, multi-dialect emulation,
  cluster replication, and ecosystem integrations.
- Designed as a clean successor, not a patch on legacy architecture.

### Versus PostgreSQL
- Eliminates mandatory WAL dependency in core paths.
- Bytecode VM with engine-verified execution.
- Built-in migration tooling and emulation without external layers.

### Versus MySQL
- Strong transactional core with MGA semantics and strict engine validation.
- Robust schema, domain, and policy enforcement beyond typical MySQL defaults.

## Gold Deliverables (Technical Marketing View)
- A production-grade, multi-dialect database engine with Firebird heritage.
- Full parity for Alpha and Beta scopes with documented, verifiable behavior.
- Enterprise-ready security, observability, and deployment coverage.
- A complete migration path for legacy databases and mixed environments.

## Specification Pointers
- Alpha scope and implementation tracking:
  - `docs/planning/ALPHA_COMPLETION_MASTER_PLAN.md`
  - `docs/findings/ALPHA_BETA_SCOPE_STATUS.md`
- Beta scope:
  - `docs/specifications/beta_requirements/README.md`
  - `docs/specifications/Cluster Specification Work/`
  - `docs/specifications/Cluster Specification Work/SBCLUSTER-12-AUTOSCALING_AND_ELASTIC_LIFECYCLE.md`
  - `docs/specifications/beta_requirements/replication/uuidv7-optimized/`
  - `docs/specifications/beta_requirements/big-data-streaming/apache-kafka/`
  - `docs/specifications/beta_requirements/nosql/`
