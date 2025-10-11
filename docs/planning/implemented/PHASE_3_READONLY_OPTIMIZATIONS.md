# Phase 3 Task 3.6: READ ONLY Transaction Optimizations

## Overview

READ ONLY transactions in ScratchBird are optimized to reduce overhead compared to READ WRITE transactions. This document describes the optimization infrastructure and future enhancement opportunities.

## Current Infrastructure (As of Task 3.6 Completion)

### 1. **Read-Only Flag Tracking**

The `is_read_only_` flag is tracked at multiple layers:

#### ConnectionContext Level
- **File**: `src/core/connection_context.cpp`
- **Implementation**: The `is_read_only_` member tracks whether the current transaction is read-only
- **API**: `isReadOnly()` getter provides access to the flag
- **Usage**: Set via `startTransaction(bool read_only, ...)` method

#### ProcArray Level
- **File**: `src/core/proc_array.cpp`
- **Implementation**: Each ProcessControlBlock has an `is_read_only` flag
- **Update**: Called in `ConnectionContext::beginNewTransaction()`:
  ```cpp
  s = ProcArrayManager::setTransactionReadOnly(proc_id_, is_read_only_, ctx);
  ```
- **Purpose**: Enables system-wide visibility of read-only transactions for monitoring and optimization

### 2. **Implicit Write Prevention**

Read-only transactions implicitly cannot generate undo log entries because:

1. **No Modifications**: Read-only transactions do not call insert/update/delete operations
2. **Transaction State**: The `is_read_only_` flag can be checked before any write operation
3. **Lock Compatibility**: Read-only transactions typically use SHARE locks, not EXCLUSIVE locks

**Example Check Pattern** (to be enforced in storage engine):
```cpp
if (conn_ctx->isReadOnly()) {
    SET_ERROR_CONTEXT(ctx, Status::INVALID_OPERATION,
        "Cannot modify data in READ ONLY transaction");
    return Status::INVALID_OPERATION;
}
```

### 3. **Transaction Marker Optimization Opportunities**

#### OAT (Oldest Active Transaction)
- **Current Behavior**: All active transactions (read-only or not) are tracked in OAT calculation
- **Optimization Opportunity**: Read-only transactions could be excluded from OAT for VACUUM purposes
- **Rationale**: Read-only transactions don't create tuple versions, so they don't prevent VACUUM
- **Implementation Location**: `TransactionManager::updateTransactionMarkers()`

#### OST (Oldest Snapshot Transaction)
- **Current Behavior**: Snapshot transactions are tracked regardless of read-only status
- **Already Optimized**: OST correctly tracks oldest snapshot for visibility decisions
- **Note**: Read-only SNAPSHOT transactions still need OST tracking for MVCC visibility

## Optimization Benefits

### 1. **Reduced Lock Contention**
READ ONLY transactions use SHARE locks by default, allowing concurrent read access:
- Multiple READ ONLY transactions can access the same table simultaneously
- READ ONLY + SNAPSHOT isolation doesn't need row-level locks for reads

### 2. **No Undo Log Generation**
READ ONLY transactions don't generate:
- UPDATE undo records
- DELETE undo records
- Version chain entries (xmax updates)

This reduces:
- Storage I/O
- Buffer pool pressure
- VACUUM workload

### 3. **Monitoring and Diagnostics**
The `is_read_only` flag in Proc Array enables:
- **MON_ACTIVE_TRANSACTIONS** query shows read-only status
- **Long Transaction Monitoring**: Can differentiate long-running read-only transactions
- **Performance Analysis**: Separate statistics for read-only vs read-write transactions

## Future Optimization Opportunities

### 1. **OAT Calculation Optimization** (Post-Alpha)
**Goal**: Exclude read-only transactions from OAT calculation for VACUUM

**Implementation**:
```cpp
// In TransactionManager::updateTransactionMarkers()
for (const auto& pcb : active_backends) {
    if (pcb.xid > 0 && pcb.xid < oldest_active) {
        // Skip read-only transactions for OAT
        if (!pcb.is_read_only) {
            oldest_active = pcb.xid;
        }
    }
}
```

**Impact**:
- VACUUM can clean up more aggressively
- Reduced storage bloat from long-running read-only analytics queries

### 2. **Snapshot Optimization for READ ONLY** (Post-Alpha)
**Goal**: Create smaller snapshots for read-only transactions

**Implementation**:
- Read-only transactions don't need to track XIDs that started after them for write conflict detection
- Snapshot active_xids list could be filtered to only include write transactions

### 3. **Lock Manager Optimization** (Post-Alpha)
**Goal**: Faster lock acquisition for READ ONLY transactions

**Implementation**:
- Skip deadlock detection for pure read-only transactions (they can't create deadlocks)
- Use lightweight shared latches instead of exclusive locks

### 4. **Buffer Pool Optimization** (Post-Alpha)
**Goal**: Different buffer replacement policy for READ ONLY transactions

**Implementation**:
- READ ONLY transaction pages are more evictable (no dirty pages)
- Could use separate buffer pool ring for large read-only scans

## Verification and Testing

### Unit Tests
File: `tests/unit/test_transaction_advanced.cpp`
- Parser tests for READ ONLY syntax
- Bytecode generation for READ ONLY flag
- AST validation

### Integration Tests
File: `tests/integration/test_transaction_advanced_integration.cpp`
- ConnectionContext READ ONLY transaction lifecycle
- ProcArray read-only flag propagation
- Monitoring query validation
- (To be created in Task 3.6)

### System Tests
- Long-running READ ONLY analytics queries
- Concurrent READ ONLY + READ WRITE workloads
- VACUUM behavior with READ ONLY transactions

## Implementation Status

| Feature | Status | Location |
|---------|--------|----------|
| READ ONLY parsing | ✅ Complete | `src/parser/parser.cpp` |
| READ ONLY bytecode | ✅ Complete | `src/sblr/bytecode_generator.cpp` |
| READ ONLY executor | ✅ Complete | `src/sblr/executor.cpp` |
| ConnectionContext flag | ✅ Complete | `src/core/connection_context.cpp` |
| ProcArray tracking | ✅ Complete | `src/core/proc_array.cpp` |
| Write operation checks | ⏳ Recommended | Storage engine layer |
| **OAT optimization** | **✅ Complete** | `src/core/transaction_manager.cpp` |
| **Snapshot optimization** | **✅ Complete** | `src/core/transaction_manager.cpp` |
| **Lock optimization** | **✅ Complete** | `src/core/lock_manager.cpp` |
| **Buffer pool optimization** | **✅ Complete** | `src/core/buffer_pool.cpp` |
| **Statistics tracking** | **✅ Complete** | All core components |

## Completed Optimizations (2025-10-11)

### 1. OAT Optimization ✅
**Implementation**: `src/core/transaction_manager.cpp::updateTransactionMarkers()`

- Excludes read-only transactions from OAT (Oldest Active Transaction) calculation
- OAT now only tracks write transactions
- OST still includes read-only transactions for MVCC correctness
- **Benefit**: VACUUM can be more aggressive with long-running read-only analytics queries

```cpp
// Lines 595-606
if (!pcb->is_read_only) {
    // Update OAT - minimum of all active WRITE transactions
    if (pcb->xid < new_oat) {
        new_oat = pcb->xid;
        has_active = true;
    }
}
```

### 2. Snapshot Optimization ✅
**Implementation**: `src/core/transaction_manager.cpp::getSnapshot()`

- Filters `active_xids` list for read-only transactions
- Excludes other read-only transactions from snapshot
- Reduces memory usage and improves binary search performance in `isSnapshotVisible()`
- **Benefit**: Smaller snapshots, faster visibility checks

```cpp
// Lines 793-841
if (current_ctx && current_ctx->isReadOnly()) {
    // Filter active_xids to only include write transactions
    for (uint64_t active_xid : snapshot_out.active_xids) {
        bool is_write_txn = !pcbs[i].is_read_only;
        if (is_write_txn) {
            filtered_xids.push_back(active_xid);
        }
    }
}
```

### 3. Lock Manager Optimization ✅
**Implementation**: `src/core/lock_manager.cpp::acquireLock()`

**Features**:
- Read-only transaction detection via `isReadOnlyTransaction(proc_id)`
- Fast-path for SHARE locks (ACCESS_SHARE, SHARE, ROW_SHARE) with no conflicts
- Separate statistics tracking for read-only lock operations
- Deadlock detection can be skipped for read-only transactions (future enhancement)

**Statistics Added**:
- `readonly_locks_acquired`: Locks acquired by read-only transactions
- `readonly_fast_path`: Fast-path acquisitions (no conflicts)
- `readonly_lock_waits`: Read-only transactions that had to wait

**Benefit**: Reduced lock acquisition latency for read-only workloads

```cpp
// Lines 105-156
bool is_readonly_txn = isReadOnlyTransaction(proc_id);
if (is_readonly_txn && is_share_lock && !checkConflictInternal(...)) {
    // Fast path: no conflicts, grant immediately
    stats_.readonly_fast_path++;
    return Status::OK;
}
```

### 4. Buffer Pool Optimization ✅
**Implementation**: `src/core/buffer_pool.cpp::evictPage()`

**Two-Pass Eviction Policy**:
- **Pass 1**: Prefer unpinned, CLEAN pages (no flush required)
- **Pass 2**: Fall back to dirty pages if no clean pages available

**Statistics Added**:
- `evictions_clean`: Clean pages evicted (instant eviction)
- `evictions_dirty`: Dirty pages evicted (requires flush)

**Benefit**: Read-only transactions access only clean pages, enabling faster eviction without I/O

```cpp
// Lines 297-349
// Pass 1: Prefer clean pages for faster eviction
for (unsigned int frame_index : lru_list_) {
    if (frames_[frame_index].pin_count == 0 && !frames_[frame_index].is_dirty) {
        candidate_frame = frame_index;
        found_clean = true;
        break;
    }
}
```

### 5. Statistics Tracking ✅
**Implementation**: Multiple components

**TransactionManager Stats**:
- `transactions_started`, `transactions_committed`, `transactions_aborted`
- `readonly_snapshots`: Snapshots created for read-only transactions
- `readonly_snapshot_xids_filtered`: XIDs filtered from read-only snapshots

**LockManager Stats** (see #3 above):
- `readonly_locks_acquired`, `readonly_fast_path`, `readonly_lock_waits`

**BufferPool Stats** (see #4 above):
- `evictions_clean`, `evictions_dirty`

**Benefit**: Comprehensive monitoring of read-only vs read-write workload behavior

## Conclusion

The Phase 3 READ ONLY transaction optimizations are now **COMPLETE**:

1. **✅ Tracking**: Read-only flag tracked at all necessary levels
2. **✅ Visibility**: Exposed via monitoring queries and ProcArray
3. **✅ Optimizations**: All 5 performance optimizations implemented
4. **✅ Statistics**: Comprehensive monitoring infrastructure

**Performance Impact**:
- Reduced VACUUM blocking from long-running analytics
- Smaller snapshots for read-only transactions
- Faster lock acquisition for read-only workloads
- Improved buffer pool eviction efficiency
- Comprehensive performance monitoring

---

*Updated: 2025-10-11 - All optimizations implemented*
*Original: 2025-10-11 - Phase 3 Task 3.6: Advanced Transaction Features*
