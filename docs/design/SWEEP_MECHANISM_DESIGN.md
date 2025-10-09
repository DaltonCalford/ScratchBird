# ScratchBird Sweep Mechanism Design

**Date:** October 9, 2025
**Status:** Design Document
**Author:** AI Assistant
**Reference:** Phase 3, Task 3.3

---

## Executive Summary

This document describes the design and implementation of the Sweep mechanism for ScratchBird, which is a critical component of the Firebird transaction model. Sweep is responsible for advancing the Oldest Interesting Transaction (OIT) marker and reclaiming space from old tuple versions that are no longer visible to any transaction.

**Key Objectives:**
- Automatic sweep triggering based on transaction gap
- Manual sweep via SWEEP DATABASE command
- Space reclamation by removing old tuple versions
- OIT advancement to enable garbage collection
- Sweep monitoring and statistics

---

## Background

### The Problem: Transaction Gap Growth

In an MVCC system:
1. Each UPDATE/DELETE creates a new tuple version, keeping the old version for concurrent transactions
2. Old versions are only safe to remove when no transaction can see them
3. The OIT (Oldest Interesting Transaction) marks the boundary: versions older than OIT can be removed
4. If OIT doesn't advance, the database accumulates old versions ("bloat")

### Solution: Sweep

Sweep advances OIT by:
1. Scanning Transaction Inventory Pages (TIP) to find committed/aborted transactions
2. Finding the first uncommitted transaction (becomes new OIT)
3. Optionally removing old tuple versions from data pages (space reclamation)

### Trigger Condition

Sweep is triggered when:
```
(OST - OIT) > sweep_interval
```

Where:
- **OST** (Oldest Snapshot Transaction): Oldest active SNAPSHOT transaction
- **OIT** (Oldest Interesting Transaction): Oldest transaction that might be visible
- **sweep_interval**: Configuration parameter (default: 20000)

The gap `(OST - OIT)` represents accumulated uncommitted or recent transactions.

---

## Architecture

### Components

```
┌─────────────────┐
│ TransactionMgr  │
│  - OIT tracking │
│  - OST tracking │
│  - Trigger check│
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│  SweepManager   │◄─── Manual trigger (SWEEP DATABASE)
│  - Scan TIP     │
│  - Advance OIT  │
│  - Statistics   │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│   TIP Pages     │
│ - Scan entries  │
│ - Find first    │
│   uncommitted   │
└─────────────────┘
```

### Key Classes

1. **SweepManager**: New class for sweep operations
   - Location: `include/scratchbird/core/sweep_manager.h`
   - Responsibilities:
     - Check sweep trigger condition
     - Execute sweep process
     - Track statistics
     - Expose monitoring data

2. **TransactionManager**: Enhanced for sweep
   - Add: `checkSweepTrigger()` - Called after commit
   - Add: `triggerSweep()` - Initiates sweep
   - Existing: `setOldestXid()` - Updates OIT

3. **Database**: Integration point
   - Hold `SweepManager` instance
   - Expose `sweep()` method for manual trigger

---

## Detailed Design

### 1. Sweep Trigger Check

**Location:** `TransactionManager::commit()` (after successful commit)

**Algorithm:**
```cpp
Status TransactionManager::checkSweepTrigger(ErrorContext* ctx) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Get current markers
    uint64_t oit = oldest_xid_;
    uint64_t ost = oldest_snapshot_;

    // No sweep needed if no snapshot transactions
    if (ost == 0) {
        return Status::OK;
    }

    // Calculate gap
    uint64_t gap = ost - oit;

    // Get sweep_interval from config
    uint32_t sweep_interval = Config::get<uint32_t>("sweep", "sweep_interval", 20000);

    // Trigger sweep if gap exceeds threshold
    if (gap > sweep_interval) {
        LOG_INFO(TRANSACTION, "Sweep triggered: gap=%lu, interval=%u", gap, sweep_interval);
        return db_->sweep_manager()->triggerSweep(false, ctx); // false = background
    }

    return Status::OK;
}
```

**When to Check:**
- After every successful commit
- Minimal overhead (just a comparison)
- Non-blocking (sweep runs in background)

### 2. Sweep Process

**Core Algorithm:**

```cpp
Status SweepManager::executeSweep(bool foreground, ErrorContext* ctx) {
    LOG_INFO(SWEEP, "Starting sweep: mode=%s", foreground ? "foreground" : "background");

    // 1. Scan TIP pages to find new OIT
    uint64_t new_oit = findFirstUncommittedTransaction(ctx);
    if (new_oit == 0) {
        return Status::OK; // No change needed
    }

    // 2. Update OIT in database header
    Status s = txn_manager_->setOldestXid(new_oit, ctx);
    if (s != Status::OK) {
        LOG_ERROR(SWEEP, "Failed to update OIT: %d", static_cast<int>(s));
        return s;
    }

    // 3. Optional: Remove old tuple versions (if foreground)
    if (foreground) {
        s = reclaimSpace(new_oit, ctx);
        if (s != Status::OK) {
            LOG_WARNING(SWEEP, "Space reclamation failed: %d", static_cast<int>(s));
            // Non-fatal - OIT is already advanced
        }
    }

    // 4. Update statistics
    updateStatistics(new_oit);

    LOG_INFO(SWEEP, "Sweep completed: old_oit=%lu, new_oit=%lu",
             txn_manager_->getOldestXid(), new_oit);

    return Status::OK;
}
```

#### 2.1 Finding First Uncommitted Transaction

```cpp
uint64_t SweepManager::findFirstUncommittedTransaction(ErrorContext* ctx) {
    uint64_t current_oit = txn_manager_->getOldestXid();
    uint64_t current_xmax = txn_manager_->getNextTransactionId();

    // Scan from current OIT to current XMAX
    for (uint64_t xid = current_oit; xid < current_xmax; xid++) {
        TransactionState state = txn_manager_->getTransactionState(xid);

        // First transaction that's not committed/aborted is new OIT
        if (state != TransactionState::COMMITTED &&
            state != TransactionState::ABORTED) {
            return xid;
        }
    }

    // All transactions are committed/aborted - OIT can advance to XMAX
    return current_xmax;
}
```

**Optimization:** This is a simplified version. Production implementation should:
- Use TIP pages directly (don't go through cache)
- Batch reads for efficiency
- Support incremental scanning
- Handle TIP page boundaries

#### 2.2 Space Reclamation (Foreground Sweep)

```cpp
Status SweepManager::reclaimSpace(uint64_t new_oit, ErrorContext* ctx) {
    // Scan all data pages
    // For each page:
    //   - Find tuples with xmax < new_oit
    //   - Remove old versions
    //   - Update forward pointers
    //   - Compact page if needed

    // NOTE: This is expensive and should only be done in foreground sweep
    // Background sweep only advances OIT

    // TODO: Implement in future iteration
    // For now, rely on cooperative/background GC for space reclamation

    LOG_INFO(SWEEP, "Space reclamation not yet implemented");
    return Status::OK;
}
```

### 3. Sweep Modes

**Background Sweep** (Default, triggered automatically):
- Only advances OIT
- Does not remove tuple versions
- Fast (< 100ms typically)
- Non-blocking

**Foreground Sweep** (Manual, via SWEEP DATABASE):
- Advances OIT
- Removes old tuple versions
- Slow (seconds to minutes)
- Blocks concurrent operations

### 4. Configuration

```ini
[sweep]
# Sweep trigger threshold (transaction gap)
sweep_interval = 20000

# Enable background sweep
background_sweep = true

# Sweep mode: background, foreground, combined
sweep_mode = background
```

### 5. Monitoring and Statistics

**Sweep Statistics Structure:**
```cpp
struct SweepStatistics {
    uint64_t sweep_count;              // Total sweeps executed
    uint64_t last_sweep_time;          // Timestamp of last sweep
    uint64_t last_sweep_duration_ms;   // Duration in milliseconds
    uint64_t last_oit_before;          // OIT before last sweep
    uint64_t last_oit_after;           // OIT after last sweep
    uint64_t total_transactions_swept; // Cumulative count
    bool sweep_in_progress;            // Is sweep currently running?
};
```

**Monitoring Query:**
```sql
SELECT * FROM MON_SWEEP;
```

**Output:**
```
MON$SWEEP_COUNT | MON$LAST_SWEEP_TIME | MON$LAST_DURATION_MS | MON$OIT_BEFORE | MON$OIT_AFTER | MON$IN_PROGRESS
----------------+---------------------+----------------------+----------------+---------------+----------------
             12 | 2025-10-09 14:23:15 |                   45 |         100000 |        120000 | FALSE
```

### 6. Manual Sweep Command

**SQL Syntax:**
```sql
SWEEP DATABASE;
```

**Implementation:**
1. Parser: Add `SWEEP DATABASE` statement
2. Executor: Recognize and execute sweep
3. Call: `db->sweep_manager()->executeSweep(true, ctx)`

**Behavior:**
- Blocks until sweep completes
- Returns success/error status
- Includes foreground space reclamation

---

## Implementation Plan

### Phase 1: Core Sweep Infrastructure (Day 1)

1. **Create SweepManager class**
   - `include/scratchbird/core/sweep_manager.h`
   - `src/core/sweep_manager.cpp`
   - Basic structure and statistics

2. **Integrate with Database**
   - Add `SweepManager* sweep_manager()` accessor
   - Initialize in `Database::create()` and `Database::open()`

3. **Implement trigger check**
   - Add `TransactionManager::checkSweepTrigger()`
   - Call after successful commit

### Phase 2: Sweep Process (Days 2-3)

1. **Implement findFirstUncommittedTransaction()**
   - Scan TIP pages efficiently
   - Handle page boundaries
   - Return new OIT

2. **Implement executeSweep()**
   - Background mode only (no space reclamation)
   - Update OIT via `setOldestXid()`
   - Thread-safe execution

3. **Add configuration support**
   - Read `sweep_interval` from config
   - Support background_sweep flag

### Phase 3: Monitoring (Day 4)

1. **Add SweepStatistics tracking**
   - Track all sweep operations
   - Store in SweepManager

2. **Expose via MON_SWEEP**
   - Add to Executor monitoring queries
   - Return sweep statistics

3. **Logging**
   - Log sweep triggers
   - Log sweep completion with stats

### Phase 4: Manual Sweep (Day 5)

1. **Parser support**
   - Add SWEEP DATABASE grammar
   - Create AST node

2. **Executor support**
   - Recognize SWEEP DATABASE statement
   - Call SweepManager::executeSweep(true)
   - Return results

### Phase 5: Testing (Days 6-7)

1. **Unit tests**
   - Test trigger condition
   - Test TIP scanning
   - Test OIT advancement

2. **Integration tests**
   - Create transaction gap
   - Verify automatic sweep
   - Verify manual sweep
   - Check statistics

---

## Testing Strategy

### Test 1: Automatic Sweep Trigger

```cpp
TEST(SweepTest, AutomaticTrigger) {
    Database db = createTestDatabase();

    // Create transaction gap
    for (int i = 0; i < 25000; i++) {
        auto conn = db.connect();
        conn->commit();
    }

    // Verify sweep was triggered
    SweepStatistics stats = db.sweep_manager()->getStatistics();
    EXPECT_GT(stats.sweep_count, 0);
}
```

### Test 2: Manual Sweep

```cpp
TEST(SweepTest, ManualSweep) {
    Database db = createTestDatabase();

    uint64_t oit_before = db.transaction_manager()->getOldestXid();

    // Execute manual sweep
    ErrorContext ctx;
    Status s = db.sweep_manager()->executeSweep(true, &ctx);
    EXPECT_EQ(s, Status::OK);

    // OIT should have advanced
    uint64_t oit_after = db.transaction_manager()->getOldestXid();
    EXPECT_GE(oit_after, oit_before);
}
```

### Test 3: OIT Advancement

```cpp
TEST(SweepTest, OITAdvancement) {
    Database db = createTestDatabase();

    // Create and commit transactions
    std::vector<uint64_t> xids;
    for (int i = 0; i < 100; i++) {
        auto conn = db.connect();
        xids.push_back(conn->getCurrentXid());
        conn->commit();
    }

    // All transactions are committed, OIT should advance
    db.sweep_manager()->executeSweep(false, nullptr);

    uint64_t oit = db.transaction_manager()->getOldestXid();
    EXPECT_GE(oit, xids[99]); // OIT should be at least last committed XID
}
```

---

## Performance Considerations

### Sweep Cost

**Background Sweep:**
- Scans TIP pages sequentially
- No data page access
- Typically < 100ms
- Frequency: Every ~20,000 transactions

**Foreground Sweep:**
- Scans TIP pages
- Scans all data pages
- Removes old versions
- Can take seconds to minutes
- Should only be manual

### Optimization Strategies

1. **Incremental Scanning**
   - Don't scan entire TIP on every sweep
   - Track last scanned position
   - Resume from checkpoint

2. **Batch Processing**
   - Read TIP entries in batches
   - Reduce I/O overhead

3. **Skip Active Regions**
   - If recent transactions are active, skip scanning them
   - Focus on older transaction range

4. **Parallel Sweep** (Future)
   - Multiple threads scan different TIP page ranges
   - Coordinate to find global minimum

---

## Future Enhancements

1. **Cooperative Sweep**
   - Integrate with page reads
   - Opportunistic space reclamation
   - Spread cost over normal operations

2. **Space Reclamation**
   - Full implementation of foreground sweep
   - Index cleanup
   - Page compaction

3. **Sweep Throttling**
   - Limit sweep frequency
   - Adaptive thresholds based on load
   - Pause sweep under high concurrency

4. **Sweep Progress Reporting**
   - Percentage complete
   - Estimated time remaining
   - Cancel/pause support

---

## References

- Firebird Documentation: Sweep and Garbage Collection
- PostgreSQL VACUUM internals
- Phase 3 Implementation Plan
- Transaction Management Design Document

---

**End of Design Document**
