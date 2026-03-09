# Specification: Constraints

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | catalog |
| **Spec Version** | 1.0.0 |
| **Status** | 🔴 Draft |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird 0.1.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:911`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:922`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/core/catalog_manager.h:886`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/core/catalog_manager.cpp:5104`

## Synopsis

This specification defines constraint metadata storage, including primary keys, unique constraints, foreign keys, check constraints, and NOT NULL constraints. It covers the unified sb_constraints table and foreign key relationships.

## Scope

### In Scope

- Constraint types and their characteristics
- Unified constraint table (sb_constraints)
- Foreign key relationships (sb_foreign_keys)
- Primary key constraints
- Unique constraints
- CHECK constraints
- NOT NULL constraints
- Deferrable constraints
- Constraint states (enabled, validated)

### Out of Scope

- Index enforcement of constraints (see `indexes.md`)
- Constraint validation algorithms
- Trigger-based constraint enforcement

## Specification

### Constraint Types

**Source:** `include/scratchbird/core/catalog_manager.h:911`

```cpp
enum class ConstraintType : uint8_t {
    PRIMARY_KEY = 0,  // PRIMARY KEY constraint
    UNIQUE = 1,       // UNIQUE constraint
    CHECK = 2,        // CHECK constraint
    FOREIGN_KEY = 3,  // FOREIGN KEY constraint
    NOT_NULL = 4,     // NOT NULL constraint (column-level)
    EXCLUSION = 5     // EXCLUSION constraint (PostgreSQL extension)
};
```

**Constraint Characteristics:**

| Type | Columns | Auto-Index | Deferred | Multi-Table |
|------|---------|------------|----------|-------------|
| PRIMARY_KEY | 1-N | Yes | Optional | No |
| UNIQUE | 1-N | Yes | Optional | No |
| CHECK | 0 (expression) | No | Optional | No |
| FOREIGN_KEY | 1-N | No (uses parent PK) | Optional | Yes (2 tables) |
| NOT_NULL | 1 | No | No | No |
| EXCLUSION | 1-N | Yes | Optional | No |

### Foreign Key Actions

**Source:** `include/scratchbird/core/catalog_manager.h:868`

```cpp
enum class FKAction : uint8_t {
    NO_ACTION = 0,   // Default: error if references exist
    RESTRICT = 1,    // Error immediately if references exist
    CASCADE = 2,     // Delete/update child rows
    SET_NULL = 3,    // Set FK columns to NULL
    SET_DEFAULT = 4  // Set FK columns to DEFAULT
};
```

### Foreign Key Match Types

**Source:** `include/scratchbird/core/catalog_manager.h:878`

```cpp
enum class FKMatchType : uint8_t {
    SIMPLE = 0,   // Default: NULL in any column = no match required
    FULL = 1,     // All columns NULL or all non-NULL
    PARTIAL = 2   // Not implemented (reserved)
};
```

### ConstraintInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:922`

```cpp
struct ConstraintInfo {
    // Identity
    ID constraint_id;                   // UUIDv7 constraint identifier
    std::string constraint_name;        // Constraint name
    bool name_is_delimited = false;     // Quoted identifier flag
    ID table_id;                        // Table this constraint applies to
    ConstraintType constraint_type;     // Type of constraint
    
    // Owner
    ID owner_id;                        // User who created constraint
    
    // Column information (for PK, UNIQUE, NOT NULL, CHECK)
    std::vector<std::string> column_names;  // Columns involved
    
    // CHECK constraint specific
    std::string check_expression;       // CHECK constraint SQL expression
    ID check_expr_oid{};                // TOAST reference for large expressions
    
    // FOREIGN KEY specific
    ID referenced_table_id;             // For FK: parent table
    std::vector<std::string> referenced_columns;  // For FK: parent columns
    FKAction on_delete = FKAction::NO_ACTION;
    FKAction on_update = FKAction::NO_ACTION;
    FKMatchType match_type = FKMatchType::SIMPLE;
    
    // EXCLUSION constraint specific
    std::string exclusion_operator;     // Operator for exclusion
    std::string index_method;           // Index method (GIST, etc.)
    
    // Common fields
    bool is_deferrable = false;         // Can constraint be deferred?
    bool initially_deferred = false;    // Defer by default?
    bool is_enabled = true;             // Can be disabled
    bool is_validated = true;           // Has constraint been validated?
    bool is_system_generated = false;   // System-generated name?
    
    // Metadata
    uint64_t created_time = 0;
    uint64_t validated_time = 0;        // When constraint was last validated
};
```

### ForeignKeyInfo Structure

**Source:** `include/scratchbird/core/catalog_manager.h:886`

```cpp
struct ForeignKeyInfo {
    ID fk_id;                           // Unique FK constraint ID
    std::string fk_name;                // Constraint name
    ID owner_id;                        // Owner UUID
    
    // Child table (referencing)
    ID child_table_id;                  // Table with the FK
    std::vector<std::string> child_columns;   // FK column names
    
    // Parent table (referenced)
    ID parent_table_id;                 // Referenced table
    std::vector<std::string> parent_columns;  // Referenced column names
    
    // Actions
    FKAction on_delete = FKAction::NO_ACTION;
    FKAction on_update = FKAction::NO_ACTION;
    FKMatchType match_type = FKMatchType::SIMPLE;
    
    // State
    bool is_enabled = true;             // Can be disabled temporarily
    bool is_deferrable = false;
    bool initially_deferred = false;
    
    // Metadata
    uint64_t created_time = 0;
    
    // Dependency tracking IDs
    ID child_dependency_id;             // Dependency: FK → child table (AUTO)
    ID parent_dependency_id;            // Dependency: FK → parent table (NORMAL)
};
```

### sb_constraints Catalog Table

**Source:** `src/core/catalog_manager.cpp:5104`

```cpp
struct ConstraintRecord {
    // Primary key
    ID constraint_id;
    
    // Identity
    ID table_id;
    char constraint_name[512];
    ID owner_id;
    
    // Type and flags
    uint8_t constraint_type;        // ConstraintType
    uint8_t is_deferrable;
    uint8_t initially_deferred;
    uint8_t is_enabled;
    uint8_t is_validated;
    uint8_t is_system_generated;
    uint8_t name_is_delimited;
    uint8_t reserved;
    
    // Column references
    uint16_t column_count;
    ID column_ids[16];              // Up to 16 columns
    
    // Foreign key specific
    uint8_t on_delete;              // FKAction
    uint8_t on_update;              // FKAction
    uint8_t match_type;             // FKMatchType
    uint8_t reserved2;
    ID referenced_table_id;
    uint16_t referenced_column_count;
    ID referenced_column_ids[16];
    
    // CHECK constraint
    ID check_expr_oid;              // TOAST reference
    
    // EXCLUSION constraint
    ID exclusion_operator_oid;      // TOAST reference
    ID index_method_oid;            // TOAST reference
    
    // Metadata
    uint64_t created_time;
    uint64_t validated_time;
    uint32_t is_valid;
    uint32_t padding;
};
```

### Constraint Naming

**System-Generated Names:**

| Constraint Type | Naming Pattern |
|-----------------|----------------|
| PRIMARY KEY | `pk_{table_name}_{column}` |
| UNIQUE | `uk_{table_name}_{column}` |
| FOREIGN KEY | `fk_{child_table}_{parent_table}` |
| CHECK | `ck_{table_name}_{hash}` |
| NOT NULL | `nn_{table_name}_{column}` |

```cpp
// Example: Generate system constraint name
std::string generateConstraintName(
    ConstraintType type,
    const std::string& table_name,
    const std::vector<std::string>& columns
) {
    switch (type) {
        case PRIMARY_KEY:
            return "PK_" + toUpper(table_name);
        case UNIQUE:
            return "UK_" + toUpper(table_name) + "_" + 
                   toUpper(columns[0]);
        case FOREIGN_KEY:
            return "FK_" + hashColumns(columns);
        case CHECK:
            return "CK_" + hashExpression(expr);
        case NOT_NULL:
            return "NN_" + toUpper(table_name) + "_" +
                   toUpper(columns[0]);
    }
}
```

### Constraint Deferral

```sql
-- DEFERRABLE constraints
CREATE TABLE orders (
    order_id INTEGER PRIMARY KEY,
    customer_id INTEGER REFERENCES customers(customer_id)
        DEFERRABLE INITIALLY DEFERRED
);

-- Deferred check
CREATE TABLE employees (
    emp_id INTEGER PRIMARY KEY,
    manager_id INTEGER,
    CONSTRAINT valid_manager 
        CHECK (manager_id != emp_id)
        DEFERRABLE INITIALLY IMMEDIATE
);
```

**Deferral Modes:**

| Mode | Validation Timing | Use Case |
|------|-------------------|----------|
| NOT DEFERRABLE | Immediate | Always enforce immediately |
| DEFERRABLE INITIALLY IMMEDIATE | Immediate (can defer) | Default immediate |
| DEFERRABLE INITIALLY DEFERRED | Deferred | Batch operations |

### Constraint Validation States

```
┌─────────────────────────────────────────────────────────────┐
│                    Constraint Lifecycle                      │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  CREATE CONSTRAINT ──▶ is_enabled=true                      │
│                      │ is_validated=true                    │
│                      │ (immediate validation)               │
│                                                             │
│  OR:                                                        │
│                                                             │
│  CREATE CONSTRAINT ──▶ is_enabled=true                      │
│   NOT VALID          │ is_validated=false                   │
│                      │ (skip existing data)                 │
│                                                             │
│  ALTER CONSTRAINT    ──▶ VALIDATE CONSTRAINT                │
│   VALIDATE             │ Check existing data                │
│                        │ Set is_validated=true              │
│                                                             │
│  ALTER CONSTRAINT    ──▶ DISABLE CONSTRAINT                 │
│   DISABLE              │ Set is_enabled=false               │
│                        │ (no enforcement)                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Foreign Key Relationships

```
┌─────────────────────┐                    ┌─────────────────────┐
│   Parent Table      │                    │    Child Table      │
│   (Referenced)      │                    │   (Referencing)     │
├─────────────────────┤                    ├─────────────────────┤
│ customer_id (PK)    │◄───────────────────│ customer_id (FK)    │
│ name                │   1:N Relationship │ order_id (PK)       │
│ email               │                    │ order_date          │
└─────────────────────┘                    └─────────────────────┘

Constraint Info:
- child_table_id = orders_table_id
- parent_table_id = customers_table_id
- child_columns = ["customer_id"]
- parent_columns = ["customer_id"]
- on_delete = NO_ACTION (default)
- on_update = NO_ACTION (default)
```

## Algorithms

### Algorithm: Create Primary Key

```
Input:  Table ID, column list, constraint name
Output: Constraint ID

1. Validate columns exist in table
2. Validate all columns are NOT NULL
3. Generate UUIDv7 for constraint_id
4. If no name provided, generate system name
5. Create unique B-tree index on columns
6. Create ConstraintRecord:
   - constraint_type = PRIMARY_KEY
   - column_ids = [ordered column IDs]
   - is_deferrable = false
   - is_validated = true
7. Commit transaction
```

### Algorithm: Create Foreign Key

```
Input:  Child table ID, child columns, parent table ID, 
        parent columns, actions
Output: Constraint ID

1. Validate child columns exist
2. Validate parent table and columns exist
3. Verify parent columns form PRIMARY KEY or UNIQUE
4. Verify column count matches
5. Verify column types compatible
6. Generate UUIDv7 for fk_id
7. Create ForeignKeyRecord
8. Create dependencies:
   - child_dependency_id: FK → child table (AUTO)
   - parent_dependency_id: FK → parent table (NORMAL)
9. Commit transaction
```

### Algorithm: Validate CHECK Constraint

```
Input:  Table ID, check expression
Output: Valid/Invalid

1. Parse check expression
2. Identify referenced columns
3. For each row in table:
   a. Evaluate expression with row values
   b. If expression returns false:
      - Return validation failure
      - Include row ID in error
4. Return success
```

### Algorithm: Enforce Foreign Key on DELETE

```
Input:  Parent table ID, parent key values, action type
Output: Success or error

1. Find all child rows referencing this parent key
2. If no children found: return success
3. Switch on_delete action:
   case NO_ACTION:
     Return error: "referenced by foreign key"
   case RESTRICT:
     Return error: "referenced by foreign key"
   case CASCADE:
     Delete all child rows
   case SET_NULL:
     Set child FK columns to NULL
   case SET_DEFAULT:
     Set child FK columns to DEFAULT
4. Return success
```

## Invariants

| ID | Invariant | Verification |
|----|-----------|-------------|
| `CON_INV_001` | constraint_id is valid UUIDv7 | isUuidV7Local() check |
| `CON_INV_002` | table_id references valid table | Foreign key check |
| `CON_INV_003` | All column_ids exist in table | Column validation |
| `CON_INV_004` | FK parent columns have PK/UK | Constraint check |
| `CON_INV_005` | PK columns are NOT NULL | Nullability check |
| `CON_INV_006` | UK columns are NOT NULL | Nullability check |
| `CON_INV_007` | FK column types match parent | Type compatibility |

## Error Handling

| Error Code | Condition | Recovery |
|------------|-----------|----------|
| `CONSTRAINT_EXISTS` | Name conflict | Choose different name |
| `INVALID_COLUMN` | Column doesn't exist | Correct column name |
| `DUPLICATE_KEY` | PK/UK violation | Remove duplicates |
| `FK_VIOLATION` | Referential integrity violation | Fix data or drop FK |
| `CHECK_VIOLATION` | CHECK constraint failed | Correct data |
| `INVALID_FK_ACTION` | Unsupported action | Use valid action |

## Test Coverage

| Test File | Coverage Area |
|-----------|---------------|
| `tests/unit/test_constraints_pk.cpp` | Primary key constraints |
| `tests/unit/test_constraints_uk.cpp` | Unique constraints |
| `tests/unit/test_constraints_fk.cpp` | Foreign key constraints |
| `tests/unit/test_constraints_check.cpp` | CHECK constraints |
| `tests/unit/test_constraints_nn.cpp` | NOT NULL constraints |
| `tests/unit/test_constraints_defer.cpp` | Deferred constraints |

## Related Specifications

- [indexes.md](./indexes.md) - Index-backed constraints
- [tables.md](./tables.md) - Table metadata
- [columns.md](./columns.md) - Column constraints
- [dependency_tracking.md](./dependency_tracking.md) - FK dependencies

## Appendix

### Constraint Record Size

| Component | Size |
|-----------|------|
| Header | 48 bytes |
| Identity | 544 bytes |
| Type/Flags | 8 bytes |
| Column refs | 260 bytes |
| FK fields | 276 bytes |
| TOAST refs | 48 bytes |
| Metadata | 16 bytes |
| **Total** | **~1200 bytes** |

### Foreign Key Action Matrix

| Parent Action | NO_ACTION | RESTRICT | CASCADE | SET_NULL | SET_DEFAULT |
|---------------|-----------|----------|---------|----------|-------------|
| DELETE | Error | Error | Delete child | NULL FK | DEFAULT |
| UPDATE | Error | Error | Update FK | NULL FK | DEFAULT |

### Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
