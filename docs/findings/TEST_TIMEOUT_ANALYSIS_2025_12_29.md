# Test Timeout Analysis - 2025-12-29

**Analysis Date:** 2025-12-29
**Analyzed By:** Claude Code
**Status:** 🔴 CRITICAL - Deadlock Identified

---

## Executive Summary

**Problem:** 4 tests timeout after 300 seconds with repeating long transaction monitor errors.

**Root Cause:** Potential deadlock in `CatalogManager::dropFunction()` and `CatalogManager::dropProcedure()` when dependency checks are performed.

**Impact:** Tests hang indefinitely, suggesting production risk if dependencies are created and drop operations attempted.

**Priority:** CRITICAL - Fix required before production use

---

## Failing Tests

All 4 timeout failures are in `StoredCodeDependencyTest`:

1. **Test #222:** `DropFunctionFailsIfCalledByAnotherFunction` (line 210)
2. **Test #224:** `DropProcedureFailsIfCalled` (line 274)
3. **Test #227:** `ComplexFunctionChain` (line 360)
4. **Test #229:** `MixedFunctionProcedureDependencies` (line 437)

**Common Pattern:**
- All tests create dependency relationships between database objects
- All attempt to drop an object that has dependents
- All expect `Status::CONSTRAINT_VIOLATION` to be returned
- **All hang instead of returning**

---

## Error Pattern

During hang, continuous error output:
```
[ERROR] [TRANSACTION] [long_transaction_monitor.cpp:267]
Failed to get active backends for long transaction check
```

**Analysis:**
- Transaction exceeds long-running threshold
- Monitor attempts to check active backends
- Monitor cannot access catalog (likely locked)
- Error repeats until 300s timeout

---

## Root Cause Analysis

### Lock Ordering Issue in CatalogManager

**File:** `src/core/catalog_manager.cpp`

#### Problem in `dropFunction()` (lines 11577-11637)

```cpp
auto CatalogManager::dropFunction(const std::string &name, bool if_exists,
                                  ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> lock(psql_mutex_);  // ← LOCK 1

    // ... find function ...

    // Check for dependents
    std::vector<DependencyInfo> dependents;
    Status status = getDependents(func.function_id, dependents, ctx);  // ← Calls function that acquires LOCK 2
    if (status != Status::OK) {
        return status;
    }

    // ... later ...

    // Clear dependencies
    clearDependenciesFor(func.function_id, ctx);  // ← Calls function that acquires LOCK 2 again

    functions_.erase(it);
    return Status::OK;
}
```

#### `getDependents()` Lock Acquisition (lines 17759-17775)

```cpp
auto CatalogManager::getDependents(const ID& object_id,
                                  std::vector<DependencyInfo>& dependents_out,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);  // ← LOCK 2

    // ... iterate dependency_cache_ ...

    return Status::OK;
}
```

#### `clearDependenciesFor()` Lock Acquisition (lines 11883-11906)

```cpp
auto CatalogManager::clearDependenciesFor(const ID& dependent_object_id,
                                          ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);  // ← LOCK 2 (again)

    // ... clear dependencies ...

    return Status::OK;
}
```

### Lock Order

**In `dropFunction()`:**
1. Acquires `psql_mutex_` (line 11580)
2. Calls `getDependents()` which acquires `dependency_cache_mutex_` (line 11763)
3. Calls `clearDependenciesFor()` which tries to acquire `dependency_cache_mutex_` again (line 11886)

**Lock Acquisition Order:**
```
psql_mutex_ → dependency_cache_mutex_ (multiple times)
```

### Potential Deadlock Scenarios

#### Scenario 1: Recursive Lock Contention

If `dependency_cache_mutex_` is a **non-recursive mutex**, the second acquisition in `clearDependenciesFor()` would deadlock.

**Evidence:**
- `std::mutex` is NOT recursive by default
- Two different functions called from `dropFunction()` both try to lock `dependency_cache_mutex_`
- If `getDependents()` doesn't release the lock before `clearDependenciesFor()` is called, **deadlock**

**However:** `getDependents()` uses `std::lock_guard`, which releases on scope exit. So this is unlikely unless there's something keeping the stack frame alive.

#### Scenario 2: Cross-Thread Deadlock

If another thread acquires locks in reverse order:

**Thread 1 (test thread):**
```
psql_mutex_ → dependency_cache_mutex_
```

**Thread 2 (long transaction monitor?):**
```
dependency_cache_mutex_ → psql_mutex_
```

This is a **classic deadlock**.

#### Scenario 3: Long Transaction Monitor Interference

From error message, the long transaction monitor is trying to access "active backends", which may require catalog access.

**Hypothesis:**
1. Test thread holds `psql_mutex_`
2. Test thread tries to acquire `dependency_cache_mutex_`
3. Long transaction monitor (separate thread) holds `dependency_cache_mutex_`
4. Monitor tries to access catalog (requires `psql_mutex_`)
5. **DEADLOCK**

---

## Test Code Analysis

### Test #222: DropFunctionFailsIfCalledByAnotherFunction

**File:** `tests/unit/test_code_dependencies.cpp` (lines 210-236)

```cpp
TEST_F(StoredCodeDependencyTest, DropFunctionFailsIfCalledByAnotherFunction)
{
    ErrorContext ctx;

    // Create table
    ID table_id = createTestTable("orders");

    // Create base function (depends on table)
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps1;
    deps1.emplace_back(table_id, CatalogManager::ObjectType::TABLE);
    ID base_func_id = createTestFunction("get_order_count", "SELECT COUNT(*) FROM orders", deps1);

    // Create caller function (depends on base function)
    std::vector<std::pair<ID, CatalogManager::ObjectType>> deps2;
    deps2.emplace_back(base_func_id, CatalogManager::ObjectType::FUNCTION);
    ID caller_func_id = createTestFunction("check_orders", "SELECT get_order_count()", deps2);

    // Try to drop base function - should fail with CONSTRAINT_VIOLATION
    Status status = catalog->dropFunction("get_order_count", false, &ctx);  // ← HANGS HERE
    EXPECT_EQ(status, Status::CONSTRAINT_VIOLATION);

    // ...
}
```

**Expected Behavior:**
- `dropFunction()` should check dependencies
- Find that `check_orders` (caller_func_id) depends on `get_order_count` (base_func_id)
- Return `Status::CONSTRAINT_VIOLATION` immediately
- Test passes

**Actual Behavior:**
- `dropFunction()` called
- **Never returns**
- Test times out after 300 seconds

---

## Long Transaction Monitor Involvement

**Error Location:** `src/core/long_transaction_monitor.cpp:267`

```
Failed to get active backends for long transaction check
```

**Analysis:**

The long transaction monitor runs in a background thread and periodically checks for long-running transactions. When `dropFunction()` hangs, the transaction exceeds the threshold, triggering the monitor.

**Probable Monitor Code Path:**
1. Monitor detects long-running transaction
2. Tries to get active backend list (requires catalog access)
3. Tries to acquire `psql_mutex_` or `dependency_cache_mutex_`
4. **Blocked** because test thread holds one of these locks
5. Monitor fails to get backends
6. Logs error
7. Retries → error repeats

**This creates a vicious cycle:**
- Test thread: Holds locks, waiting for something
- Monitor thread: Wants locks, can't access catalog
- Neither makes progress → **deadlock**

---

## Additional Evidence

### Test #223 (DropFunctionSucceedsAfterDroppingDependent) Likely PASSES

This test drops the dependent function FIRST, then drops the base function:

```cpp
// Drop caller function first
ASSERT_EQ(catalog->dropFunction("check_customers", false, &ctx), Status::OK);

// Now drop base function - should succeed (no dependents)
ASSERT_EQ(catalog->dropFunction("get_customer_count", false, &ctx), Status::OK);
```

**Why this probably works:**
- First `dropFunction()` call has no dependents (caller function has no dependents on it)
- Second `dropFunction()` call has no dependents (caller was already dropped)
- **No dependency check finds anything** → no hang

**Conclusion:** The hang only occurs when `getDependents()` finds actual dependents.

---

## Recommended Fixes

### Option 1: Remove Nested Dependency Mutex Acquisition

**Change:** Make `getDependents()` and `clearDependenciesFor()` internal helpers that don't acquire locks.

**Implementation:**
```cpp
// Internal helper (assumes dependency_cache_mutex_ already held)
auto CatalogManager::getDependentsInternal(const ID& object_id,
                                           std::vector<DependencyInfo>& dependents_out) -> Status
{
    // NO LOCK - caller must hold dependency_cache_mutex_

    dependents_out.clear();
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.referenced_object_id == object_id) {
            dependents_out.push_back(dep_info);
        }
    }
    return Status::OK;
}

// Public API (acquires lock)
auto CatalogManager::getDependents(const ID& object_id,
                                  std::vector<DependencyInfo>& dependents_out,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);
    return getDependentsInternal(object_id, dependents_out);
}

// In dropFunction:
auto CatalogManager::dropFunction(const std::string &name, bool if_exists,
                                  ErrorContext *ctx) -> Status
{
    std::lock_guard<std::mutex> psql_lock(psql_mutex_);
    std::lock_guard<std::mutex> dep_lock(dependency_cache_mutex_);  // Acquire both upfront

    // ... rest of function uses Internal helpers that don't lock ...
}
```

**Pros:**
- Fixes deadlock by acquiring both locks upfront in consistent order
- Clear lock ownership

**Cons:**
- Requires refactoring multiple functions
- Increases lock hold time (both locks held for entire operation)

### Option 2: Use Recursive Mutex

**Change:** Change `dependency_cache_mutex_` to `std::recursive_mutex`

```cpp
// In catalog_manager.h
std::recursive_mutex dependency_cache_mutex_;
```

**Pros:**
- Minimal code change
- Allows same thread to acquire lock multiple times

**Cons:**
- Hides the real problem (lock re-acquisition)
- Doesn't solve cross-thread deadlock (if that's the issue)
- Recursive mutexes are slower

### Option 3: Separate Lock for Each Operation

**Change:** Release `psql_mutex_` before calling dependency functions

```cpp
auto CatalogManager::dropFunction(const std::string &name, bool if_exists,
                                  ErrorContext *ctx) -> Status
{
    ID func_id;
    std::string func_name;

    {
        std::lock_guard<std::mutex> lock(psql_mutex_);

        auto it = functions_.find(name);
        if (it == functions_.end()) {
            // ... handle not found ...
        }

        func_id = it->second.function_id;
        func_name = name;
    }  // ← Release psql_mutex_

    // Now check dependencies (acquires dependency_cache_mutex_)
    std::vector<DependencyInfo> dependents;
    Status status = getDependents(func_id, dependents, ctx);  // NO DEADLOCK
    if (status != Status::OK) {
        return status;
    }

    if (!dependents.empty()) {
        // ... return constraint violation ...
    }

    {
        std::lock_guard<std::mutex> lock(psql_mutex_);  // Re-acquire
        // ... perform actual drop ...
    }

    // Clear dependencies
    clearDependenciesFor(func_id, ctx);  // NO DEADLOCK

    return Status::OK;
}
```

**Pros:**
- Avoids holding multiple locks simultaneously
- Reduces lock contention

**Cons:**
- Introduces race condition window between locks
- Function may be dropped by another thread between checks
- Requires careful validation logic

### Option 4: Use Try-Lock with Backoff (Not Recommended)

Attempt to acquire locks with timeouts and retry.

**Cons:**
- Doesn't solve the problem, just makes it less likely
- Adds complexity
- Poor performance

---

## Recommended Solution

**Recommendation:** **Option 1** (Internal helpers with consistent lock ordering)

**Reasoning:**
1. **Correctness:** Eliminates deadlock by ensuring consistent lock order
2. **Clarity:** Explicit about lock ownership
3. **Safety:** No race conditions introduced
4. **Maintainability:** Clear separation between public API (locks) and internal helpers (no locks)

**Implementation Plan:**
1. Create internal helper versions: `getDependentsInternal()`, `clearDependenciesForInternal()`
2. Modify `dropFunction()`, `dropProcedure()`, `dropTable()` to acquire both locks upfront
3. Update all calls to use internal helpers
4. Add comments documenting lock requirements
5. Run tests to verify fix

**Estimated Effort:** 2-3 hours

---

## Testing Plan

### Step 1: Verify Current Failure

```bash
cd build
./tests/scratchbird_tests \
  --gtest_filter="StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction" \
  --gtest_break_on_failure
```

**Expected:** Timeout after 300s

### Step 2: Apply Fix

Implement Option 1 changes in `catalog_manager.cpp`

### Step 3: Verify Fix

```bash
cd build
cmake --build . --target scratchbird_tests
./tests/scratchbird_tests \
  --gtest_filter="StoredCodeDependencyTest.*"
```

**Expected:** All 10 StoredCodeDependencyTest tests pass

### Step 4: Full Regression

```bash
cd build
ctest --output-on-failure
```

**Expected:** All 1,346 tests pass (or only BytecodeOpcodesTest fails due to build artifact)

---

## Priority and Impact

**Severity:** 🔴 CRITICAL

**Impact:**
- **Production Risk:** HIGH - Could deadlock production system if dependencies created
- **Test Coverage:** 4 tests (0.3% of suite) affected
- **User Impact:** Any user creating functions/procedures with dependencies could trigger deadlock on DROP

**Priority:** Fix before any release or production deployment

**Blocking:** Should block merges to main/release branches until resolved

---

## Additional Investigation Needed

1. **Verify Lock Usage Elsewhere:**
   - Search for all uses of `psql_mutex_` and `dependency_cache_mutex_`
   - Check if any other code acquires them in reverse order
   - Look for other potential deadlock scenarios

2. **Long Transaction Monitor:**
   - Investigate exactly what the monitor is trying to access
   - Verify monitor's lock acquisition order
   - Consider if monitor should have read-only access

3. **Thread Safety Audit:**
   - Review all CatalogManager public methods
   - Document lock acquisition patterns
   - Consider lock-order enforcement (via thread annotations or runtime checks)

---

## References

- **Test File:** `tests/unit/test_code_dependencies.cpp`
- **Implementation:** `src/core/catalog_manager.cpp`
  - `dropFunction()` - line 11577
  - `dropProcedure()` - line 11639
  - `getDependents()` - line 17759
  - `clearDependenciesFor()` - line 11883
- **Previous Analysis:** `/docs/findings/TEST_SUITE_FAILURES_2025_12_27.md`

---

**Status:** 🔴 AWAITING FIX
**Next Action:** Implement Option 1 (internal helpers with consistent lock ordering)
**Estimated Fix Time:** 2-3 hours
