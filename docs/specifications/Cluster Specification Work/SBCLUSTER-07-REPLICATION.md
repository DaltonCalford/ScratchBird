# SBCLUSTER-07: Replication

## 1. Introduction

### 1.1 Purpose
This document specifies the replication architecture for ScratchBird clusters. Replication provides **high availability** and **data durability** through per-shard replicas using two modes: **logical commit log replication** for flexibility and **physical shadow replication** for performance.

### 1.2 Scope
- Per-shard replication model
- Logical commit log replication (write-after log (WAL) streaming)
- Physical shadow replication (block-level mirroring)
- Primary-replica architecture
- Failover and promotion procedures
- Fencing mechanisms to prevent split-brain
- Replication lag monitoring and alerting
- Consistency guarantees and trade-offs

**Scope Note:** Write-after log (WAL) references describe an optional write-after log stream for replication/PITR; MGA does not use write-after log (WAL) for recovery.

### 1.3 Related Documents
- **SBCLUSTER-00**: Guiding Principles (shard-local MVCC, shared-nothing)
- **SBCLUSTER-02**: Membership and Identity (node roles, SHARD_OWNER)
- **SBCLUSTER-05**: Sharding (shard ownership, shard states)
- **SBCLUSTER-08**: Backup and Restore (coordinated backups from replicas)

### 1.4 Terminology
- **Primary**: Node that accepts writes for a shard
- **Replica**: Node that receives replicated data from the primary
- **Write-after log (WAL)**: Transaction log stream for replication/PITR
- **Logical Replication**: Replaying logical operations (INSERT, UPDATE, DELETE)
- **Physical Replication**: Copying physical data blocks
- **Replication Lag**: Time delay between primary write and replica acknowledgment
- **Fencing**: Preventing a demoted primary from accepting writes (split-brain prevention)

---

## 2. Architecture Overview

### 2.1 Design Principles

1. **Per-Shard Replication**: Each shard is replicated independently (shared-nothing)
2. **Primary-Replica Model**: One primary, N replicas per shard (N typically 1-2)
3. **Asynchronous by Default**: Replicas acknowledge asynchronously (low write latency)
4. **Synchronous Mode Optional**: Quorum-based synchronous replication for critical data
5. **Dual Replication Modes**: Logical (flexible) and Physical (fast)
6. **Raft-Coordinated Failover**: Cluster uses Raft to coordinate primary promotion
7. **Fencing via Generation Numbers**: Prevent split-brain with monotonic generation IDs

### 2.2 Replication Topology

```
┌─────────────────────────────────────────────────────────────┐
│                      SHARD 1                                │
│                                                             │
│  ┌──────────────┐  Write-after Log (WAL) Stream  ┌──────────────┐   │
│  │  PRIMARY     │ ──────────────────────>│  REPLICA 1   │   │
│  │  (Node A)    │                        │  (Node B)    │   │
│  │              │  Write-after Log (WAL) Stream │              │   │
│  │              │ ──────────────────────>│              │   │
│  └──────────────┘           │            └──────────────┘   │
│                              │                               │
│                              │            ┌──────────────┐   │
│                              └───────────>│  REPLICA 2   │   │
│                                           │  (Node C)    │   │
│                                           └──────────────┘   │
└─────────────────────────────────────────────────────────────┘

Write Path:
  1. Client → Primary (write accepted)
  2. Primary → write-after log (WAL) (durable on disk)
  3. Primary → Replicas (asynchronous streaming)
  4. Primary → Client (ACK returned, may not wait for replicas)

Read Path:
  - Reads can go to Primary or Replicas (eventual consistency)
  - Strongly consistent reads go to Primary only
```

### 2.3 Replication Modes

**Logical Replication (Default)**:
- Stream logical operations from write-after log (WAL)
- Replicas replay INSERT/UPDATE/DELETE operations
- Allows heterogeneous hardware (different storage layouts)
- Enables partial replication (filter by table/schema)

**Physical Replication (High-Performance Option)**:
- Stream physical data blocks
- Byte-for-byte identical replicas
- Faster replication, lower CPU overhead
- Requires identical storage configuration

---

## 3. Data Structures

### 3.1 ReplicationConfiguration

```cpp
struct ReplicationConfiguration {
    UUID shard_uuid;                     // Shard being replicated
    UUID primary_node_uuid;              // Current primary node
    UUID[] replica_node_uuids;           // Replica nodes (ordered by priority)

    ReplicationMode mode;                // LOGICAL | PHYSICAL
    ReplicationSyncMode sync_mode;       // ASYNC | SYNC | QUORUM

    uint32_t quorum_replicas;            // For QUORUM mode (e.g., 1 of 2)
    timeout_t sync_timeout;              // Max wait for sync replica ACK (e.g., 10s)

    uint64_t generation_number;          // Monotonic generation (for fencing)
    timestamp_t primary_promoted_at;     // When current primary was promoted
};

enum ReplicationMode {
    LOGICAL,     // Logical commit log replication
    PHYSICAL     // Physical block-level replication
};

enum ReplicationSyncMode {
    ASYNC,       // Don't wait for replica ACKs (default)
    SYNC,        // Wait for ALL replicas to ACK
    QUORUM       // Wait for QUORUM replicas to ACK (e.g., 1 of 2)
};
```

### 3.2 WALRecord

```cpp
struct WALRecord {
    uint64_t lsn;                        // Log Sequence Number (monotonic)
    UUID shard_uuid;                     // Shard this record belongs to
    uint64_t generation_number;          // Primary's generation number

    WALRecordType type;                  // COMMIT | INSERT | UPDATE | DELETE | CHECKPOINT
    uint64_t transaction_id;             // Transaction ID

    // For COMMIT records
    timestamp_t commit_timestamp;

    // For DML records (INSERT/UPDATE/DELETE)
    UUID table_uuid;
    bytes primary_key;                   // Primary key of affected row
    bytes row_data;                      // Row data (for INSERT/UPDATE)

    // For CHECKPOINT records
    uint64_t checkpoint_lsn;             // LSN of checkpoint

    bytes32 record_hash;                 // SHA-256 hash for integrity
};

enum WALRecordType {
    BEGIN,
    COMMIT,
    ROLLBACK,
    INSERT,
    UPDATE,
    DELETE,
    CHECKPOINT
};
```

### 3.3 ReplicationState

```cpp
struct ReplicationState {
    UUID shard_uuid;
    UUID replica_node_uuid;

    ReplicaRole role;                    // PRIMARY | REPLICA | DEMOTED

    uint64_t last_received_lsn;          // Last LSN received from primary
    uint64_t last_applied_lsn;           // Last LSN applied to storage
    uint64_t last_flushed_lsn;           // Last LSN flushed to disk

    timestamp_t last_contact_at;         // Last contact with primary
    duration_t replication_lag;          // Time lag behind primary

    ReplicaStatus status;                // STREAMING | CATCHING_UP | DISCONNECTED | FAILED
};

enum ReplicaRole {
    PRIMARY,     // Accepting writes
    REPLICA,     // Receiving replication stream
    DEMOTED      // Former primary that has been fenced
};

enum ReplicaStatus {
    STREAMING,       // Actively streaming and applying write-after log (WAL)
    CATCHING_UP,     // Streaming but significantly behind
    DISCONNECTED,    // Temporarily disconnected from primary
    FAILED           // Permanently failed
};
```

---

## 4. Logical Replication

### 4.1 Write-after Log (WAL) Streaming

**Primary Side**:
```cpp
void stream_wal_to_replicas(const WALRecord& record) {
    ReplicationConfiguration config = get_replication_config(record.shard_uuid);

    vector<future<ReplicaAck>> acks;

    // Stream to all replicas
    for (const UUID& replica_uuid : config.replica_node_uuids) {
        future<ReplicaAck> ack = rpc_client.send_wal_record_async(
            replica_uuid, record);
        acks.push_back(std::move(ack));
    }

    // Handle sync mode
    if (config.sync_mode == ReplicationSyncMode::ASYNC) {
        // Don't wait for ACKs
        return;
    } else if (config.sync_mode == ReplicationSyncMode::SYNC) {
        // Wait for ALL replicas to ACK
        for (auto& ack_future : acks) {
            ack_future.wait_for(config.sync_timeout);
        }
    } else if (config.sync_mode == ReplicationSyncMode::QUORUM) {
        // Wait for QUORUM replicas to ACK
        uint32_t ack_count = 0;
        for (auto& ack_future : acks) {
            if (ack_future.wait_for(config.sync_timeout) == future_status::ready) {
                ack_count++;
                if (ack_count >= config.quorum_replicas) {
                    return;  // Quorum achieved
                }
            }
        }
        throw QuorumNotAchievedException("Failed to achieve replication quorum");
    }
}
```

**Replica Side**:
```cpp
ReplicaAck receive_wal_record(const WALRecord& record) {
    ReplicationState& state = get_replication_state(record.shard_uuid);

    // 1. Verify generation number (fencing)
    if (record.generation_number < state.current_generation) {
        return ReplicaAck{status: ACK_REJECTED, reason: "Stale generation number"};
    }

    // 2. Check LSN continuity
    if (record.lsn != state.last_received_lsn + 1) {
        log_warning("LSN gap detected: expected {}, got {}",
            state.last_received_lsn + 1, record.lsn);
        // Request missing write-after log (WAL) records
        request_missing_wal(state.last_received_lsn + 1, record.lsn);
    }

    // 3. Write to replica's write-after log (WAL) buffer
    wal_buffer.append(record);
    state.last_received_lsn = record.lsn;

    // 4. Apply to storage (asynchronously)
    background_worker.schedule([record]() {
        apply_wal_record_to_storage(record);
    });

    // 5. Send ACK to primary
    state.last_contact_at = now();
    return ReplicaAck{status: ACK_SUCCESS, lsn: record.lsn};
}
```

### 4.2 Write-after Log (WAL) Record Application

```cpp
void apply_wal_record_to_storage(const WALRecord& record) {
    switch (record.type) {
        case WALRecordType::BEGIN:
            begin_transaction(record.transaction_id);
            break;

        case WALRecordType::INSERT: {
            Table* table = catalog.get_table(record.table_uuid);
            Row row = deserialize_row(record.row_data);
            table->insert_row(row, record.transaction_id);
            break;
        }

        case WALRecordType::UPDATE: {
            Table* table = catalog.get_table(record.table_uuid);
            Row new_row = deserialize_row(record.row_data);
            table->update_row(record.primary_key, new_row, record.transaction_id);
            break;
        }

        case WALRecordType::DELETE: {
            Table* table = catalog.get_table(record.table_uuid);
            table->delete_row(record.primary_key, record.transaction_id);
            break;
        }

        case WALRecordType::COMMIT:
            commit_transaction(record.transaction_id, record.commit_timestamp);
            break;

        case WALRecordType::ROLLBACK:
            rollback_transaction(record.transaction_id);
            break;

        case WALRecordType::CHECKPOINT:
            perform_checkpoint(record.checkpoint_lsn);
            break;
    }

    // Update replica state
    ReplicationState& state = get_replication_state(record.shard_uuid);
    state.last_applied_lsn = record.lsn;
}
```

### 4.3 Partial Replication (Future)

Logical replication enables filtering by table:

```sql
-- Create filtered replica (only replicate specific tables)
CREATE REPLICA FOR SHARD shard_001
  ON NODE replica_node_1
  REPLICATE TABLES (orders, customers)
  EXCLUDE TABLES (audit_log, temp_data);
```

**Use Case**: Reduce replica storage by excluding large temporary tables.

---

## 5. Physical Replication

### 5.1 Block-Level Streaming

**Primary Side**:
```cpp
void stream_physical_blocks(const vector<DataBlock>& dirty_blocks) {
    ReplicationConfiguration config = get_replication_config(shard_uuid);

    for (const DataBlock& block : dirty_blocks) {
        PhysicalReplicationMessage msg;
        msg.shard_uuid = shard_uuid;
        msg.generation_number = config.generation_number;
        msg.block_number = block.block_number;
        msg.block_data = block.data;
        msg.block_hash = compute_block_hash(block.data);

        // Stream to all replicas
        for (const UUID& replica_uuid : config.replica_node_uuids) {
            rpc_client.send_physical_block_async(replica_uuid, msg);
        }
    }
}
```

**Replica Side**:
```cpp
void receive_physical_block(const PhysicalReplicationMessage& msg) {
    // 1. Verify generation number
    if (msg.generation_number < current_generation) {
        return;  // Ignore stale blocks
    }

    // 2. Verify block hash
    bytes32 computed_hash = compute_block_hash(msg.block_data);
    if (computed_hash != msg.block_hash) {
        log_error("Block corruption detected: block {}", msg.block_number);
        return;
    }

    // 3. Write block to disk
    storage_engine.write_block(msg.block_number, msg.block_data);

    // 4. Flush to disk (ensure durability)
    storage_engine.sync();
}
```

### 5.2 Performance Characteristics

**Logical Replication**:
- **Throughput**: ~50-100 MB/s per replica (depends on CPU for deserialization)
- **CPU Usage**: Higher (parsing, row-level application)
- **Flexibility**: High (can filter tables, different storage layouts)

**Physical Replication**:
- **Throughput**: ~500-1000 MB/s per replica (limited by network/disk)
- **CPU Usage**: Lower (memcpy operations)
- **Flexibility**: Low (replicas must be identical)

---

## 6. Failover and Promotion

### 6.1 Primary Failure Detection

**Raft Leader Monitors Primary Health**:
```cpp
void monitor_shard_primary_health(const UUID& shard_uuid) {
    ReplicationConfiguration config = get_replication_config(shard_uuid);
    UUID primary_node = config.primary_node_uuid;

    // Send heartbeat
    HeartbeatResponse resp = rpc_client.heartbeat(primary_node, timeout: 5s);

    if (resp.status == HeartbeatStatus::FAILED) {
        primary_failure_count[shard_uuid]++;

        if (primary_failure_count[shard_uuid] >= 3) {
            // Primary has failed - initiate failover
            log_critical("Shard {} primary {} has failed - initiating failover",
                shard_uuid, primary_node);

            initiate_failover(shard_uuid);
        }
    } else {
        primary_failure_count[shard_uuid] = 0;  // Reset counter
    }
}
```

### 6.2 Promotion Procedure

**Raft-Coordinated Promotion**:

```cpp
void initiate_failover(const UUID& shard_uuid) {
    ReplicationConfiguration old_config = get_replication_config(shard_uuid);

    // 1. Select new primary (replica with least lag)
    UUID new_primary_uuid = select_promotion_candidate(shard_uuid, old_config);

    if (new_primary_uuid.is_nil()) {
        log_critical("No viable replica for failover - shard {} unavailable", shard_uuid);
        return;
    }

    // 2. Increment generation number (for fencing)
    uint64_t new_generation = old_config.generation_number + 1;

    // 3. Create new replication configuration
    ReplicationConfiguration new_config = old_config;
    new_config.primary_node_uuid = new_primary_uuid;
    new_config.generation_number = new_generation;
    new_config.primary_promoted_at = now();

    // Remove new primary from replica list
    new_config.replica_node_uuids.erase(
        std::remove(new_config.replica_node_uuids.begin(),
                    new_config.replica_node_uuids.end(),
                    new_primary_uuid),
        new_config.replica_node_uuids.end()
    );

    // 4. Propose configuration change via Raft
    RaftLogEntry entry;
    entry.type = REPLICATION_CONFIG_UPDATE;
    entry.shard_uuid = shard_uuid;
    entry.new_config = new_config;

    raft.propose(entry);  // Distributed consensus on promotion

    // 5. Once committed, notify new primary
    rpc_client.promote_to_primary(new_primary_uuid, shard_uuid, new_generation);

    // 6. Fence old primary (if it comes back)
    rpc_client.fence_primary(old_config.primary_node_uuid, shard_uuid, new_generation);
}
```

### 6.3 Promotion Candidate Selection

```cpp
UUID select_promotion_candidate(
    const UUID& shard_uuid,
    const ReplicationConfiguration& config
) {
    UUID best_candidate;
    uint64_t min_lag = UINT64_MAX;

    for (const UUID& replica_uuid : config.replica_node_uuids) {
        // Query replica's replication state
        ReplicationState state = rpc_client.get_replication_state(replica_uuid, shard_uuid);

        if (state.status == ReplicaStatus::STREAMING ||
            state.status == ReplicaStatus::CATCHING_UP) {

            // Compute lag in LSN units
            uint64_t lag_lsn = current_lsn - state.last_applied_lsn;

            if (lag_lsn < min_lag) {
                min_lag = lag_lsn;
                best_candidate = replica_uuid;
            }
        }
    }

    // Only promote if lag is acceptable (< 1000 LSNs)
    if (min_lag > 1000) {
        log_warning("Best replica candidate has high lag: {} LSNs", min_lag);
    }

    return best_candidate;
}
```

### 6.4 Fencing (Split-Brain Prevention)

**Generation Number Fencing**:

```cpp
Status accept_write(const WriteRequest& request) {
    ReplicationConfiguration config = get_replication_config(request.shard_uuid);

    // Check if this node is the current primary
    if (config.primary_node_uuid != my_node_uuid) {
        return Status::NOT_PRIMARY("This node is not the primary for this shard");
    }

    // Check generation number
    if (request.generation_number != config.generation_number) {
        // This node may have been demoted
        log_critical("Write rejected: generation mismatch (expected {}, got {})",
            config.generation_number, request.generation_number);

        // Query Raft for current config
        ReplicationConfiguration latest_config = raft.get_replication_config(request.shard_uuid);

        if (latest_config.primary_node_uuid != my_node_uuid) {
            // This node has been demoted - fence ourselves
            fence_self(request.shard_uuid, latest_config.generation_number);
            return Status::DEMOTED("Node has been demoted");
        }
    }

    // Accept write
    return Status::OK();
}

void fence_self(const UUID& shard_uuid, uint64_t new_generation) {
    log_critical("FENCING: Node demoted for shard {} (new generation: {})",
        shard_uuid, new_generation);

    ReplicationState& state = get_replication_state(shard_uuid);
    state.role = ReplicaRole::DEMOTED;

    // Stop accepting writes
    shard_manager.set_read_only(shard_uuid);

    // Alert administrators
    alert_administrators("NODE_DEMOTED", shard_uuid);
}
```

---

## 7. Replication Lag Monitoring

### 7.1 Lag Calculation

```cpp
struct ReplicationLag {
    uint64_t lsn_lag;                    // LSN difference
    duration_t time_lag;                 // Time difference
    uint64_t bytes_lag;                  // Bytes behind primary
};

ReplicationLag compute_replication_lag(const UUID& replica_uuid, const UUID& shard_uuid) {
    // Get primary's current LSN
    uint64_t primary_lsn = get_current_lsn(shard_uuid);

    // Get replica's last applied LSN
    ReplicationState replica_state = rpc_client.get_replication_state(replica_uuid, shard_uuid);
    uint64_t replica_lsn = replica_state.last_applied_lsn;

    ReplicationLag lag;
    lag.lsn_lag = primary_lsn - replica_lsn;

    // Time lag: primary's current time - replica's last contact
    lag.time_lag = now() - replica_state.last_contact_at;

    // Bytes lag: estimate based on write-after log (WAL) record size
    lag.bytes_lag = lag.lsn_lag * average_wal_record_size;

    return lag;
}
```

### 7.2 Lag Alerting

```cpp
void monitor_replication_lag() {
    for (const Shard& shard : cluster.get_shards()) {
        ReplicationConfiguration config = get_replication_config(shard.shard_uuid);

        for (const UUID& replica_uuid : config.replica_node_uuids) {
            ReplicationLag lag = compute_replication_lag(replica_uuid, shard.shard_uuid);

            // Alert if time lag > 10 seconds
            if (lag.time_lag > 10s) {
                alert(AlertLevel::WARNING,
                    fmt::format("Replica {} for shard {} is {}s behind",
                        replica_uuid, shard.shard_uuid, lag.time_lag.seconds()));
            }

            // Alert if LSN lag > 10,000
            if (lag.lsn_lag > 10000) {
                alert(AlertLevel::CRITICAL,
                    fmt::format("Replica {} for shard {} is {} LSNs behind",
                        replica_uuid, shard.shard_uuid, lag.lsn_lag));
            }

            // Alert if replica is disconnected
            if (lag.time_lag > 60s) {
                alert(AlertLevel::CRITICAL,
                    fmt::format("Replica {} for shard {} is DISCONNECTED",
                        replica_uuid, shard.shard_uuid));
            }
        }
    }
}
```

---

## 8. Operational Procedures

### 8.1 Adding a New Replica

```sql
-- Add a new replica for shard_001
ALTER SHARD shard_001
  ADD REPLICA ON NODE node_d
  WITH MODE = LOGICAL;
```

**Procedure**:
1. New replica connects to primary
2. Primary sends full snapshot (base backup)
3. Replica loads snapshot
4. Replica begins streaming write-after log (WAL) from snapshot LSN
5. Replica catches up to primary
6. Replica status changes to STREAMING

### 8.2 Removing a Replica

```sql
-- Remove a replica
ALTER SHARD shard_001
  REMOVE REPLICA ON NODE node_d;
```

**Procedure**:
1. Primary stops sending write-after log (WAL) to replica
2. Replica is marked as REMOVED
3. Replica's data can be deleted

### 8.3 Manual Failover

```sql
-- Manually promote a replica to primary
ALTER SHARD shard_001
  PROMOTE REPLICA ON NODE node_b TO PRIMARY;
```

**Procedure**:
1. Verify replica is caught up (lag < 100 LSNs)
2. Increment generation number
3. Promote replica to primary
4. Demote old primary to replica
5. Update Raft configuration

### 8.4 Switchback After Failover

After a failover, the original primary may come back online:

```sql
-- Switch back to original primary
ALTER SHARD shard_001
  PROMOTE REPLICA ON NODE node_a TO PRIMARY;  -- Original primary
```

**Procedure**:
1. Original primary must catch up as a replica first
2. Once caught up, can be promoted back
3. Ensures no data loss

---

## 9. Consistency Guarantees

### 9.1 Async Replication

**Guarantee**: **Eventual Consistency**

- Primary ACKs write immediately (after local write-after log (WAL) flush)
- Replicas receive and apply write-after log (WAL) asynchronously
- If primary fails before replica applies, data may be lost

**Data Loss Window**: Typically < 1 second (depends on replication lag)

### 9.2 Sync Replication

**Guarantee**: **Strong Consistency**

- Primary waits for ALL replicas to ACK before returning to client
- No data loss on primary failure (replicas have all data)
- Higher write latency (network RTT + replica flush time)

**Trade-off**: Durability vs. Latency

### 9.3 Quorum Replication

**Guarantee**: **Quorum Consistency**

- Primary waits for QUORUM replicas to ACK (e.g., 1 of 2)
- Balance between durability and latency
- Tolerates minority replica failures

**Example**: With 2 replicas, quorum = 1
- Primary + 1 replica ACK = write succeeds
- If 1 replica is down, writes still succeed

---

## 10. Security Considerations

### 10.1 Encrypted Replication Streams

Write-after log (WAL) streams are encrypted using TLS 1.3:

```cpp
// Replicas connect to primary with mTLS
TLSContext tls_ctx;
tls_ctx.load_certificate(my_node_cert);
tls_ctx.load_private_key(my_node_key);
tls_ctx.set_ca_bundle(cluster_ca_bundle);
tls_ctx.set_min_protocol_version(TLS_1_3);

// Establish encrypted connection
TLSSocket socket = tls_ctx.connect(primary_node_address);

// Stream write-after log (WAL) over encrypted socket
WALStream stream(socket);
```

### 10.2 Replica Authentication

Replicas authenticate to primary using X.509 certificates (SBCLUSTER-02):

```cpp
bool authenticate_replica(const X509Certificate& replica_cert) {
    // 1. Verify certificate chain
    if (!verify_certificate_chain(replica_cert, cluster_ca_bundle)) {
        return false;
    }

    // 2. Verify URI SAN
    string expected_uri = fmt::format("sb://cluster/{}/node/{}",
        cluster_uuid, replica_node_uuid);

    if (replica_cert.get_uri_san() != expected_uri) {
        return false;
    }

    // 3. Check node is authorized as replica
    if (!node_has_role(replica_node_uuid, NodeRole::SHARD_OWNER)) {
        return false;
    }

    return true;
}
```

### 10.3 Write-after Log (WAL) Encryption at Rest

Write-after log (WAL) files are encrypted on disk using AES-256-GCM:

```cpp
// Write encrypted write-after log (WAL) record
EncryptedWALRecord encrypt_wal_record(const WALRecord& record) {
    bytes plaintext = serialize(record);

    // Derive key from MEK (Master Encryption Key)
    bytes32 wal_key = derive_key(MEK, "wal_encryption", shard_uuid);

    // Encrypt with AES-256-GCM
    EncryptedData encrypted = aes256gcm_encrypt(wal_key, plaintext);

    EncryptedWALRecord encrypted_record;
    encrypted_record.ciphertext = encrypted.ciphertext;
    encrypted_record.nonce = encrypted.nonce;
    encrypted_record.auth_tag = encrypted.auth_tag;

    return encrypted_record;
}
```

---

## 11. Testing Procedures

### 11.1 Functional Tests

**Test: Write-after Log (WAL) Streaming**
```cpp
TEST(Replication, WALStreaming) {
    auto cluster = create_test_cluster(3, 16);

    UUID shard_uuid = cluster.shards()[0].shard_uuid;
    UUID primary_uuid = cluster.get_shard_primary(shard_uuid);
    UUID replica_uuid = cluster.get_shard_replicas(shard_uuid)[0];

    // Insert data on primary
    cluster.node(primary_uuid).execute(
        "INSERT INTO test_table (id, value) VALUES (1, 'hello')"
    );

    // Wait for replication
    sleep(100ms);

    // Verify data on replica
    ResultSet result = cluster.node(replica_uuid).execute(
        "SELECT * FROM test_table WHERE id = 1"
    );

    EXPECT_EQ(result.rows.size(), 1);
    EXPECT_EQ(result.rows[0].get_string("value"), "hello");
}
```

**Test: Failover**
```cpp
TEST(Replication, Failover) {
    auto cluster = create_test_cluster(3, 16);

    UUID shard_uuid = cluster.shards()[0].shard_uuid;
    UUID old_primary_uuid = cluster.get_shard_primary(shard_uuid);

    // Kill primary
    cluster.kill_node(old_primary_uuid);

    // Wait for failover
    ASSERT_TRUE(cluster.wait_for_shard_primary_change(shard_uuid, 30s));

    // Verify new primary is elected
    UUID new_primary_uuid = cluster.get_shard_primary(shard_uuid);
    EXPECT_NE(new_primary_uuid, old_primary_uuid);

    // Verify shard is still writable
    cluster.node(new_primary_uuid).execute(
        "INSERT INTO test_table (id, value) VALUES (2, 'world')"
    );
}
```

### 11.2 Performance Tests

**Test: Replication Throughput**
```cpp
TEST(Replication, Throughput) {
    auto cluster = create_test_cluster(2, 1);  // 1 shard, 1 replica

    UUID shard_uuid = cluster.shards()[0].shard_uuid;
    UUID primary_uuid = cluster.get_shard_primary(shard_uuid);

    // Insert 100k rows
    auto start = now();
    for (uint32_t i = 0; i < 100000; ++i) {
        cluster.node(primary_uuid).execute(
            fmt::format("INSERT INTO test_table (id, value) VALUES ({}, 'data')", i)
        );
    }
    auto elapsed = now() - start;

    // Wait for replica to catch up
    ASSERT_TRUE(cluster.wait_for_replication_lag(shard_uuid, 0, 30s));

    // Verify throughput (expect > 10k writes/sec)
    double throughput = 100000.0 / elapsed.seconds();
    EXPECT_GT(throughput, 10000.0);
}
```

---

## 12. Examples

### 12.1 Configure Async Replication

```sql
-- Set shard to async replication (default)
ALTER SHARD shard_001
  SET REPLICATION MODE = ASYNC;
```

**Behavior**: Primary ACKs writes immediately after local write-after log (WAL) flush.

### 12.2 Configure Sync Replication

```sql
-- Set shard to sync replication (high durability)
ALTER SHARD shard_001
  SET REPLICATION MODE = SYNC;
```

**Behavior**: Primary waits for all replicas to ACK before returning to client.

### 12.3 Configure Quorum Replication

```sql
-- Set shard to quorum replication (1 of 2 replicas)
ALTER SHARD shard_001
  SET REPLICATION MODE = QUORUM,
      QUORUM_REPLICAS = 1;
```

**Behavior**: Primary waits for 1 replica to ACK (out of 2 total).

### 12.4 Monitor Replication Lag

```sql
-- Query replication lag
SELECT
    shard_uuid,
    replica_node_uuid,
    lsn_lag,
    time_lag_seconds,
    status
FROM sys.replication_lag
WHERE shard_uuid = 'shard_001'
ORDER BY time_lag_seconds DESC;
```

**Output**:
```
shard_uuid  | replica_node_uuid  | lsn_lag | time_lag_seconds | status
------------|--------------------|---------|--------------------|----------
shard_001   | node_b             | 150     | 0.2                | STREAMING
shard_001   | node_c             | 8500    | 12.5               | CATCHING_UP
```

---

## 13. Performance Characteristics

### 13.1 Replication Latency

| Mode       | Latency Overhead | Durability            |
|------------|------------------|-----------------------|
| ASYNC      | ~0 ms            | Best-effort           |
| QUORUM     | ~2-5 ms          | Quorum guaranteed     |
| SYNC       | ~5-10 ms         | Full synchronous      |

### 13.2 Network Bandwidth

**Logical Replication**:
- ~50-100 MB/s per replica (CPU-bound)

**Physical Replication**:
- ~500-1000 MB/s per replica (network/disk-bound)

### 13.3 Failover Time

- **Detection**: 3-5 seconds (3 missed heartbeats @ 5s interval)
- **Promotion**: 1-2 seconds (Raft consensus + RPC)
- **Total Downtime**: ~5-10 seconds

---

## 14. References

### 14.1 Internal References
- **SBCLUSTER-00**: Guiding Principles
- **SBCLUSTER-02**: Membership and Identity
- **SBCLUSTER-05**: Sharding
- **SBCLUSTER-08**: Backup and Restore

### 14.2 External References
- **PostgreSQL Streaming Replication**: https://www.postgresql.org/docs/current/warm-standby.html
- **MySQL Group Replication**: https://dev.mysql.com/doc/refman/8.0/en/group-replication.html
- **MongoDB Replica Sets**: https://www.mongodb.com/docs/manual/replication/
- **Raft Consensus**: https://raft.github.io/

### 14.3 Research Papers
- *"Replication, History, and Grafting in the Ori File System"* - Mashtizadeh et al., SOSP 2013
- *"Chain Replication for Supporting High Throughput and Availability"* - van Renesse & Schneider, OSDI 2004

---

## 15. Revision History

| Version | Date       | Author       | Changes                                    |
|---------|------------|--------------|--------------------------------------------|
| 1.0     | 2026-01-02 | D. Calford   | Initial comprehensive specification        |

---

**Document Status**: DRAFT (Beta Specification Phase)
**Next Review**: Before Beta Implementation Phase
**Approval Required**: Chief Architect, Storage Engineering Lead, Distributed Systems Lead
