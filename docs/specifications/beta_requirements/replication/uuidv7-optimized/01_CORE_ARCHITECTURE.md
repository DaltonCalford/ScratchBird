# 01: Core Architecture - Split-Plane Hybrid Consensus

**Document:** BETA-REPLICATION-01
**Status:** DRAFT
**Version:** 1.0
**Date:** 2026-01-08

---

## 1. Introduction

### 1.1 Purpose

This document specifies the core architectural model for ScratchBird's Beta replication system, implementing a **split-plane hybrid consensus architecture** that combines:
- **Raft consensus** for metadata (control plane)
- **Leaderless replication** (Dynamo-style) for row data (data plane)
- **UUIDv7-native design** for temporal locality

This approach resolves the CAP theorem trade-off by applying different consistency models where appropriate: strong consistency for schema/topology, high availability for user data.

### 1.2 Scope

**In Scope:**
- Control plane vs data plane separation
- Consensus algorithm selection (Raft vs Leaderless)
- Per-shard replication topology
- Quorum mechanics (W + R > N)
- Failover and fencing mechanisms
- Integration with ScratchBird's Firebird MGA transactions

**Out of Scope:**
- Detailed Merkle tree implementation (see 03_TIME_PARTITIONED_MERKLE_FOREST.md)
- HLC conflict resolution (see 04_HYBRID_LOGICAL_CLOCKS.md)
- Multi-table atomicity (see 05_MULTI_TABLE_REPLICATION.md)

### 1.3 Related Documents

- `SBCLUSTER-07-REPLICATION.md` - Base per-shard write-after log (WAL) streaming
- `SBCLUSTER-05-SHARDING.md` - Shard assignment model
- `MGA_RULES.md` - Firebird transaction visibility rules
- Research: `/docs/specifications/reference/UUIDv7 Replication System Design.md`

---

## 2. Architectural Principles

### 2.1 The Split-Plane Model

**Industry Consensus (2026):** Modern distributed databases (Weaviate, Redpanda, CockroachDB) utilize split-plane architectures, recognizing that metadata and data have fundamentally different consistency requirements.

```
┌─────────────────────────────────────────────────────────────┐
│                   CONTROL PLANE (Raft)                      │
│                                                             │
│  What It Manages:                                           │
│  • Catalog & Schema (table definitions, columns, indexes)  │
│  • Cluster topology (node membership, roles)               │
│  • Shard map (which node owns which shards)                │
│  • Replication configuration (RF, quorum settings)         │
│  • Security bundle (certificates, policies)                │
│                                                             │
│  Why Raft?                                                  │
│  • Strong consistency required (no split-brain schema)     │
│  • Infrequent changes (schema changes are rare)            │
│  • Small data volume (KB, not GB)                          │
│  • Simplicity (single leader, easy to reason about)        │
│                                                             │
│  Performance Characteristics:                               │
│  • Latency: 50-100ms (Raft consensus round-trip)          │
│  • Throughput: 100 ops/sec (sufficient for DDL)            │
└─────────────────────────────────────────────────────────────┘
                               │
                               │ Shard Map Distribution
                               │
         ┌─────────────────────┼─────────────────────┐
         │                     │                     │
         ▼                     ▼                     ▼
┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
│  DATA PLANE     │   │  DATA PLANE     │   │  DATA PLANE     │
│  (Leaderless)   │   │  (Leaderless)   │   │  (Leaderless)   │
│                 │   │                 │   │                 │
│ Shard 001-005   │   │ Shard 006-010   │   │ Shard 011-016   │
│ (Node A)        │   │ (Node B)        │   │ (Node C)        │
│                 │   │                 │   │                 │
│ What It Stores: │   │ What It Stores: │   │ What It Stores: │
│ • Row data      │   │ • Row data      │   │ • Row data      │
│ • UUIDv7 IDs    │   │ • UUIDv7 IDs    │   │ • UUIDv7 IDs    │
│ • LSM trees     │   │ • LSM trees     │   │ • LSM trees     │
│ • Merkle forest │   │ • Merkle forest │   │ • Merkle forest │
│                 │   │                 │   │                 │
│ Why Leaderless? │   │ Why Leaderless? │   │ Why Leaderless? │
│ • High avail.   │   │ • No single     │   │ • Write to any  │
│ • No bottleneck │   │   failure point │   │   node          │
│ • Parallel      │   │ • Scales        │   │ • Global writes │
│   writes        │   │   linearly      │   │   low latency   │
│                 │   │                 │   │                 │
│ Performance:    │   │ Performance:    │   │ Performance:    │
│ • Latency: 5-15ms│  │ • Latency: 5-15ms│  │ • Latency: 5-15ms│
│ • Throughput:   │   │ • Throughput:   │   │ • Throughput:   │
│   50K TPS/shard │   │   50K TPS/shard │   │   50K TPS/shard │
└─────────────────┘   └─────────────────┘   └─────────────────┘
         │                     │                     │
         └──────────Gossip/Anti-Entropy──────────────┘
         (Time-partitioned Merkle tree comparison)
```

### 2.2 Why Not Single-Leader for Data?

**Problem:** In a geographically distributed cluster (e.g., US-East, US-West, EU), routing all writes through a single leader introduces unacceptable latency:

```
User in Tokyo → Leader in Virginia: ~200ms RTT
User in Tokyo → Local node (leaderless): ~10ms RTT

Result: 20x latency reduction with leaderless writes
```

**Trade-off:** Leaderless introduces **eventual consistency** and requires **conflict resolution**, but this is acceptable for user data (vs critical metadata).

### 2.3 Why Not Leaderless for Metadata?

**Problem:** Schema changes require strong consistency. Example failure scenario:

```
Timeline without Raft:
T1: Node A receives ALTER TABLE users ADD COLUMN age INT
T2: Node A updates its local catalog
T3: Client writes to Node B (hasn't seen ALTER TABLE yet)
T4: Node B rejects write (unknown column 'age')
T5: Client writes to Node A (succeeds)

Result: Split-brain schema, inconsistent cluster state
```

**Solution:** Raft ensures all nodes agree on schema before any node accepts writes for the new schema.

---

## 3. Per-Shard Replication Topology

### 3.1 Shard Ownership Model

ScratchBird uses **per-shard replication** (from SBCLUSTER-05 and SBCLUSTER-07):

```
Shard Definition:
  shard_id: UUID (UUIDv7 for time ordering)
  range: [start_key, end_key) (partition key hash range)
  replicas: [node_uuid_1, node_uuid_2, node_uuid_3]
  replication_factor: 3 (default)
  primary: node_uuid_1 (for write-after log (WAL) streaming, optional in leaderless mode)
```

**Example 16-Shard Cluster (RF=2):**

```
┌─────────┬──────────────┬──────────────┬──────────────┐
│ Shard   │ Primary      │ Replica 1    │ Replica 2    │
├─────────┼──────────────┼──────────────┼──────────────┤
│ 001     │ Node A       │ Node B       │ -            │
│ 002     │ Node A       │ Node C       │ -            │
│ 003     │ Node B       │ Node A       │ -            │
│ 004     │ Node B       │ Node C       │ -            │
│ 005     │ Node C       │ Node A       │ -            │
│ 006     │ Node C       │ Node B       │ -            │
│ ...     │ ...          │ ...          │ ...          │
│ 016     │ Node C       │ Node A       │ -            │
└─────────┴──────────────┴──────────────┴──────────────┘

Load Distribution:
  Node A: Owns 6 shards (primary), replicates 6 shards
  Node B: Owns 5 shards (primary), replicates 5 shards
  Node C: Owns 5 shards (primary), replicates 5 shards

Result: Balanced load across all nodes
```

### 3.2 Quorum Mechanics (Dynamo-Style)

For high-availability writes to the data plane, ScratchBird implements **quorum-based replication**:

**Configuration Parameters:**
- **N:** Replication factor (total replicas) - default: 2 or 3
- **W:** Write quorum (min replicas to acknowledge write) - default: 2
- **R:** Read quorum (min replicas to query for read) - default: 1

**Quorum Rule:** `W + R > N` ensures read sees at least one node with latest write.

**Example: N=3, W=2, R=2**

```
Write Path:
  1. Client sends INSERT to coordinator
  2. Coordinator routes to 3 replicas (based on partition key hash)
  3. Coordinator waits for W=2 replicas to acknowledge
  4. Returns success to client (3rd replica applies asynchronously)

Read Path:
  1. Client sends SELECT to coordinator
  2. Coordinator queries R=2 replicas
  3. If results differ, uses HLC timestamp to resolve conflict
  4. Returns consistent result to client
```

**Tunable Consistency:**

| Configuration | W | R | Latency | Consistency | Use Case |
|--------------|---|---|---------|-------------|----------|
| **Strong read** | 2 | 2 | Higher | Latest write always seen | Financial transactions |
| **Fast write** | 1 | 2 | Lower | Eventual consistency | Logging, metrics |
| **Balanced** | 2 | 1 | Medium | Good consistency | General OLTP |

### 3.3 Hybrid Model: Primary + Leaderless

**ScratchBird Beta supports BOTH models:**

**Mode 1: Primary-Replica (SBCLUSTER-07 existing)**
- One primary per shard accepts writes
- Primary streams write-after log (WAL) to replicas
- Fast, simple, but primary is bottleneck
- **Use case:** Single-region clusters, simple deployments

**Mode 2: Leaderless (Beta addition)**
- Any replica accepts writes
- Quorum coordination (W + R > N)
- Higher availability, no bottleneck
- **Use case:** Multi-region clusters, high write throughput

**Configuration:**

```sql
-- Create shard with primary-replica mode
CREATE SHARD shard_001
  REPLICATION_MODE = 'PRIMARY_REPLICA'
  REPLICATION_FACTOR = 2;

-- Create shard with leaderless mode
CREATE SHARD shard_002
  REPLICATION_MODE = 'LEADERLESS'
  REPLICATION_FACTOR = 3
  WRITE_QUORUM = 2
  READ_QUORUM = 2;
```

---

## 4. Write Path (Leaderless Mode)

### 4.1 Write Flow

```
Client
   │
   │ 1. INSERT INTO users VALUES (uuid_generate_v7(), 'Alice', ...)
   │
   ▼
Coordinator Node
   │
   │ 2. Hash partition key → Shard ID
   │    Hash('Alice') % 16 = Shard 007
   │
   │ 3. Look up replicas in shard map
   │    Shard 007 → [Node A, Node B, Node C]
   │
   │ 4. Forward write to all N=3 replicas (parallel RPC)
   │
   ├────────────┬────────────┬────────────┐
   ▼            ▼            ▼            ▼
Node A       Node B       Node C
   │            │            │
   │ 5a. Receive write request
   │ 5b. Assign LSN (local log sequence number)
   │ 5c. Generate HLC timestamp (unix_ts + logical_counter)
   │ 5d. Embed HLC in UUIDv7 (record_id)
   │ 5e. Write to local write-after log (WAL)
   │ 5f. Write to LSM tree (MemTable)
   │ 5g. Return ACK with (LSN, HLC_timestamp)
   │            │            │
   │            │            │
   └────────────┴────────────┴────────────┘
                    │
                    │ 6. Coordinator collects ACKs
                    │    - Node A: ACK at T=100ms
                    │    - Node B: ACK at T=110ms
                    │    - Node C: timeout (network delay)
                    │
                    │ 7. Check W=2 quorum
                    │    - 2 ACKs received ✓
                    │    - Quorum achieved
                    │
                    ▼
Client ← SUCCESS (write committed, durable on 2 of 3 replicas)
```

### 4.2 Handling Write Conflicts

**Scenario:** Two clients write to the same key simultaneously on different nodes.

```
T1: Client A → Node 1: UPDATE users SET age=30 WHERE user_id='uuid-123'
T2: Client B → Node 2: UPDATE users SET age=35 WHERE user_id='uuid-123'

Node 1 and Node 2 both accept the write (no leader to serialize).

Resolution:
  1. Both writes complete locally
  2. Anti-entropy process detects divergence (Merkle tree mismatch)
  3. HLC timestamps compared:
     - Client A: HLC=(unix_ts=1000, counter=1)
     - Client B: HLC=(unix_ts=1000, counter=2)  ← higher counter
  4. Client B's write wins (embedded in UUIDv7, deterministic)
  5. Node 1 updates its value to age=35 (converge)
```

---

## 5. Read Path (Leaderless Mode)

### 5.1 Read Flow

```
Client
   │
   │ SELECT * FROM users WHERE user_id = 'uuid-123'
   │
   ▼
Coordinator Node
   │
   │ 1. Hash partition key → Shard ID
   │    Hash(uuid-123) % 16 = Shard 007
   │
   │ 2. Look up replicas in shard map
   │    Shard 007 → [Node A, Node B, Node C]
   │
   │ 3. Select R=2 replicas (read quorum)
   │    Choose: Node A, Node B
   │
   ├────────────┬────────────┐
   ▼            ▼            │
Node A       Node B          │
   │            │            │
   │ 4. Execute SELECT on local storage
   │    - Node A returns: (age=30, HLC=1000.1)
   │    - Node B returns: (age=35, HLC=1000.2)
   │            │            │
   └────────────┴────────────┘
                    │
                    │ 5. Coordinator detects conflict
                    │    (different values for same key)
                    │
                    │ 6. Apply HLC conflict resolution
                    │    HLC=1000.2 > HLC=1000.1
                    │    Winner: age=35 (from Node B)
                    │
                    │ 7. Read repair (asynchronous)
                    │    Send age=35 to Node A to converge
                    │
                    ▼
Client ← age=35 (latest value based on HLC)
```

### 5.2 Read Repair

**Purpose:** Passively converge replicas during normal read operations.

```cpp
void read_repair(const vector<ReadResponse>& responses) {
    // Find the "winning" value (highest HLC timestamp)
    ReadResponse winner = *std::max_element(
        responses.begin(), responses.end(),
        [](const auto& a, const auto& b) {
            return a.hlc_timestamp < b.hlc_timestamp;
        }
    );

    // Asynchronously send winner to stale replicas
    for (const auto& resp : responses) {
        if (resp.hlc_timestamp < winner.hlc_timestamp) {
            rpc_client.async_write(resp.node_id, winner.key, winner.value, winner.hlc_timestamp);
        }
    }
}
```

---

## 6. Failover and Fencing

### 6.1 Node Failure Detection

**Raft Control Plane monitors data plane nodes:**

```cpp
// On Raft leader, every 5 seconds
void monitor_data_plane_health() {
    for (const Node& node : cluster_nodes) {
        HeartbeatResponse resp = rpc_client.heartbeat(node.node_id, timeout: 5s);

        if (resp.status == HeartbeatStatus::FAILED) {
            node_failure_count[node.node_id]++;

            if (node_failure_count[node.node_id] >= 3) {
                // Node has failed - initiate failover
                log_critical("Node {} failed - initiating failover", node.node_id);
                initiate_shard_failover(node.node_id);
            }
        } else {
            node_failure_count[node.node_id] = 0;  // Reset counter
        }
    }
}
```

### 6.2 Shard Failover Procedure

**When a node owning shards fails:**

```
Step 1: Detect Failure
  - Raft leader misses 3 consecutive heartbeats (15 seconds)
  - Node marked as FAILED in cluster state

Step 2: Propose Failover via Raft
  - Leader proposes shard map update:
    Old: Shard 007 → [Node A (failed), Node B, Node C]
    New: Shard 007 → [Node B (promoted), Node C, Node D (new replica)]

Step 3: Quorum Approves
  - Raft quorum commits new shard map
  - All nodes receive updated shard map

Step 4: Promote Replica
  - Node B becomes new primary for Shard 007
  - Node B stops replicating, starts accepting writes

Step 5: Add New Replica (Optional)
  - Node D joins as new replica
  - Node B streams snapshot to Node D
  - Node D catches up via write-after log (WAL)

Step 6: Resume Normal Operations
  - Shard 007 now has RF=3 again
  - Failover complete (< 30 seconds downtime)
```

### 6.3 Fencing (Split-Brain Prevention)

**Problem:** Failed node comes back online, still thinks it's primary.

**Solution:** Generation numbers (monotonic epoch counter).

```cpp
struct ShardGeneration {
    UUID shard_id;
    uint64_t generation_number;  // Incremented on every failover
    UUID current_primary;
};

// When accepting a write
Status accept_write(const WriteRequest& req) {
    ShardGeneration current_gen = raft.get_shard_generation(req.shard_id);

    if (req.generation_number < current_gen.generation_number) {
        // This node has been fenced (old generation)
        log_critical("FENCED: Write rejected, generation mismatch (expected {}, got {})",
            current_gen.generation_number, req.generation_number);

        // Query Raft for latest generation
        ShardGeneration latest_gen = raft.get_latest_shard_generation(req.shard_id);

        if (latest_gen.current_primary != my_node_id) {
            // This node is no longer primary - fence self
            fence_self(req.shard_id, latest_gen.generation_number);
            return Status::DEMOTED("Node has been demoted");
        }
    }

    // Accept write
    return Status::OK();
}

void fence_self(const UUID& shard_id, uint64_t new_generation) {
    log_critical("FENCING: Node demoted for shard {} (new generation: {})",
        shard_id, new_generation);

    shard_manager.set_read_only(shard_id);
    alert_administrators("NODE_DEMOTED", shard_id);
}
```

---

## 7. Integration with Firebird MGA

### 7.1 Transaction Visibility (TIP-Based)

ScratchBird uses Firebird MGA (from MGA_RULES.md), NOT PostgreSQL MVCC:

**Key Principle:** Visibility determined by Transaction Inventory Pages (TIP), not snapshots.

```cpp
// Firebird MGA visibility check
bool is_visible_mga(TransactionId version_xid, TransactionId reader_xid) {
    // Own changes always visible
    if (version_xid == reader_xid) {
        return true;
    }

    // Look up transaction state in TIP
    TxState state = get_transaction_state(version_xid);

    // Only committed transactions older than reader are visible
    if (state == TX_COMMITTED && version_xid < reader_xid) {
        return true;
    }

    // Active or aborted = not visible
    return false;
}
```

**Integration with Replication:**
- Each shard has its own TIP (per-shard transactions, no cross-shard)
- HLC timestamps used for cross-shard ordering (but no cross-shard transactions)
- Replication preserves transaction_id and commit_timestamp

### 7.2 Cross-Shard Snapshot Isolation

**ScratchBird Guarantee:** Each shard provides snapshot isolation independently.

```
Query: SELECT * FROM users JOIN orders ON users.user_id = orders.user_id

Execution:
  1. Coordinator starts query at T=1000
  2. Shard 001 (users): Snapshot at T=1000 (local TIP)
  3. Shard 007 (orders): Snapshot at T=1000 (local TIP)
  4. Each shard sees consistent snapshot (no dirty reads)
  5. Results merged at coordinator

Limitation: No cross-shard transaction atomicity
  - If T=1000 is between commit of users and orders, results may be inconsistent
  - Workaround: Schema-driven colocation (see 05_MULTI_TABLE_REPLICATION.md)
```

---

## 8. Performance Characteristics

### 8.1 Latency Breakdown (Same Datacenter)

| Operation | Latency | Notes |
|-----------|---------|-------|
| Write (W=2, N=3) | 5-15ms | Quorum wait + network RTT |
| Write (W=1, N=3) | 2-5ms | Single ACK, eventual consistency |
| Read (R=1, no conflicts) | 1-5ms | Single replica query |
| Read (R=2, conflict resolution) | 5-10ms | HLC comparison + read repair |
| Raft consensus (schema change) | 50-100ms | Used for DDL only |

### 8.2 Throughput (Per Shard)

| Workload | Throughput | Replication | Notes |
|----------|-----------|-------------|-------|
| Write-heavy (80% writes) | 50K TPS | W=2, N=3 | LSM tree optimized for writes |
| Read-heavy (80% reads) | 200K QPS | R=1 | Reads distributed across replicas |
| Mixed (50/50) | 100K ops/sec | W=2, R=1 | Balanced load |

**Scaling:** With 16 shards, total cluster throughput: 50K × 16 = **800K TPS**

---

## 9. Operational Procedures

### 9.1 Adding a Node

```sql
-- Step 1: Generate node certificate (via SBCLUSTER-02)
CALL generate_node_certificate('node_d.local', 'us-east-1');

-- Step 2: Propose node join via Raft
ALTER CLUSTER ADD NODE 'node_d.local'
  WITH ROLES = ['ENGINE', 'COORDINATOR'];

-- Step 3: Raft commits new cluster topology
-- Step 4: Optionally rebalance shards
ALTER CLUSTER REBALANCE SHARDS
  WITH STRATEGY = 'LEAST_LOADED';
```

### 9.2 Removing a Node

```sql
-- Step 1: Drain shards from node (graceful)
ALTER NODE 'node_b.local' DRAIN SHARDS;

-- Step 2: Wait for shard migration to complete
-- (Replicas promoted, new replicas added elsewhere)

-- Step 3: Remove node from cluster
ALTER CLUSTER REMOVE NODE 'node_b.local';

-- Raft commits node removal
```

---

## 10. Testing Requirements

### 10.1 Functional Tests

1. **Write Quorum:**
   - Start 3-node cluster (N=3, W=2, R=2)
   - Write to shard, verify 2 replicas receive data
   - Verify write succeeds even if 1 replica fails

2. **Read Repair:**
   - Introduce artificial divergence (one replica stale)
   - Read from 2 replicas, verify coordinator detects conflict
   - Verify read repair sends updated value to stale replica

3. **Failover:**
   - Kill primary node for shard
   - Verify Raft detects failure within 15 seconds
   - Verify replica promoted, shard remains available

4. **Fencing:**
   - Simulate network partition (old primary returns)
   - Verify old primary rejects writes (generation mismatch)
   - Verify old primary fences itself

### 10.2 Chaos Tests

1. **Network Partition:** Split cluster 2-1, verify majority continues
2. **Clock Skew:** Introduce 1-second clock skew, verify HLC prevents conflicts
3. **Cascading Failures:** Kill 2 nodes sequentially, verify cluster survives
4. **Byzantine Failures:** Simulate node returning corrupt data, verify detection

---

## 11. Conclusion

The split-plane hybrid consensus architecture provides ScratchBird with:
- **Strong consistency** for metadata (Raft)
- **High availability** for user data (Leaderless)
- **No single point of failure** (distributed writes)
- **Tunable consistency** (quorum parameters)
- **Firebird MGA compatibility** (TIP-based visibility)

This foundation enables the Beta replication features (time-partitioned Merkle forest, HLC conflict resolution, schema colocation) detailed in subsequent documents.

---

**Next Document:** [02_UUIDV7_INTEGRATION.md](./02_UUIDV7_INTEGRATION.md)

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-08
**Approval Required:** Chief Architect, Distributed Systems Lead
