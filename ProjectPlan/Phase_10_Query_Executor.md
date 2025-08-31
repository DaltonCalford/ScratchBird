# Phase 10: Query Executor

## Objective
Execute parsed SQL statements against the database.

## Prerequisites
- Phase 9 complete (SQL parser)

## Tasks

### 10.1 Execution Engine
```cpp
class Executor {
    Result execute_create_table(CreateTableNode*);
    Result execute_insert(InsertNode*);
    Result execute_select(SelectNode*);
    Result execute_update(UpdateNode*);
    Result execute_delete(DeleteNode*);
};
```

### 10.2 Table Scan
- Sequential scan through heap pages
- Apply WHERE predicates
- Return matching tuples

### 10.3 Expression Evaluation
Evaluate expressions:
- Column references
- Constants
- Comparisons (=, <, >, etc.)
- AND/OR logic

### 10.4 Result Sets
```cpp
struct Result {
    vector<string> column_names;
    vector<vector<Value>> rows;
    size_t affected_rows;
    StatusCode status;
};
```

### 10.5 Type Conversion
- Implicit conversions where safe
- Error on incompatible types
- NULL handling in expressions

## Files to Create/Modify
- `include/scratchbird/engine/executor.h`
- `src/engine/executor.cpp`
- `src/engine/expression.cpp`

## Validation Tests
```cpp
// Execute CREATE and INSERT
execute("CREATE TABLE test (id INTEGER, name TEXT)");
execute("INSERT INTO test VALUES (1, 'Alice')");

// Execute SELECT
auto result = execute("SELECT * FROM test WHERE id = 1");
assert(result.rows.size() == 1);
assert(result.rows[0][1] == "Alice");

// Execute UPDATE
execute("UPDATE test SET name = 'Bob' WHERE id = 1");
result = execute("SELECT name FROM test WHERE id = 1");
assert(result.rows[0][0] == "Bob");

// Execute DELETE
execute("DELETE FROM test WHERE id = 1");
result = execute("SELECT * FROM test");
assert(result.rows.size() == 0);
```

## Exit Criteria
- All basic SQL operations work
- WHERE clauses filter correctly
- Results returned in expected format