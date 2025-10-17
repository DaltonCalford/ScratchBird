# SCRATCHBIRD ALPHA COMPLETION - DETAILED TODO (PART 2)

**Part**: 2 of 3 (Phases 7-9)
**Date**: October 16, 2025
**Status**: Planning Phase
**Prerequisites**: ALPHA_COMPLETION_DETAILED_TODO.md (Phases 1-6)

---

## TABLE OF CONTENTS

- [Phase 7: Stored Procedures & Triggers](#phase-7-stored-procedures--triggers-275-days-55-weeks)
- [Phase 8: Advanced Features - CTEs & Window Functions](#phase-8-advanced-features---ctes--window-functions-25-days-5-weeks)
- [Phase 9: Built-in Functions](#phase-9-built-in-functions-175-days-35-weeks)

---

## PHASE 7: Stored Procedures & Triggers (27.5 days, 5.5 weeks)

**Goal**: Implement PSQL stored procedures, functions, and triggers with full Firebird compatibility

**Total Features**: 22
**Dependencies**: Phase 6 (PSQL Basics) must be complete

---

### PROC-001: Parse CREATE PROCEDURE Statement
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 2 days
- **Dependencies**: None
- **Files to Modify**:
  - `src/parser/parser.cpp`
  - `include/scratchbird/parser/ast.h`
  - `src/parser/lexer.cpp` (add procedure keywords)

**Tasks**:
1. Add `CreateProcedureStatement` AST node with fields:
   - `std::string name`
   - `std::vector<ProcedureParameter> params` (name, type, mode: IN/OUT/INOUT)
   - `std::vector<VariableDeclaration> local_vars`
   - `std::unique_ptr<Statement> body`
2. Parse `CREATE PROCEDURE name [(param_list)] AS [DECLARE ...] BEGIN ... END`
3. Support parameter modes: IN (default), OUT, INOUT
4. Parse DECLARE section for local variables
5. Parse BEGIN...END block as procedure body
6. Add validation: no duplicate parameter names, valid data types

**Acceptance Criteria**:
```sql
CREATE PROCEDURE update_salary(emp_id INTEGER, new_salary DECIMAL(10,2))
AS
DECLARE
    old_salary DECIMAL(10,2);
BEGIN
    SELECT salary FROM employees WHERE id = emp_id INTO old_salary;
    UPDATE employees SET salary = new_salary WHERE id = emp_id;
END

-- Parser generates CreateProcedureStatement AST
```

---

### PROC-002: Generate Bytecode for CREATE PROCEDURE
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 2 days
- **Dependencies**: PROC-001

**Tasks**:
1. Add `OP_CREATE_PROCEDURE` opcode
2. Generate bytecode that stores:
   - Procedure name
   - Parameter list (names, types, modes)
   - Local variable declarations
   - Compiled bytecode for procedure body
3. Emit bytecode to register procedure in catalog
4. Handle nested blocks and variable scoping

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/opcodes.h`

**Acceptance Criteria**:
- CREATE PROCEDURE generates bytecode with OP_CREATE_PROCEDURE
- Bytecode includes parameter metadata and compiled body

---

### PROC-003: Store Procedures in Catalog
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Catalog Manager
- **Estimate**: 2 days
- **Dependencies**: PROC-002

**Tasks**:
1. Add `pg_proc` system table with columns:
   - `oid` (ObjectID)
   - `proname` (VARCHAR, procedure name)
   - `pronamespace` (ObjectID, schema)
   - `prorettype` (TypeID, return type - void for procedures)
   - `proargtypes` (TEXT, argument type OIDs as array)
   - `proargnames` (TEXT, argument names as array)
   - `proargmodes` (TEXT, 'i'=IN, 'o'=OUT, 'b'=INOUT)
   - `prosrc` (TEXT, source code)
   - `probin` (BYTEA, compiled bytecode)
2. Implement `createProcedure()` in CatalogManager
3. Implement `getProcedure(name)` returning Procedure metadata
4. Implement `listProcedures(schema)`

**Files to Modify**:
- `src/core/catalog_manager.cpp`
- `include/scratchbird/core/catalog_manager.h`
- `docs/system_tables/pg_proc.md` (create)

**Acceptance Criteria**:
```cpp
catalog_mgr->createProcedure("update_salary", schema_id, params, bytecode);
auto proc = catalog_mgr->getProcedure("update_salary");
assert(proc.parameter_count == 2);
```

---

### PROC-004: Execute CREATE PROCEDURE
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 1 day
- **Dependencies**: PROC-003

**Tasks**:
1. Implement OP_CREATE_PROCEDURE execution in executor
2. Call `catalog_mgr->createProcedure()` to store procedure
3. Return success status

**Files to Modify**:
- `src/sblr/executor.cpp`

**Acceptance Criteria**:
- `CREATE PROCEDURE update_salary(...)` successfully stores procedure
- Procedure appears in `pg_proc` system table

---

### PROC-005: Parse CALL Statement
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 1 day
- **Dependencies**: PROC-001

**Tasks**:
1. Add `CallStatement` AST node:
   - `std::string procedure_name`
   - `std::vector<Expression*> arguments`
2. Parse `CALL procedure_name(arg1, arg2, ...)`
3. Support named parameters: `CALL proc(param1 => value1, param2 => value2)`

**Files to Modify**:
- `src/parser/parser.cpp`
- `include/scratchbird/parser/ast.h`

**Acceptance Criteria**:
```sql
CALL update_salary(1, 50000.00)
-- Parser generates CallStatement AST
```

---

### PROC-006: Generate Bytecode for CALL
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 1.5 days
- **Dependencies**: PROC-005

**Tasks**:
1. Add `OP_CALL_PROCEDURE` opcode
2. Look up procedure in catalog to validate parameter count/types
3. Generate bytecode to:
   - Evaluate argument expressions
   - Push arguments onto stack
   - Emit OP_CALL_PROCEDURE with procedure OID
   - Handle OUT/INOUT parameters (store result locations)

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/opcodes.h`

**Acceptance Criteria**:
- `CALL update_salary(1, 50000.00)` generates OP_CALL_PROCEDURE bytecode

---

### PROC-007: Execute CALL Statement
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 2.5 days
- **Dependencies**: PROC-006

**Tasks**:
1. Implement OP_CALL_PROCEDURE execution
2. Create new execution context for procedure:
   - Parameter stack frame
   - Local variable stack frame
3. Load procedure bytecode from catalog
4. Execute procedure body in new context
5. Handle OUT/INOUT parameters: write results back to caller's variables
6. Manage nested procedure calls (call stack)
7. Handle RETURN statement to exit early

**Files to Modify**:
- `src/sblr/executor.cpp`
- `include/scratchbird/sblr/executor.h` (add call stack)

**Acceptance Criteria**:
```sql
CALL update_salary(1, 50000.00)
-- Updates employee 1's salary to 50000
-- Procedure executes successfully
```

---

### FUNC-001: Parse CREATE FUNCTION Statement
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 1.5 days
- **Dependencies**: PROC-001

**Tasks**:
1. Add `CreateFunctionStatement` AST node (similar to CreateProcedureStatement)
2. Add `return_type` field
3. Parse `CREATE FUNCTION name(params) RETURNS type AS ... END`
4. Validate RETURN statement exists in function body
5. Support DETERMINISTIC clause

**Files to Modify**:
- `src/parser/parser.cpp`
- `include/scratchbird/parser/ast.h`

**Acceptance Criteria**:
```sql
CREATE FUNCTION calculate_bonus(salary DECIMAL(10,2))
RETURNS DECIMAL(10,2)
AS
BEGIN
    RETURN salary * 0.10;
END
-- Parser generates CreateFunctionStatement AST
```

---

### FUNC-002: Generate Bytecode for CREATE FUNCTION
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 1.5 days
- **Dependencies**: FUNC-001

**Tasks**:
1. Add `OP_CREATE_FUNCTION` opcode
2. Generate bytecode similar to CREATE PROCEDURE
3. Include return type metadata
4. Validate RETURN statement compiles correctly

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/opcodes.h`

---

### FUNC-003: Store Functions in Catalog
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Catalog Manager
- **Estimate**: 1 day
- **Dependencies**: FUNC-002, PROC-003

**Tasks**:
1. Use `pg_proc` table (functions and procedures stored together)
2. Set `prorettype` to function's return type (not void)
3. Implement `createFunction()` in CatalogManager
4. Implement `getFunction(name)`

**Files to Modify**:
- `src/core/catalog_manager.cpp`

**Acceptance Criteria**:
```cpp
catalog_mgr->createFunction("calculate_bonus", schema_id, params, return_type, bytecode);
auto func = catalog_mgr->getFunction("calculate_bonus");
assert(func.return_type == TYPE_DECIMAL);
```

---

### FUNC-004: Execute CREATE FUNCTION
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 1 day
- **Dependencies**: FUNC-003

**Tasks**:
1. Implement OP_CREATE_FUNCTION execution
2. Call `catalog_mgr->createFunction()`

**Files to Modify**:
- `src/sblr/executor.cpp`

---

### FUNC-005: Parse Function Calls in Expressions
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 2 days
- **Dependencies**: FUNC-001

**Tasks**:
1. Add `FunctionCallExpression` AST node:
   - `std::string function_name`
   - `std::vector<Expression*> arguments`
2. Parse function calls in SELECT list, WHERE clause, etc.
3. Distinguish between aggregate functions and scalar functions
4. Support built-in functions and user-defined functions

**Files to Modify**:
- `src/parser/parser.cpp`
- `include/scratchbird/parser/ast.h`

**Acceptance Criteria**:
```sql
SELECT id, salary, calculate_bonus(salary) FROM employees
-- Parser recognizes calculate_bonus() as FunctionCallExpression
```

---

### FUNC-006: Generate Bytecode for Function Calls
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 1.5 days
- **Dependencies**: FUNC-005

**Tasks**:
1. Add `OP_CALL_FUNCTION` opcode
2. Look up function in catalog
3. Generate bytecode to:
   - Evaluate argument expressions
   - Push arguments onto stack
   - Emit OP_CALL_FUNCTION with function OID
   - Push return value onto stack

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`

---

### FUNC-007: Execute Function Calls
- **Priority**: CRITICAL
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 2 days
- **Dependencies**: FUNC-006, PROC-007

**Tasks**:
1. Implement OP_CALL_FUNCTION execution
2. Create execution context for function (similar to procedure)
3. Execute function body
4. Capture RETURN value
5. Push return value onto stack for caller
6. Handle nested function calls

**Files to Modify**:
- `src/sblr/executor.cpp`

**Acceptance Criteria**:
```sql
SELECT id, salary, calculate_bonus(salary) FROM employees WHERE id = 1
-- Returns: 1, 50000.00, 5000.00
```

---

### TRIG-001: Parse CREATE TRIGGER Statement
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 2.5 days
- **Dependencies**: PROC-001

**Tasks**:
1. Add `CreateTriggerStatement` AST node:
   - `std::string name`
   - `std::string table_name`
   - `TriggerTiming timing` (BEFORE/AFTER)
   - `std::vector<TriggerEvent> events` (INSERT/UPDATE/DELETE)
   - `std::unique_ptr<Statement> body`
   - `bool for_each_row` (default true in Firebird)
2. Parse `CREATE TRIGGER name FOR table [ACTIVE|INACTIVE] {BEFORE|AFTER} {INSERT|UPDATE|DELETE} AS BEGIN ... END`
3. Support multiple events: `BEFORE INSERT OR UPDATE OR DELETE`
4. Parse AS BEGIN...END trigger body

**Files to Modify**:
- `src/parser/parser.cpp`
- `include/scratchbird/parser/ast.h`

**Acceptance Criteria**:
```sql
CREATE TRIGGER update_timestamp FOR employees
BEFORE UPDATE
AS
BEGIN
    NEW.updated_at = CURRENT_TIMESTAMP;
END
-- Parser generates CreateTriggerStatement AST
```

---

### TRIG-002: Generate Bytecode for CREATE TRIGGER
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 2 days
- **Dependencies**: TRIG-001

**Tasks**:
1. Add `OP_CREATE_TRIGGER` opcode
2. Generate bytecode that stores:
   - Trigger name, table, timing, events
   - Compiled trigger body bytecode
3. Handle NEW and OLD context variables in trigger body

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`

---

### TRIG-003: Store Triggers in Catalog
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Catalog Manager
- **Estimate**: 1.5 days
- **Dependencies**: TRIG-002

**Tasks**:
1. Add `pg_trigger` system table:
   - `oid`, `tgname` (trigger name)
   - `tgrelid` (table OID)
   - `tgtype` (bitmask: timing + events)
   - `tgenabled` (BOOLEAN)
   - `tgfoid` (function OID - for Firebird compatibility, store inline bytecode)
   - `tgsrc` (TEXT, source code)
   - `tgbin` (BYTEA, compiled bytecode)
2. Implement `createTrigger()` in CatalogManager
3. Implement `getTriggersForTable(table_id, timing, event)`

**Files to Modify**:
- `src/core/catalog_manager.cpp`
- `include/scratchbird/core/catalog_manager.h`

---

### TRIG-004: Execute CREATE TRIGGER
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 1 day
- **Dependencies**: TRIG-003

**Tasks**:
1. Implement OP_CREATE_TRIGGER execution
2. Call `catalog_mgr->createTrigger()`

**Files to Modify**:
- `src/sblr/executor.cpp`

---

### TRIG-005: Invoke BEFORE Triggers on INSERT
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Storage Engine
- **Estimate**: 2 days
- **Dependencies**: TRIG-004

**Tasks**:
1. In `storage_engine->insertTuple()`, before inserting:
   - Query `catalog_mgr->getTriggersForTable(table_id, BEFORE, INSERT)`
   - For each trigger:
     - Create execution context with NEW = incoming tuple
     - Execute trigger bytecode
     - Update tuple with modified NEW values
2. Handle trigger errors: rollback insertion

**Files to Modify**:
- `src/core/storage_engine.cpp`

**Acceptance Criteria**:
```sql
INSERT INTO employees (id, name, created_at) VALUES (1, 'Alice', NULL)
-- BEFORE INSERT trigger sets created_at = CURRENT_TIMESTAMP
-- Inserted tuple has created_at populated
```

---

### TRIG-006: Invoke AFTER Triggers on INSERT
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Storage Engine
- **Estimate**: 1.5 days
- **Dependencies**: TRIG-005

**Tasks**:
1. In `storage_engine->insertTuple()`, after successful insertion:
   - Query `catalog_mgr->getTriggersForTable(table_id, AFTER, INSERT)`
   - For each trigger:
     - Create execution context with NEW = inserted tuple
     - Execute trigger bytecode
2. AFTER triggers cannot modify NEW (read-only)

**Files to Modify**:
- `src/core/storage_engine.cpp`

---

### TRIG-007: Invoke Triggers on UPDATE
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Storage Engine
- **Estimate**: 2.5 days
- **Dependencies**: TRIG-005, TRIG-006

**Tasks**:
1. In `storage_engine->updateTuple()`:
   - BEFORE UPDATE: OLD = current tuple, NEW = updated tuple (modifiable)
   - AFTER UPDATE: OLD = original tuple, NEW = updated tuple (read-only)
2. Handle trigger chain: BEFORE can modify NEW, then AFTER sees final NEW

**Files to Modify**:
- `src/core/storage_engine.cpp`

**Acceptance Criteria**:
```sql
UPDATE employees SET salary = 60000 WHERE id = 1
-- BEFORE UPDATE trigger sets updated_at = CURRENT_TIMESTAMP
-- OLD.salary = 50000, NEW.salary = 60000
```

---

### TRIG-008: Invoke Triggers on DELETE
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Storage Engine
- **Estimate**: 1.5 days
- **Dependencies**: TRIG-005

**Tasks**:
1. In `storage_engine->deleteTuple()`:
   - BEFORE DELETE: OLD = tuple being deleted (can prevent deletion by raising error)
   - AFTER DELETE: OLD = deleted tuple
2. No NEW context for DELETE triggers

**Files to Modify**:
- `src/core/storage_engine.cpp`

---

### PROC-008: DROP PROCEDURE Statement
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Full Stack
- **Estimate**: 1.5 days
- **Dependencies**: PROC-004

**Tasks**:
1. Parse `DROP PROCEDURE [IF EXISTS] name`
2. Generate OP_DROP_PROCEDURE bytecode
3. Execute: remove from `pg_proc` table
4. Handle cascading: warn if procedure is called by other procedures

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp`
- `src/core/catalog_manager.cpp`

---

### PROC-009: DROP FUNCTION Statement
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Full Stack
- **Estimate**: 1 day
- **Dependencies**: FUNC-004

**Tasks**:
1. Parse `DROP FUNCTION [IF EXISTS] name`
2. Generate OP_DROP_FUNCTION bytecode
3. Execute: remove from `pg_proc` table

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp`

---

### PROC-010: DROP TRIGGER Statement
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Full Stack
- **Estimate**: 1 day
- **Dependencies**: TRIG-004

**Tasks**:
1. Parse `DROP TRIGGER [IF EXISTS] name`
2. Generate OP_DROP_TRIGGER bytecode
3. Execute: remove from `pg_trigger` table

**Files to Modify**:
- `src/parser/parser.cpp`
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/executor.cpp`

---

**Phase 7 Summary**:
- **Total Effort**: 27.5 days (5.5 weeks)
- **Critical Features**: 14
- **High Features**: 6
- **Medium Features**: 2
- **Deliverables**:
  - Full stored procedure support (CREATE, CALL, DROP)
  - Full function support (CREATE, inline calls, DROP)
  - Full trigger support (CREATE, BEFORE/AFTER, INSERT/UPDATE/DELETE, DROP)
  - System tables: `pg_proc`, `pg_trigger`

---

## PHASE 8: Advanced Features - CTEs & Window Functions (25 days, 5 weeks)

**Goal**: Implement Common Table Expressions (CTEs) and Window Functions for advanced querying

**Total Features**: 12
**Dependencies**: Phase 2 (Aggregation), Phase 3 (Subqueries)

---

### CTE-001: Parse WITH Clause (Non-Recursive CTEs)
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 2.5 days
- **Dependencies**: None

**Tasks**:
1. Add `WithClause` AST node:
   - `std::vector<CommonTableExpression> ctes`
   - `std::unique_ptr<SelectStatement> main_query`
2. Add `CommonTableExpression` node:
   - `std::string name`
   - `std::vector<std::string> column_names` (optional)
   - `std::unique_ptr<SelectStatement> query`
   - `bool recursive`
3. Parse `WITH cte_name AS (SELECT ...) SELECT ... FROM cte_name`
4. Support multiple CTEs: `WITH cte1 AS (...), cte2 AS (...) SELECT ...`
5. Support column name list: `WITH cte(col1, col2) AS (...)`

**Files to Modify**:
- `src/parser/parser.cpp`
- `include/scratchbird/parser/ast.h`

**Acceptance Criteria**:
```sql
WITH regional_sales AS (
    SELECT region, SUM(amount) as total
    FROM orders
    GROUP BY region
)
SELECT * FROM regional_sales WHERE total > 1000

-- Parser generates WithClause with 1 CTE
```

---

### CTE-002: Generate Bytecode for Non-Recursive CTEs
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 2.5 days
- **Dependencies**: CTE-001

**Tasks**:
1. Add `OP_CREATE_TEMP_TABLE` opcode for materializing CTE
2. For each CTE:
   - Generate bytecode for CTE query
   - Emit OP_CREATE_TEMP_TABLE to store results
   - Register CTE name in temporary table registry
3. Generate bytecode for main query referencing CTE as a table
4. Add `OP_DROP_TEMP_TABLE` after query execution

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/opcodes.h`

---

### CTE-003: Execute Non-Recursive CTEs
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 2.5 days
- **Dependencies**: CTE-002

**Tasks**:
1. Implement OP_CREATE_TEMP_TABLE:
   - Execute CTE query
   - Store results in temporary in-memory table
   - Register temp table in executor context
2. When main query references CTE, scan from temp table
3. Implement OP_DROP_TEMP_TABLE to clean up

**Files to Modify**:
- `src/sblr/executor.cpp`
- `include/scratchbird/sblr/executor.h` (add temp table registry)

**Acceptance Criteria**:
```sql
WITH regional_sales AS (
    SELECT region, SUM(amount) as total
    FROM orders
    GROUP BY region
)
SELECT * FROM regional_sales WHERE total > 1000
-- Executes correctly, returns matching rows
```

---

### CTE-004: Parse WITH RECURSIVE Clause
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 2 days
- **Dependencies**: CTE-001

**Tasks**:
1. Extend `CommonTableExpression` to support:
   - `recursive` flag
   - Detect UNION ALL pattern: `anchor UNION ALL recursive_term`
2. Parse `WITH RECURSIVE cte AS (anchor UNION ALL recursive) SELECT ...`
3. Validate recursive CTE structure: must have UNION ALL with self-reference

**Files to Modify**:
- `src/parser/parser.cpp`

**Acceptance Criteria**:
```sql
WITH RECURSIVE subordinates AS (
    SELECT id, name, manager_id FROM employees WHERE id = 1
    UNION ALL
    SELECT e.id, e.name, e.manager_id
    FROM employees e
    JOIN subordinates s ON e.manager_id = s.id
)
SELECT * FROM subordinates
-- Parser recognizes recursive CTE
```

---

### CTE-005: Generate Bytecode for WITH RECURSIVE
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 3 days
- **Dependencies**: CTE-004

**Tasks**:
1. Add `OP_RECURSIVE_CTE` opcode
2. Generate bytecode:
   - Execute anchor query → initial working table
   - Loop:
     - Execute recursive term with working table → delta
     - If delta is empty, break
     - Append delta to working table
   - Working table becomes final CTE result
3. Handle cycle detection (optional): track visited tuples

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`

---

### CTE-006: Execute WITH RECURSIVE
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 3.5 days
- **Dependencies**: CTE-005

**Tasks**:
1. Implement OP_RECURSIVE_CTE:
   - Execute anchor query → working table W₀
   - Iteration i = 1, 2, ...:
     - Execute recursive term with W_{i-1} as CTE reference
     - Delta Δᵢ = result
     - If Δᵢ is empty, stop
     - W_i = W_{i-1} ∪ Δᵢ
   - Final result = W_n
2. Implement cycle detection: abort if same tuple appears twice (optional CYCLE clause)
3. Limit recursion depth (e.g., max 1000 iterations)

**Files to Modify**:
- `src/sblr/executor.cpp`

**Acceptance Criteria**:
```sql
WITH RECURSIVE subordinates AS (
    SELECT id, name, manager_id FROM employees WHERE id = 1
    UNION ALL
    SELECT e.id, e.name, e.manager_id
    FROM employees e
    JOIN subordinates s ON e.manager_id = s.id
)
SELECT * FROM subordinates
-- Returns entire hierarchy starting from employee 1
```

---

### WIN-001: Parse OVER Clause for Window Functions
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Parser
- **Estimate**: 2.5 days
- **Dependencies**: None

**Tasks**:
1. Add `WindowSpecification` AST node:
   - `std::vector<Expression*> partition_by`
   - `std::vector<OrderByClause> order_by`
   - `FrameSpecification frame` (ROWS/RANGE, BETWEEN ... AND ...)
2. Add `WindowFunctionCall` AST node extending FunctionCallExpression:
   - `WindowSpecification over_clause`
3. Parse `ROW_NUMBER() OVER (PARTITION BY col1 ORDER BY col2)`
4. Parse frame clause: `ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW`

**Files to Modify**:
- `src/parser/parser.cpp`
- `include/scratchbird/parser/ast.h`

**Acceptance Criteria**:
```sql
SELECT
    id,
    salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) as rank
FROM employees
-- Parser generates WindowFunctionCall with OVER clause
```

---

### WIN-002: Generate Bytecode for Window Functions
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Bytecode Generator
- **Estimate**: 3 days
- **Dependencies**: WIN-001

**Tasks**:
1. Add opcodes:
   - `OP_WINDOW_PARTITION` - partition input by PARTITION BY keys
   - `OP_WINDOW_SORT` - sort each partition by ORDER BY
   - `OP_WINDOW_FUNCTION` - apply window function to each partition
2. Generate bytecode:
   - Emit OP_WINDOW_PARTITION with partition keys
   - Emit OP_WINDOW_SORT with sort keys
   - Emit OP_WINDOW_FUNCTION with function type (ROW_NUMBER, RANK, etc.)

**Files to Modify**:
- `src/sblr/bytecode_generator.cpp`
- `src/sblr/opcodes.h`

---

### WIN-003: Execute Window Functions
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor
- **Estimate**: 4 days
- **Dependencies**: WIN-002

**Tasks**:
1. Implement window function executor:
   - OP_WINDOW_PARTITION: hash-partition input by partition keys
   - OP_WINDOW_SORT: sort each partition by order keys
   - OP_WINDOW_FUNCTION:
     - For each partition:
       - For each row (in sorted order):
         - Compute window function value based on frame
         - Append result column to row
2. Implement window frame logic:
   - ROWS: physical row offset
   - RANGE: logical value offset
   - UNBOUNDED PRECEDING, N PRECEDING, CURRENT ROW, N FOLLOWING, UNBOUNDED FOLLOWING
3. Optimize: reuse sorted data if multiple window functions have same PARTITION BY/ORDER BY

**Files to Modify**:
- `src/sblr/executor.cpp`
- `src/sblr/window_executor.cpp` (new file)
- `include/scratchbird/sblr/window_executor.h` (new file)

**Acceptance Criteria**:
```sql
SELECT
    id,
    salary,
    ROW_NUMBER() OVER (PARTITION BY department ORDER BY salary DESC) as rank
FROM employees
-- Returns each employee with rank within their department
```

---

### WIN-004: Implement ROW_NUMBER(), RANK(), DENSE_RANK()
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Window Functions)
- **Estimate**: 1.5 days
- **Dependencies**: WIN-003

**Tasks**:
1. `ROW_NUMBER()`: sequential number starting at 1 for each partition
2. `RANK()`: rank with gaps (1, 1, 3, 4, ...)
3. `DENSE_RANK()`: rank without gaps (1, 1, 2, 3, ...)
4. Handle ORDER BY ties correctly

**Files to Modify**:
- `src/sblr/window_executor.cpp`

---

### WIN-005: Implement LEAD(), LAG()
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Executor (Window Functions)
- **Estimate**: 1.5 days
- **Dependencies**: WIN-003

**Tasks**:
1. `LEAD(expr, offset, default)`: access row at offset rows after current
2. `LAG(expr, offset, default)`: access row at offset rows before current
3. Default offset = 1, default value = NULL
4. Handle out-of-bounds: return default value

**Files to Modify**:
- `src/sblr/window_executor.cpp`

**Acceptance Criteria**:
```sql
SELECT
    date,
    amount,
    LAG(amount, 1, 0) OVER (ORDER BY date) as prev_amount
FROM sales
-- Shows previous day's amount for each row
```

---

### WIN-006: Implement FIRST_VALUE(), LAST_VALUE(), NTH_VALUE()
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Executor (Window Functions)
- **Estimate**: 1.5 days
- **Dependencies**: WIN-003

**Tasks**:
1. `FIRST_VALUE(expr)`: first value in window frame
2. `LAST_VALUE(expr)`: last value in window frame
3. `NTH_VALUE(expr, n)`: n-th value in window frame (1-indexed)
4. Respect frame specification (default: RANGE UNBOUNDED PRECEDING to CURRENT ROW)

**Files to Modify**:
- `src/sblr/window_executor.cpp`

---

**Phase 8 Summary**:
- **Total Effort**: 25 days (5 weeks)
- **High Features**: 9
- **Medium Features**: 3
- **Deliverables**:
  - Non-recursive CTEs (WITH clause)
  - Recursive CTEs (WITH RECURSIVE for hierarchical queries)
  - Window functions: ROW_NUMBER, RANK, DENSE_RANK, LEAD, LAG, FIRST_VALUE, LAST_VALUE, NTH_VALUE
  - Frame specifications: ROWS/RANGE, BETWEEN ... AND ...

---

## PHASE 9: Built-in Functions (17.5 days, 3.5 weeks)

**Goal**: Implement comprehensive set of Firebird-compatible built-in functions

**Total Features**: 35 (grouped by category)
**Dependencies**: Phase 1 (Core DML for testing)

---

### FUNC-100: Math Functions - Basic
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 1.5 days
- **Dependencies**: None

**Tasks**:
1. Implement functions:
   - `ABS(x)` - absolute value
   - `SIGN(x)` - -1, 0, or 1
   - `CEIL(x)` / `CEILING(x)` - round up
   - `FLOOR(x)` - round down
   - `ROUND(x, n)` - round to n decimal places
   - `TRUNC(x, n)` - truncate to n decimal places
2. Add function registry: map function name → implementation
3. Integrate with OP_CALL_FUNCTION executor

**Files to Modify**:
- `src/sblr/builtin_functions.cpp` (new)
- `include/scratchbird/sblr/builtin_functions.h` (new)
- `src/sblr/executor.cpp` (integrate function registry)

**Acceptance Criteria**:
```sql
SELECT ABS(-5), SIGN(-10), CEIL(4.3), FLOOR(4.8), ROUND(3.14159, 2)
-- Returns: 5, -1, 5, 4, 3.14
```

---

### FUNC-101: Math Functions - Trigonometry & Advanced
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 1.5 days
- **Dependencies**: FUNC-100

**Tasks**:
1. Implement:
   - `SQRT(x)` - square root
   - `POWER(x, y)` - x raised to y
   - `EXP(x)` - e^x
   - `LN(x)` - natural logarithm
   - `LOG(base, x)` - logarithm base b
   - `LOG10(x)` - base 10 logarithm
   - `SIN(x)`, `COS(x)`, `TAN(x)` - trigonometric (radians)
   - `ASIN(x)`, `ACOS(x)`, `ATAN(x)` - inverse trigonometric
   - `ATAN2(y, x)` - atan(y/x) with sign
   - `PI()` - constant π
2. Use C++ `<cmath>` for implementations

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

---

### FUNC-102: String Functions - Basic
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 2 days
- **Dependencies**: None

**Tasks**:
1. Implement:
   - `UPPER(s)` / `UCASE(s)` - uppercase
   - `LOWER(s)` / `LCASE(s)` - lowercase
   - `CHAR_LENGTH(s)` / `CHARACTER_LENGTH(s)` - character count
   - `BIT_LENGTH(s)` - bit count
   - `OCTET_LENGTH(s)` - byte count
   - `TRIM([LEADING|TRAILING|BOTH] [char FROM] s)` - remove whitespace/char
   - `LTRIM(s)`, `RTRIM(s)` - left/right trim
2. Handle UTF-8 correctly for character vs. byte length

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

**Acceptance Criteria**:
```sql
SELECT UPPER('hello'), LOWER('WORLD'), CHAR_LENGTH('café'), OCTET_LENGTH('café')
-- Returns: 'HELLO', 'world', 4, 5 (UTF-8: é = 2 bytes)
```

---

### FUNC-103: String Functions - Substring & Position
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 1.5 days
- **Dependencies**: FUNC-102

**Tasks**:
1. Implement:
   - `SUBSTRING(s FROM pos [FOR len])` - extract substring
   - `LEFT(s, n)` - first n characters
   - `RIGHT(s, n)` - last n characters
   - `POSITION(substr IN str)` - find position (1-indexed, 0 if not found)
   - `OVERLAY(s PLACING new FROM pos [FOR len])` - replace substring
2. Support 1-indexed positions (Firebird convention)

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

**Acceptance Criteria**:
```sql
SELECT SUBSTRING('hello' FROM 2 FOR 3), POSITION('lo' IN 'hello')
-- Returns: 'ell', 4
```

---

### FUNC-104: String Functions - Padding & Manipulation
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 1.5 days
- **Dependencies**: FUNC-102

**Tasks**:
1. Implement:
   - `LPAD(s, len [, pad])` - left pad to length
   - `RPAD(s, len [, pad])` - right pad to length
   - `REVERSE(s)` - reverse string
   - `REPLACE(s, find, replace)` - replace all occurrences
   - `REPEAT(s, n)` - repeat string n times
   - `SPACE(n)` - n spaces

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

---

### FUNC-105: String Functions - Concatenation
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Parser & Executor
- **Estimate**: 1 day
- **Dependencies**: None

**Tasks**:
1. Parse `||` operator for string concatenation
2. Implement `CONCAT(s1, s2, ...)` function (variadic)
3. Handle NULL: `s || NULL = NULL` (Firebird behavior)

**Files to Modify**:
- `src/parser/parser.cpp` (parse ||)
- `src/sblr/builtin_functions.cpp`

**Acceptance Criteria**:
```sql
SELECT 'Hello' || ' ' || 'World', CONCAT('a', 'b', 'c')
-- Returns: 'Hello World', 'abc'
```

---

### FUNC-106: Date/Time Functions - Current Values
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 1 day
- **Dependencies**: None

**Tasks**:
1. Implement:
   - `CURRENT_DATE` - current date
   - `CURRENT_TIME` - current time
   - `CURRENT_TIMESTAMP` - current date + time
   - `NOW()` - alias for CURRENT_TIMESTAMP
   - `LOCALTIME` - local time (no timezone)
   - `LOCALTIMESTAMP` - local timestamp
2. Use system clock for values

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

---

### FUNC-107: Date/Time Functions - Extraction
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 2 days
- **Dependencies**: FUNC-106

**Tasks**:
1. Implement `EXTRACT(part FROM datetime)`:
   - Parts: YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, MILLISECOND
   - Also: WEEK, QUARTER, DAY_OF_WEEK, DAY_OF_YEAR
2. Implement shortcuts:
   - `YEAR(date)`, `MONTH(date)`, `DAY(date)`
   - `HOUR(time)`, `MINUTE(time)`, `SECOND(time)`

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

**Acceptance Criteria**:
```sql
SELECT EXTRACT(YEAR FROM CURRENT_DATE), MONTH(CURRENT_DATE)
-- Returns: 2025, 10
```

---

### FUNC-108: Date/Time Functions - Arithmetic
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 2 days
- **Dependencies**: FUNC-106

**Tasks**:
1. Implement:
   - `DATEADD(amount unit TO datetime)` - add interval
     - Units: YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, MILLISECOND
   - `DATEDIFF(unit, start, end)` - difference between dates
   - `DATE_TRUNC(unit, datetime)` - truncate to unit (start of year/month/day/etc.)
2. Handle edge cases: month overflow (Jan 31 + 1 month = Feb 28/29)

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

**Acceptance Criteria**:
```sql
SELECT DATEADD(1 MONTH TO DATE '2025-01-31'), DATEDIFF(DAY, DATE '2025-01-01', DATE '2025-12-31')
-- Returns: '2025-02-28', 364
```

---

### FUNC-109: Conditional Functions
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 1.5 days
- **Dependencies**: None

**Tasks**:
1. Implement:
   - `COALESCE(val1, val2, ...)` - first non-NULL value
   - `NULLIF(val1, val2)` - NULL if equal, else val1
   - `IIF(condition, true_val, false_val)` - inline IF
   - `DECODE(expr, match1, result1, match2, result2, ..., default)` - multi-way branch
2. Handle short-circuit evaluation for IIF

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`

**Acceptance Criteria**:
```sql
SELECT COALESCE(NULL, NULL, 'default'), IIF(1 > 0, 'yes', 'no')
-- Returns: 'default', 'yes'
```

---

### FUNC-110: Conversion Functions
- **Priority**: HIGH
- **Status**: Not Started
- **Component**: Executor (Built-in Functions)
- **Estimate**: 2 days
- **Dependencies**: None

**Tasks**:
1. Implement:
   - `CAST(expr AS type)` - explicit type conversion
   - `CONVERT(type, expr)` - alternative syntax
2. Support conversions:
   - Numeric ↔ VARCHAR
   - DATE/TIME/TIMESTAMP ↔ VARCHAR (format: 'YYYY-MM-DD', 'HH:MI:SS', etc.)
   - Numeric ↔ Numeric (INT → DECIMAL, etc.)
3. Handle conversion errors: raise error or return NULL based on strictness setting

**Files to Modify**:
- `src/sblr/builtin_functions.cpp`
- `src/parser/parser.cpp` (parse CAST)

**Acceptance Criteria**:
```sql
SELECT CAST('123' AS INTEGER), CAST(CURRENT_DATE AS VARCHAR(10))
-- Returns: 123, '2025-10-16'
```

---

### FUNC-111: Aggregate Functions - Additional
- **Priority**: MEDIUM
- **Status**: Not Started
- **Component**: Executor (Aggregation)
- **Estimate**: 1 day
- **Dependencies**: Phase 2 (AGG-001 through AGG-005)

**Tasks**:
1. Already implemented: COUNT, SUM, AVG, MIN, MAX
2. Add:
   - `STDDEV(expr)` / `STDDEV_SAMP(expr)` - standard deviation (sample)
   - `STDDEV_POP(expr)` - standard deviation (population)
   - `VARIANCE(expr)` / `VAR_SAMP(expr)` - variance (sample)
   - `VAR_POP(expr)` - variance (population)
3. Implement using Welford's online algorithm for numerical stability

**Files to Modify**:
- `src/sblr/executor.cpp` (aggregation)

---

### FUNC-112: List Aggregation (Firebird 2.1+)
- **Priority**: LOW
- **Status**: Not Started
- **Component**: Executor (Aggregation)
- **Estimate**: 1.5 days
- **Dependencies**: Phase 2

**Tasks**:
1. Implement:
   - `LIST(expr [, separator])` - concatenate strings from group
   - Default separator: ','
   - NULL values ignored
2. Example: `SELECT department, LIST(name, '; ') FROM employees GROUP BY department`
   - Returns: 'Engineering', 'Alice; Bob; Charlie'

**Files to Modify**:
- `src/sblr/executor.cpp`

---

**Phase 9 Summary**:
- **Total Effort**: 17.5 days (3.5 weeks)
- **High Features**: 11
- **Medium Features**: 5
- **Low Features**: 1
- **Deliverables**:
  - Math functions: ABS, SIGN, CEIL, FLOOR, ROUND, SQRT, POWER, trigonometry
  - String functions: UPPER, LOWER, SUBSTRING, POSITION, TRIM, LPAD, RPAD, REPLACE, concatenation
  - Date/Time functions: CURRENT_TIMESTAMP, EXTRACT, DATEADD, DATEDIFF, DATE_TRUNC
  - Conditional functions: COALESCE, NULLIF, IIF, DECODE
  - Conversion: CAST, CONVERT
  - Aggregate: STDDEV, VARIANCE, LIST
  - Built-in function registry and execution framework

---

**End of Part 2**

**Next**: ALPHA_COMPLETION_DETAILED_TODO_PART3.md (Phases 10-12)

---

**Document Version**: 1.0
**Last Updated**: October 16, 2025
**Total Phases 7-9 Effort**: 70 days (14 weeks)
