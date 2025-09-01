# Phase 15: Aggregation and Grouping

## Objective
Implement aggregate functions and GROUP BY.

## Prerequisites
- Phase 14 complete (joins)

## Tasks

### 15.1 Aggregate Functions
```cpp
class AggregateFunction {
    virtual void add_value(Value v) = 0;
    virtual Value get_result() = 0;
    virtual void reset() = 0;
};
```

Functions to implement:
- COUNT(*)
- COUNT(column)
- SUM(column)
- AVG(column)
- MIN(column)
- MAX(column)

### 15.2 GROUP BY Processing
- Hash-based grouping
- Sort-based grouping
- Multiple grouping columns

### 15.3 HAVING Clause
- Filter groups after aggregation
- Access to aggregate results
- Cannot reference non-grouped columns

### 15.4 DISTINCT Processing
- DISTINCT in SELECT
- COUNT(DISTINCT column)
- Multiple DISTINCT aggregates

### 15.5 NULL Handling
- Aggregates ignore NULL values
- COUNT(*) includes NULL rows
- GROUP BY treats NULL as a group

## Files to Create/Modify
- `include/scratchbird/engine/aggregate.h`
- `src/engine/aggregate.cpp`
- `src/engine/group_by.cpp`

## Validation Tests
```cpp
// Basic aggregation
auto result = execute("SELECT COUNT(*), SUM(amount) FROM orders");
assert(result.rows.size() == 1);

// GROUP BY
result = execute(
    "SELECT user_id, COUNT(*), SUM(amount) "
    "FROM orders GROUP BY user_id"
);

// HAVING
result = execute(
    "SELECT user_id, COUNT(*) as cnt "
    "FROM orders GROUP BY user_id "
    "HAVING COUNT(*) > 5"
);

// DISTINCT
result = execute("SELECT COUNT(DISTINCT user_id) FROM orders");

// NULL handling
execute("INSERT INTO orders VALUES (100, NULL, 50.0)");
result = execute("SELECT COUNT(*), COUNT(user_id) FROM orders");
// COUNT(*) includes NULL row, COUNT(user_id) doesn't
```

## Exit Criteria
- All aggregate functions work
- GROUP BY produces correct groups
- HAVING filters groups correctly
- NULL values handled properly