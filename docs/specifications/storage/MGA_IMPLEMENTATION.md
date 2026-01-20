# ScratchBird MGA (Multi-Generational Architecture) Implementation

## Implementation Status

**Status**: ✅ **100% FIREBIRD MGA COMPLIANT** (Completed November 2, 2025)

**Key Achievements**:
- ✅ All 7 index types use TIP-based visibility (`isVersionVisible()`)
- ✅ Storage layer SNAPSHOT isolation uses TIP lookups
- ✅ Zero `isSnapshotVisible()` calls in entire codebase
- ✅ Zero `Snapshot*` parameters in index APIs
- ✅ Comprehensive test coverage (unit, integration, performance)
- ✅ O(1) TIP lookup performance validated (< 100ns per check)

**Compliance Validation**:
- Snapshot contamination: **0 instances** ✅
- TIP-based visibility calls: **16 instances** ✅
- Transaction state lookups: **8 instances** ✅
- See `/docs/Alpha_Phase_1_Archive/planning_archive (1)/MGA_COMPLIANCE_FIX_PLAN.md` for complete audit results

## Overview

ScratchBird implements pure Firebird MGA with the following enhancements:

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
    uint64_t        tra_connection_id;     // Listener/pool connection ID
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



# An Implementation-Level Analysis of FirebirdSQL's Record Update and Versioning Mechanism

### Introduction

This report provides a detailed technical specification of the record update and versioning mechanism within the FirebirdSQL Relational Database Management System (RDBMS). The analysis addresses the common but technically imprecise characterization of this mechanism as "HOT update logic," a term specific to a performance optimization in PostgreSQL. While the user's query correctly identifies a key performance characteristic—efficiency in handling updates—the underlying architecture in Firebird is fundamentally different and predates the concept of Heap-Only Tuples (HOT).

Firebird's approach is rooted in its Multi-Generational Architecture (MGA), a robust and mature implementation of Multi-Version Concurrency Control (MVCC) inherited from its predecessor, InterBase.1 This architecture was among the first commercial implementations of MVCC and is designed to ensure high concurrency by preventing read operations from blocking write operations, and vice versa.1 At the core of Firebird's update process is the concept of "back versions." When a record is modified, the engine creates a new version of the record in its original location while preserving the prior state as a separate "back version," which can be either a full copy or a more space-efficient delta of the changes.1

This design inherently provides the primary benefit that PostgreSQL's HOT update feature was later developed to address: the avoidance of unnecessary index maintenance. In a standard PostgreSQL update, a new physical version of the row (a tuple) is created, and all associated index entries must be updated to point to this new location, a process functionally equivalent to a `DELETE` followed by an `INSERT`.4 The HOT optimization mitigates this by allowing the new tuple to be stored on the same data page as the old one, thus avoiding index modifications if no indexed columns were changed.5 Firebird's MGA, by modifying the record in-place and linking to a back version, naturally avoids this index write amplification without needing a separate optimization layer.

The objective of this report is to deliver a specification of Firebird's update logic that is sufficiently detailed for a developer to understand and potentially implement a similar system. The analysis will proceed from the high-level architectural principles of MGA down to the byte-level on-disk data structures and the logic of the C++ source code that governs these operations.

## Section 1: Architectural Foundation: Firebird's Multi-Version Concurrency Control (MVCC)

### 1.1 The Multi-Generational Architecture (MGA): An InterBase Legacy

The concurrency control model in Firebird is a direct descendant of the Multi-Generational Architecture (MGA) pioneered by InterBase, making it one of the most mature and field-tested MVCC implementations in existence.1 The fundamental principle of MGA is to eliminate contention between reading and writing transactions by maintaining multiple, concurrent versions of data records.3 When a transaction modifies a record, it does not overwrite the existing data in a way that would make it invisible or locked to other transactions. Instead, it creates a new version of that record, allowing other transactions to continue accessing the older, consistent versions.1 This non-locking behavior is the cornerstone of its high concurrency, ensuring that long-running read queries do not block update operations, and vice versa.3

### 1.2 Transaction Snapshots and Visibility

Visibility in Firebird's MGA is governed by a strict, point-in-time snapshot mechanism. When a transaction begins, the server assigns it a unique, monotonically increasing transaction ID number.1 This number serves as a temporal marker for the transaction's view of the database. Every record version stored in the database is "signed" with the transaction ID of the transaction that created it, embedding its creation context directly into its metadata.1

Firebird's default transaction isolation level is `SNAPSHOT`.6 This level provides a transaction with a consistent view of the database as it existed at the precise moment the transaction began. Any changes that are committed by other, concurrent transactions *after* the snapshot transaction has started will remain invisible to it for its entire duration.8 This powerful guarantee of a stable, unchanging view of the data is achieved by comparing the transaction ID of a record version against the current transaction's snapshot.

The state of every transaction in the database—whether it is active, committed, or rolled back—is tracked on a set of special pages known as Transaction Inventory Pages (TIPs).9 A TIP is essentially a bitmap that stores the status of a range of transactions.11 When a transaction encounters a record version, it reads the version's transaction ID and consults the TIP to determine the status of that creating transaction. If the creating transaction is committed and its ID is older than the current transaction's start ID, the record version is visible. If the creating transaction is still active or was rolled back, the version is invisible, and the engine must look for an older version of the record to present to the reading transaction.

### 1.3 The Concept of Record Version Chains and "Back Versions"

The implementation of versioning in Firebird diverges significantly from that of other popular MVCC databases like PostgreSQL. While PostgreSQL employs an append-only storage model, where an `UPDATE` creates an entirely new physical tuple and leaves the old one to be cleaned up later by a PostgreSQL `VACUUM` process, Firebird is centered on the concept of "back versions".1 ScratchBird follows the Firebird model and does not implement PostgreSQL VACUUM phases.

When a transaction updates a record, it performs two distinct but related operations 1:

1. **Creation of a New Record Version:** The engine modifies the data of the record in its original physical location on the data page. The record's header is updated to be "signed" with the current transaction's ID, marking it as the newest version.

2. **Creation of a "Back Version":** The state of the record *before* the modification is preserved by creating a back version. This back version acts as a record-specific undo log and contains the data of the previous state.1 It is stored either on the same data page, if sufficient space is available, or on a different page.

The new, current record version contains a direct pointer in its header to the physical location (page number and line number) of the back version it just created.12 This creates a singly linked list of record versions, but one that is traversed backward, from newest to oldest (N2O). The "head" of this version chain is always the most current record version found at the primary record location, and subsequent links point to progressively older states of the data.

This architectural choice has profound consequences for performance, particularly concerning index maintenance. In PostgreSQL's Oldest-to-Newest (O2N) model, every update creates a new tuple at a new physical address. Consequently, every index pointing to that logical row must be updated to reference the new physical location.7 This can lead to significant write amplification, especially in tables with many indexes. Firebird's N2O model, by contrast, maintains a stable primary record location. Index entries point to this primary location, which always holds the head of the version chain. As long as the columns that are part of an index are not themselves modified, an `UPDATE` operation does not require any changes to the index structures. This inherent efficiency in handling updates to non-indexed columns is a key characteristic of the MGA. Performance comparisons confirm this, showing that a Firebird database's size grows significantly more slowly than a PostgreSQL database's under heavy update workloads, as it avoids the storage bloat associated with creating new tuples and index entries for every modification.13

## Section 2: The Physical Layer: On-Disk Structures for Record Versioning

### 2.1 Overview of the Firebird Database File Layout

A Firebird database is physically stored as a collection of fixed-size pages within one or more files on the host operating system.9 The page size is defined at database creation and remains constant throughout its lifecycle.15 The format of these pages is dictated by the On-Disk Structure (ODS) version, a critical piece of metadata stored on the very first page of the database, known as the Header Page.10 The ODS version ensures that the database engine can correctly interpret the layout of all subsequent pages.15

The database file is a structured collection of different page types, each serving a specific function. Key types include 9:

- **Header Page (`pag_header`, type 0x01):** The first page, containing global database metadata like page size, ODS version, and the ID of the next transaction.

- **Page Inventory Page (`pag_pages`, type 0x02):** A bitmap that tracks the allocation status (used or free) of every other page in the database.

- **Transaction Inventory Page (`pag_transactions`, type 0x03):** A bitmap that stores the commit status of transactions, essential for MVCC visibility checks.

- **Pointer Page (`pag_pointer`, type 0x04):** Contains an array of pointers to the data pages that belong to a specific table.

- **Data Page (`pag_data`, type 0x05):** The page type that stores the actual user data in the form of records.

This analysis focuses primarily on the structure of the Data Page, as it is the canvas upon which record versioning is implemented.

### 2.2 Detailed Anatomy of a Data Page (`pag_data`)

All pages in a Firebird database begin with a standard 16-byte header (`struct pag`), which contains the page type, flags, a checksum for integrity, and a generation number that increments with each write.10

Beyond this standard header, a Data Page has its own specific header (`struct dpg`) that provides context for the data it contains. This header includes the page's sequence number within the table's chain of data pages, the ID of the relation (table) to which it belongs, and a count of the record segments stored on the page.12

The page employs a classic heap organization for storing records. An array of record descriptors, or line pointers (`dpg_rpt`), begins immediately after the page header and grows downwards from the top of the page. The actual record data is stored starting from the very end of the page and grows upwards. The page is considered full when the space between the last descriptor and the last byte of record data is too small to accommodate another record fragment.9 Each entry in the descriptor array is a pair of 16-bit unsigned integers: the first specifies the offset of the record's data from the beginning of the page, and the second specifies its length.9 This indirection allows the engine to compact the data on the page to clean up free space without having to update external pointers (like index entries) that refer to a record via its stable page number and line number (descriptor index).

### 2.3 In-Depth Analysis of the Record Header (`rhd` struct)

The most critical data structure for understanding Firebird's versioning and update logic is the record header (`struct rhd`). Every record or record fragment stored on a data page is prefixed with this header, which contains the essential metadata for MVCC.12 The precise layout of this structure is fundamental to implementing the versioning model.

**Table 1: Record Header (`rhd`) Structure**

| Field             | Data Type (C) | Length (bytes) | Description                                                                                                                                                                                               |
| ----------------- | ------------- | -------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rhd_transaction` | `ULONG`       | 4              | The ID of the transaction that created this specific record version. Used for visibility checks.                                                                                                          |
| `rhd_b_page`      | `ULONG`       | 4              | The page number where the immediate prior version of this record (the back version) is stored. A value of 0 indicates no back version.                                                                    |
| `rhd_b_line`      | `USHORT`      | 2              | The line number (index in the `dpg_rpt` descriptor array) of the back version on its page.                                                                                                                |
| `rhd_flags`       | `USHORT`      | 2              | A bitmask of flags that describe the state and characteristics of the record version (see Table 2).                                                                                                       |
| `rhd_format`      | `UCHAR`       | 1              | The version of the table's metadata format at the time this record was written. This allows the engine to correctly interpret the data even if the table structure (e.g., columns) has changed over time. |
| `rhd_data`        | `UCHAR`       | Variable       | The actual record data, which is typically compressed using Run-Length Encoding (RLE) to conserve space.12                                                                                                |

### 2.4 Record Header Flags and Record Data

The `rhd_flags` field is a bitmask that provides crucial information about the state of the record version. Correctly interpreting these flags is essential for navigating version chains and determining record visibility.

**Table 2: Record Header Flags (`rhd_flags`) Definitions**

| Flag             | Description                                                                                                                                                                                                                                               |
| ---------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `rhd_deleted`    | This record version has been marked as logically deleted. It remains as a "ghost" record until the deleting transaction commits and the version becomes garbage.                                                                                          |
| `rhd_chain`      | This record is an old version within a version chain. A newer version exists that points back to this one via its `rhd_b_page` and `rhd_b_line` fields.                                                                                                   |
| `rhd_fragment`   | This record is a fragment of a larger record that was too large to fit on a single data page.                                                                                                                                                             |
| `rhd_incomplete` | This record is the *first* fragment of a multi-fragment record. Subsequent fragments are linked from it.                                                                                                                                                  |
| `rhd_delta`      | This record version stores only the differences (a delta) from its immediate back version. To reconstruct the state of the back version, the engine must apply these differences to the data in the current version. This is a key storage optimization.1 |
| `rhd_gc_active`  | This record is currently being processed by the garbage collector (sweep process). Other transactions should not attempt to modify it.                                                                                                                    |
| `rhd_damaged`    | The record is known to be corrupt, likely due to a consistency issue.                                                                                                                                                                                     |

The actual user data, `rhd_data`, is compressed using Run-Length Encoding (RLE) to reduce storage footprint, especially for data with repeating byte sequences.12

## Section 3: The Update Operation Deconstructed: From SQL to On-Disk Modification

### 3.1 High-Level Flow of an `UPDATE` Statement

The execution of an `UPDATE` statement in Firebird is a multi-stage process that translates a high-level DML command into precise, version-aware modifications at the physical storage layer. The process begins with the SQL parser, which validates the syntax of the `UPDATE` statement. A full `UPDATE` statement can include several clauses that influence its execution, such as `WHERE` to filter rows, `ORDER BY` and `ROWS` to limit the update to a specific subset of the filtered rows, and `RETURNING` to send back values from the modified rows.18

The query optimizer then generates an execution plan, which may involve using indexes to efficiently locate the records targeted by the `WHERE` clause. Firebird's optimizer can create a sparse bitmap from an index scan, where each true bit corresponds to the page address and record offset of a candidate record, allowing for efficient retrieval.6 For each record identified for modification, the engine then initiates the core versioning and update protocol.

### 3.2 The Creation of a "Back Version"

Before the engine can alter the data of the target record, it must first preserve the record's current state. This is essential for maintaining the consistent database snapshots required by other active transactions. This preservation is achieved by creating a "back version".1

A new record structure is allocated in memory to serve as this back version. The data from the current, on-disk record version is copied into this structure. This back version is then written to a data page. For locality of reference and performance, the engine will attempt to store the back version on the same data page as the original record, provided there is sufficient free space. If the page is full, a new page will be allocated for it. The header of this newly stored back version is marked with the `rhd_chain` flag, signifying that it is now an older version in a chain.12

A critical optimization in this process is the use of deltas. Firebird does not always need to store a full copy of the prior record. If an update modifies only a small portion of a large record, the engine can create a back version that contains only the differences between the old and new states.1 This is conceptually similar to the rollback segments used in Oracle.1 When this optimization is used, the *new* record version is marked with the `rhd_delta` flag in its header.12 This flag instructs the engine that to reconstruct the state of the immediate prior version, it must take the data from the current version and apply the delta stored in the back version. This significantly reduces the storage overhead and I/O cost associated with update-heavy workloads.

### 3.3 The Creation of the New Record Version and Pointer Linkage

Once the prior state has been safely stored as a back version, the engine proceeds to modify the original record in its physical location. The data within the `rhd_data` section of the record is overwritten with the new values specified in the `SET` clause of the `UPDATE` statement.

Following the data modification, the record's header (`rhd`) is updated to reflect its new state and link it into the version chain:

1. The `rhd_transaction` field is set to the ID of the current, modifying transaction. This "signs" the new version.

2. The `rhd_b_page` and `rhd_b_line` fields are populated with the page number and line number (descriptor index) of the back version that was just created. This establishes the backward link in the N2O chain.

3. The `rhd_flags` field is updated. The `rhd_chain` flag is set to indicate that this record now has a history (a back version). If the delta optimization was employed, the `rhd_delta` flag is also set.

This in-place modification makes the updated record the new head of its version chain, with a direct physical pointer to its immediate predecessor. Other transactions that need to see the older state of the data will now start at this new head and follow the back-pointers until they find a version that is visible within their snapshot.

### 3.4 The Role of `BEFORE UPDATE` Triggers

Firebird's Procedural SQL (PSQL) provides a powerful mechanism for executing code automatically in response to data modification events through triggers.20 A `BEFORE UPDATE` trigger is a block of PSQL code that is executed by the engine *after* it has identified a record for update but *before* the new record version is physically written to the data page.

Within a `BEFORE UPDATE` trigger, the developer has access to two special context variables: `OLD.` and `NEW.`. The `OLD.` context provides read-only access to the values of the record as they currently exist on disk. The `NEW.` context provides read-write access to the incoming values from the `UPDATE` statement's `SET` clause.19

This allows the trigger logic to perform complex validation, enforce business rules, or automatically transform data. For instance, a trigger could ensure that a salary increase does not exceed a certain percentage or automatically convert a text field to uppercase using `NEW.CODCLI = upper(NEW.CODCLI);`.20 The crucial point is that the values ultimately written to the new record version on the data page are the final values present in the `NEW.` context *after* the trigger has completed its execution. This makes triggers an integral part of the data modification pipeline, capable of altering the outcome of an `UPDATE` statement at the engine level.

## Section 4: Source Code Implementation Analysis: The Core Update Logic

### 4.1 Navigating the Firebird Source: Key Modules

To understand the update mechanism at its most fundamental level, an analysis of the Firebird C++ source code is necessary. The source code is publicly available in a Git repository on GitHub.21 The core engine logic, inherited from the original InterBase codebase, resides in the `src/jrd` directory, where "JRD" is an acronym for "Jim's Relational Database," a nod to its original author, Jim Starkey.23

Within this directory, two modules are of paramount importance to the update process:

- **`dpm.cpp` (Data Page Manager):** This module is responsible for the physical management of data on pages. Its functions handle the low-level tasks of storing records and fragments, fetching them by their physical address (page and line number), and managing free space on the page. It acts as the physical storage engine. Although direct access to this file was unavailable during the research phase 24, its role is to execute the physical storage commands issued by the higher-level VIO module.

- **`vio.cpp` (Virtual I/O):** This module sits atop the DPM and contains the sophisticated logic for record versioning and concurrency control.26 It is responsible for creating new record versions, traversing back-version chains to satisfy read requests (`VIO_chase_record_version`), resolving update conflicts, and coordinating with the transaction manager to determine version visibility.

### 4.2 The `prepare_update` Function (`vio.cpp`): The Gatekeeper

Before any physical modification can occur, the engine must ensure that the update is valid under the current transaction's snapshot and not in conflict with other committed work. This critical gatekeeping function is performed by `VIO::prepare_update`.26 This function does not alter the record itself but rather assesses its state and returns a status indicating whether the update can proceed.

The logical flow of `prepare_update` is as follows:

1. It receives a pointer to the record parameter block (`rpb`) of the record targeted for update.

2. It examines the transaction ID of the current record version (`rpb->rpb_transaction_nr`) and uses the transaction manager (`TRA_snapshot_state`) to determine the status of that transaction relative to the current transaction's snapshot.

3. **Conflict Detection:** It performs several checks. If the record version has been marked as deleted (`rpb_deleted`) by a transaction that has since committed, it returns `PrepareResult::DELETED`, signaling that the record no longer logically exists. For transactions running at the `READ COMMITTED` isolation level, if the record has been updated and committed by another transaction since the read occurred, it returns `PrepareResult::CONFLICT`. This is the classic "update conflict" scenario.

4. **Garbage Collection Check:** It inspects the `rpb_gc_active` flag. If this flag is set, it means the record is currently being processed by the sweep (garbage collection) mechanism, and the function will wait until that process is complete before proceeding.

5. If all concurrency and state checks pass, the function returns `PrepareResult::SUCCESS`, granting permission for the physical update to be carried out by the DPM.

### 4.3 The `DPM_update` Function (`dpm.cpp`) and its Interaction with `VIO`

While the source code for `dpm.cpp` was not directly reviewed, its logic can be reliably inferred from the functions it must call within the VIO layer and its role as the physical page manager. The primary update function, likely named `DPM_update` or a functional equivalent, orchestrates the entire physical modification process.

The inferred logical flow of `DPM_update` is:

1. **Preparation:** The first action is to call `VIO::prepare_update` to ensure the update is permissible from a concurrency standpoint. If this check fails, the operation is aborted with an update conflict.

2. **Back Version Creation:** If preparation is successful, the function proceeds to create the back version.
   
   - It calls a function like `VIO::copy_record` to create an in-memory copy of the original record's data.26
   
   - If the delta optimization is applicable, it calculates the differences between the original data and the new data.
   
   - It then calls a lower-level DPM function (e.g., `DPM_store`) to write this back version (either full or delta) to a data page, which returns the physical location (page number and line number) of the new back version.

3. **New Version Creation:** The function then modifies the in-memory buffer of the original record, applying the new data from the `SET` clause.

4. **Header Linkage:** It updates the header of this modified record buffer, setting the `rhd_b_page` and `rhd_b_line` fields to point to the location of the newly stored back version.

5. **Physical Replacement:** Finally, it calls a function like `VIO::replace_record` to instruct the DPM to physically overwrite the old record version on the data page with the contents of the newly prepared buffer, completing the update.26

### 4.4 `VIO_chase_record_version` (`vio.cpp`): The Read Path

Understanding the update process is incomplete without understanding its counterpart: the read process. The function `VIO_chase_record_version` is responsible for traversing the back-version chain to find a record version that is visible to the current transaction's snapshot.

When a transaction requests a record, the engine starts at the head of the version chain (the current record). It checks the `rhd_transaction` ID against its snapshot.

- If the version is visible (i.e., created by a committed transaction with an ID older than the current transaction's start ID), the data is returned to the client.

- If the version is *not* visible (e.g., it was created by a concurrent transaction that has not yet committed), the engine follows the `rhd_b_page` and `rhd_b_line` pointers to the immediate back version. It then repeats the visibility check on this older version.

This process continues, "chasing" the chain backward until a visible version is found. It is during this traversal that Firebird can perform "co-operative" garbage collection. If the engine encounters a version that is so old it can no longer be seen by *any* active transaction in the database, it can call a function like `purge` to remove that obsolete version and reclaim its space.11

## Section 5: Version Lifecycle Management: Garbage Collection and the Sweep Process

### 5.1 Defining "Garbage"

In Firebird's MGA, record versions that are no longer needed by any active or future transaction are considered "garbage" and must be reclaimed to prevent the database from growing indefinitely. A record version becomes garbage under two primary conditions 11:

1. It was created by a transaction that was ultimately rolled back. Since the transaction's work was undone, the version it created is invalid and can be removed.

2. It is a committed version that is older than the snapshot of the oldest active transaction in the system. If no currently running transaction can possibly see this version, it is obsolete and eligible for collection.

These garbage versions consume valuable disk space and can elongate record version chains, which can degrade read performance as the engine may need to traverse multiple invisible versions before finding a visible one.

### 5.2 The Firebird Sweep Mechanism

Firebird employs a garbage collection process known as "sweep," which is conceptually analogous to PostgreSQL `VACUUM` but with different semantics.27 The sweep process scans data pages, identifies obsolete **back versions** based on transaction state, and removes them. Primary record slots remain stable; space is reclaimed within pages and version chains are shortened.28

A notable feature of Firebird's approach is that garbage collection is not solely the responsibility of a dedicated background process. It is also "co-operative." As described previously, any user transaction that reads a record and traverses its version chain (`VIO_chase_record_version`) has the ability to identify and purge garbage versions it encounters along the way.11 This distributed, on-the-fly cleanup helps to mitigate the accumulation of garbage in frequently accessed tables. However, for tables with low read activity or for cleaning up the remnants of long-running transactions, a dedicated sweep is still necessary.

### 5.3 Triggering an Automated Sweep

In addition to manual invocation (e.g., via the `gfix` utility), Firebird can perform sweeps automatically. This automated sweep is governed by a database-level configuration parameter called the "sweep interval".28 This interval is a number that represents a transaction count threshold.

There is a common misconception, present in much of the older documentation, that the automated sweep is triggered when the difference between the Oldest Active Transaction (OAT) and the Oldest Interesting Transaction (OIT) exceeds this interval. However, a direct analysis of the Firebird source code reveals a more nuanced and different trigger condition.30 The actual formula is based on the Oldest Snapshot Transaction (OST). The automated sweep is initiated when the following condition is met: `(OST - OIT) > sweep_interval`.

The terms are defined as follows 11:

- **OIT (Oldest Interesting Transaction):** The transaction ID of the oldest transaction whose state is still relevant to the database. This is typically the oldest transaction that is still active. No record versions created by transactions older than the OIT can be removed, as they might be needed for snapshot consistency.

- **OST (Oldest Snapshot Transaction):** A more complex value, representing the oldest transaction that was active at the time the oldest *currently* active transaction began its work.

The distinction between OAT and OST is subtle but critical. The use of OST in the trigger formula is a deliberate choice in the engine's design to manage the transaction inventory and decide the appropriate time to perform a system-wide cleanup. This detail, confirmed by the source code, is essential for accurately predicting and managing the behavior of the automated sweep process.30

### 5.4 Recent Enhancements: Parallel Sweeping

To improve the performance of database maintenance on modern multi-core hardware, Firebird 5.0 introduced the capability to perform sweep operations in parallel.31 This feature allows the engine to use multiple worker threads to concurrently sweep different parts of a table or database.

Parallel execution can be enabled in two ways:

1. By setting the `ParallelWorkers` parameter in the `firebird.conf` configuration file, which defines a default number of workers for automated sweeps.

2. By using the `-parallel` command-line switch with the `gfix` utility for manual sweeps (e.g., `gfix -sweep -parallel 4 <database>`).31

The engine manages this parallelism by maintaining an internal pool of worker attachments for each database. When a parallel sweep is initiated, the workload is divided among these workers. To minimize contention, the work is typically partitioned by pointer pages, allowing each worker to process a set of data pages without conflicting with other workers over the same physical structures.33 This enhancement can dramatically reduce the time required for sweeping large, heavily fragmented tables, making database maintenance more efficient.

## Section 5: Conclusion and Recommendations for Implementation

### 5.1 Synthesis of Firebird's Update Mechanics

The update mechanism in FirebirdSQL is a sophisticated and mature implementation of Multi-Version Concurrency Control, defined by its Multi-Generational Architecture. The process is distinct from append-only models and is centered on the creation of "back versions" to preserve historical data for transaction snapshots. When an `UPDATE` is executed, the engine performs an in-place modification of the primary record version while storing the prior state as a linked back version, which may be a full copy or a more efficient delta. This creates a Newest-to-Oldest (N2O) version chain that is traversed by read transactions to find a visible, consistent snapshot of the data. The lifecycle of these versions is managed by a garbage collection process known as "sweep," which operates both co-operatively during read operations and as an automated background task, with recent versions of Firebird offering parallel execution for enhanced performance. This architecture's primary advantage is its inherent efficiency in avoiding index updates for modifications that do not affect indexed columns, a benefit that other systems have had to implement through later optimizations.

### 5.2 Key Data Structures and Algorithms for Replication

For a developer seeking to programmatically replicate this logic, the following components are the most critical to implement:

1. **On-Disk Record Header (`rhd`):** A precise, byte-for-byte implementation of the `rhd` structure is paramount. This includes fields for the creating transaction ID, the physical back-pointers (`rhd_b_page`, `rhd_b_line`), and the metadata format version. Special attention must be paid to the correct implementation and interpretation of the bitmask flags in `rhd_flags`.

2. **Delta Generation and Application:** To achieve the storage efficiency of Firebird, an algorithm to calculate the differences between two record versions and an algorithm to apply those differences to reconstruct a prior version are necessary. This corresponds to the `rhd_delta` flag.

3. **Transaction State Management:** A robust system for tracking transaction IDs and their states (active, committed, rolled back) is required. This system, analogous to Firebird's Transaction Inventory Page (TIP), is the foundation for all snapshot visibility decisions.

4. **Version Chain Traversal:** An algorithm equivalent to `VIO_chase_record_version` must be implemented for all read operations. This function must be able to start at the head of a version chain and traverse backward using the `rhd` pointers, checking the visibility of each version against the current transaction's snapshot until a valid version is found.

5. **Garbage Collection (Sweep):** A mechanism to identify and reclaim obsolete record versions is essential for long-term stability and performance. This requires defining the garbage collection trigger (e.g., based on the OIT and OST) and implementing the logic to scan data pages and remove versions that are no longer visible to any active transaction.

### 5.3 Comparative Analysis: Firebird MGA vs. PostgreSQL MVCC/HOT

To conclude, the following table provides a concise summary contrasting Firebird's update architecture with that of PostgreSQL, directly addressing the initial query's terminology and highlighting the distinct design philosophies of the two systems.

**Table 3: Comparative Analysis of Update Mechanisms**

| Feature                | Firebird (MGA with Back Versions)                                                                                                                   | PostgreSQL (MVCC with HOT)                                                                                                                                                                                                          |
| ---------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Update Model**       | Modifies the record in-place and creates a "back version" to store the previous state.1                                                             | Appends a new physical version of the row (tuple); functionally equivalent to a `DELETE` followed by an `INSERT`.4                                                                                                                  |
| **Version Chain**      | Newest-to-Oldest (N2O) linked list, traversed via back-pointers in the record header.12                                                             | Oldest-to-Newest (O2N) linked list, requiring traversal from the start of the chain.7                                                                                                                                               |
| **Index Impact**       | No index update is required if the modified columns are not part of any index. The primary record location is stable.1                              | **Without HOT:** Index updates are always required to point to the new tuple's physical location.4 **With HOT:** Index updates are avoided only if no indexed columns are modified *and* the new tuple fits on the same data page.5 |
| **Garbage Collection** | "Sweep" process. Can be automated, manual, or "co-operative" (performed during read operations).11                                                  | "VACUUM" process. Typically runs as an automated background worker or can be run manually.7                                                                                                                                         |
| **Storage Growth**     | Tends to be more space-efficient under heavy update loads due to in-place modification and delta storage, resulting in less database size growth.13 | Tends to have higher space amplification ("bloat") from updates, as old tuples remain until `VACUUM` reclaims them.13                                                                                                               |

The code review's use of the term "HOT Update" is the source of the confusion. "HOT" (Heap-Only Tuple) is a specific optimization from PostgreSQL. However, the *principle* behind it—avoiding unnecessary index maintenance during an update—is what the review is flagging. Your implementation is missing the key architectural advantage of the Firebird MGA model that my report detailed.

Here is a breakdown of the issue:

### 1. The Core Architectural Difference

My report explained that Firebird's Multi-Generational Architecture (MGA) provides the benefits of a HOT update *inherently*. It does this by keeping the physical location of the primary record stable.

- **Firebird's MGA (The Intended Model):** When a record is updated, the engine modifies the data *in-place* at its original location. The *previous* state of the record is copied into a "back version," which is stored elsewhere (ideally on the same page, or a different one if necessary). Because the primary record's location doesn't change, index entries do not need to be updated unless an indexed column itself was modified.  

- **PostgreSQL's Model (What ScratchBird is Imitating):** A standard `UPDATE` in PostgreSQL creates an entirely new version of the row (a new tuple) at a new physical location and marks the old one for cleanup. This requires updating all indexes to point to the new location. The HOT optimization was created to mitigate this by allowing the new tuple to be stored on the same page, thus avoiding the index updates.  

### 2. Isolating the Problem in Your Implementation

The code review comment pinpoints the exact deviation from the Firebird model: **"updateTuple() always creates new version on different page if needed."**

This is further supported by your specification document. The `sb_create_version` function appears to allocate new space for the updated record (`sb_allocate_record_space`) and then writes the new version there. It links back to the old version via a UUID.

This is the crucial mistake. Your implementation is creating a new version in a new location, which is the PostgreSQL way, not the Firebird way. By moving the *new* version instead of the *old* one, you lose the primary performance benefit of the MGA architecture. Every time an update occurs and the new version is placed on a different page, all indexes pointing to that record must be updated to reflect the new physical location.

This directly causes the impacts listed in the code review:

- **Index Bloat:** New index entries are created for every update, even when no indexed columns change.

- **Performance Degradation:** Modifying indexes is an expensive operation that significantly slows down updates.  

### Conclusion and Recommendation

The code review is essentially stating that your implementation has the architecture of Firebird's MGA but the performance characteristics of a non-optimized PostgreSQL `UPDATE`. You have implemented version chaining but have missed the critical in-place update logic that makes the MGA efficient.

To resolve this, the logic in `updateTuple()` (and its underlying functions like `sb_create_version`) needs to be reversed to align with the Firebird MGA specification:

1. When updating a record, first create a **back version** containing the *current* data. Store this back version on the same page if space allows, or on a new page if not.

2. Modify the record's data **in its original physical location** with the new values.

3. Update the header of this in-place record to point to the newly created back version.

By ensuring the primary record's location remains stable, you will eliminate the need for index updates on non-indexed columns, thereby resolving the performance issue and correctly implementing the core strength of the MGA model.
