# Phase 13: Query Optimization

## Objective
Implement cost-based query optimizer.

## Prerequisites
- Phase 12 complete (constraints)

## Technical Specifications
- **Complete Query Optimizer**: See `/references/technical_specifications/QUERY_OPTIMIZER_SPEC.md`

## Tasks

### 13.1 Statistics Collection
```cpp
struct TableStats {
    size_t row_count;
    size_t page_count;
    map<ColumnId, ColumnStats> column_stats;
};

struct ColumnStats {
    size_t distinct_values;
    Value min_value;
    Value max_value;
    double null_fraction;
};
```

### 13.2 Cost Model
```cpp
struct PlanCost {
    double startup_cost;
    double total_cost;
    size_t estimated_rows;
};
```

### 13.3 Plan Generation
- Generate alternative plans
- Estimate cost for each plan
- Choose lowest cost plan

### 13.4 Join Ordering
- Consider different join orders
- Use dynamic programming for small queries
- Heuristics for large queries

### 13.5 EXPLAIN Command
```sql
EXPLAIN SELECT * FROM users WHERE id = 1;
EXPLAIN ANALYZE SELECT * FROM users;  -- With execution stats
```

## Files to Create/Modify
- `include/scratchbird/engine/optimizer.h`
- `src/engine/optimizer.cpp`
- `src/engine/statistics.cpp`

## Validation Tests
```cpp
// Collect statistics
execute("ANALYZE users");

// Verify index chosen for selective query
auto plan = explain("SELECT * FROM users WHERE id = 1");
assert(plan.contains("Index Scan"));

// Verify table scan for non-selective query
plan = explain("SELECT * FROM users WHERE active = true");
assert(plan.contains("Seq Scan"));

// Join ordering
execute("CREATE TABLE small (id INTEGER)");
execute("CREATE TABLE large (id INTEGER, small_id INTEGER)");
// Insert 10 rows in small, 10000 in large
plan = explain("SELECT * FROM large JOIN small ON large.small_id = small.id");
assert(plan.contains("Hash Join"));  // Small table as hash table
```

## Exit Criteria
- Statistics collected and used
- Better plans chosen based on cost
- EXPLAIN shows query plans