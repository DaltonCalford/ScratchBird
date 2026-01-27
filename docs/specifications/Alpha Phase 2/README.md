# Alpha Phase 2 Specifications

**[← Back to Specifications Index](../README.md)**

**Version:** 1.0
**Date:** 2025-01-25
**Status:** Draft for Review

---

## Overview

This directory contains Alpha Phase 2 technical specifications for a distributed, multi-dialect SQL database engine. The system provides wire protocol compatibility with PostgreSQL, MySQL, Firebird, and MSSQL (Beta scope) while implementing a modern three-tier architecture for optimal OLTP and OLAP performance.

**Note:** These specifications represent an alternative architectural approach to distributed operation. The primary cluster architecture for Beta is documented in [Cluster Specification Work](../Cluster%20Specification%20Work/).
**Scope Note:** MSSQL/TDS support is part of Beta scope; mentions here are aligned with that requirement.

---

## Document Structure

### Core Specifications

**[00-Implementation-Roadmap.md](./00-Implementation-Roadmap.md)**  
*Start here* - Executive summary, phased implementation plan, testing strategy, and deployment guidelines. Provides the big picture and coordinates all other specifications.

**[01-Architecture-Overview.md](./01-Architecture-Overview.md)**  
High-level system architecture, component relationships, data flow, three-tier storage model, and design rationale. Essential reading for understanding the overall system design.

**[02-Clock-Synchronization-Specification.md](./02-Clock-Synchronization-Specification.md)**  
Detailed specification of the cluster-wide clock heartbeat service, uncertainty tracking, failover mechanisms, and cloud provider integrations (AWS/GCP/Azure). Critical for understanding globally ordered timestamps.

**[03-Distributed-MVCC-Specification.md](./03-Distributed-MVCC-Specification.md)**  
Complete MVCC implementation using UUID v7 versioning, transaction lifecycle, conflict resolution strategies, garbage collection, and provenance tracking. Core to understanding data consistency.

**[04-Replication-Protocol-Specification.md](./04-Replication-Protocol-Specification.md)**  
Asynchronous replication from TX engines to Ingestion tier, change data capture format, conflict detection/resolution, and OLAP batch building. Essential for multi-tier data flow.

**[05-Wire-Protocol-Integration-Specification.md](./05-Wire-Protocol-Integration-Specification.md)**  
Wire protocol handlers for PostgreSQL, MySQL, Firebird, and MSSQL/TDS (Beta scope). Parser plugin API, authentication integration, system catalog virtualization, and session management.

### Data Tier Components

**[06-Ingestion-Layer.md](./06-Ingestion-Layer.md)** *(NEW)*  
Change stream subscription, merge queue and ordering, UUID-based deduplication, conflict detection and resolution (LWW, CRDT, custom), data validation and transformation, OLAP batch building, and garbage collection strategies.

**[07-OLAP-Tier.md](./07-OLAP-Tier.md)** *(NEW)*  
Columnar storage format (Parquet), compression and encoding strategies, time-based partitioning and pruning, sharding strategies (time, geography, tenant), vectorized query execution, materialized views, indexing (zone maps, bitmap, inverted), compaction, and tiered storage.

### Operations & Deployment

**[08-Deployment-Guide.md](./08-Deployment-Guide.md)** *(NEW)*  
AWS deployment (Terraform, EC2, MSK, S3), GCP deployment (Deployment Manager, Compute Engine, Pub/Sub), self-hosted deployment (hardware specs, Ansible), hardware clock setup (GPS receivers), configuration management, and operational procedures (bootstrap, scaling, rolling updates, backup/recovery).

**[09-Monitoring-Observability.md](./09-Monitoring-Observability.md)** *(NEW)*  
Prometheus metrics by component, alerting rules (latency, lag, conflicts, clock sync), Grafana dashboards, structured JSON logging (Loki), distributed tracing (OpenTelemetry/Jaeger), performance profiling (Pyroscope), operational runbooks, and health check procedures.

### Extensibility & Migration

**[10-UDR-System-Specification.md](./10-UDR-System-Specification.md)** *(NEW)*  
UDR (User Defined Routine) plugin architecture inspired by Firebird, plugin lifecycle management, function/procedure/trigger support, message buffers and type system, memory management and safety, security sandboxing, and example implementations.

**[11-Remote-Database-UDR-Specification.md](./11-Remote-Database-UDR-Specification.md)** *(NEW)*  
**[11h-Live-Migration-Emulated-Listener.md](./11h-Live-Migration-Emulated-Listener.md)** *(NEW)*  
**[11i-ScratchBird-Client-Implementation.md](./11i-ScratchBird-Client-Implementation.md)** *(NEW)*  
Remote database connection plugin for zero-downtime migration, foreign table support (Foreign Data Wrapper pattern), protocol adapters (PostgreSQL, MySQL, Firebird, MSSQL) plus ODBC/JDBC connectors, connection pool management, query pushdown optimization, schema introspection, and hybrid query execution.

**Protocol Adapter Implementations:**
- [11a-Connection-Pool-Implementation.md](./11a-Connection-Pool-Implementation.md) - Connection pooling system
- [11b-PostgreSQL-Client-Implementation.md](./11b-PostgreSQL-Client-Implementation.md) - PostgreSQL adapter (native protocol)
- [11c-MySQL-Client-Implementation.md](./11c-MySQL-Client-Implementation.md) - MySQL adapter (native protocol)
- [11d-MSSQL-Client-Implementation.md](./11d-MSSQL-Client-Implementation.md) - MSSQL adapter (native TDS)
- [11e-Firebird-Client-Implementation.md](./11e-Firebird-Client-Implementation.md) - Firebird adapter (native protocol)
- [11f-ODBC-Client-Implementation.md](./11f-ODBC-Client-Implementation.md) - ODBC connector (embedded drivers)
- [11g-JDBC-Client-Implementation.md](./11g-JDBC-Client-Implementation.md) - JDBC connector (embedded drivers)

---

## Reading Guide

### For Executive Leadership
**Recommended sequence:**
1. [Implementation Roadmap](./00-Implementation-Roadmap.md) - Executive Summary, Success Criteria, Risk Mitigation
2. [Architecture Overview](./01-Architecture-Overview.md) - System Overview, Deployment Topologies

**Key takeaways:**
- Drop-in replacement for major databases (PostgreSQL, MySQL, Firebird, MSSQL)
- Three-tier architecture separates OLTP and OLAP workloads
- 12-month implementation timeline in 4 phases
- Scales horizontally to 100+ nodes

---

### For Architects
**Recommended sequence:**
1. [Architecture Overview](./01-Architecture-Overview.md) - Complete system design
2. [Clock Synchronization](./02-Clock-Synchronization-Specification.md) - Distributed time coordination
3. [Distributed MVCC](./03-Distributed-MVCC-Specification.md) - Version control and consistency
4. [Replication Protocol](./04-Replication-Protocol-Specification.md) - Data flow between tiers
5. [Wire Protocol Integration](./05-Wire-Protocol-Integration-Specification.md) - Client compatibility

**Key decisions to review:**
- UUID v7 + server_id eliminates distributed TID coordination
- Cluster heartbeat provides bounded clock uncertainty
- Per-protocol processes enable independent scaling
- Message broker (Kafka/NATS) for replication backbone

---

### For Infrastructure Engineers
**Recommended sequence:**
1. [Implementation Roadmap](./00-Implementation-Roadmap.md) - Deployment topologies, operational procedures
2. [Clock Synchronization](./02-Clock-Synchronization-Specification.md) - Time service deployment
3. [Architecture Overview](./01-Architecture-Overview.md) - Component placement and scaling

**Focus areas:**
- Clock master HA configuration (GPS vs NTP)
- Message broker cluster setup (Kafka/NATS)
- Network topology (dedicated clock network, QoS)
- Monitoring and alerting configuration
- Disaster recovery procedures

---

### For Database Engineers
**Recommended sequence:**
1. [Distributed MVCC](./03-Distributed-MVCC-Specification.md) - Complete read first
2. [Replication Protocol](./04-Replication-Protocol-Specification.md) - CDC and conflict resolution
3. [Architecture Overview](./01-Architecture-Overview.md) - Storage tier details
4. [Clock Synchronization](./02-Clock-Synchronization-Specification.md) - Timestamp generation

**Implementation priorities:**
- MVCC visibility rules and garbage collection
- Transaction commit protocols (with commit-wait)
- Version chain management
- Conflict resolution strategies

---

### For Protocol Engineers
**Recommended sequence:**
1. [Wire Protocol Integration](./05-Wire-Protocol-Integration-Specification.md) - Complete read first
2. [Architecture Overview](./01-Architecture-Overview.md) - Bytecode format
3. [Distributed MVCC](./03-Distributed-MVCC-Specification.md) - Transaction semantics

**Implementation focus:**
- PostgreSQL wire protocol (startup, auth, query flow)
- MySQL wire protocol (handshake, COM_QUERY, prepared statements)
- MSSQL TDS protocol (LOGIN7, SQL Batch, Beta)
- Firebird protocol (op_connect, op_execute)
- Parser plugin API and hot-swapping

---

## Key Concepts

### UUID v7 Versioning
Every row version has a UUID v7 identifier that embeds a timestamp, making it globally unique and time-ordered. This eliminates the need for distributed transaction ID coordination.

**Benefits:**
- No global TID coordinator needed
- Natural ordering by timestamp
- Deterministic conflict resolution
- Simple garbage collection

**See:** [Distributed MVCC Specification](./03-Distributed-MVCC-Specification.md)

---

### Three-Tier Storage Architecture

**Tier 1 - Transaction Engines (OLTP)**
- Ephemeral (1-24 hour retention)
- Memory-first with aggressive caching
- Horizontally scalable
- LSM-tree storage

**Tier 2 - Ingestion Engines (Consolidation)**
- Medium retention (days to weeks)
- Merge changes from multiple TX engines
- Conflict resolution
- Full MVCC history

**Tier 3 - OLAP Engines (Analytics)**
- Long retention (months to years)
- Columnar storage (Parquet)
- Heavy compression
- Sharded by dimension

**See:** [Architecture Overview](./01-Architecture-Overview.md)

---

### Clock Synchronization Heartbeat

Cluster-wide time service broadcasts heartbeats (50ms interval) containing:
- Current timestamp (nanosecond precision)
- Measured uncertainty bound
- Clock source quality metrics
- Cluster membership information

**Achievable uncertainty:**
- AWS Time Sync: 1-2ms
- GCP NTP: 2-5ms
- GPS hardware: 100-500ns

**See:** [Clock Synchronization Specification](./02-Clock-Synchronization-Specification.md)

---

### Asynchronous Replication

Changes flow through the system asynchronously:

```
TX Engine → Kafka/NATS → Ingestion → Batch → OLAP
  (100ms)                  (1s)        (5min)
```

**Properties:**
- Idempotent (UUID-based deduplication)
- Order-independent (sorted by UUID v7)
- Conflict resolution built-in
- No distributed transactions needed

**See:** [Replication Protocol Specification](./04-Replication-Protocol-Specification.md)

---

### Wire Protocol Compatibility

Each protocol runs in a dedicated process with its own parser:

```
PostgreSQL (5432) → Parser Plugin → Bytecode → Engine
MySQL (3306)      → Parser Plugin → Bytecode → Engine
MSSQL (1433, Beta) → Parser Plugin → Bytecode → Engine
Firebird (3050)   → Parser Plugin → Bytecode → Engine
```

**Benefits:**
- Native client driver support
- Hot-swappable parsers
- Independent scaling per protocol
- Fault isolation

**See:** [Wire Protocol Integration Specification](./05-Wire-Protocol-Integration-Specification.md)

---

## System Requirements

### Hardware (Production)

**Transaction Engine Node:**
- CPU: 8-16 cores
- RAM: 32-64 GB
- Storage: 500 GB NVMe SSD
- Network: 10 GbE

**Ingestion Engine Node:**
- CPU: 16-32 cores (CPU-intensive)
- RAM: 64-128 GB
- Storage: 2-4 TB SSD
- Network: 10 GbE

**OLAP Shard Node:**
- CPU: 8-16 cores
- RAM: 32-64 GB
- Storage: 5-50 TB HDD/SSD
- Network: 10-40 GbE

**Clock Master:**
- CPU: 2-4 cores
- RAM: 4-8 GB
- GPS receiver (optional, for hardware sync)
- Redundant power supply

### Software

**Operating System:**
- Linux (Ubuntu 22.04 LTS or CentOS 8+)
- Kernel 5.4+

**Dependencies:**
- Chrony (NTP client)
- Kafka or NATS (message broker)
- MessagePack or Protobuf (serialization)
- OpenSSL (TLS)
- systemd (service management)

**Build Tools:**
- GCC 9+ or Clang 10+
- CMake 3.15+
- Git

---

## Performance Targets Summary

| Component | Metric | Target |
|-----------|--------|--------|
| TX Engine | Throughput | 10,000 txn/sec |
| TX Engine | P99 Latency | < 5ms |
| TX Engine | Cache Hit | > 90% |
| Ingestion | Throughput | 1,000 changes/sec |
| Ingestion | Merge Latency | < 1s |
| Ingestion | Conflict Rate | < 0.1% |
| OLAP | Query Latency | < 1s |
| OLAP | Scan Throughput | 1 GB/s |
| OLAP | Compression | 10:1 |
| Replication | Lag (TX→Ingestion) | < 100ms |
| Replication | Lag (Ingestion→OLAP) | < 5 min |
| Clock Sync | Uncertainty (AWS) | 1-2ms |
| Clock Sync | Uncertainty (GPS) | 100-500ns |

---

## Configuration Examples

### Minimal Development Setup

```yaml
# Single-node, all components co-located
deployment:
  mode: development
  nodes: 1
  
clock:
  source: ntp
  
storage:
  tx_engine:
    cache_size_mb: 1024
    retention_hours: 24
    
protocols:
  postgresql:
    enabled: true
    port: 5432
  mysql:
    enabled: true
    port: 3306
```

### Production Cloud Setup (AWS)

```yaml
deployment:
  mode: production
  cloud: aws
  region: us-east-1
  
clock:
  source: aws_time_sync
  max_uncertainty_ms: 5
  
storage:
  tx_engines:
    count: 10
    auto_scaling:
      min: 5
      max: 50
      target_cpu: 70
      
  ingestion_engines:
    count: 3
    
  olap_shards:
    count: 20
    
replication:
  broker: kafka
  brokers:
    - kafka1.internal:9092
    - kafka2.internal:9092
    - kafka3.internal:9092
```

### Production Self-Hosted Setup

```yaml
deployment:
  mode: production
  environment: on-premise
  
clock:
  source: gps
  primary_master: 10.0.1.10
  backup_master: 10.0.1.11
  
storage:
  tx_engines:
    count: 30
    
  ingestion_engines:
    count: 10
    
  olap_shards:
    count: 50
```

---

## Monitoring Dashboards

### System Overview Dashboard
- Cluster health (all tiers)
- Active connections per protocol
- Query throughput (queries/sec)
- P50/P99 latency
- CPU, memory, disk utilization

### Clock Sync Dashboard
- Uncertainty by node
- Heartbeat miss rate
- Clock drift events
- Master failover history

### MVCC Dashboard
- Active transactions
- Version chain lengths
- GC efficiency (versions deleted)
- UUID generation rate

### Replication Dashboard
- Replication lag (all tiers)
- Change event throughput
- Conflict rate
- Message broker lag

### Query Performance Dashboard
- Query latency by protocol
- Cache hit rates
- Slow query log
- Bytecode compilation time

---

## Testing Strategy

### Automated Testing

**Unit Tests:**
- Google Test framework
- 80%+ code coverage
- Run on every commit

**Integration Tests:**
- Full system deployment
- Multi-node scenarios
- Failure injection
- Performance regression

**Protocol Compliance:**
- PostgreSQL (native protocol clients)
- MySQL (mysql-connector)
- MSSQL (pymssql, Beta)
- Firebird (fdb)

**Performance Benchmarks:**
- TPC-C (OLTP)
- TPC-H (OLAP)
- Custom workloads

### Manual Testing

- Security penetration testing
- Disaster recovery drills
- Upgrade procedures
- Documentation validation

---

## Common Questions

### Q: Why UUID v7 instead of global transaction IDs?
**A:** UUID v7 eliminates the need for a distributed TID coordinator. Each transaction engine can generate globally unique, time-ordered identifiers independently. This removes a major coordination bottleneck.

### Q: How does clock uncertainty affect consistency?
**A:** Higher uncertainty increases commit-wait time for strict consistency modes. With 1-2ms uncertainty (AWS), strict serializable commits wait 3-6ms. With 100-500ns uncertainty (GPS), waits are sub-millisecond.

### Q: Can we use this without Message Broker (Kafka/NATS)?
**A:** Technically yes, but not recommended. The message broker provides durability, ordering, and fan-out for replication. Without it, you'd need to build these features yourself.

### Q: What happens if a TX engine crashes?
**A:** Changes are already in the replication stream (durable). The engine can restart and reload recent data from Ingestion tier. No data loss occurs.

### Q: How do we handle schema changes?
**A:** Schema versions are tracked per row. Parsers can handle multiple schema versions. OLAP tier stores schema metadata with each partition. Migration tools can rewrite data to new schema.

### Q: Is this compatible with existing ORMs?
**A:** Yes! Since we implement native wire protocols, existing ORMs (SQLAlchemy, Hibernate, Entity Framework, etc.) work without modification.

---

## Contributing

### Before You Start
1. Read the Implementation Roadmap
2. Review relevant specification documents
3. Check existing issues/tasks
4. Discuss major changes with team

### Code Style
- Follow Google C++ Style Guide
- Use Doxygen comments
- Include unit tests
- Update documentation

### Review Process
1. Create feature branch
2. Implement with tests
3. Submit pull request
4. Address review comments
5. Merge after approval

---

## Support & Contact

**Documentation Issues:**  
File an issue in the docs repository

**Technical Questions:**  
Architecture team mailing list

**Security Issues:**  
security@[domain] (private reporting)

---

## License

[To be determined]

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-01-25 | Initial draft specification |
| 1.1 | 2025-11-25 | Added Ingestion Layer, OLAP Tier, Deployment Guide, Monitoring/Observability |
| 1.2 | 2025-11-25 | Added UDR System and Remote Database UDR with protocol adapter implementations |

---

**Document Index Last Updated:** 2025-11-25
**Total Documentation:** 17 comprehensive specifications (~350 KB total)
**Status:** Design Complete - Ready for Implementation

**Document Breakdown:**
- Core Architecture (00-05): 154 KB
- Data Tier Components (06-07): 59 KB
- Operations & Deployment (08-09): 62 KB
- Extensibility & Migration (10-11x): 75 KB
- Index & Guide (README): 16 KB

---

## Related Specifications

- [Cluster Specification Work](../Cluster%20Specification%20Work/) - Primary Beta cluster architecture
- [Remote Database UDR](./11-Remote-Database-UDR-Specification.md) - Remote database adapter specification
- [Live Migration (Emulated Listener)](./11h-Live-Migration-Emulated-Listener.md) - End-to-end migration flow for legacy apps
- [ScratchBird UDR Client](./11i-ScratchBird-Client-Implementation.md) - SBWP client connector (untrusted)
- [UDR System](../udr/) - UDR system specification
- [Wire Protocols](../wire_protocols/) - Wire protocol implementations
- [Replication](../beta_requirements/replication/) - Beta replication specifications

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Related:** [Cluster Specifications](../Cluster%20Specification%20Work/README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
