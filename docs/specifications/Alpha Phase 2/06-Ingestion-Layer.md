# Ingestion Layer Specification

## Overview

The Ingestion Layer serves as the consolidation tier between ephemeral Transaction Engines and persistent OLAP storage. It receives change streams from multiple TX engines, deduplicates and merges versions, resolves conflicts, validates data, and pushes consolidated data to the OLAP tier.

## Architecture

### Ingestion Engine Components

```
┌────────────────────────────────────────────┐
│ Ingestion Engine                           │
│                                            │
│ ┌────────────────────────────────────┐    │
│ │ Change Stream Subscribers          │    │
│ │ (one per TX engine)                │    │
│ └──────────────┬─────────────────────┘    │
│                │                           │
│ ┌──────────────▼─────────────────────┐    │
│ │ Merge Queue (priority by UUID v7)  │    │
│ │ - Orders changes causally           │    │
│ └──────────────┬─────────────────────┘    │
│                │                           │
│ ┌──────────────▼─────────────────────┐    │
│ │ Conflict Resolver                   │    │
│ └──────────────┬─────────────────────┘    │
│                │                           │
│ ┌──────────────▼─────────────────────┐    │
│ │ Consolidated Storage (MVCC)        │    │
│ │ - Full history with versions        │    │
│ └──────────────┬─────────────────────┘    │
│                │                           │
│ ┌──────────────▼─────────────────────┐    │
│ │ OLAP Push Scheduler                │    │
│ │ - Batch builder                     │    │
│ │ - Compression                       │    │
│ └────────────────────────────────────┘    │
└────────────────────────────────────────────┘
```

## Change Stream Subscription

### Message Broker Integration

The Ingestion Engine subscribes to change events from all TX engines via a message broker (Kafka, NATS Streaming, Redis Streams).

```c
// Subscriber configuration
struct SubscriberConfig {
    char broker_url[256];
    char topic_prefix[128];      // e.g., "db-changes-"
    uint16_t num_tx_engines;
    uint16_t consumer_group_id;
    
    // Processing options
    uint32_t batch_size;         // How many messages to fetch at once
    uint32_t batch_timeout_ms;   // Max wait time for batch
    bool enable_compression;     // Decompress incoming messages
};

// Initialize subscribers for all TX engines
int init_change_subscribers(SubscriberConfig* config) {
    for (uint16_t server_id = 1; server_id <= config->num_tx_engines; server_id++) {
        char topic[256];
        snprintf(topic, sizeof(topic), "%s%u", 
                 config->topic_prefix, server_id);
        
        Subscriber* sub = create_subscriber(
            config->broker_url,
            topic,
            config->consumer_group_id
        );
        
        if (!sub) {
            log_error("Failed to subscribe to topic: %s", topic);
            return -1;
        }
        
        // Start worker thread for this subscription
        start_subscriber_thread(sub, process_change_batch);
        
        subscribers[server_id] = sub;
    }
    
    return 0;
}
```

### Change Event Processing

```c
// Process a batch of change events
void process_change_batch(ChangeEvent* events, size_t count) {
    for (size_t i = 0; i < count; i++) {
        ChangeEvent* event = &events[i];
        
        // 1. Check for duplicate
        if (uuid_index.contains(event->row_uuid)) {
            log_debug("Duplicate UUID detected: %s, skipping",
                     uuid_to_string(event->row_uuid));
            continue;
        }
        
        // 2. Add to merge queue (sorted by UUID v7 = time order)
        merge_queue_insert(event);
    }
    
    // 3. Process merge queue if batch is complete
    if (merge_queue.size >= MERGE_BATCH_SIZE) {
        process_merge_queue();
    }
}
```

## Merge Queue and Ordering

### Priority Queue by UUID v7

The merge queue maintains causal ordering by sorting on UUID v7, which embeds timestamp information.

```c
// Merge queue structure
struct MergeQueue {
    // Min-heap ordered by UUID v7 (earliest first)
    Heap<ChangeEvent*, UUIDComparator> heap;
    
    // Watermarks per TX engine (for coordination)
    uint64_t watermarks[MAX_TX_ENGINES];
    
    // Statistics
    uint64_t events_processed;
    uint64_t conflicts_detected;
    uint64_t duplicates_skipped;
};

// Comparator for UUID v7 ordering
int uuid_v7_compare(uuid_t a, uuid_t b) {
    // Extract timestamps from UUID v7
    uint64_t ts_a = uuid_v7_extract_timestamp(a);
    uint64_t ts_b = uuid_v7_extract_timestamp(b);
    
    if (ts_a < ts_b) return -1;
    if (ts_a > ts_b) return 1;
    
    // Timestamps equal, use full UUID comparison
    return memcmp(&a, &b, sizeof(uuid_t));
}

// Insert change event into merge queue
void merge_queue_insert(ChangeEvent* event) {
    // Update watermark for this server
    uint64_t event_ts = uuid_v7_extract_timestamp(event->row_uuid);
    if (event_ts > merge_queue.watermarks[event->server_id]) {
        merge_queue.watermarks[event->server_id] = event_ts;
    }
    
    // Insert into heap (maintains ordering)
    merge_queue.heap.push(event);
}
```

### Processing Merge Queue

```c
// Process events from merge queue
void process_merge_queue() {
    // Process events in UUID v7 order (chronological)
    while (!merge_queue.heap.empty()) {
        ChangeEvent* event = merge_queue.heap.top();
        merge_queue.heap.pop();
        
        // Check if we can safely process this event
        // (all earlier events from all servers have been received)
        if (!is_safe_to_process(event)) {
            // Put back in queue, wait for earlier events
            merge_queue.heap.push(event);
            break;
        }
        
        // Process the change
        process_change_event(event);
        merge_queue.events_processed++;
    }
}

// Determine if event is safe to process
bool is_safe_to_process(ChangeEvent* event) {
    uint64_t event_ts = uuid_v7_extract_timestamp(event->row_uuid);
    
    // Check watermarks from all TX engines
    for (uint16_t server_id = 1; server_id <= num_tx_engines; server_id++) {
        if (server_id == event->server_id) {
            continue;  // Skip originating server
        }
        
        // Have we received events from this server up to event_ts?
        if (merge_queue.watermarks[server_id] < event_ts) {
            // Not safe yet - might have earlier events coming
            return false;
        }
    }
    
    return true;
}
```

## Deduplication

### UUID-Based Deduplication

```c
// UUID index for fast duplicate detection
struct UUIDIndex {
    // Hash table: uuid_t → RowVersion*
    HashMap<uuid_t, RowVersion*> index;
    
    // Bloom filter for fast negative lookups
    BloomFilter bloom_filter;
    
    // Statistics
    uint64_t total_uuids;
    uint64_t false_positives;
};

// Check if UUID already exists
bool is_duplicate_uuid(uuid_t row_uuid) {
    // Fast negative check with Bloom filter
    if (!bloom_filter.contains(row_uuid)) {
        return false;  // Definitely not a duplicate
    }
    
    // Possible duplicate, check index
    bool exists = uuid_index.index.contains(row_uuid);
    
    if (!exists) {
        uuid_index.false_positives++;
    }
    
    return exists;
}

// Add UUID to index
void add_uuid_to_index(uuid_t row_uuid, RowVersion* version) {
    bloom_filter.add(row_uuid);
    uuid_index.index.insert(row_uuid, version);
    uuid_index.total_uuids++;
}
```

### Primary Key Deduplication

For finding all versions of a logical row:

```c
// Secondary index: (table_id, primary_key) → List<uuid_t>
struct RowVersionIndex {
    // Key structure
    struct RowKey {
        uint32_t table_id;
        bytes primary_key;
    };
    
    // Index: RowKey → List of UUID v7s (time-ordered)
    HashMap<RowKey, List<uuid_t>> index;
};

// Get all versions of a row
List<uuid_t> get_row_versions(uint32_t table_id, bytes primary_key) {
    RowKey key = {table_id, primary_key};
    return row_version_index.index.get(key);
}

// Add new version to row history
void add_row_version(uint32_t table_id, bytes primary_key, uuid_t row_uuid) {
    RowKey key = {table_id, primary_key};
    
    // Get existing versions
    List<uuid_t>& versions = row_version_index.index[key];
    
    // Insert in sorted position (UUID v7 order)
    auto insert_pos = std::lower_bound(
        versions.begin(),
        versions.end(),
        row_uuid,
        uuid_v7_compare
    );
    
    versions.insert(insert_pos, row_uuid);
}
```

## Conflict Resolution

### Conflict Detection

```c
// Detect conflicts: multiple updates to same row at similar times
struct Conflict {
    uint32_t table_id;
    bytes primary_key;
    List<RowVersion*> conflicting_versions;
    ConflictType type;
};

enum ConflictType {
    CONFLICT_CONCURRENT_UPDATE,    // Updates within uncertainty window
    CONFLICT_UPDATE_DELETE,        // Update and delete at same time
    CONFLICT_CONSTRAINT_VIOLATION, // Foreign key, unique constraint
};

// Check for conflicts when processing change
Conflict* detect_conflict(ChangeEvent* event) {
    // Get all recent versions of this row
    List<uuid_t> versions = get_row_versions(
        event->table_id,
        extract_primary_key(event)
    );
    
    if (versions.size() <= 1) {
        return NULL;  // No conflict - single version
    }
    
    // Check if any versions are concurrent (within uncertainty window)
    uint64_t event_ts = uuid_v7_extract_timestamp(event->row_uuid);
    
    for (uuid_t existing_uuid : versions) {
        uint64_t existing_ts = uuid_v7_extract_timestamp(existing_uuid);
        uint64_t delta = abs(event_ts - existing_ts);
        
        if (delta <= (MAX_CLOCK_UNCERTAINTY_NS * 2)) {
            // Concurrent update detected!
            return create_conflict(
                event,
                uuid_index.index.get(existing_uuid),
                CONFLICT_CONCURRENT_UPDATE
            );
        }
    }
    
    return NULL;
}
```

### Conflict Resolution Strategies

```c
// Per-table conflict resolution strategy
enum ConflictStrategy {
    STRATEGY_LAST_WRITE_WINS,     // Use UUID v7 timestamp (default)
    STRATEGY_FIRST_WRITE_WINS,    // Reject later updates
    STRATEGY_CRDT_MERGE,          // Application-specific merge
    STRATEGY_MANUAL_REVIEW,       // Flag for human resolution
    STRATEGY_CUSTOM_FUNCTION      // User-defined resolution logic
};

// Resolve conflict based on strategy
RowVersion* resolve_conflict(Conflict* conflict) {
    ConflictStrategy strategy = get_table_conflict_strategy(conflict->table_id);
    
    switch (strategy) {
        case STRATEGY_LAST_WRITE_WINS:
            return resolve_last_write_wins(conflict);
            
        case STRATEGY_FIRST_WRITE_WINS:
            return resolve_first_write_wins(conflict);
            
        case STRATEGY_CRDT_MERGE:
            return resolve_crdt_merge(conflict);
            
        case STRATEGY_MANUAL_REVIEW:
            return flag_for_manual_review(conflict);
            
        case STRATEGY_CUSTOM_FUNCTION:
            return call_custom_resolver(conflict);
    }
}
```

### Last-Write-Wins Resolution

```c
// Simple last-write-wins using UUID v7 ordering
RowVersion* resolve_last_write_wins(Conflict* conflict) {
    RowVersion* winner = NULL;
    uuid_t max_uuid = {0};
    
    for (RowVersion* version : conflict->conflicting_versions) {
        if (uuid_v7_compare(version->row_uuid, max_uuid) > 0) {
            max_uuid = version->row_uuid;
            winner = version;
        }
    }
    
    // Mark losing versions as superseded
    for (RowVersion* version : conflict->conflicting_versions) {
        if (version != winner) {
            version->superseded = true;
            version->superseded_by = winner->row_uuid;
        }
    }
    
    log_info("Conflict resolved by last-write-wins: winner=%s",
             uuid_to_string(winner->row_uuid));
    
    merge_queue.conflicts_detected++;
    
    return winner;
}
```

### CRDT-Style Merge Resolution

For specific data types that support commutative operations:

```c
// CRDT merge for counter/accumulator fields
RowVersion* resolve_crdt_merge(Conflict* conflict) {
    // Example: Account balance updates
    if (conflict->table_id == ACCOUNTS_TABLE) {
        return resolve_balance_conflict(conflict);
    }
    
    // Example: Set operations (add/remove items)
    if (conflict->table_id == TAGS_TABLE) {
        return resolve_set_conflict(conflict);
    }
    
    // Default to last-write-wins if no CRDT handler
    return resolve_last_write_wins(conflict);
}

// Resolve account balance conflicts by merging deltas
RowVersion* resolve_balance_conflict(Conflict* conflict) {
    // Get base version (before conflict)
    RowVersion* base = find_common_ancestor(conflict->conflicting_versions);
    int64_t base_balance = extract_balance(base->row_data);
    
    // Calculate deltas from each conflicting version
    int64_t total_delta = 0;
    
    for (RowVersion* version : conflict->conflicting_versions) {
        int64_t version_balance = extract_balance(version->row_data);
        int64_t delta = version_balance - base_balance;
        total_delta += delta;
    }
    
    // Create merged version
    int64_t merged_balance = base_balance + total_delta;
    
    RowVersion* merged = create_merged_version(
        uuid_v7_generate(),  // New UUID for merged result
        conflict->table_id,
        conflict->primary_key,
        update_balance(base->row_data, merged_balance)
    );
    
    // Link to all conflicting versions
    merged->merged_from = conflict->conflicting_versions;
    
    log_info("CRDT merge resolved: base=%ld, deltas=%ld, result=%ld",
             base_balance, total_delta, merged_balance);
    
    return merged;
}
```

## Data Validation

### Constraint Validation

```c
// Validate row before committing to consolidated storage
bool validate_row_version(RowVersion* version) {
    TableSchema* schema = get_table_schema(version->table_id);
    
    // 1. Check NOT NULL constraints
    if (!validate_not_null(version, schema)) {
        log_error("NOT NULL violation for table %u", version->table_id);
        return false;
    }
    
    // 2. Check data type constraints
    if (!validate_data_types(version, schema)) {
        log_error("Data type violation for table %u", version->table_id);
        return false;
    }
    
    // 3. Check CHECK constraints
    if (!validate_check_constraints(version, schema)) {
        log_error("CHECK constraint violation for table %u", version->table_id);
        return false;
    }
    
    // 4. Check foreign key constraints
    if (!validate_foreign_keys(version, schema)) {
        log_error("Foreign key violation for table %u", version->table_id);
        return false;
    }
    
    // 5. Check unique constraints
    if (!validate_unique_constraints(version, schema)) {
        log_error("Unique constraint violation for table %u", version->table_id);
        return false;
    }
    
    return true;
}
```

### Foreign Key Validation

```c
// Validate foreign key references
bool validate_foreign_keys(RowVersion* version, TableSchema* schema) {
    for (ForeignKey* fk : schema->foreign_keys) {
        // Extract foreign key value
        bytes fk_value = extract_column_value(
            version->row_data,
            fk->column_index
        );
        
        if (is_null(fk_value)) {
            continue;  // NULL is allowed for nullable FK
        }
        
        // Check if referenced row exists
        bool exists = check_row_exists(
            fk->referenced_table_id,
            fk->referenced_column_index,
            fk_value,
            version->commit_timestamp_ms  // Check as of this time
        );
        
        if (!exists) {
            log_error("FK violation: table=%u, fk=%s, value=%s not found",
                     version->table_id,
                     fk->name,
                     bytes_to_hex(fk_value));
            return false;
        }
    }
    
    return true;
}
```

## Data Transformation

### Schema Evolution

```c
// Transform data between schema versions
RowVersion* transform_schema_version(
    RowVersion* version,
    uint32_t target_schema_version
) {
    uint32_t current_version = version->schema_version;
    
    if (current_version == target_schema_version) {
        return version;  // No transformation needed
    }
    
    // Apply migrations in sequence
    RowVersion* transformed = version;
    
    for (uint32_t v = current_version + 1; v <= target_schema_version; v++) {
        SchemaMigration* migration = get_migration(version->table_id, v);
        
        if (!migration) {
            log_error("Missing migration for table=%u, version=%u",
                     version->table_id, v);
            return NULL;
        }
        
        transformed = apply_migration(transformed, migration);
        
        if (!transformed) {
            log_error("Migration failed for table=%u, version=%u",
                     version->table_id, v);
            return NULL;
        }
    }
    
    return transformed;
}
```

### Data Enrichment

```c
// Enrich rows with computed/derived columns
RowVersion* enrich_row_data(RowVersion* version) {
    TableSchema* schema = get_table_schema(version->table_id);
    
    for (ComputedColumn* col : schema->computed_columns) {
        // Evaluate computed expression
        bytes computed_value = evaluate_expression(
            col->expression,
            version->row_data
        );
        
        // Add to row data
        add_column_value(
            &version->row_data,
            col->column_index,
            computed_value
        );
    }
    
    return version;
}
```

## Consolidated Storage (MVCC)

### Storage Structure

The Ingestion Engine maintains full MVCC history with all versions:

```c
// Storage layout
struct ConsolidatedStorage {
    // Primary storage: UUID → RowVersion
    BTreeIndex<uuid_t, RowVersion*> version_store;
    
    // Secondary index: (table_id, pk) → List<uuid_t>
    RowVersionIndex row_versions;
    
    // Secondary index: timestamp → List<uuid_t>
    TimeIndex<uint64_t, uuid_t> time_index;
    
    // Metadata
    uint64_t total_versions;
    uint64_t total_tables;
    uint64_t oldest_version_ts;
    uint64_t newest_version_ts;
};
```

### Snapshot Reads

```c
// Read row as of specific timestamp
RowVersion* read_at_snapshot(
    uint32_t table_id,
    bytes primary_key,
    uint64_t snapshot_timestamp
) {
    // Get all versions of this row
    List<uuid_t> versions = get_row_versions(table_id, primary_key);
    
    // Find latest version visible at snapshot_timestamp
    RowVersion* visible = NULL;
    
    for (uuid_t version_uuid : versions) {
        RowVersion* v = version_store.get(version_uuid);
        
        if (!v->committed) {
            continue;  // Uncommitted version not visible
        }
        
        uint64_t version_ts = uuid_v7_extract_timestamp(version_uuid);
        
        if (version_ts <= snapshot_timestamp) {
            if (!visible || version_ts > uuid_v7_extract_timestamp(visible->row_uuid)) {
                visible = v;
            }
        }
    }
    
    return visible;
}
```

## Garbage Collection

### Retention-Based GC

```c
// GC configuration
struct GCConfig {
    uint64_t retention_period_ns;      // How long to keep versions
    uint32_t gc_interval_seconds;      // How often to run GC
    uint32_t versions_per_gc_cycle;    // Batch size
    bool preserve_latest_version;      // Always keep newest version
};

// Garbage collection worker
void gc_worker_thread(GCConfig* config) {
    while (running) {
        uint64_t now_ns = get_cluster_time().timestamp_ns;
        uint64_t gc_threshold = now_ns - config->retention_period_ns;
        
        // Find old versions
        List<uuid_t> candidates = time_index.range_query(
            0,
            gc_threshold
        );
        
        uint32_t deleted = 0;
        
        for (uuid_t version_uuid : candidates) {
            if (deleted >= config->versions_per_gc_cycle) {
                break;  // Batch limit reached
            }
            
            RowVersion* version = version_store.get(version_uuid);
            
            // Check if we can delete this version
            if (can_gc_version(version, config)) {
                delete_version(version_uuid);
                deleted++;
            }
        }
        
        log_info("GC cycle complete: deleted %u versions", deleted);
        
        sleep(config->gc_interval_seconds);
    }
}

// Determine if version can be garbage collected
bool can_gc_version(RowVersion* version, GCConfig* config) {
    // 1. Check if there's a newer version
    List<uuid_t> versions = get_row_versions(
        version->table_id,
        extract_primary_key(version)
    );
    
    bool has_newer = false;
    for (uuid_t other_uuid : versions) {
        if (uuid_v7_compare(other_uuid, version->row_uuid) > 0) {
            has_newer = true;
            break;
        }
    }
    
    if (!has_newer && config->preserve_latest_version) {
        return false;  // Keep latest version
    }
    
    // 2. Check if any active snapshot needs this version
    if (is_version_needed_by_snapshot(version)) {
        return false;
    }
    
    // 3. Version is old enough and not needed
    return true;
}
```

## OLAP Push

### Batch Building

```c
// Build batches for OLAP push
struct OLAPBatch {
    uint32_t table_id;
    List<RowVersion*> versions;
    uint64_t min_timestamp;
    uint64_t max_timestamp;
    size_t total_bytes;
    CompressionType compression;
};

// Push scheduler
void olap_push_scheduler() {
    while (running) {
        // Check if it's time to push
        if (!should_push_to_olap()) {
            sleep(PUSH_CHECK_INTERVAL_SECONDS);
            continue;
        }
        
        // Build batches per table
        Map<uint32_t, OLAPBatch*> batches = build_olap_batches();
        
        // Push each batch
        for (auto& [table_id, batch] : batches) {
            push_batch_to_olap(batch);
            mark_versions_pushed(batch->versions);
        }
        
        log_info("OLAP push complete: %lu tables, %lu versions",
                 batches.size(),
                 count_total_versions(batches));
    }
}

// Determine when to push to OLAP
bool should_push_to_olap() {
    // Strategy: Micro-batches for recent data, macro-batches for older
    
    // Count unpushed versions
    uint64_t unpushed_count = count_unpushed_versions();
    
    // Check age of oldest unpushed version
    uint64_t oldest_unpushed = get_oldest_unpushed_timestamp();
    uint64_t age_seconds = (now() - oldest_unpushed) / 1000000000;
    
    // Push if:
    // - Too many unpushed versions (>100k)
    // - Oldest unpushed is >5 minutes old
    return (unpushed_count > 100000) || (age_seconds > 300);
}
```

### Compression and Format

```c
// Compress batch for OLAP
bytes compress_olap_batch(OLAPBatch* batch) {
    // Convert to columnar format
    ColumnarBatch* columnar = convert_to_columnar(batch->versions);
    
    // Apply compression per column
    CompressedColumnarBatch* compressed = compress_columns(
        columnar,
        batch->compression
    );
    
    // Serialize to Parquet or custom format
    bytes serialized = serialize_to_parquet(compressed);
    
    log_info("Compressed batch: table=%u, rows=%lu, "
             "uncompressed=%lu bytes, compressed=%lu bytes (%.1fx)",
             batch->table_id,
             batch->versions.size(),
             batch->total_bytes,
             serialized.size(),
             (double)batch->total_bytes / serialized.size());
    
    return serialized;
}
```

## Performance Optimization

### Parallel Processing

```c
// Process multiple TX engine streams in parallel
struct ParallelProcessor {
    // Thread pool
    ThreadPool* workers;
    
    // Work queues per TX engine
    Queue<ChangeEvent*> queues[MAX_TX_ENGINES];
    
    // Shared merge queue (thread-safe)
    ThreadSafeMergeQueue merge_queue;
};

// Start parallel processing
void start_parallel_processing(ParallelProcessor* proc) {
    // One worker thread per TX engine stream
    for (uint16_t server_id = 1; server_id <= num_tx_engines; server_id++) {
        proc->workers->submit([server_id, proc]() {
            process_tx_engine_stream(server_id, proc);
        });
    }
}
```

### Batch Optimization

```c
// Optimize batch processing
void optimize_batch_processing() {
    // 1. Prefetch row versions likely to conflict
    prefetch_hot_rows();
    
    // 2. Sort events by table_id for locality
    sort_events_by_table();
    
    // 3. Group events by table for batch validation
    group_events_by_table();
    
    // 4. Parallelize independent table processing
    parallel_process_tables();
}
```

## Monitoring and Metrics

### Key Metrics

```c
struct IngestionMetrics {
    // Throughput
    uint64_t events_per_second;
    uint64_t versions_per_second;
    uint64_t bytes_per_second;
    
    // Queue depths
    uint32_t merge_queue_depth;
    uint32_t olap_push_queue_depth;
    
    // Latency
    uint64_t p50_processing_latency_us;
    uint64_t p99_processing_latency_us;
    
    // Conflicts
    uint64_t conflicts_detected;
    uint64_t conflicts_resolved;
    float conflict_rate;
    
    // Validation
    uint64_t validation_failures;
    
    // Deduplication
    uint64_t duplicates_skipped;
    float dedup_rate;
    
    // GC
    uint64_t versions_gc_eligible;
    uint64_t versions_gc_deleted;
    
    // OLAP push
    uint64_t batches_pushed;
    uint64_t push_lag_seconds;
};
```

### Dashboard Queries

```sql
-- Replication lag per TX engine
SELECT 
    server_id,
    MAX(event_timestamp) as latest_event,
    NOW() - MAX(event_timestamp) as lag_seconds
FROM change_events
GROUP BY server_id
ORDER BY lag_seconds DESC;

-- Conflict rate by table
SELECT 
    table_name,
    COUNT(*) as total_events,
    SUM(CASE WHEN conflict_detected THEN 1 ELSE 0 END) as conflicts,
    (SUM(CASE WHEN conflict_detected THEN 1 ELSE 0 END)::float / COUNT(*)) * 100 as conflict_rate_pct
FROM ingestion_events
WHERE timestamp > NOW() - INTERVAL '1 hour'
GROUP BY table_name
ORDER BY conflict_rate_pct DESC;

-- OLAP push effectiveness
SELECT 
    table_name,
    AVG(batch_size) as avg_batch_size,
    AVG(compression_ratio) as avg_compression,
    AVG(push_latency_ms) as avg_push_latency
FROM olap_push_log
WHERE timestamp > NOW() - INTERVAL '1 day'
GROUP BY table_name;
```

## Configuration Example

```yaml
ingestion:
  # Message broker
  broker:
    type: kafka
    brokers:
      - kafka1:9092
      - kafka2:9092
      - kafka3:9092
    topic_prefix: "db-changes-"
    consumer_group: "ingestion-group-1"
  
  # Processing
  processing:
    batch_size: 1000
    batch_timeout_ms: 100
    worker_threads: 16
    merge_queue_size: 100000
  
  # Conflict resolution
  conflicts:
    default_strategy: last_write_wins
    table_strategies:
      accounts: crdt_merge
      audit_log: first_write_wins
      
  # Validation
  validation:
    enable_foreign_keys: true
    enable_unique_constraints: true
    enable_check_constraints: true
  
  # Storage
  storage:
    retention_days: 7
    gc_interval_seconds: 3600
    versions_per_gc_cycle: 10000
  
  # OLAP push
  olap_push:
    strategy: micro_batches
    max_batch_size: 100000
    max_batch_age_seconds: 300
    compression: zstd
    parallelism: 4
