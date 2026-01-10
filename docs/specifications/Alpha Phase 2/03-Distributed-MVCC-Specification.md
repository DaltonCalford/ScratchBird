# Distributed MVCC Specification

**Document Version:** 1.0  
**Date:** 2025-01-25  
**Status:** Draft Specification

---

## Overview

This document specifies the Multi-Version Concurrency Control (MVCC) system based on Firebird/Interbase multi-generational architecture, adapted for distributed operation using UUID v7 timestamping.

---

## Design Goals

1. **Global Ordering**: UUID v7 provides time-ordered, globally unique version identifiers
2. **No Coordination**: Transaction engines operate autonomously without distributed locks
3. **Provenance Tracking**: Every version tagged with origin (server_id, transaction_id)
4. **Deterministic Conflict Resolution**: Last-write-wins based on UUID v7 timestamps
5. **Efficient Garbage Collection**: Timestamp-based retention without global coordination

---

## Core Data Structures

### Row Version Record

```c
// Every row version in the system
struct RowVersion {
    // Version identifier (PRIMARY KEY)
    uuid_t row_uuid;                    // UUID v7 (time-ordered, globally unique)
    
    // Provenance
    uint16_t server_id;                 // Which TX engine created this
    uint64_t transaction_id;            // Local TID on that server
    
    // MVCC chain
    uuid_t prev_version_uuid;           // Previous version (NULL if first)
    
    // Table and row identification
    uint32_t table_id;
    uint64_t row_key;                   // Primary key or hash
    
    // Data
    uint32_t data_length;
    byte* row_data;                     // Actual row contents
    
    // Transaction state
    bool committed;                     // Has txn committed?
    uint64_t commit_timestamp_ns;       // When committed (from UUID v7)
    
    // Schema versioning
    uint32_t schema_version;            // For schema evolution
} __attribute__((packed));
```

### UUID v7 Format

```
UUID v7 (RFC 4122 Draft):

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           unix_ts_ms                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|          unix_ts_ms           |  ver  |       rand_a          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|var|                        rand_b                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                            rand_b                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

Fields:
- unix_ts_ms: 48 bits - Unix timestamp in milliseconds
- ver: 4 bits - Version (0111 = 7)
- rand_a: 12 bits - Random data (or sequence counter)
- var: 2 bits - Variant (10)
- rand_b: 62 bits - Random data (or server_id + random)

Total: 128 bits (16 bytes)
```

### UUID v7 Generation

```c
// Generate UUID v7 with cluster time and server_id
uuid_t uuid_v7_generate_cluster() {
    ClusterTime ct = get_cluster_time();
    
    // Convert nanoseconds to milliseconds
    uint64_t ts_ms = ct.timestamp_ns / 1000000;
    
    // Monotonic sequence for same-millisecond UUIDs
    static __thread uint64_t last_ts_ms = 0;
    static __thread uint16_t seq = 0;
    
    if (ts_ms == last_ts_ms) {
        seq++;
        if (seq == 0) {  // Overflow (4096 UUIDs in 1ms)
            // Rare: wait or bump to next millisecond
            ts_ms++;
            seq = 0;
        }
    } else if (ts_ms < last_ts_ms) {
        // Clock moved backward (should not happen with sync)
        log_critical("Clock moved backward! ts_ms=%lu, last=%lu", ts_ms, last_ts_ms);
        ts_ms = last_ts_ms + 1;  // Preserve monotonicity
        seq = 0;
    } else {
        seq = 0;
    }
    
    last_ts_ms = ts_ms;
    
    // Build UUID v7
    uuid_t uuid;
    
    // 48 bits of timestamp
    uuid.time_low = (uint32_t)(ts_ms & 0xFFFFFFFF);
    uuid.time_mid = (uint16_t)((ts_ms >> 32) & 0xFFFF);
    uuid.time_hi_and_version = (uint16_t)(((ts_ms >> 48) & 0x0FFF) | 0x7000);
    
    // Variant + sequence counter (12 bits)
    uuid.clock_seq_hi_and_reserved = (uint8_t)(0x80 | ((seq >> 6) & 0x3F));
    uuid.clock_seq_low = (uint8_t)(seq & 0x3F);
    
    // Node: embed server_id (16 bits) + random (48 bits)
    uint16_t server_id = get_server_id();
    uuid.node[0] = (server_id >> 8) & 0xFF;
    uuid.node[1] = server_id & 0xFF;
    uuid.node[2] = random_byte();
    uuid.node[3] = random_byte();
    uuid.node[4] = random_byte();
    uuid.node[5] = random_byte();
    
    return uuid;
}

// Extract timestamp from UUID v7
uint64_t uuid_v7_extract_timestamp_ms(uuid_t uuid) {
    uint64_t ts_ms = 0;
    
    ts_ms |= (uint64_t)(uuid.time_hi_and_version & 0x0FFF) << 48;
    ts_ms |= (uint64_t)uuid.time_mid << 32;
    ts_ms |= (uint64_t)uuid.time_low;
    
    return ts_ms;
}

uint64_t uuid_v7_extract_timestamp_ns(uuid_t uuid) {
    return uuid_v7_extract_timestamp_ms(uuid) * 1000000;
}

// Extract server_id from UUID v7
uint16_t uuid_v7_extract_server_id(uuid_t uuid) {
    return ((uint16_t)uuid.node[0] << 8) | uuid.node[1];
}

// Compare UUIDs for ordering
int uuid_compare(uuid_t a, uuid_t b) {
    // Compare timestamp first
    uint64_t ts_a = uuid_v7_extract_timestamp_ms(a);
    uint64_t ts_b = uuid_v7_extract_timestamp_ms(b);
    
    if (ts_a < ts_b) return -1;
    if (ts_a > ts_b) return 1;
    
    // Same millisecond: compare sequence (clock_seq)
    uint16_t seq_a = ((uint16_t)a.clock_seq_hi_and_reserved << 8) | a.clock_seq_low;
    uint16_t seq_b = ((uint16_t)b.clock_seq_hi_and_reserved << 8) | b.clock_seq_low;
    
    if (seq_a < seq_b) return -1;
    if (seq_a > seq_b) return 1;
    
    // Same timestamp and sequence: compare random bits
    return memcmp(&a, &b, sizeof(uuid_t));
}
```

---

## Transaction Model

### Transaction Structure

```c
// Transaction in progress
struct Transaction {
    // Identity
    uint64_t local_tid;                 // Monotonic on this server
    uint16_t server_id;
    
    // Timing
    uint64_t start_timestamp_ns;
    uint64_t commit_timestamp_ns;
    
    // Isolation level
    enum IsolationLevel {
        ISOLATION_READ_UNCOMMITTED = 0,
        ISOLATION_READ_COMMITTED = 1,
        ISOLATION_REPEATABLE_READ = 2,
        ISOLATION_SERIALIZABLE = 3,
        ISOLATION_SNAPSHOT = 4,
    } isolation_level;
    
    // Snapshot timestamp for MVCC
    uint64_t snapshot_timestamp_ns;
    
    // Changes made in this transaction
    int num_changes;
    RowVersion* changes[MAX_CHANGES_PER_TXN];
    
    // Transaction state
    enum TxnState {
        TXN_ACTIVE = 0,
        TXN_PREPARING = 1,
        TXN_COMMITTED = 2,
        TXN_ABORTED = 3,
    } state;
};
```

### Transaction Lifecycle

```c
// Begin transaction
Transaction* begin_transaction(IsolationLevel level) {
    Transaction* txn = alloc_transaction();
    
    txn->local_tid = get_next_local_tid();  // Atomic increment
    txn->server_id = get_server_id();
    txn->isolation_level = level;
    txn->start_timestamp_ns = get_cluster_time().timestamp_ns;
    
    // For snapshot isolation, capture snapshot timestamp
    if (level == ISOLATION_SNAPSHOT || level == ISOLATION_SERIALIZABLE) {
        ClusterTime ct = get_cluster_time();
        txn->snapshot_timestamp_ns = ct.timestamp_ns;
        
        // Optional: wait for uncertainty to pass
        if (STRICT_SNAPSHOT_MODE) {
            nanosleep(ct.uncertainty_ns);
        }
    }
    
    txn->state = TXN_ACTIVE;
    
    // Register in active transaction list
    register_active_transaction(txn);
    
    return txn;
}

// Commit transaction
int commit_transaction(Transaction* txn, ConsistencyMode mode) {
    if (txn->state != TXN_ACTIVE) {
        return ERROR_INVALID_TXN_STATE;
    }
    
    ClusterTime ct = get_cluster_time();
    txn->commit_timestamp_ns = ct.timestamp_ns;
    
    // Generate UUID v7 for each change
    for (int i = 0; i < txn->num_changes; i++) {
        RowVersion* version = txn->changes[i];
        version->row_uuid = uuid_v7_generate_cluster();
        version->server_id = txn->server_id;
        version->transaction_id = txn->local_tid;
        version->committed = true;
        version->commit_timestamp_ns = txn->commit_timestamp_ns;
    }
    
    // Commit-wait based on consistency mode
    uint32_t wait_ns = calculate_commit_wait(mode, ct.uncertainty_ns);
    if (wait_ns > 0) {
        nanosleep(wait_ns);
    }
    
    // Apply changes to storage
    for (int i = 0; i < txn->num_changes; i++) {
        storage_insert_version(txn->changes[i]);
        
        // Queue for replication
        replication_queue_push(txn->changes[i]);
    }
    
    txn->state = TXN_COMMITTED;
    
    // Remove from active transaction list
    unregister_active_transaction(txn);
    
    return COMMIT_OK;
}

// Rollback transaction
int rollback_transaction(Transaction* txn) {
    if (txn->state != TXN_ACTIVE) {
        return ERROR_INVALID_TXN_STATE;
    }
    
    // Discard all changes
    for (int i = 0; i < txn->num_changes; i++) {
        free_row_version(txn->changes[i]);
    }
    
    txn->state = TXN_ABORTED;
    unregister_active_transaction(txn);
    
    return ROLLBACK_OK;
}

// Calculate commit-wait duration
uint32_t calculate_commit_wait(ConsistencyMode mode, uint32_t uncertainty_ns) {
    switch (mode) {
        case CONSISTENCY_EVENTUAL:
            return 0;  // No wait
            
        case CONSISTENCY_BOUNDED_STALENESS:
            return uncertainty_ns;  // 1x
            
        case CONSISTENCY_MONOTONIC_READS:
            return uncertainty_ns * 2;  // 2x
            
        case CONSISTENCY_STRICT_SERIALIZABLE:
            return uncertainty_ns * 3;  // 3x
            
        default:
            return 0;
    }
}
```

---

## MVCC Visibility Rules

### Snapshot Isolation

```c
// Check if a version is visible to a transaction
bool is_version_visible(RowVersion* version, Transaction* txn) {
    // Must be committed
    if (!version->committed) {
        // Uncommitted versions only visible to same transaction
        return (version->server_id == txn->server_id &&
                version->transaction_id == txn->local_tid);
    }
    
    // Check snapshot timestamp
    uint64_t version_ts = uuid_v7_extract_timestamp_ns(version->row_uuid);
    
    switch (txn->isolation_level) {
        case ISOLATION_READ_UNCOMMITTED:
            // See everything (even uncommitted)
            return true;
            
        case ISOLATION_READ_COMMITTED:
            // See all committed versions up to now
            return version_ts <= get_cluster_time().timestamp_ns;
            
        case ISOLATION_REPEATABLE_READ:
        case ISOLATION_SNAPSHOT:
        case ISOLATION_SERIALIZABLE:
            // See committed versions at snapshot time
            return version_ts <= txn->snapshot_timestamp_ns;
            
        default:
            return false;
    }
}

// Find visible version of a row
RowVersion* read_row_version(uint32_t table_id, uint64_t row_key, 
                             Transaction* txn) {
    // Get all versions of this row
    List<RowVersion*> versions = get_row_versions(table_id, row_key);
    
    // Sort by UUID v7 (descending = newest first)
    qsort(versions.data, versions.count, sizeof(RowVersion*), 
          uuid_compare_desc);
    
    // Find first visible version
    for (int i = 0; i < versions.count; i++) {
        RowVersion* v = versions.data[i];
        
        if (is_version_visible(v, txn)) {
            return v;
        }
    }
    
    // No visible version (row doesn't exist in this snapshot)
    return NULL;
}
```

### Write Visibility and Conflicts

```c
// Write a new version
int write_row_version(uint32_t table_id, uint64_t row_key,
                     byte* data, uint32_t data_len,
                     Transaction* txn) {
    // Find current version
    RowVersion* current = read_row_version(table_id, row_key, txn);
    
    // Create new version
    RowVersion* new_version = alloc_row_version();
    new_version->table_id = table_id;
    new_version->row_key = row_key;
    new_version->row_data = malloc(data_len);
    memcpy(new_version->row_data, data, data_len);
    new_version->data_length = data_len;
    new_version->server_id = txn->server_id;
    new_version->transaction_id = txn->local_tid;
    new_version->committed = false;  // Not committed yet
    new_version->schema_version = get_current_schema_version(table_id);
    
    // Link to previous version
    if (current) {
        new_version->prev_version_uuid = current->row_uuid;
    } else {
        // First version
        memset(&new_version->prev_version_uuid, 0, sizeof(uuid_t));
    }
    
    // Add to transaction's change list
    txn->changes[txn->num_changes++] = new_version;
    
    return WRITE_OK;
}

// Delete a row (creates tombstone version)
int delete_row_version(uint32_t table_id, uint64_t row_key,
                      Transaction* txn) {
    // Find current version
    RowVersion* current = read_row_version(table_id, row_key, txn);
    
    if (!current) {
        return ERROR_ROW_NOT_FOUND;
    }
    
    // Create tombstone version (NULL data)
    RowVersion* tombstone = alloc_row_version();
    tombstone->table_id = table_id;
    tombstone->row_key = row_key;
    tombstone->row_data = NULL;
    tombstone->data_length = 0;
    tombstone->server_id = txn->server_id;
    tombstone->transaction_id = txn->local_tid;
    tombstone->committed = false;
    tombstone->prev_version_uuid = current->row_uuid;
    
    txn->changes[txn->num_changes++] = tombstone;
    
    return DELETE_OK;
}
```

---

## Storage Layer

### Index Structures

```c
// Primary index: UUID v7 → RowVersion*
// B+ tree with UUID ordering
typedef struct UUIDIndex {
    BTreeNode* root;
    uint64_t num_versions;
    
    // Cache for recent lookups
    LRUCache* lookup_cache;
} UUIDIndex;

// Secondary index: (table_id, row_key) → List<uuid_t>
// For finding all versions of a logical row
typedef struct RowKeyIndex {
    // Hash table: (table_id, row_key) → version list
    HashMap* table;
    
    struct RowVersionList {
        uint32_t table_id;
        uint64_t row_key;
        uint32_t count;
        uuid_t* uuids;  // Sorted by UUID (newest first)
    };
} RowKeyIndex;

// GC index: timestamp range → List<uuid_t>
// For efficient age-based garbage collection
typedef struct TimestampIndex {
    // Range tree: timestamp → version list
    RangeTree* tree;
    
    struct TimestampRange {
        uint64_t min_timestamp_ns;
        uint64_t max_timestamp_ns;
        uint32_t count;
        uuid_t* uuids;
    };
} TimestampIndex;
```

### Storage Operations

```c
// Insert version into storage
int storage_insert_version(RowVersion* version) {
    // 1. Insert into primary index
    uuid_index_insert(&primary_index, version->row_uuid, version);
    
    // 2. Update secondary index
    RowVersionList* list = row_key_index_get(&secondary_index,
                                             version->table_id,
                                             version->row_key);
    if (!list) {
        list = row_version_list_create(version->table_id, version->row_key);
        row_key_index_insert(&secondary_index, list);
    }
    
    // Insert UUID maintaining sort order (newest first)
    row_version_list_insert_sorted(list, version->row_uuid);
    
    // 3. Update timestamp index for GC
    uint64_t ts = uuid_v7_extract_timestamp_ns(version->row_uuid);
    timestamp_index_insert(&gc_index, ts, version->row_uuid);
    
    // 4. Persist version (data pages; optional write-after log stream)
    write_after_log_append(version);  // no-op until post-gold
    
    return STORAGE_OK;
}

// Retrieve version by UUID
RowVersion* storage_get_version(uuid_t row_uuid) {
    // Check cache first
    RowVersion* cached = lookup_cache_get(&primary_index.lookup_cache, row_uuid);
    if (cached) {
        return cached;
    }
    
    // Look up in B+ tree
    RowVersion* version = btree_lookup(primary_index.root, row_uuid);
    
    if (version) {
        // Cache for future lookups
        lookup_cache_insert(&primary_index.lookup_cache, row_uuid, version);
    }
    
    return version;
}

// Get all versions of a logical row
List<RowVersion*> get_row_versions(uint32_t table_id, uint64_t row_key) {
    RowVersionList* uuid_list = row_key_index_get(&secondary_index,
                                                   table_id, row_key);
    
    if (!uuid_list) {
        return empty_list();
    }
    
    List<RowVersion*> versions;
    versions.count = uuid_list->count;
    versions.data = malloc(sizeof(RowVersion*) * uuid_list->count);
    
    for (int i = 0; i < uuid_list->count; i++) {
        versions.data[i] = storage_get_version(uuid_list->uuids[i]);
    }
    
    return versions;
}
```

---

## Conflict Resolution

### Conflict Detection

```c
// Two versions conflict if they:
// 1. Modify the same logical row
// 2. Have different prev_version_uuid
// 3. Were created concurrently (within uncertainty window)

struct Conflict {
    RowVersion* version_a;
    RowVersion* version_b;
    ConflictType type;
};

enum ConflictType {
    CONFLICT_NONE = 0,              // No conflict
    CONFLICT_UPDATE_UPDATE = 1,     // Concurrent updates
    CONFLICT_UPDATE_DELETE = 2,     // Update vs delete
    CONFLICT_DELETE_DELETE = 3,     // Concurrent deletes (harmless)
};

// Detect conflicts
ConflictType detect_conflict(RowVersion* a, RowVersion* b) {
    // Must be same logical row
    if (a->table_id != b->table_id || a->row_key != b->row_key) {
        return CONFLICT_NONE;
    }
    
    // Check if they have different parents
    if (uuid_compare(a->prev_version_uuid, b->prev_version_uuid) != 0) {
        // Different parents = conflict
        
        bool a_is_delete = (a->data_length == 0);
        bool b_is_delete = (b->data_length == 0);
        
        if (a_is_delete && b_is_delete) {
            return CONFLICT_DELETE_DELETE;  // Harmless
        } else if (a_is_delete || b_is_delete) {
            return CONFLICT_UPDATE_DELETE;
        } else {
            return CONFLICT_UPDATE_UPDATE;
        }
    }
    
    return CONFLICT_NONE;
}
```

### Resolution Strategies

```c
// Conflict resolution strategy per table
struct ConflictResolutionStrategy {
    enum Strategy {
        STRATEGY_LAST_WRITE_WINS = 0,       // Use UUID v7 timestamp
        STRATEGY_FIRST_WRITE_WINS = 1,      // Reject later updates
        STRATEGY_CRDT_MERGE = 2,            // Application-specific merge
        STRATEGY_MANUAL_REVIEW = 3,         // Flag for human intervention
        STRATEGY_CUSTOM_FUNCTION = 4,       // User-defined function
    } strategy;
    
    // For custom resolution
    RowVersion* (*resolver_func)(RowVersion* a, RowVersion* b);
};

// Resolve conflict
RowVersion* resolve_conflict(RowVersion* a, RowVersion* b,
                             ConflictResolutionStrategy* strategy) {
    ConflictType conflict = detect_conflict(a, b);
    
    if (conflict == CONFLICT_NONE) {
        // No conflict, both are valid in their respective chains
        return NULL;  // Caller handles this
    }
    
    switch (strategy->strategy) {
        case STRATEGY_LAST_WRITE_WINS:
            return resolve_last_write_wins(a, b);
            
        case STRATEGY_FIRST_WRITE_WINS:
            return resolve_first_write_wins(a, b);
            
        case STRATEGY_CRDT_MERGE:
            return resolve_crdt_merge(a, b, strategy->resolver_func);
            
        case STRATEGY_MANUAL_REVIEW:
            return resolve_manual_review(a, b);
            
        case STRATEGY_CUSTOM_FUNCTION:
            return strategy->resolver_func(a, b);
            
        default:
            return resolve_last_write_wins(a, b);
    }
}

// Last-write-wins (most common)
RowVersion* resolve_last_write_wins(RowVersion* a, RowVersion* b) {
    int cmp = uuid_compare(a->row_uuid, b->row_uuid);
    
    if (cmp > 0) {
        // a is later
        log_info("Conflict resolved: version %s wins (LWW)",
                uuid_to_string(a->row_uuid));
        return a;
    } else {
        // b is later (or tie, which is extremely rare)
        log_info("Conflict resolved: version %s wins (LWW)",
                uuid_to_string(b->row_uuid));
        return b;
    }
}

// CRDT-style merge (for commutative updates)
RowVersion* resolve_crdt_merge(RowVersion* a, RowVersion* b,
                               MergeFunction merger) {
    // Example: account balance updates
    // Both versions added money, merge by adding both deltas
    
    RowVersion* merged = alloc_row_version();
    
    // Copy metadata from later version
    if (uuid_compare(a->row_uuid, b->row_uuid) > 0) {
        *merged = *a;
    } else {
        *merged = *b;
    }
    
    // Generate new UUID for merged version
    merged->row_uuid = uuid_v7_generate_cluster();
    
    // Application-specific merge logic
    merged->row_data = merger(a->row_data, b->row_data);
    
    // Link to both parents (for audit trail)
    // Store in custom field: merged_from_uuids[]
    
    log_info("Conflict resolved: merged %s and %s into %s",
            uuid_to_string(a->row_uuid),
            uuid_to_string(b->row_uuid),
            uuid_to_string(merged->row_uuid));
    
    return merged;
}
```

---

## Garbage Collection

### GC Strategy

```c
// Garbage collection based on timestamp retention
struct GCPolicy {
    uint64_t retention_ns;              // Keep versions this long
    uint32_t min_versions_per_row;      // Always keep N newest
    bool aggressive_gc;                 // GC aggressively vs conservatively
};

// Per-tier GC policies
GCPolicy tier_policies[] = {
    // TX Engine: aggressive, short retention
    {
        .retention_ns = 24 * 3600 * 1000000000ULL,  // 24 hours
        .min_versions_per_row = 1,
        .aggressive_gc = true,
    },
    
    // Ingestion Engine: moderate retention
    {
        .retention_ns = 7 * 24 * 3600 * 1000000000ULL,  // 7 days
        .min_versions_per_row = 2,
        .aggressive_gc = false,
    },
    
    // OLAP Engine: long retention
    {
        .retention_ns = 365 * 24 * 3600 * 1000000000ULL,  // 1 year
        .min_versions_per_row = 1,
        .aggressive_gc = false,
    },
};
```

### GC Safe Point Calculation

```c
// Calculate cluster-wide GC safe point
struct GCSafePoint {
    uint64_t safe_timestamp_ns;         // Nothing older needed
    uint64_t cluster_epoch;             // Consistency check
    uint32_t active_transactions;       // How many active txns
};

GCSafePoint calculate_gc_safe_point() {
    uint64_t min_active_snapshot = UINT64_MAX;
    uint32_t active_count = 0;
    
    // Query all active transactions across cluster
    for (int i = 0; i < cluster_node_count; i++) {
        NodeInfo* node = &cluster_nodes[i];
        
        // Get minimum snapshot timestamp from this node
        uint64_t node_min = query_node_min_snapshot(node->server_id);
        
        if (node_min < min_active_snapshot) {
            min_active_snapshot = node_min;
        }
        
        active_count += query_node_active_txn_count(node->server_id);
    }
    
    GCSafePoint safe_point;
    safe_point.safe_timestamp_ns = min_active_snapshot;
    safe_point.cluster_epoch = cluster_epoch;
    safe_point.active_transactions = active_count;
    
    return safe_point;
}
```

### GC Worker

```c
// Background GC worker thread
void* gc_worker_thread(void* arg) {
    GCPolicy* policy = (GCPolicy*)arg;
    
    while (running) {
        uint64_t start = get_monotonic_time_ms();
        
        // Calculate safe point
        GCSafePoint safe_point = calculate_gc_safe_point();
        
        // Determine GC threshold
        uint64_t now_ns = get_cluster_time().timestamp_ns;
        uint64_t gc_threshold = now_ns - policy->retention_ns;
        
        // Safe point overrides retention policy (never GC needed data)
        if (safe_point.safe_timestamp_ns < gc_threshold) {
            gc_threshold = safe_point.safe_timestamp_ns;
        }
        
        log_info("GC: safe_point=%lu, threshold=%lu, active_txns=%u",
                safe_point.safe_timestamp_ns / 1000000,  // ms
                gc_threshold / 1000000,
                safe_point.active_transactions);
        
        // Scan for old versions
        uint64_t versions_scanned = 0;
        uint64_t versions_deleted = 0;
        
        // Use timestamp index for efficient scanning
        List<uuid_t> old_uuids = timestamp_index_query(&gc_index, 0, gc_threshold);
        
        for (int i = 0; i < old_uuids.count; i++) {
            uuid_t uuid = old_uuids.data[i];
            versions_scanned++;
            
            // Can we delete this version?
            if (can_gc_version(uuid, policy, gc_threshold)) {
                storage_delete_version(uuid);
                versions_deleted++;
            }
        }
        
        uint64_t duration_ms = get_monotonic_time_ms() - start;
        
        log_info("GC completed: scanned=%lu, deleted=%lu, duration_ms=%lu",
                versions_scanned, versions_deleted, duration_ms);
        
        // Sleep until next GC cycle
        sleep(GC_INTERVAL_SECONDS);
    }
    
    return NULL;
}

// Check if a version can be garbage collected
bool can_gc_version(uuid_t row_uuid, GCPolicy* policy, uint64_t gc_threshold) {
    RowVersion* version = storage_get_version(row_uuid);
    
    if (!version) {
        return false;  // Already deleted
    }
    
    // Extract timestamp
    uint64_t version_ts = uuid_v7_extract_timestamp_ns(row_uuid);
    
    // Too new?
    if (version_ts >= gc_threshold) {
        return false;
    }
    
    // Get all versions of this row
    List<RowVersion*> versions = get_row_versions(version->table_id,
                                                  version->row_key);
    
    // Count versions newer than this one
    uint32_t newer_count = 0;
    for (int i = 0; i < versions.count; i++) {
        uint64_t ts = uuid_v7_extract_timestamp_ns(versions.data[i]->row_uuid);
        if (ts > version_ts) {
            newer_count++;
        }
    }
    
    // Keep minimum number of versions
    if (newer_count < policy->min_versions_per_row) {
        return false;
    }
    
    // Check if any active transaction might need this
    if (is_version_needed_by_active_txn(version)) {
        return false;
    }
    
    // Safe to delete
    return true;
}
```

---

## Server ID Management

### Server ID Allocation

```c
// Server ID registry (coordination service)
struct ServerIDRegistry {
    uint16_t max_server_id;             // 65535 max
    bool allocated[65536];              // Bitmap
    
    struct ServerInfo {
        uint16_t server_id;
        char hostname[256];
        uint64_t allocated_at_ns;
        uint64_t last_heartbeat_ns;
        bool active;
    } servers[65536];
};

// Allocate a server ID
uint16_t allocate_server_id(const char* hostname) {
    ServerIDRegistry* registry = get_global_registry();
    
    // Find first free ID
    for (uint16_t id = 1; id < 65536; id++) {
        if (!registry->allocated[id]) {
            // Allocate
            registry->allocated[id] = true;
            registry->servers[id].server_id = id;
            strncpy(registry->servers[id].hostname, hostname, 255);
            registry->servers[id].allocated_at_ns = get_cluster_time().timestamp_ns;
            registry->servers[id].active = true;
            
            log_info("Allocated server_id=%u to %s", id, hostname);
            
            return id;
        }
    }
    
    log_error("Server ID exhaustion! All 65535 IDs allocated");
    return 0;  // Error
}

// Release a server ID (with delay for safety)
void release_server_id(uint16_t server_id) {
    ServerIDRegistry* registry = get_global_registry();
    
    if (!registry->allocated[server_id]) {
        log_warn("Attempted to release unallocated server_id=%u", server_id);
        return;
    }
    
    // Mark as inactive (but don't immediately reuse)
    registry->servers[server_id].active = false;
    
    // Schedule for release after retention period
    schedule_delayed_release(server_id, RETENTION_PERIOD_NS);
    
    log_info("Released server_id=%u (delayed reuse)", server_id);
}

// Reuse after retention period
void delayed_release_server_id(uint16_t server_id) {
    ServerIDRegistry* registry = get_global_registry();
    
    // Check if enough time has passed
    uint64_t now = get_cluster_time().timestamp_ns;
    uint64_t allocated_at = registry->servers[server_id].allocated_at_ns;
    
    if (now - allocated_at < RETENTION_PERIOD_NS) {
        log_warn("Cannot reuse server_id=%u yet (retention period)", server_id);
        return;
    }
    
    // Clear allocation
    registry->allocated[server_id] = false;
    memset(&registry->servers[server_id], 0, sizeof(ServerInfo));
    
    log_info("Server_id=%u available for reuse", server_id);
}
```

---

## Performance Optimizations

### Version Chain Optimization

```c
// Optimize version chains for read performance
// Periodically compact chains by creating consolidated versions

void compact_version_chain(uint32_t table_id, uint64_t row_key) {
    List<RowVersion*> versions = get_row_versions(table_id, row_key);
    
    if (versions.count < VERSION_CHAIN_COMPACT_THRESHOLD) {
        return;  // Chain is short enough
    }
    
    // Find latest committed version
    RowVersion* latest = versions.data[0];
    
    // Create consolidated version with empty chain
    RowVersion* consolidated = clone_row_version(latest);
    consolidated->row_uuid = uuid_v7_generate_cluster();
    memset(&consolidated->prev_version_uuid, 0, sizeof(uuid_t));  // No parent
    
    // Insert consolidated version
    storage_insert_version(consolidated);
    
    // Mark old versions for aggressive GC
    for (int i = 0; i < versions.count; i++) {
        mark_version_for_gc(versions.data[i]->row_uuid);
    }
    
    log_info("Compacted version chain for table=%u, key=%lu (%d → 1 versions)",
            table_id, row_key, versions.count);
}
```

### Bloom Filters for Non-Existence

```c
// Use bloom filters to quickly determine if a row exists
struct RowBloomFilter {
    BloomFilter* filter;
    uint64_t version_count;
    uint64_t last_rebuild_ts;
};

// Check bloom filter before expensive lookup
RowVersion* read_row_version_optimized(uint32_t table_id, uint64_t row_key,
                                      Transaction* txn) {
    // Quick check: does this row exist at all?
    uint64_t hash = hash_table_row_key(table_id, row_key);
    
    if (!bloom_filter_contains(&row_bloom_filter, hash)) {
        // Definitely doesn't exist
        return NULL;
    }
    
    // Might exist, do full lookup
    return read_row_version(table_id, row_key, txn);
}
```

---

## Testing and Validation

### Consistency Checks

```c
// Validate MVCC invariants
void validate_mvcc_consistency() {
    log_info("Running MVCC consistency checks...");
    
    uint64_t errors = 0;
    
    // Check 1: All versions have valid UUIDs
    for (each version in storage) {
        if (!is_valid_uuid_v7(version->row_uuid)) {
            log_error("Invalid UUID v7: %s", uuid_to_string(version->row_uuid));
            errors++;
        }
    }
    
    // Check 2: Version chains are consistent
    for (each logical row) {
        List<RowVersion*> versions = get_row_versions(table_id, row_key);
        
        for (int i = 1; i < versions.count; i++) {
            RowVersion* newer = versions.data[i-1];
            RowVersion* older = versions.data[i];
            
            // Newer should point to older
            if (uuid_compare(newer->prev_version_uuid, older->row_uuid) != 0) {
                log_error("Broken version chain at %s", 
                         uuid_to_string(newer->row_uuid));
                errors++;
            }
        }
    }
    
    // Check 3: No duplicate UUIDs
    HashMap unique_check;
    for (each version in storage) {
        if (hashmap_contains(&unique_check, version->row_uuid)) {
            log_error("Duplicate UUID: %s", uuid_to_string(version->row_uuid));
            errors++;
        }
        hashmap_insert(&unique_check, version->row_uuid, 1);
    }
    
    if (errors == 0) {
        log_info("MVCC consistency check passed");
    } else {
        log_error("MVCC consistency check failed: %lu errors", errors);
    }
}
```

---

## Configuration

```yaml
mvcc:
  # UUID generation
  uuid:
    embed_server_id: true
    monotonic_sequence: true
    
  # Transaction settings
  transactions:
    default_isolation: snapshot
    max_changes_per_txn: 10000
    
  # Conflict resolution
  conflict_resolution:
    default_strategy: last_write_wins
    
    # Per-table strategies
    tables:
      account_balances:
        strategy: crdt_merge
        merger_function: sum_deltas
        
      user_sessions:
        strategy: last_write_wins
        
  # Garbage collection
  gc:
    tx_engine:
      retention_hours: 24
      min_versions: 1
      interval_seconds: 300
      
    ingestion_engine:
      retention_days: 7
      min_versions: 2
      interval_seconds: 3600
      
    olap_engine:
      retention_days: 365
      min_versions: 1
      interval_seconds: 86400
      
  # Server ID management
  server_id:
    retention_period_hours: 24
    max_servers: 1000
```

---

## Implementation Checklist

- [ ] UUID v7 generation with cluster time
- [ ] Server ID allocation/registry
- [ ] Transaction lifecycle (begin/commit/rollback)
- [ ] MVCC visibility rules
- [ ] Version storage and indexing
- [ ] Conflict detection
- [ ] Conflict resolution strategies
- [ ] GC safe point calculation
- [ ] GC worker threads
- [ ] Version chain compaction
- [ ] Bloom filter optimization
- [ ] Consistency validation
- [ ] Performance benchmarks

---

**Document Status**: Draft for Review  
**Next Review Date**: TBD  
**Approval Required**: Database Team
