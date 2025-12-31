# Test Timeout Re-Analysis - 2025-12-30

**Date:** 2025-12-30
**Status:** ✅ RESOLVED - Fix implemented and verified
**Tests Previously Failing:** All 4 StoredCodeDependencyTest timeouts (NOW PASSING)

---

## Summary

Another AI attempted to fix the deadlock by:
1. Changing `std::lock_guard` → `std::unique_lock` in drop functions
2. Calling `lock.unlock()` before resolving dependency names
3. Creating `resolveDependencyNames()` to resolve names outside the lock

**Result:** Tests still timeout after 60 seconds. Issue NOT resolved.

---

## Test Re-Run Results

```bash
ctest -R "StoredCodeDependencyTest\.(DropFunctionFailsIfCalledByAnotherFunction|DropProcedureFailsIfCalled|ComplexFunctionChain|MixedFunctionProcedureDependencies)" --timeout 60

Result: 0% tests passed, 4 tests failed out of 4

All 4 tests: ***Timeout 60 seconds
```

No "Failed to get active backends" errors in the 60-second runs (vs. 300-second original runs).

---

## Analysis of Attempted Fix

### What Was Changed

**File:** `src/core/catalog_manager.cpp`

**dropFunction() modification (lines 11577-11613):**

```cpp
auto CatalogManager::dropFunction(const std::string &name, bool if_exists,
                                  ErrorContext *ctx) -> Status
{
    std::unique_lock<std::mutex> lock(psql_mutex_);  // ← Changed from lock_guard

    // ... find function ...

    // Check for dependents
    std::vector<DependencyInfo> dependents;
    Status status = getDependents(func.function_id, dependents, ctx);  // ← Acquires dependency_cache_mutex_
    if (status != Status::OK) {
        return status;  // ← psql_mutex_ still held here
    }

    // Functions don't own other objects, so all dependents are blocking
    if (!dependents.empty()) {
        std::vector<DependencyName> resolved;
        lock.unlock();  // ← UNLOCK psql_mutex_
        resolveDependencyNames(dependents, resolved, ctx);  // ← NEW: Resolve names
        std::string error_msg = buildDependencyErrorMessage(
            name, ObjectType::FUNCTION, resolved);  // ← Changed signature
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // ... rest unchanged ...
}
```

**New function `resolveDependencyNames()` (lines 18410-18425):**

```cpp
void CatalogManager::resolveDependencyNames(const std::vector<DependencyInfo>& deps,
                                            std::vector<DependencyName>& names_out,
                                            ErrorContext* ctx)
{
    names_out.clear();
    names_out.reserve(deps.size());

    for (const auto& dep : deps) {
        DependencyName resolved;
        resolved.dependent_type = dep.dependent_type;
        resolved.dependent_name = getObjectName(dep.dependent_object_id,
                                                dep.dependent_type,
                                                ctx);  // ← Calls getObjectName
        names_out.push_back(std::move(resolved));
    }
}
```

**getObjectName() for FUNCTION (line 18227-18232):**

```cpp
case ObjectType::FUNCTION: {
    std::lock_guard<std::mutex> lock(psql_mutex_);  // ← REACQUIRES psql_mutex_!
    for (const auto& [name, info] : functions_) {
        if (info.function_id == object_id) return name;
    }
    return "<unknown>";
}
```

### Why The Fix Should Work (In Theory)

**Lock sequence:**
1. `dropFunction()` acquires `psql_mutex_`
2. `getDependents()` acquires `dependency_cache_mutex_` (briefly, then releases)
3. `lock.unlock()` releases `psql_mutex_`
4. `resolveDependencyNames()` calls `getObjectName()`
5. `getObjectName(ObjectType::FUNCTION)` acquires `psql_mutex_` ← Should work, we unlocked it!

**In a single-threaded scenario, this should not deadlock.**

### Why It's STILL Deadlocking

**Hypothesis 1: Cross-Thread Deadlock**

There may be another thread acquiring locks in reverse order.

**Possible culprit threads:**
- Long transaction monitor (background thread)
- Garbage collector (background thread)
- Sweep manager (background thread)
- Another test operation (if tests run concurrently)

**Deadlock scenario:**

**Thread 1 (Test):**
```
1. Acquire psql_mutex_
2. Call getDependents() → acquire dependency_cache_mutex_ → release
3. Unlock psql_mutex_
4. Call resolveDependencyNames()
   4a. Call getObjectName()
   4b. Try to acquire psql_mutex_ ← BLOCKS if Thread 2 holds it
```

**Thread 2 (Background):**
```
1. Acquire dependency_cache_mutex_ (for some reason)
2. Need to access catalog → try to acquire psql_mutex_ ← BLOCKS if Thread 1 is at step 1-2
```

**Classic deadlock:**
- Thread 1 holds/wants: psql_mutex_ (step 4b)
- Thread 2 holds/wants: dependency_cache_mutex_ → psql_mutex_

**BUT** - Thread 1 released `psql_mutex_` at step 3 before step 4, so this shouldn't happen...

**UNLESS** - Thread 2 acquired `dependency_cache_mutex_` BETWEEN step 3 and step 4a, AND Thread 2 is holding it while trying to get `psql_mutex_`.

**Hypothesis 2: Nested Lock in getDependents()**

The original issue was that `getDependents()` is called WHILE `psql_mutex_` is held:

```cpp
// In dropFunction():
std::unique_lock<std::mutex> lock(psql_mutex_);  // ← HELD

// ... find function ...

// Check for dependents
std::vector<DependencyInfo> dependents;
Status status = getDependents(func.function_id, dependents, ctx);  // ← psql_mutex_ STILL HELD
if (status != Status::OK) {
    return status;  // ← If this returns, psql_mutex_ released by destructor
}
```

**Lock order in Thread 1:**
```
psql_mutex_ → dependency_cache_mutex_ (in getDependents)
```

**If another thread has:**
```
dependency_cache_mutex_ → psql_mutex_
```

**Then:** Classic lock-order deadlock.

**Hypothesis 3: Recursive Dependency Resolution**

If a function depends on another function, and `resolveDependencyNames()` is called:
1. Resolves first function name (acquires `psql_mutex_`)
2. Releases `psql_mutex_`
3. Resolves second function name (acquires `psql_mutex_` again)
4. ...

This should work with `std::mutex` (non-recursive), because each `getObjectName()` call acquires and releases the lock.

**UNLESS** - there's contention and another thread sneaks in between acquisitions.

**Hypothesis 4: Infinite Loop**

Could there be an infinite loop in:
- `resolveDependencyNames()` iterating over dependencies?
- `getObjectName()` searching for the function?

**Evidence against:** The loop in `resolveDependencyNames()` is bounded by `deps.size()`, and `getObjectName()` just searches a map.

---

## Root Cause: Lock Ordering Violation

**The REAL problem:** `getDependents()` is called WHILE `psql_mutex_` is held.

```cpp
std::unique_lock<std::mutex> lock(psql_mutex_);  // ← Line 11580

// ...

Status status = getDependents(func.function_id, dependents, ctx);  // ← Line 11599, psql_mutex_ HELD
```

**`getDependents()` implementation (line 17759):**

```cpp
auto CatalogManager::getDependents(const ID& object_id,
                                  std::vector<DependencyInfo>& dependents_out,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);  // ← Acquires WHILE psql_mutex_ held

    // ... iterate dependency_cache_ ...

    return Status::OK;
}
```

**Lock acquisition order in Thread 1 (dropFunction):**
```
1. psql_mutex_
2. dependency_cache_mutex_ (in getDependents)
```

**If ANY other code path acquires in reverse order:**
```
1. dependency_cache_mutex_
2. psql_mutex_
```

**Result:** Deadlock.

---

## Recommended Fix

### Option A: Acquire Both Locks Upfront (Most Reliable)

```cpp
auto CatalogManager::dropFunction(const std::string &name, bool if_exists,
                                  ErrorContext *ctx) -> Status
{
    // Acquire BOTH locks in consistent order FIRST
    std::scoped_lock lock(psql_mutex_, dependency_cache_mutex_);

    auto it = functions_.find(name);
    if (it == functions_.end()) {
        // ... handle not found ...
    }

    const FunctionInfo& func = it->second;

    // Check for dependents - call INTERNAL version that doesn't lock
    std::vector<DependencyInfo> dependents;
    getDependentsInternal(func.function_id, dependents);  // ← No locking, assumes held

    if (!dependents.empty()) {
        // Resolve names - call INTERNAL version
        std::vector<DependencyName> resolved;
        resolveDependencyNamesInternal(dependents, resolved, ctx);  // ← No locking

        std::string error_msg = buildDependencyErrorMessage(
            name, ObjectType::FUNCTION, resolved);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // ... rest of function ...
}
```

**Create internal helpers:**

```cpp
// Private helper, assumes dependency_cache_mutex_ held
void CatalogManager::getDependentsInternal(const ID& object_id,
                                           std::vector<DependencyInfo>& dependents_out)
{
    // NO LOCK - caller must hold dependency_cache_mutex_

    dependents_out.clear();
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.referenced_object_id == object_id) {
            dependents_out.push_back(dep_info);
        }
    }
}

// Public API (acquires lock)
auto CatalogManager::getDependents(const ID& object_id,
                                  std::vector<DependencyInfo>& dependents_out,
                                  ErrorContext* ctx) -> Status
{
    std::lock_guard<std::mutex> lock(dependency_cache_mutex_);
    getDependentsInternal(object_id, dependents_out);
    return Status::OK;
}

// Private helper for name resolution, assumes psql_mutex_ held
void CatalogManager::resolveDependencyNamesInternal(
    const std::vector<DependencyInfo>& deps,
    std::vector<DependencyName>& names_out,
    ErrorContext* ctx)
{
    // NO LOCK - caller must hold psql_mutex_

    names_out.clear();
    names_out.reserve(deps.size());

    for (const auto& dep : deps) {
        DependencyName resolved;
        resolved.dependent_type = dep.dependent_type;
        resolved.dependent_name = getObjectNameInternal(dep.dependent_object_id,
                                                       dep.dependent_type,
                                                       ctx);  // ← Internal version
        names_out.push_back(std::move(resolved));
    }
}

// Private helper, assumes appropriate lock held based on type
std::string CatalogManager::getObjectNameInternal(const ID& object_id,
                                                  ObjectType type,
                                                  ErrorContext* ctx)
{
    // NO LOCK - caller must hold appropriate lock for the object type

    switch (type) {
        case ObjectType::FUNCTION: {
            // NO LOCK - assumes psql_mutex_ held
            for (const auto& [name, info] : functions_) {
                if (info.function_id == object_id) return name;
            }
            return "<unknown>";
        }
        // ... similar for other types ...
    }
}
```

**Pros:**
- Eliminates all deadlock scenarios
- Clear lock ownership
- Consistent lock ordering

**Cons:**
- Requires creating internal helper versions of several functions
- More code changes
- Hold both locks for entire operation (increased lock contention)

**Estimated effort:** 3-4 hours

### Option B: Use Try-Lock with Backoff (Not Recommended)

Use `try_lock` to detect deadlock and retry.

**Cons:**
- Doesn't solve the problem, just makes it less likely
- Complex error handling
- Poor performance

---

## Next Steps

1. **Implement Option A** (internal helpers with consistent lock ordering)
2. **Search for other lock-order violations:**
   ```bash
   # Find all places where dependency_cache_mutex_ is acquired
   grep -rn "lock.*dependency_cache_mutex_" src/core/

   # Find all places where psql_mutex_ is acquired
   grep -rn "lock.*psql_mutex_" src/core/
   ```
3. **Document lock ordering requirements** in code comments
4. **Add thread safety annotations** (if using Clang Thread Safety Analysis)
5. **Run tests** to verify fix

---

## Priority

**Severity:** 🔴 CRITICAL
**Blocking:** YES - blocks all testing and development
**Estimated Fix Time:** 3-4 hours for Option A

---

## Resolution - 2025-12-30

**Status:** ✅ FIXED

### Implementation

Option A was successfully implemented:

1. Created internal helper functions that don't acquire locks:
   - `getDependentsInternal()`
   - `clearDependenciesForInternal()`
   - `getObjectNameInternal()`
   - `resolveDependencyNamesInternal()`

2. Modified `dropFunction()`, `dropProcedure()`, and `dropTable()` to:
   - Acquire BOTH locks upfront using `std::scoped_lock(psql_mutex_, dependency_cache_mutex_)`
   - Use internal helper versions throughout
   - No lock re-acquisition

### Test Results

**After Fix:**
```bash
ctest -R "StoredCodeDependencyTest\.(DropFunctionFailsIfCalledByAnotherFunction|...)" --timeout 60

100% tests passed, 0 tests failed out of 4
Total Test time (real) = 0.08 sec
```

**All 4 tests now pass in < 0.1 seconds** (previously: 60+ second timeout)

### Details

See complete fix documentation: `/docs/findings/DEADLOCK_FIX_2025_12_30.md`

---

**Analysis By:** Claude Code
**Date:** 2025-12-30
**Resolution Date:** 2025-12-30
**Time to Fix:** ~3 hours
