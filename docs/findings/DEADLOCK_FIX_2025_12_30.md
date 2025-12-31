# Deadlock Fix - 2025-12-30

**Date:** 2025-12-30
**Status:** ✅ RESOLVED
**Tests Affected:** All 4 StoredCodeDependencyTest timeouts
**Fix Duration:** ~3 hours

---

## Summary

Fixed critical deadlock in `CatalogManager::dropFunction()` and `CatalogManager::dropProcedure()` that caused 4 tests to timeout after 60 seconds.

**Result:** All 4 tests now pass in < 0.1 seconds (was: 60+ second timeout)

---

## Root Cause

Lock ordering violation in catalog_manager.cpp:

### The Problem

**dropFunction() (line 11577) was:**
1. Acquiring `psql_mutex_` (line 11580)
2. Calling `getDependents()` which acquires `dependency_cache_mutex_` (line 11599)
3. Unlocking `psql_mutex_` (line 11607)
4. Calling `resolveDependencyNames()` → `getObjectName()` → tries to re-acquire `psql_mutex_` (line 18228)

**Deadlock scenario:**
- **Thread 1 (test):** Holds `psql_mutex_` → wants `dependency_cache_mutex_` → releases `psql_mutex_` → wants `psql_mutex_` again
- **Thread 2 (background):** Holds `dependency_cache_mutex_` → wants `psql_mutex_`
- **Result:** Classic lock-order deadlock

---

## The Fix

Implemented **Option A** from TEST_TIMEOUT_REANALYSIS_2025_12_30.md: Acquire both locks upfront with consistent ordering.

### Changes Made

#### 1. Added Internal Helper Declarations (catalog_manager.h:3350-3364)

```cpp
private:
    // Internal helpers (assume locks already held by caller)
    // These functions do NOT acquire locks - caller must hold appropriate mutexes
    void getDependentsInternal(const ID& object_id,
                               std::vector<DependencyInfo>& dependents_out);

    void clearDependenciesForInternal(const ID& dependent_object_id,
                                     ErrorContext* ctx);

    auto getObjectNameInternal(const ID& object_id, ObjectType type,
                               ErrorContext* ctx) -> std::string;

    void resolveDependencyNamesInternal(const std::vector<DependencyInfo>& deps,
                                       std::vector<DependencyName>& names_out,
                                       ErrorContext* ctx);
```

#### 2. Implemented Internal Helpers (catalog_manager.cpp)

**getDependentsInternal() (line 17808):**
```cpp
void CatalogManager::getDependentsInternal(const ID& object_id,
                                            std::vector<DependencyInfo>& dependents_out)
{
    // NO LOCK - caller must hold dependency_cache_mutex_
    dependents_out.clear();

    // Find all dependencies where this object is referenced
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.referenced_object_id == object_id) {
            dependents_out.push_back(dep_info);
        }
    }
}
```

**clearDependenciesForInternal() (line 17940):**
```cpp
void CatalogManager::clearDependenciesForInternal(const ID& dependent_object_id,
                                                   ErrorContext* ctx)
{
    // NO LOCK - caller must hold dependency_cache_mutex_

    std::vector<ID> to_delete;
    for (const auto& [dep_id, dep_info] : dependency_cache_) {
        if (dep_info.dependent_object_id == dependent_object_id) {
            to_delete.push_back(dep_id);
        }
    }

    for (const auto& dep_id : to_delete) {
        dependency_cache_.erase(dep_id);
        for (auto it = object_to_dependencies_.begin(); it != object_to_dependencies_.end(); ) {
            if (it->second == dep_id) {
                it = object_to_dependencies_.erase(it);
            } else {
                ++it;
            }
        }
        deleteDependencyRecord(dep_id, ctx);
    }
}
```

**getObjectNameInternal() (line 18390):**
- 250-line function (same as getObjectName but WITHOUT any lock_guard statements)
- Assumes caller holds appropriate mutex for each object type

**resolveDependencyNamesInternal() (line 18713):**
```cpp
void CatalogManager::resolveDependencyNamesInternal(const std::vector<DependencyInfo>& deps,
                                                     std::vector<DependencyName>& names_out,
                                                     ErrorContext* ctx)
{
    // NO LOCK - caller must hold appropriate mutexes for the object types being queried
    names_out.clear();
    names_out.reserve(deps.size());

    for (const auto& dep : deps) {
        DependencyName resolved;
        resolved.dependent_type = dep.dependent_type;
        resolved.dependent_name = getObjectNameInternal(dep.dependent_object_id,
                                                        dep.dependent_type,
                                                        ctx);
        names_out.push_back(std::move(resolved));
    }
}
```

#### 3. Fixed dropFunction() (catalog_manager.cpp:11577)

**Before:**
```cpp
std::unique_lock<std::mutex> lock(psql_mutex_);
// ...
Status status = getDependents(func.function_id, dependents, ctx);  // ← Acquires dependency_cache_mutex_
// ...
if (!dependents.empty()) {
    lock.unlock();  // ← Release psql_mutex_
    resolveDependencyNames(dependents, resolved, ctx);  // ← Tries to re-acquire psql_mutex_
    // ...
}
// ...
clearDependenciesFor(func.function_id, ctx);  // ← Acquires dependency_cache_mutex_ again
```

**After:**
```cpp
// Acquire BOTH locks in consistent order to prevent deadlock
// Lock order: psql_mutex_ first, then dependency_cache_mutex_
std::scoped_lock lock(psql_mutex_, dependency_cache_mutex_);

// ...
// Use internal version that assumes locks already held
getDependentsInternal(func.function_id, dependents);

// Functions don't own other objects, so all dependents are blocking
if (!dependents.empty()) {
    std::vector<DependencyName> resolved;
    // Use internal version that assumes locks already held
    resolveDependencyNamesInternal(dependents, resolved, ctx);
    // ...
}

// ...
// Use internal version that assumes locks already held
clearDependenciesForInternal(func.function_id, ctx);
```

**Key changes:**
1. `std::unique_lock` → `std::scoped_lock(psql_mutex_, dependency_cache_mutex_)`
2. Hold BOTH locks for entire function duration
3. No lock.unlock() calls
4. Use internal helpers that don't acquire locks

#### 4. Fixed dropProcedure() (catalog_manager.cpp:11643)

Applied identical fix pattern as dropFunction().

#### 5. Fixed dropTable() (catalog_manager.cpp:11787)

Applied same fix:
- Changed `std::lock_guard<std::mutex> lock(mutex_)` to `std::scoped_lock lock(mutex_, dependency_cache_mutex_)`
- Changed `getDependents()` → `getDependentsInternal()`
- Changed `resolveDependencyNames()` → `resolveDependencyNamesInternal()`
- Changed `clearDependenciesFor()` → `clearDependenciesForInternal()`
- Added `Status status;` declaration (was previously returned by getDependents())

---

## Testing Results

### Before Fix
```bash
ctest -R "StoredCodeDependencyTest\.(DropFunctionFailsIfCalledByAnotherFunction|...)" --timeout 60

Result: 0% tests passed, 4 tests failed out of 4
All 4 tests: ***Timeout 60 seconds
```

### After Fix
```bash
ctest -R "StoredCodeDependencyTest\.(DropFunctionFailsIfCalledByAnotherFunction|...)" --timeout 60

Test #224: StoredCodeDependencyTest.DropFunctionFailsIfCalledByAnotherFunction ... Passed 0.02 sec
Test #226: StoredCodeDependencyTest.DropProcedureFailsIfCalled ................... Passed 0.01 sec
Test #229: StoredCodeDependencyTest.ComplexFunctionChain ......................... Passed 0.01 sec
Test #231: StoredCodeDependencyTest.MixedFunctionProcedureDependencies ........... Passed 0.02 sec

100% tests passed, 0 tests failed out of 4
Total Test time (real) = 0.08 sec
```

**Improvement:** From 60+ second timeout to 0.08 seconds total ✅

---

## Why This Fix Works

### Lock Consistency
- **Always acquire locks in the same order:** `psql_mutex_` first, then `dependency_cache_mutex_`
- **Hold both locks for entire operation:** No interleaving, no race conditions
- **No lock re-acquisition:** Internal helpers assume locks already held

### Eliminates Deadlock Scenarios

**Before (deadlock possible):**
```
Thread 1: psql_mutex_ → dependency_cache_mutex_ → unlock psql_mutex_ → psql_mutex_ (DEADLOCK)
Thread 2: dependency_cache_mutex_ → psql_mutex_ (DEADLOCK)
```

**After (no deadlock):**
```
Thread 1: psql_mutex_ + dependency_cache_mutex_ (acquired together)
Thread 2: psql_mutex_ + dependency_cache_mutex_ (waits for Thread 1 to release BOTH)
```

---

## Trade-offs

### Pros
✅ Eliminates all deadlock scenarios
✅ Clear lock ownership and ordering
✅ Thread-safe by design
✅ Consistent pattern across dropFunction/dropProcedure/dropTable

### Cons
⚠️ Holds both locks longer (increased lock contention)
⚠️ More code (internal helper versions)
⚠️ Must maintain two versions of several functions

**Verdict:** Correctness > performance in this case. Lock contention is acceptable trade-off for guaranteed deadlock-free operation.

---

## Files Modified

1. **include/scratchbird/core/catalog_manager.h**
   - Added 4 internal helper declarations (lines 3350-3364)

2. **src/core/catalog_manager.cpp**
   - `getDependentsInternal()` - line 17808
   - `clearDependenciesForInternal()` - line 17940
   - `getObjectNameInternal()` - line 18390
   - `resolveDependencyNamesInternal()` - line 18713
   - `dropFunction()` - line 11577 (fixed)
   - `dropProcedure()` - line 11643 (fixed)
   - `dropTable()` - line 11787 (fixed)

---

## Related Documentation

- **Original Analysis:** `/docs/findings/TEST_TIMEOUT_ANALYSIS_2025_12_29.md`
- **Re-Analysis:** `/docs/findings/TEST_TIMEOUT_REANALYSIS_2025_12_30.md`
- **Test Isolation Plan:** `/docs/planning/TEST_ISOLATION_AND_CATEGORIZATION_PLAN.md`

---

## Next Steps

1. ✅ **Fix verified** - All 4 tests pass
2. 🔲 **Run full test suite** - Verify no regressions
3. 🔲 **Update quarantine list** - Remove these 4 tests from quarantine in TEST_ISOLATION plan
4. 🔲 **Code review** - Review lock ordering across entire CatalogManager
5. 🔲 **Document lock policy** - Add lock ordering rules to catalog_manager.h comments

---

**Status:** ✅ RESOLVED
**Fix Verified:** 2025-12-30
**Tests Passing:** 4/4 (100%)
**Execution Time:** < 0.1 seconds (was: 60+ second timeout)

---

**Fixed By:** Claude Code
**Fix Approach:** Option A (Internal helpers with consistent lock ordering)
**Estimated Effort:** 3 hours (actual)
