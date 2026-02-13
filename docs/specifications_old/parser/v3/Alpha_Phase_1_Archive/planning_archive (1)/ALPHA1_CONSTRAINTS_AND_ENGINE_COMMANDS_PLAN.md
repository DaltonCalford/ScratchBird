# Alpha 1: Constraint Features & SQL Engine Commands Implementation Plan

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


**Created:** November 21, 2025
**Status:** Not Started (~15% of Alpha 1 remaining)
**Priority:** MEDIUM
**Dependencies:** None

---

## Part A: Constraint Features

### Overview

Implement missing constraint features:
1. GENERATED columns (STORED/VIRTUAL)
2. IDENTITY columns (auto-increment)
3. Deferred constraint checking

---

## Feature A.1: GENERATED Columns

### Specification

**Syntax:**
```sql
CREATE TABLE employees (
    id INT PRIMARY KEY,
    first_name VARCHAR(50),
    last_name VARCHAR(50),
    full_name VARCHAR(100) GENERATED ALWAYS AS (first_name || ' ' || last_name) STORED,
    age INT,
    age_group VARCHAR(20) GENERATED ALWAYS AS (
        CASE
            WHEN age < 18 THEN 'Minor'
            WHEN age < 65 THEN 'Adult'
            ELSE 'Senior'
        END
    ) VIRTUAL
);
```

### Current Status

- ❌ No parser support
- ❌ No catalog support
- ❌ No executor support

### Types

**STORED Generated Columns:**
- Computed value stored physically in table
- Updated automatically when base columns change
- Can be indexed
- Takes disk space

**VIRTUAL Generated Columns:**
- Computed value NOT stored physically
- Calculated on-the-fly during SELECT
- Cannot be indexed (in most databases)
- No disk space

### Implementation Tasks

#### Task A.1.1: Catalog Extension

**File:** `include/scratchbird/core/catalog_manager.h`
**Estimated Lines:** ~30

**Extend `ColumnInfo` struct:**
```cpp
struct ColumnInfo {
    // ... existing fields ...

    // Generated column fields
    bool is_generated;                  // Is this a generated column?
    GeneratedColumnType gen_type;       // STORED or VIRTUAL
    std::string generation_expression;  // SQL expression (or serialized bytecode)
    uint64_t generation_expr_oid;       // TOAST OID for large expressions

    // Dependencies
    std::vector<uint16_t> dependent_columns;  // Columns this depends on
};

enum class GeneratedColumnType : uint8_t {
    NOT_GENERATED = 0,
    STORED = 1,
    VIRTUAL = 2
};
```

**Testing:**
- ColumnInfo serialization/deserialization
- Large generation expressions (TOAST)

---

#### Task A.1.2: Parser Extension

**File:** `src/parser/parser.cpp`
**Estimated Lines:** ~120

**Steps:**
1. Add `GENERATED ALWAYS AS` syntax to column definition
2. Parse `STORED` or `VIRTUAL` keyword
3. Parse generation expression (can be complex)
4. Store expression in AST node
5. Validate: GENERATED columns cannot be PRIMARY KEY

**Syntax Variations:**
```sql
-- Standard SQL
column_name data_type GENERATED ALWAYS AS (expression) [STORED | VIRTUAL]

-- PostgreSQL-style
column_name data_type GENERATED ALWAYS AS (expression) STORED

-- MySQL-style (default is VIRTUAL)
column_name data_type AS (expression) [STORED | VIRTUAL]
```

**Testing:**
- Simple GENERATED column
- Complex expression
- Multiple GENERATED columns
- GENERATED column with dependencies on other GENERATED columns

---

#### Task A.1.3: Executor Implementation - STORED

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~200

**INSERT Handling:**
```cpp
Status executeInsert(const InsertStmt* stmt, TransactionId xid) {
    // ... existing INSERT logic ...

    // For each GENERATED STORED column:
    for (const auto& col : table->columns) {
        if (col.is_generated && col.gen_type == STORED) {
            // Evaluate generation expression
            Value generated_value = evaluateExpression(col.generation_expression, row_values);

            // Store computed value
            row_values[col.column_id] = generated_value;
        }
    }

    // Continue with normal insert
    insertTuple(row_values, xid);
}
```

**UPDATE Handling:**
```cpp
Status executeUpdate(const UpdateStmt* stmt, TransactionId xid) {
    // ... existing UPDATE logic ...

    // Check if any dependent columns were updated
    for (const auto& col : table->columns) {
        if (col.is_generated && col.gen_type == STORED) {
            bool dependency_updated = false;
            for (uint16_t dep_col : col.dependent_columns) {
                if (updated_columns.contains(dep_col)) {
                    dependency_updated = true;
                    break;
                }
            }

            if (dependency_updated) {
                // Re-compute GENERATED column
                Value new_value = evaluateExpression(col.generation_expression, row_values);
                row_values[col.column_id] = new_value;
            }
        }
    }

    // Continue with normal update
    updateTuple(tid, row_values, xid);
}
```

**Testing:**
- INSERT with GENERATED STORED column
- UPDATE with GENERATED STORED column
- GENERATED column depending on other GENERATED column
- GENERATED column with NULL input

---

#### Task A.1.4: Executor Implementation - VIRTUAL

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~100

**SELECT Handling:**
```cpp
Status executeSelect(const SelectStmt* stmt, TransactionId xid, ResultSet& result) {
    // ... existing SELECT logic ...

    // For each row in result:
    for (auto& row : result.rows) {
        // Compute VIRTUAL generated columns
        for (const auto& col : selected_columns) {
            if (col.is_generated && col.gen_type == VIRTUAL) {
                Value computed_value = evaluateExpression(col.generation_expression, row);
                row[col.column_id] = computed_value;
            }
        }
    }

    return Status::OK;
}
```

**INSERT/UPDATE Handling:**
- Virtual columns are NOT stored
- Ignore any explicit values provided for VIRTUAL columns
- Only compute during SELECT

**Testing:**
- SELECT with VIRTUAL column
- INSERT ignoring VIRTUAL column value
- UPDATE ignoring VIRTUAL column
- Complex VIRTUAL expression

---

### Feature A.1 Completion Criteria

- [  ] Parser supports GENERATED syntax
- [  ] Catalog stores generation expressions
- [  ] STORED columns computed on INSERT/UPDATE
- [  ] VIRTUAL columns computed on SELECT
- [  ] Dependency tracking working
- [  ] All GENERATED column tests passing

---

## Feature A.2: IDENTITY Columns

### Specification

**Syntax:**
```sql
CREATE TABLE orders (
    order_id INT GENERATED ALWAYS AS IDENTITY,
    customer_id INT,
    order_date DATE
);

CREATE TABLE products (
    product_id INT GENERATED BY DEFAULT AS IDENTITY (START WITH 1000 INCREMENT BY 1),
    product_name VARCHAR(100)
);
```

### Current Status

- ❌ No parser support
- ❌ No catalog support
- ❌ No executor support

### Types

**GENERATED ALWAYS AS IDENTITY:**
- Database always generates value
- User cannot provide explicit value
- Throws error if user tries to insert value

**GENERATED BY DEFAULT AS IDENTITY:**
- Database generates value if user doesn't provide one
- User CAN provide explicit value
- More flexible

### Implementation Tasks

#### Task A.2.1: Catalog Extension

**File:** `include/scratchbird/core/catalog_manager.h`
**Estimated Lines:** ~40

**Extend `ColumnInfo` struct:**
```cpp
struct ColumnInfo {
    // ... existing fields ...

    // Identity column fields
    bool is_identity;                   // Is this an IDENTITY column?
    IdentityGenerationType identity_gen;  // ALWAYS or BY DEFAULT
    int64_t identity_start;             // START WITH value (default: 1)
    int64_t identity_increment;         // INCREMENT BY value (default: 1)
    int64_t identity_current;           // Current value (runtime, not persisted)
    uint64_t identity_sequence_id;      // Associated sequence ID
};

enum class IdentityGenerationType : uint8_t {
    NOT_IDENTITY = 0,
    ALWAYS = 1,
    BY_DEFAULT = 2
};
```

**Testing:**
- ColumnInfo with IDENTITY fields
- Persistence across restarts

---

#### Task A.2.2: Sequence Integration

**File:** `src/core/catalog_manager.cpp`
**Estimated Lines:** ~100

**Strategy:** IDENTITY columns are implemented using sequences

**Steps:**
1. When CREATE TABLE with IDENTITY column is executed:
   a. Create hidden sequence: `<table>_<column>_seq`
   b. Set START WITH and INCREMENT BY values
   c. Associate sequence with column (identity_sequence_id)

2. When INSERT occurs:
   a. Call `nextval(<sequence>)` to get next value
   b. Assign to IDENTITY column

**Implementation:**
```cpp
Status createIdentitySequence(const std::string& table_name,
                              const std::string& column_name,
                              int64_t start_value,
                              int64_t increment) {
    std::string seq_name = table_name + "_" + column_name + "_seq";

    SequenceInfo seq;
    seq.sequence_name = seq_name;
    seq.start_value = start_value;
    seq.increment_by = increment;
    seq.min_value = 1;
    seq.max_value = INT64_MAX;
    seq.cycle = false;

    return createSequence(seq);
}
```

**Testing:**
- Sequence creation for IDENTITY column
- Sequence cleanup on DROP TABLE
- Sequence value persistence

---

#### Task A.2.3: Parser Extension

**File:** `src/parser/parser.cpp`
**Estimated Lines:** ~100

**Syntax:**
```sql
GENERATED {ALWAYS | BY DEFAULT} AS IDENTITY [(sequence_options)]
```

**Steps:**
1. Add IDENTITY keyword
2. Parse ALWAYS or BY DEFAULT
3. Parse optional sequence options (START WITH, INCREMENT BY)
4. Store in AST node

**Testing:**
- GENERATED ALWAYS AS IDENTITY
- GENERATED BY DEFAULT AS IDENTITY
- Custom START WITH and INCREMENT BY
- Multiple IDENTITY columns (should error - only one per table)

---

#### Task A.2.4: Executor Implementation

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~150

**INSERT Handling:**
```cpp
Status executeInsert(const InsertStmt* stmt, TransactionId xid) {
    // ... existing INSERT logic ...

    for (const auto& col : table->columns) {
        if (col.is_identity) {
            if (col.identity_gen == ALWAYS) {
                // User cannot provide value
                if (user_provided_value_for_column(col.column_id)) {
                    return error("Cannot insert explicit value for GENERATED ALWAYS AS IDENTITY column");
                }
            }

            // Generate next value from sequence
            int64_t next_val = getNextSequenceValue(col.identity_sequence_id);
            row_values[col.column_id] = Value(next_val);
        }
    }

    insertTuple(row_values, xid);
}
```

**Testing:**
- INSERT without value (ALWAYS)
- INSERT with value (BY DEFAULT)
- INSERT with explicit value for ALWAYS (should error)
- Sequence increment verification

---

### Feature A.2 Completion Criteria

- [  ] Parser supports IDENTITY syntax
- [  ] Catalog integrates with sequences
- [  ] ALWAYS enforces no user values
- [  ] BY DEFAULT allows user values
- [  ] Sequence values persist across restarts
- [  ] All IDENTITY column tests passing

---

## Feature A.3: Deferred Constraint Checking

### Specification

**Syntax:**
```sql
CREATE TABLE parent (
    id INT PRIMARY KEY
);

CREATE TABLE child (
    id INT PRIMARY KEY,
    parent_id INT REFERENCES parent(id) DEFERRABLE INITIALLY DEFERRED
);

BEGIN;
    INSERT INTO child (id, parent_id) VALUES (1, 100);  -- Would normally violate FK
    INSERT INTO parent (id) VALUES (100);  -- Satisfies FK constraint
COMMIT;  -- Constraint checked here, transaction commits successfully
```

### Current Status

- ❌ No parser support (DEFERRABLE, INITIALLY DEFERRED)
- ❌ No executor support

### Why This Matters

- Circular foreign key references
- Complex multi-table inserts
- Data migration scenarios

### Implementation Tasks

#### Task A.3.1: Catalog Extension

**File:** `include/scratchbird/core/catalog_manager.h`
**Estimated Lines:** ~20

**Extend constraint structures:**
```cpp
struct ForeignKeyInfo {
    // ... existing fields ...

    bool is_deferrable;          // Can constraint checking be deferred?
    bool initially_deferred;     // Defer by default?
};

struct CheckConstraintInfo {
    // ... existing fields ...

    bool is_deferrable;
    bool initially_deferred;
};
```

**Testing:**
- Constraint info persistence

---

#### Task A.3.2: Parser Extension

**File:** `src/parser/parser.cpp`
**Estimated Lines:** ~80

**Syntax:**
```sql
[CONSTRAINT name] constraint_definition
    [[NOT] DEFERRABLE]
    [INITIALLY {DEFERRED | IMMEDIATE}]
```

**Steps:**
1. Add DEFERRABLE and INITIALLY keywords
2. Parse DEFERRABLE/NOT DEFERRABLE
3. Parse INITIALLY DEFERRED/IMMEDIATE
4. Store in constraint AST nodes

**Testing:**
- DEFERRABLE INITIALLY DEFERRED
- NOT DEFERRABLE
- DEFERRABLE INITIALLY IMMEDIATE

---

#### Task A.3.3: Executor Implementation

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~300

**Data Structures:**
```cpp
struct DeferredConstraintCheck {
    ConstraintType type;  // FK, CHECK, UNIQUE
    uint64_t constraint_id;
    TID affected_tid;
    std::vector<Value> values;
};

class DeferredConstraintManager {
    std::vector<DeferredConstraintCheck> deferred_checks;

    void addDeferredCheck(const DeferredConstraintCheck& check);
    Status performDeferredChecks(TransactionId xid);
    void clearDeferredChecks();
};
```

**Implementation:**
```cpp
Status executeInsert(const InsertStmt* stmt, TransactionId xid) {
    // ... existing INSERT logic ...

    // Check constraints
    for (const auto& constraint : table->constraints) {
        if (constraint.is_deferrable && constraint.initially_deferred) {
            // Defer constraint check until commit
            DeferredConstraintCheck check;
            check.type = constraint.type;
            check.constraint_id = constraint.id;
            check.affected_tid = inserted_tid;
            deferred_manager.addDeferredCheck(check);
        } else {
            // Immediate constraint check (current behavior)
            checkConstraint(constraint, inserted_tid);
        }
    }
}

Status commit(TransactionId xid) {
    // Perform all deferred constraint checks
    Status status = deferred_manager.performDeferredChecks(xid);
    if (status != Status::OK) {
        // Constraint violation, rollback transaction
        rollback(xid);
        return status;
    }

    // Proceed with commit
    // ...
}
```

**Testing:**
- Deferred FK check succeeds
- Deferred FK check fails (rollback)
- Multiple deferred constraints
- Mix of deferred and immediate constraints
- SET CONSTRAINTS (dynamic deferral)

---

### Feature A.3 Completion Criteria

- [  ] Parser supports DEFERRABLE syntax
- [  ] Catalog stores deferral settings
- [  ] Deferred checks accumulated during transaction
- [  ] Deferred checks executed at commit
- [  ] Transaction rolls back on deferred constraint violation
- [  ] All deferred constraint tests passing

---

## Part B: SQL Engine Internal Commands

### Overview

Implement SQL engine internal commands for introspection:
1. SHOW TABLES / SHOW DATABASES / SHOW COLUMNS
2. DESCRIBE TABLE
3. EXPLAIN (query plan visualization)
4. System catalog queries (pg_catalog equivalent)

---

## Feature B.1: SHOW Commands

### Specification

**Syntax:**
```sql
SHOW TABLES [FROM database] [LIKE 'pattern'];
SHOW DATABASES [LIKE 'pattern'];
SHOW COLUMNS FROM table [LIKE 'pattern'];
SHOW INDEXES FROM table;
SHOW CREATE TABLE table;
```

### Implementation Tasks

#### Task B.1.1: Parser Extension

**File:** `src/parser/parser.cpp`
**Estimated Lines:** ~150

**Steps:**
1. Add SHOW keyword
2. Implement `parseShowStatement()`
3. Parse object type (TABLES, DATABASES, COLUMNS, INDEXES)
4. Parse optional FROM clause
5. Parse optional LIKE pattern
6. Create ShowStmt AST node

**Testing:**
- SHOW TABLES
- SHOW DATABASES
- SHOW COLUMNS FROM users
- SHOW INDEXES FROM orders
- SHOW CREATE TABLE products

---

#### Task B.1.2: Executor Implementation

**File:** `src/sblr/executor.cpp`
**Estimated Lines:** ~300

**Implementation:**
```cpp
Status executeShow(const ShowStmt* stmt, ResultSet& result) {
    switch (stmt->object_type) {
        case SHOW_TABLES:
            return showTables(stmt->database_name, stmt->like_pattern, result);
        case SHOW_DATABASES:
            return showDatabases(stmt->like_pattern, result);
        case SHOW_COLUMNS:
            return showColumns(stmt->table_name, stmt->like_pattern, result);
        case SHOW_INDEXES:
            return showIndexes(stmt->table_name, result);
        case SHOW_CREATE_TABLE:
            return showCreateTable(stmt->table_name, result);
    }
}

Status showTables(const std::string& database, const std::string& pattern, ResultSet& result) {
    // Query catalog for tables
    std::vector<TableInfo> tables = catalog->getTables(database);

    // Apply LIKE filter if specified
    if (!pattern.empty()) {
        tables = filterByPattern(tables, pattern);
    }

    // Build result set
    result.columns = {"Table"};
    for (const auto& table : tables) {
        result.rows.push_back({Value(table.table_name)});
    }

    return Status::OK;
}
```

**Testing:**
- SHOW TABLES in specific database
- SHOW TABLES with LIKE pattern
- SHOW COLUMNS shows all column info
- SHOW CREATE TABLE reconstructs DDL

---

### Feature B.1 Completion Criteria

- [  ] All SHOW commands implemented
- [  ] LIKE pattern matching works
- [  ] Result sets formatted correctly
- [  ] All SHOW tests passing

---

## Feature B.2: DESCRIBE Command

### Specification

**Syntax:**
```sql
DESCRIBE table_name;
DESC table_name;  -- Alias
```

**Output:**
```
+----------+-------------+------+-----+---------+-------+
| Field    | Type        | Null | Key | Default | Extra |
+----------+-------------+------+-----+---------+-------+
| id       | INT         | NO   | PRI | NULL    |       |
| name     | VARCHAR(50) | YES  |     | NULL    |       |
| age      | INT         | YES  |     | NULL    |       |
+----------+-------------+------+-----+---------+-------+
```

### Implementation

**Parser:** Reuse SHOW COLUMNS logic
**Executor:** Format as table with Field, Type, Null, Key, Default, Extra columns

---

## Feature B.3: EXPLAIN Command

### Specification

**Syntax:**
```sql
EXPLAIN SELECT * FROM users WHERE age > 18;
EXPLAIN ANALYZE SELECT * FROM users WHERE age > 18;
```

### Implementation Tasks

#### Task B.3.1: Query Plan Formatter

**File:** `src/optimizer/query_planner.cpp`
**Estimated Lines:** ~400

**Requirements:**
1. Convert QueryPlan to human-readable format
2. Show operator tree (Seq Scan, Index Scan, Join, etc.)
3. Show estimated cost
4. Show estimated row count
5. Show index usage

**Output Format:**
```
Seq Scan on users  (cost=0.00..1000.00 rows=500 width=40)
  Filter: (age > 18)

Index Scan using users_age_idx on users  (cost=0.00..100.00 rows=50 width=40)
  Index Cond: (age > 18)

Hash Join  (cost=1000.00..5000.00 rows=1000 width=80)
  Hash Cond: (orders.user_id = users.id)
  ->  Seq Scan on orders  (cost=0.00..2000.00 rows=2000 width=40)
  ->  Hash  (cost=1000.00..1000.00 rows=500 width=40)
        ->  Seq Scan on users  (cost=0.00..1000.00 rows=500 width=40)
```

**Implementation:**
```cpp
std::string formatQueryPlan(const QueryPlan& plan, int indent_level) {
    std::ostringstream oss;

    // Format operator
    oss << std::string(indent_level * 2, ' ');
    oss << operatorName(plan.op_type);

    // Show table if applicable
    if (plan.table_name) {
        oss << " on " << *plan.table_name;
    }

    // Show cost and row estimates
    oss << "  (cost=" << plan.cost_estimate << " rows=" << plan.row_estimate << ")";

    // Show filters
    if (plan.filter) {
        oss << "\n" << std::string((indent_level + 1) * 2, ' ');
        oss << "Filter: " << formatExpression(plan.filter);
    }

    // Recursively format children
    for (const auto& child : plan.children) {
        oss << "\n" << formatQueryPlan(child, indent_level + 1);
    }

    return oss.str();
}
```

**Testing:**
- EXPLAIN simple SELECT
- EXPLAIN with JOIN
- EXPLAIN with index usage
- EXPLAIN ANALYZE (with actual execution)

---

### Feature B.3 Completion Criteria

- [  ] EXPLAIN shows query plan
- [  ] EXPLAIN ANALYZE executes and shows actual stats
- [  ] Plan format is readable
- [  ] All EXPLAIN tests passing

---

## Feature B.4: System Catalog Queries

### Specification

Provide standard views for introspection (similar to PostgreSQL's pg_catalog):

**Views:**
- `sb_tables` - All tables
- `sb_columns` - All columns
- `sb_indexes` - All indexes
- `sb_constraints` - All constraints
- `sb_sequences` - All sequences
- `sb_views` - All views
- `sb_users` - All users
- `sb_roles` - All roles

### Implementation

**File:** `src/core/catalog_manager.cpp`
**Estimated Lines:** ~200

**Strategy:** Create virtual views that query catalog tables

**Example:**
```sql
CREATE VIEW sb_tables AS
SELECT
    schema_name,
    table_name,
    table_type,
    created_time
FROM sys.tables;
```

**Testing:**
- Query each system view
- Filter system views
- Join system views

---

## Completion Criteria (All Features)

### Part A: Constraints
- [  ] GENERATED columns (STORED and VIRTUAL)
- [  ] IDENTITY columns (ALWAYS and BY DEFAULT)
- [  ] Deferred constraint checking

### Part B: SQL Commands
- [  ] SHOW commands (TABLES, DATABASES, COLUMNS, INDEXES)
- [  ] DESCRIBE command
- [  ] EXPLAIN command
- [  ] System catalog views

### Testing
- [  ] All unit tests passing
- [  ] All integration tests passing
- [  ] No memory leaks
- [  ] Documentation complete

---

## Estimated Effort

**Total Estimated Lines:** ~3,450 lines
**Estimated Time:** 90-110 hours
**Priority:** MEDIUM (required for Alpha 1 but less critical than PSQL/CTEs)

**Breakdown:**
- Part A (Constraints): 60 hours
- Part B (SQL Commands): 50 hours

---

## Dependencies

**Blocked By:** None
**Blocks:** Alpha 1 completion

---

**Last Updated:** November 21, 2025
**Next Review:** After Part A completion
