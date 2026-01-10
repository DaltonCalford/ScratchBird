# MGA Integration for Distributed Replication

**Document:** 05_MGA_INTEGRATION.md
**Status:** BETA SPECIFICATION
**Version:** 1.0
**Date:** 2026-01-09
**Authority:** Chief Architect

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [MGA vs MVCC Review](#mga-vs-mvcc-review)
3. [TIP-Based Distributed Visibility](#tip-based-distributed-visibility)
4. [Cross-Node Transaction Coordination](#cross-node-transaction-coordination)
5. [Garbage Collection Coordination](#garbage-collection-coordination)
6. [Commit Log Integration](#commit-log-integration)
7. [Transaction Markers in Distributed Setting](#transaction-markers-in-distributed-setting)
8. [Integration with Existing Implementation](#integration-with-existing-implementation)
9. [Testing and Verification](#testing-and-verification)

---

## Executive Summary

### Purpose

This specification defines how ScratchBird's **Firebird MGA (Multi-Generational Architecture)** transaction model integrates with Beta's distributed replication features (leaderless quorum, UUIDv8-HLC, time-partitioned Merkle forests, schema colocation). The design preserves MGA principles (TIP-based visibility, in-place updates, no snapshots) while extending them to multi-node environments.

**Critical Constraint**: **NO PostgreSQL MVCC assumptions**. ScratchBird uses Firebird MGA exclusively.

### Design Principles

1. **TIP-Based Visibility**: Transaction Inventory Pages (TIP) remain authoritative for visibility decisions
2. **No Snapshots**: MGA visibility via TIP state checks, not snapshot timestamps (like PostgreSQL)
3. **In-Place Updates**: Rows updated in-place with back versions, not append-only (like PostgreSQL)
4. **Distributed TIP**: Each node maintains local TIP, synchronized via replication
5. **HLC for Causality**: UUIDv8-HLC provides causal ordering, TIP provides visibility

### Key Concepts

**MGA Visibility Rule** (Single-Node):
```
Version is visible to transaction T if:
  1. version_xid is COMMITTED (checked via TIP)
  AND
  2. version_xid < T's xid (transaction order)
```

**MGA Visibility Rule** (Distributed):
```
Version is visible to transaction T if:
  1. version_xid is COMMITTED (checked via local TIP)
  AND
  2. version_xid's HLC < T's HLC (causal order)
  AND
  3. Conflict resolution applied (UUIDv8-HLC comparison if multiple versions)
```

---

## MGA vs MVCC Review

### Firebird MGA (ScratchBird's Model)

**Transaction Inventory Pages (TIP)**:
- 2-bit state per transaction: ACTIVE, COMMITTED, ABORTED, PREPARED
- Stored on disk (durable)
- Checked on every visibility decision

**Visibility**:
- Reader checks TIP: `isVersionVisible(version_xid, reader_xid)`
- If version_xid COMMITTED and version_xid < reader_xid → Visible

**Storage**:
- In-place updates: New version overwrites old (back versions chained)
- No append-only log (unlike PostgreSQL)

**Garbage Collection**:
- Sweep: Reclaim back versions when no active transactions reference them
- Based on OIT (Oldest Interesting Transaction)

**Example**:

```
Table: users (user_id, name, version_xid)

Transaction T1 (xid=100):
  INSERT INTO users VALUES ('user_123', 'Alice', 100)
  COMMIT  → TIP[100] = COMMITTED

Transaction T2 (xid=105):
  UPDATE users SET name='Alice Updated' WHERE user_id='user_123'
  → Creates back version: ('user_123', 'Alice', 100) [back pointer]
  → Primary version: ('user_123', 'Alice Updated', 105)
  COMMIT  → TIP[105] = COMMITTED

Transaction T3 (xid=110, started before T2 committed):
  SELECT * FROM users WHERE user_id='user_123'
  → Checks TIP[105]: Is T2 committed? YES
  → But T3's xid (110) > T2's xid (105)? YES
  → Visible: 'Alice Updated' (T3 sees T2's update)

Garbage Collection:
  When no transaction has xid < 100, back version ('Alice', 100) can be swept
```

### PostgreSQL MVCC (NOT ScratchBird)

**Snapshots**:
- Reader takes snapshot: `(xmin, xmax, active_xids[])`
- Snapshot stored with transaction

**Visibility**:
- Check if version_xid in snapshot's active_xids[]
- No TIP lookups (visibility encoded in snapshot)

**Storage**:
- Append-only: Old versions kept in heap, new versions appended
- Vacuum reclaims space

**Why ScratchBird Doesn't Use MVCC**:
1. Snapshots add memory overhead (active_xids[] array)
2. Append-only storage wastes space (no in-place updates)
3. TIP-based visibility is simpler (2 bits per transaction, durable)

---

## TIP-Based Distributed Visibility

### Single-Node TIP (Existing)

**Structure** (transaction_manager.h):

```cpp
struct TIPPageHeader {
    uint64_t min_xid;       // Minimum XID in this page
    uint64_t max_xid;       // Maximum XID in this page
    uint32_t num_transactions;
};

struct TIPEntry {
    uint64_t xid;           // Transaction ID
    uint8_t state;          // TransactionState (ACTIVE, COMMITTED, ABORTED, PREPARED)
    uint64_t commit_time;   // Commit timestamp (milliseconds)
};

enum class TransactionState : uint8_t {
    ACTIVE = 0,
    COMMITTED = 1,
    ABORTED = 2,
    PREPARED = 3  // 2PC prepared state
};
```

**Visibility Check** (transaction_manager.cpp):

```cpp
auto TransactionManager::isVersionVisible(
    uint64_t version_xid,
    uint64_t reader_xid
) const -> bool {
    // 1. Check TIP state
    auto state = getTIPState(version_xid);
    if (state != TransactionState::COMMITTED) {
        return false;  // Not committed → Invisible
    }

    // 2. Check transaction order
    if (version_xid >= reader_xid) {
        return false;  // Future transaction → Invisible
    }

    return true;  // Committed and in past → Visible
}
```

### Distributed TIP Extension

**Challenge**: In distributed environment, each node has local TIP (per-shard).

**Requirement**: Reader on Node A must determine visibility of version written on Node B.

**Solution**: Replicate TIP entries across nodes (via replication log).

#### TIP Replication Protocol

**Write Path** (Node A writes, replicates to Node B):

```
Node A:
  1. Allocate xid: xid_a = 12345
  2. Mark TIP[12345] = ACTIVE
  3. Execute INSERT/UPDATE
  4. Commit: Mark TIP[12345] = COMMITTED
  5. Generate replication message:
     {
       shard_id: 001,
       xid: 12345,
       state: COMMITTED,
       commit_time: 1704067200000,
       hlc: (ts=1704067200000, counter=5),
       row_data: {...}
     }
  6. Send to replicas (Node B, Node C)

Node B:
  1. Receive replication message
  2. Check if TIP[12345] exists locally
     - If not: Allocate TIP[12345] (reserve XID)
  3. Apply row_data (INSERT/UPDATE locally)
  4. Mark TIP[12345] = COMMITTED (same state as Node A)
```

**Key Insight**: XIDs are **globally unique** (allocated per-node, but replicated XIDs preserved).

#### Global XID Allocation

**Problem**: Node A allocates xid=12345, Node B allocates xid=12345 → Collision!

**Solution**: Partition XID space by node_id.

```cpp
auto TransactionManager::allocateXid() -> uint64_t {
    // XID format: [node_id (16 bits)] [local_counter (48 bits)]
    uint64_t node_id_bits = static_cast<uint64_t>(node_id_) << 48;
    uint64_t local_xid = next_xid_counter_.fetch_add(1, std::memory_order_relaxed);
    return node_id_bits | local_xid;
}
```

**Example**:
- Node 1: XIDs 0x0001_0000_0000_0001, 0x0001_0000_0000_0002, ...
- Node 2: XIDs 0x0002_0000_0000_0001, 0x0002_0000_0000_0002, ...
- No collisions!

**XID Extraction**:

```cpp
auto extractNodeId(uint64_t xid) -> uint16_t {
    return static_cast<uint16_t>(xid >> 48);
}

auto extractLocalXid(uint64_t xid) -> uint64_t {
    return xid & 0x0000_FFFF_FFFF_FFFF;
}
```

#### Distributed Visibility Check

```cpp
auto TransactionManager::isVersionVisibleDistributed(
    uint64_t version_xid,
    uint64_t reader_xid,
    const HLC& reader_hlc
) const -> bool {
    // 1. Check TIP state (local or remote)
    auto state = getTIPState(version_xid);
    if (state != TransactionState::COMMITTED) {
        return false;  // Not committed → Invisible
    }

    // 2. Check causal order (HLC, not just XID)
    auto version_hlc = getVersionHLC(version_xid);  // Lookup HLC from version_id
    if (version_hlc >= reader_hlc) {
        return false;  // Future event (causally) → Invisible
    }

    return true;  // Committed and causally in past → Visible
}
```

**Key Difference**: Use HLC for causal ordering, not XID (XIDs may be out-of-order across nodes).

---

## Cross-Node Transaction Coordination

### Local Transactions (Single-Shard)

**Scenario**: Transaction updates multiple tables colocated on same shard.

**Coordination**: Local TIP (existing transaction_manager.cpp logic, no changes).

```sql
-- Example: User + Orders colocated on Shard 7
BEGIN TRANSACTION;
INSERT INTO users (user_id, name) VALUES ('user_123', 'Alice');
INSERT INTO orders (order_id, user_id, amount) VALUES ('order_456', 'user_123', 100.0);
COMMIT;

-- Execution (Node A, Shard 7):
--   1. Allocate xid=0x0001_0000_0000_1234
--   2. Mark TIP[xid] = ACTIVE
--   3. Execute both INSERTs
--   4. Mark TIP[xid] = COMMITTED (single commit)
```

### Cross-Shard Transactions (2PC)

**Scenario**: Transaction spans multiple shards (not colocated).

**Coordination**: Two-Phase Commit (2PC) using TIP PREPARED state.

**Example**:

```sql
-- Transfer between accounts on different shards
BEGIN TRANSACTION;
UPDATE accounts SET balance = balance - 100 WHERE account_id='A';  -- Shard 3
UPDATE accounts SET balance = balance + 100 WHERE account_id='B';  -- Shard 7
COMMIT;
```

**2PC Workflow**:

```
Coordinator (Node A):
  1. Phase 1: PREPARE
     - Send PREPARE to Shard 3 (account_A)
     - Send PREPARE to Shard 7 (account_B)

Shard 3 (Node A):
  1. Allocate xid=0x0001_0000_0000_5678
  2. Mark TIP[xid] = ACTIVE
  3. Execute UPDATE (account_A balance -= 100)
  4. Mark TIP[xid] = PREPARED  ← Not COMMITTED yet!
  5. Respond to coordinator: PREPARED

Shard 7 (Node B):
  1. Allocate xid=0x0002_0000_0000_9ABC
  2. Mark TIP[xid] = ACTIVE
  3. Execute UPDATE (account_B balance += 100)
  4. Mark TIP[xid] = PREPARED
  5. Respond to coordinator: PREPARED

Coordinator (Node A):
  2. Phase 2: COMMIT (if all participants PREPARED)
     - Send COMMIT to Shard 3
     - Send COMMIT to Shard 7

Shard 3 (Node A):
  1. Mark TIP[xid=5678] = COMMITTED  ← Now visible!

Shard 7 (Node B):
  1. Mark TIP[xid=9ABC] = COMMITTED
```

**TIP State Machine**:

```
ACTIVE → PREPARED → COMMITTED (2PC success)
ACTIVE → PREPARED → ABORTED (2PC failure)
ACTIVE → COMMITTED (local transaction, no 2PC)
ACTIVE → ABORTED (rollback)
```

### Integration with Existing 2PC

**ScratchBird's transaction_manager.cpp already implements 2PC**:

```cpp
// From transaction_manager.h
auto prepareTransaction(uint64_t xid, ErrorContext* ctx) -> Status;
auto commitPreparedTransaction(uint64_t xid, ErrorContext* ctx) -> Status;
auto abortPreparedTransaction(uint64_t xid, ErrorContext* ctx) -> Status;
```

**Extension for Distributed 2PC**:

```cpp
class DistributedTransactionCoordinator {
public:
    auto executeDistributedTransaction(
        const std::vector<ShardOperation>& operations
    ) -> Status {
        std::vector<PreparedShard> prepared_shards;

        // Phase 1: PREPARE
        for (const auto& op : operations) {
            auto shard_id = op.shard_id;
            auto status = sendPrepare(shard_id, op);
            if (!status.ok()) {
                // Abort all prepared shards
                for (const auto& shard : prepared_shards) {
                    sendAbort(shard.shard_id, shard.xid);
                }
                return Status::PreparePhaseAborted();
            }
            prepared_shards.push_back({shard_id, op.xid});
        }

        // Phase 2: COMMIT
        for (const auto& shard : prepared_shards) {
            sendCommit(shard.shard_id, shard.xid);
        }

        return Status::OK();
    }

private:
    auto sendPrepare(ShardId shard_id, const ShardOperation& op) -> Status;
    auto sendCommit(ShardId shard_id, uint64_t xid) -> Status;
    auto sendAbort(ShardId shard_id, uint64_t xid) -> Status;
};
```

---

## Garbage Collection Coordination

### Single-Node GC (Existing)

**Firebird Sweep** (transaction_manager.cpp):

```cpp
auto TransactionManager::performSweep() -> Status {
    // 1. Determine OIT (Oldest Interesting Transaction)
    uint64_t oit = oldest_xid_;

    // 2. Find oldest active transaction
    uint64_t oat = oldest_active_xid_;

    // 3. Sweep back versions older than OIT
    for (auto& page : heap_pages_) {
        for (auto& row : page.rows) {
            if (row.has_back_version() && row.back_version_xid < oit) {
                // No active transaction can see this back version
                row.removeBackVersion();  // Reclaim space
            }
        }
    }

    return Status::OK();
}
```

**Transaction Markers**:
- **OIT** (Oldest Interesting Transaction): Oldest transaction that might be visible
- **OAT** (Oldest Active Transaction): Oldest transaction still running
- **OST** (Oldest Snapshot Transaction): Oldest transaction that took a snapshot (N/A in MGA)

### Distributed GC Coordination

**Challenge**: Each node has local OIT/OAT, but back versions may be visible to transactions on other nodes.

**Solution**: Compute **global OIT** (minimum OIT across all nodes).

#### Global OIT Computation

**Gossip Protocol** (every 60 seconds):

```
Node A:
  local_oit = 10000
  local_oat = 10500

Node B:
  local_oit = 9500   ← Lower than Node A
  local_oat = 10200

Node C:
  local_oit = 10300
  local_oat = 10600

Global OIT = min(Node A.oit, Node B.oit, Node C.oit) = 9500
```

**Implementation**:

```cpp
class DistributedGarbageCollector {
public:
    auto computeGlobalOIT(const std::vector<NodeId>& nodes) -> uint64_t {
        uint64_t global_oit = UINT64_MAX;

        for (const auto& node : nodes) {
            auto local_oit = fetchOIT(node);  // RPC call
            global_oit = std::min(global_oit, local_oit);
        }

        return global_oit;
    }

    auto performDistributedSweep(ShardId shard_id) -> Status {
        // 1. Compute global OIT
        auto nodes = getReplicasForShard(shard_id);
        auto global_oit = computeGlobalOIT(nodes);

        // 2. Sweep back versions older than global OIT
        for (auto& page : heap_pages_[shard_id]) {
            for (auto& row : page.rows) {
                if (row.has_back_version() && row.back_version_xid < global_oit) {
                    row.removeBackVersion();
                }
            }
        }

        return Status::OK();
    }

private:
    auto fetchOIT(NodeId node) -> uint64_t;  // Gossip or RPC
};
```

**Frequency**: Every 10 minutes (configurable: `distributed_gc_interval_ms = 600000`)

#### GC Coordination Message

```cpp
struct GCCoordinationMessage {
    NodeId sender_node_id;
    ShardId shard_id;
    uint64_t local_oit;
    uint64_t local_oat;
    uint64_t timestamp_ms;
};
```

**Gossip Exchange**:

```
Node A → Node B:
  GCCoordinationMessage {
    sender_node_id: 1,
    shard_id: 001,
    local_oit: 10000,
    local_oat: 10500,
    timestamp_ms: 1704067200000
  }

Node B → Node A:
  GCCoordinationMessage {
    sender_node_id: 2,
    shard_id: 001,
    local_oit: 9500,
    local_oat: 10200,
    timestamp_ms: 1704067200050
  }
```

### Conservative GC Strategy

**Rule**: Never sweep back versions that might be visible to any transaction on any node.

**Safety**: Use global OIT (minimum across all nodes).

**Trade-off**: More conservative than single-node GC (keeps back versions longer), but guarantees correctness.

---

## Commit Log Integration

### Commit Log Purpose

**NOT Write-after Log (WAL)**:
- ScratchBird MGA uses TIP for crash recovery, not write-after log (WAL) replay
- "Commit log" = replication log (streamed after commit)

**Purpose**:
- Replicate committed transactions to other nodes
- Enable hinted handoff (pending writes for failed replicas)

### Commit Log Format

```cpp
struct CommitLogEntry {
    uint64_t xid;                   // Transaction ID (globally unique)
    ShardId shard_id;               // Shard this transaction modified
    uint64_t commit_time_ms;        // Physical commit time
    HLC hlc;                        // Hybrid Logical Clock
    TransactionState state;         // COMMITTED or ABORTED
    std::vector<RowChange> changes; // Row-level changes
};

struct RowChange {
    std::string table_name;
    UuidV8HLC row_id;               // Primary key
    ChangeType type;                // INSERT, UPDATE, DELETE
    std::map<std::string, TypedValue> new_values;  // New column values
    std::map<std::string, TypedValue> old_values;  // Old values (for rollback)
};
```

**Example Commit Log Entry**:

```
CommitLogEntry {
    xid: 0x0001_0000_0000_1234,
    shard_id: 001,
    commit_time_ms: 1704067200000,
    hlc: (ts=1704067200000, counter=5),
    state: COMMITTED,
    changes: [
        {
            table_name: "users",
            row_id: (UUIDv8-HLC: ts=1704067200000, counter=5, node_id=1),
            type: INSERT,
            new_values: {user_id: 'user_123', name: 'Alice'},
            old_values: {}
        }
    ]
}
```

### Commit Log Streaming

**Write Path**:

```
Node A (primary replica):
  1. Execute transaction
  2. Mark TIP[xid] = COMMITTED
  3. Append to local commit log
  4. Stream commit log entry to replicas (Node B, Node C)

Node B (replica):
  1. Receive commit log entry
  2. Apply changes to local storage
  3. Mark TIP[xid] = COMMITTED (replicate TIP state)
  4. Acknowledge to Node A
```

**Implementation** (extends SBCLUSTER-07):

```cpp
class CommitLogStreamer {
public:
    auto streamCommitLogEntry(
        const CommitLogEntry& entry,
        const std::vector<NodeId>& replicas
    ) -> void {
        for (const auto& replica : replicas) {
            std::async(std::launch::async, [&]() {
                sendCommitLogEntry(replica, entry);
            });
        }
    }

private:
    auto sendCommitLogEntry(NodeId replica, const CommitLogEntry& entry) -> Status;
};
```

### Commit Log Compaction

**Problem**: Commit log grows indefinitely (all historical transactions).

**Solution**: Compact commit log based on global OIT.

**Compaction Algorithm**:

```cpp
auto CommitLog::compact(uint64_t global_oit) -> Status {
    // 1. Remove entries older than global OIT
    auto it = entries_.begin();
    while (it != entries_.end() && it->xid < global_oit) {
        it = entries_.erase(it);  // Safe to remove (no transaction can see this)
    }

    // 2. Flush compacted log to disk
    flush();

    return Status::OK();
}
```

**Frequency**: Every 1 hour (configurable: `commit_log_compaction_interval_ms = 3600000`)

---

## Transaction Markers in Distributed Setting

### Transaction Markers

**Firebird MGA Markers** (transaction_manager.h):

```cpp
class TransactionManager {
private:
    uint64_t oldest_xid_;          // OIT: Oldest Interesting Transaction
    uint64_t oldest_active_xid_;   // OAT: Oldest Active Transaction
    uint64_t oldest_snapshot_;     // OST: Oldest Snapshot (N/A in MGA)
};
```

**OIT (Oldest Interesting Transaction)**:
- Oldest transaction that might be visible to any reader
- Back versions older than OIT can be garbage collected

**OAT (Oldest Active Transaction)**:
- Oldest transaction still in ACTIVE state (not committed/aborted)
- Used for detecting long-running transactions

**OST (Oldest Snapshot Transaction)**:
- PostgreSQL concept (oldest snapshot timestamp)
- **NOT USED** in ScratchBird MGA (no snapshots)

### Distributed Markers

**Per-Node Markers** (existing):
- Each node tracks local OIT, OAT, OST

**Global Markers** (new):
- Global OIT: min(all nodes' local OIT)
- Global OAT: min(all nodes' local OAT)

**Gossip Exchange**:

```cpp
struct TransactionMarkerGossip {
    NodeId sender_node_id;
    ShardId shard_id;
    uint64_t local_oit;
    uint64_t local_oat;
    uint64_t timestamp_ms;
};

class DistributedTransactionMarkerManager {
public:
    auto exchangeMarkers(const std::vector<NodeId>& nodes) -> void {
        // Send local markers to all nodes
        for (const auto& node : nodes) {
            sendMarkers(node, {
                .sender_node_id = local_node_id_,
                .shard_id = shard_id_,
                .local_oit = local_oit_,
                .local_oat = local_oat_,
                .timestamp_ms = currentTimeMs()
            });
        }

        // Receive markers from all nodes
        for (const auto& node : nodes) {
            auto markers = receiveMarkers(node);
            updateGlobalMarkers(markers);
        }
    }

    auto updateGlobalMarkers(const TransactionMarkerGossip& markers) -> void {
        global_oit_ = std::min(global_oit_, markers.local_oit);
        global_oat_ = std::min(global_oat_, markers.local_oat);
    }

private:
    uint64_t local_oit_;
    uint64_t local_oat_;
    uint64_t global_oit_;  // Minimum across all nodes
    uint64_t global_oat_;  // Minimum across all nodes
};
```

---

## Integration with Existing Implementation

### transaction_manager.h/cpp

**Existing Code** (transaction_manager.cpp, 2,069 lines):

```cpp
class TransactionManager {
public:
    // Transaction lifecycle
    auto beginTransaction(ErrorContext* ctx) -> uint64_t;
    auto commitTransaction(uint64_t xid, ErrorContext* ctx) -> Status;
    auto abortTransaction(uint64_t xid, ErrorContext* ctx) -> Status;

    // 2PC support
    auto prepareTransaction(uint64_t xid, ErrorContext* ctx) -> Status;
    auto commitPreparedTransaction(uint64_t xid, ErrorContext* ctx) -> Status;
    auto abortPreparedTransaction(uint64_t xid, ErrorContext* ctx) -> Status;

    // Visibility
    auto isVersionVisible(uint64_t version_xid, uint64_t reader_xid) const -> bool;

    // Group commit
    auto performGroupCommit(CommitWaiter* leader, ErrorContext* ctx) -> Status;

    // Transaction markers
    auto getOldestXid() const -> uint64_t;
    auto getOldestActiveXid() const -> uint64_t;
};
```

**Extensions for Beta**:

```cpp
class TransactionManager {
public:
    // Distributed visibility (new)
    auto isVersionVisibleDistributed(
        uint64_t version_xid,
        uint64_t reader_xid,
        const HLC& reader_hlc
    ) const -> bool;

    // Global XID allocation (new)
    auto allocateGlobalXid(uint16_t node_id) -> uint64_t;

    // Distributed TIP replication (new)
    auto replicateTIPEntry(const TIPReplicationMessage& msg) -> Status;

    // Global transaction markers (new)
    auto getGlobalOIT() const -> uint64_t;
    auto getGlobalOAT() const -> uint64_t;
    auto updateGlobalMarkers(const TransactionMarkerGossip& markers) -> void;

private:
    // Distributed state (new)
    uint64_t global_oit_{0};
    uint64_t global_oat_{0};
    std::map<uint64_t, HLC> xid_to_hlc_;  // Map XID to HLC for causal ordering
};
```

### Integration Points

**1. XID Allocation**:

```cpp
// Existing (single-node)
auto TransactionManager::beginTransaction(ErrorContext* ctx) -> uint64_t {
    return next_xid_counter_.fetch_add(1, std::memory_order_relaxed);
}

// New (distributed)
auto TransactionManager::beginTransaction(ErrorContext* ctx) -> uint64_t {
    return allocateGlobalXid(node_id_);
}
```

**2. Visibility Check**:

```cpp
// Existing (single-node)
auto TransactionManager::isVersionVisible(
    uint64_t version_xid,
    uint64_t reader_xid
) const -> bool {
    auto state = getTIPState(version_xid);
    return (state == TransactionState::COMMITTED) && (version_xid < reader_xid);
}

// New (distributed)
auto TransactionManager::isVersionVisibleDistributed(
    uint64_t version_xid,
    uint64_t reader_xid,
    const HLC& reader_hlc
) const -> bool {
    auto state = getTIPState(version_xid);
    if (state != TransactionState::COMMITTED) {
        return false;
    }

    // Check causal order (HLC)
    auto version_hlc = xid_to_hlc_.at(version_xid);
    return version_hlc < reader_hlc;
}
```

**3. Commit Log Integration**:

```cpp
auto TransactionManager::commitTransaction(uint64_t xid, ErrorContext* ctx) -> Status {
    // 1. Existing: Mark TIP as COMMITTED
    tip_manager_->setTransactionState(xid, TransactionState::COMMITTED);

    // 2. New: Stream commit log entry to replicas
    CommitLogEntry entry{
        .xid = xid,
        .shard_id = shard_id_,
        .commit_time_ms = currentTimeMs(),
        .hlc = current_hlc_,
        .state = TransactionState::COMMITTED,
        .changes = extractChanges(xid)
    };
    commit_log_streamer_->streamCommitLogEntry(entry, replicas_);

    return Status::OK();
}
```

---

## Testing and Verification

### Unit Tests

**Global XID Allocation**:

```cpp
TEST(MGAIntegration, GlobalXidAllocation) {
    TransactionManager tm_node1(/*node_id=*/1);
    TransactionManager tm_node2(/*node_id=*/2);

    auto xid1 = tm_node1.allocateGlobalXid(1);
    auto xid2 = tm_node2.allocateGlobalXid(2);

    ASSERT_NE(xid1, xid2);  // No collision
    ASSERT_EQ(extractNodeId(xid1), 1);
    ASSERT_EQ(extractNodeId(xid2), 2);
}
```

**Distributed Visibility**:

```cpp
TEST(MGAIntegration, DistributedVisibility) {
    TransactionManager tm;

    // Transaction T1 (xid=100, hlc=(1000, 5))
    tm.replicateTIPEntry({.xid = 100, .state = COMMITTED, .hlc = {1000, 5}});

    // Transaction T2 (xid=105, hlc=(1000, 10)) reads T1
    HLC reader_hlc{1000, 10};
    ASSERT_TRUE(tm.isVersionVisibleDistributed(100, 105, reader_hlc));

    // Transaction T3 (xid=110, hlc=(1000, 3)) reads T1
    HLC reader_hlc_old{1000, 3};
    ASSERT_FALSE(tm.isVersionVisibleDistributed(100, 110, reader_hlc_old));  // Causally before T1
}
```

### Integration Tests

**Cross-Node Transaction Visibility**:

```cpp
TEST(MGAIntegrationTest, CrossNodeVisibility) {
    Cluster cluster(3);

    // Node A: Write user
    Node("A").execute("BEGIN TRANSACTION");
    auto xid_a = Node("A").execute("INSERT INTO users VALUES ('user_123', 'Alice')");
    Node("A").execute("COMMIT");

    // Replicate to Node B
    cluster.waitForReplication();

    // Node B: Read user (should be visible)
    auto result = Node("B").query("SELECT * FROM users WHERE user_id='user_123'");
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0].name, "Alice");
}
```

**Distributed GC**:

```cpp
TEST(MGAIntegrationTest, DistributedGC) {
    Cluster cluster(3);

    // Node A: Long-running transaction (holds OIT)
    Node("A").execute("BEGIN TRANSACTION");  // xid=100
    auto xid_long = Node("A").getCurrentXid();

    // Node B: Write + commit
    Node("B").execute("INSERT INTO users VALUES ('user_456', 'Bob')");  // xid=200

    // Node C: Update (creates back version)
    Node("C").execute("UPDATE users SET name='Bob Updated' WHERE user_id='user_456'");  // xid=300

    // GC attempt: Should NOT sweep back version (Node A's transaction still active)
    cluster.performDistributedGC();
    ASSERT_TRUE(Node("C").hasBackVersion("users", "user_456"));

    // Node A: Commit long-running transaction
    Node("A").execute("COMMIT");

    // GC attempt: Should sweep back version now
    cluster.performDistributedGC();
    ASSERT_FALSE(Node("C").hasBackVersion("users", "user_456"));
}
```

### Performance Tests

**Distributed TIP Replication Latency**:

```bash
# Benchmark: Replicate 10K TIP entries across 3 nodes
./scratchbird_benchmark --mode=tip_replication --entries=10000 --nodes=3
# Expected: < 5ms per entry (includes network RTT)
```

**Global OIT Computation**:

```bash
# Benchmark: Compute global OIT across 100 nodes
./scratchbird_benchmark --mode=global_oit --nodes=100
# Expected: < 100ms (gossip aggregation)
```

---

## Appendix: MGA_RULES.md Compliance

### Critical MGA Rules

**From /MGA_RULES.md**:

1. **No Snapshots**: ✅ TIP-based visibility, no snapshot timestamps
2. **In-Place Updates**: ✅ Back versions chained, not append-only
3. **TIP Authority**: ✅ TIP is authoritative for COMMITTED/ABORTED state
4. **No CLOG**: ✅ ScratchBird uses TIP, not PostgreSQL CLOG
5. **Firebird-Only**: ✅ No MVCC assumptions, pure MGA

### Distributed Extensions Preserve MGA

**Distributed Visibility**:
- Uses TIP state (COMMITTED/ABORTED) ← MGA principle preserved
- Adds HLC for causal ordering ← Extension, not replacement

**Distributed GC**:
- Uses global OIT (minimum across nodes) ← MGA principle preserved
- Sweeps back versions based on OIT ← Firebird sweep model preserved

**Commit Log**:
- Replication log (after commit) ← NOT write-after log (WAL) (PostgreSQL)
- TIP handles crash recovery ← MGA principle preserved

---

**Document Status:** DRAFT (Beta Specification Phase)
**Last Updated:** 2026-01-09
**Next Review:** Weekly during implementation
**Dependencies:** transaction_manager.h/cpp, MGA_RULES.md, 01_UUIDV8_HLC_ARCHITECTURE.md

---

**End of Document 05**
