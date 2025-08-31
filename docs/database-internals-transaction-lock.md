# Database Internals: Transaction Management and Lock Systems

## Table of Contents
1. [FirebirdSQL Transaction & Lock Management](#firebirdsql-transaction--lock-management)
2. [PostgreSQL Transaction & Lock Management](#postgresql-transaction--lock-management)
3. [MySQL/MariaDB Transaction & Lock Management](#mysqlmariadb-transaction--lock-management)
4. [Microsoft SQL Server Transaction & Lock Management](#microsoft-sql-server-transaction--lock-management)

---

# FirebirdSQL Transaction & Lock Management

## Lock Manager Specification

### Lock Architecture
```c
// Firebird Lock Manager Structure
typedef struct lock_manager {
    LockHeader*     lm_header;          // Shared memory header
    LockHash*       lm_hash;            // Hash table for locks
    LockOwner*      lm_owners;          // Lock owner blocks
    LockRequest*    lm_requests;        // Lock request blocks
    LockResource*   lm_resources;       // Lock resource blocks
    DeadlockQueue*  lm_deadlock_queue;  // Deadlock detection queue
    SLONG           lm_acquire_spins;   // Spin count before blocking
    SLONG           lm_acquire_retries; // Retry count
} LockManager;

// Lock types in Firebird
typedef enum lock_type {
    LCK_database = 1,      // Database lock
    LCK_relation = 2,      // Table lock
    LCK_page = 3,          // Page lock
    LCK_record = 4,        // Record lock
    LCK_transaction = 5,   // Transaction lock
    LCK_sweep = 6,         // Sweep lock
    LCK_expression = 7,    // Expression index lock
    LCK_shadow = 8,        // Shadow lock
    LCK_backup = 9,        // Backup state lock
    LCK_attachment = 10    // Attachment lock
} LockType;

// Lock modes
typedef enum lock_mode {
    LCK_none = 0,          // No lock
    LCK_null = 1,          // Null lock (placeholder)
    LCK_SR = 2,            // Shared Read
    LCK_PR = 3,            // Protected Read (repeatable read)
    LCK_SW = 4,            // Shared Write
    LCK_PW = 5,            // Protected Write
    LCK_EX = 6             // Exclusive
} LockMode;

// Lock compatibility matrix
static const bool lock_compatibility[7][7] = {
    // none null  SR   PR   SW   PW   EX
    {true, true, true, true, true, true, true},  // none
    {true, true, true, true, true, true, true},  // null
    {true, true, true, true, true, false, false}, // SR
    {true, true, true, false, false, false, false}, // PR
    {true, true, true, false, false, false, false}, // SW
    {true, true, false, false, false, false, false}, // PW
    {true, true, false, false, false, false, false}  // EX
};

// Lock request structure
typedef struct lock_request {
    LockRequest*    lrq_next;           // Next in chain
    LockRequest*    lrq_prev;           // Previous in chain
    LockOwner*      lrq_owner;          // Lock owner
    LockResource*   lrq_resource;       // Resource being locked
    LockMode        lrq_requested;      // Requested mode
    LockMode        lrq_granted;        // Granted mode
    USHORT          lrq_flags;          // Request flags
    SLONG           lrq_wait_time;      // Wait timeout
    AST_ROUTINE     lrq_ast_routine;    // Async trap routine
    void*           lrq_ast_argument;   // AST argument
} LockRequest;

// Lock acquisition algorithm
LockResult acquire_lock(LockManager* lm, LockRequest* request) {
    // Phase 1: Fast path for uncontended locks
    LockResource* resource = find_or_create_resource(lm, request);
    
    if (resource->lrs_count == 0) {
        // No other locks - grant immediately
        grant_lock(request, request->lrq_requested);
        return LOCK_SUCCESS;
    }
    
    // Phase 2: Check compatibility
    if (is_compatible(resource, request)) {
        grant_lock(request, request->lrq_requested);
        return LOCK_SUCCESS;
    }
    
    // Phase 3: Try lock conversion if already holding lower mode
    if (request->lrq_granted != LCK_none) {
        if (can_convert(resource, request)) {
            convert_lock(request, request->lrq_requested);
            return LOCK_SUCCESS;
        }
    }
    
    // Phase 4: Queue and wait
    queue_lock_request(resource, request);
    
    // Phase 5: Wait with timeout
    if (request->lrq_wait_time == 0) {
        return LOCK_TIMEOUT;  // No wait requested
    }
    
    LockResult result = wait_for_lock(request, request->lrq_wait_time);
    
    if (result == LOCK_TIMEOUT) {
        // Check for deadlock before reporting timeout
        if (detect_deadlock(lm, request)) {
            return LOCK_DEADLOCK;
        }
    }
    
    return result;
}
```

## Deadlock Detection Algorithms

### Wait-For Graph Implementation
```c
// Firebird deadlock detection using wait-for graph
typedef struct deadlock_walk {
    LockOwner*      ddw_owner;          // Current owner in walk
    LockRequest*    ddw_request;        // Current request
    ULONG           ddw_visit_mark;     // Visit marker
    DeadlockNode*   ddw_nodes;          // Graph nodes
    USHORT          ddw_node_count;     // Number of nodes
    bool            ddw_cycle_found;    // Cycle detected flag
} DeadlockWalk;

bool detect_deadlock(LockManager* lm, LockRequest* request) {
    DeadlockWalk walk;
    memset(&walk, 0, sizeof(walk));
    
    walk.ddw_owner = request->lrq_owner;
    walk.ddw_request = request;
    walk.ddw_visit_mark = ++lm->lm_deadlock_mark;
    
    // Build wait-for graph
    build_wait_graph(&walk, request);
    
    // Detect cycles using DFS
    if (find_cycle_dfs(&walk, request->lrq_owner)) {
        // Deadlock detected - choose victim
        LockRequest* victim = choose_deadlock_victim(&walk);
        
        // Signal victim
        signal_deadlock_victim(victim);
        
        return true;
    }
    
    return false;
}

// Cycle detection using DFS
bool find_cycle_dfs(DeadlockWalk* walk, LockOwner* start) {
    // Mark as visiting
    start->low_visit_mark = walk->ddw_visit_mark;
    start->low_in_stack = true;
    
    // Check all edges from this node
    for (LockRequest* req = start->low_requests; req; req = req->lrq_next) {
        if (req->lrq_granted != LCK_none) {
            continue;  // Not waiting
        }
        
        // Find who we're waiting for
        LockOwner* blocking = find_blocking_owner(req);
        
        if (!blocking) continue;
        
        if (blocking->low_visit_mark != walk->ddw_visit_mark) {
            // Not visited - recurse
            if (find_cycle_dfs(walk, blocking)) {
                return true;
            }
        } else if (blocking->low_in_stack) {
            // Back edge - cycle found!
            walk->ddw_cycle_found = true;
            record_cycle_path(walk, start, blocking);
            return true;
        }
    }
    
    start->low_in_stack = false;
    return false;
}

// Deadlock victim selection
LockRequest* choose_deadlock_victim(DeadlockWalk* walk) {
    LockRequest* victim = NULL;
    ULONG min_cost = MAX_ULONG;
    
    // Calculate cost for each participant
    for (int i = 0; i < walk->ddw_node_count; i++) {
        DeadlockNode* node = &walk->ddw_nodes[i];
        ULONG cost = calculate_victim_cost(node);
        
        if (cost < min_cost) {
            min_cost = cost;
            victim = node->ddn_request;
        }
    }
    
    return victim;
}

// Victim cost calculation
ULONG calculate_victim_cost(DeadlockNode* node) {
    ULONG cost = 0;
    
    // Factor 1: Transaction age (older = higher cost to kill)
    cost += node->ddn_owner->low_transaction_id * 1000;
    
    // Factor 2: Work done (more work = higher cost)
    cost += node->ddn_owner->low_updates * 100;
    cost += node->ddn_owner->low_inserts * 100;
    cost += node->ddn_owner->low_deletes * 100;
    
    // Factor 3: Lock priority
    cost += node->ddn_request->lrq_priority * 10000;
    
    // Factor 4: System vs user transaction
    if (node->ddn_owner->low_flags & LOW_SYSTEM) {
        cost += 1000000;  // Avoid killing system transactions
    }
    
    return cost;
}
```

## Isolation Level Implementation

### Firebird Isolation Levels
```c
// Transaction isolation parameters
typedef struct transaction_params {
    USHORT      tpb_version;        // TPB version
    USHORT      tpb_access_mode;    // Read-only/Read-write
    USHORT      tpb_isolation;      // Isolation level
    USHORT      tpb_wait_mode;      // Wait/No-wait
    USHORT      tpb_lock_timeout;   // Lock timeout
} TransactionParams;

// Isolation levels
#define ISO_READ_COMMITTED_READ_CONSISTENCY  1  // Read committed (read consistency)
#define ISO_READ_COMMITTED_RECORD_VERSION    2  // Read committed (record version)
#define ISO_REPEATABLE_READ                  3  // Snapshot/Repeatable read
#define ISO_SERIALIZABLE                     4  // Snapshot table stability

// Read Committed implementation
typedef struct read_committed_state {
    TraNumber   rc_snapshot_number;     // Current statement snapshot
    TipCache*   rc_tip_cache;          // Transaction state cache
    bool        rc_read_consistency;    // Read consistency mode
    TraNumber   rc_statement_snapshot;  // Statement-level snapshot
} ReadCommittedState;

void setup_read_committed(Transaction* trans, bool read_consistency) {
    ReadCommittedState* rc = FB_NEW ReadCommittedState();
    trans->tra_rc_state = rc;
    
    if (read_consistency) {
        // Read consistency mode - statement-level snapshots
        rc->rc_read_consistency = true;
        rc->rc_statement_snapshot = get_statement_snapshot(trans);
    } else {
        // Record version mode - see latest committed
        rc->rc_read_consistency = false;
        rc->rc_snapshot_number = MAX_TRA_NUMBER;
    }
    
    // Initialize TIP cache for transaction state checks
    rc->rc_tip_cache = create_tip_cache(trans);
}

// Snapshot isolation implementation
typedef struct snapshot_state {
    TraNumber   ss_snapshot_number;     // Snapshot transaction number
    TraNumber   ss_oldest_active;       // Oldest active at start
    TipCache*   ss_tip_cache;          // Frozen TIP state
    RecordList* ss_modified_records;    // Records modified in this transaction
} SnapshotState;

void setup_snapshot_isolation(Transaction* trans) {
    SnapshotState* ss = FB_NEW SnapshotState();
    trans->tra_snapshot_state = ss;
    
    // Freeze transaction state at start
    ss->ss_snapshot_number = trans->tra_number;
    ss->ss_oldest_active = get_oldest_active(trans->tra_database);
    
    // Cache TIP state - won't change for this transaction
    ss->ss_tip_cache = freeze_tip_state(trans);
    
    // Track modifications for write conflict detection
    ss->ss_modified_records = FB_NEW RecordList();
}

// Visibility check for different isolation levels
bool is_record_visible(Transaction* trans, RecordHeader* record) {
    TraNumber rec_transaction = record->rhd_transaction;
    
    switch (trans->tra_isolation_level) {
        case ISO_READ_COMMITTED_RECORD_VERSION:
            // See latest committed version
            return check_committed_record_version(trans, rec_transaction);
            
        case ISO_READ_COMMITTED_READ_CONSISTENCY:
            // See statement-consistent snapshot
            return check_statement_snapshot(trans, rec_transaction);
            
        case ISO_REPEATABLE_READ:
            // See transaction-consistent snapshot
            return check_transaction_snapshot(trans, rec_transaction);
            
        case ISO_SERIALIZABLE:
            // Same as repeatable read + lock checks
            if (!check_transaction_snapshot(trans, rec_transaction)) {
                return false;
            }
            // Additional stability checks
            return check_table_stability(trans, record);
    }
    
    return false;
}

// Write conflict detection for snapshot isolation
bool check_write_conflict(Transaction* trans, RecordHeader* record) {
    if (trans->tra_isolation_level < ISO_REPEATABLE_READ) {
        return false;  // No write conflict checking for read committed
    }
    
    SnapshotState* ss = trans->tra_snapshot_state;
    
    // Check if record was modified after our snapshot
    TraNumber last_update = get_last_update_transaction(record);
    
    if (last_update > ss->ss_snapshot_number) {
        // Record modified by concurrent transaction
        if (is_transaction_committed(last_update)) {
            // Committed concurrent modification - conflict!
            return true;
        }
    }
    
    return false;
}
```

---

# PostgreSQL Transaction & Lock Management

## Lock Manager Specification

### PostgreSQL Lock System
```c
// PostgreSQL Lock Manager
typedef struct {
    HTAB*           LockMethodLockHash;     // Hash table of locks
    HTAB*           LockMethodProcLockHash; // Hash table of process locks
    LWLock*         LockHashPartitionLWLocks[NUM_LOCK_PARTITIONS];
    DeadLockState   deadlock_state;         // Deadlock detection state
    int             max_locks_per_xact;     // Max locks per transaction
} LockManager;

// Lock modes in PostgreSQL
typedef enum LockMode {
    NoLock = 0,
    AccessShareLock = 1,        // SELECT
    RowShareLock = 2,          // SELECT FOR UPDATE
    RowExclusiveLock = 3,      // INSERT, UPDATE, DELETE
    ShareUpdateExclusiveLock = 4, // VACUUM, ANALYZE
    ShareLock = 5,             // CREATE INDEX
    ShareRowExclusiveLock = 6, // Rarely used
    ExclusiveLock = 7,         // Blocks ROW SHARE/SELECT FOR UPDATE
    AccessExclusiveLock = 8    // ALTER TABLE, DROP TABLE, VACUUM FULL
} LockMode;

// Lock tag identifying the locked object
typedef struct LOCKTAG {
    uint32  locktag_field1;    // Database OID or InvalidOid
    uint32  locktag_field2;    // Relation OID or InvalidOid
    uint32  locktag_field3;    // Page number or InvalidBlockNumber
    uint16  locktag_field4;    // Tuple offset or InvalidOffsetNumber
    uint8   locktag_type;      // Lock type
    uint8   locktag_lockmethodid; // Lock method ID
} LOCKTAG;

// Lock types
typedef enum LockTagType {
    LOCKTAG_RELATION,          // Whole relation
    LOCKTAG_RELATION_EXTEND,   // Relation extension
    LOCKTAG_PAGE,             // One page of a relation
    LOCKTAG_TUPLE,            // One tuple
    LOCKTAG_TRANSACTION,      // Transaction ID
    LOCKTAG_VIRTUALTRANSACTION, // Virtual transaction ID
    LOCKTAG_SPECULATIVE_TOKEN, // Speculative insertion
    LOCKTAG_OBJECT,           // Non-relation database object
    LOCKTAG_USERLOCK,         // User-defined locks
    LOCKTAG_ADVISORY          // Advisory locks
} LockTagType;

// Lock acquisition
LockAcquireResult
LockAcquire(const LOCKTAG *locktag, LockMode lockmode,
            bool sessionLock, bool dontWait) {
    LOCALLOCK  *locallock;
    LOCK       *lock;
    PROCLOCK   *proclock;
    bool        found;
    
    // Fast path for common cases
    if (FastPathStrongRelationLocks->count[lockmode] == 0) {
        if (FastPathGrantRelationLock(locktag, lockmode)) {
            return LOCKACQUIRE_OK;
        }
    }
    
    // Look up or create local lock entry
    locallock = (LOCALLOCK *) hash_search(LockMethodLocalHash,
                                          locktag, HASH_ENTER, &found);
    
    if (!found) {
        locallock->nLocks = 0;
        locallock->numLockOwners = 0;
        locallock->maxLockOwners = 0;
        locallock->lockOwners = NULL;
    }
    
    // Check if we already hold this lock
    if (locallock->nLocks > 0) {
        GrantLockLocal(locallock, lockmode);
        return LOCKACQUIRE_OK;
    }
    
    // Acquire partition lock
    uint32 hashcode = LockTagHashCode(locktag);
    LWLock *partitionLock = LockHashPartitionLock(hashcode);
    
    LWLockAcquire(partitionLock, LW_EXCLUSIVE);
    
    // Look up or create shared lock entry
    lock = (LOCK *) hash_search_with_hash_value(LockMethodLockHash,
                                                locktag, hashcode,
                                                HASH_ENTER_NULL, &found);
    
    if (!found) {
        // Initialize new lock entry
        lock->grantMask = 0;
        lock->waitMask = 0;
        SHMQueueInit(&lock->procLocks);
        ProcQueueInit(&lock->waitProcs);
        lock->nRequested = 0;
        lock->nGranted = 0;
        MemSet(lock->requested, 0, sizeof(lock->requested));
        MemSet(lock->granted, 0, sizeof(lock->granted));
    }
    
    // Look up or create process lock entry
    proclock = SetupLockInTable(lockmode, MyProc, locktag, hashcode, lock);
    
    // Check for conflicts
    if (LockCheckConflicts(lockmode, lock, proclock) != STATUS_OK) {
        if (dontWait) {
            // No wait requested - fail immediately
            LWLockRelease(partitionLock);
            return LOCKACQUIRE_NOT_AVAIL;
        }
        
        // Add to wait queue
        AddToWaitQueue(lock, lockmode);
        
        LWLockRelease(partitionLock);
        
        // Wait for lock
        ProcSleep(locallock, lockmode);
        
        // Check if we got the lock or were aborted
        if (MyProc->waitStatus != STATUS_OK) {
            if (MyProc->waitStatus == STATUS_DEADLOCK) {
                return LOCKACQUIRE_DEADLOCK;
            }
            return LOCKACQUIRE_NOT_AVAIL;
        }
    } else {
        // Grant lock immediately
        GrantLock(lock, proclock, lockmode);
        LWLockRelease(partitionLock);
    }
    
    // Update local lock info
    GrantLockLocal(locallock, lockmode);
    
    return LOCKACQUIRE_OK;
}

// Lock conflict matrix
static const bool lockMethodConflictTable[MAX_LOCKMODES][MAX_LOCKMODES] = {
    /*         NoLock  AS     RS     RX     SUX    S      SRX    X      AX */
    /* NoLock */ {false, false, false, false, false, false, false, false, false},
    /* AS */     {false, false, false, false, false, false, false, false, true},
    /* RS */     {false, false, false, false, false, false, false, true,  true},
    /* RX */     {false, false, false, false, false, true,  true,  true,  true},
    /* SUX */    {false, false, false, false, false, true,  true,  true,  true},
    /* S */      {false, false, false, true,  true,  false, true,  true,  true},
    /* SRX */    {false, false, false, true,  true,  true,  true,  true,  true},
    /* X */      {false, false, true,  true,  true,  true,  true,  true,  true},
    /* AX */     {false, true,  true,  true,  true,  true,  true,  true,  true}
};
```

## Deadlock Detection

### PostgreSQL Deadlock Detection
```c
// Deadlock detection state
typedef struct {
    PGPROC     **procs;         // Array of processes in cycle
    int         nprocs;         // Number of processes
    int        *ordering;       // Topological ordering
    EDGE       *edges;          // Wait-for edges
    int         nedges;         // Number of edges
    WAIT_ORDER *wait_orders;    // Wait ordering info
    int         nwait_orders;   // Number of wait orders
    bool        deadlock_found; // Deadlock detected flag
} DeadLockState;

// Main deadlock detection function
bool
DeadLockCheck(PGPROC *proc) {
    DeadLockState state;
    int         nprocs;
    int         nprepared;
    
    // Initialize detection state
    memset(&state, 0, sizeof(state));
    
    // Build initial process list
    nprocs = GetBlockingProcesses(proc, &state.procs);
    
    // Build wait-for graph
    if (!BuildWaitGraph(&state, nprocs)) {
        return false;  // No cycles possible
    }
    
    // Run cycle detection algorithm
    if (TopoSort(&state)) {
        // No cycles found
        return false;
    }
    
    // Cycle detected - find all cycles
    FindAllCycles(&state);
    
    // Choose deadlock victim
    PGPROC *victim = ChooseDeadlockVictim(&state);
    
    // Abort victim transaction
    if (victim == MyProc) {
        // We are the victim
        ereport(ERROR,
                (errcode(ERRCODE_T_R_DEADLOCK_DETECTED),
                 errmsg("deadlock detected")));
    } else {
        // Signal victim to abort
        SendProcSignal(victim->pid, PROCSIG_DEADLOCK, victim->backendId);
    }
    
    return true;
}

// Build wait-for graph
bool
BuildWaitGraph(DeadLockState *state, int nprocs) {
    state->edges = palloc(nprocs * nprocs * sizeof(EDGE));
    state->nedges = 0;
    
    for (int i = 0; i < nprocs; i++) {
        PGPROC *proc = state->procs[i];
        LOCK   *lock = proc->waitLock;
        
        if (!lock)
            continue;
        
        // Find all processes holding conflicting locks
        SHM_QUEUE *procLocks = &(lock->procLocks);
        PROCLOCK *proclock;
        
        proclock = (PROCLOCK *) SHMQueueNext(procLocks, procLocks,
                                             offsetof(PROCLOCK, lockLink));
        
        while (proclock) {
            if (proclock->tag.myProc != proc) {
                // Check if this holder conflicts with waiter
                if (DoLockModesConflict(proc->waitLockMode,
                                       proclock->holdMask)) {
                    // Add edge: proc waits for proclock->tag.myProc
                    AddEdge(state, proc, proclock->tag.myProc);
                }
            }
            
            proclock = (PROCLOCK *) SHMQueueNext(procLocks,
                                                &proclock->lockLink,
                                                offsetof(PROCLOCK, lockLink));
        }
    }
    
    return (state->nedges > 0);
}

// Topological sort for cycle detection
bool
TopoSort(DeadLockState *state) {
    int    *indegree;
    int     queue[MAX_PROCS];
    int     head = 0, tail = 0;
    int     sorted = 0;
    
    // Calculate in-degrees
    indegree = palloc0(state->nprocs * sizeof(int));
    for (int i = 0; i < state->nedges; i++) {
        indegree[state->edges[i].to]++;
    }
    
    // Initialize queue with zero in-degree nodes
    for (int i = 0; i < state->nprocs; i++) {
        if (indegree[i] == 0) {
            queue[tail++] = i;
        }
    }
    
    // Process queue
    while (head < tail) {
        int node = queue[head++];
        state->ordering[sorted++] = node;
        
        // Remove edges from this node
        for (int i = 0; i < state->nedges; i++) {
            if (state->edges[i].from == node) {
                int to = state->edges[i].to;
                if (--indegree[to] == 0) {
                    queue[tail++] = to;
                }
            }
        }
    }
    
    // If all nodes sorted, no cycle
    return (sorted == state->nprocs);
}

// Choose deadlock victim
PGPROC *
ChooseDeadlockVictim(DeadLockState *state) {
    PGPROC *victim = NULL;
    int     min_cost = INT_MAX;
    
    // Consider each process in the cycle
    for (int i = 0; i < state->nprocs; i++) {
        PGPROC *proc = state->procs[i];
        int cost = 0;
        
        // Factor 1: Transaction age (lower XID = older = higher cost)
        cost += (MaxTransactionId - proc->xid) / 1000;
        
        // Factor 2: Number of locks held
        cost += proc->numLocks * 10;
        
        // Factor 3: Transaction priority
        if (proc->priority == HIGH_PRIORITY) {
            cost += 10000;
        }
        
        // Factor 4: Work done
        cost += proc->tuplesModified;
        
        if (cost < min_cost) {
            min_cost = cost;
            victim = proc;
        }
    }
    
    return victim;
}
```

## Isolation Level Implementation

### PostgreSQL MVCC and Isolation
```c
// Snapshot structure for MVCC
typedef struct SnapshotData {
    SnapshotType snapshot_type;
    TransactionId xmin;         // Earliest XID still running
    TransactionId xmax;         // First XID not running
    TransactionId *xip;         // Array of XIDs in progress
    uint32      xcnt;           // Number of XIDs in xip[]
    TransactionId *subxip;      // Array of sub-XIDs in progress
    int32       subxcnt;        // Number of sub-XIDs
    bool        suboverflowed;  // Sub-XID array overflowed?
    bool        takenDuringRecovery;
    bool        copied;         // Snapshot has been copied
    CommandId   curcid;         // Command ID at snapshot time
    uint32      speculativeToken;
    uint32      active_count;   // Refcount on ActiveSnapshot
    uint32      regd_count;     // Refcount on RegisteredSnapshots
    pairingheap_node ph_node;   // Link in RegisteredSnapshots
    TimestampTz whenTaken;      // Timestamp when taken
    XLogRecPtr  lsn;           // Position in WAL when taken
} SnapshotData;

// Isolation levels
typedef enum {
    XACT_READ_UNCOMMITTED,      // Same as READ COMMITTED in PostgreSQL
    XACT_READ_COMMITTED,
    XACT_REPEATABLE_READ,
    XACT_SERIALIZABLE
} IsoLevel;

// Get snapshot for current isolation level
Snapshot
GetTransactionSnapshot(void) {
    if (IsolationUsesXactSnapshot()) {
        // REPEATABLE READ or SERIALIZABLE
        if (FirstSnapshotSet)
            return CurrentSnapshot;
            
        // First snapshot in transaction
        CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);
        FirstSnapshotSet = true;
        
        if (IsolationIsSerializable()) {
            // Initialize SSI (Serializable Snapshot Isolation)
            SetupSerializableSnapshot(CurrentSnapshot);
        }
        
        return CurrentSnapshot;
    } else {
        // READ COMMITTED - new snapshot for each statement
        CurrentSnapshot = GetSnapshotData(&CurrentSnapshotData);
        return CurrentSnapshot;
    }
}

// Visibility check using snapshot
bool
HeapTupleSatisfiesMVCC(HeapTuple tuple, Snapshot snapshot,
                       Buffer buffer) {
    HeapTupleHeader tuphdr = tuple->t_data;
    
    // Check tuple xmin (insert transaction)
    if (!TransactionIdIsValid(HeapTupleHeaderGetXmin(tuphdr))) {
        return false;  // Invalid tuple
    }
    
    if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmin(tuphdr))) {
        // Tuple inserted by our transaction
        if (HeapTupleHeaderGetCmin(tuphdr) >= snapshot->curcid) {
            return false;  // Inserted after snapshot
        }
        
        if (TransactionIdIsValid(HeapTupleHeaderGetXmax(tuphdr))) {
            // Also deleted by our transaction
            if (HeapTupleHeaderGetCmax(tuphdr) >= snapshot->curcid) {
                return true;  // Deleted after snapshot
            } else {
                return false;  // Deleted before snapshot
            }
        }
        
        return true;  // Inserted by us and not deleted
    }
    
    // Check if insert is visible
    if (!XidInSnapshot(HeapTupleHeaderGetXmin(tuphdr), snapshot)) {
        if (!TransactionIdDidCommit(HeapTupleHeaderGetXmin(tuphdr))) {
            return false;  // Insert not committed
        }
    } else {
        return false;  // Insert too new for snapshot
    }
    
    // Check tuple xmax (delete/update transaction)
    if (!TransactionIdIsValid(HeapTupleHeaderGetXmax(tuphdr))) {
        return true;  // Not deleted
    }
    
    if (TransactionIdIsCurrentTransactionId(HeapTupleHeaderGetXmax(tuphdr))) {
        // Deleted by our transaction
        if (HeapTupleHeaderGetCmax(tuphdr) >= snapshot->curcid) {
            return true;  // Deleted after snapshot
        } else {
            return false;  // Deleted before snapshot
        }
    }
    
    // Check if delete is visible
    if (!XidInSnapshot(HeapTupleHeaderGetXmax(tuphdr), snapshot)) {
        if (TransactionIdDidCommit(HeapTupleHeaderGetXmax(tuphdr))) {
            return false;  // Delete committed and visible
        }
    }
    
    return true;  // Delete not visible to snapshot
}

// Serializable Snapshot Isolation (SSI)
typedef struct SERIALIZABLEXACT {
    VirtualTransactionId vxid;
    SerCommitSeqNo commitSeqNo;
    union {
        SerCommitSeqNo earliestOutConflictCommit;
        SerCommitSeqNo lastCommitBeforeSnapshot;
    };
    Snapshot    snapshot;
    uint32      flags;
    SHM_QUEUE   outConflicts;   // Write-read conflicts out
    SHM_QUEUE   inConflicts;    // Write-read conflicts in
    SHM_QUEUE   predicateLocks;  // Predicate locks held
    SerCommitSeqNo prepareSeqNo;
    SHM_QUEUE   finishedLink;
    SerCommitSeqNo finishedBefore;
    TransactionId xmin;
    uint32      pid;
} SERIALIZABLEXACT;

// Check for serialization conflicts
void
CheckForSerializationConflict(Relation relation,
                              HeapTuple tuple,
                              Buffer buffer,
                              Snapshot snapshot) {
    SERIALIZABLEXACT *sxact;
    
    if (!IsolationIsSerializable())
        return;
    
    sxact = MySerializableXact;
    if (!sxact)
        return;
    
    // Check if tuple has predicate lock
    uint32 targettaghash = PredicateLockTargetTagHashCode(&tuple->t_self);
    LWLock *partitionLock = PredicateLockHashPartitionLock(targettaghash);
    
    LWLockAcquire(partitionLock, LW_SHARED);
    
    PREDICATELOCK *predlock = (PREDICATELOCK *)
        hash_search_with_hash_value(PredicateLockHash,
                                   &tuple->t_self,
                                   targettaghash,
                                   HASH_FIND, NULL);
    
    if (predlock) {
        // Found predicate lock - check for conflict
        SERIALIZABLEXACT *holder = predlock->tag.myXact;
        
        if (holder != sxact) {
            // Different transaction holds predicate lock
            if (XactIsConcurrent(sxact, holder)) {
                // Concurrent transactions - record conflict
                RecordConflict(sxact, holder, tuple);
                
                // Check for dangerous structure
                if (DetectDangerousStructure(sxact)) {
                    LWLockRelease(partitionLock);
                    ereport(ERROR,
                           (errcode(ERRCODE_T_R_SERIALIZATION_FAILURE),
                            errmsg("could not serialize access")));
                }
            }
        }
    }
    
    LWLockRelease(partitionLock);
}
```

---

# MySQL/MariaDB Transaction & Lock Management

## Lock Manager Specification

### InnoDB Lock System
```c
// InnoDB lock manager structure
struct lock_sys_t {
    ib_mutex_t      mutex;           // Lock system mutex
    hash_table_t*   rec_hash;        // Hash table of record locks
    hash_table_t*   prdt_hash;       // Hash table of predicate locks
    hash_table_t*   prdt_page_hash;  // Hash table of page predicate locks
    
    ib_mutex_t      wait_mutex;      // Mutex protecting wait queues
    srv_slot_t*     waiting_threads; // Array of waiting threads
    
    ulint           n_lock_max_wait_time;  // Max wait time observed
    
    os_event_t      timeout_event;   // Event for lock timeout thread
    bool            rollback_complete; // Rollback completion flag
    
    ulint           deadlock_mark;    // Deadlock check iteration counter
    ulint           deadlock_detected; // Number of deadlocks detected
};

// Lock types in InnoDB
enum lock_type_t {
    LOCK_TABLE = 1,     // Table level lock
    LOCK_REC = 2        // Record level lock
};

// Lock modes
enum lock_mode {
    LOCK_IS = 0,        // Intention shared
    LOCK_IX = 1,        // Intention exclusive
    LOCK_S = 2,         // Shared
    LOCK_X = 3,         // Exclusive
    LOCK_AUTO_INC = 4,  // AUTO_INCREMENT lock
    LOCK_NONE = 5       // No lock
};

// Record lock types
#define LOCK_ORDINARY    0  // Next-key lock
#define LOCK_GAP         512  // Gap lock
#define LOCK_REC_NOT_GAP 1024  // Record lock
#define LOCK_INSERT_INTENTION 2048  // Insert intention lock

// Lock structure
struct lock_t {
    trx_t*          trx;            // Transaction owning the lock
    UT_LIST_NODE_T(lock_t) trx_locks; // Transaction's lock list
    
    dict_index_t*   index;          // Index for record lock
    lock_t*         hash;           // Hash chain node
    
    union {
        lock_table_t    tab_lock;   // Table lock
        lock_rec_t      rec_lock;   // Record lock
    } un_member;
    
    ulint           type_mode;      // Lock type and mode
    uint32_t        m_psi_internal_thread_id;
    uint64_t        m_psi_event_id;
};

// Lock acquisition algorithm
dberr_t
lock_rec_lock(
    bool            impl,
    ulint           mode,
    const buf_block_t* block,
    ulint           heap_no,
    dict_index_t*   index,
    que_thr_t*      thr)
{
    trx_t*  trx = thr_get_trx(thr);
    
    // Fast path for implicit locks
    if (impl) {
        if (!lock_rec_has_expl(mode, block, heap_no, trx)) {
            // No explicit lock needed
            return DB_SUCCESS;
        }
    }
    
    // Check for conflicting locks
    lock_mutex_enter();
    
    lock_t* lock = lock_rec_has_expl_on_page(block->frame);
    
    if (lock == NULL) {
        // No locks on page - grant immediately
        lock = lock_rec_create(mode, block, heap_no, index, trx, true);
        lock_mutex_exit();
        return DB_SUCCESS;
    }
    
    // Check for conflicts
    if (lock_rec_has_to_wait(trx, mode, lock, heap_no)) {
        // Must wait for conflicting lock
        
        // Check for deadlock
        if (lock_deadlock_check(trx, lock)) {
            lock_mutex_exit();
            return DB_DEADLOCK;
        }
        
        // Create waiting lock
        lock_t* wait_lock = lock_rec_create(mode | LOCK_WAIT,
                                           block, heap_no,
                                           index, trx, false);
        
        // Set up wait
        lock_set_lock_and_trx_wait(wait_lock, trx);
        
        lock_mutex_exit();
        
        // Wait for lock
        lock_wait_suspend_thread(thr);
        
        // Check result after wakeup
        if (trx->error_state != DB_SUCCESS) {
            return trx->error_state;
        }
    } else {
        // Can grant immediately
        lock = lock_rec_create(mode, block, heap_no, index, trx, true);
        lock_mutex_exit();
    }
    
    return DB_SUCCESS;
}

// Lock compatibility matrix
static const byte lock_compatibility_matrix[5][5] = {
    /**         IS     IX     S      X      AI */
    /* IS */ {  TRUE,  TRUE,  TRUE,  FALSE, TRUE},
    /* IX */ {  TRUE,  TRUE,  FALSE, FALSE, TRUE},
    /* S  */ {  TRUE,  FALSE, TRUE,  FALSE, FALSE},
    /* X  */ {  FALSE, FALSE, FALSE, FALSE, FALSE},
    /* AI */ {  TRUE,  TRUE,  FALSE, FALSE, FALSE}
};

// Gap lock handling for phantom prevention
bool
lock_rec_has_to_wait_in_queue(
    const lock_t*   wait_lock)
{
    const lock_t*   lock;
    ulint           heap_no = lock_rec_find_set_bit(wait_lock);
    
    // Check each lock in queue
    for (lock = lock_rec_get_first_on_page_addr(
            lock_rec_get_page_id(wait_lock));
         lock != wait_lock;
         lock = lock_rec_get_next_on_page(lock)) {
        
        if (lock_rec_get_nth_bit(lock, heap_no)) {
            // Lock on same record
            if (lock_has_to_wait(wait_lock, lock)) {
                return true;
            }
        }
    }
    
    return false;
}
```

## Deadlock Detection

### InnoDB Deadlock Detection
```c
// Deadlock detection state
struct DeadlockChecker {
    const trx_t*    m_start_trx;     // Transaction to start search from
    const lock_t*   m_wait_lock;     // Lock the transaction is waiting for
    ulint           m_mark_start;    // Deadlock check counter
    ulint           m_cost;          // Cost of current path
    ulint           m_cost_limit;    // Maximum cost before giving up
    bool            m_too_deep;      // Reached depth limit
};

// Main deadlock detection
const trx_t*
DeadlockChecker::check_and_resolve(
    const lock_t*   lock,
    trx_t*          trx)
{
    const trx_t* victim_trx;
    
    // Initialize checker
    DeadlockChecker checker(trx, lock);
    
    // Search for deadlock
    victim_trx = checker.search();
    
    if (victim_trx != NULL) {
        // Deadlock found - resolve it
        if (victim_trx == trx) {
            // We are the victim
            lock_deadlock_notify(trx);
            return victim_trx;
        } else {
            // Roll back victim
            lock_deadlock_victim_rollback(victim_trx);
        }
    }
    
    return victim_trx;
}

// Depth-first search for cycles
const trx_t*
DeadlockChecker::search()
{
    ut_ad(lock_mutex_own());
    ut_ad(m_start_trx == m_wait_lock->trx);
    
    // Check depth limit
    if (m_cost > m_cost_limit) {
        m_too_deep = true;
        return m_start_trx;  // Give up and rollback starter
    }
    
    // Get lock holders
    lock_t* lock = get_first_lock(&heap_no);
    
    while (lock != NULL) {
        // Check if this lock blocks our wait_lock
        if (lock_has_to_wait(m_wait_lock, lock)) {
            const trx_t* lock_trx = lock->trx;
            
            // Skip if same transaction
            if (lock_trx == m_start_trx) {
                lock = get_next_lock(lock, heap_no);
                continue;
            }
            
            // Check if we've created a cycle
            if (is_visited(lock_trx)) {
                // Cycle detected!
                return select_victim(lock_trx);
            }
            
            // Mark as visited
            mark_visited(lock_trx);
            
            // Check what this transaction is waiting for
            if (lock_trx->lock.wait_lock != NULL) {
                // Recurse
                m_wait_lock = lock_trx->lock.wait_lock;
                m_cost++;
                
                const trx_t* victim = search();
                
                if (victim != NULL) {
                    return victim;
                }
            }
        }
        
        lock = get_next_lock(lock, heap_no);
    }
    
    return NULL;
}

// Select deadlock victim
const trx_t*
DeadlockChecker::select_victim(const trx_t* trx)
{
    const trx_t* victim = trx;
    ulint victim_weight = trx_weight(trx);
    
    // Walk the cycle to find best victim
    const trx_t* current = trx;
    do {
        current = get_next_in_cycle(current);
        ulint weight = trx_weight(current);
        
        if (weight < victim_weight) {
            victim = current;
            victim_weight = weight;
        }
    } while (current != trx);
    
    return victim;
}

// Calculate transaction weight for victim selection
ulint
trx_weight(const trx_t* trx)
{
    ulint weight = 0;
    
    // Factor 1: Number of locks held
    weight += UT_LIST_GET_LEN(trx->lock.trx_locks) * 1000;
    
    // Factor 2: Undo log size (work done)
    weight += trx->undo_no * 100;
    
    // Factor 3: Number of rows modified
    weight += trx->n_mysql_tables_modified * 10000;
    
    // Factor 4: Transaction age
    weight += (srv_max_trx_id - trx->id) / 1000;
    
    return weight;
}
```

## Isolation Level Implementation

### InnoDB Isolation Levels
```c
// Transaction isolation levels
enum trx_isolation_level_t {
    TRX_ISO_READ_UNCOMMITTED,
    TRX_ISO_READ_COMMITTED,
    TRX_ISO_REPEATABLE_READ,
    TRX_ISO_SERIALIZABLE
};

// Read view for MVCC
struct ReadView {
    trx_id_t    m_low_limit_id;      // High water mark
    trx_id_t    m_up_limit_id;       // Low water mark
    trx_id_t    m_creator_trx_id;    // Creator transaction ID
    
    ids_t       m_ids;                // Active transaction IDs
    m_view_list m_view_list;          // List of views
    
    bool changes_visible(trx_id_t id, const table_name_t& name) const {
        if (id < m_up_limit_id || id == m_creator_trx_id) {
            return true;
        }
        
        if (id >= m_low_limit_id) {
            return false;
        }
        
        return !std::binary_search(m_ids.begin(), m_ids.end(), id);
    }
};

// Create read view based on isolation level
ReadView*
trx_assign_read_view(trx_t* trx)
{
    if (trx->isolation_level == TRX_ISO_READ_COMMITTED) {
        // New read view for each statement
        if (trx->read_view != NULL) {
            trx_sys->mvcc->view_close(trx->read_view);
        }
        trx->read_view = trx_sys->mvcc->view_open(trx);
    } else if (trx->isolation_level >= TRX_ISO_REPEATABLE_READ) {
        // Single read view for entire transaction
        if (trx->read_view == NULL) {
            trx->read_view = trx_sys->mvcc->view_open(trx);
        }
    }
    
    return trx->read_view;
}

// Visibility check for different isolation levels
bool
row_vers_visible(
    const rec_t*    rec,
    dict_index_t*   index,
    const ReadView* view,
    trx_t*          trx)
{
    trx_id_t    trx_id;
    
    // Get transaction ID from record
    trx_id = row_get_rec_trx_id(rec, index);
    
    if (trx->isolation_level == TRX_ISO_READ_UNCOMMITTED) {
        // See everything including uncommitted
        return true;
    }
    
    if (trx->isolation_level == TRX_ISO_READ_COMMITTED) {
        // Check if committed
        if (trx_id == trx->id) {
            return true;  // Own changes
        }
        
        // Use read view to check visibility
        return view->changes_visible(trx_id, index->table->name);
    }
    
    if (trx->isolation_level >= TRX_ISO_REPEATABLE_READ) {
        // Use consistent snapshot
        if (trx_id == trx->id) {
            return true;  // Own changes
        }
        
        // Check against snapshot
        return view->changes_visible(trx_id, index->table->name);
    }
    
    return false;
}

// Next-key locking for REPEATABLE READ
dberr_t
lock_rec_insert_check_and_lock(
    ulint           flags,
    const rec_t*    rec,
    buf_block_t*    block,
    dict_index_t*   index,
    que_thr_t*      thr,
    mtr_t*          mtr,
    bool*           inherit)
{
    trx_t* trx = thr_get_trx(thr);
    
    if (trx->isolation_level <= TRX_ISO_READ_COMMITTED) {
        // No gap locking for READ COMMITTED
        return DB_SUCCESS;
    }
    
    // Check for conflicting locks on the gap
    dberr_t err = lock_rec_lock(true, LOCK_X | LOCK_GAP | LOCK_INSERT_INTENTION,
                                block, heap_no, index, thr);
    
    if (err != DB_SUCCESS) {
        return err;
    }
    
    // For SERIALIZABLE, also take shared next-key lock
    if (trx->isolation_level == TRX_ISO_SERIALIZABLE) {
        err = lock_rec_lock(false, LOCK_S | LOCK_ORDINARY,
                           block, heap_no, index, thr);
    }
    
    return err;
}
```

---

# Microsoft SQL Server Transaction & Lock Management

## Lock Manager Specification

### SQL Server Lock Manager
```c
// SQL Server Lock Manager structure
typedef struct LockManager {
    HASH_TABLE*     lock_hash_table;     // Hash table of lock resources
    LOCK_PARTITION* partitions[LOCK_PARTITION_COUNT];
    SPINLOCK        allocation_spinlock;  // For lock block allocation
    
    // Lock escalation thresholds
    ULONG           escalation_threshold;
    ULONG           memory_threshold;
    
    // Deadlock monitor
    HANDLE          deadlock_monitor_thread;
    ULONG           deadlock_check_interval;
    
    // Statistics
    ULONGLONG       total_lock_requests;
    ULONGLONG       total_lock_waits;
    ULONGLONG       total_deadlocks;
    ULONGLONG       total_lock_timeouts;
} LockManager;

// Lock resource types
typedef enum {
    LOCK_RES_DATABASE = 1,
    LOCK_RES_FILE = 2,
    LOCK_RES_OBJECT = 3,      // Table
    LOCK_RES_PAGE = 4,
    LOCK_RES_KEY = 5,         // Index key
    LOCK_RES_EXTENT = 6,
    LOCK_RES_RID = 7,         // Row ID
    LOCK_RES_APPLICATION = 8,
    LOCK_RES_METADATA = 9,
    LOCK_RES_HOBT = 10,       // Heap or B-tree
    LOCK_RES_ALLOCATION_UNIT = 11
} LockResourceType;

// Lock modes
typedef enum {
    LOCK_MODE_NL = 0,     // No Lock
    LOCK_MODE_SCH_S = 1,  // Schema Stability
    LOCK_MODE_SCH_M = 2,  // Schema Modification
    LOCK_MODE_S = 3,      // Shared
    LOCK_MODE_U = 4,      // Update
    LOCK_MODE_X = 5,      // Exclusive
    LOCK_MODE_IS = 6,     // Intent Shared
    LOCK_MODE_IU = 7,     // Intent Update
    LOCK_MODE_IX = 8,     // Intent Exclusive
    LOCK_MODE_SIU = 9,    // Shared Intent Update
    LOCK_MODE_SIX = 10,   // Shared Intent Exclusive
    LOCK_MODE_UIX = 11,   // Update Intent Exclusive
    LOCK_MODE_BU = 12,    // Bulk Update
    LOCK_MODE_RS_S = 13,  // Range Shared-Shared
    LOCK_MODE_RS_U = 14,  // Range Shared-Update
    LOCK_MODE_RI_NL = 15, // Range Insert-Null
    LOCK_MODE_RI_S = 16,  // Range Insert-Shared
    LOCK_MODE_RI_U = 17,  // Range Insert-Update
    LOCK_MODE_RI_X = 18,  // Range Insert-Exclusive
    LOCK_MODE_RX_S = 19,  // Range Exclusive-Shared
    LOCK_MODE_RX_U = 20,  // Range Exclusive-Update
    LOCK_MODE_RX_X = 21   // Range Exclusive-Exclusive
} LockMode;

// Lock compatibility matrix (simplified)
static const BOOL LockCompatibilityMatrix[22][22] = {
    // Complex matrix - showing key patterns
    // S is compatible with S and IS
    // U is compatible with S but not U
    // X is not compatible with any other lock
    // Intent locks are compatible with each other
};

// Lock resource structure
typedef struct LockResource {
    LockResourceType type;
    union {
        struct {
            DBID     database_id;
        } database;
        struct {
            DBID     database_id;
            OBJID    object_id;
        } object;
        struct {
            DBID     database_id;
            FILEID   file_id;
            PAGEID   page_id;
        } page;
        struct {
            DBID     database_id;
            OBJID    object_id;
            INDEXID  index_id;
            HASHKEY  key_hash;
        } key;
        struct {
            DBID     database_id;
            FILEID   file_id;
            PAGEID   page_id;
            SLOTID   slot_id;
        } rid;
    } res_id;
    
    LOCK_QUEUE      granted_queue;    // Granted locks
    LOCK_QUEUE      waiting_queue;    // Waiting locks
    LOCK_QUEUE      converting_queue; // Converting locks
    ULONG           lock_count;       // Total locks on resource
    SPINLOCK        resource_spinlock;
} LockResource;

// Lock request
typedef struct LockRequest {
    SESSION*        session;          // Owning session
    TRANSACTION*    transaction;      // Owning transaction
    LockResource*   resource;         // Resource being locked
    LockMode        requested_mode;   // Requested lock mode
    LockMode        granted_mode;     // Currently granted mode
    LOCK_STATUS     status;           // Current status
    HANDLE          wait_event;       // Wait event
    ULONGLONG       wait_start_time;  // When wait started
    ULONG           wait_timeout;     // Timeout in milliseconds
    struct LockRequest* next_in_queue;
    struct LockRequest* prev_in_queue;
} LockRequest;

// Lock acquisition
LOCK_RESULT
AcquireLock(
    TRANSACTION*    transaction,
    LockResource*   resource,
    LockMode        mode,
    ULONG           timeout)
{
    LockRequest request;
    LOCK_PARTITION* partition;
    
    // Initialize request
    request.transaction = transaction;
    request.resource = resource;
    request.requested_mode = mode;
    request.granted_mode = LOCK_MODE_NL;
    request.wait_timeout = timeout;
    
    // Get partition for resource
    partition = GetLockPartition(resource);
    
    EnterCriticalSection(&partition->lock);
    
    // Check for lock escalation
    if (ShouldEscalateLock(transaction, resource)) {
        EscalateLock(transaction, resource);
    }
    
    // Fast path - no conflicting locks
    if (IsLockGrantable(&request)) {
        GrantLock(&request);
        LeaveCriticalSection(&partition->lock);
        return LOCK_GRANTED;
    }
    
    // Check for deadlock potential
    if (WouldCauseDeadlock(transaction, resource, mode)) {
        LeaveCriticalSection(&partition->lock);
        return LOCK_DEADLOCK;
    }
    
    // Must wait
    if (timeout == 0) {
        LeaveCriticalSection(&partition->lock);
        return LOCK_TIMEOUT;
    }
    
    // Add to wait queue
    AddToWaitQueue(resource, &request);
    
    LeaveCriticalSection(&partition->lock);
    
    // Wait for lock
    DWORD wait_result = WaitForSingleObject(request.wait_event, timeout);
    
    if (wait_result == WAIT_TIMEOUT) {
        RemoveFromWaitQueue(&request);
        return LOCK_TIMEOUT;
    }
    
    return request.status;
}

// Lock escalation
void
EscalateLock(
    TRANSACTION*    transaction,
    LockResource*   resource)
{
    // SQL Server escalates row/key locks to table locks
    // when a transaction holds > 5000 locks on same table
    
    if (resource->type == LOCK_RES_KEY || resource->type == LOCK_RES_RID) {
        // Count locks on same table
        ULONG lock_count = CountTableLocks(transaction, resource->res_id.key.object_id);
        
        if (lock_count > ESCALATION_THRESHOLD) {
            // Request table lock
            LockResource table_resource;
            table_resource.type = LOCK_RES_OBJECT;
            table_resource.res_id.object.database_id = resource->res_id.key.database_id;
            table_resource.res_id.object.object_id = resource->res_id.key.object_id;
            
            // Try to acquire table lock
            if (AcquireLock(transaction, &table_resource, LOCK_MODE_X, 0) == LOCK_GRANTED) {
                // Release individual locks
                ReleaseRowLocks(transaction, resource->res_id.key.object_id);
            }
        }
    }
}
```

## Deadlock Detection

### SQL Server Deadlock Monitor
```c
// Deadlock detection thread
DWORD WINAPI
DeadlockMonitorThread(LPVOID param)
{
    LockManager* manager = (LockManager*)param;
    
    while (!manager->shutdown_requested) {
        // Sleep for deadlock check interval (default 5 seconds)
        Sleep(manager->deadlock_check_interval);
        
        // Build wait-for graph
        WaitForGraph* graph = BuildWaitForGraph(manager);
        
        // Find deadlock cycles
        DeadlockCycle* cycle = FindDeadlockCycle(graph);
        
        if (cycle != NULL) {
            // Choose victim
            TRANSACTION* victim = ChooseDeadlockVictim(cycle);
            
            // Kill victim
            KillTransaction(victim, DEADLOCK_VICTIM);
            
            // Generate deadlock graph for trace
            GenerateDeadlockGraph(cycle);
            
            // Update statistics
            InterlockedIncrement(&manager->total_deadlocks);
        }
        
        FreeWaitForGraph(graph);
    }
    
    return 0;
}

// Build wait-for graph
WaitForGraph*
BuildWaitForGraph(LockManager* manager)
{
    WaitForGraph* graph = AllocateGraph();
    
    // Iterate through all lock partitions
    for (int i = 0; i < LOCK_PARTITION_COUNT; i++) {
        LOCK_PARTITION* partition = manager->partitions[i];
        
        EnterCriticalSection(&partition->lock);
        
        // Check each resource in partition
        for (LockResource* resource = partition->resources;
             resource != NULL;
             resource = resource->next) {
            
            // Check waiting queue
            for (LockRequest* waiter = resource->waiting_queue.head;
                 waiter != NULL;
                 waiter = waiter->next_in_queue) {
                
                // Find who waiter is blocked by
                for (LockRequest* holder = resource->granted_queue.head;
                     holder != NULL;
                     holder = holder->next_in_queue) {
                    
                    if (!AreLocksCompatible(waiter->requested_mode,
                                           holder->granted_mode)) {
                        // Add edge: waiter -> holder
                        AddGraphEdge(graph,
                                   waiter->transaction,
                                   holder->transaction);
                    }
                }
            }
        }
        
        LeaveCriticalSection(&partition->lock);
    }
    
    return graph;
}

// Find cycles using DFS
DeadlockCycle*
FindDeadlockCycle(WaitForGraph* graph)
{
    // Initialize colors for DFS
    for (int i = 0; i < graph->node_count; i++) {
        graph->nodes[i].color = WHITE;
    }
    
    // Run DFS from each unvisited node
    for (int i = 0; i < graph->node_count; i++) {
        if (graph->nodes[i].color == WHITE) {
            DeadlockCycle* cycle = DFSVisit(graph, &graph->nodes[i]);
            if (cycle != NULL) {
                return cycle;
            }
        }
    }
    
    return NULL;
}

// Choose deadlock victim
TRANSACTION*
ChooseDeadlockVictim(DeadlockCycle* cycle)
{
    TRANSACTION* victim = NULL;
    ULONG min_cost = MAXULONG;
    
    for (int i = 0; i < cycle->node_count; i++) {
        TRANSACTION* trans = cycle->nodes[i];
        ULONG cost = 0;
        
        // Deadlock priority (user-settable)
        cost += (10 - trans->deadlock_priority) * 1000000;
        
        // Transaction log usage
        cost += trans->log_bytes_used / 1024;
        
        // CPU time used
        cost += trans->cpu_time_ms;
        
        // Lock count
        cost += trans->lock_count * 100;
        
        if (cost < min_cost) {
            min_cost = cost;
            victim = trans;
        }
    }
    
    return victim;
}
```

## Isolation Level Implementation

### SQL Server Isolation Levels
```c
// Isolation levels
typedef enum {
    ISO_READ_UNCOMMITTED = 1,
    ISO_READ_COMMITTED = 2,
    ISO_REPEATABLE_READ = 3,
    ISO_SERIALIZABLE = 4,
    ISO_SNAPSHOT = 5,           // MVCC-based
    ISO_READ_COMMITTED_SNAPSHOT = 6  // MVCC for read committed
} IsolationLevel;

// Version store for row versioning
typedef struct VersionStore {
    BYTE*           version_store_memory;
    ULONGLONG       version_store_size;
    ULONGLONG       version_store_used;
    VERSION_CHAIN** hash_table;
    ULONG           hash_table_size;
    ULONGLONG       cleanup_version;
    ULONGLONG       generation_rate;  // Bytes/sec
} VersionStore;

// Row version structure
typedef struct RowVersion {
    TRANSACTION_ID  xsn;           // Transaction sequence number
    TIMESTAMP       timestamp;     // Version timestamp
    PAGEID          page_id;       // Original page
    SLOTID          slot_id;       // Original slot
    USHORT          record_length; // Length of versioned record
    struct RowVersion* next;       // Next in version chain
    BYTE            record_data[1]; // Versioned record data
} RowVersion;

// Setup isolation level
void
SetupTransactionIsolation(
    TRANSACTION*    transaction,
    IsolationLevel  level)
{
    transaction->isolation_level = level;
    
    switch (level) {
        case ISO_READ_UNCOMMITTED:
            // No locks for reads
            transaction->flags |= TXN_FLAG_READ_UNCOMMITTED;
            break;
            
        case ISO_READ_COMMITTED:
            // Release S locks after read
            transaction->flags |= TXN_FLAG_READ_COMMITTED;
            break;
            
        case ISO_READ_COMMITTED_SNAPSHOT:
            // Use row versioning for reads
            transaction->flags |= TXN_FLAG_READ_COMMITTED_SNAPSHOT;
            transaction->snapshot_xsn = GetCurrentXSN();
            break;
            
        case ISO_REPEATABLE_READ:
            // Hold S locks until commit
            transaction->flags |= TXN_FLAG_REPEATABLE_READ;
            break;
            
        case ISO_SERIALIZABLE:
            // Hold S locks and use key-range locks
            transaction->flags |= TXN_FLAG_SERIALIZABLE;
            break;
            
        case ISO_SNAPSHOT:
            // Full MVCC snapshot isolation
            transaction->flags |= TXN_FLAG_SNAPSHOT;
            transaction->snapshot_xsn = GetCurrentXSN();
            transaction->snapshot_timestamp = GetCurrentTimestamp();
            break;
    }
}

// Visibility check for different isolation levels
BOOL
IsRowVisible(
    TRANSACTION*    transaction,
    ROW_HEADER*     row,
    RowVersion*     version)
{
    switch (transaction->isolation_level) {
        case ISO_READ_UNCOMMITTED:
            // See everything, even uncommitted
            return TRUE;
            
        case ISO_READ_COMMITTED:
            // See only committed rows
            if (row->xmin <= transaction->xid) {
                if (!IsTransactionActive(row->xmin)) {
                    // Insert committed
                    if (row->xmax == INVALID_XID) {
                        return TRUE;  // Not deleted
                    }
                    if (row->xmax > transaction->xid ||
                        IsTransactionActive(row->xmax)) {
                        return TRUE;  // Delete not visible
                    }
                }
            }
            return FALSE;
            
        case ISO_READ_COMMITTED_SNAPSHOT:
        case ISO_SNAPSHOT:
            // Use row versioning
            if (version != NULL) {
                // Find correct version
                while (version != NULL) {
                    if (version->xsn <= transaction->snapshot_xsn) {
                        // This version is visible
                        return TRUE;
                    }
                    version = version->next;
                }
            }
            return FALSE;
            
        case ISO_REPEATABLE_READ:
        case ISO_SERIALIZABLE:
            // Check against transaction start
            if (row->xmin <= transaction->start_xsn) {
                if (WasCommittedBefore(row->xmin, transaction->start_xsn)) {
                    if (row->xmax == INVALID_XID ||
                        row->xmax > transaction->start_xsn ||
                        !WasCommittedBefore(row->xmax, transaction->start_xsn)) {
                        return TRUE;
                    }
                }
            }
            return FALSE;
    }
    
    return FALSE;
}

// Key-range locking for SERIALIZABLE
LOCK_RESULT
AcquireKeyRangeLock(
    TRANSACTION*    transaction,
    INDEX*          index,
    KEY_VALUE*      key_value,
    RANGE_TYPE      range_type)
{
    LockResource resource;
    LockMode mode;
    
    // Build lock resource for key range
    resource.type = LOCK_RES_KEY;
    resource.res_id.key.database_id = index->database_id;
    resource.res_id.key.object_id = index->object_id;
    resource.res_id.key.index_id = index->index_id;
    resource.res_id.key.key_hash = HashKey(key_value);
    
    // Determine lock mode based on operation and range type
    switch (range_type) {
        case RANGE_S_S:  // Range scan
            mode = LOCK_MODE_RS_S;
            break;
        case RANGE_S_U:  // Range scan for update
            mode = LOCK_MODE_RS_U;
            break;
        case RANGE_I_N:  // Insert range
            mode = LOCK_MODE_RI_NL;
            break;
        case RANGE_X_X:  // Exclusive range
            mode = LOCK_MODE_RX_X;
            break;
    }
    
    return AcquireLock(transaction, &resource, mode, INFINITE);
}

// Snapshot conflict detection
BOOL
CheckSnapshotConflict(
    TRANSACTION*    transaction,
    ROW_HEADER*     row)
{
    if (transaction->isolation_level != ISO_SNAPSHOT) {
        return FALSE;  // Only for snapshot isolation
    }
    
    // Check if row was modified after our snapshot
    if (row->xmin > transaction->snapshot_xsn) {
        // Row inserted after snapshot
        return TRUE;
    }
    
    if (row->xmax != INVALID_XID &&
        row->xmax > transaction->snapshot_xsn &&
        row->xmax <= GetCurrentXSN()) {
        // Row deleted/updated after snapshot
        return TRUE;
    }
    
    return FALSE;
}
```