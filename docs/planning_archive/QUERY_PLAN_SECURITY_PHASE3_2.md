# Query Plan Security Integration - Phase 3.2

**Status**: Planning → Implementation
**Priority**: High (10-100x performance improvement)
**Estimated Time**: 40-60 hours
**Date**: November 11, 2025

---

## Overview

Phase 3.2 integrates security permission checking directly into the query planner, achieving **10-100x performance improvement** over executor-level checks by:

1. Filtering inaccessible tables/columns at plan time
2. Integrating permission checks with index selection
3. Caching permission results across query planning
4. Eliminating redundant permission checks during execution

---

## Current State (Phase 2)

### Executor-Level Permission Checks

```cpp
// Current approach (Phase 2): Check permissions in executor
ExecutionResult Executor::executeSelect() {
    // 1. Start execution
    // 2. Read rows from table
    // 3. FOR EACH ROW:
    //      - Check if user has SELECT permission ❌ SLOW
    //      - Filter row if no permission
    // 4. Return results
}
```

**Problems**:
- Permission check per row (**O(N)** where N = row count)
- Permission check after data is read (wasted I/O)
- No integration with index selection
- Can't optimize based on permissions

**Example**: SELECT * FROM employees (1M rows)
- 1,000,000 permission checks
- If user lacks permission: All 1M rows read then discarded
- Total time: **10+ seconds**

---

## Target State (Phase 3.2)

### Planner-Level Permission Checks

```cpp
// New approach (Phase 3.2): Check permissions in planner
std::shared_ptr<PlanNode> QueryPlanner::planQuery(...) {
    // 1. Check table-level SELECT permission ONCE ✅ FAST
    // 2. If no permission: Return error immediately
    // 3. Filter columns based on column-level permissions
    // 4. Generate plan with only accessible columns
    // 5. Executor runs without permission checks
}
```

**Benefits**:
- Permission check once per table (**O(1)**)
- Early rejection before any I/O
- Index selection aware of permissions
- Can optimize based on permissions

**Example**: SELECT * FROM employees (1M rows)
- 1 permission check (table level)
- If no permission: Immediate error (no I/O)
- If permission: Executor runs without checks
- Total time: **<100ms** (100x faster!)

---

## Design

### 1. Permission Cache in Query Planner

```cpp
class QueryPlanner {
private:
    // Permission cache for current query
    struct PermissionCache {
        std::unordered_map<ID, bool> table_select;     // table_id -> has SELECT
        std::unordered_map<ID, bool> table_insert;     // table_id -> has INSERT
        std::unordered_map<ID, bool> table_update;     // table_id -> has UPDATE
        std::unordered_map<ID, bool> table_delete;     // table_id -> has DELETE

        // Column-level permissions (Phase 3.3)
        std::unordered_map<ID, std::unordered_set<std::string>> column_select;

        // Clear cache between queries
        void clear();
    };

    PermissionCache perm_cache_;
    core::ConnectionContext* conn_ctx_;  // User context
};
```

### 2. Permission Check Integration Points

#### A. Table Access Permission (Phase 3.2)

```cpp
auto QueryPlanner::planQuery(const SelectStmt* select_stmt, ...) {
    // 1. Resolve table
    TableInfo table_info;
    catalog_->getTable(schema_id, table_name, table_info, ctx);

    // 2. CHECK PERMISSION (NEW - Phase 3.2)
    if (!checkTablePermission(table_info.table_id, Privilege::SELECT)) {
        SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED,
                         "Permission denied for table: " + table_name);
        return nullptr;  // Early rejection - no I/O wasted!
    }

    // 3. Generate paths (only if permission granted)
    auto seq_scan_path = createSeqScanPath(table_info);
    auto index_paths = createIndexScanPaths(table_info, where_clause);

    // 4. Cost and select best path
    auto best_path = selectCheapestPath(paths);

    // 5. Convert to plan
    return convertToPlan(best_path);
}
```

#### B. Column Filtering (Phase 3.3 - Future)

```cpp
auto QueryPlanner::resolveSelectList(const SelectStmt* stmt, ...) {
    std::vector<ColumnInfo> accessible_columns;

    for (auto* col_ref : stmt->select_list) {
        // Check column-level permission
        if (checkColumnPermission(table_id, col_ref->column_name, Privilege::SELECT)) {
            accessible_columns.push_back(resolveColumn(col_ref));
        } else {
            // Silently skip (or error based on policy)
            LOG_WARN(SECURITY, "Column %s filtered by permissions", col_ref->column_name);
        }
    }

    return accessible_columns;
}
```

#### C. Index Selection with Permissions

```cpp
std::vector<std::shared_ptr<Path>> QueryPlanner::createIndexScanPaths(...) {
    std::vector<std::shared_ptr<Path>> paths;

    // Get all indexes on table
    std::vector<IndexInfo> indexes;
    catalog_->listIndexesForTable(table_id, indexes, ctx);

    for (const auto& index_info : indexes) {
        // Check if user can use this index
        // (may require SELECT on indexed columns - Phase 3.3)
        if (canUseIndex(index_info)) {
            auto index_path = createIndexScanPath(table_info, index_info, quals);
            paths.push_back(index_path);
        }
    }

    return paths;
}
```

---

## Implementation Plan

### Phase 3.2.1: Basic Table-Level Checks (12-15 hours)

**Goal**: Move SELECT permission check from executor to planner

**Tasks**:
1. Add `ConnectionContext*` to QueryPlanner constructor (1 hour)
2. Implement `PermissionCache` structure (2 hours)
3. Add `checkTablePermission()` method (3 hours)
4. Integrate permission check in `planQuery()` (2 hours)
5. Remove redundant executor permission checks (2 hours)
6. Add integration tests (2 hours)

**Files to Modify**:
- `include/scratchbird/optimizer/query_planner.h`
- `src/optimizer/query_planner.cpp`
- `src/sblr/executor.cpp` (remove old checks)
- `tests/integration/test_security_plan.cpp` (new file)

**Expected Performance Improvement**: 10-50x for SELECT queries

### Phase 3.2.2: DML Permission Checks (8-10 hours)

**Goal**: Move INSERT/UPDATE/DELETE checks to planner

**Tasks**:
1. Add DML permission checking to planner (4 hours)
2. Integrate with INSERT/UPDATE/DELETE planning (2 hours)
3. Update executor to skip permission checks (1 hour)
4. Add tests for DML operations (1 hour)

**Expected Performance Improvement**: 5-20x for DML queries

### Phase 3.2.3: Permission Cache Optimization (10-12 hours)

**Goal**: Optimize permission lookups with caching

**Tasks**:
1. Implement LRU cache for permission results (4 hours)
2. Add cache invalidation on GRANT/REVOKE (3 hours)
3. Add cache statistics and monitoring (2 hours)
4. Performance benchmarking (1 hour)

**Expected Performance Improvement**: Additional 2-5x

### Phase 3.2.4: Index-Aware Permission Checks (8-10 hours)

**Goal**: Integrate permissions with index selection

**Tasks**:
1. Check indexed column permissions (3 hours)
2. Filter index paths based on permissions (2 hours)
3. Adjust cost estimates for permission overhead (2 hours)
4. Add tests for index selection with permissions (1 hour)

**Expected Performance Improvement**: Better index selection

---

## Performance Analysis

### Before (Phase 2 - Executor Checks)

| Query Type | Rows | Checks | Time | I/O |
|------------|------|--------|------|-----|
| SELECT * FROM t | 1M | 1M | 10s | 1M rows read |
| SELECT * FROM t WHERE id=1 | 1 | 1 | 50ms | 1 row read + check |
| INSERT INTO t VALUES(...) | 1 | 1 | 10ms | Write + check |

**Total overhead**: O(N) permission checks

### After (Phase 3.2 - Planner Checks)

| Query Type | Rows | Checks | Time | I/O |
|------------|------|--------|------|-----|
| SELECT * FROM t | 1M | 1 | 100ms | 1M rows read (no checks!) |
| SELECT * FROM t WHERE id=1 | 1 | 1 | 5ms | 1 row read (no check!) |
| INSERT INTO t VALUES(...) | 1 | 1 | 2ms | Write (no check!) |

**Total overhead**: O(1) permission checks

**Speedup**: 10-100x depending on query

---

## API Changes

### QueryPlanner Constructor

```cpp
// OLD (current)
QueryPlanner(Database* db, const CostModel& cost_model, StatisticsManager* stats);

// NEW (Phase 3.2)
QueryPlanner(Database* db, const CostModel& cost_model, StatisticsManager* stats,
            ConnectionContext* conn_ctx);
```

### New Methods

```cpp
class QueryPlanner {
private:
    // Check if user has permission on table
    bool checkTablePermission(const ID& table_id, Privilege privilege);

    // Check if user can use index (based on indexed columns)
    bool canUseIndex(const IndexInfo& index_info);

    // Get cached permission (or query if not cached)
    bool getCachedPermission(const ID& object_id, PermissionObjectType type,
                            Privilege privilege);

    // Clear permission cache (called between queries)
    void clearPermissionCache();
};
```

---

## Error Handling

### Permission Denied Errors

```cpp
// Planner-level permission denial
if (!checkTablePermission(table_id, Privilege::SELECT)) {
    SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED,
                     "Permission denied for table: " + table_name);
    return nullptr;  // Return null plan
}
```

**User-visible error**:
```
ERROR: Permission denied for table: employees
HINT: User 'alice' lacks SELECT privilege on table 'public.employees'
```

### Superuser Bypass

```cpp
bool QueryPlanner::checkTablePermission(const ID& table_id, Privilege privilege) {
    // Superusers bypass all permission checks
    if (conn_ctx_ && conn_ctx_->isSuperuser()) {
        return true;
    }

    // Regular permission check
    return getCachedPermission(table_id, PermissionObjectType::TABLE, privilege);
}
```

---

## Testing Strategy

### Unit Tests

1. **Permission Cache Tests**:
   - Cache hit/miss behavior
   - Cache invalidation
   - Concurrent access

2. **Permission Check Tests**:
   - Superuser bypass
   - Regular user checks
   - Role-based permissions

3. **Index Selection Tests**:
   - Index filtering based on permissions
   - Cost adjustments

### Integration Tests

1. **End-to-End SELECT Tests**:
   ```sql
   -- Setup
   CREATE USER alice;
   CREATE TABLE employees (id INT, name VARCHAR, salary DECIMAL);
   INSERT INTO employees VALUES (1, 'Bob', 50000), ...;

   -- Test without permission
   SET SESSION AUTHORIZATION alice;
   SELECT * FROM employees;  -- Should fail at plan time

   -- Grant permission
   RESET SESSION AUTHORIZATION;
   GRANT SELECT ON TABLE employees TO alice;

   -- Test with permission
   SET SESSION AUTHORIZATION alice;
   SELECT * FROM employees;  -- Should succeed
   ```

2. **Performance Benchmarks**:
   ```cpp
   // Measure planning + execution time
   auto start = std::chrono::high_resolution_clock::now();

   // Plan query
   auto plan = planner->planQuery(select_stmt, string_pool, &ctx);

   // Execute query
   auto result = executor->execute(plan);

   auto end = std::chrono::high_resolution_clock::now();
   auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

   // Verify < 100ms for 1M rows with permission
   ASSERT_LT(duration.count(), 100);
   ```

3. **DML Tests**:
   ```sql
   -- Test INSERT permission at plan time
   INSERT INTO employees VALUES (999, 'Charlie', 60000);  -- Fail if no INSERT perm

   -- Test UPDATE permission at plan time
   UPDATE employees SET salary = 55000 WHERE id = 1;  -- Fail if no UPDATE perm

   -- Test DELETE permission at plan time
   DELETE FROM employees WHERE id = 1;  -- Fail if no DELETE perm
   ```

---

## Backward Compatibility

### Migration Path

1. **Phase 2 → Phase 3.2 Transition**:
   - Executor checks remain as fallback (defensive programming)
   - Planner checks added as primary mechanism
   - Both run initially for validation
   - Executor checks removed once stable

2. **Configuration Option**:
   ```sql
   -- Allow gradual rollout
   SET security.plan_time_checks = 'enabled';  -- Default in Phase 3.2
   SET security.plan_time_checks = 'disabled'; -- Fallback to Phase 2 behavior
   ```

---

## Success Criteria

✅ **Performance**:
- [ ] 10x speedup for 100K+ row SELECT queries
- [ ] <100ms planning time for complex queries
- [ ] <10% overhead for single-row queries

✅ **Correctness**:
- [ ] All permission checks pass integration tests
- [ ] No permission bypass vulnerabilities
- [ ] Superuser bypass works correctly

✅ **Code Quality**:
- [ ] Clean separation of concerns
- [ ] Comprehensive test coverage
- [ ] Clear error messages

---

## Next Steps

1. Implement Phase 3.2.1 (table-level checks)
2. Performance benchmark
3. Implement Phase 3.2.2 (DML checks)
4. Implement Phase 3.2.3 (caching)
5. Implement Phase 3.2.4 (index awareness)

---

**Status**: Ready to implement
**Start Date**: November 11, 2025
**Target Completion**: Phase 3.2.1 in current session
