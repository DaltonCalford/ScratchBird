# Where We Are Going (Beta)

**Last Updated:** February 6, 2026  
**Current Status:** ✅ Alpha Complete, Beta Specifications Ready

---

## Purpose

This document summarizes **planned Beta scope** and how it differentiates
ScratchBird against MySQL, PostgreSQL, and FirebirdSQL in production and
enterprise use cases.

Beta focuses on **cluster capability, replication, data mobility, and
ecosystem integration**, while keeping the Alpha engine foundations intact.

---

## Alpha Completion Summary

Before proceeding to Beta, all Alpha workstreams have been completed:

| Workstream | Status | Lines Added |
|------------|--------|-------------|
| EngineIPCSessionHandler | ✅ Complete | ~3,200 |
| PostgreSQL Parser Agent | ✅ Complete | ~2,800 |
| MySQL Parser Agent | ✅ Complete | ~2,600 |
| Firebird Parser Agent | ✅ Complete | ~2,400 |
| SCRAM-SHA-256/512 Auth | ✅ Complete | ~1,800 |
| Type Mapping System | ✅ Complete | ~2,200 |
| COPY Flow Control | ✅ Complete | ~1,300 |
| Schema Introspection | ✅ Complete | ~1,300 |
| UnixSocketIPCChannel | ✅ Complete | ~1,400 |
| UDR Connectors (69 stubs) | ✅ Complete | ~690 |

**Alpha Results:**
- 84+ NOT_IMPLEMENTED stubs eliminated
- 19,400+ lines of code added
- 3,600+ tests passing (99.8% pass rate)
- 670+ new test cases for Alpha components

---

## Beta Pillars and Differentiators

### 1) Cluster + Replication (core Beta)

Building on the solid Alpha foundation:

- **Cluster membership, security, and policy** (SBCLUSTER suite)
- **Distributed MGA** with UUIDv8-HLC conflict resolution
- **Leaderless quorum replication** and time-partitioned Merkle forests
- **Schema-aware colocation** and shard-aware routing
- **Autoscaling and elastic node lifecycle** (SBCLUSTER-12)

**Competitive angle:** built-in replication architecture designed around MGA,
not bolted on later, with policy-driven elasticity as a first-class control
plane. This targets HA/DR parity with enterprise engines while preserving
ScratchBird's snapshot model.

**Foundation from Alpha:**
- Session management with LRU caching ✅
- IPC infrastructure with flow control ✅
- Authentication and security framework ✅

### 2) Distributed Query + Parallel Execution

- Parallel query execution architecture for multi-core and multi-node plans
- Shard-aware distributed execution and merge
- Query optimization for distributed environments

**Competitive angle:** scalable execution without abandoning the MGA model.

**Foundation from Alpha:**
- Query optimizer with cost-based planning ✅
- SBLR bytecode runtime ✅
- Type system with 140+ conversions ✅

### 3) Data Mobility and Remote DB Integration

- **Remote Database UDRs** for PostgreSQL/MySQL/FirebirdSQL/MSSQL/ODBC/JDBC
- **Live migration workflows** with passthrough and dual-write validation
- **Online tablespace migration** plus cross-node shard movement

**Competitive angle:** migration and coexistence as a first-class feature.

**Foundation from Alpha:**
- UDR connector framework ✅
- PostgreSQL/MySQL/Firebird connectors ✅
- COPY protocol with flow control ✅

### 4) Multi-Model and NoSQL

- Catalog models for **document, graph, key-value, column-family, time-series**
- Dedicated language specs (AQL, CQL, Cypher, Gremlin, SPARQL, etc.)
- JSON path indexing and vector/graph-ready storage contracts
- NoSQL language and storage specs are tracked under:
  - `docs/specifications/beta_requirements/nosql/`
  - `docs/specifications/beta_requirements/nosql/languages/`
  - `docs/specifications/beta_requirements/nosql/NOSQL_STORAGE_STRUCTURES_REPORT.md`

**Competitive angle:** SQL-first engine with multi-model support rather than a
separate NoSQL stack.

**Foundation from Alpha:**
- JSON/JSONB support ✅
- VECTOR type and HNSW/IVF indexes ✅
- Flexible type system ✅

### 5) Ecosystem Coverage (drivers/tools/apps)

Beta adds broad integration support:
- **Language drivers** (P0/P1/P2 coverage) - see ScratchBird-driver repo
- **ODBC/JDBC** and ORM integrations
- **BI tools** and application-specific adapters (e.g., Metabase)
- **Cloud/Container** packaging and automation

**Competitive angle:** broad client compatibility without sacrificing core
engine control.

**Foundation from Alpha:**
- Full wire protocol compatibility ✅
- Type mapping system ✅
- Schema introspection ✅

### 6) Streaming and Event Integration

- **Kafka broadcaster/client** specs for CDC, audit, and DDL event streams
- Protocol-first integration so external systems can subscribe without drivers

**Competitive angle:** database-native streaming integration without external
middleware glue.

**Foundation from Alpha:**
- Audit logging (20+ event types) ✅
- Cryptographic audit chain ✅
- IPC messaging infrastructure ✅

### 7) Performance and Observability Expansion

- Columnstore batch execution and vectorized pipelines
- Query store, plan history, and regression tracking
- Expanded monitoring parity and Prometheus metrics
- Performance benchmarking framework

**Competitive angle:** enterprise-grade visibility with MGA consistency.

**Foundation from Alpha:**
- 14 index types including Columnstore ✅
- Monitoring tables (sys.*) ✅
- LRU statement caching ✅

### 8) Security and Operations

- Cluster policy enforcement and node trust model
- Signed runtime bundles and build verification paths
- Expanded auditing and security envelopes
- "Military-grade" cluster security: CA policy, identity, mutual trust,
  and enforcement primitives for multi-node deployments

**Competitive angle:** security-first operations without external control plane
dependencies.

**Foundation from Alpha:**
- SCRAM-SHA-256/512 authentication ✅
- TLS 1.3 support ✅
- RLS/CLS enforcement ✅
- Encryption at-rest and in-transit ✅

---

## Beta Implementation Phases

### Phase 1: Foundation (Months 1-2)
- Cluster membership and node discovery
- Basic Raft consensus implementation
- mTLS infrastructure

### Phase 2: Replication (Months 2-4)
- Leaderless quorum replication
- WAL streaming
- Conflict resolution with UUIDv8-HLC

### Phase 3: Distribution (Months 4-6)
- Sharding implementation
- Distributed query execution
- Cross-shard transactions

### Phase 4: Integration (Months 6-8)
- Driver ecosystem completion
- BI tool integrations
- Cloud packaging

### Phase 5: Optimization (Months 8-10)
- Performance tuning
- Observability expansion
- Production hardening

---

## Beta Scope References

The Beta plan is defined across these specifications:

### Cluster and Replication
- Cluster architecture: `docs/specifications/Cluster Specification Work/`
- Autoscaling: `docs/specifications/Cluster Specification Work/SBCLUSTER-12-AUTOSCALING_AND_ELASTIC_LIFECYCLE.md`
- Replication suite: `docs/specifications/beta_requirements/replication/uuidv7-optimized/`
- Security: `docs/specifications/Cluster Specification Work/SBCLUSTER-03-CA-POLICY.md`

### Query and Execution
- Parallel execution: `docs/specifications/query/PARALLEL_EXECUTION_ARCHITECTURE.md`

### Data Mobility
- Remote DB UDRs: `docs/specifications/remote_database_udr/`
- Online migration: `docs/specifications/storage/TABLESPACE_ONLINE_MIGRATION.md`

### Multi-Model
- NoSQL models and languages: `docs/specifications/beta_requirements/nosql/`

### Streaming
- Kafka: `docs/specifications/beta_requirements/big-data-streaming/apache-kafka/`

### Drivers and Tools
- Drivers/integrations/tools: `docs/specifications/beta_requirements/`

---

## What This Means vs Other Engines

### Against MySQL/PostgreSQL

Beta targets built-in replication and migration tooling without needing external layers:

| Feature | MySQL/PostgreSQL | ScratchBird Beta |
|---------|------------------|------------------|
| Replication | External tools (Galera, Patroni) | Built-in leaderless quorum |
| Sharding | External (Vitess, Citus) | Native shard-aware routing |
| CDC | External (Debezium) | Native Kafka integration |
| Security | Add-on | Built-in military-grade |

### Against FirebirdSQL

Beta adds cluster/distributed features while retaining MGA and single-file heritage:

| Feature | FirebirdSQL | ScratchBird Beta |
|---------|-------------|------------------|
| Clustering | Limited | Full distributed MGA |
| Replication | Async only | Leaderless quorum |
| Modern protocols | N/A | PostgreSQL/MySQL wire compatible |
| Cloud-native | No | Container-first design |

### Against Enterprise Engines

Beta aims for HA/DR, observability, and integration breadth without proprietary lock-in:

| Feature | Oracle/DB2/SQL Server | ScratchBird Beta |
|---------|----------------------|------------------|
| Cost | $$$$ | Free (IPL 1.0) |
| Lock-in | High | Open source |
| MGA | N/A | Native |
| Multi-model | Separate products | Integrated |

---

## Success Metrics for Beta

| Metric | Target |
|--------|--------|
| Cluster nodes | 3-16 node clusters |
| Replication lag | < 100ms |
| Failover time | < 5 seconds |
| Shard rebalancing | Online, zero-downtime |
| Driver coverage | 15+ languages |
| Test pass rate | > 98% |

---

## References

- **Alpha Completion:** `../ALPHA_COMPLETION_REPORT.md`
- **Alpha Summary:** `../ALPHA_COMPLETION_SUMMARY_2026-02-06.md`
- **Official Roadmap:** `../OFFICIAL_ROADMAP.md`
- **Implementation Dashboard:** `IMPLEMENTATION_STATUS_DASHBOARD.md`
- **Cluster Specs:** `specifications/Cluster Specification Work/SBCLUSTER-SUMMARY.md`
