# ExecutorTransactionPayloadTest Timeout Analysis - 2025-12-30

**Date:** 2025-12-30
**Status:** 🟡 ANALYSIS INCOMPLETE - Suspected CREATE TABLE or Autocommit Issue
**Tests Failing:** 2 tests timing out at 300 seconds

---

## Test Failures

**Both tests timeout at 300 seconds:**

1. **AutocommitOnCommitsAfterStatement** (test_executor_transaction_payload.cpp:91)
   - Sets `AUTOCOMMIT ON`
   - Executes `CREATE TABLE autocommit_on_test (id INT)`
   - Expects XID to change after statement (commit should happen)
   - **HANGS** during CREATE TABLE or subsequent autocommit

2. **AutocommitOffKeepsXid** (test_executor_transaction_payload.cpp:108)
   - Sets `AUTOCOMMIT OFF`
   - Executes `CREATE TABLE autocommit_off_test (id INT)`
   - Expects XID to remain same (no commit)
   - **HANGS** during CREATE TABLE

---

## Test Code Analysis

### Test 1: AutocommitOnCommitsAfterStatement

```cpp
TEST_F(ExecutorTransactionPayloadTest, AutocommitOnCommitsAfterStatement) {
    // Step 1: Set autocommit ON
    auto set_compiled = compile("SET AUTOCOMMIT ON");
    ASSERT_TRUE(set_compiled.success()) << joinErrors(set_compiled.errors());
    auto result = executor_->execute(set_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_before = conn_->getCurrentXid();

    // Step 2: Execute CREATE TABLE (should auto-commit after)
    auto ddl_compiled = compile("CREATE TABLE autocommit_on_test (id INT)");
    ASSERT_TRUE(ddl_compiled.success()) << joinErrors(ddl_compiled.errors());
    result = executor_->execute(ddl_compiled.bytecode());  // ❌ HANGS HERE
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_after = conn_->getCurrentXid();
    EXPECT_NE(xid_before, xid_after);  // XID should change after autocommit
}
```

### Test 2: AutocommitOffKeepsXid

```cpp
TEST_F(ExecutorTransactionPayloadTest, AutocommitOffKeepsXid) {
    // Step 1: Set autocommit OFF
    auto set_compiled = compile("SET AUTOCOMMIT OFF");
    ASSERT_TRUE(set_compiled.success()) << joinErrors(set_compiled.errors());
    auto result = executor_->execute(set_compiled.bytecode());
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_before = conn_->getCurrentXid();

    // Step 2: Execute CREATE TABLE (should NOT auto-commit)
    auto ddl_compiled = compile("CREATE TABLE autocommit_off_test (id INT)");
    ASSERT_TRUE(ddl_compiled.success()) << joinErrors(ddl_compiled.errors());
    result = executor_->execute(ddl_compiled.bytecode());  // ❌ HANGS HERE
    ASSERT_TRUE(result.success()) << result.error();

    uint64_t xid_after = conn_->getCurrentXid();
    EXPECT_EQ(xid_before, xid_after);  // XID should NOT change (no commit)
}
```

---

## Hypotheses

### Hypothesis 1: CREATE TABLE Deadlock (Less Likely)

**Analysis:** Similar to `dropTable` deadlock, `createTable` might have lock ordering issues.

**createTable Implementation (catalog_manager.cpp:4524):**
```cpp
auto CatalogManager::createTable(...) -> Status
{
    std::lock_guard<std::mutex> lock(mutex_);  // Acquires mutex_

    // ... table creation logic ...

    // Phase 2: Create dependencies for domain-based columns
    for (const auto& col : columns_with_ids) {
        if (col.domain_id == ID{}) {
            continue;
        }
        ID dep_id;
        status = createDependency(  // ❌ Calls createDependency
            col.column_id, ObjectType::COLUMN,
            col.domain_id, ObjectType::DOMAIN,
            DependencyType::NORMAL,
            dep_id,
            ctx
        );
    }

    // Phase 5: Track sequence usage in DEFAULT expressions
    for (const auto& col : columns_with_ids) {
        // ... sequence dependency creation ...
        status = getSequenceIdByName(schema_id, name, seq_id, ctx);  // ❌ What locks?
        status = getSequence(schema_id, name, sinfo, ctx);  // ❌ What locks?
        status = createDependency(...);  // ❌ Calls createDependency again
    }
}
```

**createDependency (catalog_manager.cpp:17641):**
```cpp
auto CatalogManager::createDependency(...) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);  // Acquires dependency_cache_mutex_
    // ...
}
```

**Lock Order:**
1. `createTable()` acquires `mutex_` (line 4529)
2. `createTable()` calls `createDependency()` (line 4621, 4659, 4683, etc.)
3. `createDependency()` acquires `dependency_cache_mutex_` (line 17646)

**This is CORRECT lock ordering:** `mutex_` → `dependency_cache_mutex_`

**BUT** - If another thread tries to acquire locks in REVERSE order (dependency_cache_mutex_ → mutex_), we get a classic deadlock.

**Verdict:** Unlikely to be the direct cause, unless there's a background thread violating lock order.

---

### Hypothesis 2: Autocommit Triggers Implicit COMMIT (More Likely)

**Analysis:** When `AUTOCOMMIT ON`, after each statement execution, an implicit `COMMIT` should occur. This implicit commit might be calling functions that deadlock.

**Expected Flow:**
1. Test sets `AUTOCOMMIT ON`
2. Test executes `CREATE TABLE autocommit_on_test (id INT)`
3. Executor completes CREATE TABLE successfully
4. **Executor triggers implicit COMMIT** (because autocommit is ON)
5. **COMMIT hangs** (deadlock or infinite wait)

**Why AutocommitOffKeepsXid Also Hangs:**
- Even with `AUTOCOMMIT OFF`, `CREATE TABLE` is a DDL statement
- DDL statements often have **implicit transaction semantics**
- The issue might not be autocommit-specific, but rather something wrong with CREATE TABLE completion

**What to investigate:**
1. Does CREATE TABLE acquire a lock and never release it?
2. Does the implicit commit after CREATE TABLE try to acquire a lock in wrong order?
3. Is there a background thread (garbage collector, long transaction monitor, sweep manager) that's deadlocking with CREATE TABLE?

---

### Hypothesis 3: Background Thread Deadlock (Most Likely)

**Evidence from test log:**
```
[2025-12-30 10:56:59.660] [INFO] [VACUUM] [sweep_manager.cpp:44] SweepManager initialized
[2025-12-30 10:56:59.660] [INFO] [VACUUM] [garbage_collector.cpp:71] GarbageCollector initialized with policy: COMBINED, interval: 5000ms, rate: 1/100
[2025-12-30 10:56:59.660] [INFO] [TRANSACTION] [long_transaction_monitor.cpp:50] LongTransactionMonitor initialized: warn=600s, critical=3600s, check=60s, policy=0
[2025-12-30 10:56:59.660] [INFO] [TRANSACTION] [long_transaction_monitor.cpp:140] Long transaction monitor thread started
[2025-12-30 10:56:59.660] [INFO] [TRANSACTION] [long_transaction_monitor.cpp:230] Long transaction monitoring loop started
```

**Three background threads are started:**
1. **SweepManager** - May access heap pages and catalog
2. **GarbageCollector** - Checks pages every 5000ms, may access catalog
3. **LongTransactionMonitor** - Checks transactions every 60s, may access catalog

**Deadlock Scenario:**

**Thread 1 (Test):**
1. Holds `mutex_` (from createTable)
2. Tries to acquire `dependency_cache_mutex_` (from createDependency)

**Thread 2 (Background - GarbageCollector, SweepManager, or Monitor):**
1. Holds `dependency_cache_mutex_` (checking dependencies for cleanup)
2. Tries to acquire `mutex_` (accessing table catalog)

**Result:** Classic cross-thread deadlock.

**Why it manifests with CREATE TABLE:**
- CREATE TABLE holds `mutex_` for a long time
- Background threads may wake up during this time
- Background threads try to access catalog while test holds mutex_

**Why both AutocommitOn and AutocommitOff fail:**
- The deadlock is in CREATE TABLE itself, not in autocommit handling
- Both tests execute CREATE TABLE, so both hit the same deadlock

---

## Relationship to dropTable Deadlock

**Common Pattern:** Both `createTable` and `dropTable` are affected by lock ordering issues.

**createTable lock pattern:**
```
createTable: mutex_ → dependency_cache_mutex_ (via createDependency)
```

**dropTable lock pattern (FIXED):**
```
dropTable: std::scoped_lock(mutex_, dependency_cache_mutex_)
```

**Difference:**
- `dropTable` was fixed to acquire BOTH locks upfront
- `createTable` acquires `mutex_` first, then calls `createDependency()` which acquires `dependency_cache_mutex_` separately
- This creates a window where background threads can interleave

---

## Investigation Steps Needed

### Step 1: Verify Lock Ordering in createTable

Check if there's a background thread that acquires locks in reverse order:

```bash
# Search for code that acquires dependency_cache_mutex_ first, then mutex_
grep -rn "lock.*dependency_cache_mutex_" src/core/ | grep -B5 -A5 "mutex_"
```

### Step 2: Add Logging to Identify Deadlock

Add debug logging to:
1. `createTable()` - log when mutex_ is acquired/released
2. `createDependency()` - log when dependency_cache_mutex_ is acquired/released
3. Background threads - log when they try to acquire catalog locks

### Step 3: Check if Background Threads Access Catalog

Investigate:
- **GarbageCollector**: Does it access `table_cache_` or call catalog functions?
- **SweepManager**: Does it access catalog during sweep?
- **LongTransactionMonitor**: Does it access catalog to check transaction states?

### Step 4: Reproduce with Minimal Test

Create a minimal test that:
1. Starts database (initializes background threads)
2. Immediately executes CREATE TABLE
3. Observes if it hangs

If it hangs even without autocommit settings, confirms issue is in CREATE TABLE, not autocommit.

---

## Potential Fixes (NOT IMPLEMENTED - DOCUMENTATION ONLY)

### Fix Option 1: Apply Same Pattern as dropTable

**Change createTable to acquire both locks upfront:**

```cpp
auto CatalogManager::createTable(...) -> Status
{
    // Acquire BOTH locks in consistent order
    std::scoped_lock lock(mutex_, dependency_cache_mutex_);

    // ... table creation logic ...

    // Create dependencies WITHOUT calling createDependency (use internal version)
    for (const auto& col : columns_with_ids) {
        if (col.domain_id == ID{}) {
            continue;
        }
        ID dep_id;
        // Use createDependencyInternal (assumes locks already held)
        createDependencyInternal(col.column_id, ObjectType::COLUMN,
                                col.domain_id, ObjectType::DOMAIN,
                                DependencyType::NORMAL, dep_id, ctx);
    }
}
```

**Requires:** Creating `createDependencyInternal()` helper that assumes locks already held.

### Fix Option 2: Disable Background Threads During Tests

**Temporary workaround:**
- Disable GarbageCollector, SweepManager, LongTransactionMonitor during unit tests
- Only enable them in production/integration tests

**Pros:** Quick fix for testing
**Cons:** Doesn't solve the real problem, may hide production deadlocks

### Fix Option 3: Refine Lock Ordering Rules

**Establish strict lock hierarchy:**
1. All background threads MUST respect the same lock order: `mutex_` → `dependency_cache_mutex_`
2. No thread should acquire `dependency_cache_mutex_` without also acquiring `mutex_` first
3. Use `std::scoped_lock(mutex_, dependency_cache_mutex_)` everywhere

---

## Test Execution Timeline

### AutocommitOnCommitsAfterStatement Timeline

```
10:56:59.656 - Database initialization starts
10:56:59.660 - Background threads started
10:56:59.660 - Test SetUp complete
10:56:59.6XX - SET AUTOCOMMIT ON (succeeds)
10:56:59.6XX - CREATE TABLE autocommit_on_test (hangs)
11:01:59.749 - ***Timeout 300 seconds
```

**Total hang time:** 300 seconds = 5 minutes

### AutocommitOffKeepsXid Timeline

```
11:01:59.749 - Database initialization starts
11:01:59.754 - Background threads started
11:01:59.754 - Test SetUp complete
11:01:59.7XX - SET AUTOCOMMIT OFF (succeeds)
11:01:59.7XX - CREATE TABLE autocommit_off_test (hangs)
11:06:59.800 - ***Timeout 300 seconds
```

**Total hang time:** 300 seconds = 5 minutes

**Pattern:** Both tests hang at the same point - during CREATE TABLE execution, NOT during autocommit handling.

---

## Next Steps

1. **Verify hypothesis:** Add logging to createTable and background threads to capture exact deadlock sequence
2. **Check background thread behavior:** Determine if GC/Sweep/Monitor threads access catalog during CREATE TABLE
3. **Apply similar fix to createTable:** Use `std::scoped_lock` and internal helper versions
4. **Run test again** with logging enabled to confirm root cause

---

## Priority

**Severity:** 🟡 **HIGH** (but lower than dropTable deadlock)
**Impact:** Blocks 2 tests, affects CREATE TABLE operations
**Relationship:** Likely related to dropTable deadlock (same lock ordering issues)
**Must Fix:** YES - after dropTable fixes are complete

---

**Analysis By:** Claude Code
**Date:** 2025-12-30
**Status:** DOCUMENTED - NO FIX ATTEMPTED PER USER REQUEST
**Confidence:** Medium (hypothesis needs verification)
