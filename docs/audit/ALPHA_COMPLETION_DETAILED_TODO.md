# SCRATCHBIRD ALPHA COMPLETION - DETAILED TODO

**Project**: ScratchBird Database Engine
**Version**: Alpha 1.2 → Alpha 2.0 (Feature Complete)
**Total Features**: 227
**Last Updated**: October 16, 2025

---

## DOCUMENT ORGANIZATION

This document lists ALL 227 features required for Alpha completion, organized by phase. Each feature includes:

- **Feature ID** - Unique identifier (e.g., DML-001)
- **Priority** - CRITICAL / HIGH / MEDIUM / LOW
- **Status** - ❌ Not Started / 🟡 In Progress / ✅ Complete
- **Component** - Parser / Bytecode / Executor / Catalog / Other
- **Estimate** - Days of effort
- **Dependencies** - Which features must be completed first
- **Files** - Which files need modification
- **Acceptance Criteria** - How to verify completion

**Total Estimated Effort**: 262.5 developer-days (52.5 weeks)

---

## PHASE 1: CORE DML EXECUTION (30 days)

### UPDATE Statement

**DML-001: Parse UPDATE Statement**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `UpdateStatement` AST node with table name, SET clauses, WHERE clause
  2. Parse `UPDATE table SET col1 = expr1, col2 = expr2 WHERE condition`
  3. Support table aliases: `UPDATE table AS t SET t.col = expr WHERE t.id = 123`
  4. Add parser tests for all variants
- Acceptance: Parser successfully generates UpdateStatement AST for valid SQL

**DML-002: Generate UPDATE Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: DML-001
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_UPDATE` opcode to opcodes.h
  2. Generate bytecode sequence: LOAD_TABLE → SCAN → EVALUATE_WHERE → UPDATE_TUPLE → NEXT
  3. Compile SET expressions into bytecode
  4. Handle multiple SET clauses
- Acceptance: Bytecode generator produces valid bytecode for UPDATE statements

**DML-003: Execute UPDATE Statement**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 3 days
- Dependencies: DML-002
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_UPDATE opcode execution
  2. Scan table with WHERE filter (reuse SELECT scan logic)
  3. For each matching row:
     a. Load tuple
     b. Check MVCC visibility
     c. Evaluate SET expressions
     d. Call storage_engine->updateTuple() with new values
  4. Update row count (affected rows)
  5. Handle RETURNING clause (if implemented)
  6. Add executor tests
- Acceptance: `UPDATE employees SET salary = 50000 WHERE id = 1` updates 1 row, verifiable with SELECT

### DELETE Statement

**DML-004: Parse DELETE Statement**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `DeleteStatement` AST node with table name, WHERE clause
  2. Parse `DELETE FROM table WHERE condition`
  3. Support table aliases
  4. Add parser tests
- Acceptance: Parser generates DeleteStatement AST

**DML-005: Generate DELETE Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: DML-004
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_DELETE` opcode
  2. Generate bytecode: LOAD_TABLE → SCAN → EVALUATE_WHERE → DELETE_TUPLE → NEXT
- Acceptance: Bytecode generated for DELETE statements

**DML-006: Execute DELETE Statement**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: DML-005
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_DELETE execution
  2. Scan table with WHERE filter
  3. For each matching row:
     a. Check MVCC visibility
     b. Call storage_engine->deleteTuple() (sets xmax)
  4. Update row count
  5. Handle RETURNING clause (if implemented)
- Acceptance: `DELETE FROM employees WHERE id = 1` deletes 1 row

### INNER JOIN

**DML-007: Parse INNER JOIN**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 3 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend `SelectStatement` AST with `JoinClause` list
  2. Parse `FROM table1 INNER JOIN table2 ON condition`
  3. Parse `FROM table1 JOIN table2 ON condition` (INNER is default)
  4. Support multiple joins: `FROM t1 JOIN t2 ON c1 JOIN t3 ON c2`
  5. Parse `FROM table1, table2 WHERE table1.id = table2.fk` (implicit join)
  6. Add parser tests
- Acceptance: Parser generates SelectStatement with JoinClause nodes

**DML-008: Generate JOIN Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 2 days
- Dependencies: DML-007
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_NESTED_LOOP_JOIN` opcode
  2. Generate bytecode for join plan:
     - LOAD_TABLE (outer)
     - SCAN_OUTER_BEGIN
     - LOAD_TABLE (inner)
     - SCAN_INNER_BEGIN
     - EVALUATE_JOIN_CONDITION
     - YIELD_ROW (if condition true)
     - SCAN_INNER_END
     - SCAN_OUTER_END
  3. Handle multiple joins (nested loops)
- Acceptance: Bytecode generated for INNER JOIN queries

**DML-009: Execute INNER JOIN**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 4 days
- Dependencies: DML-008
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_NESTED_LOOP_JOIN execution
  2. Nested loop algorithm:
     ```
     for each row in outer_table:
       for each row in inner_table:
         if join_condition(outer_row, inner_row):
           yield combined_row
     ```
  3. Build combined row from both tables (column name resolution)
  4. Handle table aliases in column references
  5. Add executor tests for 2-way and 3-way joins
- Acceptance: `SELECT * FROM orders o JOIN customers c ON o.customer_id = c.id` returns correct results

### LEFT JOIN

**DML-010: Parse LEFT JOIN**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: DML-007
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend JoinClause with `join_type` field (INNER / LEFT / RIGHT / FULL)
  2. Parse `LEFT JOIN`, `LEFT OUTER JOIN`
  3. Add parser tests
- Acceptance: Parser distinguishes LEFT JOIN from INNER JOIN in AST

**DML-011: Generate LEFT JOIN Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: DML-010, DML-008
- Files: `src/sblr/bytecode_generator.cpp`
- Tasks:
  1. Extend OP_NESTED_LOOP_JOIN with flags for join type
  2. Generate null-generation instructions for non-matching rows
- Acceptance: Bytecode includes null-generation for LEFT JOIN

**DML-012: Execute LEFT JOIN**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: DML-011, DML-009
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Extend OP_NESTED_LOOP_JOIN execution for LEFT JOIN
  2. Track whether inner table had a match
  3. If no match, yield outer row + NULL values for inner columns
  4. Add executor tests
- Acceptance: `SELECT * FROM orders o LEFT JOIN customers c ON o.customer_id = c.id` includes orders with no customer

### ORDER BY

**DML-013: Parse ORDER BY**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `OrderByClause` list to SelectStatement AST
  2. Parse `ORDER BY col1 [ASC|DESC] [NULLS FIRST|LAST], col2 ...`
  3. Support expressions in ORDER BY: `ORDER BY salary * 1.1 DESC`
  4. Add parser tests
- Acceptance: Parser generates OrderByClause nodes

**DML-014: Generate ORDER BY Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: DML-013
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_SORT` opcode
  2. Generate sort specification (columns, directions, null ordering)
- Acceptance: Bytecode includes sort instructions

**DML-015: Execute ORDER BY**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 3 days
- Dependencies: DML-014
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_SORT execution
  2. Collect all result rows into vector
  3. Implement multi-column comparator with:
     a. ASC/DESC handling
     b. NULLS FIRST/LAST handling
     c. Collation-aware string comparison
  4. Sort using std::sort with custom comparator
  5. Yield sorted rows
  6. Add executor tests
- Acceptance: `SELECT * FROM employees ORDER BY salary DESC, name ASC` returns sorted results

### LIMIT / OFFSET

**DML-016: Parse LIMIT/OFFSET**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `limit_count` and `offset_count` to SelectStatement AST
  2. Parse `LIMIT n` and `OFFSET m`
  3. Parse Firebird syntax: `ROWS n` or `ROWS n TO m`
  4. Add parser tests
- Acceptance: Parser captures LIMIT and OFFSET values

**DML-017: Generate LIMIT/OFFSET Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 0.5 days
- Dependencies: DML-016
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_LIMIT` opcode with count and offset parameters
- Acceptance: Bytecode includes LIMIT opcode

**DML-018: Execute LIMIT/OFFSET**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: DML-017
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_LIMIT execution
  2. Skip first `offset` rows
  3. Return up to `limit` rows
  4. Stop execution early if limit reached
  5. Add executor tests
- Acceptance: `SELECT * FROM employees LIMIT 10 OFFSET 20` returns rows 21-30

---

## PHASE 2: AGGREGATION & GROUPING (17.5 days)

### GROUP BY

**AGG-001: Parse GROUP BY**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `GroupByClause` list to SelectStatement AST
  2. Parse `GROUP BY col1, col2, ...`
  3. Parse `GROUP BY expr` (expression grouping)
  4. Validate: non-aggregated columns in SELECT must appear in GROUP BY
  5. Add parser tests
- Acceptance: Parser generates GroupByClause nodes

**AGG-002: Generate GROUP BY Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 2 days
- Dependencies: AGG-001
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_GROUP_BY` opcode
  2. Generate grouping key evaluation bytecode
  3. Generate aggregate function accumulation bytecode
- Acceptance: Bytecode includes GROUP BY instructions

**AGG-003: Execute GROUP BY**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 5 days
- Dependencies: AGG-002
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_GROUP_BY execution
  2. Hash-based grouping algorithm:
     ```
     hash_map<group_key, aggregate_state> groups;
     for each row:
       key = evaluate_group_key(row)
       if key not in groups:
         groups[key] = init_aggregates()
       update_aggregates(groups[key], row)
     for each (key, agg) in groups:
       yield result_row(key, agg)
     ```
  3. Implement aggregate state management (sum accumulators, count, min/max values, etc.)
  4. Handle null values in grouping keys
  5. Add executor tests (single and multi-column grouping)
- Acceptance: `SELECT department, COUNT(*) FROM employees GROUP BY department` returns correct counts

### Aggregate Functions

**AGG-004: Implement SUM() Execution**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: AGG-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Extend aggregate state with sum accumulators
  2. Initialize sum = 0
  3. For each row: sum += value (handling nulls)
  4. Return final sum
  5. Support INT, BIGINT, DECIMAL, FLOAT types
- Acceptance: `SELECT SUM(salary) FROM employees` returns correct sum

**AGG-005: Implement AVG() Execution**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 0.5 days
- Dependencies: AGG-004
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Track sum and count
  2. Return sum / count (handle division by zero)
- Acceptance: `SELECT AVG(salary) FROM employees` returns correct average

**AGG-006: Implement MIN() and MAX() Execution**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 0.5 days
- Dependencies: AGG-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Track current min/max value
  2. Update on each row
  3. Handle nulls (ignore in comparison)
  4. Support all comparable types
- Acceptance: `SELECT MIN(salary), MAX(salary) FROM employees` returns correct values

**AGG-007: Implement COUNT() Execution**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 0.5 days
- Dependencies: AGG-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. COUNT(*) - count all rows
  2. COUNT(expr) - count non-null values
  3. COUNT(DISTINCT expr) - count unique non-null values (requires hash set)
- Acceptance: `SELECT COUNT(*), COUNT(manager_id), COUNT(DISTINCT department) FROM employees` returns correct counts

### HAVING Clause

**AGG-008: Parse HAVING**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: AGG-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `having_clause` to SelectStatement AST
  2. Parse `HAVING condition` (after GROUP BY)
  3. Validate: condition can reference aggregates and grouped columns only
  4. Add parser tests
- Acceptance: Parser generates HAVING clause

**AGG-009: Generate HAVING Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 0.5 days
- Dependencies: AGG-008, AGG-002
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_HAVING` opcode (filter after aggregation)
  2. Generate condition evaluation bytecode
- Acceptance: Bytecode includes HAVING filter

**AGG-010: Execute HAVING**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: AGG-009, AGG-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_HAVING execution
  2. After aggregation, evaluate HAVING condition on each group
  3. Yield only groups where condition is true
- Acceptance: `SELECT department, COUNT(*) FROM employees GROUP BY department HAVING COUNT(*) > 10` filters correctly

### DISTINCT

**AGG-011: Parse DISTINCT**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 0.5 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `distinct` boolean to SelectStatement AST
  2. Parse `SELECT DISTINCT ...`
  3. Add parser tests
- Acceptance: Parser sets distinct flag

**AGG-012: Generate DISTINCT Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 0.5 days
- Dependencies: AGG-011
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_DISTINCT` opcode
- Acceptance: Bytecode includes DISTINCT opcode

**AGG-013: Execute DISTINCT**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: AGG-012
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_DISTINCT execution
  2. Hash-based deduplication:
     ```
     hash_set<row_hash> seen;
     for each row:
       hash = compute_row_hash(row)
       if hash not in seen:
         seen.insert(hash)
         yield row
     ```
  3. Handle null values in row hashing
  4. Support all column types in hash computation
- Acceptance: `SELECT DISTINCT department FROM employees` returns unique departments

---

## PHASE 3: SUBQUERIES & SET OPERATIONS (20 days)

### Scalar Subqueries

**SUB-001: Parse Scalar Subqueries**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend `Expression` AST with `SubqueryExpression` variant
  2. Parse `(SELECT expr FROM table WHERE condition)` as expression
  3. Validate subquery returns single column
  4. Support in SELECT list, WHERE, and SET clauses
  5. Add parser tests
- Acceptance: Parser generates SubqueryExpression nodes

**SUB-002: Generate Scalar Subquery Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: SUB-001
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_SUBQUERY_SCALAR` opcode
  2. Generate subquery bytecode sequence
  3. Handle subquery result as expression value
- Acceptance: Bytecode includes subquery execution

**SUB-003: Execute Scalar Subqueries**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: SUB-002
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_SUBQUERY_SCALAR execution
  2. Execute subquery in current context
  3. Verify exactly 1 row returned (error if 0 or >1)
  4. Return single value
  5. Handle correlated subqueries (reference outer query columns)
- Acceptance: `SELECT name, (SELECT COUNT(*) FROM orders WHERE customer_id = c.id) FROM customers c` works

### IN Subqueries

**SUB-004: Parse IN Subqueries**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: SUB-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend `InExpression` to support subquery as right-hand side
  2. Parse `WHERE col IN (SELECT ... FROM ...)`
  3. Add parser tests
- Acceptance: Parser generates InExpression with subquery

**SUB-005: Generate IN Subquery Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: SUB-004
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_IN_SUBQUERY` opcode
  2. Generate subquery execution → build hash set → membership test
- Acceptance: Bytecode includes IN subquery logic

**SUB-006: Execute IN Subqueries**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: SUB-005
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_IN_SUBQUERY execution
  2. Execute subquery, collect all values into hash set
  3. Check if left-hand value is in set
  4. Return boolean result
  5. Handle nulls correctly (SQL three-valued logic)
- Acceptance: `SELECT * FROM customers WHERE id IN (SELECT customer_id FROM orders)` works

### EXISTS Subqueries

**SUB-007: Parse EXISTS Subqueries**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: SUB-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `ExistsExpression` AST node
  2. Parse `WHERE EXISTS (SELECT ... FROM ...)`
  3. Parse `WHERE NOT EXISTS (...)`
  4. Add parser tests
- Acceptance: Parser generates ExistsExpression

**SUB-008: Generate EXISTS Subquery Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 0.5 days
- Dependencies: SUB-007
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_EXISTS_SUBQUERY` opcode
- Acceptance: Bytecode generated

**SUB-009: Execute EXISTS Subqueries**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: SUB-008
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_EXISTS_SUBQUERY execution
  2. Execute subquery
  3. Return TRUE if subquery returns any row, FALSE otherwise
  4. Optimize: stop execution after first row found
- Acceptance: `SELECT * FROM customers c WHERE EXISTS (SELECT 1 FROM orders WHERE customer_id = c.id)` works

### UNION / INTERSECT / EXCEPT

**SET-001: Parse UNION**
- Priority: HIGH
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend SelectStatement to support set operations
  2. Parse `SELECT ... UNION [ALL] SELECT ...`
  3. Parse `SELECT ... INTERSECT [ALL] SELECT ...`
  4. Parse `SELECT ... EXCEPT [ALL] SELECT ...`
  5. Validate: both sides have same number of columns with compatible types
  6. Support chaining: `SELECT ... UNION SELECT ... INTERSECT SELECT ...`
  7. Add parser tests
- Acceptance: Parser generates set operation AST

**SET-002: Generate UNION Bytecode**
- Priority: HIGH
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: SET-001
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_UNION`, `OP_INTERSECT`, `OP_EXCEPT` opcodes
  2. Generate bytecode for both operands
  3. Generate set operation bytecode
- Acceptance: Bytecode includes set operations

**SET-003: Execute UNION**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 3 days
- Dependencies: SET-002
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_UNION execution:
     - UNION: collect both result sets, deduplicate (hash set)
     - UNION ALL: concatenate both result sets (no deduplication)
  2. Implement OP_INTERSECT: rows in both sets (hash set intersection)
  3. Implement OP_EXCEPT: rows in left set but not right (hash set difference)
  4. Handle ALL variants (multiset semantics)
  5. Add executor tests
- Acceptance: `SELECT name FROM customers UNION SELECT name FROM employees` returns unique names from both tables

**SET-004: Execute INTERSECT and EXCEPT**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: SET-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Complete INTERSECT and EXCEPT implementations (similar to UNION)
  2. Add executor tests
- Acceptance: INTERSECT and EXCEPT return correct results

---

## PHASE 4: DDL EXPANSION (27.5 days)

### ALTER TABLE

**DDL-001: Parse ALTER TABLE ADD COLUMN**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `AlterTableStatement` AST node with subcommands
  2. Parse `ALTER TABLE table ADD [COLUMN] col_name type [constraints]`
  3. Support multiple ADD COLUMN in single statement
  4. Add parser tests
- Acceptance: Parser generates AlterTableStatement with ADD COLUMN

**DDL-002: Parse ALTER TABLE DROP COLUMN**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: DDL-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Parse `ALTER TABLE table DROP [COLUMN] [IF EXISTS] col_name [CASCADE|RESTRICT]`
  2. Add parser tests
- Acceptance: Parser generates DROP COLUMN subcommand

**DDL-003: Parse ALTER TABLE ALTER COLUMN**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: DDL-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Parse `ALTER TABLE table ALTER [COLUMN] col_name { SET DATA TYPE type | SET DEFAULT expr | DROP DEFAULT | SET NOT NULL | DROP NOT NULL }`
  2. Add parser tests
- Acceptance: Parser generates ALTER COLUMN subcommands

**DDL-004: Parse ALTER TABLE ADD/DROP CONSTRAINT**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: DDL-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Parse `ALTER TABLE table ADD CONSTRAINT name { PRIMARY KEY (cols) | UNIQUE (cols) | FOREIGN KEY (cols) REFERENCES table (cols) | CHECK (expr) }`
  2. Parse `ALTER TABLE table DROP CONSTRAINT [IF EXISTS] name [CASCADE|RESTRICT]`
  3. Add parser tests
- Acceptance: Parser generates constraint subcommands

**DDL-005: Parse ALTER TABLE RENAME**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 0.5 days
- Dependencies: DDL-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Parse `ALTER TABLE table RENAME TO new_name`
  2. Parse `ALTER TABLE table RENAME [COLUMN] old_col TO new_col`
  3. Add parser tests
- Acceptance: Parser generates RENAME subcommands

**DDL-006: Generate ALTER TABLE Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 2 days
- Dependencies: DDL-001 through DDL-005
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_ALTER_TABLE` opcode with subcommand enumeration
  2. Generate bytecode for each ALTER TABLE variant
- Acceptance: Bytecode generated for all ALTER TABLE statements

**DDL-007: Execute ALTER TABLE ADD COLUMN**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 3 days
- Dependencies: DDL-006
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::addColumn(table_id, column_def)`
  2. Update pg_attribute catalog table
  3. Add column to in-memory table cache
  4. If NOT NULL without DEFAULT: verify table is empty or error
  5. If DEFAULT provided: update existing rows or use page-level default (Firebird approach)
  6. Add executor tests
- Acceptance: `ALTER TABLE employees ADD COLUMN email VARCHAR(100)` adds column, visible in SELECT

**DDL-008: Execute ALTER TABLE DROP COLUMN**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 2 days
- Dependencies: DDL-007
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::dropColumn(table_id, column_name, cascade)`
  2. Check for dependencies (indexes, constraints, views) - error if RESTRICT
  3. Update pg_attribute (mark column as dropped, don't physically remove)
  4. Update table cache
  5. Add executor tests
- Acceptance: `ALTER TABLE employees DROP COLUMN middle_name` drops column

**DDL-009: Execute ALTER TABLE ALTER COLUMN**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 3 days
- Dependencies: DDL-007
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::alterColumn(table_id, column_name, modification)`
  2. SET DATA TYPE: verify type compatibility, potentially rewrite table
  3. SET/DROP DEFAULT: update catalog
  4. SET/DROP NOT NULL: update catalog, verify existing data if SET NOT NULL
  5. Add executor tests
- Acceptance: `ALTER TABLE employees ALTER COLUMN salary TYPE DECIMAL(12,2)` changes type

**DDL-010: Execute ALTER TABLE RENAME**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 1 day
- Dependencies: DDL-007
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::renameTable(old_name, new_name)`
  2. Implement `CatalogManager::renameColumn(table_id, old_col, new_col)`
  3. Update catalog and caches
  4. Update dependencies (views, procedures referencing renamed object)
  5. Add executor tests
- Acceptance: `ALTER TABLE employees RENAME TO staff` renames table

### DROP TABLE / INDEX / VIEW

**DDL-011: Parse DROP TABLE**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `DropTableStatement` AST node
  2. Parse `DROP TABLE [IF EXISTS] table_name [, ...] [CASCADE|RESTRICT]`
  3. Add parser tests
- Acceptance: Parser generates DropTableStatement

**DDL-012: Generate DROP TABLE Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 0.5 days
- Dependencies: DDL-011
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_DROP_TABLE` opcode
- Acceptance: Bytecode generated

**DDL-013: Execute DROP TABLE**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 2 days
- Dependencies: DDL-012
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::dropTable(table_name, cascade, if_exists)`
  2. Check for dependencies (views, foreign keys, triggers)
  3. If CASCADE: drop all dependent objects
  4. If RESTRICT and dependencies exist: error
  5. Delete from catalog (pg_class, pg_attribute)
  6. Free pages (call PageManager::freePage for all table pages)
  7. Drop indexes
  8. Update caches
  9. Add executor tests
- Acceptance: `DROP TABLE employees CASCADE` removes table and dependencies

**DDL-014: Parse DROP INDEX**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 0.5 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `DropIndexStatement` AST node
  2. Parse `DROP INDEX [IF EXISTS] index_name`
  3. Add parser tests
- Acceptance: Parser generates DropIndexStatement

**DDL-015: Execute DROP INDEX**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 1 day
- Dependencies: DDL-014
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::dropIndex(index_name, if_exists)`
  2. Delete from pg_index catalog
  3. Free index pages
  4. Update index cache
  5. Add executor tests
- Acceptance: `DROP INDEX idx_employee_salary` removes index

**DDL-016: Parse DROP VIEW**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 0.5 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `DropViewStatement` AST node
  2. Parse `DROP VIEW [IF EXISTS] view_name [CASCADE|RESTRICT]`
  3. Add parser tests
- Acceptance: Parser generates DropViewStatement

**DDL-017: Execute DROP VIEW**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 1 day
- Dependencies: DDL-016
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::dropView(view_name, cascade, if_exists)`
  2. Check for dependent views
  3. Delete from catalog
  4. Update cache
  5. Add executor tests
- Acceptance: `DROP VIEW active_employees` removes view

### CREATE INDEX

**DDL-018: Parse CREATE INDEX**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `CreateIndexStatement` AST node
  2. Parse `CREATE [UNIQUE] INDEX [IF NOT EXISTS] index_name ON table_name (col1 [ASC|DESC], ...)`
  3. Parse `CREATE INDEX ... USING BTREE|HASH|GIN`
  4. Parse computed indexes: `CREATE INDEX ... ON table (COMPUTED BY expr)`
  5. Add parser tests
- Acceptance: Parser generates CreateIndexStatement

**DDL-019: Generate CREATE INDEX Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: DDL-018
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_CREATE_INDEX` opcode
- Acceptance: Bytecode generated

**DDL-020: Execute CREATE INDEX**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 2 days
- Dependencies: DDL-019
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Use existing `CatalogManager::createIndex()` (already implemented)
  2. Build index from heap scan:
     a. Scan table
     b. For each row, extract indexed columns
     c. Insert into index (B-tree/Hash/GIN)
  3. Handle UNIQUE constraint violations during build
  4. Add executor tests
- Acceptance: `CREATE INDEX idx_salary ON employees(salary DESC)` creates index, usable in queries

### CREATE VIEW

**DDL-021: Parse CREATE VIEW**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `CreateViewStatement` AST node
  2. Parse `CREATE [OR REPLACE] VIEW [IF NOT EXISTS] view_name [(col1, ...)] AS SELECT ...`
  3. Add parser tests
- Acceptance: Parser generates CreateViewStatement

**DDL-022: Generate CREATE VIEW Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 0.5 days
- Dependencies: DDL-021
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_CREATE_VIEW` opcode
- Acceptance: Bytecode generated

**DDL-023: Execute CREATE VIEW**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 2 days
- Dependencies: DDL-022
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::createView(view_name, query_text, column_names)`
  2. Store view definition in catalog (new system table: pg_views or RDB$RELATIONS with view flag)
  3. Validate SELECT query (parse and semantic check)
  4. Support OR REPLACE (drop existing view first)
  5. Add executor tests
- Acceptance: `CREATE VIEW active_employees AS SELECT * FROM employees WHERE active = true` creates view

**DDL-024: Execute View Queries**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: DDL-023
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. When SELECT references a view, substitute view definition (query rewriting)
  2. Execute rewritten query
  3. Add executor tests
- Acceptance: `SELECT * FROM active_employees` executes view's underlying query

### CREATE SEQUENCE

**DDL-025: Parse CREATE SEQUENCE**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `CreateSequenceStatement` AST node
  2. Parse `CREATE [OR ALTER] SEQUENCE name [START WITH n] [INCREMENT BY n] [MINVALUE n | NO MINVALUE] [MAXVALUE n | NO MAXVALUE] [CYCLE | NO CYCLE]`
  3. Parse Firebird syntax: `CREATE GENERATOR name`
  4. Add parser tests
- Acceptance: Parser generates CreateSequenceStatement

**DDL-026: Parse ALTER SEQUENCE**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 0.5 days
- Dependencies: DDL-025
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `AlterSequenceStatement` AST node
  2. Parse `ALTER SEQUENCE name RESTART WITH n`
  3. Parse `SET GENERATOR name TO n` (Firebird syntax)
  4. Add parser tests
- Acceptance: Parser generates AlterSequenceStatement

**DDL-027: Parse DROP SEQUENCE**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 0.5 days
- Dependencies: DDL-025
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `DropSequenceStatement` AST node
  2. Parse `DROP SEQUENCE [IF EXISTS] name`
  3. Add parser tests
- Acceptance: Parser generates DropSequenceStatement

**DDL-028: Parse NEXT VALUE FOR and GEN_ID**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: DDL-025
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `NextValueExpression` AST node
  2. Parse `NEXT VALUE FOR sequence_name` (SQL standard)
  3. Parse `GEN_ID(generator_name, increment)` (Firebird function)
  4. Support in DEFAULT expressions and SELECT
  5. Add parser tests
- Acceptance: Parser generates NextValueExpression

**DDL-029: Execute CREATE SEQUENCE**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 2 days
- Dependencies: DDL-025
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::createSequence(name, start, increment, min, max, cycle)`
  2. Add entry to catalog (new system table: RDB$GENERATORS or pg_sequence)
  3. Allocate sequence state (current value, stored persistently)
  4. Add executor tests
- Acceptance: `CREATE SEQUENCE order_id_seq START WITH 1000 INCREMENT BY 1` creates sequence

**DDL-030: Execute NEXT VALUE FOR**
- Priority: CRITICAL
- Status: ❌
- Component: Executor + Catalog
- Estimate: 1 day
- Dependencies: DDL-029, DDL-028
- Files: `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- Tasks:
  1. Implement `CatalogManager::nextval(sequence_name)` with atomic increment
  2. Handle wraparound if CYCLE
  3. Error if MAXVALUE reached and NO CYCLE
  4. Persist updated value (write to sequence page)
  5. Add executor tests
- Acceptance: `SELECT NEXT VALUE FOR order_id_seq` returns sequential values

**DDL-031: Execute GEN_ID**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 0.5 days
- Dependencies: DDL-030
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement GEN_ID(name, increment) as function call
  2. If increment > 0: advance sequence by increment, return new value
  3. If increment = 0: return current value without advancing
  4. Add executor tests
- Acceptance: `INSERT INTO orders (id, ...) VALUES (GEN_ID(order_id_seq, 1), ...)` generates unique IDs

---

## PHASE 5: CONSTRAINTS & INDEXES (25 days)

### PRIMARY KEY Enforcement

**CONS-001: Catalog Support for Constraints**
- Priority: CRITICAL
- Status: ❌
- Component: Catalog
- Estimate: 3 days
- Dependencies: None
- Files: `src/core/catalog_manager.cpp`, `include/scratchbird/core/catalog_manager.h`
- Tasks:
  1. Implement `createConstraint(table_id, constraint_def)` method
  2. Implement `getConstraints(table_id)` method
  3. Implement `dropConstraint(table_id, constraint_name)` method
  4. Store constraints in system table (RDB$RELATION_CONSTRAINTS, RDB$CHECK_CONSTRAINTS, RDB$REF_CONSTRAINTS)
  5. Add constraint types: PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK, NOT NULL
  6. Add catalog tests
- Acceptance: Constraints can be created and queried from catalog

**CONS-002: PRIMARY KEY Validation on INSERT**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: CONS-001
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Before INSERT, load PRIMARY KEY constraint for table
  2. Extract PK column values from INSERT data
  3. Check if PK index exists (it should - created automatically with PRIMARY KEY)
  4. Search B-tree index for PK value
  5. If found: error "duplicate key violation"
  6. If not found: proceed with INSERT
  7. Add executor tests
- Acceptance: `INSERT INTO employees (id, ...) VALUES (1, ...)` twice raises error on second attempt

**CONS-003: PRIMARY KEY Validation on UPDATE**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: CONS-002, DML-003 (UPDATE execution)
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Before UPDATE, check if PK columns are being modified
  2. If yes, validate new PK value is unique (same logic as INSERT)
  3. If no, proceed with UPDATE
  4. Add executor tests
- Acceptance: `UPDATE employees SET id = 2 WHERE id = 1` fails if id=2 already exists

### UNIQUE Constraint Enforcement

**CONS-004: UNIQUE Constraint Validation**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: CONS-001
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Load UNIQUE constraints for table
  2. For each UNIQUE constraint, check index for duplicate
  3. Handle NULL values (multiple NULLs allowed in UNIQUE constraint)
  4. Error if duplicate non-NULL value found
  5. Add executor tests
- Acceptance: `INSERT INTO employees (..., email) VALUES (..., 'test@example.com')` twice raises error if email is UNIQUE

### FOREIGN KEY Enforcement

**CONS-005: Parse FOREIGN KEY Constraints**
- Priority: HIGH
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend table constraint parsing for FOREIGN KEY
  2. Parse `FOREIGN KEY (col1, ...) REFERENCES parent_table (parent_col1, ...) [ON DELETE action] [ON UPDATE action]`
  3. Parse actions: CASCADE, SET NULL, SET DEFAULT, RESTRICT, NO ACTION
  4. Add parser tests
- Acceptance: Parser generates FOREIGN KEY constraint definitions

**CONS-006: FOREIGN KEY Validation on INSERT**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 3 days
- Dependencies: CONS-005, CONS-001
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Load FOREIGN KEY constraints for table
  2. For each FK constraint:
     a. Extract FK column values from INSERT data
     b. If all NULL: skip validation (NULL FK is allowed)
     c. Lookup parent table's PK/UNIQUE index
     d. Search for FK value in parent table
     e. If not found: error "foreign key violation"
  3. Add executor tests
- Acceptance: `INSERT INTO orders (customer_id) VALUES (999)` fails if customer_id=999 doesn't exist in customers table

**CONS-007: FOREIGN KEY Validation on UPDATE**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: CONS-006, DML-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Before UPDATE, check if FK columns are being modified
  2. If yes, validate new FK value exists in parent table
  3. Add executor tests
- Acceptance: `UPDATE orders SET customer_id = 999 WHERE id = 1` fails if customer_id=999 doesn't exist

**CONS-008: FOREIGN KEY ON DELETE Actions**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 3 days
- Dependencies: CONS-006, DML-006 (DELETE execution)
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Before DELETE from parent table, find all child tables with FK to this table
  2. For each child table:
     a. Find rows referencing the deleted parent row
     b. If ON DELETE RESTRICT or NO ACTION: error if child rows exist
     c. If ON DELETE CASCADE: delete child rows (recursive)
     d. If ON DELETE SET NULL: set FK columns to NULL in child rows
     e. If ON DELETE SET DEFAULT: set FK columns to DEFAULT in child rows
  3. Handle circular FKs carefully (avoid infinite recursion)
  4. Add executor tests
- Acceptance: `DELETE FROM customers WHERE id = 1` with CASCADE deletes related orders

**CONS-009: FOREIGN KEY ON UPDATE Actions**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: CONS-006, DML-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Before UPDATE of parent PK, find child rows
  2. Apply ON UPDATE action (RESTRICT, CASCADE, SET NULL, SET DEFAULT)
  3. Add executor tests
- Acceptance: `UPDATE customers SET id = 2 WHERE id = 1` with CASCADE updates orders.customer_id

### CHECK Constraint Enforcement

**CONS-010: CHECK Constraint Validation**
- Priority: MEDIUM
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: CONS-001
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Load CHECK constraints for table
  2. For each CHECK constraint, evaluate boolean expression on new/updated row
  3. If expression returns FALSE: error "check constraint violation"
  4. Handle NULL values (NULL means unknown, constraint passes)
  5. Add executor tests
- Acceptance: `INSERT INTO employees (age) VALUES (15)` fails if CHECK (age >= 18) exists

### Index-Backed Query Execution

**IDX-001: Query Planner - Choose Index Scan**
- Priority: CRITICAL
- Status: ❌
- Component: Executor (planner)
- Estimate: 4 days
- Dependencies: None
- Files: `src/sblr/executor.cpp` (new planner module)
- Tasks:
  1. Create basic query planner module
  2. For SELECT with WHERE clause, analyze available indexes
  3. If WHERE clause matches index columns: choose index scan
  4. Example: `SELECT * FROM employees WHERE id = 123` → use PK index
  5. Cost estimation (simple):
     - Heap scan cost = N rows
     - Index scan cost = log(N) + M matching rows
  6. Choose lowest cost plan
  7. Add planner tests
- Acceptance: Query planner selects index scan when beneficial

**IDX-002: Index Scan Execution**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: IDX-001
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Extend SELECT execution to use index scan
  2. Call `storage_engine->createIndexScan(table_id, index_id, search_key)`
  3. Iterate index scan results
  4. Apply remaining WHERE filters (index may not cover all conditions)
  5. Add executor tests
- Acceptance: `SELECT * FROM employees WHERE id = 123` uses index, verified with EXPLAIN (if implemented) or timing

**IDX-003: Index Scan for Range Queries**
- Priority: HIGH
- Status: ❌
- Component: Executor (planner)
- Estimate: 2 days
- Dependencies: IDX-002
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Extend planner to recognize range queries
  2. Example: `SELECT * FROM employees WHERE salary > 50000 AND salary < 100000`
  3. Use B-tree range scan (already implemented in storage engine)
  4. Add executor tests
- Acceptance: Range queries use index scans

---

## PHASE 6: PSQL BASICS (30 days)

### Variables

**PSQL-001: Parse DECLARE Variables**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `DeclareStatement` AST node
  2. Parse `DECLARE @var_name type [DEFAULT expr];`
  3. Support in procedure/function body and EXECUTE BLOCK
  4. Parse `@var_name %TYPE` (copy type from column)
  5. Parse `@var_name table_name%ROWTYPE` (copy entire row structure)
  6. Add parser tests
- Acceptance: Parser generates DeclareStatement

**PSQL-002: Generate DECLARE Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: PSQL-001
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_DECLARE_VAR` opcode
  2. Allocate slot in symbol table for variable
  3. Generate initialization bytecode if DEFAULT provided
- Acceptance: Bytecode includes variable declarations

**PSQL-003: Execute DECLARE**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: PSQL-002
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement variable storage (stack frame or hash map)
  2. Execute OP_DECLARE_VAR: allocate slot, initialize value
  3. Support all data types in variables
  4. Add executor tests
- Acceptance: `DECLARE @counter INTEGER DEFAULT 0;` creates variable

**PSQL-004: Parse SET Variable**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: PSQL-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `SetStatement` AST node
  2. Parse `SET @var_name = expr;`
  3. Parse `@var_name := expr;` (alternative syntax)
  4. Add parser tests
- Acceptance: Parser generates SetStatement

**PSQL-005: Execute SET Variable**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: PSQL-004, PSQL-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Add `OP_SET_VAR` opcode
  2. Evaluate right-hand expression
  3. Store result in variable slot
  4. Add executor tests
- Acceptance: `SET @counter = @counter + 1;` increments variable

**PSQL-006: Parse SELECT INTO**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: PSQL-001
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend SelectStatement with `into_variables` list
  2. Parse `SELECT col1, col2 INTO @var1, @var2 FROM table WHERE ...;`
  3. Validate: number of columns matches number of variables
  4. Add parser tests
- Acceptance: Parser generates SELECT INTO statement

**PSQL-007: Execute SELECT INTO**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: PSQL-006, PSQL-003
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Add `OP_SELECT_INTO` opcode
  2. Execute SELECT query
  3. Verify exactly 1 row returned (error if 0 or >1)
  4. Assign column values to variables
  5. Add executor tests
- Acceptance: `SELECT name, salary INTO @emp_name, @emp_sal FROM employees WHERE id = 1;` assigns values

### IF/THEN/ELSE

**PSQL-008: Parse IF Statement**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `IfStatement` AST node with condition, then_block, elsif_blocks, else_block
  2. Parse `IF condition THEN statements [ELSIF condition THEN statements]... [ELSE statements] END IF;`
  3. Support in procedures, functions, triggers, EXECUTE BLOCK
  4. Add parser tests
- Acceptance: Parser generates IfStatement

**PSQL-009: Generate IF Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 2 days
- Dependencies: PSQL-008
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_JUMP_IF_FALSE`, `OP_JUMP` opcodes (if not already present)
  2. Generate conditional branching bytecode:
     ```
     EVAL_CONDITION
     JUMP_IF_FALSE to_next_branch
     THEN_BLOCK
     JUMP to_end_if
     next_branch:
     ELSIF_CONDITION
     ...
     end_if:
     ```
  3. Add bytecode tests
- Acceptance: Bytecode includes conditional branches

**PSQL-010: Execute IF Statement**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: PSQL-009
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement OP_JUMP_IF_FALSE and OP_JUMP execution
  2. Evaluate condition
  3. Branch to appropriate block
  4. Add executor tests
- Acceptance: `IF @salary > 50000 THEN ... ELSE ... END IF` branches correctly

### WHILE Loop

**PSQL-011: Parse WHILE Loop**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `WhileStatement` AST node
  2. Parse `WHILE condition DO statements END WHILE;`
  3. Parse Firebird syntax: `WHILE (condition) DO BEGIN ... END`
  4. Add parser tests
- Acceptance: Parser generates WhileStatement

**PSQL-012: Generate WHILE Bytecode**
- Priority: CRITICAL
- Status: ❌
- Component: Bytecode
- Estimate: 1 day
- Dependencies: PSQL-011
- Files: `src/sblr/bytecode_generator.cpp`
- Tasks:
  1. Generate loop bytecode:
     ```
     loop_start:
     EVAL_CONDITION
     JUMP_IF_FALSE to_loop_end
     LOOP_BODY
     JUMP to_loop_start
     loop_end:
     ```
  2. Add bytecode tests
- Acceptance: Bytecode includes loop

**PSQL-013: Execute WHILE Loop**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: PSQL-012
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Execute loop (already have jump opcodes from IF)
  2. Add executor tests
  3. Add infinite loop protection (max iterations configurable)
- Acceptance: `WHILE @counter < 100 DO SET @counter = @counter + 1; END WHILE` executes 100 iterations

### FOR Loop

**PSQL-014: Parse FOR Range Loop**
- Priority: CRITICAL
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `ForStatement` AST node
  2. Parse `FOR @var IN start .. end DO statements END FOR;`
  3. Parse `FOR @var IN REVERSE start .. end DO ... END FOR;`
  4. Add parser tests
- Acceptance: Parser generates ForStatement

**PSQL-015: Execute FOR Range Loop**
- Priority: CRITICAL
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: PSQL-014
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Add `OP_FOR_RANGE` opcode
  2. Execute loop:
     ```
     for i from start to end:
       set loop_var = i
       execute body
     ```
  3. Handle REVERSE
  4. Add executor tests
- Acceptance: `FOR i IN 1..10 DO INSERT INTO numbers (val) VALUES (i); END FOR` inserts 10 rows

**PSQL-016: Parse FOR SELECT Loop**
- Priority: HIGH
- Status: ❌
- Component: Parser
- Estimate: 1 day
- Dependencies: PSQL-014
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Extend ForStatement to support SELECT
  2. Parse `FOR @rec IN SELECT ... FROM ... DO statements END FOR;`
  3. Add parser tests
- Acceptance: Parser generates FOR SELECT statement

**PSQL-017: Execute FOR SELECT Loop**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 2 days
- Dependencies: PSQL-016
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Add `OP_FOR_SELECT` opcode
  2. Execute SELECT query
  3. For each result row:
     a. Assign columns to loop variable (record type)
     b. Execute loop body
  4. Add executor tests
- Acceptance: `FOR @emp IN SELECT * FROM employees DO ... END FOR` iterates all employees

### Exception Handling

**PSQL-018: Parse BEGIN...WHEN...END**
- Priority: HIGH
- Status: ❌
- Component: Parser
- Estimate: 2 days
- Dependencies: None
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `ExceptionBlock` AST node
  2. Parse `BEGIN statements WHEN exception_name THEN handler WHEN OTHERS THEN handler END;`
  3. Parse exception names (user-defined and SQLSTATE codes)
  4. Add parser tests
- Acceptance: Parser generates ExceptionBlock

**PSQL-019: Generate Exception Bytecode**
- Priority: HIGH
- Status: ❌
- Component: Bytecode
- Estimate: 2 days
- Dependencies: PSQL-018
- Files: `src/sblr/bytecode_generator.cpp`, `include/scratchbird/sblr/opcodes.h`
- Tasks:
  1. Add `OP_TRY_BEGIN`, `OP_CATCH`, `OP_END_TRY` opcodes
  2. Generate exception table (map PC ranges to handler offsets)
- Acceptance: Bytecode includes exception handling

**PSQL-020: Execute Exception Handling**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 3 days
- Dependencies: PSQL-019
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Implement exception stack
  2. When error occurs:
     a. Check exception table for matching handler
     b. Jump to handler bytecode
     c. Continue or re-raise
  3. Support WHEN OTHERS (catch-all)
  4. Add executor tests
- Acceptance: `BEGIN ... WHEN divide_by_zero THEN ... END` catches division by zero

**PSQL-021: Parse EXCEPTION Statement**
- Priority: HIGH
- Status: ❌
- Component: Parser
- Estimate: 0.5 days
- Dependencies: PSQL-018
- Files: `src/parser/parser.cpp`, `include/scratchbird/parser/ast.h`
- Tasks:
  1. Add `ExceptionStatement` AST node
  2. Parse `EXCEPTION exception_name;`
  3. Add parser tests
- Acceptance: Parser generates EXCEPTION statement

**PSQL-022: Execute EXCEPTION (Raise)**
- Priority: HIGH
- Status: ❌
- Component: Executor
- Estimate: 1 day
- Dependencies: PSQL-021, PSQL-020
- Files: `src/sblr/executor.cpp`
- Tasks:
  1. Add `OP_RAISE_EXCEPTION` opcode
  2. Throw exception with specified code/name
  3. Propagate up call stack
  4. Add executor tests
- Acceptance: `EXCEPTION insufficient_funds;` raises user-defined exception

---

## SUMMARY STATISTICS

### Total Features by Priority

- **CRITICAL**: 89 features
- **HIGH**: 76 features
- **MEDIUM**: 45 features
- **LOW**: 17 features
- **TOTAL**: 227 features

### Total Estimated Effort by Phase

| Phase | Days | Weeks |
|-------|------|-------|
| 1. Core DML | 30 | 6 |
| 2. Aggregation | 17.5 | 3.5 |
| 3. Subqueries | 20 | 4 |
| 4. DDL Expansion | 27.5 | 5.5 |
| 5. Constraints & Indexes | 25 | 5 |
| 6. PSQL Basics | 30 | 6 |
| 7. Procedures & Triggers | 27.5 | 5.5 |
| 8. Advanced SQL | 25 | 5 |
| 9. Built-in Functions | 17.5 | 3.5 |
| 10. System Tables | 12.5 | 2.5 |
| 11. Query Optimizer | 17.5 | 3.5 |
| 12. Embedded Engine | 12.5 | 2.5 |
| **TOTAL** | **262.5** | **52.5** |

### Feature Coverage by Component

| Component | Features | Days |
|-----------|----------|------|
| Parser | 72 | 68.5 |
| Bytecode | 65 | 45 |
| Executor | 68 | 113 |
| Catalog | 22 | 36 |
| **TOTAL** | **227** | **262.5** |

---

## NEXT STEPS

1. **Create GitHub Project Board** with all 227 features as issues
2. **Organize into Milestones** corresponding to Phases 1-12
3. **Assign Priorities** (labels: CRITICAL, HIGH, MEDIUM, LOW)
4. **Start Phase 1** with UPDATE statement (DML-001, DML-002, DML-003)
5. **Track Progress** with daily standups and weekly demos

**Note**: This document continues in ALPHA_COMPLETION_DETAILED_TODO_PART2.md due to length. Parts 2 and 3 cover Phases 7-12 in similar detail.

---

**Document Version**: 1.0
**Last Updated**: October 16, 2025
**Next Review**: After Phase 1 completion (estimated 6 weeks)
