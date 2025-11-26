# Replication Protocol Specification

**Document Version:** 1.0  
**Date:** 2025-01-25  
**Status:** Draft Specification

---

## Overview

This document specifies the replication protocol for propagating changes from Transaction Engines to Ingestion Engines, and from Ingestion to OLAP tiers. The protocol is based on logical replication with change data capture (CDC).

---

## Design Goals

1. **Asynchronous**: Replication does not block transaction commits
2. **Idempotent**: Replay is safe, duplicate detection built-in
3. **Order-Independent**: Ingestion sorts by UUID v7, order of arrival doesn't matter
4. **Scalable**: Supports multiple TX engines replicating to multiple Ingestion engines
5. **Observable**: Clear metrics for lag, throughput, and conflicts

---

## Replication Architecture

### Data Flow

```
┌─────────────────────────────────────────────────────────┐
│  Transaction Engine                                     │
│  ┌───────────────────────────────────────────────────┐  │
│  │  Transaction Commit                               │  │
│  │  1. Apply to local storage                        │  │
│  │  2. Generate change events                        │  │
│  │  3. Push to replication queue                     │  │
│  │  4. Acknowledge to client ✓                       │  │
│  └────────────────────┬──────────────────────────────┘  │
│                       │                                  │
│  ┌────────────────────▼──────────────────────────────┐  │
│  │  Replication Queue (in-memory)                    │  │
│  │  - Bounded buffer                                 │  │
│  │  - Overflow to disk                               │  │
│  └────────────────────┬──────────────────────────────┘  │
│                       │                                  │
│  ┌────────────────────▼──────────────────────────────┐  │
│  │  Replication Worker Thread                        │  │
│  │  - Serialize changes                              │  │
│  │  - Publish to message broker                      │  │
│  │  - Update replicated watermark                    │  │
│  └────────────────────────────────────────────────────┘ │
└───────────────────────┬──────────────────────────────────┘
                        │
              Message Broker (Kafka/NATS)
              Topic: db.changes.<table_id>
                        │
        ┌───────────────┼───────────────┐
        │               │               │
┌───────▼──────┐ ┌──────▼──────┐ ┌─────▼──────┐
│ Ingestion    │ │ Ingestion   │ │ Other      │
│ Engine 1     │ │ Engine 2    │ │ Subscribers│
│              │ │             │ │ (Search,   │
│ Subscribe to │ │ Subscribe to│ │  Cache,    │
│ all topics   │ │ all topics  │ │  Analytics)│
└──────────────┘ └─────────────┘ └────────────┘
```

---

## Change Event Format

### Message Structure

```protobuf
// Change event published to message broker
message ChangeEvent {
    // Version identity
    bytes row_uuid = 1;                 // UUID v7 (16 bytes)
    uint32 server_id = 2;               // Source TX engine
    uint64 transaction_id = 3;          // Local TID on that server
    
    // MVCC chain
    bytes prev_version_uuid = 4;        // Previous version (16 bytes, NULL if first)
    
    // Table and row
    uint32 table_id = 5;
    uint64 row_key = 6;                 // Primary key or hash
    
    // Operation type
    oneof operation {
        InsertOperation insert = 7;
        UpdateOperation update = 8;
        DeleteOperation delete = 9;
    }
    
    // Timing
    uint64 commit_timestamp_ns = 10;    // When committed
    
    // Schema version
    uint32 schema_version = 11;
    
    // Consistency metadata
    ConsistencyMode consistency_mode = 12;
    uint32 uncertainty_ns = 13;         // Clock uncertainty at commit
}

message InsertOperation {
    bytes row_data = 1;                 // Full row data
    uint32 data_length = 2;
}

message UpdateOperation {
    bytes old_row_data = 1;             // Previous state (for conflict resolution)
    bytes new_row_data = 2;             // New state
    uint32 old_data_length = 3;
    uint32 new_data_length = 4;
    
    // Optional: column-level changes for efficiency
    repeated ColumnChange column_changes = 5;
}

message DeleteOperation {
    bytes row_data = 1;                 // Final state before delete (for audit)
    uint32 data_length = 2;
}

message ColumnChange {
    uint32 column_id = 1;
    bytes old_value = 2;
    bytes new_value = 3;
}

enum ConsistencyMode {
    CONSISTENCY_EVENTUAL = 0;
    CONSISTENCY_BOUNDED_STALENESS = 1;
    CONSISTENCY_MONOTONIC_READS = 2;
    CONSISTENCY_STRICT_SERIALIZABLE = 3;
}
```

### Serialization Format

```c
// Use MessagePack for compact binary encoding
// Alternative: Protocol Buffers (more structured, better versioning)

// Encode change event
bytes encode_change_event(ChangeEvent* event) {
    msgpack_sbuffer sbuf;
    msgpack_sbuffer_init(&sbuf);
    
    msgpack_packer packer;
    msgpack_packer_init(&packer, &sbuf, msgpack_sbuffer_write);
    
    // Pack fields
    msgpack_pack_map(&packer, 13);  // 13 fields
    
    // row_uuid
    msgpack_pack_str(&packer, 8);
    msgpack_pack_str_body(&packer, "row_uuid", 8);
    msgpack_pack_bin(&packer, 16);
    msgpack_pack_bin_body(&packer, &event->row_uuid, 16);
    
    // server_id
    msgpack_pack_str(&packer, 9);
    msgpack_pack_str_body(&packer, "server_id", 9);
    msgpack_pack_uint16(&packer, event->server_id);
    
    // ... pack remaining fields ...
    
    bytes result = {sbuf.data, sbuf.size};
    return result;
}

// Decode change event
ChangeEvent* decode_change_event(bytes data) {
    msgpack_unpacker unpacker;
    msgpack_unpacker_init(&unpacker, data.size);
    msgpack_unpacker_feed(&unpacker, data.ptr, data.size);
    
    msgpack_unpacked unpacked;
    msgpack_unpacked_init(&unpacked);
    
    if (msgpack_unpacker_next(&unpacker, &unpacked) != MSGPACK_UNPACK_SUCCESS) {
        return NULL;
    }
    
    ChangeEvent* event = alloc_change_event();
    
    // Unpack fields
    msgpack_object obj = unpacked.data;
    if (obj.type != MSGPACK_OBJECT_MAP) {
        return NULL;
    }
    
    // ... extract fields from map ...
    
    return event;
}
```

---

## Transaction Engine: Replication Producer

### Replication Queue

```c
// In-memory queue for pending change events
struct ReplicationQueue {
    // Ring buffer for efficiency
    ChangeEvent* events[QUEUE_SIZE];
    uint32_t head;          // Next write position
    uint32_t tail;          // Next read position
    uint32_t count;         // Number of events
    
    // Overflow handling
    FILE* overflow_file;    // Spill to disk if queue fills
    uint64_t overflow_count;
    
    // Synchronization
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    
    // Metrics
    uint64_t enqueued_total;
    uint64_t dequeued_total;
    uint64_t overflow_events;
};

// Push event to replication queue
int replication_queue_push(ChangeEvent* event) {
    pthread_mutex_lock(&replication_queue.lock);
    
    // Check if queue is full
    while (replication_queue.count >= QUEUE_SIZE) {
        // Spill to disk
        if (replication_queue.overflow_file == NULL) {
            replication_queue.overflow_file = 
                fopen("/var/db/replication_overflow.dat", "wb");
        }
        
        // Write oldest event to disk
        ChangeEvent* oldest = replication_queue.events[replication_queue.tail];
        fwrite(oldest, sizeof(ChangeEvent), 1, replication_queue.overflow_file);
        replication_queue.overflow_count++;
        
        // Remove from queue
        replication_queue.tail = (replication_queue.tail + 1) % QUEUE_SIZE;
        replication_queue.count--;
        
        log_warn("Replication queue overflow, spilled event to disk");
    }
    
    // Add to queue
    replication_queue.events[replication_queue.head] = event;
    replication_queue.head = (replication_queue.head + 1) % QUEUE_SIZE;
    replication_queue.count++;
    replication_queue.enqueued_total++;
    
    pthread_cond_signal(&replication_queue.not_empty);
    pthread_mutex_unlock(&replication_queue.lock);
    
    return QUEUE_OK;
}

// Pop event from replication queue
ChangeEvent* replication_queue_pop(int timeout_ms) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000;
    
    pthread_mutex_lock(&replication_queue.lock);
    
    // Wait for event
    while (replication_queue.count == 0) {
        int ret = pthread_cond_timedwait(&replication_queue.not_empty,
                                        &replication_queue.lock, &ts);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&replication_queue.lock);
            return NULL;
        }
    }
    
    // Pop from queue
    ChangeEvent* event = replication_queue.events[replication_queue.tail];
    replication_queue.tail = (replication_queue.tail + 1) % QUEUE_SIZE;
    replication_queue.count--;
    replication_queue.dequeued_total++;
    
    pthread_cond_signal(&replication_queue.not_full);
    pthread_mutex_unlock(&replication_queue.lock);
    
    return event;
}
```

### Replication Worker

```c
// Replication worker thread
void* replication_worker_thread(void* arg) {
    KafkaProducer* producer = kafka_producer_create(BROKER_LIST);
    
    while (running) {
        // Pop change event
        ChangeEvent* event = replication_queue_pop(100);  // 100ms timeout
        
        if (!event) {
            continue;  // Timeout, retry
        }
        
        // Serialize
        bytes data = encode_change_event(event);
        
        // Determine topic (partition by table_id)
        char topic[256];
        snprintf(topic, sizeof(topic), "db.changes.%u", event->table_id);
        
        // Publish to Kafka
        int ret = kafka_produce(producer, topic, data.ptr, data.size,
                               event->row_uuid.bytes, 16);  // Use UUID as key
        
        if (ret != KAFKA_OK) {
            log_error("Failed to publish change event: %s", kafka_error_string(ret));
            
            // Re-enqueue for retry
            replication_queue_push(event);
            
            // Backoff
            sleep_ms(1000);
            continue;
        }
        
        // Update watermark
        update_replicated_watermark(event->row_uuid);
        
        // Free event
        free_change_event(event);
        
        // Metrics
        replication_events_published++;
        replication_bytes_published += data.size;
    }
    
    kafka_producer_destroy(producer);
    return NULL;
}

// Track replication progress
struct ReplicationWatermark {
    uuid_t last_replicated_uuid;        // Latest replicated UUID
    uint64_t last_replicated_ts_ns;     // Timestamp of that UUID
    uint64_t events_replicated;         // Total events
    uint64_t bytes_replicated;          // Total bytes
};

void update_replicated_watermark(uuid_t row_uuid) {
    uint64_t ts = uuid_v7_extract_timestamp_ns(row_uuid);
    
    if (ts > watermark.last_replicated_ts_ns) {
        watermark.last_replicated_uuid = row_uuid;
        watermark.last_replicated_ts_ns = ts;
    }
    
    watermark.events_replicated++;
}
```

---

## Ingestion Engine: Replication Consumer

### Change Stream Subscriber

```c
// Subscribe to all table topics
void* ingestion_subscriber_thread(void* arg) {
    KafkaConsumer* consumer = kafka_consumer_create(BROKER_LIST, GROUP_ID);
    
    // Subscribe to all db.changes.* topics
    kafka_consumer_subscribe(consumer, "db.changes.*");
    
    while (running) {
        // Poll for messages
        KafkaMessage* msg = kafka_consumer_poll(consumer, 100);
        
        if (!msg) {
            continue;  // Timeout
        }
        
        // Decode change event
        ChangeEvent* event = decode_change_event((bytes){msg->payload, msg->len});
        
        if (!event) {
            log_error("Failed to decode change event");
            kafka_message_destroy(msg);
            continue;
        }
        
        // Process event
        process_change_event(event);
        
        // Commit offset
        kafka_consumer_commit(consumer);
        
        // Cleanup
        free_change_event(event);
        kafka_message_destroy(msg);
    }
    
    kafka_consumer_destroy(consumer);
    return NULL;
}
```

### Change Event Processing

```c
// Process incoming change event
void process_change_event(ChangeEvent* event) {
    // 1. Deduplication check
    if (uuid_index_contains(&ingestion_uuid_index, event->row_uuid)) {
        log_debug("Duplicate change event: %s", uuid_to_string(event->row_uuid));
        ingestion_duplicates_detected++;
        return;  // Already processed
    }
    
    // 2. Get existing versions for this row
    List<RowVersion*> existing = get_row_versions(event->table_id, event->row_key);
    
    // 3. Convert change event to row version
    RowVersion* new_version = change_event_to_row_version(event);
    
    // 4. Detect conflicts
    for (int i = 0; i < existing.count; i++) {
        ConflictType conflict = detect_conflict(new_version, existing.data[i]);
        
        if (conflict != CONFLICT_NONE) {
            log_info("Conflict detected: %s vs %s (type=%d)",
                    uuid_to_string(new_version->row_uuid),
                    uuid_to_string(existing.data[i]->row_uuid),
                    conflict);
            
            // Resolve conflict
            ConflictResolutionStrategy* strategy = 
                get_conflict_strategy(event->table_id);
            
            RowVersion* winner = resolve_conflict(new_version, existing.data[i],
                                                 strategy);
            
            if (winner != new_version) {
                // Current version lost conflict, discard
                free_row_version(new_version);
                ingestion_conflicts_resolved++;
                return;
            }
            
            // New version wins, continue processing
        }
    }
    
    // 5. Determine position in version chain
    insert_version_in_chain(new_version, existing);
    
    // 6. Store merged version
    storage_insert_version(new_version);
    uuid_index_insert(&ingestion_uuid_index, new_version->row_uuid);
    
    // 7. Update metrics
    ingestion_events_processed++;
    update_ingestion_watermark(new_version->row_uuid);
    
    log_debug("Processed change event: %s", uuid_to_string(new_version->row_uuid));
}

// Convert change event to row version
RowVersion* change_event_to_row_version(ChangeEvent* event) {
    RowVersion* version = alloc_row_version();
    
    version->row_uuid = event->row_uuid;
    version->server_id = event->server_id;
    version->transaction_id = event->transaction_id;
    version->prev_version_uuid = event->prev_version_uuid;
    version->table_id = event->table_id;
    version->row_key = event->row_key;
    version->committed = true;
    version->commit_timestamp_ns = event->commit_timestamp_ns;
    version->schema_version = event->schema_version;
    
    // Extract row data based on operation type
    switch (event->operation_type) {
        case OP_INSERT:
            version->data_length = event->insert.data_length;
            version->row_data = malloc(event->insert.data_length);
            memcpy(version->row_data, event->insert.row_data, 
                  event->insert.data_length);
            break;
            
        case OP_UPDATE:
            version->data_length = event->update.new_data_length;
            version->row_data = malloc(event->update.new_data_length);
            memcpy(version->row_data, event->update.new_row_data,
                  event->update.new_data_length);
            break;
            
        case OP_DELETE:
            // Tombstone (NULL data)
            version->data_length = 0;
            version->row_data = NULL;
            break;
    }
    
    return version;
}

// Insert version into existing chain
void insert_version_in_chain(RowVersion* new_version,
                             List<RowVersion*> existing) {
    // Find position based on UUID v7 ordering
    RowVersion* insert_after = NULL;
    RowVersion* insert_before = NULL;
    
    for (int i = 0; i < existing.count; i++) {
        RowVersion* v = existing.data[i];
        
        int cmp = uuid_compare(v->row_uuid, new_version->row_uuid);
        
        if (cmp < 0) {
            // v is earlier than new_version
            insert_after = v;
        } else if (cmp > 0) {
            // v is later than new_version
            insert_before = v;
            break;
        }
    }
    
    // Update chain links
    if (insert_after) {
        new_version->prev_version_uuid = insert_after->row_uuid;
    }
    
    if (insert_before) {
        // Update insert_before to point to new_version
        insert_before->prev_version_uuid = new_version->row_uuid;
        storage_update_version(insert_before);
    }
}
```

---

## Ingestion to OLAP: Batch Replication

### Batch Builder

```c
// Build batches for OLAP push
struct OLAPBatch {
    uint32_t table_id;
    uint64_t min_timestamp_ns;
    uint64_t max_timestamp_ns;
    uint32_t row_count;
    RowVersion** rows;
    
    // Columnar format
    ColumnarData* columns;
    
    // Compression
    CompressionCodec codec;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
};

// Batch building thread
void* olap_batch_builder_thread(void* arg) {
    while (running) {
        // Wait for batch interval
        sleep(OLAP_BATCH_INTERVAL_SECONDS);
        
        // For each table
        for (uint32_t table_id = 0; table_id < NUM_TABLES; table_id++) {
            OLAPBatch* batch = build_olap_batch(table_id);
            
            if (batch->row_count == 0) {
                continue;  // No new data
            }
            
            // Push to OLAP tier
            push_batch_to_olap(batch);
            
            // Update watermark
            update_olap_push_watermark(table_id, batch->max_timestamp_ns);
            
            free_olap_batch(batch);
        }
    }
    
    return NULL;
}

// Build batch for a table
OLAPBatch* build_olap_batch(uint32_t table_id) {
    OLAPBatch* batch = alloc_olap_batch();
    batch->table_id = table_id;
    
    // Get unpushed versions
    uint64_t last_pushed_ts = get_olap_push_watermark(table_id);
    List<RowVersion*> unpushed = get_versions_since(table_id, last_pushed_ts);
    
    if (unpushed.count == 0) {
        return batch;  // Empty batch
    }
    
    // Convert to columnar format
    batch->columns = convert_to_columnar(unpushed);
    batch->row_count = unpushed.count;
    
    // Extract timestamp range
    batch->min_timestamp_ns = uuid_v7_extract_timestamp_ns(
        unpushed.data[0]->row_uuid);
    batch->max_timestamp_ns = uuid_v7_extract_timestamp_ns(
        unpushed.data[unpushed.count - 1]->row_uuid);
    
    // Compress
    batch->codec = COMPRESSION_ZSTD;
    compress_batch(batch);
    
    log_info("Built OLAP batch: table=%u, rows=%u, size=%u bytes (%.1fx compression)",
            table_id, batch->row_count, batch->compressed_size,
            (float)batch->uncompressed_size / batch->compressed_size);
    
    return batch;
}

// Push batch to OLAP tier
void push_batch_to_olap(OLAPBatch* batch) {
    // Determine target shard
    uint32_t shard_id = route_to_olap_shard(batch->table_id,
                                           batch->min_timestamp_ns);
    
    OLAPShard* shard = get_olap_shard(shard_id);
    
    // Send batch
    int ret = olap_shard_ingest_batch(shard, batch);
    
    if (ret != INGEST_OK) {
        log_error("Failed to push batch to OLAP shard %u", shard_id);
        return;
    }
    
    log_info("Pushed batch to OLAP shard %u: %u rows",
            shard_id, batch->row_count);
}
```

### Columnar Conversion

```c
// Convert row-oriented data to columnar format
ColumnarData* convert_to_columnar(List<RowVersion*> rows) {
    ColumnarData* columnar = alloc_columnar_data();
    
    // Get table schema
    TableSchema* schema = get_table_schema(rows.data[0]->table_id);
    
    columnar->num_columns = schema->num_columns;
    columnar->columns = malloc(sizeof(ColumnData) * schema->num_columns);
    
    // For each column
    for (int col = 0; col < schema->num_columns; col++) {
        ColumnData* column = &columnar->columns[col];
        column->column_id = schema->columns[col].column_id;
        column->data_type = schema->columns[col].data_type;
        column->num_rows = rows.count;
        column->data = malloc(column->num_rows * sizeof_datatype(column->data_type));
        column->nulls = malloc(column->num_rows / 8 + 1);  // Bitmap
        
        // Extract column values from each row
        for (int row = 0; row < rows.count; row++) {
            extract_column_value(rows.data[row], col, column, row);
        }
        
        // Dictionary encoding for string columns
        if (column->data_type == TYPE_STRING) {
            dictionary_encode_column(column);
        }
    }
    
    return columnar;
}
```

---

## Replication Lag Monitoring

### Metrics

```c
// Replication lag metrics
struct ReplicationMetrics {
    // TX Engine → Ingestion lag
    uint64_t tx_to_ingestion_lag_ns;       // Time since last event
    uint64_t tx_to_ingestion_queue_depth;  // Events pending
    
    // Ingestion → OLAP lag
    uint64_t ingestion_to_olap_lag_ns;
    uint32_t unpushed_row_count;
    
    // Throughput
    uint64_t events_per_second;
    uint64_t bytes_per_second;
    
    // Conflicts
    uint64_t conflicts_detected;
    uint64_t conflicts_resolved;
};

// Calculate replication lag
void calculate_replication_lag() {
    ClusterTime now = get_cluster_time();
    
    // TX → Ingestion lag
    uint64_t last_tx_event_ts = watermark.last_replicated_ts_ns;
    metrics.tx_to_ingestion_lag_ns = now.timestamp_ns - last_tx_event_ts;
    metrics.tx_to_ingestion_queue_depth = replication_queue.count;
    
    // Ingestion → OLAP lag
    uint64_t last_olap_push_ts = get_olap_push_watermark_max();
    metrics.ingestion_to_olap_lag_ns = now.timestamp_ns - last_olap_push_ts;
    metrics.unpushed_row_count = count_unpushed_rows();
    
    // Log if lag is high
    if (metrics.tx_to_ingestion_lag_ns > WARN_LAG_NS) {
        log_warn("High replication lag: TX→Ingestion = %.2f seconds",
                metrics.tx_to_ingestion_lag_ns / 1e9);
    }
}
```

### Dashboard Queries

```sql
-- Replication lag overview
SELECT 
    server_id,
    tx_to_ingestion_lag_ms,
    ingestion_to_olap_lag_ms,
    queue_depth,
    events_per_sec
FROM replication_metrics
ORDER BY tx_to_ingestion_lag_ms DESC;

-- Conflict resolution stats
SELECT 
    table_name,
    conflicts_detected,
    conflicts_resolved,
    conflict_rate_pct
FROM conflict_metrics
WHERE timestamp > NOW() - INTERVAL '1 hour'
ORDER BY conflicts_detected DESC;

-- Replication throughput
SELECT 
    DATE_TRUNC('minute', timestamp) as minute,
    SUM(events_replicated) as events,
    SUM(bytes_replicated) / 1024 / 1024 as mb
FROM replication_throughput
GROUP BY minute
ORDER BY minute DESC
LIMIT 60;
```

---

## Failure Handling

### Replication Failure Scenarios

**Scenario 1: Message Broker Unavailable**

```c
// TX Engine side
void handle_broker_unavailable() {
    // Continue queuing changes in memory
    log_warn("Message broker unavailable, buffering locally");
    
    // If queue fills, spill to disk
    // Commits continue succeeding
    // Eventually queue drains when broker recovers
}
```

**Scenario 2: Ingestion Engine Behind**

```c
// Detect slow consumer
void detect_slow_consumer() {
    uint64_t lag_ns = metrics.tx_to_ingestion_lag_ns;
    
    if (lag_ns > CRITICAL_LAG_NS) {
        log_critical("Ingestion severely lagging: %.2f seconds",
                    lag_ns / 1e9);
        
        // Alert ops
        send_alert(ALERT_REPLICATION_LAG, lag_ns);
        
        // Consider scaling up Ingestion capacity
        request_scale_up(TIER_INGESTION);
    }
}
```

**Scenario 3: Network Partition**

```c
// TX Engine partitioned from message broker
void handle_network_partition() {
    // Buffer changes locally (disk spill)
    // Set health check to degraded
    set_node_health(HEALTH_DEGRADED);
    
    // When partition heals, replay buffer
    // Message broker deduplicates by UUID
}
```

---

## Configuration

```yaml
replication:
  # Message broker
  broker:
    type: kafka  # kafka, nats, redis_streams
    brokers:
      - kafka1.internal:9092
      - kafka2.internal:9092
      - kafka3.internal:9092
    
    # Topics
    topic_prefix: db.changes
    num_partitions: 32
    replication_factor: 3
    
  # TX Engine producer
  producer:
    queue_size: 10000
    overflow_to_disk: true
    overflow_path: /var/db/replication_overflow.dat
    batch_size: 100
    linger_ms: 10
    compression: zstd
    
  # Ingestion consumer
  consumer:
    group_id: ingestion-engines
    poll_timeout_ms: 100
    max_poll_records: 500
    enable_auto_commit: false
    
  # OLAP push
  olap_push:
    batch_interval_seconds: 300  # 5 minutes
    min_batch_size: 1000
    max_batch_size: 100000
    compression: zstd
    compression_level: 3
    
  # Monitoring
  monitoring:
    lag_warn_threshold_ms: 5000
    lag_critical_threshold_ms: 30000
    alert_on_lag: true
```

---

## Implementation Checklist

- [ ] Change event protobuf/msgpack schema
- [ ] TX Engine replication queue
- [ ] TX Engine replication worker
- [ ] Kafka/NATS producer integration
- [ ] Ingestion consumer threads
- [ ] Change event processing
- [ ] Conflict detection and resolution
- [ ] Version chain insertion logic
- [ ] OLAP batch builder
- [ ] Columnar conversion
- [ ] Compression (zstd)
- [ ] Replication lag monitoring
- [ ] Failure handling (broker down, partition)
- [ ] Performance benchmarks

---

**Document Status**: Draft for Review  
**Next Review Date**: TBD  
**Approval Required**: Infrastructure Team
