# ScratchBird MGA (Multi-Generational Architecture) Implementation

## Overview

ScratchBird adopts and enhances Firebird's MGA with key improvements:
- **UUID-based object references** instead of page/slot numbers
- **64-bit transaction IDs** from the start (no wraparound issues)
- **Enhanced TIP structure** supporting 5 page sizes
- **Distributed transaction support** (future)
- **Integrated with shadow database replication**

## Transaction ID Generation

### Enhanced Transaction ID Structure

```c
// ScratchBird uses 64-bit transaction IDs everywhere
typedef uint64_t TransactionId;

// Extended transaction ID block for ScratchBird
struct TransactionIdBlock {
    TransactionId   tib_next;              // Next transaction ID to assign
    TransactionId   tib_oldest;            // Oldest interesting transaction (OIT)
    TransactionId   tib_oldest_active;     // Oldest active transaction (OAT)
    TransactionId   tib_oldest_snapshot;   // Oldest snapshot transaction (OST)
    TransactionId   tib_sweep_threshold;   // When to trigger sweep
    uint32_t        tib_sweep_interval;    // Automatic sweep interval
    uint16_t        tib_database_flags;    // Database state flags
    uint16_t        tib_page_size;         // Current page size (8K-128K)
    UUID            tib_database_uuid;     // Database UUID
    
    // Statistics
    uint64_t        tib_total_commits;     // Total commits
    uint64_t        tib_total_rollbacks;   // Total rollbacks
    uint64_t        tib_gc_cycles;         // GC cycles completed
};

// Transaction ID allocation for ScratchBird
TransactionId sb_allocate_transaction_id(Database* dbb) {
    // Use atomic increment for better concurrency
    TransactionId trans_id = atomic_fetch_add(&dbb->dbb_next_transaction, 1);
    
    // No wraparound with 64-bit IDs (would take centuries)
    // But still check for safety
    if (trans_id >= UINT64_MAX - 1000000) {
        throw DatabaseError("Transaction ID space exhausted");
    }
    
    // Extend TIP pages if needed (every 256 transactions)
    if (trans_id % TRANS_PER_TIP_PAGE == 0) {
        sb_extend_tip(dbb, trans_id);
    }
    
    // Check if sweep needed
    if (should_trigger_sweep(dbb, trans_id)) {
        schedule_background_sweep(dbb);
    }
    
    return trans_id;
}
```

### Transaction Structure

```c
// ScratchBird transaction structure
struct SBTransaction {
    // Identity
    TransactionId   tra_number;            // This transaction's ID
    UUID            tra_uuid;               // Transaction UUID (for distributed)
    
    // Snapshot information
    TransactionId   tra_top;                // Highest transaction at start
    TransactionId   tra_oldest;             // OIT at start
    TransactionId   tra_oldest_active;      // OAT at start
    
    // State
    enum TxState {
        TX_ACTIVE,
        TX_PREPARING,       // Two-phase commit preparing
        TX_PREPARED,        // Two-phase commit prepared
        TX_COMMITTING,
        TX_COMMITTED,
        TX_ABORTING,
        TX_ABORTED
    } tra_state;
    
    // Isolation level
    enum IsolationLevel {
        ISO_READ_COMMITTED,
        ISO_REPEATABLE_READ,
        ISO_SERIALIZABLE
    } tra_isolation;
    
    // Flags
    uint32_t        tra_flags;
    
    // Lock management
    TransactionLock* tra_lock;             // Transaction lock
    LockList*       tra_locks;              // Held locks
    
    // TIP cache for this transaction
    TIPCache*       tra_tip_cache;         // Cached TIP state
    
    // Version management
    VersionList*    tra_versions;          // Created versions
    SavepointStack* tra_savepoints;        // Savepoint stack
    
    // Statistics
    uint64_t        tra_records_read;
    uint64_t        tra_records_written;
    uint64_t        tra_versions_created;
    uint64_t        tra_gc_records;
    
    // Connection info
    UUID            tra_session_uuid;      // Session UUID
    uint64_t        tra_connection_id;     // Y-Valve connection ID
};
```

## Version Chain Management

### UUID-Based Version Pointers

```c
// ScratchBird record header with UUID references
struct SBRecordHeader {
    TransactionId   rhd_transaction;       // Transaction that created version
    UUID            rhd_back_version;       // UUID of back version (or null)
    uint32_t        rhd_flags;              // Record flags
    uint32_t        rhd_format;             // Record format version
    uint32_t        rhd_length;             // Data length
    uint8_t         rhd_compression;        // Compression type
    uint8_t         rhd_reserved[3];        // Alignment
    uint8_t         rhd_data[];             // Record data follows
};

// Record flags
#define RHD_DELETED         0x0001  // Record is deleted
#define RHD_CHAIN           0x0002  // Has back version
#define RHD_FRAGMENTED      0x0004  // Record is fragmented
#define RHD_DELTA           0x0008  // Delta compressed
#define RHD_COMPRESSED      0x0010  // Full compression
#define RHD_BLOB_INLINE     0x0020  // BLOB data inline
#define RHD_BLOB_EXTERNAL   0x0040  // BLOB in external storage
#define RHD_REPLICATED      0x0080  // Replicated to shadow
#define RHD_ENCRYPTED       0x0100  // Record encrypted
```

### Version Creation with UUIDs

```c
// Create new version in ScratchBird
UUID sb_create_version(SBTransaction* trans, 
                      const RecordData* data,
                      const UUID* back_version) {
    Database* dbb = trans->tra_database;
    
    // Generate UUID for new version
    UUID version_uuid = generate_uuid_v7();
    
    // Allocate space based on page size
    PageSize page_size = dbb->dbb_page_size;
    DataPage* page = sb_allocate_record_space(dbb, data->length, page_size);
    
    // Build record header
    SBRecordHeader* header = (SBRecordHeader*) 
        sb_get_record_slot(page, version_uuid);
    
    header->rhd_transaction = trans->tra_number;
    header->rhd_flags = 0;
    header->rhd_format = data->format;
    header->rhd_length = data->length;
    
    // Set back version if updating
    if (back_version && !uuid_is_null(*back_version)) {
        header->rhd_back_version = *back_version;
        header->rhd_flags |= RHD_CHAIN;
        
        // Try delta compression
        if (should_use_delta(data, back_version)) {
            apply_delta_compression(header, data, back_version);
            header->rhd_flags |= RHD_DELTA;
        }
    } else {
        uuid_clear(header->rhd_back_version);
    }
    
    // Copy or compress data
    if (header->rhd_flags & RHD_DELTA) {
        // Delta already applied
    } else if (should_compress(data)) {
        size_t compressed_size = compress_record(
            header->rhd_data, 
            data->data, 
            data->length
        );
        header->rhd_length = compressed_size;
        header->rhd_flags |= RHD_COMPRESSED;
        header->rhd_compression = COMPRESSION_LZ4;
    } else {
        memcpy(header->rhd_data, data->data, data->length);
    }
    
    // Add to transaction's version list
    add_version_to_transaction(trans, version_uuid);
    
    // Mark page dirty
    sb_mark_page_dirty(page, trans->tra_number);
    
    // Log for replication
    if (dbb->dbb_flags & DBB_REPLICATED) {
        log_version_for_replication(trans, version_uuid, header);
    }
    
    return version_uuid;
}
```

### Enhanced Visibility Rules

```c
// ScratchBird visibility check with isolation levels
VisibilityState sb_check_visibility(const SBTransaction* reader,
                                   const SBRecordHeader* version) {
    TransactionId version_trans = version->rhd_transaction;
    TransactionId reader_trans = reader->tra_number;
    
    // Deleted record check
    if (version->rhd_flags & RHD_DELETED) {
        // Check if deletion is visible
        if (version_trans == reader_trans) {
            return VIS_DELETED;  // Own deletion
        }
        if (is_committed_before(version_trans, reader)) {
            return VIS_DELETED;  // Committed deletion
        }
        // Deletion not visible, continue to back version
        return VIS_CONTINUE;
    }
    
    // Own changes always visible
    if (version_trans == reader_trans) {
        return VIS_VISIBLE;
    }
    
    // Apply isolation level rules
    switch (reader->tra_isolation) {
        case ISO_READ_COMMITTED:
            // See latest committed version
            if (is_committed(version_trans)) {
                return VIS_VISIBLE;
            }
            break;
            
        case ISO_REPEATABLE_READ:
            // See versions committed before statement
            if (version_trans <= reader->tra_top) {
                if (is_committed_before(version_trans, reader)) {
                    return VIS_VISIBLE;
                }
            }
            break;
            
        case ISO_SERIALIZABLE:
            // See versions committed before transaction
            if (version_trans < reader->tra_oldest_active) {
                return VIS_VISIBLE;
            }
            break;
    }
    
    // Version not visible, check back version
    return VIS_CONTINUE;
}

// Walk version chain using UUIDs
Record* sb_get_visible_version(SBTransaction* trans, UUID record_uuid) {
    Database* dbb = trans->tra_database;
    UUID current_uuid = record_uuid;
    
    while (!uuid_is_null(current_uuid)) {
        // Fetch version by UUID
        SBRecordHeader* version = sb_fetch_version(dbb, current_uuid);
        if (!version) {
            return nullptr;  // Version not found
        }
        
        // Check visibility
        VisibilityState vis = sb_check_visibility(trans, version);
        
        switch (vis) {
            case VIS_VISIBLE:
                return sb_materialize_record(version);
                
            case VIS_DELETED:
                return nullptr;  // Record deleted
                
            case VIS_CONTINUE:
                // Move to back version
                current_uuid = version->rhd_back_version;
                break;
                
            case VIS_INVISIBLE:
                // Skip to back version
                current_uuid = version->rhd_back_version;
                break;
        }
    }
    
    return nullptr;  // No visible version found
}
```

## Enhanced TIP (Transaction Inventory Page)

### Multi-Page-Size TIP Support

```c
// ScratchBird TIP page structure supporting all page sizes
struct SBTipPage {
    PageHeader      tip_header;            // Standard page header (96 bytes)
    TransactionId   tip_min;               // Minimum transaction on page
    TransactionId   tip_max;               // Maximum transaction on page
    uint32_t        tip_next_page;         // Next TIP page number
    uint32_t        tip_transactions_count; // Number of transactions on page
    
    // Page-size dependent array
    uint8_t         tip_transactions[];    // Transaction states (2 bits each)
};

// Calculate TIP capacity based on page size
uint32_t sb_tip_capacity(PageSize page_size) {
    uint32_t usable = page_size - sizeof(SBTipPage);
    return (usable * 8) / 2;  // 2 bits per transaction
}

// TIP constants for different page sizes
struct TIPConstants {
    PageSize    page_size;
    uint32_t    transactions_per_page;
    uint32_t    bytes_per_page;
};

static const TIPConstants TIP_CONSTANTS[] = {
    { PAGE_8K,   (8192 - 128) * 4,  8192 - 128 },   // ~32K transactions
    { PAGE_16K,  (16384 - 128) * 4, 16384 - 128 },  // ~65K transactions
    { PAGE_32K,  (32768 - 128) * 4, 32768 - 128 },  // ~130K transactions
    { PAGE_64K,  (65536 - 128) * 4, 65536 - 128 },  // ~261K transactions
    { PAGE_128K, (131072 - 128) * 4, 131072 - 128 }  // ~523K transactions
};
```

### Optimized TIP Cache

```c
// Enhanced TIP cache with LRU and sharding
struct SBTIPCache {
    // Sharded for concurrency (reduce contention)
    struct Shard {
        RWLock          lock;
        LRUCache<TransactionId, TxState> cache;
        uint64_t        hits;
        uint64_t        misses;
    } shards[TIP_CACHE_SHARDS];
    
    // Global cache stats
    atomic<uint64_t> total_hits;
    atomic<uint64_t> total_misses;
    atomic<uint64_t> evictions;
    
    // Cache configuration
    size_t          entries_per_shard;
    size_t          total_memory;
};

// Get transaction state with caching
TxState sb_get_transaction_state(Database* dbb, 
                                TransactionId trans_id) {
    SBTIPCache* cache = dbb->dbb_tip_cache;
    
    // Determine shard
    size_t shard_idx = trans_id % TIP_CACHE_SHARDS;
    auto& shard = cache->shards[shard_idx];
    
    // Try cache first (read lock)
    {
        ReadLock lock(shard.lock);
        if (auto* state = shard.cache.get(trans_id)) {
            shard.hits++;
            cache->total_hits++;
            return *state;
        }
    }
    
    // Cache miss - fetch from TIP page
    shard.misses++;
    cache->total_misses++;
    
    TxState state = sb_fetch_tip_state(dbb, trans_id);
    
    // Update cache (write lock)
    {
        WriteLock lock(shard.lock);
        shard.cache.put(trans_id, state);
    }
    
    return state;
}
```

## Garbage Collection Enhancements

### Adaptive Garbage Collection

```c
// ScratchBird adaptive GC based on workload
struct AdaptiveGC {
    enum Mode {
        GC_AGGRESSIVE,      // High version chain length
        GC_NORMAL,          // Normal operation
        GC_LAZY,           // Low activity
        GC_OFF             // Disabled
    } mode;
    
    // Thresholds
    uint32_t    aggressive_threshold;  // Avg chain length
    uint32_t    lazy_threshold;        // Transaction rate
    
    // Statistics for adaptation
    uint64_t    avg_chain_length;
    uint64_t    transaction_rate;
    uint64_t    gc_overhead_percent;
};

// Adaptive GC decision
void sb_adaptive_gc_cycle(Database* dbb) {
    AdaptiveGC* gc = &dbb->dbb_adaptive_gc;
    
    // Calculate metrics
    gc->avg_chain_length = calculate_avg_chain_length(dbb);
    gc->transaction_rate = calculate_transaction_rate(dbb);
    
    // Adjust mode based on metrics
    if (gc->avg_chain_length > gc->aggressive_threshold) {
        gc->mode = GC_AGGRESSIVE;
        run_aggressive_gc(dbb);
    } else if (gc->transaction_rate < gc->lazy_threshold) {
        gc->mode = GC_LAZY;
        run_lazy_gc(dbb);
    } else {
        gc->mode = GC_NORMAL;
        run_normal_gc(dbb);
    }
    
    // Log GC decision
    log_gc_decision(dbb, gc);
}
```

### Parallel Garbage Collection

```c
// Parallel GC for multi-core systems
struct ParallelGC {
    ThreadPool*     gc_threads;
    WorkQueue<PageNumber> pages_to_gc;
    atomic<uint64_t> pages_processed;
    atomic<uint64_t> versions_removed;
};

// Parallel GC worker
void parallel_gc_worker(Database* dbb, ParallelGC* pgc) {
    PageNumber page_num;
    
    while (pgc->pages_to_gc.pop(page_num)) {
        // Process single page
        uint64_t removed = gc_page(dbb, page_num);
        
        pgc->pages_processed++;
        pgc->versions_removed += removed;
        
        // Yield periodically
        if (pgc->pages_processed % 100 == 0) {
            std::this_thread::yield();
        }
    }
}

// Run parallel GC
void sb_run_parallel_gc(Database* dbb) {
    ParallelGC pgc;
    pgc.gc_threads = new ThreadPool(std::thread::hardware_concurrency());
    
    // Queue all data pages
    for (PageNumber page = FIRST_DATA_PAGE; 
         page < dbb->dbb_page_count; 
         page++) {
        if (is_data_page(dbb, page)) {
            pgc.pages_to_gc.push(page);
        }
    }
    
    // Start workers
    for (int i = 0; i < pgc.gc_threads->size(); i++) {
        pgc.gc_threads->submit(parallel_gc_worker, dbb, &pgc);
    }
    
    // Wait for completion
    pgc.gc_threads->wait_all();
    
    // Log results
    log_gc_stats(dbb, pgc.pages_processed, pgc.versions_removed);
}
```

## Integration with Shadow Databases

```c
// MGA-aware shadow replication
struct MGAShadowReplication {
    // Track transaction states for shadow
    TransactionId   shadow_oit;        // Shadow's OIT
    TransactionId   shadow_oat;        // Shadow's OAT
    TransactionId   shadow_last_committed; // Last committed on shadow
    
    // Version replication
    Queue<VersionUpdate> pending_versions;
    
    // TIP synchronization
    Queue<TIPUpdate> pending_tip_updates;
};

// Replicate version to shadow
void replicate_version_to_shadow(Database* dbb, 
                                const SBRecordHeader* version,
                                UUID version_uuid) {
    MGAShadowReplication* shadow = dbb->dbb_shadow;
    
    // Create version update packet
    VersionUpdate update;
    update.version_uuid = version_uuid;
    update.transaction_id = version->rhd_transaction;
    update.back_version = version->rhd_back_version;
    update.flags = version->rhd_flags;
    update.data_length = version->rhd_length;
    update.data = copy_data(version->rhd_data, version->rhd_length);
    
    // Queue for replication
    shadow->pending_versions.push(update);
    
    // Wake replication thread if needed
    if (shadow->pending_versions.size() > REPLICATION_BATCH_SIZE) {
        wake_replication_thread(dbb);
    }
}

// Synchronize TIP state with shadow
void sync_tip_with_shadow(Database* dbb, 
                         TransactionId trans_id,
                         TxState new_state) {
    MGAShadowReplication* shadow = dbb->dbb_shadow;
    
    // Create TIP update
    TIPUpdate update;
    update.transaction_id = trans_id;
    update.new_state = new_state;
    update.timestamp = get_current_timestamp();
    
    // Queue for replication
    shadow->pending_tip_updates.push(update);
    
    // Immediate sync for commits
    if (new_state == TX_COMMITTED) {
        flush_shadow_updates(dbb);
    }
}
```

## Performance Monitoring

```c
// Comprehensive MGA statistics for ScratchBird
struct SBMGAStatistics {
    // Transaction metrics
    struct {
        uint64_t    total_started;
        uint64_t    total_committed;
        uint64_t    total_rolled_back;
        uint64_t    currently_active;
        uint64_t    max_concurrent;
        uint64_t    avg_duration_ms;
    } transactions;
    
    // Version chain metrics
    struct {
        uint64_t    total_versions;
        uint64_t    avg_chain_length;
        uint64_t    max_chain_length;
        uint64_t    chains_over_10;
        uint64_t    chains_over_100;
    } versions;
    
    // Garbage collection metrics
    struct {
        uint64_t    gc_cycles;
        uint64_t    versions_removed;
        uint64_t    pages_cleaned;
        uint64_t    avg_gc_time_ms;
        uint64_t    gc_efficiency_percent;
    } gc;
    
    // TIP metrics
    struct {
        uint64_t    tip_pages;
        uint64_t    tip_cache_hits;
        uint64_t    tip_cache_misses;
        uint64_t    tip_cache_hit_rate;
        uint64_t    tip_compressions;
    } tip;
    
    // Conflict metrics
    struct {
        uint64_t    update_conflicts;
        uint64_t    deadlocks;
        uint64_t    wait_timeouts;
        uint64_t    avg_wait_time_ms;
    } conflicts;
};

// Collect MGA statistics
void sb_collect_mga_stats(Database* dbb, SBMGAStatistics* stats) {
    // Real-time transaction stats
    stats->transactions.currently_active = 
        count_active_transactions(dbb);
    
    // Version chain analysis (sampled)
    analyze_version_chains(dbb, stats);
    
    // GC effectiveness
    stats->gc.gc_efficiency_percent = 
        (stats->gc.versions_removed * 100) / 
        (stats->versions.total_versions + 1);
    
    // TIP cache effectiveness
    stats->tip.tip_cache_hit_rate = 
        (stats->tip.tip_cache_hits * 100) / 
        (stats->tip.tip_cache_hits + stats->tip.tip_cache_misses + 1);
}
```

## Configuration

```yaml
# MGA configuration for ScratchBird
mga:
  # Transaction management
  transaction:
    max_concurrent: 10000
    timeout_seconds: 300
    deadlock_timeout_ms: 1000
    
  # Garbage collection
  gc:
    mode: adaptive  # adaptive, aggressive, normal, lazy, off
    cooperative_enabled: true
    background_enabled: true
    parallel_threads: 4
    sweep_interval: 20000
    
  # TIP cache
  tip_cache:
    shards: 16
    entries_per_shard: 10000
    memory_limit_mb: 64
    
  # Version management
  versions:
    delta_compression: true
    compression_algorithm: lz4
    max_chain_length: 100
    chain_warning_threshold: 50
```

This implementation provides ScratchBird with a robust, scalable MGA that improves upon Firebird's design while maintaining compatibility with its core concepts.