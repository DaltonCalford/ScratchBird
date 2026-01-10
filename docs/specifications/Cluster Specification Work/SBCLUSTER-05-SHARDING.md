# SBCLUSTER-05: Sharding

## 1. Introduction

### 1.1 Purpose
This document specifies the sharding architecture for ScratchBird clusters. Sharding partitions data across multiple nodes using **consistent hashing**, enabling horizontal scalability while maintaining the shared-nothing, shard-local MVCC model that is fundamental to ScratchBird's design.

### 1.2 Scope
- Consistent hash sharding strategy
- Shard map structure and versioning
- Virtual nodes (vnodes) for balanced distribution
- Shard assignment and routing algorithms
- Shard ownership and node assignment
- Future resharding and rebalancing capabilities
- Cross-shard query limitations

### 1.3 Related Documents
- **SBCLUSTER-00**: Guiding Principles (shard-local MVCC, no cross-shard transactions)
- **SBCLUSTER-01**: Cluster Configuration Epoch (CCE references shard_map_hash)
- **SBCLUSTER-02**: Membership and Identity (SHARD_OWNER role)
- **SBCLUSTER-06**: Distributed Query (coordinator/subplan model)
- **SBCLUSTER-07**: Replication (per-shard replication)

### 1.4 Terminology
- **Shard**: An independent data partition with its own MVCC engine and storage
- **Shard Key**: The column(s) used to determine shard assignment for a row
- **Shard Map**: Immutable mapping from hash space to shard UUIDs and owner nodes
- **Virtual Node (vnode)**: Logical partitions of the hash ring for better balance
- **Consistent Hashing**: Partitioning strategy that minimizes data movement during topology changes
- **Resharding**: Process of changing the number of shards or redistributing data
- **Shared-Nothing**: Each shard operates independently with no shared storage

---

## 2. Architecture Overview

### 2.1 Design Principles

1. **Shared-Nothing**: Each shard has its own storage, MVCC engine, and transaction log
2. **Shard-Local MVCC**: Firebird MGA MVCC operates within a single shard; no cross-shard transactions
3. **Consistent Hashing**: Use consistent hash ring to minimize data movement during resharding
4. **Virtual Nodes**: Each physical node owns multiple vnodes for better load distribution
5. **Versioned Shard Maps**: Shard maps are immutable, versioned, and distributed via CCE
6. **Deterministic Routing**: Same shard key always routes to the same shard (given a shard map version)

### 2.2 Hash Ring Architecture

```
                         Hash Ring (2^64 values, wrapped)

     0x0000...000                                    0xFFFF...FFF
        │                                                  │
        ▼                                                  ▼
    ┌───────────────────────────────────────────────────────┐
    │                                                       │
    │   vnode_001  vnode_002  vnode_003  ...  vnode_256    │◄── Virtual nodes
    │    (shard1)   (shard2)   (shard1)       (shard3)     │
    │                                                       │
    └───────────────────────────────────────────────────────┘
           ▲                             ▲
           │                             │
      Node A                        Node B
      owns shards 1, 3           owns shard 2
```

**Key Concepts**:
- Hash space: 2^64 (full range of uint64_t)
- Each shard owns multiple vnodes distributed around the ring
- A row's shard key is hashed; the result determines which vnode (and thus which shard) owns it
- Vnodes provide fine-grained distribution and easier rebalancing

### 2.3 Shard Ownership Model

```
┌─────────────────────────────────────────────────────┐
│                 SHARD MAP VERSION 5                 │
│  ┌───────────────────────────────────────────────┐  │
│  │  Shard 1 (UUID: 01234...)                     │  │
│  │    Primary Owner:  Node A                     │  │
│  │    Replicas:       Node B, Node C             │  │
│  │    Vnodes:         [0, 64, 128, 192]          │  │
│  └───────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────┐  │
│  │  Shard 2 (UUID: 12345...)                     │  │
│  │    Primary Owner:  Node B                     │  │
│  │    Replicas:       Node A, Node D             │  │
│  │    Vnodes:         [32, 96, 160, 224]         │  │
│  └───────────────────────────────────────────────┘  │
│  │  ... (more shards)                            │  │
└─────────────────────────────────────────────────────┘
```

---

## 3. Data Structures

### 3.1 ShardMap

```cpp
struct ShardMap {
    uint64_t version;                    // Monotonic shard map version
    UUID cluster_uuid;                   // Must match cluster identity
    timestamp_t created_at;              // Map creation timestamp

    uint32_t vnode_count;                // Total number of virtual nodes (e.g., 256)
    uint32_t shard_count;                // Number of shards

    Shard[] shards;                      // Ordered by shard_uuid
    VirtualNode[] vnodes;                // Ordered by hash_range_start

    // Metadata
    UUID created_by_user_uuid;           // Administrator who created map
    string description;                  // Human-readable description

    // Signature (Ed25519)
    Signature map_signature;             // Signed by cluster administrators
};
```

### 3.2 Shard

```cpp
struct Shard {
    UUID shard_uuid;                     // Unique shard identifier (v7)
    string shard_name;                   // Human-readable name (e.g., "shard_001")

    UUID primary_owner_node_uuid;        // Node that owns the primary copy
    UUID[] replica_node_uuids;           // Nodes that own replicas (ordered)

    uint32_t[] vnode_indices;            // Indices of vnodes owned by this shard

    ShardState state;                    // ACTIVE | DRAINING | MIGRATING | DISABLED
    timestamp_t created_at;
};

enum ShardState {
    ACTIVE,      // Shard is active and accepting reads/writes
    DRAINING,    // Shard is being drained (no new writes, reads allowed)
    MIGRATING,   // Shard is being migrated to a new owner
    DISABLED     // Shard is disabled (no reads or writes)
};
```

### 3.3 VirtualNode

```cpp
struct VirtualNode {
    uint32_t vnode_index;                // Index in the vnode array (0..vnode_count-1)
    uint64_t hash_range_start;           // Start of hash range (inclusive)
    uint64_t hash_range_end;             // End of hash range (exclusive)

    UUID shard_uuid;                     // Shard that owns this vnode

    // For debugging/monitoring
    string token_description;            // Human-readable (e.g., "vnode_042")
};
```

**Hash Range Calculation**:
```cpp
uint64_t hash_ring_size = UINT64_MAX;   // 2^64
uint64_t vnode_size = hash_ring_size / vnode_count;

for (uint32_t i = 0; i < vnode_count; ++i) {
    vnodes[i].hash_range_start = i * vnode_size;
    vnodes[i].hash_range_end = (i + 1) * vnode_size;
    vnodes[i].vnode_index = i;
}
```

### 3.4 ShardKey

```cpp
struct ShardKey {
    UUID table_uuid;                     // Table this shard key applies to
    UUID[] column_uuids;                 // Columns that form the shard key (ordered)

    ShardKeyType type;                   // HASH | RANGE (Alpha: HASH only)
};

enum ShardKeyType {
    HASH,        // Consistent hash on shard key columns
    RANGE        // Range partitioning (future, not in Beta)
};
```

---

## 4. Shard Assignment Algorithm

### 4.1 Hash Function

ScratchBird uses **xxHash64** for shard key hashing:

```cpp
uint64_t compute_shard_key_hash(const ShardKey& shard_key, const Row& row) {
    XXH64_state_t state;
    XXH64_reset(&state, 0);  // Seed = 0 for determinism

    // Hash each column in the shard key (in order)
    for (const UUID& col_uuid : shard_key.column_uuids) {
        Value value = row.get_column(col_uuid);

        // Serialize value canonically
        ByteBuffer value_bytes = serialize_value_canonical(value);
        XXH64_update(&state, value_bytes.data(), value_bytes.size());
    }

    return XXH64_digest(&state);
}
```

**Why xxHash64?**
- Fast (8+ GB/s on modern CPUs)
- Good distribution properties
- Widely used (RocksDB, ClickHouse, etc.)
- Stable across platforms

### 4.2 Vnode Lookup

Given a hash value, find the owning vnode:

```cpp
const VirtualNode& find_vnode(const ShardMap& shard_map, uint64_t hash_value) {
    // Binary search on vnodes (sorted by hash_range_start)
    uint32_t left = 0;
    uint32_t right = shard_map.vnode_count - 1;

    while (left < right) {
        uint32_t mid = left + (right - left) / 2;

        if (hash_value < shard_map.vnodes[mid].hash_range_start) {
            right = mid - 1;
        } else if (hash_value >= shard_map.vnodes[mid].hash_range_end) {
            left = mid + 1;
        } else {
            return shard_map.vnodes[mid];  // Found
        }
    }

    return shard_map.vnodes[left];
}
```

### 4.3 Shard Routing

Complete routing from row to shard owner:

```cpp
UUID route_to_shard_owner(
    const ShardMap& shard_map,
    const ShardKey& shard_key,
    const Row& row
) {
    // 1. Compute shard key hash
    uint64_t hash_value = compute_shard_key_hash(shard_key, row);

    // 2. Find vnode
    const VirtualNode& vnode = find_vnode(shard_map, hash_value);

    // 3. Find shard
    const Shard* shard = find_shard(shard_map, vnode.shard_uuid);
    if (!shard) {
        throw ShardNotFoundException(vnode.shard_uuid);
    }

    // 4. Return primary owner node
    return shard->primary_owner_node_uuid;
}
```

---

## 5. Shard Map Distribution

### 5.1 Shard Map Creation

**Administrative Command**:
```sql
-- Create a new shard map with 16 shards, 256 vnodes
CREATE SHARD MAP VERSION 2
  WITH SHARD_COUNT = 16,
       VNODE_COUNT = 256,
       DESCRIPTION = 'Initial 16-shard distribution';

-- Assign shards to nodes
ASSIGN SHARD shard_001 TO NODE node_a PRIMARY, node_b REPLICA, node_c REPLICA;
ASSIGN SHARD shard_002 TO NODE node_b PRIMARY, node_c REPLICA, node_d REPLICA;
-- ... etc

-- Publish shard map to cluster
PUBLISH SHARD MAP VERSION 2;
```

### 5.2 Shard Map Distribution via CCE

Shard maps are distributed via Cluster Configuration Epoch (SBCLUSTER-01):

```cpp
struct ClusterConfigEpoch {
    // ... other fields
    bytes32 shard_map_hash;              // SHA-256 hash of shard map
};
```

**Distribution Flow**:
1. Administrator creates and publishes shard map
2. Shard map is serialized canonically and hashed (SHA-256)
3. Hash is included in next CCE
4. CCE is distributed via Raft
5. All nodes retrieve shard map by hash and apply it

### 5.3 Canonical Serialization

```cpp
ByteBuffer serialize_canonical(const ShardMap& shard_map) {
    ByteBuffer buf;

    // Header
    buf.append_u64_be(shard_map.version);
    buf.append_bytes(shard_map.cluster_uuid.bytes(), 16);
    buf.append_u64_be(shard_map.created_at.micros_since_epoch());
    buf.append_u32_be(shard_map.vnode_count);
    buf.append_u32_be(shard_map.shard_count);

    // Shards (sorted by shard_uuid)
    auto sorted_shards = shard_map.shards;
    std::sort(sorted_shards.begin(), sorted_shards.end(),
        [](const Shard& a, const Shard& b) { return a.shard_uuid < b.shard_uuid; });

    for (const Shard& shard : sorted_shards) {
        buf.append_bytes(serialize_shard(shard));
    }

    // Vnodes (sorted by vnode_index)
    for (const VirtualNode& vnode : shard_map.vnodes) {
        buf.append_bytes(serialize_vnode(vnode));
    }

    // Metadata
    buf.append_bytes(shard_map.created_by_user_uuid.bytes(), 16);
    buf.append_u32_be(shard_map.description.length());
    buf.append_bytes(shard_map.description.data(), shard_map.description.length());

    return buf;
}

bytes32 compute_shard_map_hash(const ShardMap& shard_map) {
    ByteBuffer canonical = serialize_canonical(shard_map);
    return SHA256(canonical.data(), canonical.size());
}
```

---

## 6. Shard-Local MVCC

### 6.1 No Cross-Shard Transactions

**CRITICAL RULE**: ScratchBird does NOT support transactions that span multiple shards.

```sql
-- VALID: Single-shard transaction
BEGIN TRANSACTION;
  UPDATE orders SET status = 'SHIPPED' WHERE order_id = 12345;
  INSERT INTO audit_log (action, order_id) VALUES ('SHIPPED', 12345);
COMMIT;

-- INVALID: Cross-shard transaction (if orders and customers are sharded differently)
BEGIN TRANSACTION;
  UPDATE orders SET status = 'SHIPPED' WHERE order_id = 12345;     -- Shard A
  UPDATE customers SET last_order = NOW() WHERE customer_id = 789; -- Shard B
COMMIT;  -- ERROR: Cross-shard transactions not supported
```

**Rationale**:
- Firebird MGA MVCC is shard-local
- No distributed 2PC (Two-Phase Commit)
- Simpler implementation, better performance
- Forces application-level design for distributed data

### 6.2 Shard-Local Isolation

Each shard maintains its own:
- **Transaction ID Space**: OAT, OIT, OST (Oldest Active, In-Flight, Snapshot Transactions)
- **MVCC Versions**: Record versions are shard-local
- **Write-after log (WAL)**: Each shard has its own log stream
- **Snapshot Isolation**: Snapshots are shard-local

### 6.3 Distributed Queries (Read-Only)

While transactions cannot span shards, **read-only queries** can:

```sql
-- Distributed aggregation query (read-only)
SELECT customer_region, COUNT(*), SUM(order_total)
FROM orders
GROUP BY customer_region;
```

**How it works**:
1. Coordinator node receives query
2. Coordinator generates subplans for each shard
3. Each shard executes its subplan independently (shard-local snapshot)
4. Coordinator merges results

**Consistency**: Each shard uses its own snapshot; results may not be globally consistent (eventual consistency).

---

## 7. Resharding (Future)

### 7.1 Resharding Strategies

**Beta: Manual Resharding Only**
- Administrator creates new shard map with desired topology
- Data migration happens offline or via application logic
- New shard map published via CCE

**Post-Beta: Online Resharding**
- Live data migration between shards
- Dual-write during migration
- Automatic convergence and cutover

### 7.2 Resharding Procedure (Manual)

```sql
-- Step 1: Create new shard map with more shards
CREATE SHARD MAP VERSION 3
  WITH SHARD_COUNT = 32,  -- Double the shards
       VNODE_COUNT = 256,
       DESCRIPTION = 'Resharding from 16 to 32 shards';

-- Step 2: Assign new shards to nodes
ASSIGN SHARD shard_017 TO NODE node_e PRIMARY, node_f REPLICA;
-- ... etc

-- Step 3: Mark old shard map as DRAINING
SET SHARD MAP VERSION 2 STATE = DRAINING;

-- Step 4: Application performs data migration
-- (Details depend on application-specific logic)

-- Step 5: Publish new shard map
PUBLISH SHARD MAP VERSION 3;

-- Step 6: Retire old shard map
RETIRE SHARD MAP VERSION 2;
```

### 7.3 Virtual Node Rebalancing

When resharding, vnodes can be reassigned to different shards:

```
Old Shard Map (4 shards, 8 vnodes):
  Shard 1: vnodes [0, 4]
  Shard 2: vnodes [1, 5]
  Shard 3: vnodes [2, 6]
  Shard 4: vnodes [3, 7]

New Shard Map (8 shards, 8 vnodes):
  Shard 1: vnodes [0]
  Shard 2: vnodes [1]
  Shard 3: vnodes [2]
  Shard 4: vnodes [3]
  Shard 5: vnodes [4]  ← Moved from Shard 1
  Shard 6: vnodes [5]  ← Moved from Shard 2
  Shard 7: vnodes [6]  ← Moved from Shard 3
  Shard 8: vnodes [7]  ← Moved from Shard 4
```

**Data Movement**: Only rows in moved vnodes need migration (50% in this example).

---

## 8. Operational Procedures

### 8.1 Initial Shard Map Creation

When bootstrapping a cluster, create the initial shard map:

```cpp
ShardMap create_initial_shard_map(
    const UUID& cluster_uuid,
    uint32_t shard_count,
    const vector<UUID>& node_uuids
) {
    ShardMap shard_map;
    shard_map.version = 1;
    shard_map.cluster_uuid = cluster_uuid;
    shard_map.created_at = now();
    shard_map.vnode_count = 256;  // Default: 256 vnodes
    shard_map.shard_count = shard_count;

    // Create shards
    for (uint32_t i = 0; i < shard_count; ++i) {
        Shard shard;
        shard.shard_uuid = UUID::v7();
        shard.shard_name = fmt::format("shard_{:03d}", i + 1);

        // Round-robin assignment to nodes
        shard.primary_owner_node_uuid = node_uuids[i % node_uuids.size()];

        // Assign replicas (next 2 nodes in round-robin)
        for (uint32_t r = 1; r <= 2; ++r) {
            UUID replica_node = node_uuids[(i + r) % node_uuids.size()];
            if (replica_node != shard.primary_owner_node_uuid) {
                shard.replica_node_uuids.push_back(replica_node);
            }
        }

        shard.state = ShardState::ACTIVE;
        shard.created_at = now();

        shard_map.shards.push_back(shard);
    }

    // Create vnodes and assign to shards
    uint32_t vnodes_per_shard = shard_map.vnode_count / shard_count;
    for (uint32_t i = 0; i < shard_map.vnode_count; ++i) {
        VirtualNode vnode;
        vnode.vnode_index = i;
        vnode.hash_range_start = (UINT64_MAX / shard_map.vnode_count) * i;
        vnode.hash_range_end = (UINT64_MAX / shard_map.vnode_count) * (i + 1);

        // Assign vnode to shard (round-robin)
        uint32_t shard_index = i % shard_count;
        vnode.shard_uuid = shard_map.shards[shard_index].shard_uuid;

        shard_map.vnodes.push_back(vnode);

        // Update shard's vnode list
        shard_map.shards[shard_index].vnode_indices.push_back(i);
    }

    shard_map.created_by_user_uuid = SYSTEM_UUID;
    shard_map.description = "Initial shard map";

    return shard_map;
}
```

### 8.2 Shard Health Monitoring

**Metrics to Monitor**:
- **Shard Size**: Disk usage per shard
- **Shard Load**: QPS (queries per second) per shard
- **Shard Balance**: Variance in size/load across shards
- **Hot Shards**: Shards with disproportionately high load

**Alerting**:
- Alert if shard size variance > 30%
- Alert if any shard has > 2x average QPS
- Alert if shard state is DRAINING or MIGRATING for > 1 hour

### 8.3 Shard Rebalancing Triggers

**When to Rebalance**:
- Shard size variance exceeds threshold (e.g., 50%)
- Hot shards detected (load imbalance)
- Cluster topology changes (nodes added/removed)
- Application growth (data volume increases)

**Rebalancing Process**:
1. Analyze shard distribution and load
2. Propose new shard map with better balance
3. Estimate data migration cost
4. Get administrator approval
5. Execute resharding procedure

---

## 9. Security Considerations

### 9.1 Shard Map Integrity

- Shard maps are **cryptographically signed** (Ed25519)
- Only cluster administrators can create/publish shard maps
- Shard map hash is bound to CCE (tampering detected)

### 9.2 Shard Access Control

- Shard ownership is verified via node certificates (SBCLUSTER-02)
- Only the primary owner or replica nodes can access a shard's storage
- Unauthorized shard access is logged and blocked

### 9.3 Shard Isolation

- Shards are storage-isolated (separate data files)
- Shards are process-isolated (separate MVCC engines)
- Crash in one shard does not affect other shards

---

## 10. Testing Procedures

### 10.1 Functional Tests

**Test: Shard Key Hashing Determinism**
```cpp
TEST(Sharding, HashingDeterminism) {
    ShardKey shard_key = create_test_shard_key();
    Row row = create_test_row();

    uint64_t hash1 = compute_shard_key_hash(shard_key, row);
    uint64_t hash2 = compute_shard_key_hash(shard_key, row);

    EXPECT_EQ(hash1, hash2);  // Same row = same hash
}
```

**Test: Vnode Lookup**
```cpp
TEST(Sharding, VnodeLookup) {
    ShardMap shard_map = create_test_shard_map(16, 256);

    // Test various hash values
    EXPECT_EQ(find_vnode(shard_map, 0x0000000000000000).vnode_index, 0);
    EXPECT_EQ(find_vnode(shard_map, 0x0100000000000000).vnode_index, 1);
    EXPECT_EQ(find_vnode(shard_map, UINT64_MAX).vnode_index, 255);
}
```

**Test: Shard Routing**
```cpp
TEST(Sharding, Routing) {
    ShardMap shard_map = create_test_shard_map(4, 256);
    ShardKey shard_key = create_shard_key_on_column("customer_id");

    Row row1 = create_row_with_customer_id(12345);
    Row row2 = create_row_with_customer_id(12345);  // Same customer
    Row row3 = create_row_with_customer_id(67890);  // Different customer

    UUID shard1 = route_to_shard_owner(shard_map, shard_key, row1);
    UUID shard2 = route_to_shard_owner(shard_map, shard_key, row2);
    UUID shard3 = route_to_shard_owner(shard_map, shard_key, row3);

    EXPECT_EQ(shard1, shard2);  // Same customer = same shard
    // shard3 may or may not equal shard1 (depends on hash collision)
}
```

### 10.2 Distribution Tests

**Test: Uniform Distribution**
```cpp
TEST(Sharding, UniformDistribution) {
    ShardMap shard_map = create_test_shard_map(16, 256);
    ShardKey shard_key = create_shard_key_on_column("order_id");

    // Hash 1 million random order IDs
    map<UUID, uint32_t> shard_counts;
    for (uint32_t i = 0; i < 1000000; ++i) {
        Row row = create_row_with_order_id(random_uuid());
        UUID shard_uuid = route_to_shard_owner(shard_map, shard_key, row);
        shard_counts[shard_uuid]++;
    }

    // Verify distribution is roughly uniform (within 10% of expected)
    uint32_t expected_per_shard = 1000000 / 16;  // 62,500
    for (const auto& [shard_uuid, count] : shard_counts) {
        double deviation = abs((int)count - (int)expected_per_shard) / (double)expected_per_shard;
        EXPECT_LT(deviation, 0.10);  // Less than 10% deviation
    }
}
```

### 10.3 Resharding Tests

**Test: Vnode Migration**
```cpp
TEST(Sharding, VnodeMigration) {
    ShardMap old_map = create_test_shard_map(4, 256);
    ShardMap new_map = create_test_shard_map(8, 256);  // Double shards

    // Compute which rows need migration
    ShardKey shard_key = create_shard_key_on_column("user_id");
    vector<Row> test_rows = create_test_rows(10000);

    uint32_t migration_count = 0;
    for (const Row& row : test_rows) {
        UUID old_shard = route_to_shard_owner(old_map, shard_key, row);
        UUID new_shard = route_to_shard_owner(new_map, shard_key, row);

        if (old_shard != new_shard) {
            migration_count++;
        }
    }

    // When doubling shards, expect ~50% migration
    double migration_ratio = migration_count / (double)test_rows.size();
    EXPECT_NEAR(migration_ratio, 0.50, 0.05);  // 50% ± 5%
}
```

---

## 11. Examples

### 11.1 Creating a Sharded Table

```sql
-- Create a sharded table with customer_id as shard key
CREATE TABLE orders (
    order_id UUID PRIMARY KEY,
    customer_id UUID NOT NULL,
    order_date TIMESTAMP NOT NULL,
    total_amount DECIMAL(10, 2) NOT NULL,
    status VARCHAR(20) NOT NULL
)
SHARD BY HASH(customer_id);
```

**Behavior**:
- All orders for a given `customer_id` will be on the same shard
- Queries filtering by `customer_id` can be routed to a single shard
- Queries not filtering by `customer_id` must query all shards (distributed query)

### 11.2 Single-Shard Query

```sql
-- This query can be routed to a single shard
SELECT order_id, order_date, total_amount
FROM orders
WHERE customer_id = '01234567-89ab-cdef-0123-456789abcdef'
ORDER BY order_date DESC
LIMIT 10;
```

**Execution**:
1. Coordinator hashes `customer_id` value
2. Finds vnode and shard owner via shard map
3. Routes query to that shard's primary owner node
4. Shard executes query locally
5. Results returned to client

### 11.3 Distributed Query (All Shards)

```sql
-- This query requires all shards (no shard key filter)
SELECT status, COUNT(*), SUM(total_amount)
FROM orders
WHERE order_date >= '2026-01-01'
GROUP BY status;
```

**Execution**:
1. Coordinator generates subplan for each shard
2. Each shard executes:
   ```sql
   SELECT status, COUNT(*), SUM(total_amount)
   FROM orders_local
   WHERE order_date >= '2026-01-01'
   GROUP BY status;
   ```
3. Coordinator merges results (re-aggregates)
4. Final result returned to client

### 11.4 Cross-Shard Join (Not Supported)

```sql
-- INVALID: Cross-shard join (if orders and customers have different shard keys)
SELECT o.order_id, c.customer_name, o.total_amount
FROM orders o
JOIN customers c ON o.customer_id = c.customer_id
WHERE o.order_date >= '2026-01-01';
```

**Error**: `Cross-shard joins are not supported. Consider co-locating orders and customers by sharding both on customer_id.`

**Solution**: Shard both tables by `customer_id` so joins are shard-local.

---

## 12. Performance Characteristics

### 12.1 Lookup Performance

- **Shard Key Hash**: O(1) - xxHash64 is very fast
- **Vnode Lookup**: O(log V) - binary search on 256 vnodes ≈ 8 comparisons
- **Shard Metadata**: O(1) - hash table lookup
- **Total Routing Time**: < 10 microseconds on modern hardware

### 12.2 Shard Count Recommendations

| Cluster Size | Recommended Shards | Vnodes | Notes |
|--------------|-------------------|--------|-------|
| 3 nodes      | 6-12 shards       | 256    | 2-4 shards per node |
| 10 nodes     | 30-50 shards      | 256    | 3-5 shards per node |
| 100 nodes    | 300-500 shards    | 512    | 3-5 shards per node |

**Rationale**:
- More shards = finer-grained rebalancing
- Fewer shards = less overhead
- Target: 3-5 shards per node for good balance

### 12.3 Vnode Count Recommendations

- **Small clusters (< 10 nodes)**: 256 vnodes
- **Medium clusters (10-100 nodes)**: 512 vnodes
- **Large clusters (> 100 nodes)**: 1024 vnodes

**Tradeoffs**:
- More vnodes = better distribution, slower lookup
- Fewer vnodes = faster lookup, worse distribution

---

## 13. References

### 13.1 Internal References
- **SBCLUSTER-00**: Guiding Principles
- **SBCLUSTER-01**: Cluster Configuration Epoch
- **SBCLUSTER-02**: Membership and Identity
- **SBCLUSTER-06**: Distributed Query
- **SBCLUSTER-07**: Replication

### 13.2 External References
- **Consistent Hashing**: Karger et al., "Consistent Hashing and Random Trees", STOC 1997
- **DynamoDB Partitioning**: Amazon DynamoDB paper (SIGMOD 2022)
- **Cassandra Architecture**: "Cassandra: A Decentralized Structured Storage System", LADIS 2009
- **YugabyteDB Sharding**: https://www.yugabyte.com/blog/four-data-sharding-strategies-we-analyzed-in-building-a-distributed-sql-database/
- **xxHash**: https://github.com/Cyan4973/xxHash

### 13.3 Research Papers
- *"Consistent Hashing and Random Trees: Distributed Caching Protocols for Relieving Hot Spots on the World Wide Web"* - Karger et al., STOC 1997
- *"Chord: A Scalable Peer-to-peer Lookup Service for Internet Applications"* - Stoica et al., SIGCOMM 2001

---

## 14. Revision History

| Version | Date       | Author       | Changes                                    |
|---------|------------|--------------|--------------------------------------------|
| 1.0     | 2026-01-02 | D. Calford   | Initial comprehensive specification        |

---

**Document Status**: DRAFT (Beta Specification Phase)
**Next Review**: Before Beta Implementation Phase
**Approval Required**: Chief Architect, Distributed Systems Lead, Storage Engineering Lead
