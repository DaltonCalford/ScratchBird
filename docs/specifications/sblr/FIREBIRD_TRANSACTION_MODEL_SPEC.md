# Firebird Transaction Model Technical Specification

**Date:** October 7, 2025
**Status:** Technical Specification for ScratchBird Implementation
**Version:** 1.0
**Based On:** Firebird 4.0+ Transaction Architecture

---

## Executive Summary

This specification documents Firebird's transaction implementation model for adoption in ScratchBird. Firebird's Multi-Generational Architecture (MGA) provides robust MVCC with proven mechanisms for managing long-running transactions, garbage collection, and transaction isolation.

**Key Adoption:** ScratchBird will implement Firebird's approach to:
- Transaction markers (OIT, OAT, OST, Next)
- Sweep mechanism for garbage collection
- Configurable handling of long-standing transactions
- Isolation levels (SNAPSHOT, SNAPSHOT TABLE STABILITY, READ COMMITTED)
- Transaction parameter buffer (TPB) configuration

---

## Part 1: Multi-Generational Architecture (MGA)

### 1.1 Core Concept

Firebird uses Multi-Generational Architecture (MGA) where:
- Database contains multiple versions of data rows
- Updates create new versions rather than overwriting
- Old versions remain until no transaction needs them
- Readers don't block writers, writers don't block readers

### 1.2 Record Versioning

**Version Chain Structure:**
```
Current Version (newest)
    |
    v
Back Version 1
    |
    v
Back Version 2
    |
    v
... (older versions)
```

**Each Record Version Contains:**
- Transaction ID (XID) that created this version
- Transaction ID (XID) that deleted/updated this version (if any)
- Pointer to previous version (delta/back record)
- Actual data payload

**Example Lifecycle:**
```
T1: INSERT row (creates version V1 with xmin=100)
T2: UPDATE row (creates V2 with xmin=200, V1.xmax=200)
T3: UPDATE row (creates V3 with xmin=300, V2.xmax=300)

Version chain: V3 -> V2 -> V1

Active transactions see appropriate version based on visibility rules.
```

### 1.3 Benefits

1. **No Read Locks:** Readers always see consistent snapshot
2. **Cheap Rollback:** Just mark transaction as rolled back, versions cleanup later
3. **Point-in-Time Consistency:** Each transaction sees database as of its start time
4. **High Concurrency:** Multiple writers don't block each other (unless same row)

### 1.4 Tradeoffs

1. **Database Growth:** Old versions accumulate, require cleanup (sweep)
2. **Long Transactions:** Block removal of old versions, causing growth
3. **Garbage Collection:** Requires maintenance to reclaim space
4. **Index Maintenance:** Multiple versions mean larger indexes

---

## Part 2: Transaction Markers

### 2.1 The Four Transaction Markers

Firebird maintains four critical transaction IDs in the database header:

#### 2.1.1 OIT (Oldest Interesting Transaction)

**Definition:** The oldest transaction ID that is NOT in committed state.

**Purpose:** Marks the boundary below which all transactions are committed.

**Significance:**
- Record versions created by transactions < OIT can be garbage collected
- All back versions with xmax < OIT are candidates for removal
- Critical for sweep operation

**Example:**
```
Transaction states in TIP:
100: committed
101: committed
102: active     <-- OIT (first non-committed)
103: committed
104: active
105: committed
```

#### 2.1.2 OAT (Oldest Active Transaction)

**Definition:** The oldest transaction ID in active state.

**Purpose:** Identifies the oldest currently running transaction.

**Significance:**
- Transaction has not committed or rolled back
- May still be reading/writing data
- Helps identify long-running transactions

**Example:**
```
Transaction states:
100-101: committed
102: rolled back
103: active     <-- OAT (first active)
104: committed
105: active
```

#### 2.1.3 OST (Oldest Snapshot Transaction)

**Definition:** The oldest transaction ID that started with SNAPSHOT isolation.

**Purpose:** Critical for sweep interval calculation.

**Significance:**
- SNAPSHOT transactions see database as of their start time
- Long-running SNAPSHOT transaction blocks cleanup of all versions it might see
- **Sweep trigger:** When (OST - OIT) > sweep_interval

**Why OST, not OAT?**
- READ COMMITTED transactions don't need old versions (they see latest committed)
- Only SNAPSHOT transactions require old versions preserved
- OST more accurately reflects garbage collection pressure

#### 2.1.4 NEXT Transaction ID

**Definition:** The transaction ID to be assigned to the next transaction.

**Purpose:** Transaction ID counter.

**Formula:** `Next = (highest assigned XID) + 1`

### 2.2 Transaction Marker Storage

**Location:** Database header page

**Structure:**
```cpp
struct DatabaseHeader {
    uint32_t next_transaction;    // Next TID to assign
    uint32_t oldest_transaction;  // OIT
    uint32_t oldest_active;       // OAT
    uint32_t oldest_snapshot;     // OST
    // ... other fields
};
```

### 2.3 Checking Transaction Markers

**Via gstat utility:**
```bash
$ gstat -h database.fdb
Database header page information:
    Next transaction        12045
    Oldest transaction      11523
    Oldest active           11987
    Oldest snapshot         11987
```

**Via Monitoring Tables:**
```sql
SELECT
    MON$NEXT_TRANSACTION,
    MON$OLDEST_TRANSACTION,
    MON$OLDEST_ACTIVE,
    MON$OLDEST_SNAPSHOT
FROM MON$DATABASE;
```

**ScratchBird Implementation:**
```cpp
struct TransactionMarkers {
    TransactionId next_xid;       // Next transaction ID to assign
    TransactionId oit;            // Oldest Interesting Transaction
    TransactionId oat;            // Oldest Active Transaction
    TransactionId ost;            // Oldest Snapshot Transaction
};

// Stored in database header page
// Updated atomically during transaction operations
```

---

## Part 3: Long-Running Transaction Management

### 3.1 The Problem

**Long-running transactions prevent garbage collection:**

```
Time -->
T1 starts (SNAPSHOT, XID=100) ────────────────────────── still running
T2 (XID=101) commits ──┐
T3 (XID=102) commits ──┤
T4 (XID=103) commits ──┤
...                    │
T1000 (XID=1000) ─────┘

Problem:
- OIT = 100 (T1 is oldest active)
- Back versions created by T2-T1000 cannot be removed
- T1 might still need to see them
- Database grows continuously
- Performance degrades
```

**Impact:**
- Database file grows unbounded
- Indexes become bloated
- Query performance degrades
- Disk space exhaustion

### 3.2 Detection

**Monitor transaction gap:**
```sql
SELECT
    MON$NEXT_TRANSACTION - MON$OLDEST_TRANSACTION as TRANSACTION_GAP,
    MON$OLDEST_TRANSACTION as OIT,
    MON$OLDEST_ACTIVE as OAT,
    MON$OLDEST_SNAPSHOT as OST
FROM MON$DATABASE;
```

**Warning Thresholds:**
- Gap > 50,000: Monitor closely
- Gap > 100,000: Investigate immediately
- Gap > 500,000: Critical - severe performance impact

**Identify culprit transaction:**
```sql
SELECT
    MON$TRANSACTION_ID,
    MON$ATTACHMENT_ID,
    MON$STATE,
    MON$TIMESTAMP,
    DATEDIFF(SECOND FROM MON$TIMESTAMP TO CURRENT_TIMESTAMP) as AGE_SECONDS
FROM MON$TRANSACTIONS
WHERE MON$TRANSACTION_ID = (SELECT MON$OLDEST_ACTIVE FROM MON$DATABASE)
   OR MON$TRANSACTION_ID = (SELECT MON$OLDEST_SNAPSHOT FROM MON$DATABASE);
```

### 3.3 Firebird's Configuration Options

#### 3.3.1 Automatic Monitoring (Firebird 4.0+)

**Configuration Parameter:** `LongRunningThreshold`

**Location:** `firebird.conf` or `databases.conf`

**Syntax:**
```ini
# Time in seconds after which transaction is considered "long-running"
# Default: 0 (disabled)
# Recommended: 300-900 (5-15 minutes)
LongRunningThreshold = 600
```

**Behavior:**
- Transactions running longer than threshold trigger logging
- Warning logged to firebird.log
- Can be monitored via MON$TRANSACTIONS.MON$FLAGS

**Example Log Entry:**
```
INET/inet_error: Long-running transaction detected
    Database: /data/mydb.fdb
    Transaction: 12045
    Started: 2025-10-07 10:00:00
    Duration: 900 seconds
    Attachment: 23
```

#### 3.3.2 Automatic Rollback (Custom Configuration)

**Configuration Parameter:** `MaxTransactionAge` (ScratchBird Extension)

**Proposed for ScratchBird:**
```ini
[transactions]
# Maximum transaction age in seconds before automatic action
max_transaction_age = 3600

# Action to take: LOG, ROLLBACK_READONLY, ROLLBACK_ALL, TERMINATE_CONNECTION
long_transaction_action = LOG

# Apply to read-only transactions only
apply_to_readonly_only = true

# Exemptions by application name
exempt_applications = monitoring_daemon, reporting_service
```

**Actions:**
1. **LOG:** Write warning to log (Firebird default)
2. **ROLLBACK_READONLY:** Rollback read-only transactions only
3. **ROLLBACK_ALL:** Rollback any long transaction (dangerous)
4. **TERMINATE_CONNECTION:** Close connection (forceful)

### 3.4 Application-Level Mitigation

#### 3.4.1 Use READ COMMITTED for Long Queries

**Problem:** SNAPSHOT transaction holds OST back
```sql
-- BAD: Long-running report with SNAPSHOT
SET TRANSACTION ISOLATION LEVEL SNAPSHOT;
SELECT * FROM huge_table WHERE complex_condition;
-- Blocks garbage collection while query runs
```

**Solution:** Use READ COMMITTED
```sql
-- GOOD: Long-running report with READ COMMITTED
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;
SELECT * FROM huge_table WHERE complex_condition;
-- Does NOT block garbage collection
```

**Why:** READ COMMITTED doesn't hold OST marker because it doesn't need old versions.

#### 3.4.2 Use READ ONLY for Monitoring

**For monitoring daemons:**
```sql
SET TRANSACTION READ ONLY
    ISOLATION LEVEL READ COMMITTED;
-- Firebird marks as "pre-committed" internally
-- Doesn't advance OAT or OST
```

**Benefits:**
- Explicitly read-only transactions can be handled specially by server
- May not advance OIT/OAT markers
- Doesn't block garbage collection

#### 3.4.3 Break Long Transactions into Batches

**Instead of:**
```sql
START TRANSACTION;
UPDATE huge_table SET status = 'processed';  -- Millions of rows
COMMIT;
```

**Use batching:**
```sql
-- Process in chunks
FOR EACH BATCH OF 10000 ROWS:
    START TRANSACTION;
    UPDATE huge_table SET status = 'processed'
    WHERE id BETWEEN :start AND :end;
    COMMIT;
    -- New transaction starts immediately (always-in-transaction)
END FOR;
```

### 3.5 ScratchBird Implementation Requirements

```cpp
class TransactionManager {
public:
    struct LongTransactionPolicy {
        // Detection threshold (seconds)
        uint32_t warning_threshold = 600;      // 10 minutes
        uint32_t critical_threshold = 3600;    // 1 hour

        // Actions
        enum Action {
            LOG_ONLY,
            ROLLBACK_READONLY,
            ROLLBACK_ALL,
            TERMINATE_CONNECTION
        };
        Action warning_action = LOG_ONLY;
        Action critical_action = LOG_ONLY;

        // Exemptions
        bool exempt_readonly = false;
        std::vector<std::string> exempt_apps;
    };

    // Check for long-running transactions periodically
    void checkLongTransactions(const LongTransactionPolicy& policy);

    // Get transaction age
    uint64_t getTransactionAge(TransactionId xid);

    // Force rollback (with safeguards)
    Status forceRollback(TransactionId xid, const std::string& reason);
};
```

---

## Part 4: Sweep Mechanism

### 4.1 Purpose

**Sweep** is Firebird's garbage collection process that:
- Removes old record versions that no transaction can see
- Advances OIT marker forward
- Reclaims disk space
- Improves query performance

### 4.2 When Sweep Triggers

**Automatic Sweep Condition:**
```
IF (OST - OIT) > sweep_interval THEN
    trigger_sweep()
END IF
```

**Critical Correction:** Many documents incorrectly state `(Next - OIT)` or `(OAT - OIT)`. The correct formula verified by Firebird developers is **(OST - OIT)**.

**Why OST?**
- OST represents oldest SNAPSHOT transaction
- SNAPSHOT transactions hold old versions
- OST more accurately reflects garbage accumulation

### 4.3 Sweep Interval Configuration

**Default:** 20,000 transactions

**Checking:**
```bash
$ gstat -h database.fdb | grep "Sweep interval"
    Sweep interval:         20000
```

**Setting with gfix:**
```bash
# Set sweep interval to 10,000
$ gfix -h 10000 database.fdb -user SYSDBA -password masterkey

# Disable automatic sweep (set to 0)
$ gfix -h 0 database.fdb -user SYSDBA -password masterkey
```

**Via SQL (Firebird 3.0+):**
```sql
ALTER DATABASE SET SWEEP INTERVAL 10000;
```

### 4.4 Manual Sweep

**Using gfix:**
```bash
# Perform immediate sweep
$ gfix -sweep database.fdb -user SYSDBA -password masterkey
```

**Sweep modes:**
- **Cooperative:** Attachments participate in sweep
- **Exclusive:** Database locked during sweep (older versions)

### 4.5 Sweep Process

**What Sweep Does:**

1. **Scan TIP (Transaction Inventory Pages):**
   - Find all committed transactions
   - Identify range from OIT to Next

2. **Advance OIT:**
   - Find first non-committed transaction
   - Update OIT in database header

3. **Remove Old Back Versions:**
   - For each table:
     - Scan data pages
     - Remove back versions with xmax < OIT
     - Reclaim space

4. **Update Indexes:**
   - Remove index entries pointing to garbage
   - May cause index page consolidation

**Sweep Performance:**
- Can be I/O intensive (scans entire database)
- Holds shared locks (blocks exclusive operations)
- Duration depends on database size and garbage amount

### 4.6 Alternative: Garbage Collection During Backup

**gbak automatically sweeps unless disabled:**
```bash
# Backup WITH sweep (default)
$ gbak -b database.fdb backup.fbk

# Backup WITHOUT sweep (faster)
$ gbak -b -g database.fdb backup.fbk
```

**Best Practice:**
- Disable automatic sweep (set interval to 0)
- Perform regular nightly backups (gbak sweeps automatically)
- OR schedule manual sweep during maintenance window

### 4.7 ScratchBird Sweep Configuration

```ini
[sweep]
# Automatic sweep interval (0 = disabled)
sweep_interval = 20000

# Allow sweep to run in background thread
background_sweep = true

# Maximum sweep duration (seconds, 0 = unlimited)
max_sweep_duration = 3600

# Cooperative vs background mode
sweep_mode = cooperative  # cooperative | background | off

# Minimum free space to reclaim before sweep triggers
min_garbage_threshold = 104857600  # 100 MB
```

**Implementation:**
```cpp
class SweepManager {
public:
    struct SweepConfig {
        uint32_t sweep_interval = 20000;
        bool background_sweep = true;
        uint32_t max_duration_seconds = 0;  // 0 = unlimited
        enum Mode { COOPERATIVE, BACKGROUND, OFF } mode = COOPERATIVE;
    };

    // Check if sweep should trigger
    bool shouldSweep(const TransactionMarkers& markers);

    // Perform sweep
    Status performSweep(Database* db, ErrorContext* err_ctx);

    // Background sweep thread
    void backgroundSweepThread();
};
```

---

## Part 5: Garbage Collection Mechanisms

### 5.1 Cooperative Garbage Collection

**Definition:** Each transaction cleans up garbage it encounters during normal operation.

**How It Works:**
```
Transaction T reads page P:
1. For each record on page:
   - Check if back versions are garbage (xmax < T's snapshot)
   - If yes, remove back version
   - Update forward pointers
2. Continue with query
```

**Characteristics:**
- No separate GC thread
- Every transaction does its share
- Can slow down queries after large updates/deletes
- Used by Firebird Classic Server (always)
- Firebird SuperServer: configurable

**Pros:**
- No background overhead
- Automatic cleanup
- Simple implementation

**Cons:**
- Query performance varies (first query after update pays cleanup cost)
- Large scans after bulk updates are slow

### 5.2 Background Garbage Collection

**Definition:** Dedicated background thread(s) perform garbage collection.

**How It Works:**
```
Background GC Thread:
1. Track pages with garbage (dirty page list)
2. Periodically scan dirty pages
3. Remove garbage versions
4. Mark pages clean
```

**Characteristics:**
- Available in Firebird 2.0+ SuperServer
- Configurable via GCPolicy setting
- Reduces query variance

**Configuration (firebird.conf):**
```ini
# GCPolicy: cooperative | background | combined
# Classic: ignores setting, always cooperative
# SuperServer:
#   cooperative - only cooperative GC
#   background - only background thread GC
#   combined - both (default)
GCPolicy = combined
```

**Pros:**
- More predictable query performance
- Cleanup happens asynchronously
- Better for OLTP workloads

**Cons:**
- Background thread overhead
- More complex implementation
- Classic Server doesn't support it

### 5.3 Comparison

| Aspect | Cooperative | Background | Combined |
|--------|-------------|------------|----------|
| **Who cleans** | Queries | Background thread | Both |
| **Query variance** | High | Low | Low |
| **Overhead** | None | Thread + memory | Moderate |
| **Classic Server** | ✅ Yes | ❌ No | ❌ No |
| **SuperServer** | ✅ Yes | ✅ Yes | ✅ Yes |
| **Firebird default** | Classic only | - | SuperServer |

### 5.4 ScratchBird Implementation

**Recommendation:** Support both, configurable

```cpp
enum class GCPolicy {
    COOPERATIVE,   // Queries clean up garbage they see
    BACKGROUND,    // Dedicated GC thread
    COMBINED       // Both mechanisms (recommended)
};

class GarbageCollector {
public:
    // Cooperative GC: called during page reads
    void cleanPageIfNeeded(Page* page, Snapshot* snapshot);

    // Background GC: runs in separate thread
    void backgroundGCThread();

    // Track pages with garbage
    void markPageDirty(PageId page_id);

    // Configuration
    void setPolicy(GCPolicy policy);
};
```

---

## Part 6: Transaction Isolation Levels

### 6.1 Overview

Firebird supports three isolation levels:

1. **SNAPSHOT** (aka CONCURRENCY) - Default
2. **SNAPSHOT TABLE STABILITY** - Most restrictive
3. **READ COMMITTED** - Most flexible

### 6.2 SNAPSHOT Isolation

**Also known as:** CONCURRENCY

**Behavior:**
- Transaction sees database as of its start time
- Sees only changes committed before transaction started
- Changes by concurrent transactions are NOT visible
- Provides repeatable reads

**Example:**
```sql
-- Connection 1
SET TRANSACTION ISOLATION LEVEL SNAPSHOT;  -- T1, XID=100
SELECT * FROM accounts WHERE id = 1;       -- balance = 500

-- Connection 2 (concurrent)
START TRANSACTION;                          -- T2, XID=101
UPDATE accounts SET balance = 600 WHERE id = 1;
COMMIT;                                     -- T2 commits

-- Back to Connection 1
SELECT * FROM accounts WHERE id = 1;       -- Still sees balance = 500
COMMIT;                                     -- End T1
START TRANSACTION;                          -- T3, XID=102
SELECT * FROM accounts WHERE id = 1;       -- Now sees balance = 600
```

**Use Cases:**
- Reports requiring point-in-time consistency
- Queries that run multiple times and need same results
- Default for most applications

**Tradeoffs:**
- Long-running SNAPSHOT holds OST marker
- Blocks garbage collection
- Can cause update conflicts

**TPB Parameters:**
```cpp
// Transaction Parameter Buffer for SNAPSHOT
isc_tpb_concurrency,
isc_tpb_write,     // or isc_tpb_read for read-only
isc_tpb_wait       // or isc_tpb_nowait
```

### 6.3 SNAPSHOT TABLE STABILITY

**Behavior:**
- Most restrictive isolation level
- Transaction has exclusive or shared access to specified tables
- Used with RESERVING clause
- Other transactions cannot modify reserved tables

**Example:**
```sql
SET TRANSACTION
    ISOLATION LEVEL SNAPSHOT TABLE STABILITY
    RESERVING accounts FOR PROTECTED WRITE;

-- This transaction has exclusive write access to 'accounts'
-- Other transactions CANNOT modify 'accounts' until this commits
UPDATE accounts SET balance = balance * 1.05;
COMMIT;
```

**Use Cases:**
- Batch operations requiring table-level consistency
- Data migrations
- Bulk updates where conflicts unacceptable

**Table Reservation Options:**

```sql
RESERVING table_name [, ...] FOR
    [SHARED | PROTECTED] [READ | WRITE]
```

**Reservation Matrix:**

| Reservation | Meaning | Conflicts With |
|-------------|---------|----------------|
| `SHARED READ` | Shared read lock | PROTECTED WRITE, any WRITE to same table |
| `SHARED WRITE` | Shared write lock | PROTECTED READ/WRITE |
| `PROTECTED READ` | Protected read | Any WRITE to same table |
| `PROTECTED WRITE` | Exclusive write | Any access to same table |

**TPB Parameters:**
```cpp
isc_tpb_consistency,  // TABLE STABILITY
isc_tpb_write,
isc_tpb_wait
// Plus isc_tpb_lock_read/isc_tpb_lock_write for RESERVING
```

### 6.4 READ COMMITTED

**Behavior:**
- Sees all committed changes from other transactions
- Even if committed after this transaction started
- No repeatable reads
- Most flexible, least isolation

**Example:**
```sql
-- Connection 1
SET TRANSACTION ISOLATION LEVEL READ COMMITTED;  -- T1
SELECT * FROM accounts WHERE id = 1;             -- balance = 500

-- Connection 2
START TRANSACTION;
UPDATE accounts SET balance = 600 WHERE id = 1;
COMMIT;

-- Back to Connection 1 (same transaction)
SELECT * FROM accounts WHERE id = 1;             -- Now sees balance = 600
```

**Use Cases:**
- Long-running monitoring queries
- Reports that need latest data
- Applications tolerant of non-repeatable reads

**Benefits for Long Transactions:**
- Does NOT hold OST marker
- Does NOT block garbage collection
- Ideal for long-running queries

### 6.5 READ COMMITTED Variants (Firebird 4.0+)

Firebird 4.0 introduced three variants of READ COMMITTED:

#### 6.5.1 READ COMMITTED READ CONSISTENCY (New Default)

**Behavior:**
- Statement-level consistency
- Each SQL statement sees consistent snapshot
- Different statements in same transaction may see different data
- Solves "Halloween problem"

**Example:**
```sql
SET TRANSACTION ISOLATION LEVEL READ COMMITTED READ CONSISTENCY;

-- Statement 1 sees snapshot at time T1
SELECT * FROM accounts WHERE balance > 1000;

-- Another transaction commits changes

-- Statement 2 sees snapshot at time T2 (later)
SELECT * FROM accounts WHERE balance < 1000;
-- Sees changes committed after statement 1
```

**Configuration (firebird.conf):**
```ini
ReadConsistency = 1  # Default in Firebird 4.0+
```

**TPB Parameters:**
```cpp
isc_tpb_read_committed,
isc_tpb_rec_version,
isc_tpb_read_consistency  // New in FB 4.0
```

#### 6.5.2 READ COMMITTED RECORD VERSION

**Behavior:**
- Original READ COMMITTED behavior
- May see inconsistent data within single statement
- Faster than READ CONSISTENCY

**Example:**
```sql
SET TRANSACTION ISOLATION LEVEL READ COMMITTED RECORD VERSION;

UPDATE accounts SET balance = balance + 100;
-- If another transaction commits during this scan,
-- some rows may get new value, others old value + 100
-- (Can double-apply update to some rows)
```

**Use When:**
- Performance critical
- Consistency within statement not required
- Legacy application compatibility

**TPB Parameters:**
```cpp
isc_tpb_read_committed,
isc_tpb_rec_version
```

#### 6.5.3 READ COMMITTED NO RECORD VERSION

**Behavior:**
- Waits for locks instead of reading old versions
- More pessimistic locking
- Update conflicts cause waits or errors

**Use When:**
- Need to ensure seeing latest committed data
- Can tolerate waits/deadlocks
- Prefer locking over versioning

**TPB Parameters:**
```cpp
isc_tpb_read_committed,
isc_tpb_no_rec_version
```

### 6.6 Isolation Level Comparison

| Isolation Level | Repeatable Read | Sees Concurrent Changes | Holds OST | Use Case |
|-----------------|-----------------|-------------------------|-----------|----------|
| **SNAPSHOT** | ✅ Yes | ❌ No | ✅ Yes | Reports, consistency required |
| **SNAPSHOT TABLE STABILITY** | ✅ Yes | ❌ No | ✅ Yes | Batch updates, migrations |
| **READ COMMITTED** | ❌ No | ✅ Yes | ❌ No | Long queries, monitoring |

### 6.7 ScratchBird Implementation

```cpp
enum class IsolationLevel {
    SNAPSHOT,                    // Firebird: CONCURRENCY
    SNAPSHOT_TABLE_STABILITY,    // Firebird: CONSISTENCY
    READ_COMMITTED_READ_CONSISTENCY,  // Firebird 4.0+
    READ_COMMITTED_RECORD_VERSION,
    READ_COMMITTED_NO_RECORD_VERSION
};

class Transaction {
    IsolationLevel isolation_level_;
    Snapshot snapshot_;           // For SNAPSHOT modes
    bool is_read_only_;

    // Table reservations for TABLE STABILITY
    struct TableReservation {
        std::string table_name;
        enum LockMode { SHARED_READ, SHARED_WRITE,
                       PROTECTED_READ, PROTECTED_WRITE } mode;
    };
    std::vector<TableReservation> reservations_;
};
```

---

## Part 7: SET TRANSACTION Syntax

### 7.1 Complete Syntax

```sql
SET TRANSACTION
    [NAME transaction_name]
    [READ {ONLY | WRITE}]
    [[NO] WAIT]
    [LOCK TIMEOUT seconds]
    [ISOLATION LEVEL <isolation_level>]
    [NO AUTO UNDO]
    [RESTART REQUESTS]
    [AUTO COMMIT]
    [IGNORE LIMBO]
    [RESERVING <table_spec> [, <table_spec> ...]]

<isolation_level> ::=
    SNAPSHOT [TABLE STABILITY]
  | READ COMMITTED [[NO] RECORD_VERSION | READ CONSISTENCY]

<table_spec> ::=
    table_name [, table_name ...]
    [FOR [SHARED | PROTECTED] {READ | WRITE}]
```

### 7.2 Parameter Details

#### 7.2.1 READ ONLY / READ WRITE

**Default:** READ WRITE

**READ ONLY:**
- No modifications allowed
- Can be optimized by server (marked pre-committed)
- Doesn't advance OIT/OAT in some cases
- Ideal for long-running queries

**READ WRITE:**
- Can modify data
- Default mode

#### 7.2.2 WAIT / NO WAIT

**Default:** WAIT

**WAIT:**
- Transaction waits for locks held by other transactions
- Waits indefinitely (or until LOCK TIMEOUT)
- Deadlock detection applies

**NO WAIT:**
- Lock conflict causes immediate error
- No waiting
- Useful for optimistic locking strategies

**Mutual Exclusion:** Cannot specify both NO WAIT and LOCK TIMEOUT

#### 7.2.3 LOCK TIMEOUT

**Syntax:** `LOCK TIMEOUT seconds`

**Default:** Infinite wait (if WAIT specified)

**Behavior:**
- Wait up to specified seconds for lock
- If lock not acquired, raise error
- Overrides NO WAIT

**Example:**
```sql
SET TRANSACTION
    LOCK TIMEOUT 30
    ISOLATION LEVEL SNAPSHOT;

-- Will wait up to 30 seconds for locks
```

#### 7.2.4 RESERVING Clause

**Purpose:** Reserve tables at transaction start (for TABLE STABILITY)

**Syntax:**
```sql
RESERVING table1, table2 FOR [SHARED | PROTECTED] [READ | WRITE]
```

**Example:**
```sql
SET TRANSACTION
    ISOLATION LEVEL SNAPSHOT TABLE STABILITY
    RESERVING
        accounts FOR PROTECTED WRITE,
        customers FOR SHARED READ;

-- Exclusive write on accounts, shared read on customers
```

**Lock Acquisition:**
- Locks acquired at transaction start
- If locks unavailable:
  - WAIT: Transaction waits
  - NO WAIT: Transaction fails immediately
  - LOCK TIMEOUT: Wait up to timeout

#### 7.2.5 NO AUTO UNDO (Advanced)

**Purpose:** Disable automatic undo log for long transactions

**Use Case:** Very large batch operations

**Warning:** Rollback will be slower

#### 7.2.6 AUTO COMMIT (Deprecated)

**Purpose:** Commit automatically after each statement

**Status:** Deprecated, not recommended

**Note:** Conflicts with ScratchBird's always-in-transaction model

### 7.3 Common Patterns

#### Pattern 1: Long-Running Report (Read-Only)
```sql
SET TRANSACTION
    READ ONLY
    ISOLATION LEVEL READ COMMITTED
    WAIT;

-- Doesn't hold OST, won't block garbage collection
```

#### Pattern 2: Batch Update with Table Lock
```sql
SET TRANSACTION
    ISOLATION LEVEL SNAPSHOT TABLE STABILITY
    RESERVING staging_table FOR PROTECTED WRITE
    LOCK TIMEOUT 60;

-- Exclusive access to staging_table, timeout after 60 seconds
```

#### Pattern 3: Optimistic Locking
```sql
SET TRANSACTION
    ISOLATION LEVEL SNAPSHOT
    NO WAIT;

-- Fail immediately on lock conflict
```

#### Pattern 4: Critical Update with Timeout
```sql
SET TRANSACTION
    READ WRITE
    ISOLATION LEVEL SNAPSHOT
    LOCK TIMEOUT 30;

-- Wait max 30 seconds for locks
```

---

## Part 8: Transaction Parameter Buffer (TPB)

### 8.1 Overview

**TPB** = Transaction Parameter Buffer

**Purpose:** Low-level binary encoding of transaction parameters passed to Firebird API

**Location:** Client API level (not SQL)

### 8.2 TPB Structure

```cpp
// Example TPB construction
std::vector<uint8_t> tpb;

// Isolation level
tpb.push_back(isc_tpb_version3);
tpb.push_back(isc_tpb_concurrency);  // SNAPSHOT

// Access mode
tpb.push_back(isc_tpb_write);        // READ WRITE

// Lock resolution
tpb.push_back(isc_tpb_wait);         // WAIT

// Pass TPB to isc_start_transaction()
```

### 8.3 Common TPB Constants

```cpp
// Version
isc_tpb_version1
isc_tpb_version3  // Current

// Isolation levels
isc_tpb_concurrency         // SNAPSHOT
isc_tpb_consistency         // SNAPSHOT TABLE STABILITY
isc_tpb_read_committed      // READ COMMITTED

// Read committed variants
isc_tpb_rec_version         // RECORD VERSION
isc_tpb_no_rec_version      // NO RECORD VERSION
isc_tpb_read_consistency    // READ CONSISTENCY (FB 4.0+)

// Access mode
isc_tpb_read                // READ ONLY
isc_tpb_write               // READ WRITE

// Lock resolution
isc_tpb_wait                // WAIT
isc_tpb_nowait              // NO WAIT

// Table reservation
isc_tpb_lock_read          // RESERVING FOR READ
isc_tpb_lock_write         // RESERVING FOR WRITE
isc_tpb_shared             // SHARED
isc_tpb_protected          // PROTECTED
isc_tpb_exclusive          // EXCLUSIVE (deprecated)

// Other
isc_tpb_auto_commit
isc_tpb_no_auto_undo
isc_tpb_restart_requests
isc_tpb_ignore_limbo
```

### 8.4 TPB Examples

#### Example 1: Default Transaction
```cpp
uint8_t tpb[] = {
    isc_tpb_version3,
    isc_tpb_write,
    isc_tpb_wait,
    isc_tpb_concurrency
};
// READ WRITE + WAIT + SNAPSHOT
```

#### Example 2: Read-Only Report
```cpp
uint8_t tpb[] = {
    isc_tpb_version3,
    isc_tpb_read,
    isc_tpb_wait,
    isc_tpb_read_committed,
    isc_tpb_rec_version
};
// READ ONLY + WAIT + READ COMMITTED RECORD VERSION
```

#### Example 3: Table Stability with Reservation
```cpp
uint8_t tpb[] = {
    isc_tpb_version3,
    isc_tpb_write,
    isc_tpb_wait,
    isc_tpb_consistency,
    isc_tpb_lock_write, 2, 't','1',  // Table "t1"
    isc_tpb_protected
};
// SNAPSHOT TABLE STABILITY RESERVING t1 FOR PROTECTED WRITE
```

### 8.5 ScratchBird TPB Implementation

```cpp
class TransactionParameterBuffer {
public:
    void setIsolationLevel(IsolationLevel level);
    void setReadOnly(bool read_only);
    void setWait(bool wait, uint32_t timeout_seconds = 0);
    void addTableReservation(const std::string& table,
                            ReservationMode mode);

    // Encode to binary TPB
    std::vector<uint8_t> encode() const;

    // Decode from binary TPB
    static TransactionParameterBuffer decode(const uint8_t* tpb, size_t len);

private:
    IsolationLevel isolation_;
    bool read_only_;
    bool wait_;
    uint32_t lock_timeout_;
    std::vector<TableReservation> reservations_;
};
```

---

## Part 9: Implementation Roadmap for ScratchBird

### 9.1 Phase 1: Transaction Markers (1 week)

**Tasks:**
1. Add transaction marker fields to database header
   - `next_xid`, `oit`, `oat`, `ost`
2. Implement marker update logic
   - Update OAT when transaction starts/ends
   - Update OIT during sweep
   - Update OST when SNAPSHOT transaction starts
3. Add monitoring query support
   - Expose markers via system catalog
4. Implement transaction state tracking in TIP

**Files:**
- `src/core/transaction_manager.cpp`
- `include/scratchbird/core/transaction_manager.h`
- `src/core/database_header.h`

### 9.2 Phase 2: Isolation Levels (2 weeks)

**Tasks:**
1. Implement SNAPSHOT isolation
   - Point-in-time snapshot creation
   - Visibility checks using snapshot
2. Implement READ COMMITTED
   - Latest committed version visibility
   - Statement-level consistency (READ CONSISTENCY)
3. Implement SNAPSHOT TABLE STABILITY
   - Table reservation mechanism
   - Lock acquisition at transaction start
4. Add isolation level to ConnectionContext
5. Parser support for ISOLATION LEVEL clause

**Files:**
- `src/core/snapshot.cpp` (new)
- `src/core/visibility.cpp`
- `src/parser/parser_v2.cpp`

### 9.3 Phase 3: Sweep Mechanism (2 weeks)

**Tasks:**
1. Implement sweep interval checking
   - Formula: `(OST - OIT) > sweep_interval`
2. Implement sweep process
   - Scan TIP, advance OIT
   - Remove old back versions
   - Update indexes
3. Add configuration support
   - `sweep_interval` parameter
   - Manual sweep trigger
4. Add sweep monitoring
   - Sweep progress tracking
   - Sweep statistics

**Files:**
- `src/core/sweep_manager.cpp` (new)
- `include/scratchbird/core/sweep_manager.h` (new)
- `src/core/config.cpp`

### 9.4 Phase 4: Garbage Collection (2 weeks)

**Tasks:**
1. Implement cooperative GC
   - Clean garbage during page reads
   - Visibility-based version removal
2. Implement background GC (optional)
   - Dirty page tracking
   - Background GC thread
3. Add GC policy configuration
   - COOPERATIVE / BACKGROUND / COMBINED
4. Integrate with sweep mechanism

**Files:**
- `src/core/garbage_collector.cpp` (new)
- `include/scratchbird/core/garbage_collector.h` (new)

### 9.5 Phase 5: Long Transaction Management (1 week)

**Tasks:**
1. Implement transaction age tracking
   - Record transaction start time
   - Calculate age on query
2. Add long transaction detection
   - Configurable threshold
   - Warning log messages
3. Add long transaction policies
   - LOG / ROLLBACK / TERMINATE options
   - Exemption lists
4. Monitoring queries
   - Identify long transactions
   - Transaction gap reporting

**Files:**
- `src/core/transaction_monitor.cpp` (new)
- `src/core/config.cpp`

### 9.6 Phase 6: Advanced Features (2 weeks)

**Tasks:**
1. Implement table reservation (RESERVING clause)
   - Lock acquisition at transaction start
   - SHARED / PROTECTED modes
2. Implement LOCK TIMEOUT
   - Timeout-based lock waiting
   - Integration with lock manager
3. Implement READ ONLY optimization
   - Skip undo log for read-only transactions
   - Potential OIT/OAT optimization
4. Parser support for full SET TRANSACTION syntax

**Files:**
- `src/parser/parser_v2.cpp`
- `src/core/lock_manager.cpp`
- `src/core/transaction_manager.cpp`

### 9.7 Total Estimated Effort

**Total: 10 weeks** (2.5 months)

**Parallelization Potential:**
- Phase 1-2 can overlap (different developers)
- Phase 3-4 can overlap (different developers)
- Phase 5-6 can overlap with testing

**With 2 developers: 6-7 weeks**

---

## Part 10: Configuration Reference

### 10.1 ScratchBird Configuration File (sb_config.ini)

```ini
[transactions]
# Transaction marker update frequency
marker_update_interval = 100  # Update header every N transactions

# Default isolation level
default_isolation_level = SNAPSHOT  # SNAPSHOT | READ_COMMITTED | SNAPSHOT_TABLE_STABILITY

# Default transaction mode
default_read_only = false
default_wait = true
default_lock_timeout = 0  # 0 = infinite wait

[sweep]
# Automatic sweep interval (0 = disabled)
# Sweep triggers when (OST - OIT) > sweep_interval
sweep_interval = 20000

# Background sweep
background_sweep = true
max_sweep_duration = 3600  # seconds, 0 = unlimited

# Sweep mode
sweep_mode = combined  # cooperative | background | combined | off

# Minimum garbage to trigger sweep (bytes)
min_garbage_threshold = 104857600  # 100 MB

[garbage_collection]
# GC policy
gc_policy = combined  # cooperative | background | combined

# Background GC thread count
background_gc_threads = 2

# GC sleep interval (milliseconds)
gc_sleep_interval = 1000

# Pages to process per GC cycle
gc_batch_size = 100

[long_transactions]
# Long-running transaction detection
enable_detection = true
warning_threshold = 600    # seconds (10 minutes)
critical_threshold = 3600  # seconds (1 hour)

# Actions: LOG | ROLLBACK_READONLY | ROLLBACK_ALL | TERMINATE
warning_action = LOG
critical_action = LOG

# Exemptions
exempt_readonly = false
exempt_applications = monitoring,reporting

# Monitoring interval
check_interval = 60  # seconds

[monitoring]
# Enable transaction monitoring tables
enable_monitoring = true

# MON$ table update frequency
monitoring_interval = 5  # seconds
```

---

## Part 11: Monitoring Queries

### 11.1 Transaction Markers

```sql
-- View current transaction markers
SELECT
    next_transaction as NEXT_TXN,
    oldest_transaction as OIT,
    oldest_active as OAT,
    oldest_snapshot as OST,
    (next_transaction - oldest_transaction) as TXN_GAP,
    (oldest_snapshot - oldest_transaction) as SWEEP_GAP
FROM sb_database_info;
```

### 11.2 Active Transactions

```sql
-- List all active transactions
SELECT
    transaction_id,
    connection_id,
    isolation_level,
    is_read_only,
    start_timestamp,
    CURRENT_TIMESTAMP - start_timestamp as AGE
FROM sb_active_transactions
ORDER BY start_timestamp;
```

### 11.3 Long-Running Transactions

```sql
-- Find long-running transactions
SELECT
    transaction_id,
    connection_id,
    isolation_level,
    start_timestamp,
    EXTRACT(EPOCH FROM (CURRENT_TIMESTAMP - start_timestamp)) as AGE_SECONDS
FROM sb_active_transactions
WHERE CURRENT_TIMESTAMP - start_timestamp > INTERVAL '10 minutes'
ORDER BY start_timestamp;
```

### 11.4 Transaction Gap Analysis

```sql
-- Analyze transaction gap and sweep status
SELECT
    (next_transaction - oldest_transaction) as TOTAL_GAP,
    (oldest_snapshot - oldest_transaction) as SWEEP_GAP,
    sweep_interval,
    CASE
        WHEN (oldest_snapshot - oldest_transaction) > sweep_interval
        THEN 'SWEEP NEEDED'
        ELSE 'OK'
    END as SWEEP_STATUS
FROM sb_database_info;
```

### 11.5 Garbage Collection Statistics

```sql
-- View garbage collection statistics
SELECT
    pages_scanned,
    versions_removed,
    space_reclaimed_bytes,
    last_sweep_timestamp,
    sweep_duration_seconds
FROM sb_gc_stats;
```

---

## Part 12: References and Resources

### 12.1 Firebird Documentation

- [Firebird Language Reference](https://www.firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref40/)
- [Understanding Firebird Transactions](https://www.ibphoenix.com/articles/art-00000400)
- [Transaction Control Documentation](https://www.firebirdsql.org/file/documentation/chunk/en/refdocs/fblangref40/fblangref40-transacs.html)
- [Multi-Generational Architecture](https://www.ibexpert.net/ibe/pmwiki.php?n=Doc.MGA)

### 12.2 Technical Papers

- [ACID Compliance and Firebird](https://firebirdsql.org/file/documentation/papers_presentations/html/paper-fbent-acid.html)
- [Firebird Conference 2019: Transaction Internals](https://www.ibphoenix.com/files/conf2019/03Transactions-internals.pdf)
- [The Truth About Automated Sweep](https://www.firebirdnews.org/the-truth-about-automated-sweep/)

### 12.3 Related ScratchBird Documents

- [Transaction Management Design](../../design/TRANSACTION_MANAGEMENT_DESIGN.md) - Always-in-transaction model
- [Alpha 1.2 Requirements](../../Alpha_Phase_1_Archive/issues/ALPHA_1_2_REQUIREMENTS.md) - Core design principles
- [Code Audit Report](../../Alpha_Phase_1_Archive/status_archive/2025-10-pre-phase2-3/audits-old/OctAudit/audit_2025_10_06.md) - Current implementation status
- [TODO CRIT-002](../../development/TODO.md) - ConnectionContext implementation

---

## Appendix A: Glossary

**MGA** - Multi-Generational Architecture: Database architecture maintaining multiple versions of records

**MVCC** - Multi-Version Concurrency Control: Concurrency control method using record versioning

**OIT** - Oldest Interesting Transaction: Oldest transaction not in committed state

**OAT** - Oldest Active Transaction: Oldest currently active (running) transaction

**OST** - Oldest Snapshot Transaction: Oldest transaction started with SNAPSHOT isolation

**TIP** - Transaction Inventory Pages: Pages storing transaction state information

**Sweep** - Garbage collection process that removes old record versions and advances OIT

**Back Version** - Older version of a record created by UPDATE or DELETE

**Delta** - Pointer from old version to newer version in version chain

**TPB** - Transaction Parameter Buffer: Binary encoding of transaction parameters

**Cooperative GC** - Garbage collection performed by queries encountering garbage

**Background GC** - Garbage collection performed by dedicated background thread

---

**Document Version:** 1.0
**Date:** October 7, 2025
**Status:** Technical Specification for Implementation
**Target:** ScratchBird Alpha 1.2+
**Estimated Implementation Effort:** 10 weeks (6-7 weeks with 2 developers)
