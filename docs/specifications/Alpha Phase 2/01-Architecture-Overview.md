# Database Engine Architecture Overview

**Document Version:** 1.0  
**Date:** 2025-01-25  
**Status:** Draft Specification

**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

---

## Executive Summary

This document describes a distributed, multi-dialect SQL database engine designed for high-performance OLTP and OLAP workloads. The system uses a three-tier architecture with protocol-agnostic wire handlers, supporting PostgreSQL, MySQL, Firebird, and MSSQL/TDS client compatibility (Beta scope).

### Key Design Principles

1. **Wire Protocol Compatibility**: Drop-in replacement for PostgreSQL, MySQL, Firebird, and MSSQL/TDS (Beta scope)
2. **Multi-Tier Storage**: Separate transaction, ingestion, and analytics engines
3. **Distributed MVCC**: UUID v7-based versioning with Firebird-style multi-generational architecture
4. **Clock Synchronization**: Cluster-wide heartbeat for globally ordered timestamps
5. **Flexible Deployment**: Cloud-native with self-hosted hardware upgrade path

### Related Specs

- [Component Model and Responsibilities](../catalog/COMPONENT_MODEL_AND_RESPONSIBILITIES.md)

---

## System Architecture

### High-Level Component Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    CLIENT APPLICATIONS                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │PostgreSQL│  │  MySQL   │  │  MSSQL   │  │ Firebird │       │
│  │  Client  │  │  Client  │  │  Client  │  │  Client  │       │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └────┬─────┘       │
└───────┼─────────────┼─────────────┼─────────────┼──────────────┘
        │             │             │             │
    Port 5432     Port 3306     Port 1433     Port 3050
        │             │             │             │
┌───────▼─────────────▼─────────────▼─────────────▼──────────────┐
│              PROTOCOL HANDLER LAYER (Per-Protocol Process)      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  Wire Protocol Handler (Thread Pool)                     │  │
│  │  - Connection management                                 │  │
│  │  - Protocol encoding/decoding                            │  │
│  │  - Authentication handshake                              │  │
│  └────────────────────────┬─────────────────────────────────┘  │
│                           │                                     │
│  ┌────────────────────────▼─────────────────────────────────┐  │
│  │  SQL Parser Plugin (.so)                                 │  │
│  │  - Dialect-specific parsing                              │  │
│  │  - AST → Bytecode compilation                            │  │
│  │  - Statement cache (SQL → Bytecode)                      │  │
│  │  - System catalog virtualization                         │  │
│  └────────────────────────┬─────────────────────────────────┘  │
└───────────────────────────┼──────────────────────────────────┘
* MSSQL/TDS is in Beta scope and part of compatibility targets.
                            │ Unix Domain Socket
                            │ (MessagePack/Protobuf)
┌───────────────────────────▼──────────────────────────────────┐
│                    ENGINE CORE PROCESS                        │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  Query Router                                           │  │
│  │  - Tier selection (TX/Ingestion/OLAP)                  │  │
│  │  - Timestamp-based routing                             │  │
│  │  - Consistency mode enforcement                        │  │
│  └────────────────────┬──────────────┬─────────────────────┘  │
│                       │              │                         │
│  ┌────────────────────▼──────────────▼─────────────────────┐  │
│  │  Bytecode Executor                                       │  │
│  │  - Execution plan cache                                 │  │
│  │  - MVCC transaction manager                             │  │
│  └─────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
┌───────▼──────┐  ┌─────────▼────────┐  ┌──────▼──────┐
│  TIER 1:     │  │  TIER 2:         │  │  TIER 3:    │
│  Transaction │  │  Ingestion       │  │  OLAP       │
│  Engines     │  │  Engines         │  │  Engines    │
│  (Ephemeral) │  │  (Consolidation) │  │  (Analytics)│
│              │  │                  │  │             │
│  - Hot cache │  │  - Deduplication │  │  - Columnar │
│  - 1-24hr    │  │  - Validation    │  │  - Sharded  │
│  - LSM tree  │  │  - Full MVCC     │  │  - Immutable│
└──────────────┘  └──────────────────┘  └─────────────┘
```

---

## Component Responsibilities (Alpha Model)

This matrix and diagram reflect the current Engine/Parser/Network Listener model and match
`specifications/catalog/COMPONENT_MODEL_AND_RESPONSIBILITIES.md`.
Reuse artifacts: [Component Model Diagrams](../../diagrams/component_model_diagrams.md),
[Component Responsibility Matrix](../../diagrams/component_responsibility_matrix.md).

### Responsibility Matrix

| Responsibility | Engine Core | Parser | Network Listener | Notes |
| --- | --- | --- | --- | --- |
| Accept connections and TLS | None | None | Primary | Listener owns accept/bind and TLS termination if configured. |
| Wire protocol decode/encode | None | Primary | None | Dialect-specific framing and message handling. |
| SQL dialect parsing | None | Primary | None | Produces SBLR from dialect SQL. |
| SBLR validation and execution | Primary | None | None | Engine is sole authority for SBLR. |
| Authentication decisions | Primary | Support | None | Parser extracts credentials; engine decides. |
| Authorization decisions | Primary | None | None | Engine enforces permissions. |
| Session lifecycle | Primary | Support | Support | Parser tracks per-connection state; listener manages parser pool. |
| Catalog virtualization | Primary | Support | None | Engine defines surfaces; parser emulates dialect views. |
| Shared SBLR cache | Primary | None | None | Global cache across sessions with security gating. |
| Per-session compile cache | None | Primary | None | SQL -> SBLR artifacts scoped to connection. |
| Storage, transactions, GC, maintenance | Primary | None | None | Firebird-style MGA; no write-after log (WAL, optional post-gold) in core. |
| Scheduler (task planning) | Primary | None | None | Engine schedules GC and maintenance work. |
| Job system (execution workers) | Primary | None | None | Engine owns background job execution. |
| UDR connectors (local/remote) | Primary | None | None | Engine executes UDR connectors and enforces security. |
| Parser pool management | None | None | Primary | Spawn/assign/recycle parser workers. |
| Cluster manager (listener coordination) | Support | None | Primary | Listener-to-listener routing and admission control in cluster mode. |
| Stale connection/txn detection | Primary | Support | Support | Engine authoritative, parser/listener report. |

### Component Flow (Mermaid)

```mermaid
flowchart LR
  Clients[Clients] --> Listener[Network Listener]
  Listener -->|socket handoff| Parser[Parser (per connection)]
  Parser -->|IPC or embedded| Engine[Engine Core]
  Engine --> Catalog[(Catalog/Metadata)]
  Engine --> Storage[(Storage/Transactions)]
  Engine --> Cache[(Shared SBLR Cache)]
  Parser --> SessionCache[(Per-session Cache)]
```

### Component Flow (ASCII)

```
Clients
  |
  v
Network Listener --(socket handoff)--> Parser (per connection)
                                          |
                                          v
                                    Engine Core
                                      |   |   |
                                      |   |   +--> Shared SBLR Cache
                                      |   +------> Catalog/Metadata
                                      +----------> Storage/Transactions
```

---

## Three-Tier Storage Architecture

### Tier 1: Transaction Engines (OLTP)

**Purpose**: High-speed transaction processing with aggressive caching

**Characteristics**:
- Ephemeral storage (1-24 hour retention)
- Memory-first architecture (90%+ working set in RAM)
- Horizontal scaling for traffic spikes
- LSM-tree or log-structured storage
- Local query result cache with TTL

**Use Cases**:
- Web application transactions
- API request processing
- Session management
- Shopping cart operations
- Real-time user interactions

**Storage Layout**:
```
Transaction Engine Storage:
├─ In-Memory Cache (LRU)
│  ├─ Query result cache (bytecode hash → result set)
│  ├─ Prepared statement cache
│  └─ Hot row cache
│
├─ Persistent Storage (SSD)
│  ├─ LSM tree for row versions
│  ├─ Write-after log (WAL, optional post-gold) (durability)
│  └─ Periodic snapshots
│
└─ Replication Queue
   └─ Change events → Message broker
```

**Performance Targets**:
- P99 latency: < 5ms
- Cache hit rate: > 90%
- Throughput: 10,000+ txn/sec per engine
- Replication lag: < 100ms

---

### Tier 2: Ingestion Engines (Consolidation)

**Purpose**: Merge data from multiple transaction engines, provide consolidated view

**Characteristics**:
- Medium retention (days to weeks)
- CPU-heavy for deduplication and validation
- Full MVCC history maintenance
- B-tree indexes for efficient lookups
- Handles conflict resolution

**Use Cases**:
- Recent historical queries (last 7 days)
- Data consolidation from TX engines
- ETL processing and validation
- Source for OLAP tier updates
- Point-in-time recovery

**Storage Layout**:
```
Ingestion Engine Storage:
├─ Version Store (B-tree)
│  ├─ Primary index: UUID v7
│  ├─ Secondary index: (table_id, row_key) → UUID list
│  └─ MVCC chain links (prev_version_uuid)
│
├─ Change Streams
│  ├─ Subscribe to TX engine replication
│  ├─ Merge queue (priority by UUID v7 timestamp)
│  └─ Conflict resolver
│
└─ OLAP Push Scheduler
   ├─ Batch builder (columnar format)
   ├─ Compression
   └─ Partition management
```

**Performance Targets**:
- Merge latency: < 1 second
- Conflict rate: < 0.1%
- Push lag to OLAP: < 5 minutes
- Deduplication rate: 99.9%+

---

### Tier 3: OLAP Engines (Analytics)

**Purpose**: Long-term storage and analytical query processing

**Characteristics**:
- Long retention (months to years)
- Columnar storage (Parquet/ORC)
- Immutable partitions (mostly)
- Heavy compression (10:1 typical)
- Horizontal sharding by dimension

**Use Cases**:
- Business intelligence dashboards
- Historical reporting
- Data warehousing
- Aggregate queries
- Trend analysis

**Storage Layout**:
```
OLAP Shard Storage:
├─ Partition Structure
│  ├─ Time-based partitions (daily/weekly/monthly)
│  ├─ Columnar files (Parquet)
│  └─ Partition metadata (zone maps, bloom filters)
│
├─ Indexing
│  ├─ Bitmap indexes (low cardinality columns)
│  ├─ Zone maps (min/max per partition)
│  └─ Bloom filters (membership tests)
│
└─ Sharding Strategy
   ├─ Time-based shards (hot vs cold)
   ├─ Geography-based shards
   └─ Tenant-based shards (multi-tenant SaaS)
```

**Performance Targets**:
- Query latency: < 1s for dashboards
- Data freshness: < 5 minutes (configurable)
- Compression ratio: 10:1 typical
- Scan throughput: 1 GB/s per core

---

## Data Flow Through Tiers

### Write Path

```
1. Client writes to TX Engine
   INSERT INTO users VALUES (...)
   
2. TX Engine processes locally
   - Validate constraints
   - Generate UUID v7 for new row version
   - Tag with (server_id, local_transaction_id)
   - Apply to local storage
   - Update local cache
   - Acknowledge to client ✓
   
3. Async replication to Ingestion
   - Serialize change event (MessagePack)
   - Publish to message broker (Kafka/NATS)
   - TX Engine continues processing
   
4. Ingestion Engine receives change
   - Deduplicate by UUID v7
   - Resolve conflicts (if any)
   - Merge into consolidated MVCC store
   - Update secondary indexes
   
5. Batch push to OLAP
   - Accumulate changes for 5-15 minutes
   - Convert to columnar format
   - Compress and partition
   - Write immutable file
   - Update partition metadata
```

### Read Path

```
1. Client query arrives at Protocol Handler
   SELECT * FROM users WHERE created_at > '2025-01-20'
   
2. Parser converts to bytecode
   - Cache hit? Use cached bytecode
   - Cache miss? Parse SQL → Bytecode
   
3. Query Router determines tier
   - Recent data (< 24 hours): TX Engine
   - Recent history (< 7 days): Ingestion Engine
   - Historical (> 7 days): OLAP Engine
   
4. Execute on appropriate tier
   - MVCC visibility check
   - Apply filters and predicates
   - Return result set
   
5. Format for protocol
   - Parser converts to protocol-specific format
   - Stream to client
```

---

## Protocol Handler Architecture

Each protocol (PostgreSQL, MySQL, Firebird, MSSQL/TDS) runs in a separate process with its own parser plugin.

### Protocol Handler Process

```
Process per Protocol:
├─ Wire Protocol Handler
│  ├─ Socket listener (dedicated port)
│  ├─ Connection pool management
│  ├─ Protocol-specific authentication
│  ├─ Message encoding/decoding
│  └─ Session state management
│
├─ SQL Parser Plugin (.so)
│  ├─ Dialect-specific lexer/parser
│  ├─ AST construction
│  ├─ Bytecode compiler
│  ├─ Statement cache (SQL → Bytecode)
│  └─ System catalog virtualization
│
└─ Engine Communication
   ├─ Unix domain socket to engine
   ├─ MessagePack/Protobuf serialization
   └─ Request/response handling
```

### Benefits of Per-Protocol Process

1. **Isolation**: Parser bugs don't affect other protocols
2. **Hot-swap**: Upgrade PostgreSQL parser without affecting MySQL clients
3. **Resource Control**: Set memory/CPU limits per protocol
4. **Independent Scaling**: Run more instances of popular protocols
5. **Debugging**: Clear process boundaries for troubleshooting

---

## Bytecode Execution Model

The engine core executes a protocol-agnostic bytecode format.

### Bytecode Format

```
Bytecode Program:
├─ Header
│  ├─ Version
│  ├─ Bytecode hash (SHA256)
│  ├─ Parameter count
│  └─ Return column metadata
│
├─ Instructions (stack-based VM)
│  ├─ LOAD_TABLE <table_id>
│  ├─ FILTER <predicate_bytecode>
│  ├─ PROJECT <column_list>
│  ├─ JOIN <join_type> <other_table>
│  ├─ AGGREGATE <agg_functions>
│  └─ RETURN
│
└─ Constant Pool
   ├─ Literal values
   ├─ Table references
   └─ Function references
```

### Execution Flow

```c
// Pseudocode for bytecode execution
ResultSet execute_bytecode(BytecodeProgram* bc, Parameter* params) {
    // 1. Check execution plan cache
    ExecutionPlan* plan = plan_cache_lookup(bc->hash);
    
    if (!plan) {
        // 2. Build execution plan
        plan = build_execution_plan(bc);
        plan_cache_insert(bc->hash, plan);
    }
    
    // 3. Bind parameters
    bind_parameters(plan, params);
    
    // 4. Execute plan
    ResultSet result = execute_plan(plan);
    
    return result;
}
```

---

## Consistency Guarantees

### Consistency Modes

The system supports multiple consistency levels, configurable per-table:

| Mode | Guarantee | Use Case | Commit Wait |
|------|-----------|----------|-------------|
| **Eventual** | Changes propagate eventually | Session data, cache | None |
| **Bounded Staleness** | Reads may be up to Δt stale | Most transactions | 1x uncertainty |
| **Monotonic Reads** | Read-your-writes within session | User accounts | 2x uncertainty |
| **Strict Serializable** | Linearizable across cluster | Financial transactions | 3x uncertainty |

### Consistency Configuration

```yaml
# Per-table consistency settings
table_consistency:
  sessions:
    mode: eventual
    max_staleness_ms: 1000
    
  users:
    mode: monotonic_reads
    max_staleness_ms: 100
    
  financial_transactions:
    mode: strict_serializable
    max_staleness_ms: 10
    allow_degraded: false  # Fail if can't guarantee
```

---

## Deployment Topologies

### Cloud Deployment (AWS Example)

```
Region: US-East-1
├─ Availability Zone 1a
│  ├─ Clock Master (AWS Time Sync)
│  ├─ TX Engine 1
│  ├─ TX Engine 2
│  └─ Ingestion Engine 1
│
├─ Availability Zone 1b
│  ├─ Clock Master (backup)
│  ├─ TX Engine 3
│  └─ OLAP Shard 1
│
└─ Availability Zone 1c
   ├─ Ingestion Engine 2
   └─ OLAP Shard 2

Clock Uncertainty: 1-2ms (AWS Time Sync)
AZ-to-AZ Latency: 1-2ms
```

### Self-Hosted Deployment (Hardware)

```
Datacenter:
├─ Clock Master Rack
│  ├─ GPS Grandmaster Clock (Stratum 0)
│  └─ Backup GPS Grandmaster
│
├─ Transaction Engine Rack
│  ├─ TX Engine 1-8 (NVMe SSDs)
│  └─ 10GbE network
│
├─ Ingestion Engine Rack
│  ├─ Ingestion Engine 1-4 (high CPU)
│  └─ Message broker cluster
│
└─ OLAP Rack
   ├─ OLAP Shards 1-16 (high capacity)
   └─ 40GbE network for data movement

Clock Uncertainty: 100-500ns (GPS)
Rack-to-Rack Latency: < 500μs
```

---

## Scalability Model

### Horizontal Scaling

**Transaction Engines**:
- Spin up/down based on traffic
- Auto-scaling triggers: CPU, connection count
- No coordination needed (autonomous)
- Typical scale: 2-100+ instances

**Ingestion Engines**:
- Scale based on replication queue depth
- Partition by table or hash range
- Coordination via message broker
- Typical scale: 2-10 instances

**OLAP Engines**:
- Shard by time, geography, or tenant
- Add shards as data grows
- Static topology (infrequent changes)
- Typical scale: 4-100+ shards

### Capacity Planning

```
Example: 1 million transactions/day

TX Engines:
- Average: 12 txn/sec
- Peak (10x): 120 txn/sec
- TX Engine capacity: 10,000 txn/sec
- Instances needed: 1 (nominal), 2 (for HA/peak)

Ingestion:
- Change rate: 12 changes/sec average
- Merge overhead: 2x
- Ingestion capacity: 1000 changes/sec
- Instances needed: 1 (nominal), 2 (for HA)

OLAP:
- Daily data: ~1 GB/day (compressed)
- Query workload: 100 queries/hour
- OLAP capacity: 1000 queries/hour
- Shards needed: 1 (nominal), 2+ (for growth)
```

---

## Failure Modes and Recovery

### Transaction Engine Failure

**Symptoms**: Engine stops responding, heartbeat timeout

**Impact**: Clients connected to that engine see connection loss

**Recovery**:
1. Load balancer detects failure (health check)
2. Routes new traffic to healthy engines
3. Clients reconnect and retry transactions
4. Failed engine data is in replication stream (safe)
5. Spin up replacement engine
6. Load recent snapshot from Ingestion tier

**Data Loss**: None (changes already replicated)

### Ingestion Engine Failure

**Symptoms**: Replication queue backs up

**Impact**: TX engines buffer changes locally

**Recovery**:
1. TX engines continue operating (eventual consistency)
2. Start replacement Ingestion engine
3. Replay from last checkpoint in replication stream
4. Catch up with buffered changes
5. Resume OLAP push

**Data Loss**: None (TX engines buffer, message broker persists)

### OLAP Shard Failure

**Symptoms**: Queries to that shard timeout

**Impact**: Historical queries fail or return partial results

**Recovery**:
1. Mark shard as unavailable
2. Rebuild from Ingestion tier (hours to days)
3. Or restore from backup
4. Resume query processing

**Data Loss**: None (rebuild from source of truth)

### Clock Master Failure

**Symptoms**: Clock uncertainty increases

**Impact**: Consistency guarantees degrade

**Recovery**:
1. Backup clock master takes over
2. Nodes update to new master
3. Uncertainty may increase temporarily
4. Transactions may pause if strict mode enabled

**Data Loss**: None (affects latency, not correctness)

---

## Performance Targets

### Transaction Engine

| Metric | Target | Measurement |
|--------|--------|-------------|
| P50 Latency | < 2ms | Simple SELECT |
| P99 Latency | < 5ms | Simple SELECT |
| P99.9 Latency | < 20ms | Simple SELECT |
| Throughput | 10,000 txn/sec | Single engine |
| Cache Hit Rate | > 90% | Query result cache |
| Write Amplification | < 3x | LSM tree |

### Ingestion Engine

| Metric | Target | Measurement |
|--------|--------|-------------|
| Merge Latency | < 1s | Change event to merged |
| Throughput | 1,000 changes/sec | Single engine |
| Conflict Rate | < 0.1% | Concurrent updates |
| Push Lag | < 5 min | To OLAP tier |
| Dedup Efficiency | 99.9%+ | Duplicate detection |

### OLAP Engine

| Metric | Target | Measurement |
|--------|--------|-------------|
| Query Latency | < 1s | Dashboard queries |
| Scan Throughput | 1 GB/s | Sequential scan |
| Compression Ratio | 10:1 | Columnar storage |
| Data Freshness | < 5 min | From Ingestion |
| Concurrent Queries | 100+ | Per shard |

---

## Technology Stack

### Core Components

- **Programming Language**: C/C++ (engine core), Rust (optional parsers)
- **Wire Protocols**: Native implementations (PostgreSQL, MySQL, Firebird, MSSQL/TDS)
- **Serialization**: MessagePack or Protocol Buffers
- **Message Broker**: Kafka or NATS Streaming
- **Clock Sync**: Chrony (software), GPS/PTP (hardware)
- **Storage Format**: Custom LSM-tree (TX), B-tree (Ingestion), Parquet (OLAP)

### Dependencies

- **Protocol references** (PostgreSQL, MySQL, Firebird, MSSQL/TDS)
- **MessagePack** or **Protobuf** (serialization)
- **Kafka** or **NATS** (message broker)
- **Chrony** (NTP client)
- **Arrow** (optional, for OLAP columnar format)

---

## Next Steps

This overview provides the foundation. Detailed specifications follow in:

1. **Clock Synchronization Specification** - Heartbeat protocol, uncertainty tracking
2. **Distributed MVCC Specification** - UUID v7 versioning, garbage collection
3. **Replication Protocol Specification** - Change data capture, conflict resolution
4. **Multi-Tier Storage Specification** - Storage layouts, caching strategies
5. **Wire Protocol Integration Specification** - Parser API, system catalog virtualization
6. **Query Routing Specification** - Tier selection, consistency enforcement
7. **Operational Monitoring Specification** - Metrics, alerting, dashboards

---

**Document Status**: Draft for Review  
**Next Review Date**: TBD  
**Approval Required**: Architecture Team
