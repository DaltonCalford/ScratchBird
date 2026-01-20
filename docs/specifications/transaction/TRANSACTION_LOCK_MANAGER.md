# ScratchBird Transaction Management - Lock Manager
## Part 2 of Transaction and Lock Management Specification

## Overview

ScratchBird's lock manager combines Firebird's lightweight locking (due to MGA reducing lock needs), PostgreSQL's comprehensive lock types including predicate locks for true serializability, and adds modern features like deadlock detection algorithms and advisory locks.

**MGA Reference:** See `MGA_RULES.md` for Multi-Generational Architecture semantics (visibility, TIP usage, recovery).

## 1. Lock Manager Architecture

### 1.1 Core Lock Manager Structure

```c
// Main lock manager structure
typedef struct sb_lock_manager {
    // Lock hash tables
    HashTable*      lm_lock_hash;          // Locks by tag
    HashTable*      lm_proc_hash;          // Locks by process
    HashTable*      lm_xact_hash;          // Locks by transaction
    
    // Lock pools
    LockPool*       lm_lock_pool;          // Lock structure pool
    LockPool*       lm_proc_lock_pool;     // Process lock pool
    LockPool*       lm_edge_pool;          // Wait-for edge pool
    
    // Deadlock detection
    DeadlockDetector* lm_deadlock_detector;
    uint32_t        lm_deadlock_timeout_ms; // Deadlock check timeout
    
    // Statistics
    LockStats       lm_stats;              // Lock statistics
    
    // Configuration
    uint32_t        lm_max_locks;          // Maximum locks
    uint32_t        lm_max_pred_locks;     // Maximum predicate locks
    uint32_t        lm_lock_timeout_ms;    // Lock acquisition timeout
    
    // Synchronization
    pthread_mutex_t lm_mutex;              // Global lock manager mutex
    pthread_cond_t  lm_wait_cv;            // Wait condition variable
} SBLockManager;

// Lock tag identifying a lockable object
typedef struct lock_tag {
    UUID            lt_database_uuid;      // Database UUID
    UUID            lt_object_uuid;        // Object UUID (table, index, etc.)
    LockType        lt_lock_type;          // Type of lock
    
    // Type-specific fields
    union {
        // Relation lock
        struct {
            ForkNumber  fork_num;           // Fork number
        } relation;
        
        // Page lock
        struct {
            ForkNumber  fork_num;           // Fork number
            BlockNumber block_num;          // Page number
        } page;
        
        // Tuple lock
        struct {
            BlockNumber block_num;          // Page number
            OffsetNumber offset_num;        // Tuple offset
        } tuple;
        
        // Transaction lock
        struct {
            TransactionId xid;              // Transaction ID
        } transaction;
        
        // Advisory lock
        struct {
            uint64_t    key1;               // User key 1
            uint64_t    key2;               // User key 2
        } advisory;
        
        // Predicate lock (for serializable)
        struct {
            PredicateType pred_type;        // Predicate type
            void*       pred_data;          // Predicate data
        } predicate;
    } lt_field;
} LockTag;

// Lock types
typedef enum lock_type {
    LOCK_TYPE_DATABASE,         // Database-level lock
    LOCK_TYPE_SCHEMA,          // Schema-level lock
    LOCK_TYPE_RELATION,        // Table-level lock
    LOCK_TYPE_PAGE,            // Page-level lock
    LOCK_TYPE_TUPLE,           // Row-level lock
    LOCK_TYPE_TRANSACTION,     // Transaction lock
    LOCK_TYPE_VIRTUALXID,      // Virtual transaction ID
    LOCK_TYPE_OBJECT,          // Generic object lock
    LOCK_TYPE_USERLOCK,        // User/advisory lock
    LOCK_TYPE_PREDICATE        // Predicate lock (serializable)
} LockType;
```

### 1.2 Lock Modes and Compatibility

```c
// Lock modes (PostgreSQL-inspired but simplified)
typedef enum lock_mode {
    NO_LOCK = 0,                // No lock
    ACCESS_SHARE,               // SELECT
    ROW_SHARE,                  // SELECT FOR UPDATE
    ROW_EXCLUSIVE,              // UPDATE, DELETE, INSERT
    SHARE_UPDATE_EXCLUSIVE,     // Sweep/GC, CREATE INDEX CONCURRENTLY
    SHARE,                      // CREATE INDEX
    SHARE_ROW_EXCLUSIVE,        // Like EXCLUSIVE but allows ROW SHARE
    EXCLUSIVE,                  // Blocks ROW SHARE/SELECT FOR UPDATE
    ACCESS_EXCLUSIVE            // ALTER TABLE, DROP TABLE, exclusive maintenance
} LockMode;

// Lock compatibility matrix
static const bool lock_compatibility[9][9] = {
    //         NONE  AS    RS    RE    SUE   S     SRE   E     AE
    /* NONE */ {1,    1,    1,    1,    1,    1,    1,    1,    1},
    /* AS   */ {1,    1,    1,    1,    1,    1,    1,    0,    0},
    /* RS   */ {1,    1,    1,    1,    1,    1,    0,    0,    0},
    /* RE   */ {1,    1,    1,    0,    0,    0,    0,    0,    0},
    /* SUE  */ {1,    1,    1,    0,    0,    0,    0,    0,    0},
    /* S    */ {1,    1,    1,    0,    0,    1,    0,    0,    0},
    /* SRE  */ {1,    1,    0,    0,    0,    0,    0,    0,    0},
    /* E    */ {1,    0,    0,    0,    0,    0,    0,    0,    0},
    /* AE   */ {1,    0,    0,    0,    0,    0,    0,    0,    0}
};

// Check if lock modes are compatible
bool are_locks_compatible(LockMode held, LockMode requested) {
    return lock_compatibility[held][requested];
}

// Get strongest lock mode from list
LockMode get_strongest_lock_mode(LockMode* modes, int count) {
    LockMode strongest = NO_LOCK;
    
    for (int i = 0; i < count; i++) {
        if (modes[i] > strongest) {
            strongest = modes[i];
        }
    }
    
    return strongest;
}
```

### 1.3 Lock Structure

```c
// Lock structure representing a lockable object
typedef struct lock_object {
    LockTag         lock_tag;              // Lock identifier
    
    // Granted locks
    ProcLockList*   lock_granted;          // List of granted locks
    uint32_t        lock_n_granted;        // Number granted
    uint32_t        lock_granted_modes;    // Bitmask of granted modes
    
    // Waiting locks
    ProcLockList*   lock_waiters;          // List of waiting locks
    uint32_t        lock_n_waiting;        // Number waiting
    uint32_t        lock_wait_modes;       // Bitmask of wait modes
    
    // Statistics
    uint64_t        lock_acquisitions;     // Total acquisitions
    uint64_t        lock_waits;            // Total waits
    uint64_t        lock_timeouts;         // Total timeouts
    
    // Hash chain
    struct lock_object* lock_hash_next;    // Next in hash chain
} LockObject;

// Process lock structure (lock held by a process)
typedef struct proc_lock {
    // Identity
    ProcessId       pl_pid;                // Process ID
    TransactionId   pl_xid;                // Transaction ID
    LockObject*     pl_lock;               // Lock object
    
    // Lock mode
    LockMode        pl_mode;               // Granted mode
    LockMode        pl_wait_mode;          // Waiting for mode
    
    // State
    bool            pl_granted;            // Is granted
    TimestampTz     pl_grant_time;         // When granted
    TimestampTz     pl_wait_start;         // When started waiting
    
    // Wait queue
    struct proc_lock* pl_wait_next;        // Next waiter
    struct proc_lock* pl_wait_prev;        // Previous waiter
    
    // Process list
    struct proc_lock* pl_proc_next;        // Next for this process
    struct proc_lock* pl_proc_prev;        // Previous for this process
} ProcLock;
```

## 2. Lock Acquisition and Release

### 2.1 Lock Acquisition

```c
// Acquire lock
LockResult acquire_lock(
    SBLockManager* lm,
    LockTag* tag,
    LockMode mode,
    bool wait,
    bool dontWait)
{
    pthread_mutex_lock(&lm->lm_mutex);
    
    // Find or create lock object
    LockObject* lock = find_or_create_lock(lm, tag);
    
    // Get current process lock
    ProcessId pid = getpid();
    TransactionId xid = GetCurrentTransactionId();
    ProcLock* proc_lock = find_proc_lock(lock, pid, xid);
    
    if (proc_lock != NULL && proc_lock->pl_granted) {
        // Already hold a lock - check for upgrade
        if (proc_lock->pl_mode >= mode) {
            // Already have strong enough lock
            pthread_mutex_unlock(&lm->lm_mutex);
            return LOCK_OK;
        }
        
        // Need to upgrade lock
        if (can_upgrade_lock(lock, proc_lock, mode)) {
            upgrade_lock(proc_lock, mode);
            pthread_mutex_unlock(&lm->lm_mutex);
            return LOCK_OK;
        }
    }
    
    // Create new process lock
    if (proc_lock == NULL) {
        proc_lock = create_proc_lock(lm, pid, xid, lock, mode);
    }
    
    // Check if can grant immediately
    if (can_grant_lock(lock, mode)) {
        grant_lock(lock, proc_lock, mode);
        pthread_mutex_unlock(&lm->lm_mutex);
        return LOCK_OK;
    }
    
    // Cannot grant immediately
    if (dontWait) {
        pthread_mutex_unlock(&lm->lm_mutex);
        return LOCK_NOT_AVAILABLE;
    }
    
    if (!wait) {
        pthread_mutex_unlock(&lm->lm_mutex);
        return LOCK_NOT_AVAILABLE;
    }
    
    // Add to wait queue
    add_to_wait_queue(lock, proc_lock, mode);
    
    // Wait for lock with timeout
    struct timespec timeout;
    clock_gettime(CLOCK_REALTIME, &timeout);
    timeout.tv_sec += lm->lm_lock_timeout_ms / 1000;
    timeout.tv_nsec += (lm->lm_lock_timeout_ms % 1000) * 1000000;
    
    int wait_result = pthread_cond_timedwait(&lm->lm_wait_cv, 
                                             &lm->lm_mutex, 
                                             &timeout);
    
    if (wait_result == ETIMEDOUT) {
        // Timeout - check for deadlock
        if (is_deadlock_detected(lm, proc_lock)) {
            remove_from_wait_queue(lock, proc_lock);
            pthread_mutex_unlock(&lm->lm_mutex);
            return LOCK_DEADLOCK;
        }
        
        remove_from_wait_queue(lock, proc_lock);
        pthread_mutex_unlock(&lm->lm_mutex);
        return LOCK_TIMEOUT;
    }
    
    // Woken up - check if granted
    if (proc_lock->pl_granted) {
        pthread_mutex_unlock(&lm->lm_mutex);
        return LOCK_OK;
    }
    
    // Not granted - must have been deadlock victim
    pthread_mutex_unlock(&lm->lm_mutex);
    return LOCK_DEADLOCK;
}

// Check if lock can be granted
bool can_grant_lock(LockObject* lock, LockMode mode) {
    // Check against all granted locks
    for (ProcLock* granted : lock->lock_granted) {
        if (!are_locks_compatible(granted->pl_mode, mode)) {
            return false;
        }
    }
    
    // Check if any waiters have priority
    for (ProcLock* waiter : lock->lock_waiters) {
        if (waiter->pl_wait_mode > mode) {
            // Stronger lock waiting - don't jump queue
            return false;
        }
    }
    
    return true;
}

// Grant lock to process
void grant_lock(LockObject* lock, ProcLock* proc_lock, LockMode mode) {
    proc_lock->pl_mode = mode;
    proc_lock->pl_granted = true;
    proc_lock->pl_grant_time = GetCurrentTimestamp();
    
    // Add to granted list
    add_to_granted_list(lock, proc_lock);
    
    // Update granted modes bitmask
    lock->lock_granted_modes |= (1 << mode);
    lock->lock_n_granted++;
    
    // Update statistics
    lock->lock_acquisitions++;
}
```

### 2.2 Lock Release

```c
// Release lock
LockResult release_lock(
    SBLockManager* lm,
    LockTag* tag,
    LockMode mode)
{
    pthread_mutex_lock(&lm->lm_mutex);
    
    // Find lock object
    LockObject* lock = find_lock(lm, tag);
    
    if (lock == NULL) {
        pthread_mutex_unlock(&lm->lm_mutex);
        return LOCK_NOT_FOUND;
    }
    
    // Find process lock
    ProcessId pid = getpid();
    TransactionId xid = GetCurrentTransactionId();
    ProcLock* proc_lock = find_proc_lock(lock, pid, xid);
    
    if (proc_lock == NULL || !proc_lock->pl_granted) {
        pthread_mutex_unlock(&lm->lm_mutex);
        return LOCK_NOT_HELD;
    }
    
    // Remove from granted list
    remove_from_granted_list(lock, proc_lock);
    lock->lock_n_granted--;
    
    // Update granted modes bitmask if needed
    if (!has_granted_mode(lock, proc_lock->pl_mode)) {
        lock->lock_granted_modes &= ~(1 << proc_lock->pl_mode);
    }
    
    // Free process lock
    free_proc_lock(lm, proc_lock);
    
    // Wake up waiters that can now be granted
    wake_compatible_waiters(lm, lock);
    
    // If no more locks on object, remove it
    if (lock->lock_n_granted == 0 && lock->lock_n_waiting == 0) {
        remove_lock_object(lm, lock);
    }
    
    pthread_mutex_unlock(&lm->lm_mutex);
    
    return LOCK_OK;
}

// Wake up compatible waiters
void wake_compatible_waiters(
    SBLockManager* lm,
    LockObject* lock)
{
    ProcLock* waiter = lock->lock_waiters;
    
    while (waiter != NULL) {
        ProcLock* next = waiter->pl_wait_next;
        
        if (can_grant_lock(lock, waiter->pl_wait_mode)) {
            // Remove from wait queue
            remove_from_wait_queue(lock, waiter);
            
            // Grant lock
            grant_lock(lock, waiter, waiter->pl_wait_mode);
            
            // Wake up process
            pthread_cond_signal(&lm->lm_wait_cv);
        }
        
        waiter = next;
    }
}
```

## 3. Deadlock Detection

### 3.1 Wait-For Graph

```c
// Wait-for graph for deadlock detection
typedef struct wait_for_graph {
    // Nodes (processes)
    WFGNode**       wfg_nodes;             // Array of nodes
    uint32_t        wfg_n_nodes;           // Number of nodes
    
    // Edges (wait-for relationships)
    WFGEdge**       wfg_edges;             // Array of edges
    uint32_t        wfg_n_edges;           // Number of edges
    
    // Detection state
    uint32_t        wfg_visit_mark;        // Visit marker
    bool            wfg_cycle_found;       // Cycle detected
    ProcessId*      wfg_cycle;             // Cycle participants
    uint32_t        wfg_cycle_length;      // Cycle length
} WaitForGraph;

// Wait-for graph node
typedef struct wfg_node {
    ProcessId       node_pid;              // Process ID
    TransactionId   node_xid;              // Transaction ID
    
    // Edges
    WFGEdge*        node_out_edges;        // Outgoing edges (waiting for)
    WFGEdge*        node_in_edges;         // Incoming edges (waited by)
    
    // Detection state
    uint32_t        node_visit_mark;       // Visit marker
    bool            node_on_stack;         // On DFS stack
} WFGNode;

// Wait-for graph edge
typedef struct wfg_edge {
    WFGNode*        edge_from;             // Waiting process
    WFGNode*        edge_to;               // Waited process
    LockObject*     edge_lock;             // Lock being waited for
    
    // Chain
    struct wfg_edge* edge_next_out;        // Next outgoing edge
    struct wfg_edge* edge_next_in;         // Next incoming edge
} WFGEdge;

// Build wait-for graph
WaitForGraph* build_wait_for_graph(SBLockManager* lm) {
    WaitForGraph* wfg = allocate(sizeof(WaitForGraph));
    
    // Create nodes for all processes with locks
    for (ProcLock* pl : lm->all_proc_locks) {
        WFGNode* node = find_or_create_node(wfg, pl->pl_pid, pl->pl_xid);
        
        // If waiting, create edge to lock holders
        if (!pl->pl_granted && pl->pl_wait_mode != NO_LOCK) {
            LockObject* lock = pl->pl_lock;
            
            for (ProcLock* holder : lock->lock_granted) {
                if (!are_locks_compatible(holder->pl_mode, pl->pl_wait_mode)) {
                    WFGNode* holder_node = find_or_create_node(wfg, 
                                                              holder->pl_pid,
                                                              holder->pl_xid);
                    create_edge(wfg, node, holder_node, lock);
                }
            }
        }
    }
    
    return wfg;
}

// Detect deadlock using DFS
bool detect_deadlock_dfs(WaitForGraph* wfg) {
    wfg->wfg_visit_mark++;
    
    for (uint32_t i = 0; i < wfg->wfg_n_nodes; i++) {
        WFGNode* node = wfg->wfg_nodes[i];
        
        if (node->node_visit_mark != wfg->wfg_visit_mark) {
            if (visit_node_dfs(wfg, node)) {
                return true;  // Cycle found
            }
        }
    }
    
    return false;  // No cycle
}

// DFS visit node
bool visit_node_dfs(WaitForGraph* wfg, WFGNode* node) {
    node->node_visit_mark = wfg->wfg_visit_mark;
    node->node_on_stack = true;
    
    // Check all outgoing edges
    for (WFGEdge* edge = node->node_out_edges; edge; edge = edge->edge_next_out) {
        WFGNode* target = edge->edge_to;
        
        if (target->node_on_stack) {
            // Found cycle
            wfg->wfg_cycle_found = true;
            record_cycle(wfg, node, target);
            return true;
        }
        
        if (target->node_visit_mark != wfg->wfg_visit_mark) {
            if (visit_node_dfs(wfg, target)) {
                return true;
            }
        }
    }
    
    node->node_on_stack = false;
    return false;
}
```

### 3.2 Deadlock Resolution

```c
// Deadlock detector
typedef struct deadlock_detector {
    // Configuration
    uint32_t        dd_check_interval_ms;  // Check interval
    DeadlockMethod  dd_method;             // Detection method
    
    // State
    pthread_t       dd_thread;             // Detector thread
    bool            dd_active;             // Detector active
    
    // Statistics
    uint64_t        dd_checks;             // Checks performed
    uint64_t        dd_deadlocks_found;    // Deadlocks found
    uint64_t        dd_victims_chosen;     // Victims chosen
} DeadlockDetector;

// Deadlock detection methods
typedef enum deadlock_method {
    DEADLOCK_WAIT_FOR_GRAPH,       // Traditional wait-for graph
    DEADLOCK_WOUND_WAIT,           // Wound-wait (timestamp-based)
    DEADLOCK_WAIT_DIE,             // Wait-die (timestamp-based)
    DEADLOCK_TIMEOUT_ONLY          // Simple timeout
} DeadlockMethod;

// Deadlock detector main loop
void* deadlock_detector_main(void* arg) {
    SBLockManager* lm = (SBLockManager*)arg;
    DeadlockDetector* dd = lm->lm_deadlock_detector;
    
    while (dd->dd_active) {
        usleep(dd->dd_check_interval_ms * 1000);
        
        pthread_mutex_lock(&lm->lm_mutex);
        
        switch (dd->dd_method) {
            case DEADLOCK_WAIT_FOR_GRAPH:
                check_deadlock_wait_for_graph(lm);
                break;
                
            case DEADLOCK_WOUND_WAIT:
                check_deadlock_wound_wait(lm);
                break;
                
            case DEADLOCK_WAIT_DIE:
                check_deadlock_wait_die(lm);
                break;
                
            case DEADLOCK_TIMEOUT_ONLY:
                // Just use timeouts
                break;
        }
        
        dd->dd_checks++;
        
        pthread_mutex_unlock(&lm->lm_mutex);
    }
    
    return NULL;
}

// Check for deadlock using wait-for graph
void check_deadlock_wait_for_graph(SBLockManager* lm) {
    // Build wait-for graph
    WaitForGraph* wfg = build_wait_for_graph(lm);
    
    // Detect cycles
    if (detect_deadlock_dfs(wfg)) {
        // Choose victim
        ProcessId victim = choose_deadlock_victim(wfg);
        
        // Kill victim
        kill_deadlock_victim(lm, victim);
        
        lm->lm_deadlock_detector->dd_deadlocks_found++;
        lm->lm_deadlock_detector->dd_victims_chosen++;
    }
    
    free_wait_for_graph(wfg);
}

// Choose deadlock victim
ProcessId choose_deadlock_victim(WaitForGraph* wfg) {
    // Choose victim with lowest cost
    ProcessId victim = 0;
    uint64_t min_cost = UINT64_MAX;
    
    for (uint32_t i = 0; i < wfg->wfg_cycle_length; i++) {
        ProcessId pid = wfg->wfg_cycle[i];
        uint64_t cost = calculate_victim_cost(pid);
        
        if (cost < min_cost) {
            min_cost = cost;
            victim = pid;
        }
    }
    
    return victim;
}

// Calculate cost of choosing process as victim
uint64_t calculate_victim_cost(ProcessId pid) {
    SBTransaction* txn = get_transaction_by_pid(pid);
    
    if (txn == NULL) {
        return 0;
    }
    
    uint64_t cost = 0;
    
    // Consider work done
    cost += txn->txn_tuples_inserted * 10;
    cost += txn->txn_tuples_updated * 20;
    cost += txn->txn_tuples_deleted * 15;
    
    // Consider time running
    TimestampTz now = GetCurrentTimestamp();
    cost += (now - txn->txn_start_time) / 1000;  // Milliseconds
    
    // Consider lock count
    cost += list_length(txn->txn_locks) * 5;
    
    return cost;
}
```

## 4. Predicate Locking (for Serializable Isolation)

### 4.1 Predicate Lock Structure

```c
// Predicate lock for serializable isolation
typedef struct predicate_lock {
    // Lock identity
    PredicateLockTag pl_tag;               // Predicate lock tag
    
    // Predicate definition
    PredicateType   pl_type;               // Type of predicate
    void*           pl_predicate;          // Predicate data
    
    // Holding transaction
    TransactionId   pl_xid;                // Transaction ID
    
    // Conflict tracking
    TransactionId*  pl_conflicts;          // Conflicting XIDs
    uint32_t        pl_n_conflicts;        // Number of conflicts
    
    // Chain
    struct predicate_lock* pl_next;        // Next in hash
} PredicateLock;

// Predicate lock tag
typedef struct predicate_lock_tag {
    UUID            plt_relation_uuid;     // Relation UUID
    PredicateType   plt_type;              // Predicate type
    
    union {
        // Range predicate
        struct {
            Datum       lower_bound;        // Lower bound
            Datum       upper_bound;        // Upper bound
            bool        lower_inclusive;   // Include lower
            bool        upper_inclusive;   // Include upper
        } range;
        
        // Page predicate
        struct {
            BlockNumber page_num;           // Page number
        } page;
        
        // Relation predicate
        struct {
            // Entire relation locked
        } relation;
    } plt_data;
} PredicateLockTag;

// Predicate types
typedef enum predicate_type {
    PREDICATE_RELATION,     // Entire relation
    PREDICATE_PAGE,         // Entire page
    PREDICATE_TUPLE,        // Single tuple
    PREDICATE_RANGE,        // Range of values
    PREDICATE_GAP           // Gap between values
} PredicateType;
```

### 4.2 Predicate Lock Operations

```c
// Acquire predicate lock
Status acquire_predicate_lock(
    SBLockManager* lm,
    PredicateLockTag* tag,
    TransactionId xid)
{
    // Only for serializable transactions
    if (GetCurrentIsolationLevel() != ISOLATION_SERIALIZABLE) {
        return STATUS_OK;
    }
    
    pthread_mutex_lock(&lm->lm_mutex);
    
    // Check for conflicts with existing predicate locks
    PredicateLock* existing = find_predicate_locks(lm, tag);
    
    while (existing != NULL) {
        if (existing->pl_xid != xid) {
            // Different transaction - potential conflict
            if (predicate_locks_conflict(tag, &existing->pl_tag)) {
                // Record conflict
                record_predicate_conflict(existing, xid);
                record_predicate_conflict_reverse(xid, existing->pl_xid);
            }
        }
        existing = existing->pl_next;
    }
    
    // Create new predicate lock
    PredicateLock* lock = allocate(sizeof(PredicateLock));
    lock->pl_tag = *tag;
    lock->pl_xid = xid;
    lock->pl_conflicts = NULL;
    lock->pl_n_conflicts = 0;
    
    // Add to hash table
    add_predicate_lock(lm, lock);
    
    pthread_mutex_unlock(&lm->lm_mutex);
    
    return STATUS_OK;
}

// Check if predicate locks conflict
bool predicate_locks_conflict(
    PredicateLockTag* tag1,
    PredicateLockTag* tag2)
{
    // Different relations never conflict
    if (!UUID_equals(tag1->plt_relation_uuid, tag2->plt_relation_uuid)) {
        return false;
    }
    
    // Check based on predicate types
    if (tag1->plt_type == PREDICATE_RELATION ||
        tag2->plt_type == PREDICATE_RELATION) {
        // Relation lock conflicts with everything on that relation
        return true;
    }
    
    if (tag1->plt_type == PREDICATE_PAGE &&
        tag2->plt_type == PREDICATE_PAGE) {
        // Page locks conflict if same page
        return tag1->plt_data.page.page_num == 
               tag2->plt_data.page.page_num;
    }
    
    if (tag1->plt_type == PREDICATE_RANGE &&
        tag2->plt_type == PREDICATE_RANGE) {
        // Range locks conflict if ranges overlap
        return ranges_overlap(&tag1->plt_data.range,
                            &tag2->plt_data.range);
    }
    
    // Add more conflict checks as needed
    return false;
}

// Check for serialization conflicts at commit
bool check_serialization_conflicts(SBTransaction* txn) {
    // Get all predicate locks held by this transaction
    PredicateLockList* my_locks = get_transaction_predicate_locks(txn->txn_id);
    
    // Check each lock for dangerous structures
    for (PredicateLock* lock : my_locks) {
        if (has_dangerous_structure(lock, txn->txn_id)) {
            return false;  // Would violate serializability
        }
    }
    
    return true;  // Safe to commit
}

// Check for dangerous structure (rw-conflicts forming cycle)
bool has_dangerous_structure(
    PredicateLock* lock,
    TransactionId my_xid)
{
    // Look for pattern: T1 --rw--> T2 --rw--> T3 --rw--> T1
    // where we are one of the transactions
    
    for (uint32_t i = 0; i < lock->pl_n_conflicts; i++) {
        TransactionId conflict_xid = lock->pl_conflicts[i];
        
        // Check if conflict transaction has conflicts back to us
        if (has_path_back_to(conflict_xid, my_xid, 2)) {
            return true;  // Found dangerous structure
        }
    }
    
    return false;
}
```

## 5. Advisory Locks

### 5.1 Advisory Lock Implementation

```c
// Advisory (user) locks
typedef struct advisory_lock {
    // Lock key
    uint64_t        al_key1;               // User key 1
    uint64_t        al_key2;               // User key 2
    
    // Lock mode
    LockMode        al_mode;               // Lock mode
    
    // Owner
    ProcessId       al_owner_pid;          // Owner process
    SessionId       al_session_id;         // Session ID
    
    // Shared/exclusive
    bool            al_shared;             // Is shared lock
    uint32_t        al_share_count;        // Share count
    
    // Chain
    struct advisory_lock* al_next;         // Next in hash
} AdvisoryLock;

// Acquire advisory lock
bool pg_advisory_lock(uint64_t key1, uint64_t key2, bool shared) {
    SBLockManager* lm = get_lock_manager();
    
    LockTag tag;
    tag.lt_lock_type = LOCK_TYPE_USERLOCK;
    tag.lt_field.advisory.key1 = key1;
    tag.lt_field.advisory.key2 = key2;
    
    LockMode mode = shared ? ACCESS_SHARE : ACCESS_EXCLUSIVE;
    
    LockResult result = acquire_lock(lm, &tag, mode, true, false);
    
    return result == LOCK_OK;
}

// Try advisory lock (non-blocking)
bool pg_try_advisory_lock(uint64_t key1, uint64_t key2, bool shared) {
    SBLockManager* lm = get_lock_manager();
    
    LockTag tag;
    tag.lt_lock_type = LOCK_TYPE_USERLOCK;
    tag.lt_field.advisory.key1 = key1;
    tag.lt_field.advisory.key2 = key2;
    
    LockMode mode = shared ? ACCESS_SHARE : ACCESS_EXCLUSIVE;
    
    LockResult result = acquire_lock(lm, &tag, mode, false, true);
    
    return result == LOCK_OK;
}

// Release advisory lock
bool pg_advisory_unlock(uint64_t key1, uint64_t key2, bool shared) {
    SBLockManager* lm = get_lock_manager();
    
    LockTag tag;
    tag.lt_lock_type = LOCK_TYPE_USERLOCK;
    tag.lt_field.advisory.key1 = key1;
    tag.lt_field.advisory.key2 = key2;
    
    LockMode mode = shared ? ACCESS_SHARE : ACCESS_EXCLUSIVE;
    
    LockResult result = release_lock(lm, &tag, mode);
    
    return result == LOCK_OK;
}

// Release all advisory locks for session
void pg_advisory_unlock_all() {
    SBLockManager* lm = get_lock_manager();
    SessionId session = GetCurrentSessionId();
    
    pthread_mutex_lock(&lm->lm_mutex);
    
    // Find all advisory locks for this session
    List* locks_to_release = NIL;
    
    for (AdvisoryLock* lock : lm->advisory_locks) {
        if (lock->al_session_id == session) {
            locks_to_release = lappend(locks_to_release, lock);
        }
    }
    
    pthread_mutex_unlock(&lm->lm_mutex);
    
    // Release them
    for (AdvisoryLock* lock : locks_to_release) {
        pg_advisory_unlock(lock->al_key1, lock->al_key2, lock->al_shared);
    }
    
    list_free(locks_to_release);
}
```

## 6. Lock Monitoring and Diagnostics

### 6.1 Lock Statistics

```c
// Lock statistics
typedef struct lock_stats {
    // Acquisition statistics
    uint64_t        ls_acquisitions;       // Total acquisitions
    uint64_t        ls_immediate_grants;   // Immediate grants
    uint64_t        ls_waits;              // Had to wait
    uint64_t        ls_timeouts;           // Timed out
    uint64_t        ls_deadlocks;          // Deadlocked
    
    // Wait time statistics
    uint64_t        ls_total_wait_time;    // Total wait time (ms)
    uint64_t        ls_max_wait_time;      // Maximum wait time
    double          ls_avg_wait_time;      // Average wait time
    
    // Lock type breakdown
    uint64_t        ls_by_type[LOCK_TYPE_COUNT];
    
    // Lock mode breakdown
    uint64_t        ls_by_mode[LOCK_MODE_COUNT];
    
    // Deadlock statistics
    uint64_t        ls_deadlock_checks;    // Checks performed
    uint64_t        ls_deadlocks_found;    // Deadlocks found
    uint64_t        ls_victims_chosen;     // Victims chosen
} LockStats;

// Get lock statistics
LockStats* get_lock_statistics(SBLockManager* lm) {
    LockStats* stats = allocate(sizeof(LockStats));
    
    pthread_mutex_lock(&lm->lm_mutex);
    
    memcpy(stats, &lm->lm_stats, sizeof(LockStats));
    
    // Calculate averages
    if (stats->ls_waits > 0) {
        stats->ls_avg_wait_time = (double)stats->ls_total_wait_time / 
                                  stats->ls_waits;
    }
    
    pthread_mutex_unlock(&lm->lm_mutex);
    
    return stats;
}

// Lock monitoring view
typedef struct lock_view_entry {
    // Lock information
    LockType        lv_lock_type;          // Type of lock
    UUID            lv_database_uuid;      // Database
    UUID            lv_relation_uuid;      // Relation (if applicable)
    BlockNumber     lv_page;               // Page (if applicable)
    OffsetNumber    lv_tuple;              // Tuple (if applicable)
    TransactionId   lv_virtualxid;         // Virtual XID (if applicable)
    
    // Process information
    ProcessId       lv_pid;                // Process ID
    TransactionId   lv_xid;                // Transaction ID
    
    // Lock state
    LockMode        lv_mode;               // Lock mode
    bool            lv_granted;            // Is granted
    TimestampTz     lv_grant_time;         // When granted
    TimestampTz     lv_wait_start;         // When started waiting
} LockViewEntry;

SQL exposure:
- `sys.locks` view (see `operations/MONITORING_SQL_VIEWS.md`)

// Get current locks (for pg_locks view)
List* get_all_locks(SBLockManager* lm) {
    List* result = NIL;
    
    pthread_mutex_lock(&lm->lm_mutex);
    
    // Iterate through all lock objects
    for (LockObject* lock : lm->all_locks) {
        // Add granted locks
        for (ProcLock* pl : lock->lock_granted) {
            LockViewEntry* entry = create_lock_view_entry(lock, pl, true);
            result = lappend(result, entry);
        }
        
        // Add waiting locks
        for (ProcLock* pl : lock->lock_waiters) {
            LockViewEntry* entry = create_lock_view_entry(lock, pl, false);
            result = lappend(result, entry);
        }
    }
    
    pthread_mutex_unlock(&lm->lm_mutex);
    
    return result;
}
```

## 7. Integration with MGA

### 7.1 Reduced Locking with MGA

```c
// MGA reduces lock requirements
typedef struct mga_lock_optimization {
    // Read operations need no locks with MGA
    bool            mlo_readers_need_locks;    // false
    
    // Writers only conflict with other writers
    bool            mlo_writers_block_readers; // false
    bool            mlo_readers_block_writers; // false
    bool            mlo_writers_block_writers; // true
    
    // Lock granularity can be coarser
    LockGranularity mlo_preferred_granularity; // PAGE or TABLE
} MGALockOptimization;

// Optimized lock acquisition for MGA
LockResult acquire_lock_mga_optimized(
    SBLockManager* lm,
    LockTag* tag,
    LockMode mode,
    bool is_read_only)
{
    // Readers don't need locks with MGA
    if (is_read_only && mode <= ROW_SHARE) {
        return LOCK_OK;  // No lock needed
    }
    
    // Writers need lighter locks
    if (mode == ROW_EXCLUSIVE) {
        // Only need to prevent other writers
        mode = ROW_SHARE;  // Downgrade lock requirement
    }
    
    // Proceed with normal lock acquisition
    return acquire_lock(lm, tag, mode, true, false);
}

// Check if operation needs lock with MGA
bool needs_lock_with_mga(
    OperationType op_type,
    IsolationLevel isolation)
{
    switch (op_type) {
        case OP_SELECT:
            // SELECT never needs locks with MGA
            return false;
            
        case OP_INSERT:
        case OP_UPDATE:
        case OP_DELETE:
            // DML needs locks to coordinate writers
            return true;
            
        case OP_DDL:
            // DDL always needs locks
            return true;
            
        default:
            return true;
    }
}
```

## Implementation Notes

This lock manager implementation provides:

1. **Comprehensive lock types** including relation, page, tuple, and predicate locks
2. **Multiple lock modes** with a complete compatibility matrix
3. **Deadlock detection** using wait-for graph, wound-wait, or wait-die
4. **Predicate locking** for true serializable isolation
5. **Advisory locks** for application-level coordination
6. **Lock monitoring** and statistics for diagnostics
7. **MGA optimization** reducing lock requirements significantly

The lock manager works in conjunction with MGA to provide high concurrency with minimal locking overhead. Readers never block writers and vice versa, with locks primarily used to coordinate multiple writers.
