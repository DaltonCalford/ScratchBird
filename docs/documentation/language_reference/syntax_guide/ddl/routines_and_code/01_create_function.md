<!-- 
NOTE: Source code anchors in this document have been verified against the 
actual ScratchBird codebase. Any previously unverified claims have been removed.
Verification date: 2026-03-08
-->

# CREATE FUNCTION

[Prev](./README.md) | [Next](./02_alter_function.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md)

## Coverage and Evidence Status

Status: Complete

- Source anchor: /home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:1

## Synopsis

Creates a new function that can be called in SQL queries.

## Syntax

```sql
CREATE [ OR REPLACE ] FUNCTION function_name ( [ argmode ] [ argname ] argtype [ { DEFAULT | = } default_expr ] [, ...] )
    [ RETURNS rettype
      | RETURNS TABLE ( column_name column_type [, ...] ) ]
    { LANGUAGE lang_name
      | TRANSFORM { FOR TYPE type_name } [, ...]
      | WINDOW
      | IMMUTABLE | STABLE | VOLATILE
      | [ NOT ] LEAKPROOF
      | CALLED ON NULL INPUT | RETURNS NULL ON NULL INPUT | STRICT
      | [ EXTERNAL ] SECURITY INVOKER | [ EXTERNAL ] SECURITY DEFINER
      | PARALLEL { UNSAFE | RESTRICTED | SAFE }
      | COST execution_cost
      | ROWS result_rows
      | SUPPORT support_function
      | SET configuration_parameter { TO value | = value | FROM CURRENT }
      | AS 'definition'
      | AS 'obj_file', 'link_symbol'
    } ...
```

## Parameters

| Parameter | Description |
|-----------|-------------|
| `OR REPLACE` | Replace existing function |
| `argmode` | IN, OUT, INOUT, or VARIADIC |
| `argname` | Parameter name |
| `argtype` | Parameter data type |
| `rettype` | Return type |
| `LANGUAGE` | Implementation language (SQL, plpgsql, C, etc.) |
| `IMMUTABLE` | Same result for same inputs (can be cached) |
| `STABLE` | Same result within single query |
| `VOLATILE` | Result can change (default) |
| `SECURITY DEFINER` | Execute with function owner's privileges |
| `SECURITY INVOKER` | Execute with caller's privileges (default) |

## Examples

### SQL Functions

```sql
-- Simple function
CREATE FUNCTION add_numbers(a INTEGER, b INTEGER)
RETURNS INTEGER AS $$
    SELECT a + b;
$$ LANGUAGE SQL IMMUTABLE;

-- Function with default parameter
CREATE FUNCTION greet(name TEXT DEFAULT 'World')
RETURNS TEXT AS $$
    SELECT 'Hello, ' || name || '!';
$$ LANGUAGE SQL IMMUTABLE;

-- Function returning TABLE
CREATE FUNCTION get_active_users()
RETURNS TABLE (id UUID, name TEXT, email TEXT) AS $$
    SELECT id, name, email FROM users WHERE status = 'active';
$$ LANGUAGE SQL STABLE;

-- Function with multiple statements
CREATE FUNCTION calculate_order_total(order_id UUID)
RETURNS DECIMAL(10,2) AS $$
    SELECT COALESCE(SUM(quantity * price), 0)
    FROM order_items
    WHERE order_id = $1;
$$ LANGUAGE SQL STABLE;
```

### PL/pgSQL Functions

```sql
-- Function with variables and control flow
CREATE FUNCTION process_payment(
    p_user_id UUID,
    p_amount DECIMAL(10,2)
) RETURNS BOOLEAN AS $$
DECLARE
    v_balance DECIMAL(10,2);
BEGIN
    -- Get current balance
    SELECT balance INTO v_balance
    FROM accounts WHERE user_id = p_user_id;
    
    -- Check sufficient funds
    IF v_balance < p_amount THEN
        RETURN FALSE;
    END IF;
    
    -- Deduct amount
    UPDATE accounts 
    SET balance = balance - p_amount
    WHERE user_id = p_user_id;
    
    RETURN TRUE;
END;
$$ LANGUAGE plpgsql;

-- Function with exception handling
CREATE FUNCTION safe_divide(a NUMERIC, b NUMERIC)
RETURNS NUMERIC AS $$
BEGIN
    RETURN a / b;
EXCEPTION WHEN division_by_zero THEN
    RETURN NULL;
END;
$$ LANGUAGE plpgsql IMMUTABLE;
```

### Security Definer Functions

```sql
-- Function that runs as owner (for controlled access)
CREATE FUNCTION get_user_salary(user_id UUID)
RETURNS DECIMAL(10,2) AS $$
    SELECT salary FROM employees WHERE id = user_id;
$$ LANGUAGE SQL STABLE SECURITY DEFINER;

-- Grant execute to limited users
GRANT EXECUTE ON FUNCTION get_user_salary(UUID) TO manager_role;
```

### Variadic Functions

```sql
-- Variable number of arguments
CREATE FUNCTION sum_all(VARIADIC numbers NUMERIC[])
RETURNS NUMERIC AS $$
    SELECT COALESCE(SUM(n), 0) FROM unnest(numbers) AS n;
$$ LANGUAGE SQL IMMUTABLE;

-- Usage: SELECT sum_all(1, 2, 3, 4, 5);
```

## Function Volatility Categories

| Category | Description | Example |
|----------|-------------|---------|
| `IMMUTABLE` | Always same result for same args | `add_numbers(1, 2)` |
| `STABLE` | Same result within single query | `current_user_id()` |
| `VOLATILE` | Can return different results | `random()`, `now()` |

## Parser Acceptance Cases

```sql
CREATE FUNCTION f1() RETURNS INTEGER AS 'SELECT 1' LANGUAGE SQL;
CREATE FUNCTION f1(a INT) RETURNS INT AS 'SELECT a' LANGUAGE SQL;
CREATE OR REPLACE FUNCTION f1() RETURNS INT AS 'SELECT 1' LANGUAGE SQL IMMUTABLE;
```

## See Also

- [CREATE PROCEDURE](04_create_procedure.md)
- [CREATE TRIGGER](07_create_trigger.md)
- [DROP FUNCTION](03_drop_function.md)
