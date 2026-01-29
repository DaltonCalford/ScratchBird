# Where We Are Going (Beta)

## Purpose
This document summarizes **planned Beta scope** and how it differentiates
ScratchBird against MySQL, PostgreSQL, and FirebirdSQL in production and
enterprise use cases.

Beta focuses on **cluster capability, replication, data mobility, and
ecosystem integration**, while keeping the Alpha engine foundations intact.

## Beta Pillars and Differentiators

### 1) Cluster + Replication (core Beta)
- **Cluster membership, security, and policy** (SBCLUSTER suite).
- **Distributed MGA** with UUIDv8-HLC conflict resolution.
- **Leaderless quorum replication** and time-partitioned Merkle forests.
- **Schema-aware colocation** and shard-aware routing.
- **Autoscaling and elastic node lifecycle** (SBCLUSTER-12).

**Competitive angle:** built-in replication architecture designed around MGA,
not bolted on later, with policy-driven elasticity as a first-class control
plane. This targets HA/DR parity with enterprise engines while preserving
ScratchBird's snapshot model.

### 2) Distributed Query + Parallel Execution
- Parallel query execution architecture for multi-core and multi-node plans.
- Shard-aware distributed execution and merge.

**Competitive angle:** scalable execution without abandoning the MGA model.

### 3) Data Mobility and Remote DB Integration
- **Remote Database UDRs** for PostgreSQL/MySQL/FirebirdSQL/MSSQL/ODBC/JDBC.
- **Live migration workflows** with passthrough and dual-write validation.
- **Online tablespace migration** plus cross-node shard movement.

**Competitive angle:** migration and coexistence as a first-class feature.

### 4) Multi-Model and NoSQL
- Catalog models for **document, graph, key-value, column-family, time-series**.
- Dedicated language specs (AQL, CQL, Cypher, Gremlin, SPARQL, etc.).
- JSON path indexing and vector/graph-ready storage contracts.
- NoSQL language and storage specs are tracked under:
  - `docs/specifications/beta_requirements/nosql/`
  - `docs/specifications/beta_requirements/nosql/languages/`
  - `docs/specifications/beta_requirements/nosql/NOSQL_STORAGE_STRUCTURES_REPORT.md`

**Competitive angle:** SQL-first engine with multi-model support rather than a
separate NoSQL stack.

### 5) Ecosystem Coverage (drivers/tools/apps)
Beta adds broad integration support:
- **Language drivers** (P0/P1/P2 coverage).
- **ODBC/JDBC** and ORM integrations.
- **BI tools** and application-specific adapters (e.g., Metabase).
- **Cloud/Container** packaging and automation.

**Competitive angle:** broad client compatibility without sacrificing core
engine control.

### 6) Streaming and Event Integration
- **Kafka broadcaster/client** specs for CDC, audit, and DDL event streams.
- Protocol-first integration so external systems can subscribe without drivers.

**Competitive angle:** database-native streaming integration without external
middleware glue.

### 7) Performance and Observability Expansion
- Columnstore batch execution and vectorized pipelines.
- Query store, plan history, and regression tracking.
- Expanded monitoring parity and Prometheus metrics.

**Competitive angle:** enterprise-grade visibility with MGA consistency.

### 8) Security and Operations
- Cluster policy enforcement and node trust model.
- Signed runtime bundles and build verification paths.
- Expanded auditing and security envelopes.
- "Military-grade" cluster security: CA policy, identity, mutual trust,
  and enforcement primitives for multi-node deployments.

**Competitive angle:** security-first operations without external control plane
dependencies.

## Beta Scope References
The Beta plan is defined across these specifications:
- Cluster architecture: `docs/specifications/Cluster Specification Work/`
- Autoscaling: `docs/specifications/Cluster Specification Work/SBCLUSTER-12-AUTOSCALING_AND_ELASTIC_LIFECYCLE.md`
- Replication suite: `docs/specifications/beta_requirements/replication/uuidv7-optimized/`
- Parallel execution: `docs/specifications/query/PARALLEL_EXECUTION_ARCHITECTURE.md`
- Remote DB UDRs: `docs/specifications/remote_database_udr/`
- NoSQL models and languages: `docs/specifications/beta_requirements/nosql/`
- Streaming (Kafka): `docs/specifications/beta_requirements/big-data-streaming/apache-kafka/`
- Drivers/integrations/tools: `docs/specifications/beta_requirements/`
- Online migration/sharding: `docs/specifications/storage/TABLESPACE_ONLINE_MIGRATION.md`
- Cluster security and policy:
  - `docs/specifications/Cluster Specification Work/`
  - `docs/specifications/Cluster Specification Work/SBCLUSTER-03-CA-POLICY.md`
  - `docs/specifications/Cluster Specification Work/SBCLUSTER-04-SECURITY-BUNDLE.md`

## What This Means vs Other Engines
- **Against MySQL/PostgreSQL**: Beta targets built-in replication and
  migration tooling without needing external layers.
- **Against FirebirdSQL**: Beta adds cluster/distributed features while
  retaining MGA and single-file heritage.
- **Against enterprise engines**: Beta aims for HA/DR, observability, and
  integration breadth without proprietary lock-in.
