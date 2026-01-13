# Firebird SQL - Programmable SQL (PSQL)

## Overview

Firebird's Procedural SQL (PSQL) extends standard SQL with procedural programming constructs including:
- Stored procedures
- Stored functions (UDFs - User Defined Functions)
- Triggers
- Packages (Firebird 3.0+)
- Exceptions
- EXECUTE BLOCK (anonymous code blocks)

**Current Status**: PSQL features are **not implemented** in ScratchBird's Firebird emulation. The parser does not accept most PSQL DDL statements, and procedural code execution is not supported by the V2 pipeline.

**Important**: This document describes standard Firebird PSQL features for reference. Users should be aware that these features are not currently available in ScratchBird Firebird emulation.

---

## PSQL Language Constructs

Standard Firebird PSQL supports the following procedural language features:

### Control Flow

- **BEGIN ... END**: Code blocks
- **IF ... THEN ... ELSE**: Conditional execution
- **WHILE ... DO**: Loop while condition is true
- **FOR SELECT ... DO**: Iterate over query results
- **FOR EXECUTE STATEMENT ... DO**: Execute dynamic SQL in a loop
- **CASE**: Multi-way branching

### Variables and Assignment

- **DECLARE VARIABLE**: Declare local variables
- **:variable = expression**: Assignment
- **SELECT INTO**: Assign query results to variables

### Flow Control

- **EXIT**: Exit current loop
- **SUSPEND**: Return a row from selectable procedure (like yield)
- **EXCEPTION**: Raise user-defined or system exceptions
- **RETURN**: Return from function

### Other Features

- **Cursors**: Named result sets
- **Comments**: `/* */` or `--`
- **String literals**: Single quotes with '' for escaping

---

## CREATE PROCEDURE

### Description

In standard Firebird, CREATE PROCEDURE defines a stored procedure that can be executed by client applications or called from other procedures, triggers, or functions.

**Status**: Not implemented

The Firebird parser in ScratchBird does not currently accept CREATE PROCEDURE statements and will generate parser errors.

### Standard Firebird Syntax

```sql
CREATE [OR ALTER] PROCEDURE procedure_name
    [ ( [ param_name param_type [, ...] ] ) ]
    [ RETURNS ( [ param_name param_type [, ...] ] ) ]
AS
[DECLARE VARIABLE var_name var_type;]
[...]
BEGIN
    -- procedure body
    [...]
END
```

### Standard Firebird Examples

#### Simple Procedure

```sql
-- This will NOT work in current ScratchBird implementation
CREATE PROCEDURE hello_world
AS
BEGIN
    -- Procedure body
END;
```

#### Procedure with Parameters

```sql
-- This will NOT work in current ScratchBird implementation
CREATE PROCEDURE update_salary (
    emp_id INTEGER,
    new_salary DECIMAL(10,2)
)
AS
BEGIN
    UPDATE employees
    SET salary = :new_salary
    WHERE employee_id = :emp_id;
END;
```

#### Selectable Procedure

```sql
-- This will NOT work in current ScratchBird implementation
CREATE PROCEDURE get_employees
RETURNS (
    emp_id INTEGER,
    full_name VARCHAR(100),
    salary DECIMAL(10,2)
)
AS
BEGIN
    FOR SELECT employee_id, first_name || ' ' || last_name, salary
        FROM employees
        INTO :emp_id, :full_name, :salary
    DO
        SUSPEND;  -- Return each row
END;
```

### Workarounds

Since stored procedures are not supported:

1. **Use application logic**: Implement procedure logic in your application code
2. **Use views**: For simple data transformations, create views instead
3. **Use inline SQL**: Execute SQL statements directly from your application

---

## CREATE FUNCTION

### Description

In standard Firebird, CREATE FUNCTION defines a User-Defined Function (UDF) that can be called in SQL expressions.

**Status**: Not implemented

The Firebird parser does not accept CREATE FUNCTION statements.

### Standard Firebird Syntax

```sql
CREATE [OR ALTER] FUNCTION function_name
    [ ( [ param_name param_type [, ...] ] ) ]
    RETURNS return_type
AS
[DECLARE VARIABLE var_name var_type;]
[...]
BEGIN
    -- function body
    [...]
    RETURN result_value;
END
```

### Standard Firebird Examples

#### Simple Function

```sql
-- This will NOT work in current ScratchBird implementation
CREATE FUNCTION add_numbers (
    a INTEGER,
    b INTEGER
)
RETURNS INTEGER
AS
BEGIN
    RETURN a + b;
END;
```

#### Function with Business Logic

```sql
-- This will NOT work in current ScratchBird implementation
CREATE FUNCTION calculate_tax (
    amount DECIMAL(10,2),
    tax_rate DECIMAL(5,4)
)
RETURNS DECIMAL(10,2)
AS
DECLARE VARIABLE tax DECIMAL(10,2);
BEGIN
    tax = amount * tax_rate;
    RETURN tax;
END;
```

### Workarounds

Since user-defined functions are not supported:

1. **Use built-in functions**: Firebird has many built-in functions
2. **Use CASE expressions**: For conditional logic
3. **Use application logic**: Compute values in your application
4. **Use views with expressions**: For reusable calculations

---

## CREATE TRIGGER

### Description

In standard Firebird, triggers are procedural code that automatically executes in response to table events (INSERT, UPDATE, DELETE).

**Status**: Not implemented

The Firebird parser does not accept CREATE TRIGGER statements.

### Standard Firebird Syntax

```sql
CREATE [OR ALTER] TRIGGER trigger_name
    FOR table_name
    [ACTIVE | INACTIVE]
    {BEFORE | AFTER} {INSERT | UPDATE | DELETE}
    [POSITION number]
AS
[DECLARE VARIABLE var_name var_type;]
[...]
BEGIN
    -- trigger body
    [...]
END
```

### Standard Firebird Examples

#### Before Insert Trigger

```sql
-- This will NOT work in current ScratchBird implementation
CREATE TRIGGER bi_orders FOR orders
ACTIVE BEFORE INSERT
AS
BEGIN
    IF (NEW.order_date IS NULL) THEN
        NEW.order_date = CURRENT_TIMESTAMP;
    IF (NEW.status IS NULL) THEN
        NEW.status = 'NEW';
END;
```

#### After Update Trigger

```sql
-- This will NOT work in current ScratchBird implementation
CREATE TRIGGER au_inventory FOR inventory
AFTER UPDATE
AS
BEGIN
    IF (NEW.quantity < 10 AND OLD.quantity >= 10) THEN
        INSERT INTO low_stock_alerts (product_id, alert_date)
        VALUES (NEW.product_id, CURRENT_TIMESTAMP);
END;
```

#### Audit Trigger

```sql
-- This will NOT work in current ScratchBird implementation
CREATE TRIGGER ad_employees FOR employees
AFTER DELETE
AS
BEGIN
    INSERT INTO employees_audit (
        employee_id, action, action_date, user_name
    ) VALUES (
        OLD.employee_id, 'DELETE', CURRENT_TIMESTAMP, CURRENT_USER
    );
END;
```

### Trigger Context Variables (NEW / OLD)

**Description**

Firebird exposes row-level context variables for triggers:

- `NEW`: Row values after INSERT/UPDATE
- `OLD`: Row values before UPDATE/DELETE

**Syntax**
```sql
NEW.<column>
OLD.<column>
```

**Examples**
```sql
IF (NEW.status IS NULL) THEN
    NEW.status = 'NEW';

IF (OLD.price <> NEW.price) THEN
    INSERT INTO price_history (id, old_price, new_price)
    VALUES (NEW.id, OLD.price, NEW.price);
```

**Status**
- Not implemented: CREATE TRIGGER is not parsed in the Firebird emulation
  layer, so NEW/OLD are not available in SQL.
- Runtime triggers (C++-registered) expose old/new values via TriggerContext,
  but there is no SQL trigger interpreter.

### Workarounds

Since triggers are not supported:

1. **Application-level logic**: Implement trigger logic in your application
2. **Explicit SQL**: Execute necessary actions explicitly
3. **Database constraints**: Use CHECK constraints for validation
4. **Middleware**: Use application middleware or ORM features

---

## CREATE PACKAGE

### Description

In standard Firebird (3.0+), packages group related procedures and functions into a named module.

**Status**: Not implemented

The Firebird parser does not accept CREATE PACKAGE statements.

### Standard Firebird Syntax

```sql
-- Package header
CREATE [OR ALTER] PACKAGE package_name
AS
BEGIN
    PROCEDURE proc_name [(params)];
    FUNCTION func_name [(params)] RETURNS type;
END;

-- Package body
CREATE [OR ALTER] PACKAGE BODY package_name
AS
BEGIN
    PROCEDURE proc_name [(params)]
    AS
    BEGIN
        -- implementation
    END;

    FUNCTION func_name [(params)] RETURNS type
    AS
    BEGIN
        -- implementation
        RETURN value;
    END;
END;
```

### Workarounds

Since packages are not supported:

1. **Naming conventions**: Use prefixes to group related procedures/functions
2. **Schema organization**: Document related functions as a logical group
3. **Application modules**: Organize related logic in application code

---

## CREATE EXCEPTION

### Description

In standard Firebird, user-defined exceptions can be created and raised in PSQL code.

**Status**: Not implemented

The Firebird parser does not accept CREATE EXCEPTION statements.

### Standard Firebird Syntax

```sql
CREATE [OR ALTER] EXCEPTION exception_name 'error message';
```

### Standard Firebird Example

```sql
-- This will NOT work in current ScratchBird implementation
CREATE EXCEPTION invalid_age 'Age must be between 18 and 65';
CREATE EXCEPTION insufficient_funds 'Account balance too low';
```

### Workarounds

Since custom exceptions are not supported:

1. **Use CHECK constraints**: For data validation
2. **Application exceptions**: Raise exceptions in application code
3. **Error codes**: Use application-level error handling

---

## EXECUTE PROCEDURE

### Description

In standard Firebird, EXECUTE PROCEDURE executes a stored procedure.

**Status**: Not implemented

The Firebird parser does not accept EXECUTE PROCEDURE statements.

### Standard Firebird Syntax

```sql
EXECUTE PROCEDURE procedure_name [(param_value, ...)];

-- Or for selectable procedures:
SELECT * FROM procedure_name [(param_value, ...)];
```

### Workarounds

Execute logic directly using SQL statements or application code.

---

## EXECUTE BLOCK

### Description

In standard Firebird, EXECUTE BLOCK allows you to execute anonymous PSQL code without creating a stored procedure.

**Status**: Not implemented

The Firebird parser does not accept EXECUTE BLOCK statements.

### Standard Firebird Syntax

```sql
EXECUTE BLOCK
    [(input_param param_type = ? [, ...])]
    [RETURNS (output_param param_type [, ...])]
AS
[DECLARE VARIABLE var_name var_type;]
[...]
BEGIN
    -- block body
    [...]
    [SUSPEND;]  -- For selectable blocks
END
```

### Standard Firebird Example

```sql
-- This will NOT work in current ScratchBird implementation
EXECUTE BLOCK
RETURNS (product_name VARCHAR(100), total_sales DECIMAL(12,2))
AS
BEGIN
    FOR SELECT p.product_name, SUM(o.quantity * o.unit_price)
        FROM products p
        JOIN order_items o ON p.product_id = o.product_id
        GROUP BY p.product_name
        HAVING SUM(o.quantity * o.unit_price) > 1000
        INTO :product_name, :total_sales
    DO
        SUSPEND;
END;
```

### Workarounds

1. **Use regular SELECT**: For simple queries
2. **Use CTEs**: Common Table Expressions for complex queries
3. **Use application logic**: For procedural operations

---

## PSQL Language Elements

### BEGIN ... END

Status: Parsed but not executed

The parser can parse BEGIN...END blocks in AST, but the semantic analyzer and bytecode generator do not support PSQL nodes.

### IF ... THEN ... ELSE

Status: Parsed but not executed

```sql
IF (condition) THEN
    statement;
[ELSE
    statement;]
```

### WHILE ... DO

Status: Parsed but not executed

```sql
WHILE (condition) DO
    statement;
```

### FOR SELECT ... DO

Status: Parsed but not executed

```sql
FOR SELECT columns FROM table INTO variables DO
    statement;
```

### Variable Declaration

Status: Parsed but not executed

```sql
DECLARE VARIABLE var_name type;
```

### Assignment

Status: Parsed but not executed

```sql
var_name = expression;
:var_name = expression;
```

### SUSPEND

Status: Parsed but not executed

Returns a row from a selectable procedure.

### EXIT

Status: Parsed but not executed

Exits the current loop.

---

## Known Limitations

### Not Implemented

**CREATE PROCEDURE**
- Parser rejects CREATE PROCEDURE statements
- Will generate parse errors
- Stored procedures cannot be created
- Cannot execute procedures

**CREATE FUNCTION**
- Parser rejects CREATE FUNCTION statements
- User-defined functions cannot be created
- Cannot call custom functions in SQL

**CREATE TRIGGER**
- Parser rejects CREATE TRIGGER statements
- No automatic trigger execution
- Cannot implement database-level business logic

**CREATE PACKAGE**
- Parser rejects CREATE PACKAGE statements
- Cannot group procedures/functions into modules

**CREATE EXCEPTION**
- Parser rejects CREATE EXCEPTION statements
- Cannot define custom exceptions
- Cannot raise user-defined exceptions

**EXECUTE PROCEDURE**
- Parser rejects EXECUTE PROCEDURE statements
- Cannot call stored procedures

**EXECUTE BLOCK**
- Parser rejects EXECUTE BLOCK statements
- Cannot execute anonymous PSQL code blocks

### Stubbed (Parsed But Not Executed)

**PSQL Language Constructs**
- BEGIN...END, IF, WHILE, FOR - Parser creates AST nodes
- SemanticAnalyzerV2 rejects these nodes
- BytecodeGeneratorV2 cannot generate code for PSQL
- Executor has no support for procedural execution

### Specification Deltas

**V2 Pipeline Limitations**

The V2 pipeline (semantic analyzer → bytecode generator → executor) is designed for SQL DML/DDL, not procedural code:

1. No symbol table for procedure-local variables
2. No control flow graph for procedural logic
3. No bytecode opcodes for procedural constructs
4. No runtime support for procedure calls, variable scoping, or control flow

**Impact**:
- PSQL features require a complete procedural runtime
- Current architecture is SQL-focused, not procedure-focused
- Implementing PSQL would require significant V2 pipeline enhancements

### Workarounds

**Instead of Stored Procedures**:
1. Implement logic in application code
2. Use SQL scripts executed by the application
3. Use transactions for multi-statement operations

**Instead of Triggers**:
1. Implement trigger logic in application (before/after save hooks)
2. Use ORM features (if available)
3. Use middleware or event listeners
4. Execute necessary SQL explicitly

**Instead of Functions**:
1. Use built-in SQL functions
2. Use CASE expressions for conditional logic
3. Compute values in application code
4. Use views with calculated columns

**Instead of EXECUTE BLOCK**:
1. Use Common Table Expressions (CTEs) for complex queries
2. Use subqueries
3. Execute multiple statements from application
4. Use temporary tables for intermediate results

### Recommendations

For Firebird applications being ported to ScratchBird:

1. **Audit existing PSQL code**: Identify all procedures, functions, triggers, and blocks
2. **Categorize by purpose**:
   - Data validation → Use CHECK constraints or application logic
   - Calculated fields → Use computed columns or views
   - Audit logging → Implement in application
   - Complex queries → Use CTEs or views
3. **Refactor to SQL + application logic**: Move procedural code to application tier
4. **Use database features where possible**: Constraints, computed columns, views
5. **Document dependencies**: Map which application logic replaces which PSQL objects

### Future Considerations

If PSQL support is planned for future implementation, it would require:

- Extended AST node types for procedural constructs
- Symbol table management for procedure-local variables
- Control flow graph construction
- New bytecode opcodes for procedural operations (JUMP, CALL, RETURN, etc.)
- Runtime stack and call frame management
- Variable scoping and lifetime management
- Exception handling infrastructure

### Specification References

- `/home/dcalford/CliWork/ScratchBird/docs/specifications/reference/firebird/`
- `/home/dcalford/CliWork/ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`
- Standard Firebird documentation on PSQL: https://firebirdsql.org/refdocs/langrefupd25-psql.html
