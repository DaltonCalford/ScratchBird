[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Firebird SQL - Programmable SQL (PSQL)

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## Overview

Firebird's Procedural SQL (PSQL) extends standard SQL with procedural programming constructs including:
- Stored procedures
- Stored functions (UDFs - User Defined Functions)
- Triggers
- Packages (Firebird 3.0+)
- Exceptions
- EXECUTE BLOCK (anonymous code blocks)

**Current Status**: PSQL DDL statements (CREATE/ALTER PROCEDURE, FUNCTION, TRIGGER, PACKAGE, EXCEPTION) are **fully parsed** by the Firebird emulation parser. DDL is registered in the catalog. Procedural body interpretation (control flow, cursor operations, SUSPEND) is not yet wired for runtime execution - bodies are stored as source text.

**Important**: PSQL DDL works for defining and storing procedures, functions, triggers, packages, and exceptions. Runtime procedural body execution (complex control flow within bodies) is a known limitation.

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

**Status**: Implemented (DDL parsing and catalog registration)

The Firebird parser accepts CREATE [OR ALTER] PROCEDURE with parameters, RETURNS clause, and body capture. Procedure definitions are registered in the catalog. Procedural body runtime interpretation is not yet wired.

### Syntax

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

### Examples

#### Simple Procedure

```sql

CREATE PROCEDURE hello_world
AS
BEGIN
    -- Procedure body
END;
```

#### Procedure with Parameters

```sql

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

---

## CREATE FUNCTION

### Description

Creates a User-Defined Function (UDF) that can be called in SQL expressions.

**Status**: Implemented (DDL parsing and catalog registration)

The Firebird parser accepts CREATE [OR ALTER] FUNCTION with parameters, RETURNS clause, DETERMINISTIC, and body capture. Function definitions are registered in the catalog. Procedural body runtime interpretation is not yet wired.

### Syntax

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

### Examples

#### Simple Function

```sql

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

---

## CREATE TRIGGER

### Description

Creates a trigger that automatically executes in response to table events (INSERT, UPDATE, DELETE).

**Status**: Implemented (DDL parsing and catalog registration)

The Firebird parser accepts CREATE [OR ALTER] TRIGGER with FOR table, ACTIVE/INACTIVE, BEFORE/AFTER timing, INSERT/UPDATE/DELETE events, POSITION, and body capture. Trigger definitions are registered in the catalog. Procedural body runtime interpretation is not yet wired.

### Syntax

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

### Examples

#### Before Insert Trigger

```sql

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

Trigger quick reference: [Trigger Cheat Sheet](../../user-guides/Trigger-Cheat-Sheet.md)

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
- CREATE TRIGGER is fully parsed in the Firebird emulation layer
- NEW/OLD pseudo-records are captured in the trigger body source text
- Runtime triggers (C++-registered) expose old/new values via TriggerContext
- SQL trigger body procedural interpretation is not yet wired for runtime execution

---

## CREATE PACKAGE

### Description

Packages group related procedures and functions into a named module (Firebird 3.0+).

**Status**: Implemented (DDL parsing and catalog registration)

The Firebird parser accepts CREATE [OR ALTER] PACKAGE with header and body. Package definitions are registered in the catalog.

### Syntax

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

---

## CREATE EXCEPTION

### Description

Creates a user-defined exception that can be raised in PSQL code.

**Status**: Implemented (DDL parsing and catalog registration)

The Firebird parser accepts CREATE [OR ALTER] EXCEPTION with exception name and message. Exception definitions are registered in the catalog.

### Syntax

```sql
CREATE [OR ALTER] EXCEPTION exception_name 'error message';
```

### Standard Firebird Example

```sql

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

Executes a stored procedure.

**Status**: Implemented (parsing)

The Firebird parser accepts EXECUTE PROCEDURE with arguments.

### Syntax

```sql
EXECUTE PROCEDURE procedure_name [(param_value, ...)];

-- Or for selectable procedures:
SELECT * FROM procedure_name [(param_value, ...)];
```

---

## EXECUTE BLOCK

### Description

Executes anonymous PSQL code without creating a stored procedure.

**Status**: Implemented (parsing)

The Firebird parser accepts EXECUTE BLOCK with input parameters, RETURNS output definitions, DECLARE VARIABLE, and BEGIN/END body parsing. Procedural body runtime execution is not yet wired.

### Syntax

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


---

## PSQL Language Elements

### BEGIN ... END

Status: Parsed; bodies stored as source text. Runtime procedural execution not yet wired.

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

### What Works

**PSQL DDL (All Parsed and Catalog-Registered):**
- CREATE [OR ALTER] PROCEDURE with parameters, RETURNS, SQL SECURITY, and body capture
- CREATE [OR ALTER] FUNCTION with parameters, RETURNS, DETERMINISTIC, and body capture
- CREATE [OR ALTER] TRIGGER with FOR table, ACTIVE/INACTIVE, BEFORE/AFTER, events, POSITION, and body
- CREATE [OR ALTER] PACKAGE with header and body
- CREATE [OR ALTER] EXCEPTION with message
- DROP PROCEDURE/FUNCTION/TRIGGER/PACKAGE/EXCEPTION (all with IF EXISTS)
- EXECUTE PROCEDURE with arguments
- EXECUTE BLOCK with parameters, RETURNS, DECLARE VARIABLE, and body
- EXECUTE STATEMENT for dynamic SQL

**PSQL Language Elements (Parsed in Bodies):**
- BEGIN...END blocks, IF...THEN...ELSE, WHILE...DO, FOR SELECT...DO
- DECLARE VARIABLE, variable assignment, SELECT INTO
- EXIT, SUSPEND, RETURN, EXCEPTION raise

### Remaining Gaps

**Procedural Body Runtime Execution:**
- Function/procedure/trigger bodies are stored as source text in the catalog
- Complex procedural control flow (IF, WHILE, FOR, SUSPEND, exception handling) within bodies is not yet wired for runtime interpretation
- Simple single-statement bodies work through the standard execution path
- Runtime triggers (C++-registered) expose old/new values via TriggerContext, but SQL trigger body interpretation is not yet wired

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
