# CASCADE DROP Specification

**Version:** 2.0 (CONSERVATIVE POLICY)
**Date:** 2025-12-14
**Status:** Design Specification

---

## 1. Overview

This document defines the standardized DROP behavior for all ScratchBird database objects.

### 1.1 Core Philosophy: SAFETY FIRST

**ScratchBird follows a CONSERVATIVE drop policy:**
- **Default behavior: RESTRICT** - Drop fails if ANY dependencies exist
- **Explicit dependencies BLOCK drops** - User must manually resolve dependencies first
- **Owned objects auto-drop** - Only objects physically owned by the parent cascade automatically
- **Clear error messages** - Report ALL blocking dependencies when drop fails

**Rationale:** Prevent accidental data loss and maintain referential integrity. Users must explicitly clean up dependencies before dropping objects.

---

## 2. Drop Behavior Categories

### ✅ **ALWAYS** - Auto-Drop (Owned Objects)
Objects that are **physically owned** by the parent and have no independent existence.

**Examples:**
- Table owns its indexes, triggers, constraints
- Index owned by table
- Trigger owned by table

**Behavior:** Dropped automatically when owner is dropped (no user intervention needed).

---

### ❌ **RESTRICT** - Error and Report (Dependent Objects)
Objects that **reference** or **depend on** the target but are independent.

**Examples:**
- Views referencing a table
- Functions using a table
- Columns using a sequence in DEFAULT
- Foreign keys TO a table (parent side)

**Behavior:** Drop **FAILS** with error message listing ALL blocking dependencies. User must manually drop dependents first.

---

### 🚫 **EMPTY ONLY** - Must Be Empty (Container Objects)
Container objects that can only be dropped when completely empty.

**Examples:**
- Schemas (must contain no objects)

**Behavior:** Drop **FAILS** if schema contains ANY objects. User must manually drop/move all objects first.

---

## 3. Object-by-Object Drop Specifications

---

## 3.1 DROP TABLE

### Syntax:
```sql
DROP TABLE table_name;
```

### Auto-Drop (Owned Objects):

| Object Type | Behavior | Rationale |
|-------------|----------|-----------|
| **Indexes on this table** | ✅ ALWAYS | Physically owned, no meaning without table |
| **Triggers on this table** | ✅ ALWAYS | Physically owned, cannot fire without table |
| **CHECK constraints** | ✅ ALWAYS | Physically owned, validates table data only |
| **UNIQUE constraints** | ✅ ALWAYS | Physically owned, enforces table uniqueness |
| **PRIMARY KEY** | ✅ ALWAYS | Physically owned, identifies table rows |
| **Foreign keys FROM this table** | ✅ ALWAYS | Child side FK - table owns this constraint |

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **Views referencing this table** | ❌ RESTRICT | "Cannot drop table - views depend on it" |
| **Materialized views based on this table** | ❌ RESTRICT | "Cannot drop table - materialized views depend on it" |
| **Foreign keys TO this table (parent side)** | ❌ RESTRICT | "Cannot drop table - foreign keys reference it" |
| **Functions/procedures using this table** | ❌ RESTRICT | "Cannot drop table - stored code references it" |

### Implementation:

```sql
-- SUCCESS - no dependents
DROP TABLE temporary_data;
-- ✅ Drops table
-- ✅ Auto-drops all indexes on temporary_data
-- ✅ Auto-drops all triggers on temporary_data
-- ✅ Auto-drops all constraints on temporary_data
-- ✅ Auto-drops all FKs FROM temporary_data

-- FAILURE - views depend on table
DROP TABLE employees;
-- ❌ ERROR: Cannot drop table "employees" because other objects depend on it
-- DETAIL:
--   - View "employee_summary" depends on table "employees"
--   - View "active_employees" depends on table "employees"
--   - Materialized view "employee_stats" depends on table "employees"
--   - Function "get_employee_name" references table "employees"
-- HINT: Drop or modify dependent objects first.

-- FAILURE - foreign keys reference this table
DROP TABLE customers;
-- ❌ ERROR: Cannot drop table "customers" because foreign keys reference it
-- DETAIL:
--   - Foreign key "fk_orders_customer" on table "orders" references "customers"
--   - Foreign key "fk_invoices_customer" on table "invoices" references "customers"
-- HINT: Drop foreign keys or child tables first.
```

### User Workflow:

```sql
-- Step 1: Check dependencies
SELECT * FROM SYS.DEPENDENCIES WHERE referenced_object_id = <table_id>;

-- Step 2: Drop dependent views
DROP VIEW employee_summary;
DROP VIEW active_employees;

-- Step 3: Drop foreign keys from child tables
ALTER TABLE orders DROP CONSTRAINT fk_orders_customer;
ALTER TABLE invoices DROP CONSTRAINT fk_invoices_customer;

-- Step 4: Now can drop table
DROP TABLE customers;
-- ✅ SUCCESS
```

---

## 3.2 DROP VIEW

### Syntax:
```sql
DROP VIEW view_name;
```

### Auto-Drop (Owned Objects):

| Object Type | Behavior | Rationale |
|-------------|----------|-----------|
| **Triggers on this view** | ✅ ALWAYS | Owned by view (if SELECT triggers supported) |

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **Views referencing this view** | ❌ RESTRICT | "Cannot drop view - other views depend on it" |
| **Materialized views based on this view** | ❌ RESTRICT | "Cannot drop view - materialized views depend on it" |
| **Functions/procedures using this view** | ❌ RESTRICT | "Cannot drop view - stored code references it" |

### Implementation:

```sql
-- FAILURE - dependent views exist
DROP VIEW employee_summary;
-- ❌ ERROR: Cannot drop view "employee_summary" because other objects depend on it
-- DETAIL:
--   - View "department_summary" depends on view "employee_summary"
--   - Materialized view "monthly_stats" depends on view "employee_summary"
-- HINT: Drop dependent views first.

-- User must drop dependents manually
DROP VIEW department_summary;
DROP VIEW monthly_stats;
DROP VIEW employee_summary;  -- Now succeeds
```

---

## 3.3 DROP SEQUENCE

### Syntax:
```sql
DROP SEQUENCE sequence_name;
```

### Auto-Drop (Owned Objects):
**None** - Sequences are independent objects.

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **Columns with DEFAULT using this sequence** | ❌ RESTRICT | "Cannot drop sequence - columns use it in DEFAULT" |

### Implementation:

```sql
-- FAILURE - columns use this sequence
DROP SEQUENCE emp_id_seq;
-- ❌ ERROR: Cannot drop sequence "emp_id_seq" because columns depend on it
-- DETAIL:
--   - Column "employees.id" uses this sequence in DEFAULT
--   - Column "temp_employees.id" uses this sequence in DEFAULT
-- HINT: Drop DEFAULT constraints first using:
--   ALTER TABLE employees ALTER COLUMN id DROP DEFAULT;

-- User must remove DEFAULT clauses manually
ALTER TABLE employees ALTER COLUMN id DROP DEFAULT;
ALTER TABLE temp_employees ALTER COLUMN id DROP DEFAULT;

-- Now can drop sequence
DROP SEQUENCE emp_id_seq;
-- ✅ SUCCESS
```

---

## 3.4 DROP DOMAIN

### Syntax:
```sql
DROP DOMAIN domain_name;
```

### Auto-Drop (Owned Objects):
**None** - Domains are independent objects.

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **Columns typed as this domain** | ❌ RESTRICT | "Cannot drop domain - columns use this type" |

### Implementation:

```sql
-- FAILURE - columns use this domain
DROP DOMAIN email_address;
-- ❌ ERROR: Cannot drop domain "email_address" because columns use it
-- DETAIL:
--   - Column "users.email" is typed as "email_address"
--   - Column "contacts.email" is typed as "email_address"
-- HINT: Alter columns to base type first using:
--   ALTER TABLE users ALTER COLUMN email TYPE VARCHAR(255);

-- User must alter columns to base type manually
ALTER TABLE users ALTER COLUMN email TYPE VARCHAR(255);
ALTER TABLE contacts ALTER COLUMN email TYPE VARCHAR(255);

-- Now can drop domain
DROP DOMAIN email_address;
-- ✅ SUCCESS
```

**Note:** When altering column from domain to base type, **domain constraints are lost** (CHECK, NOT NULL). User should recreate constraints if needed:
```sql
ALTER TABLE users ALTER COLUMN email TYPE VARCHAR(255);
ALTER TABLE users ADD CONSTRAINT chk_email CHECK (email LIKE '%@%');
```

---

## 3.5 DROP FUNCTION / DROP PROCEDURE

### Syntax:
```sql
DROP FUNCTION function_name;
DROP PROCEDURE procedure_name;
```

### Auto-Drop (Owned Objects):
**None** - Functions/procedures are independent objects.

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **Triggers calling this function/procedure** | ❌ RESTRICT | "Cannot drop function - triggers call it" |
| **Other functions/procedures calling this** | ❌ RESTRICT | "Cannot drop function - other code calls it" |
| **Views using this function** | ❌ RESTRICT | "Cannot drop function - views use it" |
| **Package members calling this** | ❌ RESTRICT | "Cannot drop function - packages call it" |

### Implementation:

```sql
-- FAILURE - triggers call this function
DROP FUNCTION calculate_bonus;
-- ❌ ERROR: Cannot drop function "calculate_bonus" because other objects depend on it
-- DETAIL:
--   - Trigger "before_insert_employee" calls this function
--   - Procedure "process_payroll" calls this function
--   - View "employee_compensation" uses this function
-- HINT: Drop or modify dependent objects first.

-- User must drop dependents manually
DROP TRIGGER before_insert_employee;
ALTER PROCEDURE process_payroll ...;  -- Rewrite without calculate_bonus
DROP VIEW employee_compensation;

-- Now can drop function
DROP FUNCTION calculate_bonus;
-- ✅ SUCCESS
```

---

## 3.6 DROP TRIGGER

### Syntax:
```sql
DROP TRIGGER trigger_name;
```

### Auto-Drop (Owned Objects):
**None** - Triggers don't own other objects.

### RESTRICT (Must Drop First):
**None** - Triggers are **leaf objects**. Nothing depends on triggers.

### Implementation:

```sql
DROP TRIGGER before_insert_employee;
-- ✅ ALWAYS succeeds (if trigger exists)
-- ❌ Does NOT drop parent table (employees)
-- ❌ Does NOT drop called procedure (validate_employee)
```

**Note:** Triggers are automatically dropped when their parent table is dropped.

---

## 3.7 DROP CONSTRAINT (Foreign Key)

### Syntax:
```sql
ALTER TABLE table_name DROP CONSTRAINT constraint_name;
```

### Auto-Drop (Owned Objects):
**None** - Constraints don't own other objects.

### RESTRICT (Must Drop First):
**None** - Foreign keys are **leaf objects**. Nothing depends on FKs.

### Implementation:

```sql
ALTER TABLE orders DROP CONSTRAINT fk_orders_customer;
-- ✅ ALWAYS succeeds (if constraint exists)
-- ❌ Does NOT drop child table (orders)
-- ❌ Does NOT drop parent table (customers)
-- ⚠️ Orphaned rows are now allowed
```

**Note:** Foreign keys are automatically dropped when their child table is dropped.

---

## 3.8 DROP PACKAGE

### Syntax:
```sql
DROP PACKAGE package_name;
```

### Auto-Drop (Owned Objects):

| Object Type | Behavior | Rationale |
|-------------|----------|-----------|
| **Package functions/procedures** | ✅ ALWAYS | Package members are owned by package |

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **Triggers calling package procedures** | ❌ RESTRICT | "Cannot drop package - triggers call members" |
| **Functions/procedures calling package members** | ❌ RESTRICT | "Cannot drop package - code calls members" |
| **Views using package functions** | ❌ RESTRICT | "Cannot drop package - views use members" |

### Implementation:

```sql
-- FAILURE - triggers call package members
DROP PACKAGE employee_pkg;
-- ❌ ERROR: Cannot drop package "employee_pkg" because other objects depend on it
-- DETAIL:
--   - Trigger "before_insert_employee" calls "employee_pkg.validate"
--   - Procedure "process_payroll" calls "employee_pkg.calculate_salary"
--   - View "employee_summary" uses "employee_pkg.get_status"
-- HINT: Drop or modify dependent objects first.

-- User must drop dependents manually
DROP TRIGGER before_insert_employee;
ALTER PROCEDURE process_payroll ...;  -- Rewrite without package
DROP VIEW employee_summary;

-- Now can drop package
DROP PACKAGE employee_pkg;
-- ✅ SUCCESS
-- ✅ Auto-drops all package members (procedures/functions inside package)
```

---

## 3.9 DROP UDR (User-Defined Routine)

### Syntax:
```sql
DROP UDR udr_name;
```

**Same as DROP FUNCTION/PROCEDURE** - UDRs are external functions with same dependency rules.

---

## 3.10 DROP EXCEPTION

### Syntax:
```sql
DROP EXCEPTION exception_name;
```

### Auto-Drop (Owned Objects):
**None** - Exceptions are independent objects.

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **Procedures that RAISE this exception** | ❌ RESTRICT | "Cannot drop exception - procedures raise it" |

### Implementation:

```sql
-- FAILURE - procedures raise this exception
DROP EXCEPTION invalid_employee;
-- ❌ ERROR: Cannot drop exception "invalid_employee" because procedures reference it
-- DETAIL:
--   - Procedure "validate_employee" raises this exception
--   - Procedure "insert_employee" raises this exception
-- HINT: Modify procedures to remove RAISE statements or use different exception.

-- User must modify procedures manually
ALTER PROCEDURE validate_employee ...;  -- Remove RAISE invalid_employee
ALTER PROCEDURE insert_employee ...;    -- Remove RAISE invalid_employee

-- Now can drop exception
DROP EXCEPTION invalid_employee;
-- ✅ SUCCESS
```

---

## 3.11 DROP INDEX

### Syntax:
```sql
DROP INDEX index_name;
```

### Auto-Drop (Owned Objects):
**None** - Indexes don't own other objects.

### RESTRICT (Must Drop First):
**None** - Indexes are **leaf objects**. Nothing depends on indexes.

### Implementation:

```sql
DROP INDEX idx_employees_email;
-- ✅ ALWAYS succeeds (if index exists)
-- ❌ Does NOT drop table (employees)
-- ⚠️ Queries may slow down but still work
```

**Note:** Indexes are automatically dropped when their parent table is dropped.

---

## 3.12 DROP SYNONYM

### Syntax:
```sql
DROP SYNONYM synonym_name;
```

### Auto-Drop (Owned Objects):
**None** - Synonyms don't own other objects.

### RESTRICT (Must Drop First):
**None** - Synonyms are **transparent aliases**. Code using synonym will get "object not found" error after synonym dropped, but synonym itself has no dependencies.

### Implementation:

```sql
DROP SYNONYM emp;
-- ✅ ALWAYS succeeds (if synonym exists)
-- ❌ Does NOT drop target table (employees)
-- ⚠️ Code using "emp" will fail, but this is user's responsibility
```

**Design Note:** Synonyms are convenience aliases. We could track objects that reference the synonym and block drop, but this would be overly restrictive. Users must know what code uses synonyms.

---

## 3.13 DROP SCHEMA

### Syntax:
```sql
DROP SCHEMA schema_name;
```

### Auto-Drop (Owned Objects):
**None** - Schema can only be dropped when **completely empty**.

### RESTRICT (Must Drop First):

| Object Type | Behavior | Error Message |
|-------------|----------|---------------|
| **ALL objects in schema** | 🚫 EMPTY ONLY | "Cannot drop schema - schema contains objects" |
| **Child schemas (hierarchical)** | 🚫 EMPTY ONLY | "Cannot drop schema - schema contains child schemas" |

### Implementation:

```sql
-- FAILURE - schema contains objects
DROP SCHEMA company;
-- ❌ ERROR: Cannot drop schema "company" because it contains objects
-- DETAIL:
--   Schema "company" contains:
--   - 47 tables
--   - 123 views
--   - 89 functions
--   - 12 procedures
--   - 34 sequences
--   - 8 domains
--   - 2 child schemas (company.hr, company.payroll)
--   Total: 315 objects
-- HINT: Drop or move all objects from schema first.

-- SUCCESS - empty schema
DROP SCHEMA temp_schema;
-- ✅ Schema is empty
-- ✅ SUCCESS
```

### User Workflow:

```sql
-- Step 1: List all objects in schema
SELECT object_name, object_type FROM SYS.OBJECTS
WHERE schema_id = <schema_id>
ORDER BY object_type, object_name;

-- Step 2: Drop all objects manually (in dependency order)
-- First: leaf objects (triggers, indexes, views)
DROP VIEW company.employee_summary;
DROP TRIGGER company.validate_employee;
-- ...

-- Second: tables (after views/triggers dropped)
DROP TABLE company.employees;
-- ...

-- Third: code objects
DROP FUNCTION company.calculate_bonus;
-- ...

-- Fourth: type objects
DROP SEQUENCE company.emp_id_seq;
DROP DOMAIN company.email_address;
-- ...

-- Fifth: child schemas (after they're empty)
DROP SCHEMA company.hr;
DROP SCHEMA company.payroll;

-- Step 3: Now can drop parent schema
DROP SCHEMA company;
-- ✅ SUCCESS
```

**Alternative:** Provide utility to move all objects to different schema:
```sql
-- Move all objects from company to archive schema
CALL SYS.MOVE_SCHEMA_OBJECTS('company', 'archive');

-- Now company is empty
DROP SCHEMA company;  -- ✅ SUCCESS
```

---

## 4. Summary Table

### 4.1 Drop Behavior by Object Type

| Object Type | Auto-Drops | Blocks On | Can Be Blocked By |
|-------------|-----------|-----------|-------------------|
| **TABLE** | Indexes, triggers, constraints, FKs FROM | Views, FKs TO, MVs, functions | N/A |
| **VIEW** | Triggers on view (rare) | Dependent views, MVs, functions | N/A |
| **SEQUENCE** | Nothing | Columns using in DEFAULT | N/A |
| **DOMAIN** | Nothing | Columns typed as domain | N/A |
| **FUNCTION** | Nothing | Triggers, views, code calling it | N/A |
| **PROCEDURE** | Nothing | Triggers, code calling it | N/A |
| **TRIGGER** | Nothing | Nothing (leaf) | N/A |
| **CONSTRAINT (FK)** | Nothing | Nothing (leaf) | N/A |
| **PACKAGE** | Package members | Triggers, code calling members | N/A |
| **UDR** | Nothing | Triggers, code calling it | N/A |
| **EXCEPTION** | Nothing | Procedures raising it | N/A |
| **INDEX** | Nothing | Nothing (leaf) | N/A |
| **SYNONYM** | Nothing | Nothing (alias) | N/A |
| **SCHEMA** | Nothing (must be empty) | ANY object in schema | N/A |

---

## 5. Error Message Format

### 5.1 Standard Error Structure

All dependency errors MUST follow this format:

```
ERROR: Cannot drop <object_type> "<object_name>" because other objects depend on it
DETAIL:
  <Dependency list - grouped by type>
HINT: <Suggested action>
```

### 5.2 Error Message Examples

**Example 1: Table with multiple dependency types**
```
ERROR: Cannot drop table "employees" because other objects depend on it
DETAIL:
  Views:
    - employee_summary
    - active_employees
    - department_roster
  Materialized Views:
    - employee_stats_mv
  Functions:
    - get_employee_name(id INTEGER)
    - calculate_employee_bonus(id INTEGER)
  Foreign Keys:
    - fk_orders_employee on table "orders" (column: employee_id → employees.id)
    - fk_timesheets_employee on table "timesheets" (column: employee_id → employees.id)
HINT: Drop dependent objects first, or use ALTER TABLE to drop/modify foreign keys.
```

**Example 2: View with dependents**
```
ERROR: Cannot drop view "employee_summary" because other objects depend on it
DETAIL:
  Views:
    - department_summary (references employee_summary.department_id)
    - manager_report (joins with employee_summary)
  Materialized Views:
    - monthly_stats_mv
HINT: Drop dependent views first in order: department_summary, manager_report, monthly_stats_mv
```

**Example 3: Sequence in use**
```
ERROR: Cannot drop sequence "emp_id_seq" because columns depend on it
DETAIL:
  Columns using this sequence in DEFAULT:
    - employees.id (DEFAULT nextval('emp_id_seq'))
    - temp_employees.id (DEFAULT nextval('emp_id_seq'))
HINT: Remove DEFAULT constraints first:
  ALTER TABLE employees ALTER COLUMN id DROP DEFAULT;
  ALTER TABLE temp_employees ALTER COLUMN id DROP DEFAULT;
```

**Example 4: Function with dependents**
```
ERROR: Cannot drop function "calculate_bonus(INTEGER)" because other objects depend on it
DETAIL:
  Triggers:
    - before_insert_employee on table "employees"
    - before_update_employee on table "employees"
  Procedures:
    - process_payroll
  Views:
    - employee_compensation
HINT: Drop or modify dependent objects to remove references to this function.
```

**Example 5: Schema not empty**
```
ERROR: Cannot drop schema "company" because it contains objects
DETAIL:
  Schema "company" contains:
    Tables: 47
    Views: 123
    Functions: 89
    Procedures: 12
    Sequences: 34
    Domains: 8
    Triggers: 156
    Indexes: 234
    Child Schemas: 2 (company.hr, company.payroll)
  Total: 705 objects
HINT: Drop all objects from schema first. To see list of objects:
  SELECT object_name, object_type FROM SYS.OBJECTS WHERE schema_name = 'company';
```

---

## 6. Dependency Query API

### 6.1 Check Dependencies Before Drop

Users can query dependencies before attempting drop:

```sql
-- Check what depends ON an object (blocks drop)
SELECT d.dependent_object_name, d.dependent_object_type
FROM SYS.DEPENDENCIES d
WHERE d.referenced_object_id = <object_id>
ORDER BY d.dependent_object_type, d.dependent_object_name;

-- Check what an object depends ON (what it references)
SELECT d.referenced_object_name, d.referenced_object_type
FROM SYS.DEPENDENCIES d
WHERE d.dependent_object_id = <object_id>
ORDER BY d.referenced_object_type, d.referenced_object_name;

-- Get full dependency tree (recursive)
WITH RECURSIVE dep_tree AS (
  -- Base: direct dependencies
  SELECT dependent_object_id, referenced_object_id, 1 as depth
  FROM SYS.DEPENDENCIES
  WHERE referenced_object_id = <object_id>

  UNION ALL

  -- Recursive: transitive dependencies
  SELECT d.dependent_object_id, d.referenced_object_id, dt.depth + 1
  FROM SYS.DEPENDENCIES d
  JOIN dep_tree dt ON d.referenced_object_id = dt.dependent_object_id
  WHERE dt.depth < 10  -- Prevent infinite loops
)
SELECT DISTINCT * FROM dep_tree;
```

---

## 7. Implementation Guidelines

### 7.1 Drop Operation Flow

**For every DROP operation:**

```
1. BEGIN TRANSACTION
2. Check if object exists
   - If not, return NOT_FOUND or success (if IF EXISTS)
3. Identify owned objects (auto-drop list)
   - Indexes, triggers, constraints for tables
   - Package members for packages
4. Check for blocking dependencies
   - Query dependency_cache_ for dependents
   - If dependents exist:
     a. Build error message with grouped dependency list
     b. ROLLBACK
     c. Return error
5. Drop owned objects (in reverse dependency order)
6. Drop target object (soft delete: set is_valid = 0)
7. Clear dependencies: clearDependenciesFor(object_id)
8. COMMIT
```

### 7.2 Auto-Drop Order

When dropping owned objects, follow this order:

```
1. Triggers (leaf objects, depend on table)
2. Indexes (leaf objects, depend on table)
3. Constraints (CHECK, UNIQUE, PK)
4. Foreign keys FROM this table (child side)
5. Target object (table itself)
```

**Rationale:** Drop in reverse dependency order (dependents before dependencies).

### 7.3 Dependency Checking Implementation

```cpp
// Example: dropTable implementation
Status CatalogManager::dropTable(const ID& table_id, ErrorContext* ctx)
{
    // 1. Get table info
    TableInfo table_info;
    Status status = getTable(table_id, table_info, ctx);
    if (status != Status::OK) return status;

    // 2. Check for blocking dependencies
    std::vector<DependencyInfo> dependents;
    status = getDependents(table_id, dependents, ctx);
    if (status != Status::OK) return status;

    // 3. Filter out owned objects (these will auto-drop)
    std::vector<DependencyInfo> blocking_deps;
    for (const auto& dep : dependents) {
        // Skip owned objects
        if (dep.dependent_type == ObjectType::INDEX) continue;      // Owned
        if (dep.dependent_type == ObjectType::TRIGGER) continue;    // Owned
        if (dep.dependent_type == ObjectType::CONSTRAINT) continue; // Owned

        // Skip child-side FKs (owned by this table)
        if (dep.dependent_type == ObjectType::FOREIGN_KEY &&
            dep.dependent_object_id == table_id) continue;  // FK FROM this table

        // Everything else blocks drop
        blocking_deps.push_back(dep);
    }

    // 4. If blocking dependencies exist, fail with error
    if (!blocking_deps.empty()) {
        std::string error_msg = buildDependencyErrorMessage(table_info.name, blocking_deps);
        SET_ERROR_CONTEXT(ctx, Status::CONSTRAINT_VIOLATION, error_msg.c_str());
        return Status::CONSTRAINT_VIOLATION;
    }

    // 5. Drop owned objects (auto-cascade)
    status = dropOwnedIndexes(table_id, ctx);
    if (status != Status::OK) return status;

    status = dropOwnedTriggers(table_id, ctx);
    if (status != Status::OK) return status;

    status = dropOwnedConstraints(table_id, ctx);
    if (status != Status::OK) return status;

    // 6. Soft delete table
    status = deleteTableRecord(table_id, ctx);
    if (status != Status::OK) return status;

    // 7. Clear dependencies
    clearDependenciesFor(table_id, ctx);

    // 8. Remove from cache
    table_cache_.erase(table_id);

    return Status::OK;
}
```

---

## 8. Compatibility Notes

### 8.1 Firebird Compatibility

**Firebird 3.0+ behavior:**
- DROP TABLE - Fails if views depend on it (RESTRICT behavior) ✅ MATCHES
- DROP VIEW - Fails if other views depend on it (RESTRICT behavior) ✅ MATCHES
- DROP SEQUENCE - Fails if columns use it (RESTRICT behavior) ✅ MATCHES
- DROP DOMAIN - Fails if columns use it (RESTRICT behavior) ✅ MATCHES
- DROP PROCEDURE - Fails if code calls it (RESTRICT behavior) ✅ MATCHES

**Firebird CASCADE:**
- Firebird 3.0 added limited CASCADE support for compatibility
- ScratchBird does NOT implement CASCADE keyword (always RESTRICT)

**Conclusion:** ScratchBird matches Firebird's conservative drop semantics ✅

---

### 8.2 PostgreSQL Differences

**PostgreSQL allows:**
- DROP TABLE CASCADE - drops dependent views/functions
- DROP FUNCTION CASCADE - drops dependent triggers/views
- DROP SCHEMA CASCADE - drops all contained objects

**ScratchBird is more conservative:**
- All drops are RESTRICT by default
- Schema must be empty before drop
- User must manually resolve all dependencies

**Rationale:** Safety over convenience. PostgreSQL's CASCADE can drop hundreds of objects accidentally.

---

## 9. Testing Requirements

### 9.1 Test Cases Per Object Type

For each droppable object type:

**Test 1: Drop with no dependencies**
- Create object
- Drop object
- ✅ Should succeed
- ✅ Should auto-drop owned objects (indexes, triggers)
- ✅ Should clear dependencies

**Test 2: Drop with blocking dependencies**
- Create object A
- Create object B that depends on A
- Try to drop A
- ❌ Should fail with error
- ✅ Error should list object B
- ✅ Object A should NOT be dropped (transaction rolled back)

**Test 3: Drop after resolving dependencies**
- Create object A
- Create object B that depends on A
- Drop object B (resolve dependency)
- Drop object A
- ✅ Should succeed

**Test 4: Drop with multiple dependency types**
- Create table
- Create view on table
- Create FK to table
- Create function using table
- Try to drop table
- ❌ Should fail with error
- ✅ Error should list ALL dependents grouped by type

**Test 5: Auto-drop of owned objects**
- Create table
- Create 5 indexes on table
- Create 3 triggers on table
- Drop table
- ✅ Table should be dropped
- ✅ All indexes should be auto-dropped
- ✅ All triggers should be auto-dropped

**Test 6: Schema empty check**
- Create schema
- Create table in schema
- Try to drop schema
- ❌ Should fail (schema not empty)
- Drop table
- Drop schema
- ✅ Should succeed (schema now empty)

---

## 10. Future Considerations

### 10.1 CASCADE Keyword (Not Implemented)

**Current decision:** No CASCADE support. All drops are RESTRICT.

**Future option:** Could add CASCADE for user convenience:
```sql
DROP TABLE employees CASCADE;
-- Would drop dependent views, FKs, etc.
```

**If implemented:**
- Require explicit confirmation for CASCADE drops
- Show full list of objects that will be dropped
- Require typing object name to confirm
- Log CASCADE operations to audit trail

**Recommendation:** Do NOT implement CASCADE. Keep conservative RESTRICT-only policy for safety.

---

### 10.2 Dependency Visualization

**Future enhancement:** Provide tools to visualize dependency graphs:

```sql
-- Show dependency tree
SHOW DEPENDENCIES FOR TABLE employees;

Output:
  employees (table)
  ├── Owned objects (auto-drop):
  │   ├── idx_employees_email (index)
  │   ├── idx_employees_dept (index)
  │   ├── before_insert_employee (trigger)
  │   └── fk_employees_departments (foreign key FROM)
  └── Blocking dependencies (must drop first):
      ├── employee_summary (view)
      ├── active_employees (view)
      ├── fk_orders_employee (foreign key TO from orders)
      └── get_employee_name (function)
```

---

## 11. Summary

### 11.1 Core Principles

1. **RESTRICT by default** - Safety first, user must resolve dependencies
2. **Owned objects auto-drop** - Indexes, triggers, constraints dropped with parent
3. **Clear error messages** - List ALL blocking dependencies grouped by type
4. **Empty schemas only** - Schema drop requires manual cleanup of all objects
5. **No CASCADE keyword** - Conservative policy prevents accidental data loss

### 11.2 Key Behaviors

| Action | Auto-Drops | Blocks On |
|--------|-----------|-----------|
| DROP TABLE | Indexes, triggers, constraints, child-side FKs | Views, parent-side FKs, functions |
| DROP VIEW | Triggers on view | Dependent views, functions |
| DROP FUNCTION | Nothing | Triggers, views, code calling it |
| DROP SEQUENCE | Nothing | Columns using it in DEFAULT |
| DROP DOMAIN | Nothing | Columns typed as domain |
| DROP SCHEMA | **NOTHING** | **ANY object in schema** |

### 11.3 User Workflow

1. **Check dependencies** - Query SYS.DEPENDENCIES before drop
2. **Resolve dependencies** - Drop/modify dependent objects manually
3. **Drop object** - Once dependencies resolved, drop succeeds
4. **Owned objects auto-drop** - No need to drop indexes/triggers manually

**This conservative approach prevents accidental data loss while maintaining referential integrity.**

---

**END OF SPECIFICATION**
