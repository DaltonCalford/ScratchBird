# Alpha 1 - Medium Priority Issues (P2) Implementation Plan

**Created:** November 23, 2025
**Priority:** P2 - MEDIUM
**Estimated Effort:** 100-150 hours
**Target:** Beta 2
**Dependencies:** P0 and P1 items should be complete

---

## OVERVIEW

This plan covers 25 medium-priority issues focused on performance optimizations, feature completeness, testing improvements, and code quality. These items enhance the system but are not critical for Beta 1.

**Execution Strategy:** Split into 4 parallel work streams:
- **Agent A:** Performance Optimizations (P2-1 through P2-5) - 27-33 hours
- **Agent B:** Feature Completeness (P2-6 through P2-10) - 73-100 hours
- **Agent C:** Testing & Quality (P2-11 through P2-14) - 39-50 hours
- **Agent D:** Code Quality (P2-15 through P2-17) - 16-22 hours

---

## AGENT A: PERFORMANCE OPTIMIZATIONS

**Total Effort:** 27-33 hours

### P2-1: Page Table Lock Partitioning (8-10 hours)
- **Current:** Single mutex for entire buffer pool page table
- **Improvement:** Partition into N buckets (e.g., 64) with separate locks
- **Impact:** Reduced lock contention under high concurrency

### P2-2: Dirty Page Counter (2-3 hours)
- **Current:** O(N) scan to count dirty pages
- **Improvement:** Atomic counter updated on flag changes
- **Impact:** O(1) dirty page count for background writer

### P2-3: TOAST Chunk Prefetching (6-8 hours)
- **Current:** Sequential chunk reads (many random I/Os)
- **Improvement:** Batch read all chunks in single range scan
- **Impact:** 5-10x faster for large TOAST values

### P2-4: Permission Cache TTL Reduction (1 hour)
- **Current:** 60-second TTL creates race window
- **Improvement:** Reduce to 5-10 seconds
- **Impact:** Smaller TOCTOU window

### P2-5: Hash Index Directory Resize (10-12 hours)
- **Current:** Directory expansion blocks all writes
- **Improvement:** Concurrent resize with fine-grained locking
- **Impact:** No write stalls during resize

---

## AGENT B: FEATURE COMPLETENESS

**Total Effort:** 73-100 hours

### P2-6: GENERATED Columns (25-30 hours)
**Missing:**
- STORED variant (computed once, stored physically)
- VIRTUAL variant (computed on SELECT)
- Dependency tracking for column references
- Expression validation
- Update triggers for dependent columns

### P2-7: Deferred Constraints (20-25 hours)
**Missing:**
- DEFERRABLE support
- INITIALLY DEFERRED/IMMEDIATE
- Deferred check accumulation per transaction
- Commit-time validation
- SET CONSTRAINTS statement

### P2-8: Statement-Level Triggers (20-25 hours)
**Missing:**
- FOR EACH STATEMENT firing (not just FOR EACH ROW)
- Transition table support (OLD TABLE, NEW TABLE)
- REFERENCING clause
- Statement-level WHEN conditions

### P2-9: Window Function Frames (20-30 hours)
**Missing:**
- ROWS BETWEEN ... AND ...
- RANGE BETWEEN ... AND ...
- GROUPS BETWEEN ... AND ...
- LAG/LEAD offset handling
- Frame boundary calculations

### P2-10: Statistical Aggregate Functions (8-10 hours)
**Current:** Stubs only
**Needed:**
- Proper mean accumulation (Welford's algorithm)
- Sum of squares calculation
- Variance/stddev calculation
- Correlation calculation
- Linear regression aggregates

---

## AGENT C: TESTING & QUALITY

**Total Effort:** 39-50 hours

### P2-11: Edge Case Test Suite (8-10 hours)
**Coverage:**
- NaN handling in all mathematical functions
- Infinity handling in comparisons
- Integer overflow/underflow
- Float precision loss
- Character encoding edge cases (UTF-8, invalid sequences)
- Timezone edge cases
- Leap seconds, daylight saving time

### P2-12: Concurrent Transaction Tests (10-12 hours)
**Coverage:**
- Concurrent INSERT/UPDATE/DELETE
- Snapshot isolation verification
- Deadlock detection and resolution
- Lock escalation behavior
- Read-write conflicts
- Write-write conflicts

### P2-13: Performance Benchmark Suite (15-20 hours)
**Benchmarks:**
- TPC-H queries (all 22 queries)
- TPC-C transaction processing
- Index creation time (various sizes)
- Transaction throughput (TPS measurements)
- Aggregate performance (GROUP BY, window functions)
- Join performance (nested loop, hash, merge)

### P2-14: Constraint Enforcement Tests (6-8 hours)
**Coverage:**
- CHECK constraint violations (all data types)
- FK constraint violations (INSERT, UPDATE, DELETE)
- UNIQUE constraint violations (single, composite)
- NOT NULL constraint violations
- IDENTITY column behavior (GENERATED ALWAYS vs BY DEFAULT)
- Deferred constraint checking

---

## AGENT D: CODE QUALITY

**Total Effort:** 16-22 hours

### P2-15: Role Cycle Detection (4-6 hours)
**Issue:** No cycle detection in role grants (A → B → C → A)
**Fix:**
```cpp
Status validateRoleGrant(ID granter_role, ID grantee_role, ErrorContext* ctx) {
    // DFS-based cycle detection
    std::unordered_set<ID> visited;
    std::unordered_set<ID> recursion_stack;

    if (hasCycle(grantee_role, granter_role, visited, recursion_stack)) {
        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
            "Role grant would create a cycle");
        return Status::INVALID_ARGUMENT;
    }

    return Status::OK;
}
```

### P2-16: Policy Expression Validation (4-6 hours)
**Issue:** RLS policies validated at execution time only
**Fix:** Validate at CREATE POLICY time:
- Parse policy expression
- Verify column references exist
- Check data type compatibility
- Validate function calls

### P2-17: Error Message Context (8-10 hours)
**Improvement:**
- Include constraint names in violation messages
- Add violating values to error context
- Provide suggested fixes
- Include line numbers for PSQL errors
- Show query fragment causing error

**Example:**
```
Before: "Foreign key constraint violated"
After:  "Foreign key constraint 'fk_orders_customer_id' violated:
         no parent row found for customer_id = 12345 in table 'customers'"
```

---

## ADDITIONAL P2 ITEMS (Brief Summaries)

### P2-18: Materialized View Refresh Strategies (15-20 hours)
- Incremental refresh (only changed rows)
- Concurrent refresh (non-blocking)
- Refresh dependencies (cascade)

### P2-19: Query Result Caching (12-15 hours)
- Cache SELECT results
- Invalidate on table modifications
- LRU eviction policy

### P2-20: Parallel Query Execution (30-40 hours)
- Parallel sequential scans
- Parallel aggregates
- Parallel joins
- Worker pool management

### P2-21: Prepared Statement Cache (8-10 hours)
- Parse once, execute many
- Parameter binding
- Cache eviction (LRU)

### P2-22: Connection Pooling (10-12 hours)
- Reuse connections
- Pool size limits
- Idle timeout
- Connection validation

### P2-23: Backup/Restore Improvements (15-20 hours)
- Incremental backups
- Parallel backup/restore
- Compression
- Point-in-time recovery

### P2-24: Query Planner Statistics (20-25 hours)
- Cost estimation
- Cardinality estimation
- Selectivity estimation
- Histogram-based statistics

### P2-25: Index Advisor (15-20 hours)
- Analyze query patterns
- Suggest missing indexes
- Identify unused indexes
- Cost/benefit analysis

---

## EXECUTION TIMELINE

**Total Duration:** 12-15 weeks with 4 agents in parallel

### Weeks 1-2
- Agent A: P2-2, P2-4, Start P2-1
- Agent B: P2-10, Start P2-6
- Agent C: P2-11
- Agent D: P2-15, P2-16

### Weeks 3-6
- Agent A: P2-1, P2-3
- Agent B: P2-6 (complete)
- Agent C: P2-12, P2-14
- Agent D: P2-17

### Weeks 7-10
- Agent A: P2-5
- Agent B: P2-7, P2-8
- Agent C: P2-13

### Weeks 11-15
- Agent B: P2-9 (complete)
- All: Integration testing, bug fixes

---

## COMPLETION CRITERIA

- ✅ All unit tests passing
- ✅ Performance benchmarks show improvement
- ✅ No regressions in existing functionality
- ✅ Code coverage > 80% for new code
- ✅ All optimizations validated with benchmarks

---

**Document Status:** READY FOR IMPLEMENTATION
**Last Updated:** November 23, 2025
