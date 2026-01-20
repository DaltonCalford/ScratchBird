# ScratchBird Transaction and Lock Management - Main Specification

## Master Document for Transaction and Lock Management Implementation

---

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).
**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

## IMPLEMENTATION STATUS: COMPLETED

## **Current Alpha Implementation:**

- Basic 32-bit XID tracking only
- No MGA/MVCC implementation
- No distributed transactions
- No lock manager (uses std::mutex for page-level locking)
- No savepoints
- Single-threaded operation

**This specification describes the target architecture for Phase 2+.**

See `include/scratchbird/core/transaction_manager.h` for current basic implementation.

---

## Overview

ScratchBird's transaction and lock management system (PLANNED) will provide ACID guarantees through a Multi-Generational Architecture (MGA/MVCC) foundation, comprehensive locking mechanisms, and distributed transaction support. This document serves as the main specification, integrating all transaction management components.

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│           Application Layer (SQL/API)            │
└─────────────────────────────────────────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────┐
│         Transaction Manager Interface            │
├─────────────────────────────────────────────────┤
│  • Begin/Commit/Rollback                        │
│  • Savepoints                                   │
│  • Isolation Levels                             │
│  • Distributed Coordination                     │
└─────────────────────────────────────────────────┘
                        │
        ┌───────────────┼───────────────┐
        ▼               ▼               ▼
┌──────────────┐ ┌──────────────┐ ┌──────────────┐
│   MGA Core   │ │ Lock Manager │ │ Distributed  │
│              │ │              │ │   Manager    │
├──────────────┤ ├──────────────┤ ├──────────────┤
│ • 64-bit XID │ │ • Lock types │ │ • 2PC/3PC    │
│ • TIP pages  │ │ • Deadlock   │ │ • Raft       │
│ • Snapshots  │ │ • Predicate  │ │ • Recovery   │
│ • Version    │ │ • Advisory   │ │ • Partition  │
│   chains     │ │   locks      │ │   handling   │
│ • Garbage    │ │ • Monitor    │ │ • Global     │
│   collection │ │              │ │   snapshots  │
└──────────────┘ └──────────────┘ └──────────────┘
                        │
                        ▼
┌─────────────────────────────────────────────────┐
│              Storage Engine                      │
│        (Buffer Pool, Page Management)           │
└─────────────────────────────────────────────────┘
```

## Component Specifications

### 1. MGA Core

**Specification**: `TRANSACTION_MGA_CORE.md`

Key features:

- 64-bit transaction IDs (no wraparound)
- Transaction Inventory Pages (TIP) for state tracking
- MVCC snapshots for isolation
- Version chain management for updates
- Garbage collection without blocking readers
- Savepoints and nested transactions
- Two-phase commit for prepared transactions

### 2. Lock Manager

**Specification**: `TRANSACTION_LOCK_MANAGER.md`

Key features:

- Comprehensive lock types (relation, page, tuple, predicate)
- Multiple lock modes with compatibility matrix
- Deadlock detection (wait-for graph, wound-wait, wait-die)
- Predicate locks for true serializable isolation
- Advisory locks for application coordination
- Lock monitoring and statistics
- MGA optimizations reducing lock needs

### 3. Distributed Transactions

**Specification**: `TRANSACTION_DISTRIBUTED.md`

Key features:

- Multiple protocols (2PC, 3PC, Raft)
- Coordinator and participant recovery
- Network partition handling
- Read-only optimizations
- Batching and pipelining
- Global snapshots for consistency
- Heuristic decisions for failures

## 1. Transaction Manager Interface

### 1.1 Main Transaction Manager Structure

```c
// Master transaction manager
typedef struct sb_transaction_manager {
    // Component managers
    MGAManager*     tm_mga_manager;        // MGA core
    SBLockManager*  tm_lock_manager;       // Lock manager
    GlobalTransactionManager* tm_gtm;      // Distributed manager

    // Transaction tracking
    HashTable*      tm_active_transactions; // Active transactions
    HashTable*      tm_prepared_transactions; // Prepared (2PC)

    // Configuration
    TransactionConfig tm_config;           // Configuration

    // Statistics
    TransactionStats tm_stats;             // Global statistics

    // Background workers
    // NOTE: GC thread is owned by Database/GarbageCollector; see TRANSACTION_MGA_CORE.md.
    pthread_t       tm_monitor_thread;     // Monitor thread
    pthread_t       tm_recovery_thread;    // Recovery thread
} SBTransactionManager;

// Transaction configuration
typedef struct transaction_config {
    // MGA settings
    uint64_t        tc_max_transactions;   // Max concurrent transactions
    uint32_t        tc_tip_pages_per_segment; // TIP pages per segment

    // Lock settings
    uint32_t        tc_max_locks_per_xact; // Max locks per transaction
    uint32_t        tc_lock_timeout_ms;    // Lock timeout
    uint32_t        tc_deadlock_timeout_ms; // Deadlock check interval

    // Distributed settings
    bool            tc_enable_distributed; // Enable distributed transactions
    DTxProtocol     tc_default_protocol;   // Default protocol (2PC/3PC/Raft)
    uint32_t        tc_prepare_timeout_ms; // Prepare timeout

    // Garbage collection policy is specified in TRANSACTION_MGA_CORE.md
    // (GCPolicy + sweep interval + garbage_collection.* settings).
    // Glossary: FIREBIRD_GC_SWEEP_GLOSSARY.md

    // Performance
    bool            tc_enable_group_commit; // Group commit optimization
    uint32_t        tc_group_commit_delay_us; // Group commit delay
} TransactionConfig;
```

### 1.2 Transaction Operations

```c
// Begin transaction with options
SBTransaction* sb_begin_transaction_ex(
    SBTransactionManager* tm,
    TransactionOptions* options)
{
    // Allocate transaction structure
    SBTransaction* txn = allocate_transaction(tm);

    // Set options
    txn->txn_isolation = options->isolation_level ?: 
                        ISOLATION_READ_COMMITTED;
    txn->txn_read_only = options->read_only;
    txn->txn_deferrable = options->deferrable;

    // MGA: Allocate XID and take snapshot
    txn->txn_id = mga_allocate_xid(tm->tm_mga_manager);
    mga_take_snapshot(tm->tm_mga_manager, txn);

    // Initialize lock context
    txn->txn_lock_context = create_lock_context(tm->tm_lock_manager);

    // Distributed: Check if distributed
    if (options->distributed || options->nodes != NULL) {
        txn->txn_distributed = true;
        txn->txn_dtx = begin_distributed_transaction(
            tm->tm_gtm, options->nodes);
    }

    // Add to active transactions
    add_active_transaction(tm, txn);

    // Start monitoring
    start_transaction_monitoring(tm, txn);

    return txn;
}

// Commit transaction
Status sb_commit_transaction(
    SBTransactionManager* tm,
    SBTransaction* txn)
{
    Status status;

    // Pre-commit checks
    if (!pre_commit_checks(txn)) {
        return STATUS_CANNOT_COMMIT;
    }

    // Serializable: Check for conflicts
    if (txn->txn_isolation == ISOLATION_SERIALIZABLE) {
        if (!check_serializable_conflicts(tm->tm_lock_manager, txn)) {
            return STATUS_SERIALIZATION_FAILURE;
        }
    }

    // Distributed: Use appropriate protocol
    if (txn->txn_distributed) {
        switch (tm->tm_config.tc_default_protocol) {
            case DTX_PROTOCOL_2PC:
                status = commit_distributed_2pc(tm->tm_gtm, txn->txn_dtx);
                break;
            case DTX_PROTOCOL_3PC:
                status = commit_distributed_3pc(tm->tm_gtm, txn->txn_dtx);
                break;
            case DTX_PROTOCOL_RAFT:
                status = commit_distributed_raft(tm->tm_gtm, txn->txn_dtx);
                break;
        }

        if (status != STATUS_OK) {
            rollback_transaction_internal(tm, txn);
            return status;
        }
    }

    // Group commit optimization
    if (tm->tm_config.tc_enable_group_commit && !txn->txn_read_only) {
        add_to_group_commit(tm, txn);
        wait_for_group_commit(tm, txn);
    }

    // MGA: Update TIP
    mga_commit_transaction(tm->tm_mga_manager, txn);

    // Release all locks
    release_all_transaction_locks(tm->tm_lock_manager, txn);

    // Update statistics
    update_commit_statistics(tm, txn);

    // Remove from active
    remove_active_transaction(tm, txn);

    // Cleanup
    cleanup_transaction(txn);

    return STATUS_OK;
}

// Rollback transaction
Status sb_rollback_transaction(
    SBTransactionManager* tm,
    SBTransaction* txn)
{
    // Distributed: Rollback distributed
    if (txn->txn_distributed) {
        abort_distributed_transaction(tm->tm_gtm, txn->txn_dtx);
    }

    // MGA: Mark as aborted in TIP
    mga_abort_transaction(tm->tm_mga_manager, txn);

    // Release all locks
    release_all_transaction_locks(tm->tm_lock_manager, txn);

    // Update statistics
    update_rollback_statistics(tm, txn);

    // Remove from active
    remove_active_transaction(tm, txn);

    // Cleanup
    cleanup_transaction(txn);

    return STATUS_OK;
}
```

## 2. Isolation Level Implementation

### 2.1 Isolation Level Behaviors

```c
// Read Committed implementation
bool tuple_visible_read_committed(
    HeapTupleHeader tuple,
    SBTransaction* txn)
{
    // Take new snapshot for each statement
    TransactionSnapshot* snap = get_statement_snapshot(txn);

    return tuple_satisfies_snapshot(tuple, snap);
}

// Repeatable Read implementation (Snapshot Isolation)
bool tuple_visible_repeatable_read(
    HeapTupleHeader tuple,
    SBTransaction* txn)
{
    // Use transaction snapshot
    TransactionSnapshot* snap = txn->txn_snapshot;

    return tuple_satisfies_snapshot(tuple, snap);
}

// Serializable implementation
bool tuple_visible_serializable(
    HeapTupleHeader tuple,
    SBTransaction* txn,
    SBLockManager* lm)
{
    // Same visibility as repeatable read
    if (!tuple_visible_repeatable_read(tuple, txn)) {
        return false;
    }

    // But also track reads with predicate locks
    PredicateLockTag tag;
    tag.plt_relation_uuid = tuple->t_tableoid;
    tag.plt_type = PREDICATE_TUPLE;
    tag.plt_data.tuple.tid = tuple->t_ctid;

    acquire_predicate_lock(lm, &tag, txn->txn_id);

    return true;
}

// Check for serialization anomalies
bool check_serialization_anomalies(
    SBTransaction* txn,
    SBLockManager* lm)
{
    // Check for dangerous structures in predicate locks
    PredicateLockList* my_locks = get_transaction_predicate_locks(
        lm, txn->txn_id);

    for (PredicateLock* lock : my_locks) {
        // Look for rw-conflicts forming cycles
        if (has_dangerous_structure(lock, txn->txn_id)) {
            // Would create non-serializable history
            return false;
        }
    }

    return true;
}
```

## 3. Performance Optimizations

### 3.1 Group Commit

```c
// Group commit coordinator
typedef struct group_commit_coordinator {
    // Current group
    TransactionList* gc_current_group;     // Transactions in group
    uint32_t        gc_group_size;         // Current size
    uint32_t        gc_max_group_size;     // Maximum size

    // Timing
    TimestampTz     gc_group_start;        // Group start time
    uint32_t        gc_max_delay_us;       // Maximum delay

    // Synchronization
    pthread_mutex_t gc_mutex;              // Mutex
    pthread_cond_t  gc_commit_cv;          // Commit condition

    // Statistics
    uint64_t        gc_groups_committed;   // Groups committed
    uint64_t        gc_transactions_grouped; // Transactions grouped
} GroupCommitCoordinator;

// Add transaction to group commit
void add_to_group_commit(
    SBTransactionManager* tm,
    SBTransaction* txn)
{
    GroupCommitCoordinator* gc = tm->tm_group_commit;

    pthread_mutex_lock(&gc->gc_mutex);

    // Add to current group
    add_to_list(gc->gc_current_group, txn);
    gc->gc_group_size++;

    // Check if should commit group
    if (gc->gc_group_size >= gc->gc_max_group_size ||
        elapsed_time(gc->gc_group_start) >= gc->gc_max_delay_us) {

        // Commit the group
        commit_transaction_group(tm, gc->gc_current_group);

        // Reset for next group
        gc->gc_current_group = create_transaction_list();
        gc->gc_group_size = 0;
        gc->gc_group_start = GetCurrentTimestamp();

        // Wake up waiting transactions
        pthread_cond_broadcast(&gc->gc_commit_cv);

        gc->gc_groups_committed++;
    }

    pthread_mutex_unlock(&gc->gc_mutex);
}

// Commit a group of transactions together
void commit_transaction_group(
    SBTransactionManager* tm,
    TransactionList* group)
{
    // Phase 1: Prepare all transactions
    for (SBTransaction* txn : group) {
        prepare_for_commit(txn);
    }

    // Phase 2: Update TIP for all
    mga_batch_commit(tm->tm_mga_manager, group);

    // Phase 3: Flush optional post-gold write-after log (WAL) once for all
    flush_wal_for_group(group);

    // Phase 4: Release locks for all
    for (SBTransaction* txn : group) {
        release_all_transaction_locks(tm->tm_lock_manager, txn);
    }

    tm->tm_group_commit->gc_transactions_grouped += list_length(group);
}
```

### 3.2 Read-Only Optimizations

```c
// Optimized read-only transaction
typedef struct read_only_transaction {
    TransactionId   rot_xid;               // Transaction ID (may be virtual)
    TransactionSnapshot* rot_snapshot;     // Snapshot
    bool            rot_distributed;       // Is distributed
    GlobalSnapshot* rot_global_snapshot;   // Global snapshot
} ReadOnlyTransaction;

// Execute read-only transaction
Status execute_read_only_transaction(
    SBTransactionManager* tm,
    ReadOnlyTransaction* rot)
{
    // No locks needed with MGA
    // No TIP updates needed
    // No optional post-gold write-after log (WAL) needed

    if (rot->rot_distributed) {
        // Use global snapshot for distributed reads
        return execute_distributed_read_only(tm->tm_gtm, rot);
    }

    // Just use snapshot for visibility
    // Transaction can run without any coordination

    return STATUS_OK;
}

// Virtual transaction IDs for read-only
TransactionId allocate_virtual_xid(SBTransactionManager* tm) {
    // Use high bit to indicate virtual XID
    static _Atomic uint64_t virtual_xid_counter = 0x8000000000000000;

    return atomic_fetch_add(&virtual_xid_counter, 1);
}
```

## 4. Monitoring and Diagnostics

### 4.1 Transaction Monitoring

```c
// Transaction monitoring view
typedef struct transaction_view_entry {
    // Transaction info
    TransactionId   tv_xid;                // Transaction ID
    UUID            tv_uuid;               // Transaction UUID

    // State
    TransactionState tv_state;             // Current state
    IsolationLevel  tv_isolation;          // Isolation level
    bool            tv_read_only;          // Read-only flag

    // Timing
    TimestampTz     tv_start_time;         // Start time
    uint64_t        tv_duration_ms;        // Duration so far

    // Activity
    char            tv_current_query[1024]; // Current query
    char            tv_wait_event[64];     // Wait event

    // Resources
    uint32_t        tv_locks_held;         // Number of locks
    uint32_t        tv_pages_modified;     // Pages modified

    // Distributed
    bool            tv_distributed;        // Is distributed
    UUID            tv_coordinator;        // Coordinator node
} TransactionViewEntry;

SQL exposure:
- `sys.transactions` view (see `operations/MONITORING_SQL_VIEWS.md`)

// Get all transactions (for monitoring)
List* get_all_transactions(SBTransactionManager* tm) {
    List* result = NIL;

    pthread_mutex_lock(&tm->tm_mutex);

    for (SBTransaction* txn : tm->tm_active_transactions) {
        TransactionViewEntry* entry = create_view_entry(txn);
        result = lappend(result, entry);
    }

    pthread_mutex_unlock(&tm->tm_mutex);

    return result;
}

// Transaction statistics
typedef struct transaction_stats {
    // Commit/rollback
    uint64_t        ts_commits;            // Total commits
    uint64_t        ts_rollbacks;          // Total rollbacks
    double          ts_commit_ratio;       // Commit ratio

    // Performance
    uint64_t        ts_avg_duration_ms;    // Average duration
    uint64_t        ts_max_duration_ms;    // Maximum duration

    // Conflicts
    uint64_t        ts_serialization_failures; // Serialization failures
    uint64_t        ts_deadlocks;          // Deadlocks detected
    uint64_t        ts_lock_timeouts;      // Lock timeouts

    // Distributed
    uint64_t        ts_distributed_commits; // Distributed commits
    uint64_t        ts_distributed_aborts; // Distributed aborts
    uint64_t        ts_network_failures;   // Network failures

    // MGA
    uint64_t        ts_oldest_xid;         // Oldest XID
    uint64_t        ts_oldest_active_xid;  // Oldest active XID
    uint64_t        ts_gc_cycles;          // GC cycles run
    uint64_t        ts_tuples_vacuumed;    // Tuples swept/GC'ed
} TransactionStats;
```

## 5. Recovery and Consistency

### 5.1 Crash Recovery

```c
// Recovery manager
typedef struct recovery_manager {
    // Recovery state
    RecoveryState   rm_state;              // Current state
    LSN             rm_min_recovery_lsn;   // Minimum LSN to recover
    LSN             rm_recovery_target_lsn; // Target LSN

    // Prepared transactions
    PreparedTxnList* rm_prepared_local;    // Local prepared
    PreparedTxnList* rm_prepared_distributed; // Distributed prepared

    // Recovery progress
    uint64_t        rm_transactions_recovered; // Transactions recovered
    uint64_t        rm_transactions_aborted; // Transactions aborted
} RecoveryManager;

// Perform crash recovery
Status perform_crash_recovery(
    SBTransactionManager* tm)
{
    RecoveryManager* rm = tm->tm_recovery_manager;

    // Phase 1: Read prepared transaction logs
    rm->rm_prepared_local = read_prepared_transactions();
    rm->rm_prepared_distributed = read_distributed_prepared();

    // Phase 2: Recover MGA state
    recover_mga_state(tm->tm_mga_manager);

    // Phase 3: Recover prepared transactions
    for (PreparedTxn* ptx : rm->rm_prepared_local) {
        if (ptx->distributed) {
            // Query coordinator for decision
            recover_distributed_participant(tm->tm_gtm, ptx);
        } else {
            // Local prepared - can abort
            abort_prepared_transaction(tm, ptx);
        }
    }

    // Phase 4: Recover distributed coordinator role
    for (PreparedTxn* ptx : rm->rm_prepared_distributed) {
        recover_distributed_coordinator(tm->tm_gtm, ptx);
    }

    // Phase 5: Clean up incomplete transactions
    cleanup_incomplete_transactions(tm);

    // Phase 6: Start background processes
    // GC start is managed by Database/GarbageCollector policy (see TRANSACTION_MGA_CORE.md)
    start_garbage_collector(get_database(tm));
    start_deadlock_detector(tm->tm_lock_manager);

    return STATUS_OK;
}
```

## 6. Integration Examples

### 6.1 Query Execution Integration

```c
// Execute query with transaction management
QueryResult* execute_query_with_transaction(
    SBTransactionManager* tm,
    Query* query,
    TransactionOptions* options)
{
    // Start transaction if needed
    SBTransaction* txn = get_current_transaction();
    bool started_transaction = false;

    if (txn == NULL) {
        txn = sb_begin_transaction_ex(tm, options);
        started_transaction = true;
    }

    QueryResult* result = NULL;

    TRY {
        // Set snapshot for query
        set_query_snapshot(query, txn->txn_snapshot);

        // Execute query
        result = execute_query(query);

        // Auto-commit if we started transaction
        if (started_transaction && !query->is_select) {
            sb_commit_transaction(tm, txn);
        }
    }
    CATCH {
        // Rollback on error
        if (started_transaction) {
            sb_rollback_transaction(tm, txn);
        }
        RETHROW;
    }

    return result;
}
```

### 6.2 Storage Engine Integration

```c
// Storage operation with transaction context
Status storage_insert_tuple(
    StorageEngine* storage,
    Relation rel,
    HeapTuple tuple)
{
    // Get current transaction
    SBTransaction* txn = get_current_transaction();

    if (txn == NULL) {
        return STATUS_NO_TRANSACTION;
    }

    // Set transaction info in tuple
    tuple->t_data->t_xmin = txn->txn_id;
    tuple->t_data->t_cmin = txn->txn_command_id;
    tuple->t_data->t_xmax = InvalidTransactionId;

    // Acquire necessary locks
    LockTag tag;
    tag.lt_lock_type = LOCK_TYPE_RELATION;
    tag.lt_object_uuid = rel->rd_uuid;

    LockResult lock_result = acquire_lock(
        txn->txn_tm->tm_lock_manager,
        &tag,
        ROW_EXCLUSIVE,
        true,
        false);

    if (lock_result != LOCK_OK) {
        return STATUS_LOCK_FAILED;
    }

    // Insert tuple
    ItemPointer tid = heap_insert(storage, rel, tuple);

    // Track in transaction
    add_inserted_tuple(txn, tid);

    return STATUS_OK;
}
```

### Conclusion

The ScratchBird transaction and lock management system provides:

- **MGA/MVCC** for high concurrency without read locks
- **64-bit transaction IDs** eliminating wraparound concerns
- **Comprehensive locking** with minimal overhead due to MGA
- **True serializability** through predicate locking
- **Distributed transactions** with multiple protocols
- **Robust recovery** from failures and network partitions
- **Performance optimizations** including group commit and read-only optimizations

The system combines the best aspects of:

- **Firebird's** MGA architecture and lightweight locking
- **PostgreSQL's** comprehensive lock types and monitoring
- **Modern** distributed protocols and consensus algorithms

This design ensures ACID compliance while maintaining high performance and scalability for both single-node and distributed deployments.
