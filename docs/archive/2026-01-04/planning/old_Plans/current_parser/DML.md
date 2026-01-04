# ScratchBird DML Commands - Complete Parser Specification

**Document Purpose**: Comprehensive audit of INSERT, UPDATE, DELETE, and MERGE statement parsing in ScratchBird's parser implementation for comparative analysis with Firebird SQL standards.

**Parser Source**: `/home/dcalford/CliWork/ScratchBird/src/parser/parser.cpp`
**AST Structures**: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast.h`

---

## Table of Contents

1. [INSERT Statement](#1-insert-statement)
2. [UPDATE Statement](#2-update-statement)
3. [DELETE Statement](#3-delete-statement)
4. [MERGE Statement](#4-merge-statement)
5. [Common Features](#5-common-features)
6. [Expression Support](#6-expression-support)
7. [Limitations and Notes](#7-limitations-and-notes)

---

## 1. INSERT Statement

### 1.1 Parser Implementation

**Parser Function**: `Parser::parseInsert()` (line 2656-2914)
**AST Node**: `InsertStmt` (ast.h lines 1944-2019)

### 1.2 BNF Syntax

```bnf
INSERT_STMT ::=
    INSERT INTO table_name
    [ '(' column_list ')' ]
    VALUES value_list [ ',' value_list ]*
    [ on_conflict_clause ]
    [ returning_clause ]

column_list ::= column_name [ ',' column_name ]*

value_list ::= '(' expression [ ',' expression ]* ')'

on_conflict_clause ::=
    ON CONFLICT [ '(' conflict_columns ')' ]
    DO ( NOTHING | UPDATE SET assignment_list [ WHERE expression ] )

conflict_columns ::= column_name [ ',' column_name ]*

assignment_list ::= column_name '=' expression [ ',' column_name '=' expression ]*

returning_clause ::= RETURNING ( '*' | column_list )
```

### 1.3 Supported Features

#### 1.3.1 Multi-Row INSERT
- **Syntax**: `INSERT INTO table VALUES (row1), (row2), ...`
- **Implementation**: Parses multiple value tuples separated by commas
- **Validation**: All rows must have the same number of values
- **Storage**: `std::vector<std::vector<Expression*>> value_rows_`

#### 1.3.2 Column List
- **Optional**: Column list can be omitted (inserts into all columns in order)
- **Explicit**: `INSERT INTO table (col1, col2) VALUES (val1, val2)`
- **Validation**: Column count must match value count if specified
- **Storage**: `std::vector<StringPool::StringId> columns_`

#### 1.3.3 ON CONFLICT (UPSERT)
- **Conflict Detection**: `ON CONFLICT [(col1, col2)] DO ...`
- **Actions**:
  - `DO NOTHING`: Silently skip conflicting rows
  - `DO UPDATE SET col = expr, ...`: Update on conflict
- **Optional WHERE**: Additional condition for DO UPDATE
- **EXCLUDED Reference**: Can reference excluded values in UPDATE expressions
- **Storage**: `OnConflictClause` struct with action, columns, updates

#### 1.3.4 RETURNING Clause
- **Return All**: `RETURNING *`
- **Return Specific**: `RETURNING col1, col2, ...`
- **Use Case**: Get inserted values (especially useful for auto-generated IDs)
- **Storage**: `bool has_returning_`, `std::vector<StringPool::StringId> returning_columns_`

### 1.4 Value Expressions

All values in VALUES clause support full expression parsing via `parseExpression()`:
- Literals (numbers, strings, NULL, TRUE, FALSE)
- Identifiers (for DEFAULT or column references in subqueries)
- Binary operations (+, -, *, /, %, ||, etc.)
- Function calls (built-in and user-defined)
- CASE expressions
- CAST expressions
- Subqueries (scalar subqueries)
- Aggregate functions (if semantically valid)

### 1.5 NOT Supported

- **DEFAULT VALUES**: `INSERT INTO table DEFAULT VALUES`
- **INSERT ... SELECT**: `INSERT INTO table SELECT ...`
- **INSERT ... RETURNING INTO**: Stored procedure output parameter syntax
- **Table aliases**: Cannot alias target table
- **WITH clause**: No CTE support for INSERT
- **ON DUPLICATE KEY UPDATE**: MySQL-style syntax (use ON CONFLICT instead)

### 1.6 AST Structure

```cpp
class InsertStmt : public Statement
{
    StringPool::StringId table_name_;
    std::vector<StringPool::StringId> columns_;               // Optional column list
    std::vector<std::vector<Expression*>> value_rows_;        // Multi-row support
    bool has_returning_;
    std::vector<StringPool::StringId> returning_columns_;
    OnConflictClause on_conflict_;                            // UPSERT support
};

struct OnConflictClause
{
    OnConflictAction action;                                  // NONE, DO_NOTHING, DO_UPDATE
    std::vector<StringPool::StringId> conflict_columns_;      // Target columns for conflict
    std::vector<StringPool::StringId> update_columns_;        // SET columns
    std::vector<Expression*> update_values_;                  // SET values
    Expression* where_clause_;                                // Optional WHERE for DO UPDATE
};
```

### 1.7 Examples

```sql
-- Simple single-row insert
INSERT INTO employees (id, name, salary) VALUES (1, 'Alice', 50000);

-- Multi-row insert
INSERT INTO employees (id, name, salary)
VALUES
    (1, 'Alice', 50000),
    (2, 'Bob', 60000),
    (3, 'Charlie', 55000);

-- Insert without column list (all columns)
INSERT INTO employees VALUES (4, 'David', 'Engineering', 70000, '2024-01-01');

-- Insert with expressions
INSERT INTO employees (id, name, salary)
VALUES (5, UPPER('eve'), 50000 * 1.1);

-- UPSERT: Insert or do nothing on conflict
INSERT INTO employees (id, name, salary)
VALUES (1, 'Alice', 50000)
ON CONFLICT (id) DO NOTHING;

-- UPSERT: Insert or update on conflict
INSERT INTO employees (id, name, salary)
VALUES (1, 'Alice', 55000)
ON CONFLICT (id)
DO UPDATE SET salary = EXCLUDED.salary;

-- UPSERT with conditional update
INSERT INTO employees (id, name, salary)
VALUES (1, 'Alice', 55000)
ON CONFLICT (id)
DO UPDATE SET salary = EXCLUDED.salary
WHERE employees.salary < EXCLUDED.salary;

-- Insert with RETURNING
INSERT INTO employees (name, salary)
VALUES ('Frank', 60000)
RETURNING id;

-- Multi-row insert with RETURNING
INSERT INTO employees (name, salary)
VALUES ('Grace', 65000), ('Henry', 70000)
RETURNING *;
```

---

## 2. UPDATE Statement

### 2.1 Parser Implementation

**Parser Function**: `Parser::parseUpdate()` (line 3543-3647)
**AST Node**: `UpdateStmt` (ast.h lines 2429-2470)

### 2.2 BNF Syntax

```bnf
UPDATE_STMT ::=
    UPDATE table_name
    SET assignment_list
    [ WHERE expression ]
    [ returning_clause ]

assignment_list ::=
    column_name '=' expression
    [ ',' column_name '=' expression ]*

returning_clause ::= RETURNING ( '*' | column_list )
```

### 2.3 Supported Features

#### 2.3.1 SET Clause
- **Multiple Assignments**: Update multiple columns in one statement
- **Expression Values**: Full expression support for assigned values
- **Column References**: Can reference other columns in same table
- **Storage**: `std::vector<Assignment>` where each assignment has column name and expression

#### 2.3.2 WHERE Clause
- **Optional**: Without WHERE, updates all rows (full table update)
- **Full Expression Support**: Any valid boolean expression
- **Column References**: Can reference any column in the table
- **Subqueries**: Can use subqueries in WHERE condition

#### 2.3.3 RETURNING Clause
- **Return All**: `RETURNING *`
- **Return Specific**: `RETURNING col1, col2, ...`
- **Use Case**: Get updated values without separate SELECT
- **Returns**: All affected rows with specified columns

### 2.4 Expression Support

All assignment values and WHERE conditions support full expression parsing:
- Literals and constants
- Column references (current row values)
- Arithmetic operations
- String operations (concatenation, UPPER, LOWER, etc.)
- Function calls (scalar and aggregate in subqueries)
- CASE expressions
- Subqueries (scalar, EXISTS, IN)
- CAST operations
- NULL handling (IS NULL, COALESCE, NULLIF)

### 2.5 NOT Supported

- **FROM Clause**: `UPDATE table SET col = val FROM other_table WHERE ...`
- **Table Joins**: Cannot join with other tables directly in UPDATE
- **Table Aliases**: Cannot alias the target table
- **Multiple Tables**: Cannot update multiple tables in one statement
- **WITH Clause**: No CTE support for UPDATE
- **OF Columns**: `FOR UPDATE OF column_list` cursor syntax
- **LIMIT**: No row limit on updates

### 2.6 AST Structure

```cpp
class UpdateStmt : public Statement
{
    StringPool::StringId table_name_;
    std::vector<Assignment> assignments_;
    Expression* where_clause_;
    bool has_returning_;
    std::vector<StringPool::StringId> returning_columns_;
};

struct Assignment
{
    StringPool::StringId column_name;
    Expression* value;
};
```

### 2.7 Examples

```sql
-- Simple update
UPDATE employees SET salary = 60000 WHERE id = 1;

-- Multiple column update
UPDATE employees
SET salary = 65000, department = 'Engineering'
WHERE id = 2;

-- Update with expression
UPDATE employees
SET salary = salary * 1.1
WHERE department = 'Sales';

-- Update with function call
UPDATE employees
SET name = UPPER(name), updated_at = CURRENT_TIMESTAMP
WHERE hire_date < '2020-01-01';

-- Update with CASE expression
UPDATE employees
SET bonus = CASE
    WHEN salary < 50000 THEN salary * 0.1
    WHEN salary < 70000 THEN salary * 0.08
    ELSE salary * 0.05
END;

-- Update with subquery
UPDATE employees
SET salary = (SELECT AVG(salary) FROM employees WHERE department = e.department)
WHERE id = 5;

-- Update with EXISTS
UPDATE employees e
SET salary = salary * 1.15
WHERE EXISTS (
    SELECT 1 FROM performance_reviews
    WHERE employee_id = e.id AND rating = 'Excellent'
);

-- Update all rows (no WHERE)
UPDATE employees SET status = 'active';

-- Update with RETURNING
UPDATE employees
SET salary = salary * 1.1
WHERE department = 'Engineering'
RETURNING id, name, salary;

-- Update with complex WHERE
UPDATE employees
SET salary = salary * 1.05
WHERE salary BETWEEN 40000 AND 60000
  AND department IN ('Sales', 'Marketing')
  AND hire_date >= DATE '2023-01-01';
```

---

## 3. DELETE Statement

### 3.1 Parser Implementation

**Parser Function**: `Parser::parseDelete()` (line 3649-3721)
**AST Node**: `DeleteStmt` (ast.h lines 2473-2509)

### 3.2 BNF Syntax

```bnf
DELETE_STMT ::=
    DELETE FROM table_name
    [ WHERE expression ]
    [ returning_clause ]

returning_clause ::= RETURNING ( '*' | column_list )
```

### 3.3 Supported Features

#### 3.3.1 FROM Clause
- **Required**: `FROM` keyword is mandatory
- **Single Table**: Deletes from one table only
- **No Alias**: Table aliasing not supported

#### 3.3.2 WHERE Clause
- **Optional**: Without WHERE, deletes all rows (TRUNCATE-like behavior)
- **Full Expression Support**: Any valid boolean expression
- **Subqueries**: Can use subqueries in WHERE condition
- **Performance Warning**: No WHERE = full table delete

#### 3.3.3 RETURNING Clause
- **Return All**: `RETURNING *`
- **Return Specific**: `RETURNING col1, col2, ...`
- **Use Case**: Audit deleted rows, get deleted values
- **Returns**: All deleted rows with specified columns

### 3.4 Expression Support

WHERE clause supports full expression parsing:
- Literals and constants
- Column references
- Comparison operators (=, <, >, <=, >=, <>, !=)
- Logical operators (AND, OR, NOT)
- IN, NOT IN (with lists or subqueries)
- EXISTS, NOT EXISTS
- BETWEEN, NOT BETWEEN
- IS NULL, IS NOT NULL
- LIKE, NOT LIKE (pattern matching)
- Subqueries (scalar, row, table)
- Function calls

### 3.5 NOT Supported

- **USING Clause**: `DELETE FROM t1 USING t2 WHERE t1.id = t2.id`
- **Table Joins**: Cannot join with other tables
- **Table Aliases**: Cannot alias the target table
- **Multiple Tables**: Cannot delete from multiple tables
- **WITH Clause**: No CTE support for DELETE
- **LIMIT**: No row limit on deletes
- **ORDER BY**: Cannot order deletions
- **TRUNCATE Alternative**: No special TRUNCATE command (use DELETE FROM without WHERE)

### 3.6 AST Structure

```cpp
class DeleteStmt : public Statement
{
    StringPool::StringId table_name_;
    Expression* where_clause_;
    bool has_returning_;
    std::vector<StringPool::StringId> returning_columns_;
};
```

### 3.7 Examples

```sql
-- Simple delete
DELETE FROM employees WHERE id = 1;

-- Delete with multiple conditions
DELETE FROM employees
WHERE department = 'Sales' AND salary < 40000;

-- Delete with IN clause
DELETE FROM employees
WHERE id IN (1, 2, 3, 4, 5);

-- Delete with subquery
DELETE FROM employees
WHERE id IN (
    SELECT employee_id FROM terminations
    WHERE termination_date < '2024-01-01'
);

-- Delete with EXISTS
DELETE FROM employees e
WHERE EXISTS (
    SELECT 1 FROM disciplinary_actions
    WHERE employee_id = e.id AND action = 'Terminated'
);

-- Delete with NOT IN
DELETE FROM employees
WHERE department NOT IN ('Engineering', 'Sales', 'Marketing');

-- Delete with BETWEEN
DELETE FROM log_entries
WHERE log_date BETWEEN '2023-01-01' AND '2023-12-31';

-- Delete with NULL check
DELETE FROM employees
WHERE termination_date IS NOT NULL;

-- Delete with pattern matching
DELETE FROM temp_tables
WHERE table_name LIKE 'temp_%';

-- Delete all rows (use with caution!)
DELETE FROM temp_data;

-- Delete with RETURNING
DELETE FROM employees
WHERE department = 'Deprecated'
RETURNING id, name, hire_date;

-- Delete with complex condition
DELETE FROM employees
WHERE (salary < 30000 AND hire_date < '2020-01-01')
   OR (status = 'inactive' AND last_login < '2023-01-01');
```

---

## 4. MERGE Statement

### 4.1 Parser Implementation

**Parser Function**: `Parser::parseMerge()` (line 3723-3994)
**AST Node**: `MergeStmt` (ast.h lines 2512-2559)

### 4.2 BNF Syntax

```bnf
MERGE_STMT ::=
    MERGE INTO target_table
    USING source
    ON join_condition
    when_clause+

source ::=
    table_name
    | '(' select_statement ')'

when_clause ::=
    when_matched_clause
    | when_not_matched_clause
    | when_not_matched_by_source_clause

when_matched_clause ::=
    WHEN MATCHED
    THEN UPDATE SET assignment_list

when_not_matched_clause ::=
    WHEN NOT MATCHED
    THEN INSERT [ '(' column_list ')' ] VALUES '(' expression_list ')'

when_not_matched_by_source_clause ::=
    WHEN NOT MATCHED BY SOURCE
    THEN DELETE

assignment_list ::= column_name '=' expression [ ',' column_name '=' expression ]*

column_list ::= column_name [ ',' column_name ]*

expression_list ::= expression [ ',' expression ]*
```

### 4.3 Supported Features

#### 4.3.1 USING Clause (Source)
- **Table Source**: Direct table reference
- **Subquery Source**: `USING (SELECT ...) AS alias`
- **Expression Wrapping**: Source is stored as Expression* (IdentifierExpr or SubqueryExpr)
- **No Alias Required**: Alias is optional for subqueries

#### 4.3.2 ON Condition
- **Join Condition**: Full expression support for matching logic
- **Column References**: Can reference both target and source
- **Complex Conditions**: Supports AND, OR, and other operators
- **Storage**: `Expression* on_condition_`

#### 4.3.3 WHEN MATCHED
- **Action**: UPDATE with SET clause
- **Multiple Columns**: Can update multiple columns
- **Expression Values**: Full expression support for SET values
- **Source References**: Can reference source columns in expressions
- **Optional Condition**: No per-clause conditions (only main ON condition)

#### 4.3.4 WHEN NOT MATCHED
- **Action**: INSERT with VALUES clause
- **Column List**: Optional column list (defaults to all columns)
- **Expression Values**: Full expression support for inserted values
- **Source References**: Can reference source columns

#### 4.3.5 WHEN NOT MATCHED BY SOURCE
- **Action**: DELETE (removes rows in target not in source)
- **No Condition**: Deletes all non-matching target rows
- **Use Case**: Synchronization where target should match source exactly

#### 4.3.6 Multiple WHEN Clauses
- **Allowed**: Can have multiple WHEN clauses
- **Order Matters**: Processed in order specified
- **At Least One**: Requires at least one WHEN clause
- **Storage**: `std::vector<WhenClause> when_clauses_`

### 4.4 Expression Support

All MERGE components support full expression parsing:
- **ON condition**: Any boolean expression
- **SET values**: Any expression (literals, functions, source/target column references)
- **INSERT values**: Any expression (literals, functions, source column references)

Special references:
- Target table columns: Direct column references
- Source table columns: Qualified or unqualified column references

### 4.5 NOT Supported

- **Conditional WHEN**: `WHEN MATCHED AND condition THEN ...`
- **Multiple Actions**: Only one action per WHEN type
- **Target Aliases**: Cannot alias target table
- **Source Aliases**: Source alias not parsed (though subqueries work)
- **MERGE ... RETURNING**: No RETURNING clause for MERGE
- **WITH Clause**: No CTE support for MERGE

### 4.6 AST Structure

```cpp
class MergeStmt : public Statement
{
    StringPool::StringId target_table_;
    Expression* source_;              // Table or subquery
    Expression* on_condition_;
    std::vector<WhenClause> when_clauses_;
};

struct WhenClause
{
    enum Type {
        MATCHED,              // WHEN MATCHED THEN UPDATE
        NOT_MATCHED,          // WHEN NOT MATCHED THEN INSERT
        NOT_MATCHED_BY_SOURCE // WHEN NOT MATCHED BY SOURCE THEN DELETE
    };

    Type type;
    Expression* condition;  // Optional (currently not parsed)

    // For UPDATE
    std::vector<Assignment> assignments;

    // For INSERT
    std::vector<StringPool::StringId> insert_columns;
    std::vector<Expression*> insert_values;
};
```

### 4.7 Examples

```sql
-- Simple MERGE with table source
MERGE INTO target_employees
USING source_employees
ON target_employees.id = source_employees.id
WHEN MATCHED THEN
    UPDATE SET salary = source_employees.salary
WHEN NOT MATCHED THEN
    INSERT (id, name, salary) VALUES (source_employees.id, source_employees.name, source_employees.salary);

-- MERGE with subquery source
MERGE INTO employees
USING (SELECT id, new_salary FROM salary_updates WHERE effective_date = CURRENT_DATE)
ON employees.id = id
WHEN MATCHED THEN
    UPDATE SET salary = new_salary;

-- MERGE with multiple columns updated
MERGE INTO employees
USING staged_employees
ON employees.id = staged_employees.id
WHEN MATCHED THEN
    UPDATE SET
        name = staged_employees.name,
        department = staged_employees.department,
        salary = staged_employees.salary,
        updated_at = CURRENT_TIMESTAMP;

-- MERGE with expressions in UPDATE
MERGE INTO employees
USING salary_adjustments
ON employees.id = salary_adjustments.employee_id
WHEN MATCHED THEN
    UPDATE SET salary = employees.salary * (1 + salary_adjustments.increase_pct);

-- MERGE with all three WHEN clauses
MERGE INTO product_inventory
USING warehouse_stock
ON product_inventory.product_id = warehouse_stock.product_id
WHEN MATCHED THEN
    UPDATE SET quantity = warehouse_stock.quantity
WHEN NOT MATCHED THEN
    INSERT (product_id, product_name, quantity)
    VALUES (warehouse_stock.product_id, warehouse_stock.product_name, warehouse_stock.quantity)
WHEN NOT MATCHED BY SOURCE THEN
    DELETE;

-- MERGE for data synchronization
MERGE INTO cache_table
USING (
    SELECT key, value, last_modified
    FROM master_table
    WHERE last_modified > (SELECT MAX(sync_time) FROM sync_log)
)
ON cache_table.key = key
WHEN MATCHED THEN
    UPDATE SET value = value, last_modified = last_modified
WHEN NOT MATCHED THEN
    INSERT (key, value, last_modified) VALUES (key, value, last_modified);

-- MERGE with computed values
MERGE INTO summary_stats
USING (
    SELECT department, COUNT(*) as emp_count, AVG(salary) as avg_salary
    FROM employees
    GROUP BY department
)
ON summary_stats.department = department
WHEN MATCHED THEN
    UPDATE SET
        employee_count = emp_count,
        average_salary = avg_salary,
        last_updated = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (department, employee_count, average_salary, last_updated)
    VALUES (department, emp_count, avg_salary, CURRENT_TIMESTAMP);
```

---

## 5. Common Features

### 5.1 RETURNING Clause

Supported in INSERT, UPDATE, and DELETE (but NOT MERGE):

**Syntax**:
```sql
RETURNING *
RETURNING column1, column2, ...
```

**Use Cases**:
- Get auto-generated IDs from INSERT
- Audit deleted values from DELETE
- Verify updated values from UPDATE
- Avoid separate SELECT queries

**Limitations**:
- Column names only (no expressions or aliases)
- Cannot use `*` with specific columns
- Returns all affected rows
- Not supported in MERGE

### 5.2 Table References

**All DML Commands**:
- Single table name only
- No table aliases in current implementation
- No schema qualification parsing (assumes current schema)
- Uses StringPool::StringId for table names

**MERGE Exception**:
- Source can be table or subquery
- Subqueries wrapped in SubqueryExpr

### 5.3 Expression Parsing

All DML statements use `Parser::parseExpression()` which supports:

**Primary Expressions**:
- Literals: integers, floats, strings, NULL, TRUE, FALSE
- Identifiers: column names, qualified names (table.column)
- Parameters: Positional ($1, $2) and named (:name)

**Operators**:
- Arithmetic: +, -, *, /, %
- Comparison: =, <>, !=, <, >, <=, >=
- Logical: AND, OR, NOT
- String: || (concatenation), LIKE, ILIKE
- NULL: IS NULL, IS NOT NULL

**Functions**:
- Scalar functions: UPPER, LOWER, SUBSTRING, etc.
- Aggregate functions: COUNT, SUM, AVG, MIN, MAX
- Window functions: ROW_NUMBER, RANK, etc.
- Date/time: CURRENT_TIMESTAMP, EXTRACT, etc.
- JSON: JSON_EXTRACT, JSON_ARRAY, etc.

**Complex Expressions**:
- CASE expressions (simple and searched)
- CAST and type conversions
- COALESCE and NULLIF
- Subqueries (scalar, EXISTS, IN, ANY, ALL)
- Array expressions: IN (list)
- BETWEEN expressions

### 5.4 Validation

**Parser-Level Validation**:
- Token sequence correctness
- Syntax structure validation
- Column/value count matching (INSERT)
- Required clauses presence

**NOT Validated at Parse Time**:
- Table existence
- Column existence
- Type compatibility
- Constraint violations
- Permission checks

These are handled by semantic analyzer and executor.

---

## 6. Expression Support

### 6.1 Expression Types Available in DML

All DML statements can use these expression types in appropriate contexts:

#### 6.1.1 Literal Expressions
```cpp
class LiteralExpr : public Expression
```
- Integer literals: `123`, `-456`
- Float literals: `3.14`, `-0.5`, `1.23e-4`
- String literals: `'text'`, `'it''s'` (escaped quotes)
- Boolean: `TRUE`, `FALSE`
- NULL: `NULL`

#### 6.1.2 Identifier Expressions
```cpp
class IdentifierExpr : public Expression
```
- Simple column: `column_name`
- Qualified column: `table_name.column_name`
- System columns: `CURRENT_USER`, `CURRENT_DATABASE`

#### 6.1.3 Binary Operations
```cpp
class BinaryOpExpr : public Expression
```
- Arithmetic: `a + b`, `a - b`, `a * b`, `a / b`, `a % b`
- Comparison: `a = b`, `a <> b`, `a < b`, `a > b`, `a <= b`, `a >= b`
- Logical: `a AND b`, `a OR b`
- String: `a || b` (concatenation)
- Pattern: `a LIKE b`, `a ILIKE b`
- Range: `a BETWEEN b AND c`

#### 6.1.4 Function Calls
```cpp
class FunctionCallExpr : public Expression
```
- Scalar: `UPPER('text')`, `SUBSTRING('text', 1, 5)`
- Math: `ABS(-5)`, `ROUND(3.14159, 2)`, `SQRT(16)`
- Date/Time: `CURRENT_TIMESTAMP`, `DATE_TRUNC('day', timestamp)`
- String: `LENGTH('text')`, `TRIM(' text ')`
- Conditional: `GREATEST(a, b, c)`, `LEAST(a, b, c)`

#### 6.1.5 Aggregate Expressions
```cpp
class AggregateExpr : public Expression
```
- Basic: `COUNT(*)`, `SUM(column)`, `AVG(column)`, `MIN(column)`, `MAX(column)`
- Statistical: `STDDEV(column)`, `VARIANCE(column)`
- DISTINCT: `COUNT(DISTINCT column)`
- FILTER: `COUNT(*) FILTER (WHERE condition)` (if supported)

**Note**: Aggregates only valid in subqueries for DML, not in direct SET/VALUES clauses.

#### 6.1.6 CASE Expressions
```cpp
class CaseExpr : public Expression
```
- Searched CASE:
  ```sql
  CASE
      WHEN condition1 THEN result1
      WHEN condition2 THEN result2
      ELSE default_result
  END
  ```
- Simple CASE:
  ```sql
  CASE expression
      WHEN value1 THEN result1
      WHEN value2 THEN result2
      ELSE default_result
  END
  ```

#### 6.1.7 CAST Expressions
```cpp
class CastExpr : public Expression
```
- Explicit cast: `CAST(expression AS type)`
- Type conversion: `CAST('123' AS INTEGER)`

#### 6.1.8 Subquery Expressions
```cpp
class SubqueryExpr : public Expression
```
- Scalar subquery: `(SELECT value FROM table WHERE ...)`
- EXISTS: `EXISTS (SELECT 1 FROM table WHERE ...)`
- IN: `column IN (SELECT id FROM table)`
- Comparison: `value = (SELECT MAX(column) FROM table)`

#### 6.1.9 NULL Handling
```cpp
class CoalesceExpr : public Expression
class NullIfExpr : public Expression
```
- COALESCE: `COALESCE(expr1, expr2, expr3)` - first non-NULL
- NULLIF: `NULLIF(expr1, expr2)` - NULL if equal, else expr1
- IS NULL: `column IS NULL`
- IS NOT NULL: `column IS NOT NULL`

#### 6.1.10 Other Expressions
- **EXTRACT**: `EXTRACT(YEAR FROM date_column)`
- **Sequence Functions**: `NEXTVAL('seq_name')`, `CURRVAL('seq_name')`
- **Window Functions**: `ROW_NUMBER() OVER (...)` (in subqueries)
- **JSON Functions**: `JSON_EXTRACT(json_col, '$.path')`
- **GROUPING**: `GROUPING(column)` (for ROLLUP/CUBE)

### 6.2 Context-Specific Expression Usage

#### INSERT Statement
- **VALUES clause**: Any expression except aggregates without subquery
- **ON CONFLICT SET**: Any expression, can reference EXCLUDED.column
- **WHERE (in ON CONFLICT)**: Boolean expressions

#### UPDATE Statement
- **SET clause**: Any expression, can reference current column values
- **WHERE clause**: Boolean expressions, full subquery support

#### DELETE Statement
- **WHERE clause**: Boolean expressions, full subquery support

#### MERGE Statement
- **ON condition**: Boolean expressions
- **UPDATE SET**: Any expression, can reference source/target columns
- **INSERT VALUES**: Any expression, can reference source columns

---

## 7. Limitations and Notes

### 7.1 INSERT Limitations

1. **No INSERT ... SELECT**: Cannot insert from SELECT query
   - Workaround: Use application-level loop or multi-row VALUES

2. **No DEFAULT VALUES**: Cannot use `INSERT INTO table DEFAULT VALUES`
   - Workaround: Explicitly list all columns with DEFAULT

3. **No DEFAULT keyword**: Cannot use `VALUES (1, DEFAULT, 'text')`
   - Workaround: Explicitly provide all values

4. **No OVERRIDING**: Cannot use `OVERRIDING SYSTEM VALUE` for identity columns

5. **No Multi-Table Insert**: Cannot insert into multiple tables in one statement

### 7.2 UPDATE Limitations

1. **No FROM Clause**: Cannot join with other tables
   - Workaround: Use correlated subqueries in SET or WHERE

2. **No Table Alias**: Target table cannot be aliased
   - Impact: Self-referencing in subqueries requires table name

3. **No Multi-Table Update**: Cannot update multiple tables

4. **No LIMIT**: Cannot limit number of rows updated
   - Workaround: Use WHERE clause with primary key IN (SELECT ... LIMIT N)

5. **No ORDER BY**: Cannot control update order

### 7.3 DELETE Limitations

1. **No USING Clause**: Cannot join with other tables
   - Workaround: Use subqueries in WHERE clause

2. **No Table Alias**: Target table cannot be aliased

3. **No LIMIT**: Cannot limit number of rows deleted
   - Workaround: Use WHERE clause with primary key IN (SELECT ... LIMIT N)

4. **No ORDER BY**: Cannot control deletion order

5. **No TRUNCATE**: No separate fast-delete command
   - Workaround: Use `DELETE FROM table` (but slower)

### 7.4 MERGE Limitations

1. **No Conditional WHEN**: Cannot use `WHEN MATCHED AND condition THEN ...`
   - Impact: All matching rows get same action

2. **No RETURNING**: Cannot return merged rows

3. **Single Action per Type**: Only one WHEN clause of each type

4. **No Source Alias**: Source table/subquery alias not parsed
   - Impact: Column references may be ambiguous

5. **No DELETE Action for MATCHED**: `WHEN MATCHED THEN DELETE` not supported
   - Only UPDATE allowed for matched rows

### 7.5 General DML Limitations

1. **No WITH Clause**: Common Table Expressions not supported in DML
   - Note: WITH is supported in SELECT, but not INSERT/UPDATE/DELETE/MERGE

2. **No Table Aliases**: Target tables cannot be aliased in any DML

3. **No Schema Qualification**: Schema names not parsed
   - Uses current/default schema only

4. **No Column Aliases in RETURNING**: `RETURNING col AS alias` not supported

5. **No RETURNING Expressions**: `RETURNING col * 2` not supported
   - Only column names or *

6. **No Batch Statements**: Cannot combine multiple DML in single parse
   - Each statement must be parsed separately

### 7.6 Comparison with Firebird

**Similarities**:
- Basic DML syntax largely compatible
- RETURNING clause (Firebird-style)
- Multi-row INSERT support

**Differences**:
- Firebird: `UPDATE OR INSERT` → ScratchBird: `INSERT ... ON CONFLICT`
- Firebird: More flexible MERGE conditions
- Firebird: RETURNING INTO for procedures
- Firebird: DEFAULT VALUES support
- Firebird: More complex JOIN support in DML

### 7.7 Parser vs. Executor Separation

**Parser Responsibilities** (this document):
- Syntax validation
- AST construction
- Token sequence verification
- Basic structure validation

**NOT Parser Responsibilities**:
- Semantic validation (type checking)
- Table/column existence verification
- Permission checking
- Constraint validation
- Execution planning
- Transaction handling

All semantic and execution concerns are handled downstream by:
- Semantic Analyzer (`semantic_analyzer.cpp`)
- Query Planner (`query_planner.cpp`)
- Executor (`executor.cpp`)

---

## Summary

The ScratchBird parser implements a robust subset of SQL DML with the following highlights:

**Strengths**:
- Multi-row INSERT support
- PostgreSQL-style UPSERT (ON CONFLICT)
- RETURNING clause for INSERT, UPDATE, DELETE
- Full MERGE statement support
- Rich expression support (CASE, CAST, subqueries)
- Clean AST representation

**Notable Omissions** (compared to full SQL standard):
- INSERT ... SELECT
- DEFAULT VALUES / DEFAULT keyword
- FROM/USING clauses in UPDATE/DELETE
- WITH clauses in DML
- Table aliases in DML
- Conditional WHEN in MERGE

**Design Philosophy**:
- Simple, single-table DML operations
- Complex logic via expressions and subqueries
- Firebird-inspired RETURNING syntax
- PostgreSQL-inspired UPSERT syntax
- Clear separation of parsing and semantic analysis

This makes the parser suitable for most common DML operations while maintaining simplicity and clarity in the implementation.
