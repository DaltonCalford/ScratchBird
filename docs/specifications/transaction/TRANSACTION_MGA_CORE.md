# ScratchBird Transaction Management - MGA Core
## Part 1 of Transaction and Lock Management Specification

## Overview

ScratchBird's transaction system is built on Multi-Generational Architecture (MGA/MVCC) inherited from Firebird, enhanced with 64-bit transaction IDs, PostgreSQL-style predicate locking for true serializability, and built-in support for distributed transactions. This specification details the core MGA implementation.

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).
**WAL Scope:** ScratchBird does not use write-after log (WAL) for recovery in Alpha; any WAL support is optional post-gold (replication/PITR).
Any WAL references in this document describe an optional post-gold stream for
replication/PITR only.

## 1. Transaction ID Management

### 1.1 Transaction ID Structure

```c
// 64-bit transaction IDs - no wraparound issues
typedef uint64_t TransactionId;

// Special transaction IDs
#define InvalidTransactionId        ((TransactionId) 0)
#define BootstrapTransactionId      ((TransactionId) 1)
#define FrozenTransactionId         ((TransactionId) 2)
#define FirstNormalTransactionId    ((TransactionId) 3)
#define MaxTransactionId            ((TransactionId) 0xFFFFFFFFFFFFFFFF)

// Transaction ID generation block
typedef struct transaction_id_generator {
    // Current state
    TransactionId   tig_next_xid;          // Next XID to assign
    TransactionId   tig_oldest_xid;        // Oldest XID still running
    TransactionId   tig_oldest_active;     // Oldest active XID (OAT)
    TransactionId   tig_oldest_snapshot;   // Oldest snapshot XID (OST)
    
    // UUID support for distributed
    UUID            tig_node_uuid;         // Node UUID for distributed XIDs
    uint16_t        tig_node_id;           // Node ID component
    
    // Synchronization
    pthread_mutex_t tig_mutex;             // Mutex for XID generation
    
    // Statistics
    uint64_t        tig_xids_generated;    // Total XIDs generated
    uint64_t        tig_xids_committed;    // XIDs committed
    uint64_t        tig_xids_aborted;      // XIDs aborted
} TransactionIdGenerator;

// Generate new transaction ID
TransactionId allocate_transaction_id(
    TransactionIdGenerator* gen)
{
    pthread_mutex_lock(&gen->tig_mutex);
    
    TransactionId xid = gen->tig_next_xid++;
    
    // With 64-bit IDs, wraparound would take centuries
    // But still check for safety
    if (xid >= MaxTransactionId - 1000000) {
        pthread_mutex_unlock(&gen->tig_mutex);
        elog(PANIC, "Transaction ID space nearly exhausted");
    }
    
    gen->tig_xids_generated++;
    
    pthread_mutex_unlock(&gen->tig_mutex);
    
    // Extend TIP if needed
    if (xid % XIDS_PER_TIP_PAGE == 0) {
        extend_tip_pages(xid);
    }
    
    return xid;
}

// Transaction ID with distributed support
typedef struct distributed_xid {
    TransactionId   dxid_local;            // Local transaction ID
    UUID            dxid_global;           // Global transaction UUID
    uint16_t        dxid_node_id;          // Originating node
    uint32_t        dxid_sequence;         // Sequence within node
} DistributedXid;
```

### 1.2 Transaction Inventory Pages (TIP)

```c
// TIP page size depends on database page size
#define XIDS_PER_TIP_PAGE(page_size) \
    ((page_size - sizeof(TIPPageHeader)) * 4)

// Transaction states in TIP (2 bits per transaction)
typedef enum transaction_state {
    TXN_STATE_ACTIVE = 0,       // Transaction active
    TXN_STATE_COMMITTED = 1,    // Transaction committed
    TXN_STATE_ABORTED = 2,      // Transaction aborted
    TXN_STATE_LIMBO = 3         // Two-phase commit limbo
} TransactionState;

// TIP page header
typedef struct tip_page_header {
    SBPageHeader    tph_header;            // Standard page header
    TransactionId   tph_oldest_xid;        // Oldest XID on this page
    TransactionId   tph_newest_xid;        // Newest XID on this page
    uint32_t        tph_n_transactions;    // Number of transactions
    
    // Statistics
    uint32_t        tph_n_committed;       // Committed count
    uint32_t        tph_n_aborted;         // Aborted count
    uint32_t        tph_n_limbo;           // Limbo count
} TIPPageHeader;

// TIP page structure
typedef struct tip_page {
    TIPPageHeader   tip_header;            // Page header
    uint8_t         tip_bits[FLEXIBLE_ARRAY_MEMBER]; // Transaction bits
} TIPPage;

// Get transaction state from TIP
TransactionState get_transaction_state(
    TransactionId xid)
{
    // Calculate TIP page number
    uint32_t page_size = get_current_page_size();
    uint32_t xids_per_page = XIDS_PER_TIP_PAGE(page_size);
    BlockNumber tip_page_num = xid / xids_per_page;
    uint32_t offset_in_page = xid % xids_per_page;
    
    // Read TIP page
    TIPPage* tip_page = read_tip_page(tip_page_num);
    
    // Extract 2-bit state
    uint32_t byte_offset = offset_in_page / 4;
    uint32_t bit_offset = (offset_in_page % 4) * 2;
    uint8_t byte = tip_page->tip_bits[byte_offset];
    
    TransactionState state = (byte >> bit_offset) & 0x03;
    
    release_tip_page(tip_page);
    
    return state;
}

// Set transaction state in TIP
void set_transaction_state(
    TransactionId xid,
    TransactionState new_state)
{
    // Calculate TIP page location
    uint32_t page_size = get_current_page_size();
    uint32_t xids_per_page = XIDS_PER_TIP_PAGE(page_size);
    BlockNumber tip_page_num = xid / xids_per_page;
    uint32_t offset_in_page = xid % xids_per_page;
    
    // Get TIP page for update
    TIPPage* tip_page = get_tip_page_for_update(tip_page_num);
    
    // Update 2-bit state
    uint32_t byte_offset = offset_in_page / 4;
    uint32_t bit_offset = (offset_in_page % 4) * 2;
    uint8_t* byte_ptr = &tip_page->tip_bits[byte_offset];
    
    // Clear old state and set new
    *byte_ptr = (*byte_ptr & ~(0x03 << bit_offset)) | 
                (new_state << bit_offset);
    
    // Update statistics
    update_tip_statistics(tip_page, xid, new_state);
    
    // Mark page dirty
    mark_tip_page_dirty(tip_page);
    
    release_tip_page(tip_page);
}
```

## 2. Transaction Structure and Management

### 2.1 Transaction Descriptor

```c
// Main transaction structure
typedef struct sb_transaction {
    // Identity
    TransactionId   txn_id;                // Transaction ID
    UUID            txn_uuid;              // Transaction UUID (for distributed)
    
    // State
    TransactionState txn_state;            // Current state
    IsolationLevel  txn_isolation;         // Isolation level
    bool            txn_read_only;         // Read-only transaction
    bool            txn_deferrable;        // Deferrable (for serializable)
    
    // Snapshot data (for MVCC)
    TransactionId   txn_snapshot_xmin;     // Lowest XID to consider
    TransactionId   txn_snapshot_xmax;     // Highest XID + 1 to consider
    TransactionId*  txn_snapshot_xip;      // Array of active XIDs
    uint32_t        txn_snapshot_xcnt;     // Count of active XIDs
    CommandId       txn_command_id;        // Current command ID
    
    // Timestamps
    TimestampTz     txn_start_time;        // Start timestamp
    TimestampTz     txn_commit_time;       // Commit timestamp (if committed)
    
    // Lock management
    LockList*       txn_locks;             // Held locks
    PredicateLockList* txn_predicate_locks; // Predicate locks (serializable)
    
    // Modified data tracking
    DirtyPageList*  txn_dirty_pages;       // Modified pages
    TupleList*      txn_inserted_tuples;   // Inserted tuples
    TupleList*      txn_deleted_tuples;    // Deleted tuples
    
    // Savepoints
    SavepointStack* txn_savepoints;        // Savepoint stack
    uint32_t        txn_savepoint_level;   // Current savepoint level
    
    // Two-phase commit
    bool            txn_prepared;          // In prepared state
    char            txn_gid[200];          // Global transaction ID
    
    // Distributed transaction
    bool            txn_distributed;       // Is distributed
    NodeList*       txn_participants;      // Participant nodes
    
    // Statistics
    uint64_t        txn_tuples_inserted;   // Tuples inserted
    uint64_t        txn_tuples_updated;    // Tuples updated
    uint64_t        txn_tuples_deleted;    // Tuples deleted
    uint64_t        txn_pages_read;        // Pages read
    uint64_t        txn_pages_written;     // Pages written
} SBTransaction;

// Isolation levels
typedef enum isolation_level {
    ISOLATION_READ_COMMITTED,       // Default
    ISOLATION_REPEATABLE_READ,      // Snapshot isolation
    ISOLATION_SERIALIZABLE,         // True serializability
    ISOLATION_READ_UNCOMMITTED      // For compatibility only
} IsolationLevel;
```

### 2.2 Transaction Operations

```c
// Begin transaction
SBTransaction* begin_transaction(
    IsolationLevel isolation,
    bool read_only,
    bool deferrable)
{
    // Allocate transaction structure
    SBTransaction* txn = allocate(sizeof(SBTransaction));
    
    // Generate transaction ID
    txn->txn_id = allocate_transaction_id(get_xid_generator());
    txn->txn_uuid = generate_uuid_v7();
    
    // Set properties
    txn->txn_isolation = isolation;
    txn->txn_read_only = read_only;
    txn->txn_deferrable = deferrable;
    txn->txn_state = TXN_STATE_ACTIVE;
    
    // Take snapshot
    take_transaction_snapshot(txn);
    
    // Initialize lists
    txn->txn_locks = create_lock_list();
    txn->txn_dirty_pages = create_dirty_page_list();
    
    // Register in TIP
    set_transaction_state(txn->txn_id, TXN_STATE_ACTIVE);
    
    // Add to active transaction list
    add_to_active_transactions(txn);
    
    // Set start time
    txn->txn_start_time = GetCurrentTimestamp();
    
    return txn;
}

// Commit transaction
Status commit_transaction(SBTransaction* txn) {
    // Check if can commit
    if (txn->txn_state != TXN_STATE_ACTIVE &&
        txn->txn_state != TXN_STATE_PREPARED) {
        return STATUS_INVALID_TRANSACTION_STATE;
    }
    
    // For serializable, check for conflicts
    if (txn->txn_isolation == ISOLATION_SERIALIZABLE) {
        if (!check_serializable_conflicts(txn)) {
            return STATUS_SERIALIZATION_FAILURE;
        }
    }
    
    // Phase 1: Pre-commit (for 2PC)
    if (txn->txn_distributed) {
        if (!prepare_distributed_commit(txn)) {
            return STATUS_PREPARE_FAILED;
        }
    }
    
    // Phase 2: Update TIP
    set_transaction_state(txn->txn_id, TXN_STATE_COMMITTED);
    
    // Phase 3: Release locks
    release_all_locks(txn);
    
    // Phase 4: Flush dirty pages if needed
    if (!txn->txn_read_only) {
        flush_transaction_pages(txn);
    }
    
    // Phase 5: Update statistics
    update_transaction_statistics(txn);
    
    // Phase 6: Remove from active list
    remove_from_active_transactions(txn);
    
    // Set commit time
    txn->txn_commit_time = GetCurrentTimestamp();
    txn->txn_state = TXN_STATE_COMMITTED;
    
    // Cleanup
    cleanup_transaction(txn);
    
    return STATUS_OK;
}

// Rollback transaction
Status rollback_transaction(SBTransaction* txn) {
    // Update TIP
    set_transaction_state(txn->txn_id, TXN_STATE_ABORTED);
    
    // Release all locks
    release_all_locks(txn);
    
    // No undo needed with MGA - old versions still exist
    // Just mark our changes as aborted
    
    // Remove from active list
    remove_from_active_transactions(txn);
    
    txn->txn_state = TXN_STATE_ABORTED;
    
    // Cleanup
    cleanup_transaction(txn);
    
    return STATUS_OK;
}
```

## 3. Snapshot Management (MVCC)

### 3.1 Snapshot Structure

```c
// Transaction snapshot for MVCC visibility
typedef struct transaction_snapshot {
    // Snapshot bounds
    TransactionId   snap_xmin;             // Lowest XID to consider
    TransactionId   snap_xmax;             // Highest XID + 1 to consider
    
    // Active transactions at snapshot time
    TransactionId*  snap_xip;              // Array of active XIDs
    uint32_t        snap_xcnt;             // Count of active XIDs
    
    // Snapshot metadata
    TimestampTz     snap_timestamp;        // When snapshot taken
    LSN             snap_lsn;              // LSN at snapshot
    
    // For serializable isolation
    bool            snap_serializable;     // Is serializable snapshot
    PredicateLockTarget* snap_predicate_locks; // Predicate lock targets
    
    // Reference counting
    uint32_t        snap_refcount;         // Reference count
} TransactionSnapshot;

// Take transaction snapshot
void take_transaction_snapshot(SBTransaction* txn) {
    TransactionSnapshot* snap = allocate(sizeof(TransactionSnapshot));
    
    // Get current transaction state
    TransactionIdGenerator* gen = get_xid_generator();
    
    pthread_mutex_lock(&gen->tig_mutex);
    
    snap->snap_xmax = gen->tig_next_xid;
    snap->snap_xmin = gen->tig_oldest_xid;
    
    // Collect active transactions
    ActiveTransactionList* active = get_active_transactions();
    snap->snap_xcnt = active->count;
    snap->snap_xip = allocate(sizeof(TransactionId) * snap->snap_xcnt);
    
    uint32_t idx = 0;
    for (SBTransaction* active_txn : active->transactions) {
        if (active_txn->txn_id < snap->snap_xmax &&
            active_txn->txn_id >= snap->snap_xmin) {
            snap->snap_xip[idx++] = active_txn->txn_id;
        }
    }
    snap->snap_xcnt = idx;
    
    pthread_mutex_unlock(&gen->tig_mutex);
    
    // Sort active XIDs for binary search
    qsort(snap->snap_xip, snap->snap_xcnt, 
          sizeof(TransactionId), compare_xids);
    
    // Set metadata
    snap->snap_timestamp = GetCurrentTimestamp();
    snap->snap_lsn = GetCurrentLSN();
    snap->snap_serializable = (txn->txn_isolation == ISOLATION_SERIALIZABLE);
    
    // Assign to transaction
    txn->txn_snapshot_xmin = snap->snap_xmin;
    txn->txn_snapshot_xmax = snap->snap_xmax;
    txn->txn_snapshot_xip = snap->snap_xip;
    txn->txn_snapshot_xcnt = snap->snap_xcnt;
}

// Check if XID is visible in snapshot
bool xid_visible_in_snapshot(
    TransactionId xid,
    TransactionSnapshot* snap)
{
    // XIDs before snapshot are visible if committed
    if (xid < snap->snap_xmin) {
        return get_transaction_state(xid) == TXN_STATE_COMMITTED;
    }
    
    // XIDs after snapshot are not visible
    if (xid >= snap->snap_xmax) {
        return false;
    }
    
    // Check if XID was active at snapshot time
    if (binary_search(snap->snap_xip, snap->snap_xcnt, xid)) {
        return false;  // Was active, not visible
    }
    
    // Not active at snapshot - check if committed
    return get_transaction_state(xid) == TXN_STATE_COMMITTED;
}
```

### 3.2 Tuple Visibility

```c
// Check tuple visibility for MVCC
bool tuple_satisfies_mvcc(
    HeapTupleHeader tuple,
    TransactionSnapshot* snap,
    Buffer buffer)
{
    TransactionId xmin = tuple->t_xmin;
    TransactionId xmax = tuple->t_xmax;
    
    // Check insert visibility
    if (xmin == InvalidTransactionId) {
        return false;  // Invalid tuple
    }
    
    // Our own transaction's changes are always visible
    if (TransactionIdIsCurrentTransactionId(xmin)) {
        if (tuple->t_infomask & HEAP_XMIN_ABORTED) {
            return false;  // We aborted this insert
        }
        
        // Check if deleted by us
        if (TransactionIdIsValid(xmax)) {
            if (TransactionIdIsCurrentTransactionId(xmax)) {
                return false;  // We deleted it
            }
        }
        
        return true;  // Our insert, not deleted by us
    }
    
    // Check if insert is visible
    if (!xid_visible_in_snapshot(xmin, snap)) {
        return false;  // Insert not visible
    }
    
    // Insert is visible - check if deleted
    if (!TransactionIdIsValid(xmax)) {
        return true;  // Not deleted
    }
    
    // Check delete visibility
    if (TransactionIdIsCurrentTransactionId(xmax)) {
        return false;  // Deleted by us
    }
    
    if (xid_visible_in_snapshot(xmax, snap)) {
        return false;  // Delete is visible
    }
    
    return true;  // Delete not visible, tuple is visible
}

// Visibility check with hint bits optimization
bool heap_tuple_satisfies_snapshot(
    HeapTuple tuple,
    TransactionSnapshot* snap,
    Buffer buffer)
{
    HeapTupleHeader header = tuple->t_data;
    
    // Fast path: check hint bits
    if (header->t_infomask & HEAP_XMIN_COMMITTED) {
        // Insert definitely committed
        if (header->t_infomask & HEAP_XMAX_INVALID) {
            return true;  // Not deleted
        }
        
        if (header->t_infomask & HEAP_XMAX_COMMITTED) {
            // Delete also committed - check against snapshot
            if (header->t_xmax < snap->snap_xmin) {
                return false;  // Deleted before snapshot
            }
        }
    } else if (header->t_infomask & HEAP_XMIN_ABORTED) {
        return false;  // Insert aborted
    }
    
    // Slow path: full visibility check
    return tuple_satisfies_mvcc(header, snap, buffer);
}
```

## 4. Version Chain Management

### 4.1 Version Chain Structure

```c
// Version chain for update chains
typedef struct version_chain {
    ItemPointer     vc_tid;                // Tuple ID
    TransactionId   vc_xmin;               // Creating XID
    TransactionId   vc_xmax;               // Deleting XID
    CommandId       vc_cmin;               // Creating command
    CommandId       vc_cmax;               // Deleting command
    ItemPointer     vc_next;               // Next in chain
    ItemPointer     vc_prev;               // Previous in chain
} VersionChain;

// Create new version for update
ItemPointer create_new_version(
    Relation rel,
    ItemPointer old_tid,
    HeapTuple new_tuple,
    TransactionId xid,
    CommandId cid)
{
    // Get old tuple
    HeapTuple old_tuple = heap_fetch(rel, old_tid);
    HeapTupleHeader old_header = old_tuple->t_data;
    
    // Mark old version as deleted by us
    old_header->t_xmax = xid;
    old_header->t_cmax = cid;
    old_header->t_infomask |= HEAP_XMAX_VALID;
    
    // Set new version's predecessor
    HeapTupleHeader new_header = new_tuple->t_data;
    new_header->t_xmin = xid;
    new_header->t_cmin = cid;
    new_header->t_xmax = InvalidTransactionId;
    new_header->t_ctid = *old_tid;  // Points to old version
    
    // Insert new version
    ItemPointer new_tid = heap_insert(rel, new_tuple);
    
    // Update old version's forward pointer
    old_header->t_ctid = *new_tid;
    
    // Mark buffer dirty
    mark_buffer_dirty(old_tuple->t_buffer);
    
    return new_tid;
}

// Follow version chain to find visible version
HeapTuple follow_version_chain(
    Relation rel,
    ItemPointer tid,
    TransactionSnapshot* snap)
{
    ItemPointer current_tid = tid;
    
    while (ItemPointerIsValid(current_tid)) {
        HeapTuple tuple = heap_fetch(rel, current_tid);
        
        if (tuple == NULL) {
            return NULL;  // Chain broken
        }
        
        // Check if this version is visible
        if (heap_tuple_satisfies_snapshot(tuple, snap, tuple->t_buffer)) {
            return tuple;  // Found visible version
        }
        
        HeapTupleHeader header = tuple->t_data;
        
        // Move to next version in chain
        if (ItemPointerEquals(&header->t_ctid, current_tid)) {
            // End of chain
            break;
        }
        
        current_tid = &header->t_ctid;
        
        // Release previous version
        heap_release_fetch(tuple);
    }
    
    return NULL;  // No visible version found
}
```

## 5. Garbage Collection and Sweep (Firebird MGA)

### 5.1 Design Model (Firebird)

Firebird MGA does **not** use PostgreSQL-style VACUUM phases. Garbage collection removes
obsolete **back versions** once they are older than OIT and committed; primary records stay
stable. GC is split into:

- **Cooperative GC** on record access
- **Background GC thread** (policy-controlled)
- **Sweep** (database-wide pass)

Firebird reference points:
- Sweep pass: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/vio.cpp:4735-4842`
- GC thread: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/vio.cpp:5640-5865`
- GC page notification: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/vio.cpp:6430-6489`
- Sweep trigger: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/tra.cpp:3796-3802`
- GC policy: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/jrd.cpp:7679-7690`

Glossary: `ScratchBird/docs/specifications/transaction/FIREBIRD_GC_SWEEP_GLOSSARY.md`

### 5.2 MGA Visibility Markers

- **OIT** (Oldest Interesting Transaction) bounds the oldest version that might still be visible.
- **OAT** (Oldest Active Transaction) identifies the oldest still-running transaction.
- **OST** (Oldest Snapshot Transaction) tracks the oldest snapshot still in use.

A record version is garbage if:
- it was deleted/updated by a committed transaction, and
- its `xmax` is **older than OIT**, and
- no active snapshot can see it.

### 5.3 Cooperative GC (Record Access)

```c
bool can_gc(RecordVersion* back, TxnMarkers* m)
{
    if (back->xmax == INVALID_XID) return false;
    if (!back->xmax_committed) return false;
    if (back->xmax >= m->oit) return false;
    return true;
}

void gc_on_access(Record* primary, TxnMarkers* m)
{
    RecordVersion* back = primary->back_version;
    while (back && can_gc(back, m)) {
        unlink_back_version(primary, back);
        remove_index_entries(back);
        remove_blob_versions(back);
        free_back_version(back);
        back = primary->back_version;
    }
}
```

### 5.4 Candidate Page Tracking (Firebird Model)

Firebird tracks GC candidates with a **per-relation GC bitmap** keyed by data page sequence.
When a record access discovers GC candidates, it calls `notify_garbage_collector`, which:
- marks the relation bitmap for the page,
- sets `gc_pending`,
- optionally wakes the GC thread if the oldest snapshot advanced.

Reference: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/vio.cpp:6430-6489`.

### 5.5 Background GC Thread (Firebird Pattern)

The GC thread runs only when policy includes background GC. It:
- attaches as a dedicated "Garbage Collector" attachment,
- uses a **read-only, read committed, no-lock** transaction,
- scans relation GC bitmaps for candidate pages,
- iterates candidate pages with `VIO_next_record` to drive per-record GC,
- refreshes `tra_oldest` / `tra_oldest_active` to keep GC decisions current,
- flushes pages opportunistically after GC.

Reference: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/vio.cpp:5640-5865`.

### 5.6 Sweep (Database-wide Pass)

Sweep is a forced GC pass, triggered when `(OST - OIT) > sweep_interval`. It:
- acquires a single **sweep lock**,
- iterates all non-temporary relations,
- scans all records with sweeper flags to force GC,
- does **not** remove primary records (only back versions).

References:
- Trigger: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/tra.cpp:3796-3802`
- Locking: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/Database.cpp:245-350`
- Sweep scan: `Firebird-6.0.0.1124-1ccdf1c-source/src/jrd/vio.cpp:4791-4837`

### 5.7 ScratchBird Canonical Behavior (Aligned to Firebird)

- GC removes **back versions only**, never primary record slots.
- GC eligibility is based on OIT and committed `xmax`.
- Index and blob/TOAST entries for removed back versions must be cleaned.
- Cooperative GC may run on record/page access and DML paths.
- Background GC is policy-controlled (cooperative/background/combined).
- Sweep is the database-wide pass to force GC; it should not introduce
  PostgreSQL VACUUM phases (FSM/VM, relation stats, or page truncation).

### 5.8 Deviations from Firebird 6.0 (Explicit)

- **Candidate tracking**: Firebird uses relation GC bitmaps and a wake semaphore
  (`vio.cpp:6430-6489`). ScratchBird uses a global dirty-page map keyed by page_id
  and marks pages on DML (`ScratchBird/src/core/storage_engine.cpp:1000-1004`).
- **Background GC attachment**: Firebird GC runs as a dedicated attachment with a
  precommitted read-only transaction (`vio.cpp:5640-5777`). ScratchBird's GC loop
  operates without an explicit transaction context and reads OIT directly
  (`ScratchBird/src/core/garbage_collector.cpp:269-399`).
- **Sweep**: Firebird starts a sweeper thread based on sweep interval and enforces
  a single sweep via the sweep lock (`tra.cpp:3796-3802`, `Database.cpp:245-350`).
  ScratchBird's `SweepManager` advances OIT and notifies GC but does not reclaim
  space yet (`ScratchBird/src/core/sweep_manager.cpp:215-228`).
- **VACUUM phases**: Firebird does not implement PostgreSQL-style VACUUM phases.
  ScratchBird currently has a `Vacuum` utility that performs heap scans and
  pruning plus a `freezeTable` helper (`ScratchBird/src/core/vacuum.cpp:40-107`,
  `ScratchBird/src/core/vacuum.cpp:597-703`). These are non-Firebird extensions
  and must obey MGA rules (no primary record removal).
- **Policy/config**: Firebird uses GC policy from config and sweep interval stored
  in the database header (`jrd.cpp:7679-7690`, `ods.h:575`). ScratchBird uses
  `garbage_collection.*` config keys for policy/interval/rate.

## 6. Savepoints and Nested Transactions

### 6.1 Savepoint Management

```c
// Savepoint structure
typedef struct savepoint {
    char            sp_name[NAMEDATALEN];  // Savepoint name
    uint32_t        sp_level;              // Nesting level
    TransactionId   sp_xid;                // Transaction ID
    CommandId       sp_cid;                // Command ID at savepoint
    
    // State to restore
    TransactionSnapshot* sp_snapshot;      // Snapshot at savepoint
    LockList*       sp_locks;              // Locks at savepoint
    TupleList*      sp_inserted;           // Tuples inserted after
    TupleList*      sp_deleted;            // Tuples deleted after
    
    // Chain
    struct savepoint* sp_parent;           // Parent savepoint
} Savepoint;

// Create savepoint
Savepoint* create_savepoint(
    SBTransaction* txn,
    const char* name)
{
    Savepoint* sp = allocate(sizeof(Savepoint));
    
    strncpy(sp->sp_name, name, NAMEDATALEN);
    sp->sp_level = txn->txn_savepoint_level + 1;
    sp->sp_xid = txn->txn_id;
    sp->sp_cid = txn->txn_command_id;
    
    // Save current state
    sp->sp_snapshot = copy_snapshot(txn->txn_snapshot);
    sp->sp_locks = copy_lock_list(txn->txn_locks);
    sp->sp_inserted = create_tuple_list();
    sp->sp_deleted = create_tuple_list();
    
    // Link to parent
    sp->sp_parent = txn->txn_savepoints;
    txn->txn_savepoints = sp;
    txn->txn_savepoint_level++;
    
    return sp;
}

// Rollback to savepoint
Status rollback_to_savepoint(
    SBTransaction* txn,
    const char* name)
{
    Savepoint* sp = txn->txn_savepoints;
    
    // Find named savepoint
    while (sp != NULL && strcmp(sp->sp_name, name) != 0) {
        sp = sp->sp_parent;
    }
    
    if (sp == NULL) {
        return STATUS_SAVEPOINT_NOT_FOUND;
    }
    
    // Rollback changes made after savepoint
    Savepoint* current = txn->txn_savepoints;
    
    while (current != sp) {
        // Mark inserted tuples as aborted
        for (ItemPointer tid : current->sp_inserted) {
            HeapTuple tuple = heap_fetch_for_update(tid);
            tuple->t_data->t_infomask |= HEAP_XMIN_ABORTED;
            heap_release_fetch(tuple);
        }
        
        // Clear delete marks
        for (ItemPointer tid : current->sp_deleted) {
            HeapTuple tuple = heap_fetch_for_update(tid);
            tuple->t_data->t_xmax = InvalidTransactionId;
            tuple->t_data->t_infomask &= ~HEAP_XMAX_VALID;
            heap_release_fetch(tuple);
        }
        
        // Release locks acquired after savepoint
        release_locks_after_savepoint(txn, current);
        
        Savepoint* next = current->sp_parent;
        free_savepoint(current);
        current = next;
    }
    
    // Restore state
    txn->txn_savepoints = sp;
    txn->txn_savepoint_level = sp->sp_level;
    txn->txn_command_id = sp->sp_cid;
    
    return STATUS_OK;
}

// Release savepoint
Status release_savepoint(
    SBTransaction* txn,
    const char* name)
{
    Savepoint* sp = txn->txn_savepoints;
    Savepoint* prev = NULL;
    
    // Find named savepoint
    while (sp != NULL && strcmp(sp->sp_name, name) != 0) {
        prev = sp;
        sp = sp->sp_parent;
    }
    
    if (sp == NULL) {
        return STATUS_SAVEPOINT_NOT_FOUND;
    }
    
    // Remove from chain
    if (prev != NULL) {
        prev->sp_parent = sp->sp_parent;
    } else {
        txn->txn_savepoints = sp->sp_parent;
    }
    
    // Merge changes into parent
    if (prev != NULL) {
        merge_tuple_lists(prev->sp_inserted, sp->sp_inserted);
        merge_tuple_lists(prev->sp_deleted, sp->sp_deleted);
    }
    
    free_savepoint(sp);
    txn->txn_savepoint_level--;
    
    return STATUS_OK;
}
```

## 7. Two-Phase Commit (2PC)

### 7.1 Prepared Transaction Management

```c
// Prepared transaction state
typedef struct prepared_transaction {
    // Identity
    TransactionId   pt_xid;                // Transaction ID
    char            pt_gid[200];           // Global transaction ID
    
    // State
    TimestampTz     pt_prepared_at;        // When prepared
    UUID            pt_database_uuid;      // Database UUID
    
    // Participants (for coordinator)
    NodeList*       pt_participants;       // Participant nodes
    uint32_t        pt_n_participants;     // Number of participants
    uint32_t        pt_n_prepared;         // Number prepared
    uint32_t        pt_n_committed;        // Number committed
    
    // Data to commit
    TupleList*      pt_inserted;           // Inserted tuples
    TupleList*      pt_deleted;            // Deleted tuples
    TupleList*      pt_updated;            // Updated tuples
    LockList*       pt_locks;              // Held locks
    
    // Recovery information
    uint32_t        pt_checkpoint_num;     // Checkpoint number
    LSN             pt_prepare_lsn;        // Prepare LSN
} PreparedTransaction;

// Prepare transaction for two-phase commit
Status prepare_transaction(
    SBTransaction* txn,
    const char* gid)
{
    // Check state
    if (txn->txn_state != TXN_STATE_ACTIVE) {
        return STATUS_INVALID_TRANSACTION_STATE;
    }
    
    // Create prepared transaction record
    PreparedTransaction* pt = allocate(sizeof(PreparedTransaction));
    pt->pt_xid = txn->txn_id;
    strncpy(pt->pt_gid, gid, sizeof(pt->pt_gid));
    pt->pt_prepared_at = GetCurrentTimestamp();
    
    // Save modified data
    pt->pt_inserted = copy_tuple_list(txn->txn_inserted_tuples);
    pt->pt_deleted = copy_tuple_list(txn->txn_deleted_tuples);
    pt->pt_locks = copy_lock_list(txn->txn_locks);
    
    // Write prepare record to write-after log (WAL, optional post-gold)
    pt->pt_prepare_lsn = write_prepare_record(pt);
    
    // Flush write-after log (WAL, optional post-gold) to ensure durability
    flush_wal(pt->pt_prepare_lsn);
    
    // Update TIP to limbo state
    set_transaction_state(txn->txn_id, TXN_STATE_LIMBO);
    
    // Store in prepared transaction list
    add_prepared_transaction(pt);
    
    // Update transaction state
    txn->txn_state = TXN_STATE_LIMBO;
    txn->txn_prepared = true;
    
    return STATUS_OK;
}

// Commit prepared transaction
Status commit_prepared(const char* gid) {
    // Find prepared transaction
    PreparedTransaction* pt = find_prepared_transaction(gid);
    
    if (pt == NULL) {
        return STATUS_PREPARED_TRANSACTION_NOT_FOUND;
    }
    
    // Update TIP to committed
    set_transaction_state(pt->pt_xid, TXN_STATE_COMMITTED);
    
    // Write commit record
    write_commit_prepared_record(pt);
    
    // Release locks
    release_lock_list(pt->pt_locks);
    
    // Remove from prepared list
    remove_prepared_transaction(pt);
    
    // Free resources
    free_prepared_transaction(pt);
    
    return STATUS_OK;
}

// Rollback prepared transaction
Status rollback_prepared(const char* gid) {
    // Find prepared transaction
    PreparedTransaction* pt = find_prepared_transaction(gid);
    
    if (pt == NULL) {
        return STATUS_PREPARED_TRANSACTION_NOT_FOUND;
    }
    
    // Update TIP to aborted
    set_transaction_state(pt->pt_xid, TXN_STATE_ABORTED);
    
    // Write abort record
    write_abort_prepared_record(pt);
    
    // Mark inserted tuples as aborted
    for (ItemPointer tid : pt->pt_inserted) {
        mark_tuple_aborted(tid, pt->pt_xid);
    }
    
    // Clear delete marks
    for (ItemPointer tid : pt->pt_deleted) {
        clear_tuple_delete(tid, pt->pt_xid);
    }
    
    // Release locks
    release_lock_list(pt->pt_locks);
    
    // Remove from prepared list
    remove_prepared_transaction(pt);
    
    // Free resources
    free_prepared_transaction(pt);
    
    return STATUS_OK;
}
```

## Implementation Notes

This MGA core implementation provides:

1. **64-bit transaction IDs** eliminating wraparound issues
2. **Efficient TIP (Transaction Inventory Pages)** for transaction state
3. **Full MVCC** with snapshot isolation
4. **Version chain management** for updates
5. **Garbage collection** without blocking readers
6. **Savepoints** and nested transaction support
7. **Two-phase commit** for distributed transactions

The system is designed to work without write-after log (WAL, optional post-gold) for basic ACID
properties (minus durability), with an optional post-gold write-after log (WAL)
added only for replication/PITR (not crash recovery).
