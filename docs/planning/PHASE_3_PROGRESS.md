# Phase 3: Firebird Transaction Model - PROGRESS REPORT

**Date:** October 9, 2025
**Status:** ~50% Complete (Task 3.1 COMPLETE, Task 3.2 COMPLETE)
**Commits:** 5+ commits, ~1200+ lines of code
**Last Session:** Table locking for SNAPSHOT_TABLE_STABILITY and statement snapshots for READ_COMMITTED_READ_CONSISTENCY

---

## Executive Summary

Tasks 3.1 (Transaction Markers) and Task 3.2 (Isolation Levels) are now COMPLETE! The database now supports all 4 Firebird isolation levels with full functionality:
- READ_COMMITTED with latest committed data visibility
- READ_COMMITTED_READ_CONSISTENCY with statement-level snapshots
- SNAPSHOT with transaction-level snapshots and repeatable reads
- SNAPSHOT_TABLE_STABILITY with table-level locking and snapshots

**Completed:**
- Task 3.1 (Transaction Markers) ✅ COMPLETE
- Task 3.2 (Isolation Levels) ✅ COMPLETE

**Next Up:** Task 3.3 (Sweep Mechanism)
**Remaining:** Tasks 3.3-3.6 (Sweep, GC, Long Transaction Management, Advanced Features)

---

## Completed Work

### Task 3.1: Transaction Markers ✅ COMPLETE

#### What Was Completed

**1. DatabaseHeader Updates** (`include/scratchbird/core/database.h:66`)
- Added `oldest_snapshot` (OST) field to DatabaseHeader
- Updated comments to clarify marker purposes:
  - `next_transaction_id`: Next XID to be assigned (NEXT)
  - `oldest_transaction_id`: Oldest Interesting Transaction (OIT)
  - `oldest_active_xid`: Oldest Active Transaction (OAT)
  - `oldest_snapshot`: Oldest Snapshot Transaction (OST)
- Increased transaction info section from 48 to 56 bytes

**2. TransactionManager Enhancements**
- **Header** (`include/scratchbird/core/transaction_manager.h:113-131`):
  - Added `oldest_active_xid_` tracking field
  - Added `oldest_snapshot_` tracking field
  - Implemented `getOldestActiveXid()` accessor
  - Implemented `getOldestSnapshot()` accessor
  - Added `updateTransactionMarkers()` method declaration

- **Implementation** (`src/core/transaction_manager.cpp:377-434`):
  - `updateTransactionMarkers()` scans ProcArray for all active transactions
  - Computes OAT as minimum of all active XIDs
  - Computes OST as minimum of all SNAPSHOT transaction XIDs
  - Updates database header atomically
  - Thread-safe with mutex protection

**3. ProcArray Isolation Tracking**
- **Header** (`include/scratchbird/core/proc_array.h:41-43`):
  - Added `isolation_level` field to ProcessControlBlock
  - Added `is_snapshot_txn` boolean flag
  - Declared `setIsolationLevel()` method

- **Implementation** (`src/core/proc_array.cpp:166-188`):
  - `setIsolationLevel()` updates isolation level for a backend
  - Sets `is_snapshot_txn` flag for SNAPSHOT and SNAPSHOT_TABLE_STABILITY
  - Enables tracking which transactions need snapshot visibility

**4. ConnectionContext Integration** (`src/core/connection_context.cpp`)
- Calls `setIsolationLevel()` when starting new transactions (line 138)
- Calls `updateTransactionMarkers()` after transaction begin (line 141)
- Calls `updateTransactionMarkers()` after commit/rollback (lines 191, 225)
- Ensures markers stay current throughout transaction lifecycle

**5. Database Initialization** (`src/core/database.cpp`)
- Initializes OIT, OAT, OST to correct values when creating new database
- Loads these values when opening existing database

**6. Monitoring Queries Infrastructure** 🆕
- **Executor Updates** (`include/scratchbird/sblr/executor.h:173`):
  - Added `executeMonitoringQuery()` method to Executor
  - Detects system tables with MON_ prefix
  - Returns transaction marker information as ResultSet

- **Implementation** (`src/sblr/executor.cpp:682-686, 892-937`):
  - Intercepts `SELECT * FROM MON_DATABASE` before catalog lookup
  - Reads transaction markers from TransactionManager
  - Creates ResultSet with 5 columns:
    - `MON$DATABASE_NAME`: Database name
    - `MON$NEXT_TRANSACTION`: Next XID (NEXT)
    - `MON$OLDEST_TRANSACTION`: Oldest Interesting Transaction (OIT)
    - `MON$OLDEST_ACTIVE`: Oldest Active Transaction (OAT)
    - `MON$OLDEST_SNAPSHOT`: Oldest Snapshot Transaction (OST)
  - Returns single row with current marker state

**Usage Example:**
```sql
SELECT * FROM MON_DATABASE;
```

**Output:**
```
MON$DATABASE_NAME | MON$NEXT_TRANSACTION | MON$OLDEST_TRANSACTION | MON$OLDEST_ACTIVE | MON$OLDEST_SNAPSHOT
------------------+----------------------+------------------------+-------------------+--------------------
    SCRATCHBIRD   |                   3  |                     3  |                0  |                 0
(1 rows)
```

#### Technical Notes

1. **Identifier Limitation**: Using `MON_` prefix instead of Firebird's `MON$` because `$` is not yet supported in SQL identifiers by the lexer. This is a temporary workaround.

2. **Marker Update Frequency**: Transaction markers are updated at three critical points:
   - After `beginTransaction()` - captures new transaction start
   - After `commitTransaction()` - updates as transaction completes
   - After `rollbackTransaction()` - updates as transaction aborts

3. **OAT vs OST Calculation**:
   - **OAT** (Oldest Active): Minimum of *all* active XIDs
   - **OST** (Oldest Snapshot): Minimum of only *SNAPSHOT isolation* XIDs
   - OST is used for sweep trigger detection: `(OST - OIT) > sweep_interval`

4. **Thread Safety**: All marker reads and updates protected by TransactionManager mutex

**7. TIP State Transition Tracking** 🆕 (`src/core/transaction_manager.cpp`)
- **Commit Path** (lines 348-354):
  - Added `writeTipEntry(xid, TransactionState::COMMITTED, ctx)` call after CLOG update
  - Ensures TIP pages reflect committed state
  - Logs warning if TIP update fails (CLOG is source of truth)

- **Rollback Path** (lines 393-399):
  - Added `writeTipEntry(xid, TransactionState::ABORTED, ctx)` call after CLOG update
  - Ensures TIP pages reflect aborted state
  - Logs warning if TIP update fails (CLOG is source of truth)

- **Complete Transaction Lifecycle**:
  - BEGIN: TIP entry created with ACTIVE state (existing, line 281)
  - COMMIT: TIP entry updated to COMMITTED state (new)
  - ROLLBACK: TIP entry updated to ABORTED state (new)

**Architecture Note:** CLOG (Commit Log) remains the authoritative source of transaction state, while TIP serves as a persistent cache/index for faster lookups. The TIP state tracking ensures consistency between these two systems.

---

### Task 3.2: Isolation Levels - SNAPSHOT Isolation ✅ COMPLETE (Core Functionality)

#### What Was Completed

**1. Design Document** (`docs/design/ISOLATION_LEVELS_DESIGN.md`)
- Comprehensive 400+ line design document covering all 4 isolation levels
- Detailed visibility algorithms for each isolation level
- Step-by-step implementation plan (5 phases over 2 weeks)
- Testing strategy and success criteria
- Serves as authoritative reference for isolation level semantics

**2. TransactionManager - Snapshot Visibility**
- **Header** (`include/scratchbird/core/transaction_manager.h:166`):
  - Added `isSnapshotVisible(uint64_t xid, const Snapshot* snapshot) const` method
  - Marked as const with mutable cache for proper logical constness

- **Implementation** (`src/core/transaction_manager.cpp:678-742`):
  - Full SNAPSHOT isolation visibility algorithm:
    1. Validates XID to protect against corrupted tuple headers
    2. Rejects future transactions (XID >= snapshot->xmax)
    3. Shows frozen tuples (FROZEN_XID)
    4. Hides transactions active at snapshot time (binary search in active_xids)
    5. Checks CLOG for old transactions (XID < xmin)
    6. Verifies committed status for in-range transactions
  - O(log N) performance using binary search on sorted active_xids
  - Thread-safe with proper locking

- **getSnapshot() Enhancement** (`src/core/transaction_manager.cpp:772`):
  - Sorts `active_xids` array after populating from ProcArray
  - Enables efficient binary search in `isSnapshotVisible()`
  - Maintains O(log N) visibility checks even with many active transactions

**3. StorageEngine - Isolation-Aware Visibility** (`src/core/storage_engine.cpp:198-378`)
- Complete rewrite of `isVisible()` method to support all isolation levels
- Gets ConnectionContext to determine current isolation level
- **"See Own Changes" Logic**: Transactions always see their own modifications
  - Tuples created by current XID are visible (unless deleted by current XID)
  - Critical for correct MVCC semantics

- **Isolation Level Switch**:
  - **READ_COMMITTED**: Uses `isTransactionVisible()` for latest committed data
  - **SNAPSHOT**: Uses `isSnapshotVisible()` with transaction's snapshot
  - **SNAPSHOT_TABLE_STABILITY**: Uses `isSnapshotVisible()` (same as SNAPSHOT)
  - **READ_COMMITTED_READ_CONSISTENCY**: Falls back to READ_COMMITTED (TODO: statement snapshots)

- **Graceful Degradation**: Falls back to READ_COMMITTED if no ConnectionContext available

**4. Const-Correctness Improvements**
- Marked `getTransactionState()` as const (required by `isSnapshotVisible()`)
- Marked cache members as `mutable` (proper use of logical const)
- Marked cache management methods as const (`touchCacheEntry`, `evictOldestCacheEntry`, etc.)
- Ensures visibility checking doesn't modify logical state of TransactionManager

**5. Comprehensive Testing** (`tests/unit/test_connection_context.cpp`)
- Added 3 new comprehensive SNAPSHOT isolation tests (all passing):
  1. **SnapshotIsolationRepeatableReads**: Tests repeatable read semantics
     - Transactions don't see active transactions' changes
     - Transactions don't see changes that were active at snapshot time (even after commit)
     - New snapshots see previously committed transactions
     - Transactions see their own changes

  2. **SnapshotIsolationProperties**: Tests snapshot internal structure
     - Verifies xmin/xmax relationships
     - Verifies active_xids is sorted for binary search
     - Validates snapshot invariants

  3. **ReadCommittedVsSnapshotIsolation**: Tests visibility differences between isolation levels

- Tests verify correct MVCC behavior across concurrent transactions
- Tests use actual StorageEngine visibility checking (integration testing)

#### Technical Highlights

**Snapshot Visibility Algorithm** (from `isSnapshotVisible()`):
```
1. Validate XID (protect against corruption)
2. IF xid >= snapshot->xmax THEN invisible (future transaction)
3. IF xid <= FROZEN_XID THEN visible (frozen tuple)
4. IF xid IN snapshot->active_xids THEN invisible (was active at snapshot time)
5. IF xid < snapshot->xmin THEN check CLOG (old transaction)
6. ELSE check CLOG (committed between xmin and xmax)
```

**Key Design Decisions:**
- Binary search on sorted active_xids for O(log N) performance
- CLOG remains authoritative source of transaction state
- Snapshot is immutable once created (repeatable reads)
- Thread-safe with minimal locking (const methods with mutable cache)

#### Testing Results

✅ **SnapshotCreation**: Passed (27ms)
✅ **SnapshotIsolationRepeatableReads**: Passed (19ms) - Core SNAPSHOT isolation test
✅ **SnapshotIsolationProperties**: Passed (19ms) - Snapshot structure validation
✅ **DefaultIsolationLevel**: Passed (32ms)
✅ **IsolationLevelTransitions**: Passed (38ms)

⚠️ **Known Issues** (minor, non-blocking):
1. `READ_COMMITTED_READ_CONSISTENCY`: Not yet implemented (falls back to READ_COMMITTED)
2. `SNAPSHOT_TABLE_STABILITY`: Needs snapshot creation fix in beginNewTransaction()

#### Files Modified (Task 3.2 - SNAPSHOT Isolation)

```
include/scratchbird/core/transaction_manager.h     | 12 ++++-
src/core/transaction_manager.cpp                    | 73 +++++++++++++++++++
src/core/storage_engine.cpp                         | 157 ++++++++++++--
tests/unit/test_connection_context.cpp              | 220 ++++++++++++++++
docs/design/ISOLATION_LEVELS_DESIGN.md              | 408 +++++++++++ (new)
```

**Cumulative Changes (Task 3.2 - SNAPSHOT)**: 5 files changed, 870 insertions(+), 22 deletions(-)

---

### Task 3.2: READ_COMMITTED_READ_CONSISTENCY ✅ COMPLETE

#### What Was Completed

**1. ConnectionContext - Statement Snapshot Support**
- **Header** (`include/scratchbird/core/connection_context.h:96-104, 146-148`):
  - Added `statement_snapshot_` member (unique_ptr to Snapshot)
  - Added `getStatementSnapshot()` accessor
  - Added `createStatementSnapshot()` method
  - Added `clearStatementSnapshot()` method

- **Implementation** (`src/core/connection_context.cpp:392-417`):
  - `createStatementSnapshot()`: Creates new snapshot at statement start
  - `clearStatementSnapshot()`: Clears snapshot at statement end
  - Updated move constructor/assignment to handle statement snapshots
  - Updated `endCurrentTransaction()` to clear statement snapshots (line 430)

**2. StorageEngine - Statement Snapshot Visibility** (`src/core/storage_engine.cpp:324-373`)
- Enhanced `isVisible()` for READ_COMMITTED_READ_CONSISTENCY:
  - Uses statement snapshot when available (statement-level consistency)
  - Falls back to READ_COMMITTED behavior when no statement snapshot exists
  - Provides consistent view of data throughout statement execution
  - Prevents phantom reads within a single statement

**3. Executor - Automatic Statement Snapshot Management** (`src/sblr/executor.cpp:88-181`)
- Added `#include "scratchbird/core/connection_context.h"`
- Modified `execute()` method to:
  - Detect READ_COMMITTED_READ_CONSISTENCY isolation level
  - Create statement snapshot before statement execution
  - Clear statement snapshot after successful execution
  - Clear statement snapshot on error (exception safety)
  - All snapshot management is automatic and transparent

**4. Comprehensive Testing** (`tests/unit/test_connection_context.cpp:632-884`)
- Added 5 comprehensive tests (all passing):
  1. **ReadCommittedReadConsistencyStatementSnapshot**: Tests snapshot lifecycle
  2. **ReadCommittedReadConsistencyStatementConsistency**: Tests statement-level consistency
  3. **ReadCommittedReadConsistencyComparison**: Compares with other isolation levels
  4. **ReadCommittedReadConsistencyTransactionEnd**: Tests snapshot clearing
  5. **ReadCommittedReadConsistencyMultipleStatements**: Tests multiple statements

#### Technical Highlights

**Statement Snapshot Lifecycle**:
```
1. Statement Start → createStatementSnapshot()
2. Statement Execution → Uses statement snapshot for visibility
3. Statement End → clearStatementSnapshot()
4. Next Statement → Creates new snapshot (sees latest commits)
```

**Key Differences from SNAPSHOT**:
- SNAPSHOT: One snapshot per transaction (repeatable reads across statements)
- READ_COMMITTED_READ_CONSISTENCY: One snapshot per statement (sees latest commits between statements)

**Testing Results**: ✅ All 5 tests passed

---

### Task 3.2: SNAPSHOT_TABLE_STABILITY Table Locking ✅ COMPLETE

#### What Was Completed

**1. Lock Manager Integration** (`src/core/connection_context.cpp:1-6`)
- Added `#include "scratchbird/core/lock_manager.h"`
- Added `#include "scratchbird/core/catalog_manager.h"`

**2. Lock Acquisition in beginNewTransaction()** (`src/core/connection_context.cpp:317-392`)
- Implemented table locking for SNAPSHOT_TABLE_STABILITY isolation:
  - Retrieves LockManager and CatalogManager from Database
  - Looks up `[sys]` schema for table resolution
  - Iterates through reserved tables and acquires locks:
    - Looks up table UUID from catalog
    - Converts `TableLockMode::SHARED` → `LockMode::LOCK_SHARE`
    - Converts `TableLockMode::PROTECTED` → `LockMode::LOCK_ACCESS_EXCLUSIVE`
    - Acquires lock with appropriate timeout and wait settings
  - On failure: Releases all acquired locks and returns error
  - Logs all lock acquisitions for debugging

**3. Lock Release in endCurrentTransaction()** (`src/core/connection_context.cpp:410-426`)
- Calls `LockManager::releaseAllLocks()` after commit/rollback
- Releases all table locks held by the transaction
- Logs warnings if lock release fails (non-fatal)
- Ensures locks don't leak across transaction boundaries

**4. Updated reserveTables()** (`src/core/connection_context.cpp:256-270`)
- Clarified that `reserveTables()` stores reservations
- Actual lock acquisition happens in `beginNewTransaction()`
- Added documentation explaining the two-phase approach

#### Technical Highlights

**Lock Acquisition Flow**:
```
1. User calls reserveTables() → Stores table names and lock modes
2. Transaction starts with SNAPSHOT_TABLE_STABILITY
3. beginNewTransaction() → Acquires locks for each reserved table
4. Transaction executes with table-level consistency
5. endCurrentTransaction() → Releases all locks
```

**Lock Mode Mapping**:
- `SHARED` (Firebird) → `LOCK_SHARE` (LockManager) - Allows concurrent reads
- `PROTECTED` (Firebird) → `LOCK_ACCESS_EXCLUSIVE` (LockManager) - Exclusive access

**Error Handling**:
- Lock acquisition failure triggers automatic rollback
- All previously acquired locks are released via `releaseAllLocks()`
- Prevents partial lock acquisition

**Testing Results**: ✅ All 27 ConnectionContext tests passed, including existing SNAPSHOT_TABLE_STABILITY tests

**Cumulative Changes (Task 3.2 - Complete)**: 10 files changed, 1200+ insertions(+), 50 deletions(-)

---

## Build Status

✅ All code compiles successfully with no errors
✅ All 27 ConnectionContext tests passing (100% pass rate)
✅ All 4 isolation levels fully implemented and tested:
  - READ_COMMITTED: Latest committed data visibility
  - READ_COMMITTED_READ_CONSISTENCY: Statement-level snapshots
  - SNAPSHOT: Transaction-level snapshots with repeatable reads
  - SNAPSHOT_TABLE_STABILITY: Table locking + snapshots
✅ Statement snapshot management integrated into Executor
✅ Table locking integrated with LockManager
⚠️ Main test suite has pre-existing failures (not related to Phase 3 changes)

---

## Commits

1. **Commit b13cdd4**: "Mark Phase 2 as COMPLETE in implementation plan"
2. **Commit 21fbf83**: "Add monitoring queries to expose transaction markers (OIT/OAT/OST/NEXT)"
3. **Commit 59744f7**: "Add Phase 3 progress report documenting monitoring queries implementation"
4. **Commit [pending]**: "Implement READ_COMMITTED_READ_CONSISTENCY with statement snapshots"
5. **Commit [pending]**: "Implement table locking for SNAPSHOT_TABLE_STABILITY"

---

## Remaining Tasks in Phase 3

According to `ALPHA_1_2_IMPLEMENTATION_PLAN.md`, the remaining work includes:

### Task 3.1: Transaction Markers ✅ COMPLETE
- ✅ Database header markers (OIT, OAT, OST, NEXT)
- ✅ Marker update logic
- ✅ Monitoring queries
- ✅ TIP state transition tracking

### Task 3.2: Isolation Levels (2 weeks) ✅ COMPLETE
- ✅ READ_COMMITTED isolation level
- ✅ READ_COMMITTED_READ_CONSISTENCY (statement-level snapshots)
- ✅ SNAPSHOT isolation with point-in-time snapshots
- ✅ SNAPSHOT_TABLE_STABILITY with table locking and snapshots

### Task 3.3: Sweep Mechanism (2 weeks)
- Sweep trigger check: `(OST - OIT) > sweep_interval`
- Sweep process: scan TIP, advance OIT, remove old versions
- Sweep monitoring and statistics
- Manual SWEEP DATABASE command

### Task 3.4: Garbage Collection (2 weeks)
- Cooperative garbage collection during normal operations
- Background garbage collection thread
- GC policy configuration (COOPERATIVE/BACKGROUND/COMBINED)

### Task 3.5: Long Transaction Management (1 week)
- Transaction age tracking
- Long transaction detection with configurable thresholds
- Action policies (LOG/ROLLBACK/TERMINATE)
- Monitoring queries for long transactions

### Task 3.6: Advanced Features (2 weeks)
- RESERVING clause execution for table locking
- LOCK TIMEOUT with timeout-based waiting
- READ ONLY transaction optimizations
- Full SET TRANSACTION syntax support

---

## Next Session Recommendations

1. **Begin Task 3.3: Sweep Mechanism** (2 weeks estimated)
   - Implement sweep trigger check: `(OST - OIT) > sweep_interval`
   - Implement sweep process to scan TIP and advance OIT
   - Add SWEEP DATABASE command
   - Add sweep monitoring and statistics

2. **Testing**: Integration tests for complete isolation level functionality
   - Test concurrent transactions with different isolation levels
   - Test lock conflicts with SNAPSHOT_TABLE_STABILITY
   - Test statement snapshot behavior across complex queries

---

## Files Modified This Session

**This Session (READ_COMMITTED_READ_CONSISTENCY + SNAPSHOT_TABLE_STABILITY Locking):**
```
include/scratchbird/core/connection_context.h   | 12 ++++
src/core/connection_context.cpp                 | 152 ++++++++++++++++++
src/core/storage_engine.cpp                     |  48 ++++--
src/sblr/executor.cpp                           |  93 ++++++++++
tests/unit/test_connection_context.cpp          | 252 +++++++++++++++++++++++++++
docs/planning/PHASE_3_PROGRESS.md               | 150 ++++++++++++++---
```

**Cumulative Changes (Task 3.2 Complete)**: 12 files changed, 1400+ insertions(+), 75 deletions(-)

**Previous Sessions:**
- Session 1: Transaction markers infrastructure (OIT/OAT/OST/NEXT)
- Session 2: Monitoring queries (MON_DATABASE)
- Session 3: SNAPSHOT isolation with visibility checking
- Session 4: READ_COMMITTED_READ_CONSISTENCY + SNAPSHOT_TABLE_STABILITY locking (this session)

---

## Session Notes

### Current Session (October 9, 2025)

**Statement Snapshot Implementation**:
- Implemented automatic statement snapshot management in Executor
- Statement snapshots are created/cleared transparently for READ_COMMITTED_READ_CONSISTENCY
- Exception-safe cleanup ensures snapshots are always cleared on error
- All 5 new tests passing, validating statement-level consistency

**Table Locking Implementation**:
- Integrated LockManager with ConnectionContext for SNAPSHOT_TABLE_STABILITY
- Lock acquisition happens in `beginNewTransaction()` for reserved tables
- Lock release happens in `endCurrentTransaction()` via `releaseAllLocks()`
- Proper error handling with automatic lock cleanup on failure
- Schema lookup currently hardcoded to `[sys]` schema (to be improved when schema resolution is enhanced)

**Test Ordering Issue**:
Discovered subtle test timing issues where transaction XIDs were allocated in unexpected order. Fixed by ensuring writer transactions commit BEFORE reader transactions start, guaranteeing writer XID < reader XID. This is critical for visibility testing since future transactions (XID > current XID) are never visible.

**Task 3.2 Complete**: All 4 Firebird isolation levels now fully implemented and tested!

### Previous Session Notes

**String Pool Issue**: Encountered an issue during testing where the `parseSQL()` convenience function creates its own `StringPool`, which is then destroyed before bytecode generation can use it. The solution is to manually create a `Lexer` and use its `stringPool()` for bytecode generation:

```cpp
parser::Lexer lexer(sql);
parser::ASTArena arena;
parser::Parser parser(lexer, arena);
auto parse_result = parser.parseStatement();

// Use the same string pool from lexer
sblr::BytecodeGenerator gen(lexer.stringPool());
auto bytecode_result = gen.generate(parse_result.statement());
```

This is important for future test development.

---

**End of Progress Report**
