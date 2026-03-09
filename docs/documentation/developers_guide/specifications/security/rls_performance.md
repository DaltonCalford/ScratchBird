# Specification: RLS Performance

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | security/rls/performance |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Security Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/sblr/executor.cpp` (RLS predicate application)
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp` (Policy caching)
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/integration/test_security_phase3_5_rls_dml.cpp`

## Synopsis

This specification defines Row-Level Security performance optimization strategies, including predicate pushdown, policy caching, query plan integration, and performance monitoring.

## Scope

### In Scope

- RLS predicate pushdown
- Policy expression caching
- Query plan integration
- Performance monitoring and statistics
- Optimization strategies
- Bulk operation handling

### Out of Scope

- RLS policy syntax (see `rls_policy_syntax.md`)
- RLS enforcement semantics (see `rls_policy_enforcement.md`)
- General query optimization

## Background

RLS can significantly impact query performance if not optimized. The key strategies are: predicate pushdown to allow index usage, policy caching to avoid recomputation, and efficient expression evaluation.

## Specification

### Performance Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Query Processing                         │
├─────────────────────────────────────────────────────────────┤
│  ┌──────────────┐                                           │
│  │ SQL Parser   │                                           │
│  └──────┬───────┘                                           │
│         ▼                                                   │
│  ┌──────────────┐     ┌─────────────────┐                   │
│  │ Query Plan   │────►│ RLS Integration │                   │
│  │ Generator    │     │ (add predicates)│                   │
│  └──────┬───────┘     └────────┬────────┘                   │
│         ▼                      │                            │
│  ┌──────────────┐              ▼                            │
│  │ Optimizer    │────►┌─────────────────┐                   │
│  │              │     │ Predicate Push  │                   │
│  └──────┬───────┘     │ Down to Scans   │                   │
│         │             └─────────────────┘                   │
│         ▼                                                   │
│  ┌──────────────┐     ┌─────────────────┐                   │
│  │ Executor     │────►│ RLS Cache       │                   │
│  │              │     │ (reuse policies)│                   │
│  └──────────────┘     └─────────────────┘                   │
└─────────────────────────────────────────────────────────────┘
```

### RLS Predicate Structure

```cpp
// RLS Predicate Cache Entry
struct RlsPredicateCache {
    ID table_id;
    ID user_id;
    std::vector<ID> role_ids;
    CommandType command;
    
    // Cached predicate
    ExprNode* predicate_expr;
    std::string predicate_sql;
    
    // Cache metadata
    uint64_t policy_epoch;
    std::chrono::steady_clock::time_point cached_at;
    uint64_t use_count;
};
```

### Predicate Pushdown

RLS predicates are pushed down to table scans when possible:

```
Original Query:
SELECT * FROM orders WHERE status = 'pending'

With RLS:
SELECT * FROM orders 
WHERE status = 'pending' 
  AND (user_id = current_user_id())  -- RLS predicate

Pushed Down:
Index Scan on orders
  Index Cond: (status = 'pending')
  Filter: (user_id = current_user_id())  -- RLS predicate
```

**Pushdown Rules:**

| Query Type | Pushdown Strategy | Index Usage |
|------------|-------------------|-------------|
| Simple SELECT | Push to scan filter | Partial |
| SELECT + WHERE | Merge with WHERE | Full if compatible |
| JOIN | Push to each table | Per-table |
| Subquery | Push into subquery | If possible |
| UNION | Push to each branch | Per-branch |
| Aggregate | Push before aggregate | Pre-filter |

### Policy Caching

```cpp
// Policy cache in RlsEnforcer
class RlsPolicyCache {
public:
    // Lookup cached predicate
    std::optional<ExprNode*> lookup(
        ID table_id,
        ID user_id,
        const std::vector<ID>& role_ids,
        CommandType command,
        uint64_t current_epoch
    );
    
    // Insert predicate into cache
    void insert(const RlsPredicateCache& entry);
    
    // Invalidate on policy change
    void invalidateTable(ID table_id);
    void invalidatePolicy(ID policy_id);
    void invalidateUser(ID user_id);
    
private:
    std::unordered_map<CacheKey, RlsPredicateCache, CacheKeyHash> cache_;
    mutable std::shared_mutex mutex_;
    size_t max_entries_ = 1000;
    std::chrono::seconds ttl_{60};
};
```

**Cache Key:**
```cpp
struct CacheKey {
    ID table_id;
    ID user_id;
    std::vector<ID> role_ids;  // Sorted
    CommandType command;
    
    bool operator==(const CacheKey& other) const;
};
```

### Policy Epoch for Cache Invalidation

```cpp
// Global and per-table policy epochs
class PolicyEpochManager {
public:
    // Get current epochs
    uint64_t getGlobalEpoch() const;
    uint64_t getTableEpoch(ID table_id) const;
    
    // Increment epochs on changes
    void incrementGlobalEpoch();  // On CREATE/DROP POLICY
    void incrementTableEpoch(ID table_id);  // On ALTER POLICY
    
private:
    std::atomic<uint64_t> global_epoch_{1};
    std::unordered_map<ID, uint64_t> table_epochs_;
    mutable std::shared_mutex mutex_;
};
```

### Query Plan Integration

```
Algorithm: Integrate RLS into Query Plan

Input: query_plan, rls_context
Output: Modified plan with RLS predicates

1. FOR EACH scan_node in query_plan.table_scans:
     table_id = scan_node.table_id
     
     IF NOT table_has_rls_enabled(table_id):
       continue
     
     IF user_has_bypassrls(rls_context.user_id):
       continue
     
     // Get or generate predicate
     predicate = getRlsPredicate(table_id, SELECT, rls_context)
     
     IF predicate is not empty:
       // Add to scan filter
       scan_node.filter = AND(scan_node.filter, predicate)
       scan_node.has_rls_filter = true

2. FOR EACH dml_node in query_plan.dml_nodes:
     table_id = dml_node.table_id
     
     IF NOT table_has_rls_enabled(table_id):
       continue
     
     // Get USING predicate (for UPDATE/DELETE target selection)
     IF dml_node.command IN (UPDATE, DELETE):
       using_pred = getRlsPredicate(table_id, dml_node.command, rls_context)
       IF using_pred is not empty:
         dml_node.using_filter = using_pred
     
     // Get WITH CHECK predicate (for INSERT/UPDATE value validation)
     IF dml_node.command IN (INSERT, UPDATE):
       check_pred = getWithCheckPredicate(table_id, dml_node.command, rls_context)
       IF check_pred is not empty:
         dml_node.with_check = check_pred

3. RETURN modified_plan
```

### Optimization Strategies

#### 1. Static Predicate Extraction

If a policy expression doesn't depend on user context, extract and optimize:

```sql
-- Static predicate
CREATE POLICY active_only ON orders
    USING (is_active = true);

-- Can be optimized to constant filter
```

#### 2. Index-Aware Predicate Generation

Generate predicates that can use indexes:

```sql
-- Good: Can use index on user_id
CREATE POLICY user_isolation ON orders
    USING (user_id = current_user_id());

-- Less optimal: Function on column prevents index
CREATE POLICY user_isolation ON orders
    USING (hash(user_id) = hash(current_user_id()));
```

#### 3. Partition Pruning

For partitioned tables, combine RLS with partition pruning:

```sql
-- Partitioned by tenant_id, RLS by tenant_id
CREATE POLICY tenant_isolation ON orders
    USING (tenant_id = current_setting('app.tenant_id')::int);

-- Query planner can:
-- 1. Apply RLS predicate
-- 2. Prune to single partition
-- 3. Use partition-local index
```

#### 4. Bulk Operation Optimization

For bulk operations, batch RLS checks:

```cpp
// Instead of checking each row individually
for (row : rows) {
    checkRls(row);  // Expensive per-row
}

// Group by RLS predicate satisfaction
Map<predicate_result, rows> groups = groupByPredicate(rows);
for (group : groups) {
    if (group.predicate_satisfied) {
        processBatch(group.rows);
    } else {
        rejectBatch(group.rows);
    }
}
```

### Performance Monitoring

```cpp
// RLS Performance Statistics
struct RlsPerformanceStats {
    // Policy evaluation
    uint64_t policies_evaluated = 0;
    uint64_t policies_cached = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    
    // Predicate generation
    uint64_t predicates_generated = 0;
    std::chrono::microseconds total_generation_time{0};
    
    // Query impact
    uint64_t queries_with_rls = 0;
    uint64_t rows_filtered_by_rls = 0;
    
    // Cache statistics
    double getCacheHitRate() const {
        if (cache_hits + cache_misses == 0) return 0.0;
        return static_cast<double>(cache_hits) / (cache_hits + cache_misses);
    }
};
```

### EXPLAIN Integration

```sql
-- Show RLS in query plan
EXPLAIN (ANALYZE, VERBOSE) SELECT * FROM orders;

-- Output includes:
-- "Seq Scan on public.orders"
-- "  Filter: (user_id = current_user_id())"  -- RLS filter
-- "  Rows Removed by Filter: 10000"
```

### Best Practices

| Practice | Impact | Recommendation |
|----------|--------|----------------|
| Index RLS columns | High | Create indexes on columns in USING clauses |
| Avoid functions on columns | High | Use column = value, not function(column) |
| Limit policies per table | Medium | Fewer policies = faster evaluation |
| Use RESTRICTIVE sparingly | Low | RESTRICTIVE adds AND complexity |
| Cache warm-up | Medium | Pre-warm cache for hot tables |
| Monitor cache hit rate | Medium | Target > 95% cache hit rate |

### Performance Tuning

```sql
-- 1. Create index on RLS columns
CREATE INDEX idx_orders_user_id ON orders(user_id);

-- 2. Use simple equality predicates
CREATE POLICY good ON orders USING (user_id = current_user_id());
-- Avoid: USING (extract(year from created_at) = extract(year from now()))

-- 3. Consider expression indexes for common RLS predicates
CREATE INDEX idx_orders_tenant ON orders(
    (current_setting('app.tenant_id')::int)
);

-- 4. Partition by RLS key when possible
CREATE TABLE orders PARTITION BY LIST (tenant_id);

-- 5. Regularly analyze tables for accurate statistics
ANALYZE orders;
```

### Benchmarks

Expected performance characteristics:

| Operation | With RLS | Without RLS | Overhead |
|-----------|----------|-------------|----------|
| Simple SELECT | 1.1x | 1.0x | 10% |
| SELECT + Index | 1.05x | 1.0x | 5% |
| Complex JOIN | 1.2x | 1.0x | 20% |
| INSERT | 1.15x | 1.0x | 15% |
| UPDATE | 1.25x | 1.0x | 25% |
| DELETE | 1.2x | 1.0x | 20% |

*Overhead decreases with proper indexing and caching*

## Invariants

1. **Cache Consistency**: Cache entries invalidated on policy change
   - Verification: Epoch-based validation

2. **Predicate Correctness**: Pushed predicates equivalent to original
   - Verification: Expression equivalence check

3. **Index Usage**: Prefer index-friendly predicates
   - Verification: Query planner statistics

## Related Specifications

- `rls_policy_enforcement.md` - Policy enforcement
- `rls_policy_syntax.md` - Policy syntax
- `indexes/index_btree.md` - Index usage

## Appendix

### Troubleshooting Performance Issues

```sql
-- Check if RLS is using indexes
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM orders;

-- Look for:
-- - "Seq Scan" when should be "Index Scan"
-- - High "Rows Removed by Filter"

-- Check RLS cache statistics
SELECT * FROM pg_stat_rls_cache;

-- Check policy evaluation count
SELECT tablename, policyname, calls, total_time
FROM pg_stat_rls_policies;
```

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
