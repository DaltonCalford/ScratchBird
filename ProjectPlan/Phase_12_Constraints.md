# Phase 12: Constraints and Integrity

## Objective
Implement database constraints for data integrity.

## Prerequisites
- Phase 11 complete (indexing)

## Tasks

### 12.1 Constraint Types
```sql
NOT NULL
PRIMARY KEY
FOREIGN KEY REFERENCES table(column)
CHECK (expression)
DEFAULT value
```

### 12.2 Constraint Storage
Add to catalog:
```sql
SDB$CONSTRAINTS (constraint_id, relation_id, type, definition)
```

### 12.3 Constraint Enforcement
- Check on INSERT
- Check on UPDATE
- Cascade/restrict for foreign keys
- Deferred constraint checking

### 12.4 Foreign Key Actions
```sql
ON DELETE CASCADE
ON DELETE RESTRICT
ON DELETE SET NULL
ON UPDATE CASCADE
ON UPDATE RESTRICT
```

### 12.5 Check Constraints
- Parse and store check expressions
- Evaluate on row modifications
- Support column and table-level checks

## Files to Create/Modify
- `include/scratchbird/engine/constraints.h`
- `src/engine/constraint_manager.cpp`

## Validation Tests
```cpp
// NOT NULL
execute("CREATE TABLE test (id INTEGER NOT NULL)");
auto result = execute("INSERT INTO test VALUES (NULL)");
assert(result.status == StatusCode::NotNullViolation);

// PRIMARY KEY
execute("CREATE TABLE users (id INTEGER PRIMARY KEY)");
execute("INSERT INTO users VALUES (1)");
result = execute("INSERT INTO users VALUES (1)");
assert(result.status == StatusCode::PrimaryKeyViolation);

// FOREIGN KEY
execute("CREATE TABLE orders (id INTEGER, user_id INTEGER, "
        "FOREIGN KEY (user_id) REFERENCES users(id))");
result = execute("INSERT INTO orders VALUES (1, 999)");
assert(result.status == StatusCode::ForeignKeyViolation);

// CHECK constraint
execute("CREATE TABLE products (price REAL CHECK (price > 0))");
result = execute("INSERT INTO products VALUES (-10)");
assert(result.status == StatusCode::CheckViolation);
```

## Exit Criteria
- All constraint types enforced
- Appropriate error messages
- Foreign key actions work correctly