# Phase 14: Join Operations

## Objective
Implement various join algorithms.

## Prerequisites
- Phase 13 complete (query optimization)

## Tasks

### 14.1 Nested Loop Join
```cpp
class NestedLoopJoin {
    Result execute(Relation* outer, Relation* inner, JoinPredicate pred);
};
```

### 14.2 Hash Join
```cpp
class HashJoin {
    Result execute(Relation* build, Relation* probe, JoinPredicate pred);
};
```

### 14.3 Sort-Merge Join
```cpp
class SortMergeJoin {
    Result execute(Relation* left, Relation* right, JoinPredicate pred);
};
```

### 14.4 Join Types
- INNER JOIN
- LEFT OUTER JOIN
- RIGHT OUTER JOIN
- FULL OUTER JOIN
- CROSS JOIN

### 14.5 Join Predicates
- Equi-joins (a.id = b.id)
- Non-equi joins (a.val < b.val)
- Multiple join conditions
- NULL handling in joins

## Files to Create/Modify
- `include/scratchbird/engine/join.h`
- `src/engine/nested_loop_join.cpp`
- `src/engine/hash_join.cpp`
- `src/engine/sort_merge_join.cpp`

## Validation Tests
```cpp
// Setup tables
execute("CREATE TABLE users (id INTEGER, name TEXT)");
execute("CREATE TABLE orders (id INTEGER, user_id INTEGER, amount REAL)");

// Inner join
auto result = execute(
    "SELECT u.name, o.amount "
    "FROM users u INNER JOIN orders o ON u.id = o.user_id"
);

// Left join
result = execute(
    "SELECT u.name, o.amount "
    "FROM users u LEFT JOIN orders o ON u.id = o.user_id"
);
// Should include users with no orders (NULL amount)

// Multiple joins
execute("CREATE TABLE products (id INTEGER, name TEXT)");
execute("CREATE TABLE order_items (order_id INTEGER, product_id INTEGER)");
result = execute(
    "SELECT u.name, p.name "
    "FROM users u "
    "JOIN orders o ON u.id = o.user_id "
    "JOIN order_items oi ON o.id = oi.order_id "
    "JOIN products p ON oi.product_id = p.id"
);
```

## Exit Criteria
- All join types work correctly
- Optimizer chooses appropriate join algorithm
- NULL values handled properly in outer joins