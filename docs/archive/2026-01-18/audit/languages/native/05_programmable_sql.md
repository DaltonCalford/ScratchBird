# Native V2 SQL - Programmable SQL (PSQL)

## Overview

This document describes programmable SQL features in ScratchBird's Native V2 SQL dialect, including stored procedures, functions, triggers, and procedural blocks.

**CRITICAL STATUS:** Programmable SQL features are largely **not implemented** in the V2 parser. This document describes the intended functionality based on specifications. Most features listed here are planned but not yet available.

**Parser Pipeline:** V2 Parser → AST v2 → SemanticAnalyzerV2 → BytecodeGeneratorV2 → Executor

**Source Code References:**
- Parser: `/ScratchBird/src/parser/parser_v2.cpp`
- AST: `/ScratchBird/include/scratchbird/parser/ast_v2.h`
- Semantic/Bytecode: `/ScratchBird/src/sblr/semantic_analyzer_v2.cpp`, `/ScratchBird/src/sblr/bytecode_generator_v2.cpp`
- Executor: `/ScratchBird/src/sblr/executor.cpp`

---

## CREATE FUNCTION

### Description

**STATUS: NOT IMPLEMENTED IN V2 PARSER**

Creates a stored function that returns a value. Functions can be used in SQL expressions and must return a value.

### Syntax (Planned)

```sql
CREATE [OR REPLACE] FUNCTION <function_name> (
    [<parameter_name> <parameter_type> [, ...]]
)
RETURNS <return_type>
AS
BEGIN
    <function_body>
END
```

### Parameters (Planned)

- **OR REPLACE**: Replaces existing function with same name
- **function_name**: Name of the function
- **parameter_name**: Parameter name
- **parameter_type**: Data type of parameter
- **return_type**: Return data type
- **function_body**: Procedural SQL statements

### Examples (Planned)

**Example 1: Simple function**
```sql
CREATE FUNCTION calculate_tax(amount DECIMAL)
RETURNS DECIMAL
AS
BEGIN
    RETURN amount * 0.08;
END
```

**Example 2: Function with conditional logic**
```sql
CREATE FUNCTION get_discount(customer_level VARCHAR)
RETURNS DECIMAL
AS
BEGIN
    IF customer_level = 'gold' THEN
        RETURN 0.20;
    ELSIF customer_level = 'silver' THEN
        RETURN 0.10;
    ELSE
        RETURN 0.05;
    END IF;
END
```

**Example 3: Function querying data**
```sql
CREATE FUNCTION get_customer_total(cust_id INTEGER)
RETURNS DECIMAL
AS
    total DECIMAL;
BEGIN
    SELECT SUM(amount) INTO total
    FROM orders
    WHERE customer_id = cust_id;
    RETURN COALESCE(total, 0);
END
```

### Notes

- AST nodes exist for function definitions but are not parsed in V2
- Parser has TODO markers for function implementation
- Spec reference: `/docs/specifications/ddl/DDL_FUNCTIONS.md`

---

## CREATE PROCEDURE

### Description

**STATUS: NOT IMPLEMENTED IN V2 PARSER**

Creates a stored procedure for executing procedural logic. Unlike functions, procedures don't return values directly but can have OUT parameters.

### Syntax (Planned)

```sql
CREATE [OR REPLACE] PROCEDURE <procedure_name> (
    [<parameter_name> [IN | OUT | INOUT] <parameter_type> [, ...]]
)
AS
BEGIN
    <procedure_body>
END
```

### Parameters (Planned)

- **OR REPLACE**: Replaces existing procedure
- **procedure_name**: Name of the procedure
- **IN**: Input parameter (default)
- **OUT**: Output parameter
- **INOUT**: Input/output parameter
- **procedure_body**: Procedural SQL statements

### Examples (Planned)

**Example 1: Simple procedure**
```sql
CREATE PROCEDURE update_user_status(
    user_id INTEGER,
    new_status VARCHAR
)
AS
BEGIN
    UPDATE users
    SET status = new_status,
        updated_at = CURRENT_TIMESTAMP
    WHERE id = user_id;
END
```

**Example 2: Procedure with OUT parameter**
```sql
CREATE PROCEDURE create_order(
    cust_id INTEGER,
    amount DECIMAL,
    OUT order_id INTEGER
)
AS
BEGIN
    INSERT INTO orders (customer_id, amount, created_at)
    VALUES (cust_id, amount, CURRENT_TIMESTAMP)
    RETURNING id INTO order_id;
END
```

**Example 3: Complex procedure with transactions**
```sql
CREATE PROCEDURE process_payment(
    order_id INTEGER,
    payment_amount DECIMAL
)
AS
BEGIN
    -- Update order
    UPDATE orders
    SET paid_amount = paid_amount + payment_amount
    WHERE id = order_id;

    -- Record payment
    INSERT INTO payments (order_id, amount, paid_at)
    VALUES (order_id, payment_amount, CURRENT_TIMESTAMP);

    -- Update status if fully paid
    UPDATE orders
    SET status = 'paid'
    WHERE id = order_id
      AND paid_amount >= total_amount;

    COMMIT;
EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK;
        RAISE;
END
```

### Notes

- AST nodes exist but not parsed in V2
- Firebird-style procedure syntax planned
- Spec reference: `/docs/specifications/ddl/DDL_PROCEDURES.md`

---

## CREATE TRIGGER

### Description

**STATUS: NOT IMPLEMENTED IN V2 PARSER**

Creates a trigger that automatically executes in response to INSERT, UPDATE, or DELETE operations on a table.

### Syntax (Planned)

```sql
CREATE [OR REPLACE] TRIGGER <trigger_name>
    FOR <table_name>
    [BEFORE | AFTER] [INSERT | UPDATE | DELETE]
    [POSITION <number>]
AS
BEGIN
    <trigger_body>
END
```

### Parameters (Planned)

- **OR REPLACE**: Replaces existing trigger
- **trigger_name**: Name of the trigger
- **table_name**: Table the trigger is attached to
- **BEFORE | AFTER**: Execution timing
- **INSERT | UPDATE | DELETE**: Event type
- **POSITION**: Execution order (lower numbers first)
- **trigger_body**: Procedural SQL statements

### Examples (Planned)

**Example 1: Before insert trigger**
```sql
CREATE TRIGGER set_created_timestamp
    FOR users
    BEFORE INSERT
AS
BEGIN
    NEW.created_at = CURRENT_TIMESTAMP;
END
```

**Example 2: After update trigger**
```sql
CREATE TRIGGER log_status_change
    FOR orders
    AFTER UPDATE
AS
BEGIN
    IF OLD.status <> NEW.status THEN
        INSERT INTO order_status_log (order_id, old_status, new_status, changed_at)
        VALUES (NEW.id, OLD.status, NEW.status, CURRENT_TIMESTAMP);
    END IF;
END
```

**Example 3: Before delete trigger**
```sql
CREATE TRIGGER prevent_admin_delete
    FOR users
    BEFORE DELETE
AS
BEGIN
    IF OLD.role = 'admin' THEN
        EXCEPTION custom_exception 'Cannot delete admin users';
    END IF;
END
```

### Notes

- AST nodes exist but not parsed in V2
- Firebird-style trigger syntax planned
- Spec reference: `/docs/specifications/ddl/DDL_TRIGGERS.md`

---

### Trigger Context Variables (NEW / OLD)

#### Description
Row-level triggers expose pseudo-records for column values before and after the
change.

#### Syntax (Planned)
```sql
NEW.<column>
OLD.<column>
```

#### Examples (Planned)
```sql
-- BEFORE INSERT
NEW.created_at = CURRENT_TIMESTAMP;

-- AFTER UPDATE
IF OLD.status <> NEW.status THEN
    INSERT INTO audit_log (id, old_status, new_status)
    VALUES (NEW.id, OLD.status, NEW.status);
END IF;
```

#### Status
- Not implemented in V2 parser (CREATE TRIGGER is not parsed).
- Runtime triggers (C++-registered) expose old/new values via TriggerContext,
  but there is no SQL trigger interpreter.

---

## EXECUTE BLOCK

### Description

**STATUS: NOT IMPLEMENTED IN V2 PARSER**

Executes an anonymous procedural block without creating a stored procedure.

### Syntax (Planned)

```sql
EXECUTE BLOCK [(parameter_declarations)]
[RETURNS (output_definitions)]
AS
    [<variable_declarations>]
BEGIN
    <procedural_statements>
END
```

### Examples (Planned)

**Example 1: Simple anonymous block**
```sql
EXECUTE BLOCK
AS
BEGIN
    UPDATE users SET last_login = CURRENT_TIMESTAMP WHERE id = 42;
END
```

**Example 2: Block with variables**
```sql
EXECUTE BLOCK
AS
    DECLARE total_count INTEGER;
    DECLARE avg_amount DECIMAL;
BEGIN
    SELECT COUNT(*), AVG(amount)
    INTO total_count, avg_amount
    FROM orders
    WHERE status = 'completed';

    INSERT INTO daily_stats (stat_date, order_count, avg_order)
    VALUES (CURRENT_DATE, total_count, avg_amount);
END
```

**Example 3: Block with returns**
```sql
EXECUTE BLOCK
RETURNS (user_id INTEGER, user_name VARCHAR(100))
AS
BEGIN
    FOR SELECT id, name FROM users WHERE active = 1
        INTO user_id, user_name
    DO
        SUSPEND;
END
```

### Notes

- Useful for one-time complex operations
- Can have input parameters and output results
- SUSPEND statement yields row in result set
- Not parsed in V2

---

## Procedural Statements

### Description

**STATUS: NOT IMPLEMENTED IN V2 PARSER**

Procedural SQL provides flow control and variable handling within functions, procedures, triggers, and blocks.

### Control Flow (Planned)

**IF Statement:**
```sql
IF condition THEN
    statements;
ELSIF condition THEN
    statements;
ELSE
    statements;
END IF;
```

**WHILE Loop:**
```sql
WHILE condition DO
BEGIN
    statements;
END
```

**FOR Loop:**
```sql
FOR variable IN (select_statement) DO
BEGIN
    statements;
END
```

**FOR Loop (numeric):**
```sql
FOR variable = start_value TO end_value [BY step] DO
BEGIN
    statements;
END
```

### Variables (Planned)

**Declaration:**
```sql
DECLARE variable_name type [DEFAULT value];
```

**Assignment:**
```sql
variable_name = expression;
variable_name := expression; -- Alternative syntax
```

**SELECT INTO:**
```sql
SELECT column1, column2
INTO variable1, variable2
FROM table
WHERE condition;
```

### Other Statements (Planned)

**SUSPEND:**
```sql
SUSPEND; -- Yield current row in result set
```

**EXIT:**
```sql
EXIT; -- Exit loop or procedure
```

**RETURN:**
```sql
RETURN value; -- Return from function
```

**EXCEPTION:**
```sql
EXCEPTION exception_name 'message';
```

### Notes

- Full Firebird-style procedural SQL planned
- Not implemented in V2 parser
- Spec reference: `/docs/specifications/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md`

---

## EXECUTE PROCEDURE / EXECUTE STATEMENT

### Description

**STATUS: NOT IMPLEMENTED IN V2 PARSER**

Executes a stored procedure or dynamic SQL statement.

### Syntax (Planned)

```sql
EXECUTE PROCEDURE procedure_name [(parameters)]
    [RETURNING_VALUES variable_list];

EXECUTE STATEMENT sql_string
    [WITH AUTONOMOUS TRANSACTION]
    [INTO variable_list];
```

### Examples (Planned)

**Example 1: Execute procedure**
```sql
EXECUTE PROCEDURE update_statistics;
```

**Example 2: Execute procedure with parameters**
```sql
EXECUTE PROCEDURE create_user('alice', 'alice@example.com')
    RETURNING_VALUES :new_user_id;
```

**Example 3: Execute dynamic SQL**
```sql
EXECUTE STATEMENT 'UPDATE ' || :table_name || ' SET active = 1';
```

### Notes

- Required for calling stored procedures
- Dynamic SQL execution for runtime-generated queries
- Not implemented in V2 parser

---

## Known Limitations

### Missing Features

**All Programmable SQL:**
- CREATE FUNCTION not parsed in V2
- CREATE PROCEDURE not parsed in V2
- CREATE TRIGGER not parsed in V2
- EXECUTE BLOCK not parsed in V2
- EXECUTE PROCEDURE not parsed
- EXECUTE STATEMENT not parsed
- All procedural statements (IF, WHILE, FOR, etc.) not parsed
- Variable declarations not supported
- Exception handling not implemented
- Spec references:
  - `/docs/specifications/ddl/DDL_FUNCTIONS.md`
  - `/docs/specifications/ddl/DDL_PROCEDURES.md`
  - `/docs/specifications/ddl/DDL_TRIGGERS.md`

**Parser Status:**
- AST nodes defined in `ast_v2.h` for functions, procedures, triggers
- Parser has TODO comments indicating planned implementation
- No bytecode generation or executor support currently
- Critical finding documented in `/docs/audit/parsers/CRITICAL_FINDINGS.md`

**Alternative Approaches:**
- Use multiple SQL statements instead of procedures
- Implement logic in application layer
- Use emulated database procedures (Firebird, PostgreSQL, etc.) via CREATE DATABASE EMULATED

### Implementation Priority

Programmable SQL is a **critical gap** for Firebird compatibility and advanced database functionality:

1. **High Priority**: Basic CREATE FUNCTION/PROCEDURE parsing
2. **High Priority**: Simple procedural blocks (BEGIN/END, IF, assignments)
3. **Medium Priority**: Triggers (BEFORE/AFTER INSERT/UPDATE/DELETE)
4. **Medium Priority**: Loops (WHILE, FOR)
5. **Medium Priority**: Exception handling
6. **Lower Priority**: EXECUTE BLOCK with SUSPEND
7. **Lower Priority**: Dynamic SQL (EXECUTE STATEMENT)

### Spec References

- Functions: `/docs/specifications/ddl/DDL_FUNCTIONS.md`
- Procedures: `/docs/specifications/ddl/DDL_PROCEDURES.md`
- Triggers: `/docs/specifications/ddl/DDL_TRIGGERS.md`
- Transaction model: `/docs/specifications/sblr/FIREBIRD_TRANSACTION_MODEL_SPEC.md`
- Parser audit: `/docs/audit/parsers/V2/SUMMARY.md`
- Critical findings: `/docs/audit/parsers/CRITICAL_FINDINGS.md`

### General Notes

- This is the largest gap in V2 parser compared to Firebird and PostgreSQL
- Implementation would significantly improve Firebird migration compatibility
- All features are spec-defined but not implemented
- Work is planned for future Alpha/Beta phases
