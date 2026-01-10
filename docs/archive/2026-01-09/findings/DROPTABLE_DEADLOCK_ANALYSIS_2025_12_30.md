# dropTable Deadlock Analysis - 2025-12-30

**Date:** 2025-12-30
**Status:** ✅ RESOLVED - dropTable/dropSequence deadlock eliminated
**Tests:** `ctest -R TableDependencyTest --test-dir build` (12/12 passed, 2026-01-10)

---

## Executive Summary

The deadlock fix implemented earlier for `dropFunction()` and `dropProcedure()` DID work for those functions. However, `dropTable()` has a DIFFERENT deadlock pattern that was not addressed.

### Status Update (2026-01-10)
- Added internal helpers: `getDependenciesForInternal()` and `dropSequenceInternal()`; wired `dropTable()` to internal versions.
- Updated `dropSequence()` to lock `sequence_cache_mutex_` + `dependency_cache_mutex_` up front and use internal helpers.
- Table dependency tests now pass without timeouts.

**Root Cause:** `dropTable()` holds both `mutex_` and `dependency_cache_mutex_` using `std::scoped_lock`, then calls several functions that try to RE-ACQUIRE `dependency_cache_mutex_`. Since `std::mutex` is NON-RECURSIVE, this causes a **same-thread deadlock**.

---

## Affected Tests

**TableDependencyTest Timeouts (all 300 seconds):**
1. `DropTableAutoDropsIndexes` - hangs after creating index dependencies
2. `DropTableAutoDropsTriggers` - hangs after creating trigger dependencies
3. `DropTableAutoDropsChildFK` - hangs after creating FK dependencies
4. `ComplexMixedDependencies` - hangs after creating mixed dependencies

**Pattern:** All tests hang immediately after logging "Created dependency" messages, when the test attempts to call `dropTable()`.

---

## The Deadlock Mechanism

### Current dropTable() Implementation (catalog_manager.cpp:11787)

```cpp
Status CatalogManager::dropTable(const ID &table_id, bool cascade, ErrorContext *ctx)
{
    // Line 11797: Acquire BOTH locks
    std::scoped_lock lock(mutex_, dependency_cache_mutex_);

    // ... validation ...

    // Line 11812: OK - uses internal version
    getDependentsInternal(table_id, all_deps);

    // ... filtering ...

    // Line 11822: OK - uses internal version
    resolveDependencyNamesInternal(filtered.blocking, resolved, ctx);

    // ... owned object processing ...

    // ❌ DEADLOCK #1 - Line 11890
    Status deps_status = getDependenciesFor(table_id, table_deps, ctx);

    // ❌ DEADLOCK #2 - Line 11910
    status = dropSequence(dep.dependent_object_id, false, ctx);

    // ... more operations ...
}
```

### Deadlock #1: getDependenciesFor() Re-Acquisition (Line 11890)

**Location:** catalog_manager.cpp:11890
```cpp
// dropTable holds: mutex_, dependency_cache_mutex_
Status deps_status = getDependenciesFor(table_id, table_deps, ctx);
```

**getDependenciesFor() implementation (Line 17750):**
```cpp
auto CatalogManager::getDependenciesFor(const ID& object_id,
                                       std::vector<DependencyInfo>& dependencies_out,
                                       ErrorContext* ctx) -> Status
{
    // ❌ DEADLOCK: Tries to re-acquire dependency_cache_mutex_
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);

    dependencies_out.clear();
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.dependent_object_id == object_id) {
            dependencies_out.push_back(dep_info);
        }
    }
    return Status::OK;
}
```

**Deadlock Sequence:**
1. `dropTable()` acquires `dependency_cache_mutex_` (line 11797)
2. `dropTable()` calls `getDependenciesFor()` (line 11890)
3. `getDependenciesFor()` tries to acquire `dependency_cache_mutex_` (line 17754)
4. **DEADLOCK** - Same thread trying to lock a non-recursive mutex it already holds

### Deadlock #2: dropSequence() Re-Acquisition (Line 11910)

**Location:** catalog_manager.cpp:11910
```cpp
// dropTable holds: mutex_, dependency_cache_mutex_
status = dropSequence(dep.dependent_object_id, false, ctx);
```

**dropSequence() implementation (Line 15427):**
```cpp
auto CatalogManager::dropSequence(const ID& sequence_id, bool cascade, ErrorContext* ctx) -> Status
{
    // ... sequence lookup ...

    // ❌ DEADLOCK: Tries to acquire dependency_cache_mutex_
    Status status = getDependents(sequence_id, deps, ctx);  // Line 15446

    if (!deps.empty()) {
        // ❌ DEADLOCK: Tries to acquire both mutexes again
        resolveDependencyNames(deps, resolved, ctx);  // Line 15453
    }

    // ... rest of function ...
}
```

**getDependents() implementation (Line 17768):**
```cpp
auto CatalogManager::getDependents(const ID& object_id,
                                  std::vector<DependencyInfo>& dependents_out,
                                  ErrorContext* ctx) -> Status
{
    // ❌ DEADLOCK: Tries to re-acquire dependency_cache_mutex_
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);

    // ... implementation ...
}
```

**Deadlock Sequence:**
1. `dropTable()` acquires `dependency_cache_mutex_` (line 11797)
2. `dropTable()` calls `dropSequence()` (line 11910)
3. `dropSequence()` calls `getDependents()` (line 15446)
4. `getDependents()` tries to acquire `dependency_cache_mutex_` (line 17773)
5. **DEADLOCK** - Same thread trying to lock a non-recursive mutex it already holds

---

## Why The Previous Fix Didn't Cover This

The deadlock fix for `dropFunction()` and `dropProcedure()` (implemented in DEADLOCK_FIX_2025_12_30.md) correctly:
- Used `std::scoped_lock(psql_mutex_, dependency_cache_mutex_)`
- Used internal helper versions: `getDependentsInternal()`, `resolveDependencyNamesInternal()`, `clearDependenciesForInternal()`
- Avoided calling any public API functions that re-acquire locks

However, `dropTable()` has **additional complexity**:
1. It needs to drop **owned sequences** (line 11901-11915)
2. It needs to call `getDependenciesFor()` to clear table→sequence dependencies (line 11890)
3. It needs to drop **triggers** (line 11857-11868), which uses a DIFFERENT mutex (`trigger_mutex_`)
4. It needs to drop **constraints** (line 11878-11885)

The fix was **incomplete** because:
- `getDependenciesFor()` was called instead of creating a `getDependenciesForInternal()` version
- `dropSequence()` was called instead of creating a `dropSequenceInternal()` version
- The original implementer may not have realized these functions also acquire `dependency_cache_mutex_`

---

## Functions That Need Internal Versions

### Already Have Internal Versions ✅
1. `getDependentsInternal()` - line 17808 ✅
2. `clearDependenciesForInternal()` - line 17940 ✅
3. `resolveDependencyNamesInternal()` - line 18713 ✅
4. `getObjectNameInternal()` - line 18390 ✅

### Need To Create Internal Versions ❌
1. **`getDependenciesForInternal()`** - Does NOT exist
   - Public version at line 17750 acquires `dependency_cache_mutex_`
   - Called from dropTable at line 11890

2. **`dropSequenceInternal()`** - Does NOT exist
   - Public version at line 15427 calls `getDependents()` and `resolveDependencyNames()`
   - Called from dropTable at line 11910

3. **`dropConstraintInternal()`** - Does NOT exist (if dropConstraint also acquires locks)
   - Called from dropTable at line 11880
   - Need to verify what locks dropConstraint acquires

---

## Additional Issues Found

### Issue: dropSequence() Has Same Pattern as dropFunction()

**dropSequence() (Line 15427):**
```cpp
auto CatalogManager::dropSequence(const ID& sequence_id, bool cascade, ErrorContext* ctx) -> Status
{
    // ... sequence lookup ...

    std::vector<DependencyInfo> deps;
    Status status = getDependents(sequence_id, deps, ctx);  // ❌ Acquires dependency_cache_mutex_

    if (!deps.empty()) {
        std::vector<DependencyName> resolved;
        resolveDependencyNames(deps, resolved, ctx);  // ❌ Calls public version
        // ...
    }

    // ... clear dependencies ...
    clearDependenciesFor(sequence_id, ctx);  // ❌ Calls public version
}
```

**This means `dropSequence()` itself needs the SAME fix that was applied to `dropFunction()`:**
- Change to `std::scoped_lock(sequence_cache_mutex_, dependency_cache_mutex_)` (if it needs both)
- Use `getDependentsInternal()` instead of `getDependents()`
- Use `resolveDependencyNamesInternal()` instead of `resolveDependencyNames()`
- Use `clearDependenciesForInternal()` instead of `clearDependenciesFor()`

**BUT** - This creates a circular dependency problem:
- `dropTable()` needs `dropSequenceInternal()` (which assumes locks held)
- But `dropSequence()` also needs to be usable standalone (which means it must acquire locks)

**Solution:** Create TWO versions:
1. `dropSequence()` - Public API, acquires locks
2. `dropSequenceInternal()` - Internal helper, assumes locks held

---

## Test Output Analysis

**TableDependencyTest.DropTableAutoDropsIndexes:**
```
[2025-12-30 11:07:55.350] [INFO] Created dependency: 019b7004-18d6-7337-8a8b-35c4fd611f6b (3) -> ...
[2025-12-30 11:07:55.350] [INFO] Created dependency: 019b7004-18d6-7ed7-b43e-eac469e86617 (3) -> ...
***Timeout 300 seconds
```
- Test creates 2 index dependencies
- Test calls dropTable
- dropTable hangs at line 11890 (`getDependenciesFor()`) or line 11910 (`dropSequence()`)
- Never completes, times out after 300 seconds

**TableDependencyTest.DropTableAutoDropsTriggers:**
```
[2025-12-30 11:12:55.407] [INFO] Created dependency: 019b7008-acef-7c4c-8592-a6a429f86881 (7) -> ...
[2025-12-30 11:12:55.407] [INFO] Created trigger 'trig1' on table ''
[2025-12-30 11:12:55.407] [INFO] Created dependency: 019b7008-acef-73d1-acb2-71679ac847ec (7) -> ...
[2025-12-30 11:12:55.407] [INFO] Created trigger 'trig2' on table ''
***Timeout 300 seconds
```
- Test creates 2 trigger dependencies
- Test calls dropTable
- dropTable hangs (same deadlock)

---

## Why This Is Different From The Previous Deadlock

**Previous Deadlock (dropFunction/dropProcedure) - FIXED:**
- Type: Cross-thread deadlock OR same-thread re-acquisition
- Pattern: Function acquires `psql_mutex_`, then calls another function that tries to acquire it
- Solution: Acquire both locks upfront, use internal helpers

**Current Deadlock (dropTable) - NOT FIXED:**
- Type: Same-thread re-acquisition (definitely)
- Pattern: dropTable acquires `mutex_` + `dependency_cache_mutex_`, then calls:
  - `getDependenciesFor()` which tries to acquire `dependency_cache_mutex_` again
  - `dropSequence()` which calls `getDependents()` which tries to acquire `dependency_cache_mutex_` again
- Solution: Create `getDependenciesForInternal()` and `dropSequenceInternal()` helpers

---

## Required Changes (NOT IMPLEMENTED - DOCUMENTATION ONLY)

### Change 1: Create getDependenciesForInternal()

**Add to catalog_manager.h:**
```cpp
private:
    // Internal helper (assumes dependency_cache_mutex_ already held)
    void getDependenciesForInternal(const ID& object_id,
                                   std::vector<DependencyInfo>& dependencies_out);
```

**Add to catalog_manager.cpp (near line 17766):**
```cpp
void CatalogManager::getDependenciesForInternal(const ID& object_id,
                                               std::vector<DependencyInfo>& dependencies_out)
{
    // NO LOCK - caller must hold dependency_cache_mutex_
    dependencies_out.clear();

    // Find all dependencies where this object is the dependent
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.dependent_object_id == object_id) {
            dependencies_out.push_back(dep_info);
        }
    }
}
```

**Update dropTable() line 11890:**
```cpp
// OLD (WRONG):
Status deps_status = getDependenciesFor(table_id, table_deps, ctx);

// NEW (CORRECT):
getDependenciesForInternal(table_id, table_deps);
```

### Change 2: Create dropSequenceInternal()

This is more complex because dropSequence has significant logic.

**Add to catalog_manager.h:**
```cpp
private:
    // Internal helper (assumes sequence_cache_mutex_ and dependency_cache_mutex_ already held)
    auto dropSequenceInternal(const ID& sequence_id, ErrorContext* ctx) -> Status;
```

**Implementation:** Extract the body of dropSequence() into dropSequenceInternal(), removing all lock acquisitions and using internal helper versions.

**Update dropTable() line 11910:**
```cpp
// OLD (WRONG):
status = dropSequence(dep.dependent_object_id, false, ctx);

// NEW (CORRECT):
status = dropSequenceInternal(dep.dependent_object_id, ctx);
```

### Change 3: Fix dropSequence() Itself

Even when called standalone, `dropSequence()` has the same deadlock pattern as the old `dropFunction()`.

**Update dropSequence() (line 15427):**
```cpp
auto CatalogManager::dropSequence(const ID& sequence_id, bool cascade, ErrorContext* ctx) -> Status
{
    (void)cascade;

    // Acquire BOTH locks in consistent order
    std::scoped_lock lock(sequence_cache_mutex_, dependency_cache_mutex_);

    // ... use internal helper versions throughout ...
    getDependentsInternal(sequence_id, deps);
    resolveDependencyNamesInternal(deps, resolved, ctx);
    clearDependenciesForInternal(sequence_id, ctx);

    // ... rest of implementation ...
}
```

---

## Lock Ordering Rules (Critical)

**Consistent Lock Order Across ALL Functions:**
1. `mutex_` (table mutex) - FIRST
2. `sequence_cache_mutex_` - SECOND
3. `trigger_mutex_` - CAN BE ACQUIRED INDEPENDENTLY (separate subsystem)
4. `dependency_cache_mutex_` - LAST

**Rule:** If a function needs multiple locks, it MUST acquire them in this order.

**Current Violations:**
- `dropTable()` acquires `mutex_` + `dependency_cache_mutex_` ✅ (correct order)
- `dropSequence()` acquires `sequence_cache_mutex_` first, but then calls functions that acquire `dependency_cache_mutex_` ❌ (wrong - should acquire both upfront)

---

## ExecutorTransactionPayloadTest Timeouts

**Tests Failing:**
1. `AutocommitOnCommitsAfterStatement` - timeout 300s
2. `AutocommitOffKeepsXid` - timeout 300s

**Analysis Needed:** These may be UNRELATED to the dropTable deadlock. Need separate investigation.

**Hypothesis:** These tests may be:
1. Waiting for a transaction to complete that never does
2. Hitting a different deadlock in transaction handling code
3. Having issues with autocommit state management

**Recommended:** Investigate these separately after dropTable fixes are complete.

---

## Estimated Complexity

**dropTable Fix Complexity:** MEDIUM-HIGH
- Need to create `getDependenciesForInternal()`  (simple)
- Need to create `dropSequenceInternal()` (complex - significant logic)
- Need to fix `dropSequence()` standalone (medium)
- Need to audit `dropConstraint()` for similar issues

**Estimated Time:** 4-6 hours

**Risk Level:** MEDIUM
- Changes affect critical drop operations
- Must maintain transactional consistency
- Must not break existing functionality

---

## Testing Strategy

After fixes are implemented:

1. **Run TableDependencyTest suite:**
   ```bash
   ctest -R TableDependencyTest --output-on-failure
   ```

2. **Run dropTable-specific tests:**
   ```bash
   ctest -R ".*DropTable.*" --output-on-failure
   ```

3. **Run full dependency test suite:**
   ```bash
   ctest -R ".*Dependency.*" --output-on-failure
   ```

4. **Verify no regressions in previously fixed tests:**
   ```bash
   ctest -R StoredCodeDependencyTest --output-on-failure
   ```

---

## Priority

**Severity:** 🔴 **CRITICAL BLOCKER**
**Impact:** Blocks ALL table drop operations with dependencies
**Tests Blocked:** 4 TableDependencyTest + potentially more
**Must Fix:** YES - before any further development

---

**Analysis By:** Claude Code
**Date:** 2025-12-30
**Status:** DOCUMENTED - NO FIX ATTEMPTED PER USER REQUEST
