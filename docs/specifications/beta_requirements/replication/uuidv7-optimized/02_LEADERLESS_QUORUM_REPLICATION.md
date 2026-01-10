# Leaderless Quorum Replication (Mode 3)

**Document:** 02_LEADERLESS_QUORUM_REPLICATION.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Replication Modes Overview](#replication-modes-overview)
3. [Quorum Consensus Model](#quorum-consensus-model)
4. [Write Path](#write-path)
5. [Read Path](#read-path)
6. [Hinted Handoff](#hinted-handoff)
7. [Failure Handling](#failure-handling)
8. [Consistency Guarantees](#consistency-guarantees)
9. [Integration with SBCLUSTER-07](#integration-with-sbcluster-07)
10. [Testing and Verification](#testing-and-verification)

---

## Executive Summary

### Purpose

This specification defines **Mode 3: Leaderless Quorum Replication**, a multi-master replication model where any node can accept writes for a given shard. Writes are coordinated via quorum consensus (W + R > N), enabling high availability and partition tolerance while maintaining eventual consistency with deterministic conflict resolution (via UUIDv8-HLC).

**Key Innovation**: Mode 3 extends ScratchBird's existing replication architecture (SBCLUSTER-07) without replacing it. Primary-replica replication (Mode 1/2) remains the default; Mode 3 is opt-in for tables requiring multi-master writes.

### Design Principles

1. **Coexistence**: Mode 3 coexists with Mode 1 (Logical) and Mode 2 (Physical) replication
2. **Per-Table Granularity**: Replication mode configured per table, not per database
3. **CAP Trade-off**: Prioritizes Availability + Partition Tolerance (AP) over strong Consistency (C)
4. **Deterministic Convergence**: UUIDv8-HLC ensures all replicas converge to same state
5. **Firebird MGA Compatibility**: Integrates with TIP-based visibility, no PostgreSQL MVCC assumptions

### Use Cases

**When to Use Mode 3**:
- IoT sensor data ingestion (multi-region writes)
- Event sourcing with partition tolerance requirements
- Shopping carts (multi-datacenter availability)
- Any workload prioritizing write availability over strong consistency

**When NOT to Use Mode 3**:
- Financial transactions (require strong consistency)
- User authentication (single source of truth)
- Schema/catalog updates (control plane uses Raft)
- Single-region deployments (Mode 1/2 simpler and faster)

---

## Replication Modes Overview

### Mode 1: Logical Replication (Existing)

**Characteristics**:
- Primary-replica model (one writer, multiple readers)
- Logical changes (SQL-level) streamed to replicas
- Replicas can have different physical layouts
- Failover: Manual promotion of replica to primary

**Specified in**: SBCLUSTER-07-REPLICATION.md (Section 4.1)

### Mode 2: Physical Replication (Existing)

**Characteristics**:
- Primary-replica model (one writer, multiple readers)
- Physical pages streamed to replicas (block-level)
- Replicas identical to primary (bit-for-bit)
- Failover: Fast promotion (no replay needed)

**Specified in**: SBCLUSTER-07-REPLICATION.md (Section 4.2)

### Mode 3: Leaderless Quorum (New - Beta)

**Characteristics**:
- **Multi-master**: Any node can accept writes
- **Quorum coordination**: Write succeeds if W nodes acknowledge
- **Eventual consistency**: All replicas converge via anti-entropy
- **Conflict resolution**: UUIDv8-HLC deterministic ordering

**This Document**: Full specification below

---

## Quorum Consensus Model

### Quorum Parameters

**Replication Factor (N)**:
- Total number of replicas for a shard
- Configured per table: `WITH (replication_factor = 3)`
- Typical values: N=3 (default), N=5 (high availability)

**Write Quorum (W)**:
- Minimum nodes that must acknowledge write for success
- Configured per table: `WITH (write_quorum = 2)`
- Constraint: `1 ≤ W ≤ N`

**Read Quorum (R)**:
- Minimum nodes that must respond to read for success
- Configured per table: `WITH (read_quorum = 2)`
- Constraint: `1 ≤ R ≤ N`

**Consistency Constraint**:
- **Strong consistency**: `W + R > N` (guarantees read sees latest write)
- **Eventual consistency**: `W + R ≤ N` (allows stale reads, faster)

**Example Configurations**:

| N | W | R | Consistency | Tolerance | Use Case |
|---|---|---|-------------|-----------|----------|
| 3 | 2 | 2 | Strong | 1 node failure | Balanced (default) |
| 3 | 1 | 3 | Eventual | 2 node failures (write) | Write-heavy workloads |
| 3 | 3 | 1 | Eventual | 2 node failures (read) | Read-heavy workloads |
| 5 | 3 | 3 | Strong | 2 node failures | High availability |
| 5 | 2 | 4 | Eventual | 3 node failures (write) | Extreme write availability |

### Shard Assignment

**Consistent Hashing** (borrowed from SBCLUSTER-05):
- Each shard has N replicas distributed across cluster
- Replicas assigned to nodes via consistent hash ring
- Primary affinity: First replica in hash ring is "preferred coordinator" (not required)

**Example**:

```
Shard 001 (N=3):
  Replica 1: Node A (preferred coordinator)
  Replica 2: Node B
  Replica 3: Node C

Write Quorum (W=2):
  Write succeeds if any 2 of {A, B, C} acknowledge
  Coordinator: Any client can send write to any node
```

---

## Write Path

### Write Workflow

**Step 1: Client sends write to any node (coordinator)**

```
Client → Node A: INSERT INTO sensor_data (sensor_id, value) VALUES (gen_uuidv8_hlc(), 42.5);
```

**Step 2: Coordinator determines replica set**

```
Node A:
  1. Parse query, extract table: sensor_data
  2. Lookup replication config: mode=leaderless_quorum, N=3, W=2, R=2
  3. Hash partition key to determine shard: shard_id = hash(sensor_id) % num_shards
  4. Lookup replica set: {Node A, Node B, Node C}
```

**Step 3: Coordinator generates version ID**

```
Node A:
  1. Generate UUIDv8-HLC: version_id = gen_uuidv8_hlc()  // ts=1000, counter=5, node_id=1
  2. Update local HLC state (monotonicity guarantee)
  3. Create write payload: {sensor_id, value=42.5, version_id, xid}
```

**Step 4: Coordinator sends write to replica set**

```
Node A → {Node A (local), Node B, Node C}:
  Write request: {shard_id, row_data, version_id, xid, hlc=(1000, 5)}
```

**Step 5: Each replica applies write locally**

```
Node A (local):
  1. Begin transaction (TIP allocates xid_a)
  2. Insert row with version_id (UUIDv8-HLC)
  3. Update TIP: mark xid_a as COMMITTED
  4. Respond to coordinator: ACK

Node B:
  1. Receive write request
  2. Update local HLC from coordinator's HLC: updateHlcFromRemote(1000, 5)
  3. Begin transaction (TIP allocates xid_b)
  4. Insert row with version_id (same UUID from coordinator)
  5. Update TIP: mark xid_b as COMMITTED
  6. Respond to coordinator: ACK

Node C:
  (Same as Node B)
```

**Step 6: Coordinator waits for W acknowledgments**

```
Node A (coordinator):
  Received ACKs: {Node A, Node B}  // W=2, success!
  Response to client: INSERT successful (1 row affected)
```

**Step 7: Asynchronous replication to remaining nodes**

```
Node A (coordinator):
  Node C's ACK arrives late (network delay) → Apply normally
  Or Node C didn't respond → Hinted handoff (see section below)
```

### Write Path Pseudocode

```cpp
auto ReplicationCoordinator::coordinateWrite(
    const WriteRequest& req,
    const ReplicaSet& replicas,
    size_t write_quorum
) -> Status {
    // 1. Generate version ID (UUIDv8-HLC)
    auto version_id = uuid_generator_.generate();

    // 2. Create write payload
    WritePayload payload{
        .shard_id = req.shard_id,
        .row_data = req.row_data,
        .version_id = version_id,
        .xid = req.xid,
        .hlc_timestamp = version_id.getTimestamp(),
        .hlc_counter = version_id.getHlcCounter()
    };

    // 3. Send write to all replicas in parallel
    std::vector<std::future<Status>> futures;
    for (const auto& replica : replicas) {
        futures.push_back(
            std::async(std::launch::async, [&]() {
                return sendWriteToReplica(replica, payload);
            })
        );
    }

    // 4. Wait for W acknowledgments (with timeout)
    size_t acks = 0;
    std::vector<ReplicaId> successful_replicas;
    std::vector<ReplicaId> failed_replicas;

    for (size_t i = 0; i < futures.size(); ++i) {
        auto status = futures[i].wait_for(std::chrono::milliseconds(write_timeout_ms_));
        if (status == std::future_status::ready && futures[i].get().ok()) {
            acks++;
            successful_replicas.push_back(replicas[i]);
        } else {
            failed_replicas.push_back(replicas[i]);
        }

        // Early return if quorum achieved
        if (acks >= write_quorum) {
            return Status::OK();
        }
    }

    // 5. Check if quorum achieved
    if (acks >= write_quorum) {
        // Success: Store hints for failed replicas (async)
        for (const auto& replica : failed_replicas) {
            storeHintedHandoff(replica, payload);
        }
        return Status::OK();
    } else {
        // Failure: Quorum not achieved, rollback successful writes
        for (const auto& replica : successful_replicas) {
            sendRollback(replica, payload.xid);
        }
        return Status::QuorumNotAchieved();
    }
}
```

### Write Latency

**Best Case**: `W=1`, coordinator is local replica
- Latency: Single local write (< 1ms)

**Typical Case**: `W=2`, N=3, same datacenter
- Latency: Local write + 1 network RTT (~5-10ms)

**Worst Case**: `W=3`, N=3, cross-datacenter
- Latency: Local write + 2 network RTTs (~100-200ms)

---

## Read Path

### Read Workflow

**Step 1: Client sends read to any node (coordinator)**

```
Client → Node A: SELECT value FROM sensor_data WHERE sensor_id = 'X';
```

**Step 2: Coordinator determines replica set**

```
Node A:
  1. Parse query, extract table: sensor_data
  2. Lookup replication config: mode=leaderless_quorum, N=3, W=2, R=2
  3. Hash partition key: shard_id = hash('X') % num_shards
  4. Lookup replica set: {Node A, Node B, Node C}
```

**Step 3: Coordinator sends read to R replicas**

```
Node A → {Node A (local), Node B}:  // R=2
  Read request: {shard_id, sensor_id='X', reader_xid}
```

**Step 4: Each replica returns version(s)**

```
Node A (local):
  1. Query local storage: SELECT * FROM sensor_data WHERE sensor_id='X'
  2. Apply MGA visibility: Filter versions visible to reader_xid
  3. Return: {version_id_1=(ts=1000, counter=5), value=42.5}

Node B:
  1. Same as Node A
  2. Return: {version_id_1=(ts=1000, counter=5), value=42.5}
```

**Step 5: Coordinator reconciles versions**

```
Node A (coordinator):
  Received versions:
    - Node A: {version_id_1=(1000, 5), value=42.5}
    - Node B: {version_id_1=(1000, 5), value=42.5}

  Reconciliation:
    - Same version ID → No conflict
    - Return: value=42.5
```

**Step 6: Conflict resolution (if versions differ)**

```
Scenario: Node A and Node B have different versions

Node A: {version_id_a=(ts=1000, counter=5), value=42.5}
Node B: {version_id_b=(ts=1000, counter=10), value=99.9}

Coordinator reconciliation:
  1. Compare UUIDv8-HLC: version_id_a < version_id_b (counter 5 < 10)
  2. Winner: version_id_b (value=99.9)
  3. Trigger read repair: Send version_id_b to Node A (async)
  4. Return to client: value=99.9
```

### Read Path Pseudocode

```cpp
auto ReplicationCoordinator::coordinateRead(
    const ReadRequest& req,
    const ReplicaSet& replicas,
    size_t read_quorum
) -> Result<RowData> {
    // 1. Send read to R replicas in parallel
    std::vector<std::future<Result<RowData>>> futures;
    for (size_t i = 0; i < read_quorum && i < replicas.size(); ++i) {
        futures.push_back(
            std::async(std::launch::async, [&]() {
                return sendReadToReplica(replicas[i], req);
            })
        );
    }

    // 2. Collect responses
    std::vector<RowData> versions;
    for (auto& fut : futures) {
        auto result = fut.get();
        if (result.ok()) {
            versions.push_back(result.value());
        }
    }

    // 3. Check if quorum achieved
    if (versions.size() < read_quorum) {
        return Status::QuorumNotAchieved();
    }

    // 4. Reconcile versions (conflict resolution)
    auto winner = reconcileVersions(versions);

    // 5. Trigger read repair if versions differ (async)
    for (size_t i = 0; i < versions.size(); ++i) {
        if (versions[i].version_id != winner.version_id) {
            sendReadRepair(replicas[i], winner);
        }
    }

    return winner;
}

auto ReplicationCoordinator::reconcileVersions(
    const std::vector<RowData>& versions
) -> RowData {
    if (versions.empty()) {
        return RowData{};  // No data
    }

    // Find version with highest UUIDv8-HLC
    auto winner = versions[0];
    for (size_t i = 1; i < versions.size(); ++i) {
        if (winner.version_id < versions[i].version_id) {
            winner = versions[i];
        }
    }

    return winner;
}
```

### Read Latency

**Best Case**: `R=1`, coordinator is local replica
- Latency: Single local read (< 1ms)

**Typical Case**: `R=2`, N=3, same datacenter
- Latency: Local read + 1 network RTT (~5-10ms)

**Worst Case**: `R=3`, N=3, cross-datacenter
- Latency: Local read + 2 network RTTs + reconciliation (~100-200ms)

### Read Consistency

**Strong Consistency** (`W + R > N`):
- Guarantees: Read sees latest committed write
- Proof: Quorum overlap ensures at least one replica has latest version

**Example**: N=3, W=2, R=2
- Write to {Node A, Node B} (W=2)
- Read from {Node B, Node C} (R=2)
- Overlap: Node B has latest version → Read returns latest

**Eventual Consistency** (`W + R ≤ N`):
- Guarantees: Read may see stale data (older version)
- Convergence: Anti-entropy repairs divergence over time

**Example**: N=3, W=1, R=1
- Write to {Node A} (W=1)
- Read from {Node C} (R=1, doesn't have latest yet)
- Result: Stale read (acceptable for some workloads)

---

## Hinted Handoff

### Purpose

**Problem**: Write quorum achieved (W nodes acknowledged), but some replicas were unavailable (down, network partition, slow).

**Solution**: Coordinator stores "hint" (pending write) and delivers it when failed replica recovers.

### Hinted Handoff Workflow

**Scenario**: N=3, W=2, write to {Node A, Node B, Node C}

```
1. Write sent to all replicas:
   - Node A: ACK (success)
   - Node B: ACK (success)
   - Node C: TIMEOUT (down)

2. Quorum achieved (W=2), write succeeds

3. Coordinator stores hint for Node C:
   Hint = {
       target_replica: Node C,
       shard_id: 001,
       row_data: {sensor_id='X', value=42.5},
       version_id: (ts=1000, counter=5, node_id=1),
       xid: 12345,
       timestamp: 1704067200000
   }

4. Hint stored in coordinator's local hint log (durable)

5. Background hint delivery process:
   LOOP (every 10 seconds):
       FOR each hint in hint_log:
           IF target_replica is reachable:
               Send write to target_replica
               IF success:
                   Delete hint from hint_log
               END IF
           END IF
       END FOR
   END LOOP
```

### Hint Storage

**Hint Log Location**: `/var/lib/scratchbird/hints/{node_id}/`

**File Format**: Append-only log per target replica

```
hints_node_c.log:
  [Hint 1] shard=001, version_id=..., row_data=..., timestamp=...
  [Hint 2] shard=002, version_id=..., row_data=..., timestamp=...
  ...
```

**Hint Expiration**: Hints older than 7 days are discarded (configurable: `hint_expiration_days`)

**Hint Replay**: When Node C recovers, coordinators replay all pending hints

### Hint Delivery Pseudocode

```cpp
class HintedHandoffManager {
public:
    // Store hint when replica unavailable
    auto storeHint(const ReplicaId& target, const WritePayload& payload) -> void {
        Hint hint{
            .target_replica = target,
            .shard_id = payload.shard_id,
            .row_data = payload.row_data,
            .version_id = payload.version_id,
            .xid = payload.xid,
            .timestamp = currentTimeMs()
        };

        hint_log_.append(hint);  // Durable write to hint log
    }

    // Background delivery loop
    auto deliverHints() -> void {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(10));

            auto hints = hint_log_.readAll();
            for (const auto& hint : hints) {
                // Check if target replica is reachable
                if (isReplicaReachable(hint.target_replica)) {
                    auto status = sendWriteToReplica(hint.target_replica, hint);
                    if (status.ok()) {
                        hint_log_.remove(hint);  // Hint delivered successfully
                    }
                }

                // Check expiration
                if (currentTimeMs() - hint.timestamp > hint_expiration_ms_) {
                    hint_log_.remove(hint);  // Discard expired hint
                    LOG_WARNING("Hint expired: target={}, shard={}", hint.target_replica, hint.shard_id);
                }
            }
        }
    }

private:
    HintLog hint_log_;
    std::atomic<bool> running_{true};
    uint64_t hint_expiration_ms_ = 7 * 24 * 3600 * 1000;  // 7 days
};
```

### Hint Limitations

**Not a Replacement for Anti-Entropy**:
- Hints are best-effort (coordinator can crash, losing hints)
- Anti-entropy (Merkle trees) is the authoritative divergence detection

**Hint Storms**:
- If many coordinators have hints for same replica, delivery can overwhelm target
- Mitigation: Rate limit hint delivery (max 1000 hints/sec per target)

---

## Failure Handling

### Node Failure

**Scenario**: One replica fails (crashes, network partition, maintenance)

**Behavior**:
- **Write Path**: If W replicas still reachable, write succeeds (hints stored for failed replica)
- **Read Path**: If R replicas still reachable, read succeeds
- **Recovery**: Failed node rejoins cluster, receives hints + anti-entropy repairs

**Example**: N=3, W=2, R=2, Node C fails
- Write: Succeeds if {Node A, Node B} acknowledge (W=2)
- Read: Succeeds if {Node A, Node B} respond (R=2)
- Node C recovers: Receives hints from coordinators + Merkle tree sync

### Coordinator Failure

**Scenario**: Coordinator crashes mid-write (after sending writes to replicas, before responding to client)

**Behavior**:
- **Client**: Timeout or connection reset (doesn't know if write succeeded)
- **Replicas**: Some may have applied write, some may not
- **Recovery**: Client retries write (idempotency via UUIDv8-HLC)

**Idempotency**:
- Client regenerates same UUIDv8-HLC (or uses deterministic ID)
- Replicas detect duplicate: `version_id` already exists → No-op

**Example**:
```
1. Client sends write, coordinator crashes after Node A applied
2. Client retries write to new coordinator (Node B)
3. Node B sends write to {Node A, Node B, Node C}
4. Node A: version_id already exists → Return ACK (idempotent)
5. Node B, Node C: Apply write normally
6. Quorum achieved, write succeeds
```

### Network Partition

**Scenario**: Cluster splits into two partitions (e.g., {Node A, Node B} and {Node C})

**Behavior** (depends on quorum configuration):

**Partition 1: {Node A, Node B}** (W=2, N=3)
- Write: Succeeds if both nodes in partition (W=2 achieved)
- Read: Succeeds if both nodes in partition (R=2 achieved)

**Partition 2: {Node C}** (W=2, N=3)
- Write: **FAILS** (only 1 node, cannot achieve W=2)
- Read: **FAILS** (only 1 node, cannot achieve R=2)

**Result**: Partition 1 is available (can serve reads/writes), Partition 2 is unavailable.

**Recovery**:
- Network partition heals
- Anti-entropy detects divergence between partitions
- Conflict resolution via UUIDv8-HLC (deterministic convergence)

### Quorum Unavailability

**Scenario**: Too many nodes fail to achieve quorum

**Example**: N=3, W=2, R=2, Nodes B and C both fail
- Only Node A remains (1 < W=2, 1 < R=2)
- **Write**: FAIL (QuorumNotAchieved error)
- **Read**: FAIL (QuorumNotAchieved error)

**Client Response**:
```json
{
  "error": "QuorumNotAchieved",
  "message": "Write requires W=2 nodes, only 1 available",
  "available_replicas": 1,
  "required_replicas": 2
}
```

**Operator Response**:
- Reduce W/R temporarily: `ALTER TABLE sensor_data SET write_quorum=1;`
- Or add new replicas to cluster (SBCLUSTER-02 membership)

---

## Consistency Guarantees

### Read-Your-Writes

**Guarantee**: After a client writes, subsequent reads by that client see the write.

**Mechanism**:
1. Client writes with UUIDv8-HLC: `version_id = (ts=1000, counter=5)`
2. Write succeeds (W replicas acknowledged)
3. Client reads from R replicas, at least one has `version_id`
4. Coordinator returns `version_id` (highest HLC)

**Condition**: `W + R > N` (quorum overlap ensures at least one replica has write)

**Example**: N=3, W=2, R=2
- Write to {Node A, Node B} → version_id stored
- Read from {Node B, Node C} → Node B has version_id → Returned

### Monotonic Reads

**Guarantee**: If client reads version V1, subsequent reads return V1 or later (never earlier).

**Mechanism**:
1. Client reads version_id=V1
2. Client sends V1 with next read request: `READ WHERE version_id >= V1`
3. Coordinator filters versions older than V1

**Implementation**:
```sql
-- Client tracks last seen version
client.last_version_id = version_id_from_previous_read;

-- Next read includes version constraint
SELECT * FROM sensor_data
WHERE sensor_id = 'X'
  AND version_id >= :last_version_id;
```

### Monotonic Writes

**Guarantee**: Client's writes are applied in order (write W1 before write W2 → W1's version < W2's version).

**Mechanism**: UUIDv8-HLC monotonicity
- Write W1: `version_id_1 = (ts=1000, counter=5)`
- Write W2: `version_id_2 = (ts=1000, counter=6)` (counter incremented)
- HLC guarantees: `version_id_1 < version_id_2`

**Causal Order Preserved**: If W2 depends on W1 (e.g., read W1's value before writing W2), HLC ensures correct ordering.

### Causal Consistency

**Guarantee**: If operation A causally precedes operation B, all nodes see A before B.

**Mechanism**: HLC causal tracking
1. Node X writes with HLC: `hlc_x = (ts=1000, counter=5)`
2. Node Y receives HLC via replication message
3. Node Y updates local HLC: `updateHlcFromRemote(1000, 5)`
4. Node Y writes with new HLC: `hlc_y = (ts=1000, counter=6)`
5. All nodes see: `hlc_x < hlc_y` → A before B

**Violation Prevention**: Without HLC, clock skew could cause Node Y to write with `ts=999` (earlier than Node X's `ts=1000`), breaking causality.

### Eventual Consistency

**Guarantee**: Given no new writes, all replicas converge to same state.

**Mechanism**:
1. **Quorum writes**: W replicas have write immediately
2. **Hinted handoff**: Missing replicas receive hints when they recover
3. **Anti-entropy**: Merkle tree comparison detects divergence, triggers repair
4. **Conflict resolution**: UUIDv8-HLC deterministic ordering ensures same winner on all nodes

**Convergence Time**: Typically < 1 minute (depends on anti-entropy interval, hint delivery rate)

---

## Integration with SBCLUSTER-07

### SBCLUSTER-07 Replication Overview

**Existing Modes** (SBCLUSTER-07-REPLICATION.md):

**Mode 1: Logical Replication**
- Primary streams logical changes (INSERT, UPDATE, DELETE) to replicas
- Replicas apply SQL statements
- Specified in Section 4.1 of SBCLUSTER-07

**Mode 2: Physical Replication**
- Primary streams physical pages (block-level) to replicas
- Replicas apply page writes directly to storage
- Specified in Section 4.2 of SBCLUSTER-07

### Mode 3 Extension

**Mode 3: Leaderless Quorum Replication** (this document)
- Extends SBCLUSTER-07 with multi-master capability
- Reuses commit log streaming infrastructure for hint delivery
- Adds UUIDv8-HLC for conflict resolution
- Adds quorum coordination layer

**Shared Components**:
- **Commit Log**: Mode 3 uses commit log for hint persistence
- **Gossip Protocol**: Mode 3 reuses gossip for replica discovery
- **Security Bundle**: Mode 3 uses same TLS certificates for replica authentication

**New Components**:
- **Quorum Coordinator**: Manages W/R quorum coordination
- **Hinted Handoff Manager**: Stores and delivers hints
- **Conflict Resolver**: Applies UUIDv8-HLC ordering

### Configuration

**Catalog Schema Extension**:

```sql
-- sys_tables: Add replication mode column
ALTER TABLE sys_tables ADD COLUMN replication_mode VARCHAR(20) DEFAULT 'primary_replica';
-- Values: 'primary_replica' (Mode 1/2), 'leaderless_quorum' (Mode 3)

ALTER TABLE sys_tables ADD COLUMN replication_factor INTEGER DEFAULT 1;
ALTER TABLE sys_tables ADD COLUMN write_quorum INTEGER DEFAULT 1;
ALTER TABLE sys_tables ADD COLUMN read_quorum INTEGER DEFAULT 1;
```

**Table Creation**:

```sql
-- Mode 1/2 (Primary-Replica, default)
CREATE TABLE users (
    user_id UUID PRIMARY KEY,
    name VARCHAR(100)
);
-- Implicitly: replication_mode='primary_replica'

-- Mode 3 (Leaderless Quorum)
CREATE TABLE sensor_data (
    sensor_id UUID PRIMARY KEY,
    value DOUBLE
) WITH (
    replication_mode = 'leaderless_quorum',
    replication_factor = 3,
    write_quorum = 2,
    read_quorum = 2,
    uuid_type = 'v8_hlc'  -- Required for Mode 3
);
```

**Runtime Changes**:

```sql
-- Alter replication mode (requires table lock)
ALTER TABLE sensor_data SET replication_mode = 'leaderless_quorum';

-- Adjust quorum parameters (online, no lock)
ALTER TABLE sensor_data SET write_quorum = 3;
ALTER TABLE sensor_data SET read_quorum = 1;
```

### Compatibility Matrix

| Feature | Mode 1 (Logical) | Mode 2 (Physical) | Mode 3 (Leaderless) |
|---------|------------------|-------------------|---------------------|
| UUID Type | UUIDv7 | UUIDv7 | UUIDv8-HLC (required) |
| Write Latency | Low (single primary) | Low (single primary) | Medium (quorum wait) |
| Read Latency | Low (local replica) | Low (local replica) | Medium (quorum reconciliation) |
| Write Availability | Low (primary SPOF) | Low (primary SPOF) | High (any node writes) |
| Consistency | Strong (single writer) | Strong (single writer) | Eventual (tunable with W+R) |
| Failover | Manual promotion | Fast promotion | Automatic (no leader) |
| Cross-DC | Async replication | Async replication | Quorum across regions |

---

## Testing and Verification

### Unit Tests

**Quorum Coordination Test**:

```cpp
TEST(LeaderlessQuorum, WriteQuorumSuccess) {
    ReplicationCoordinator coordinator;
    ReplicaSet replicas = {Node("A"), Node("B"), Node("C")};

    WriteRequest req{/* shard_id=1, data=... */};
    auto status = coordinator.coordinateWrite(req, replicas, /*W=*/2);

    ASSERT_TRUE(status.ok());
    // Verify at least 2 replicas have the write
}

TEST(LeaderlessQuorum, WriteQuorumFailure) {
    ReplicationCoordinator coordinator;
    ReplicaSet replicas = {Node("A"), Node("B"), Node("C")};
    // Simulate Node B and Node C offline
    Node("B").setOffline(true);
    Node("C").setOffline(true);

    WriteRequest req{/* ... */};
    auto status = coordinator.coordinateWrite(req, replicas, /*W=*/2);

    ASSERT_FALSE(status.ok());
    ASSERT_EQ(status.code(), StatusCode::QuorumNotAchieved);
}
```

**Hinted Handoff Test**:

```cpp
TEST(HintedHandoff, HintDeliveryOnRecovery) {
    HintedHandoffManager mgr;
    Node("C").setOffline(true);

    // Write fails to reach Node C
    WritePayload payload{/* ... */};
    mgr.storeHint(Node("C"), payload);

    // Node C recovers
    Node("C").setOffline(false);

    // Deliver hints
    mgr.deliverHints();

    // Verify Node C received the write
    ASSERT_TRUE(Node("C").hasWrite(payload.version_id));
}
```

### Integration Tests

**Multi-Node Conflict Resolution**:

```cpp
TEST(LeaderlessQuorumIntegration, ConflictResolution) {
    // Setup: 3-node cluster (N=3, W=2, R=2)
    Cluster cluster;
    cluster.createTable("sensor_data", {
        .replication_mode = "leaderless_quorum",
        .N = 3, .W = 2, .R = 2,
        .uuid_type = "v8_hlc"
    });

    // Concurrent writes (network partition)
    Node("A").execute("INSERT INTO sensor_data VALUES (gen_uuidv8_hlc(), 42.5)");
    Node("B").execute("INSERT INTO sensor_data VALUES (gen_uuidv8_hlc(), 99.9)");

    // Trigger anti-entropy
    cluster.performAntiEntropy();

    // Verify convergence: all nodes have same winner
    auto result_a = Node("A").query("SELECT * FROM sensor_data");
    auto result_b = Node("B").query("SELECT * FROM sensor_data");
    auto result_c = Node("C").query("SELECT * FROM sensor_data");

    ASSERT_EQ(result_a, result_b);
    ASSERT_EQ(result_b, result_c);
    // Winner determined by UUIDv8-HLC comparison
}
```

### Chaos Tests

**Network Partition Test**:

```cpp
TEST(ChaosTest, NetworkPartition) {
    // Setup: 5-node cluster (N=5, W=3, R=3)
    Cluster cluster(5);

    // Simulate partition: {Node A, Node B, Node C} vs {Node D, Node E}
    cluster.createPartition({Node("A"), Node("B"), Node("C")}, {Node("D"), Node("E")});

    // Partition 1: Can achieve W=3 (all 3 nodes)
    auto status_p1 = Node("A").execute("INSERT INTO sensor_data VALUES (gen_uuidv8_hlc(), 42.5)");
    ASSERT_TRUE(status_p1.ok());

    // Partition 2: Cannot achieve W=3 (only 2 nodes)
    auto status_p2 = Node("D").execute("INSERT INTO sensor_data VALUES (gen_uuidv8_hlc(), 99.9)");
    ASSERT_FALSE(status_p2.ok());

    // Heal partition
    cluster.healPartition();

    // Verify convergence
    cluster.performAntiEntropy();
    auto result_a = Node("A").query("SELECT COUNT(*) FROM sensor_data");
    ASSERT_EQ(result_a, 1);  // Only Partition 1's write persisted
}
```

**Node Failure Test**:

```cpp
TEST(ChaosTest, NodeFailureRecovery) {
    Cluster cluster(3);  // N=3, W=2, R=2

    // Write succeeds (Node A, Node B acknowledge)
    Node("A").execute("INSERT INTO sensor_data VALUES (gen_uuidv8_hlc(), 42.5)");

    // Node C crashes (misses write)
    Node("C").crash();

    // Node C recovers
    Node("C").recover();

    // Verify Node C receives hint or anti-entropy repair
    std::this_thread::sleep_for(std::chrono::seconds(15));  // Wait for hint delivery
    auto result_c = Node("C").query("SELECT * FROM sensor_data");
    ASSERT_EQ(result_c.size(), 1);  // Node C has the write
}
```

### Performance Benchmarks

**Write Throughput** (target: 50K TPS per shard):

```bash
# Benchmark: N=3, W=2, R=2, 10M writes
./scratchbird_benchmark --mode=leaderless_quorum --writes=10000000 --N=3 --W=2 --R=2
# Expected: > 50,000 writes/sec per shard
```

**Read Latency** (target: p99 < 20ms, same DC):

```bash
# Benchmark: N=3, W=2, R=2, 1M reads
./scratchbird_benchmark --mode=leaderless_quorum --reads=1000000 --N=3 --W=2 --R=2
# Expected: p50 < 5ms, p99 < 20ms
```

---

## Appendix: Comparison with Industry Systems

### Amazon Dynamo

**Similarities**:
- Leaderless replication with quorum (W + R > N)
- Hinted handoff for failed replicas
- Anti-entropy via Merkle trees

**Differences**:
- Dynamo uses vector clocks (complex, unbounded size)
- ScratchBird uses UUIDv8-HLC (fixed size, simpler)
- Dynamo uses consistent hashing for partitioning
- ScratchBird uses schema-driven colocation (multi-table atomicity)

### Apache Cassandra

**Similarities**:
- Tunable consistency (W, R parameters)
- Gossip protocol for membership
- Leaderless multi-master writes

**Differences**:
- Cassandra uses hash-based Merkle trees (high CPU overhead)
- ScratchBird uses time-partitioned Merkle trees (< 2% CPU)
- Cassandra uses LWW with physical timestamps (clock skew issues)
- ScratchBird uses HLC (causal ordering)

### CockroachDB

**Similarities**:
- Uses HLC for causal ordering
- Multi-region replication

**Differences**:
- CockroachDB uses Raft consensus (CP system, strong consistency)
- ScratchBird Mode 3 uses quorum (AP system, eventual consistency)
- CockroachDB has single leader per range
- ScratchBird Mode 3 is truly leaderless

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Dependencies:** 01_UUIDV8_HLC_ARCHITECTURE.md, SBCLUSTER-07-REPLICATION.md

---

**End of Document 02**
