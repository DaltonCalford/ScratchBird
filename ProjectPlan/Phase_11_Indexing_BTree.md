# Phase 11: B-Tree Indexing

## Objective
Implement B-Tree indexes for efficient data access.

## Prerequisites
- Phase 10 complete (query executor)

## Tasks

### 11.1 B-Tree Structure
```cpp
struct BTreeNode {
    bool is_leaf;
    vector<Key> keys;
    vector<PageNo> children;  // For internal nodes
    vector<RowId> values;      // For leaf nodes
    PageNo next_leaf;          // For range scans
};
```

### 11.2 Index Operations
```cpp
class BTreeIndex {
    void insert(Key key, RowId rid);
    bool remove(Key key);
    RowId find(Key key);
    vector<RowId> range_scan(Key start, Key end);
};
```

### 11.3 Index Creation
```sql
CREATE INDEX idx_name ON table(column);
DROP INDEX idx_name;
```

### 11.4 Index Scan Executor
- Modify executor to use indexes
- Choose between table scan and index scan
- Support index-only scans when possible

### 11.5 Unique Constraints
- Support UNIQUE indexes
- Enforce uniqueness on insert/update
- Return constraint violation errors

## Files to Create/Modify
- `include/scratchbird/engine/btree.h`
- `src/engine/btree.cpp`
- `src/engine/index_scan.cpp`

## Validation Tests
```cpp
// Create index
execute("CREATE INDEX idx_users_id ON users(id)");

// Verify index used for point query
auto plan = explain("SELECT * FROM users WHERE id = 100");
assert(plan.contains("Index Scan"));

// Range scan
execute("SELECT * FROM users WHERE id BETWEEN 10 AND 20");

// Unique constraint
execute("CREATE UNIQUE INDEX idx_email ON users(email)");
auto result = execute("INSERT INTO users VALUES (1, 'test@example.com')");
auto result2 = execute("INSERT INTO users VALUES (2, 'test@example.com')");
assert(result2.status == StatusCode::UniqueViolation);
```

## Exit Criteria
- B-Tree operations work correctly
- Indexes improve query performance
- Unique constraints enforced