# SBCLUSTER-08: Backup and Restore

## 1. Introduction

### 1.1 Purpose
This document specifies the backup and restore architecture for ScratchBird clusters. Backups are performed **per-shard** with optional **cluster-consistent backup sets** for point-in-time recovery. A critical principle: **restore does not restore trust** - cryptographic material (certificates, CA, keys) is never backed up or restored.

### 1.2 Scope
- Per-shard backup procedures
- Cluster-consistent backup sets (coordinated snapshots)
- Full and incremental backups
- Restore procedures and validation
- Trust boundary enforcement (no certificate/key backup)
- Backup encryption and verification
- Retention policies and lifecycle management
- Cross-region backup replication

**Scope Note:** WAL references describe an optional write-after log stream for replication/PITR; MGA does not use WAL for recovery.

### 1.3 Related Documents
- **SBCLUSTER-00**: Guiding Principles (shard-local MVCC, shared-nothing)
- **SBCLUSTER-02**: Membership and Identity (certificates never backed up)
- **SBCLUSTER-05**: Sharding (per-shard backup model)
- **SBCLUSTER-07**: Replication (backup from replicas to reduce primary load)

### 1.4 Terminology
- **Backup Set**: Collection of per-shard backups taken at coordinated time
- **Full Backup**: Complete snapshot of shard data at a point in time
- **Incremental Backup**: Changes since last backup (write-after log (WAL) segments)
- **PITR**: Point-In-Time Recovery (restore to specific timestamp)
- **Trust Material**: Certificates, private keys, CA bundles (never backed up)
- **Backup Target**: Node with BACKUP_TARGET role that stores backups

---

## 2. Architecture Overview

### 2.1 Design Principles

1. **Per-Shard Backups**: Each shard backed up independently (shared-nothing)
2. **Backup from Replicas**: Prefer backup from replicas to avoid primary load
3. **Cluster-Consistent Sets**: Optional coordinated snapshots across all shards
4. **No Trust Material**: Certificates, keys, CA bundles NEVER backed up
5. **Encrypted at Rest**: All backups encrypted with AES-256-GCM
6. **Immutable Backups**: Once written, backups cannot be modified
7. **Retention Policies**: Automated lifecycle management (daily, weekly, monthly)

### 2.2 Backup Topology

```
┌─────────────────────────────────────────────────────────────┐
│                   BACKUP COORDINATOR                        │
│  (Raft Leader orchestrates cluster-wide backups)            │
└─────┬───────────────┬───────────────┬───────────────────────┘
      │               │               │
      ▼               ▼               ▼
┌─────────────┐ ┌─────────────┐ ┌─────────────┐
│  SHARD 1    │ │  SHARD 2    │ │  SHARD 3    │
│  Replica    │ │  Replica    │ │  Replica    │
│  (Node B)   │ │  (Node C)   │ │  (Node D)   │
└──────┬──────┘ └──────┬──────┘ └──────┬──────┘
       │               │               │
       │  Backup Data  │               │
       ▼               ▼               ▼
┌─────────────────────────────────────────────────────────────┐
│              BACKUP TARGET NODE (Node E)                    │
│  - Receives and stores encrypted backup files               │
│  - S3/Object Storage backend                                │
│  - Deduplication and compression                            │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 Trust Boundary

**CRITICAL**: Restore does not restore trust.

**Never Backed Up**:
- Node X.509 certificates
- Private keys (Ed25519, ECDSA)
- CA bundles
- TLS session keys
- Cluster UUIDs (regenerated on restore)

**Backed Up**:
- User data (tables, rows)
- Catalog metadata (schemas, domains, procedures)
- Write-after log (WAL) segments
- Security policies (RLS, CLS, masking functions)
- User accounts and grants

**Implication**: Restored cluster must re-establish trust from scratch (new CA, new certificates).

---

## 3. Data Structures

### 3.1 BackupSet

```cpp
struct BackupSet {
    UUID backup_set_uuid;                // Unique backup set identifier (v7)
    UUID cluster_uuid;                   // Original cluster UUID

    BackupType type;                     // FULL | INCREMENTAL
    timestamp_t backup_started_at;       // When backup began
    timestamp_t backup_completed_at;     // When backup completed

    ShardBackup[] shard_backups;         // Per-shard backup metadata

    bool is_cluster_consistent;          // Coordinated snapshot across shards?
    timestamp_t consistency_timestamp;   // PITR timestamp (if cluster-consistent)

    BackupSetState state;                // PENDING | IN_PROGRESS | COMPLETED | FAILED

    bytes32 backup_set_hash;             // SHA-256 hash of all shard backup hashes
    Signature backup_set_signature;      // Signed by backup coordinator
};

enum BackupType {
    FULL,         // Full snapshot
    INCREMENTAL   // write-after log (WAL) segments since last full backup
};

enum BackupSetState {
    PENDING,
    IN_PROGRESS,
    COMPLETED,
    FAILED
};
```

### 3.2 ShardBackup

```cpp
struct ShardBackup {
    UUID shard_backup_uuid;              // Unique shard backup identifier (v7)
    UUID shard_uuid;                     // Shard being backed up
    UUID backup_set_uuid;                // Parent backup set

    UUID source_node_uuid;               // Node backup was taken from (usually replica)
    uint64_t source_generation;          // Replication generation at backup time

    BackupType type;                     // FULL | INCREMENTAL
    uint64_t start_lsn;                  // Starting LSN
    uint64_t end_lsn;                    // Ending LSN
    timestamp_t backup_timestamp;        // PITR timestamp

    string backup_path;                  // Path on backup target (e.g., s3://bucket/...)
    uint64_t backup_size_bytes;          // Uncompressed size
    uint64_t compressed_size_bytes;      // Compressed size

    bytes32 backup_hash;                 // SHA-256 hash of backup data
    EncryptionInfo encryption;           // Encryption metadata
};
```

### 3.3 BackupTarget

```cpp
struct BackupTarget {
    UUID target_node_uuid;               // Node with BACKUP_TARGET role
    BackupStorageType storage_type;      // S3 | AZURE_BLOB | GCS | LOCAL

    string storage_endpoint;             // e.g., s3://bucket-name/prefix
    CredentialInfo credentials;          // Access credentials (encrypted)

    uint64_t total_capacity_bytes;       // Total storage capacity
    uint64_t used_capacity_bytes;        // Used storage

    BackupRetentionPolicy retention;     // Retention policy
};

enum BackupStorageType {
    S3,           // Amazon S3 or compatible
    AZURE_BLOB,   // Azure Blob Storage
    GCS,          // Google Cloud Storage
    LOCAL         // Local filesystem (for testing/dev)
};
```

### 3.4 BackupRetentionPolicy

```cpp
struct BackupRetentionPolicy {
    uint32_t daily_retention_days;       // Keep daily backups for N days (e.g., 7)
    uint32_t weekly_retention_weeks;     // Keep weekly backups for N weeks (e.g., 4)
    uint32_t monthly_retention_months;   // Keep monthly backups for N months (e.g., 12)
    uint32_t yearly_retention_years;     // Keep yearly backups for N years (e.g., 5)

    bool auto_delete_expired;            // Automatically delete expired backups?
};
```

---

## 4. Backup Procedures

### 4.1 Per-Shard Full Backup

**Algorithm**: Take full snapshot of a single shard.

```cpp
ShardBackup perform_full_shard_backup(
    const UUID& shard_uuid,
    const UUID& source_node_uuid,
    const BackupTarget& target
) {
    // 1. Start read-only snapshot on source node
    Transaction snapshot_txn = begin_transaction(
        source_node_uuid,
        TransactionMode::READ_ONLY
    );

    uint64_t snapshot_lsn = snapshot_txn.get_snapshot_lsn();
    timestamp_t snapshot_time = now();

    // 2. Create backup metadata
    ShardBackup backup;
    backup.shard_backup_uuid = UUID::v7();
    backup.shard_uuid = shard_uuid;
    backup.source_node_uuid = source_node_uuid;
    backup.type = BackupType::FULL;
    backup.start_lsn = 0;
    backup.end_lsn = snapshot_lsn;
    backup.backup_timestamp = snapshot_time;

    // 3. Stream shard data to backup target
    BackupStream stream = open_backup_stream(target, backup.shard_backup_uuid);

    // 4. Backup catalog metadata (schemas, tables, domains, etc.)
    CatalogSnapshot catalog = snapshot_txn.export_catalog();
    stream.write_catalog(catalog);

    // 5. Backup user data (table by table)
    for (const Table& table : catalog.tables) {
        TableIterator iter = snapshot_txn.scan_table(table.table_uuid);

        while (iter.has_next()) {
            Row row = iter.next();
            stream.write_row(table.table_uuid, row);
        }
    }

    // 6. Backup write-after log (WAL) segments up to snapshot_lsn
    WALSegments wal = export_wal_segments(shard_uuid, 0, snapshot_lsn);
    stream.write_wal_segments(wal);

    // 7. Finalize backup
    stream.close();

    backup.backup_size_bytes = stream.get_total_bytes_written();
    backup.compressed_size_bytes = stream.get_compressed_bytes_written();
    backup.backup_hash = stream.get_backup_hash();
    backup.backup_path = stream.get_backup_path();

    snapshot_txn.commit();

    return backup;
}
```

### 4.2 Per-Shard Incremental Backup

**Algorithm**: Backup write-after log (WAL) segments since last backup.

```cpp
ShardBackup perform_incremental_shard_backup(
    const UUID& shard_uuid,
    const ShardBackup& last_full_backup,
    const BackupTarget& target
) {
    // 1. Get current LSN
    uint64_t current_lsn = get_current_lsn(shard_uuid);

    // 2. Create incremental backup metadata
    ShardBackup backup;
    backup.shard_backup_uuid = UUID::v7();
    backup.shard_uuid = shard_uuid;
    backup.type = BackupType::INCREMENTAL;
    backup.start_lsn = last_full_backup.end_lsn;
    backup.end_lsn = current_lsn;
    backup.backup_timestamp = now();

    // 3. Export write-after log (WAL) segments
    WALSegments wal = export_wal_segments(
        shard_uuid,
        backup.start_lsn,
        backup.end_lsn
    );

    // 4. Write write-after log (WAL) segments to backup target
    BackupStream stream = open_backup_stream(target, backup.shard_backup_uuid);
    stream.write_wal_segments(wal);
    stream.close();

    backup.backup_size_bytes = stream.get_total_bytes_written();
    backup.compressed_size_bytes = stream.get_compressed_bytes_written();
    backup.backup_hash = stream.get_backup_hash();
    backup.backup_path = stream.get_backup_path();

    return backup;
}
```

### 4.3 Cluster-Consistent Backup Set

**Algorithm**: Coordinate snapshots across all shards for PITR.

```cpp
BackupSet perform_cluster_consistent_backup(const BackupTarget& target) {
    // 1. Create backup set
    BackupSet backup_set;
    backup_set.backup_set_uuid = UUID::v7();
    backup_set.cluster_uuid = get_cluster_uuid();
    backup_set.type = BackupType::FULL;
    backup_set.is_cluster_consistent = true;
    backup_set.backup_started_at = now();
    backup_set.state = BackupSetState::IN_PROGRESS;

    // 2. Coordinate snapshot timestamp across all shards
    //    Use Raft to agree on a global snapshot time
    timestamp_t global_snapshot_time = coordinate_snapshot_time();
    backup_set.consistency_timestamp = global_snapshot_time;

    // 3. For each shard, wait until LSN reaches snapshot time
    map<UUID, uint64_t> shard_snapshot_lsns;

    for (const Shard& shard : cluster.get_shards()) {
        // Wait for shard's LSN to reach global snapshot time
        uint64_t snapshot_lsn = wait_for_lsn_at_timestamp(
            shard.shard_uuid,
            global_snapshot_time
        );

        shard_snapshot_lsns[shard.shard_uuid] = snapshot_lsn;
    }

    // 4. Perform per-shard backups concurrently
    vector<future<ShardBackup>> shard_backup_futures;

    for (const Shard& shard : cluster.get_shards()) {
        // Backup from replica if available, else from primary
        UUID backup_source = select_backup_source(shard);

        future<ShardBackup> backup_future = async([&]() {
            return perform_full_shard_backup(
                shard.shard_uuid,
                backup_source,
                target
            );
        });

        shard_backup_futures.push_back(std::move(backup_future));
    }

    // 5. Wait for all shard backups to complete
    for (auto& future : shard_backup_futures) {
        ShardBackup shard_backup = future.get();
        backup_set.shard_backups.push_back(shard_backup);
    }

    // 6. Compute backup set hash
    backup_set.backup_completed_at = now();
    backup_set.backup_set_hash = compute_backup_set_hash(backup_set);
    backup_set.state = BackupSetState::COMPLETED;

    // 7. Sign backup set
    backup_set.backup_set_signature = sign_backup_set(backup_set);

    // 8. Store backup set metadata
    store_backup_set_metadata(target, backup_set);

    return backup_set;
}
```

---

## 5. Restore Procedures

### 5.1 Restore Trust Boundary

**CRITICAL PRINCIPLE**: Restore does NOT restore trust.

When restoring a cluster from backup:

1. **Generate NEW cluster UUID** (different from original)
2. **Generate NEW CA** (do not reuse original CA)
3. **Issue NEW certificates** for all nodes
4. **Re-establish mTLS** connections
5. **User credentials preserved** (username/password hashes restored)
6. **Security policies preserved** (RLS, CLS, masking functions restored)

**Rationale**: If original cluster was compromised, restored cluster starts with clean trust.

### 5.2 Per-Shard Restore

**Algorithm**: Restore a single shard from backup.

```cpp
void restore_shard_from_backup(
    const UUID& target_shard_uuid,
    const ShardBackup& full_backup,
    const vector<ShardBackup>& incremental_backups
) {
    // 1. Verify backup integrity
    if (!verify_backup_hash(full_backup)) {
        throw BackupCorruptedException("Full backup hash mismatch");
    }

    // 2. Create empty shard
    Shard shard = create_empty_shard(target_shard_uuid);

    // 3. Restore catalog from full backup
    BackupStream full_stream = open_backup_stream_for_read(full_backup);
    CatalogSnapshot catalog = full_stream.read_catalog();

    shard.import_catalog(catalog);

    // 4. Restore user data from full backup
    while (full_stream.has_next_row()) {
        UUID table_uuid = full_stream.read_table_uuid();
        Row row = full_stream.read_row();

        shard.insert_row(table_uuid, row);
    }

    full_stream.close();

    // 5. Apply incremental backups (write-after log (WAL) replay)
    for (const ShardBackup& inc_backup : incremental_backups) {
        BackupStream inc_stream = open_backup_stream_for_read(inc_backup);
        WALSegments wal = inc_stream.read_wal_segments();

        for (const WALRecord& record : wal.records) {
            apply_wal_record_to_storage(shard, record);
        }

        inc_stream.close();
    }

    // 6. Update shard metadata
    shard.set_restored_from_backup(full_backup.shard_backup_uuid);
    shard.set_restore_timestamp(now());
}
```

### 5.3 Cluster Restore from Backup Set

**Algorithm**: Restore entire cluster from cluster-consistent backup set.

```cpp
void restore_cluster_from_backup_set(
    const BackupSet& backup_set,
    const BackupTarget& backup_target
) {
    // 1. CRITICAL: Generate new cluster UUID (trust boundary)
    UUID new_cluster_uuid = UUID::v7();

    log_warning("Restoring cluster with NEW UUID: {} (original: {})",
        new_cluster_uuid, backup_set.cluster_uuid);

    // 2. Initialize new cluster infrastructure
    initialize_cluster(new_cluster_uuid);

    // 3. Generate NEW CA and certificates (trust boundary)
    CAPolicy new_ca_policy = generate_new_ca_policy(new_cluster_uuid);
    issue_node_certificates(new_ca_policy);

    // 4. Create shards matching backup set
    for (const ShardBackup& shard_backup : backup_set.shard_backups) {
        // Generate new shard UUID
        UUID new_shard_uuid = UUID::v7();

        log_info("Restoring shard {} as new shard {}",
            shard_backup.shard_uuid, new_shard_uuid);

        // Restore shard data
        restore_shard_from_backup(new_shard_uuid, shard_backup, {});
    }

    // 5. Rebuild shard map
    ShardMap restored_shard_map = rebuild_shard_map_from_restored_shards();
    publish_shard_map(restored_shard_map);

    // 6. Restore security bundle (policies only, not trust material)
    SecurityBundle restored_security_bundle = extract_security_bundle_from_backup(backup_set);
    publish_security_bundle(restored_security_bundle);

    // 7. Mark cluster as restored
    set_cluster_metadata("restored_from_backup", backup_set.backup_set_uuid.to_string());
    set_cluster_metadata("restored_at", now().to_string());
    set_cluster_metadata("original_cluster_uuid", backup_set.cluster_uuid.to_string());
}
```

### 5.4 Point-In-Time Recovery (PITR)

**Algorithm**: Restore cluster to specific timestamp using write-after log (WAL) replay.

```cpp
void restore_to_point_in_time(
    const BackupSet& full_backup_set,
    const timestamp_t target_timestamp
) {
    // 1. Restore from full backup
    restore_cluster_from_backup_set(full_backup_set, backup_target);

    // 2. For each shard, replay write-after log (WAL) up to target timestamp
    for (const ShardBackup& shard_backup : full_backup_set.shard_backups) {
        // Find all incremental backups after full backup
        vector<ShardBackup> incremental_backups =
            find_incremental_backups_after(shard_backup.backup_timestamp);

        // Replay write-after log (WAL) up to target timestamp
        replay_wal_until_timestamp(
            shard_backup.shard_uuid,
            incremental_backups,
            target_timestamp
        );
    }
}

void replay_wal_until_timestamp(
    const UUID& shard_uuid,
    const vector<ShardBackup>& incremental_backups,
    const timestamp_t target_timestamp
) {
    for (const ShardBackup& inc_backup : incremental_backups) {
        BackupStream stream = open_backup_stream_for_read(inc_backup);
        WALSegments wal = stream.read_wal_segments();

        for (const WALRecord& record : wal.records) {
            // Stop when we reach target timestamp
            if (record.commit_timestamp > target_timestamp) {
                return;
            }

            apply_wal_record_to_storage(shard_uuid, record);
        }

        stream.close();
    }
}
```

---

## 6. Backup Encryption

### 6.1 Encryption at Rest

All backups are encrypted using AES-256-GCM:

```cpp
struct EncryptedBackup {
    bytes32 encryption_key_id;           // Key used for encryption
    bytes32 nonce;                       // Random nonce (96 bits)
    bytes ciphertext;                    // Encrypted backup data
    bytes16 auth_tag;                    // GCM authentication tag
};

EncryptedBackup encrypt_backup(const bytes& backup_data, const UUID& backup_uuid) {
    // 1. Derive backup-specific encryption key
    bytes32 backup_key = derive_key(
        cluster_mek,                     // Master Encryption Key
        "backup_encryption",
        backup_uuid
    );

    // 2. Generate random nonce
    bytes32 nonce = generate_random_nonce();

    // 3. Encrypt with AES-256-GCM
    EncryptedData encrypted = aes256gcm_encrypt(backup_key, nonce, backup_data);

    EncryptedBackup enc_backup;
    enc_backup.encryption_key_id = compute_key_id(backup_key);
    enc_backup.nonce = nonce;
    enc_backup.ciphertext = encrypted.ciphertext;
    enc_backup.auth_tag = encrypted.auth_tag;

    return enc_backup;
}
```

### 6.2 Key Management

**Backup Encryption Keys**:
- Derived from cluster Master Encryption Key (MEK)
- Per-backup derivation using backup UUID
- Keys never stored with backup data

**Key Rotation**:
- When MEK is rotated, old backups remain encrypted with old key
- New backups use new MEK
- Restore requires access to MEK at time of backup

---

## 7. Backup Verification

### 7.1 Hash Verification

```cpp
bool verify_backup_hash(const ShardBackup& backup) {
    // 1. Download backup from storage
    BackupStream stream = open_backup_stream_for_read(backup);

    // 2. Compute hash while reading
    SHA256Context hash_ctx;

    while (stream.has_more_data()) {
        bytes chunk = stream.read_chunk(1024 * 1024);  // 1MB chunks
        hash_ctx.update(chunk);
    }

    bytes32 computed_hash = hash_ctx.finalize();
    stream.close();

    // 3. Compare with stored hash
    if (computed_hash != backup.backup_hash) {
        log_error("Backup hash mismatch: expected {}, got {}",
            hex(backup.backup_hash), hex(computed_hash));
        return false;
    }

    return true;
}
```

### 7.2 Test Restore

Periodically perform test restores to verify backup integrity:

```cpp
void perform_test_restore(const BackupSet& backup_set) {
    // 1. Create isolated test environment
    TestCluster test_cluster = create_test_cluster();

    // 2. Attempt restore
    try {
        restore_cluster_from_backup_set(backup_set, test_backup_target);

        // 3. Verify restored data
        verify_restored_cluster(test_cluster, backup_set);

        log_info("Test restore of backup set {} succeeded",
            backup_set.backup_set_uuid);
    } catch (const Exception& e) {
        log_error("Test restore of backup set {} FAILED: {}",
            backup_set.backup_set_uuid, e.message());

        alert_administrators("BACKUP_VERIFICATION_FAILED", backup_set.backup_set_uuid);
    } finally {
        // 4. Clean up test environment
        test_cluster.destroy();
    }
}
```

---

## 8. Retention and Lifecycle Management

### 8.1 Retention Policy Enforcement

```cpp
void enforce_retention_policy(const BackupRetentionPolicy& policy) {
    timestamp_t now_ts = now();

    vector<BackupSet> all_backups = list_all_backup_sets();

    for (const BackupSet& backup_set : all_backups) {
        duration_t age = now_ts - backup_set.backup_completed_at;

        bool should_keep = false;

        // Keep if within daily retention
        if (age < duration_t::days(policy.daily_retention_days)) {
            should_keep = true;
        }
        // Keep if weekly backup within weekly retention
        else if (is_weekly_backup(backup_set) &&
                 age < duration_t::weeks(policy.weekly_retention_weeks)) {
            should_keep = true;
        }
        // Keep if monthly backup within monthly retention
        else if (is_monthly_backup(backup_set) &&
                 age < duration_t::months(policy.monthly_retention_months)) {
            should_keep = true;
        }
        // Keep if yearly backup within yearly retention
        else if (is_yearly_backup(backup_set) &&
                 age < duration_t::years(policy.yearly_retention_years)) {
            should_keep = true;
        }

        if (!should_keep && policy.auto_delete_expired) {
            log_info("Deleting expired backup set {}", backup_set.backup_set_uuid);
            delete_backup_set(backup_set);
        }
    }
}
```

### 8.2 Backup Scheduling

```sql
-- Create backup schedule
CREATE BACKUP SCHEDULE daily_full_backup
  FREQUENCY = DAILY AT '02:00 UTC'
  TYPE = FULL
  CLUSTER_CONSISTENT = TRUE
  TARGET = backup_target_s3;

CREATE BACKUP SCHEDULE hourly_incremental_backup
  FREQUENCY = HOURLY
  TYPE = INCREMENTAL
  TARGET = backup_target_s3;
```

---

## 9. Operational Procedures

### 9.1 Manual Full Backup

```sql
-- Perform manual cluster-wide backup
BACKUP CLUSTER
  TO backup_target_s3
  WITH CLUSTER_CONSISTENT = TRUE,
       DESCRIPTION = 'Pre-upgrade backup';
```

### 9.2 Manual Restore

```sql
-- Restore cluster from backup set
RESTORE CLUSTER
  FROM BACKUP SET 'backup_set_uuid'
  AT backup_target_s3
  WITH GENERATE_NEW_CLUSTER_UUID = TRUE,
       GENERATE_NEW_CA = TRUE;
```

### 9.3 List Available Backups

```sql
-- List all backup sets
SELECT
    backup_set_uuid,
    backup_started_at,
    backup_completed_at,
    is_cluster_consistent,
    state,
    COUNT(shard_backups) as num_shards,
    SUM(compressed_size_bytes) as total_size_bytes
FROM sys.backup_sets
GROUP BY backup_set_uuid
ORDER BY backup_started_at DESC
LIMIT 10;
```

### 9.4 Verify Backup Integrity

```sql
-- Verify backup hashes
VERIFY BACKUP SET 'backup_set_uuid';
```

---

## 10. Security Considerations

### 10.1 Trust Boundary Enforcement

**Implementation**: Ensure restore never imports trust material.

```cpp
void restore_catalog_from_backup(const CatalogSnapshot& catalog) {
    // 1. Restore user tables
    for (const Table& table : catalog.tables) {
        if (table.schema_name != "sys") {  // User tables only
            import_table_schema(table);
        }
    }

    // 2. Restore security policies (RLS, CLS, masking)
    import_security_policies(catalog.security_policies);

    // 3. Restore user accounts and grants
    import_user_accounts(catalog.users);
    import_grants(catalog.grants);

    // 4. NEVER restore trust material
    //    - Certificates
    //    - Private keys
    //    - CA bundles
    //    These are REGENERATED post-restore

    if (catalog.contains_certificates() || catalog.contains_private_keys()) {
        throw SecurityViolationException(
            "Backup contains trust material - this should never happen!");
    }
}
```

### 10.2 Backup Access Control

**Who can perform backups?**
- Cluster administrators only
- Requires `BACKUP` privilege

**Who can restore backups?**
- Cluster administrators only
- Requires `RESTORE` privilege
- Restore is destructive - replaces entire cluster

### 10.3 Backup Audit Logging

```cpp
void log_backup_audit_event(const BackupSet& backup_set, const UUID& admin_uuid) {
    AuditRecord record;
    record.event_type = AuditEventType::BACKUP_CREATED;
    record.backup_set_uuid = backup_set.backup_set_uuid;
    record.admin_uuid = admin_uuid;
    record.is_cluster_consistent = backup_set.is_cluster_consistent;
    record.num_shards = backup_set.shard_backups.size();
    record.total_size_bytes = compute_total_backup_size(backup_set);
    record.timestamp = now();

    audit_log.write(record);
}
```

---

## 11. Testing Procedures

### 11.1 Functional Tests

**Test: Full Backup and Restore**
```cpp
TEST(Backup, FullBackupAndRestore) {
    auto cluster = create_test_cluster(3, 16);

    // Insert test data
    insert_test_data(cluster, 10000);

    // Perform full backup
    BackupSet backup_set = cluster.backup_cluster(backup_target);

    ASSERT_EQ(backup_set.state, BackupSetState::COMPLETED);
    ASSERT_EQ(backup_set.shard_backups.size(), 16);

    // Destroy cluster
    cluster.destroy();

    // Restore from backup
    auto restored_cluster = restore_cluster_from_backup(backup_set, backup_target);

    // Verify data
    uint64_t row_count = restored_cluster.execute_query("SELECT COUNT(*) FROM test_table");
    EXPECT_EQ(row_count, 10000);
}
```

**Test: PITR**
```cpp
TEST(Backup, PointInTimeRecovery) {
    auto cluster = create_test_cluster(3, 16);

    // T0: Insert 5000 rows
    insert_test_data(cluster, 5000);
    timestamp_t t0 = now();

    // T1: Full backup
    BackupSet full_backup = cluster.backup_cluster(backup_target);

    // T2: Insert 5000 more rows
    insert_test_data(cluster, 5000);
    timestamp_t t2 = now();

    // T3: Incremental backup
    BackupSet inc_backup = cluster.backup_cluster_incremental(backup_target);

    // T4: Delete all rows
    cluster.execute_query("DELETE FROM test_table");

    // Restore to T2 (should have 10000 rows)
    auto restored_cluster = restore_cluster_to_pitr(full_backup, inc_backup, t2);

    uint64_t row_count = restored_cluster.execute_query("SELECT COUNT(*) FROM test_table");
    EXPECT_EQ(row_count, 10000);
}
```

### 11.2 Security Tests

**Test: Trust Material Not Backed Up**
```cpp
TEST(Backup, TrustMaterialNotBackedUp) {
    auto cluster = create_test_cluster(3, 16);

    // Perform backup
    BackupSet backup_set = cluster.backup_cluster(backup_target);

    // Inspect backup contents
    for (const ShardBackup& shard_backup : backup_set.shard_backups) {
        BackupContents contents = read_backup_contents(shard_backup);

        // Verify no certificates
        EXPECT_FALSE(contents.contains("certificates"));
        EXPECT_FALSE(contents.contains("private_keys"));
        EXPECT_FALSE(contents.contains("ca_bundle"));
    }
}
```

**Test: Restored Cluster Has New UUID**
```cpp
TEST(Backup, RestoredClusterNewUUID) {
    auto cluster = create_test_cluster(3, 16);
    UUID original_uuid = cluster.get_cluster_uuid();

    // Backup and restore
    BackupSet backup_set = cluster.backup_cluster(backup_target);
    cluster.destroy();

    auto restored_cluster = restore_cluster_from_backup(backup_set, backup_target);
    UUID restored_uuid = restored_cluster.get_cluster_uuid();

    // UUIDs should be different
    EXPECT_NE(original_uuid, restored_uuid);
}
```

---

## 12. Examples

### 12.1 Daily Automated Backup

```sql
-- Schedule daily full backup at 2 AM UTC
CREATE BACKUP SCHEDULE daily_backup
  FREQUENCY = DAILY AT '02:00 UTC'
  TYPE = FULL
  CLUSTER_CONSISTENT = TRUE
  TARGET = backup_target_s3
  RETENTION_POLICY = (
    DAILY_RETENTION = 7,
    WEEKLY_RETENTION = 4,
    MONTHLY_RETENTION = 12,
    YEARLY_RETENTION = 5
  );
```

### 12.2 Pre-Upgrade Backup

```sql
-- Manual backup before major upgrade
BACKUP CLUSTER
  TO backup_target_s3
  WITH CLUSTER_CONSISTENT = TRUE,
       DESCRIPTION = 'Pre-upgrade to v2.0 backup',
       TAG = 'before_v2_upgrade';
```

### 12.3 Restore After Disaster

```sql
-- List recent backups
SELECT backup_set_uuid, backup_completed_at, description
FROM sys.backup_sets
WHERE state = 'COMPLETED'
ORDER BY backup_completed_at DESC
LIMIT 5;

-- Restore from most recent cluster-consistent backup
RESTORE CLUSTER
  FROM BACKUP SET '01234567-89ab-cdef-0123-456789abcdef'
  AT backup_target_s3
  WITH GENERATE_NEW_CLUSTER_UUID = TRUE,
       GENERATE_NEW_CA = TRUE;
```

---

## 13. Performance Characteristics

### 13.1 Backup Performance

| Shard Size | Full Backup Time | Incremental Backup Time |
|------------|------------------|-------------------------|
| 10 GB      | ~5 minutes       | ~30 seconds             |
| 100 GB     | ~30 minutes      | ~2 minutes              |
| 1 TB       | ~3 hours         | ~10 minutes             |

**Assumptions**: Backup from replica to S3, 100 MB/s network bandwidth.

### 13.2 Restore Performance

| Cluster Size | Restore Time |
|--------------|--------------|
| 100 GB       | ~45 minutes  |
| 1 TB         | ~5 hours     |
| 10 TB        | ~2 days      |

**Assumptions**: Restore from S3, 100 MB/s network bandwidth, 16 shards restored in parallel.

---

## 14. References

### 14.1 Internal References
- **SBCLUSTER-00**: Guiding Principles
- **SBCLUSTER-02**: Membership and Identity
- **SBCLUSTER-05**: Sharding
- **SBCLUSTER-07**: Replication

### 14.2 External References
- **PostgreSQL Backup and Restore**: https://www.postgresql.org/docs/current/backup.html
- **MySQL Backup Strategies**: https://dev.mysql.com/doc/refman/8.0/en/backup-methods.html
- **AWS RDS Automated Backups**: https://docs.aws.amazon.com/AmazonRDS/latest/UserGuide/USER_WorkingWithAutomatedBackups.html

### 14.3 Research Papers
- *"Efficient Backup and Restore in Distributed Databases"* - Kumar et al., VLDB 2019

---

## 15. Revision History

| Version | Date       | Author       | Changes                                    |
|---------|------------|--------------|--------------------------------------------|
| 1.0     | 2026-01-02 | D. Calford   | Initial comprehensive specification        |

---

**Document Status**: DRAFT (Beta Specification Phase)
**Next Review**: Before Beta Implementation Phase
**Approval Required**: Chief Architect, Storage Engineering Lead, Security Lead
