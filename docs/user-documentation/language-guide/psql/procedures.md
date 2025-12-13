# Stored Procedures

Server-side procedures for complex operations.

[Back to PSQL Index](index.md) | [Back to Language Guide](../index.md)

---

## Syntax

```sql
CREATE [ OR REPLACE ] PROCEDURE procedure_name ( [ parameters ] )
AS $$
[ DECLARE declarations ]
BEGIN
    statements;
END;
$$ LANGUAGE plpgsql;
```

---

## Basic Procedure

```sql
CREATE PROCEDURE greet(name TEXT)
AS $$
BEGIN
    RAISE NOTICE 'Hello, %!', name;
END;
$$ LANGUAGE plpgsql;

-- Call
CALL greet('World');
```

---

## Parameters

### Input Parameters

```sql
CREATE PROCEDURE update_salary(emp_id INTEGER, new_salary DECIMAL)
AS $$
BEGIN
    UPDATE employees SET salary = new_salary WHERE id = emp_id;
END;
$$ LANGUAGE plpgsql;

CALL update_salary(1, 75000.00);
```

### Output Parameters

```sql
CREATE PROCEDURE get_stats(
    OUT total_count INTEGER,
    OUT avg_amount DECIMAL
)
AS $$
BEGIN
    SELECT COUNT(*), AVG(amount)
    INTO total_count, avg_amount
    FROM orders;
END;
$$ LANGUAGE plpgsql;

-- Use
CALL get_stats(NULL, NULL);
```

### INOUT Parameters

```sql
CREATE PROCEDURE increment_counter(INOUT counter INTEGER)
AS $$
BEGIN
    counter := counter + 1;
END;
$$ LANGUAGE plpgsql;
```

---

## Variables

```sql
CREATE PROCEDURE calculate_tax(order_id INTEGER)
AS $$
DECLARE
    subtotal DECIMAL(10,2);
    tax_rate DECIMAL(4,4) := 0.0825;
    total DECIMAL(10,2);
BEGIN
    SELECT amount INTO subtotal FROM orders WHERE id = order_id;
    total := subtotal * (1 + tax_rate);
    UPDATE orders SET amount = total WHERE id = order_id;
END;
$$ LANGUAGE plpgsql;
```

---

## Control Flow

### IF Statement

```sql
CREATE PROCEDURE update_status(order_id INTEGER)
AS $$
DECLARE
    order_total DECIMAL;
BEGIN
    SELECT total INTO order_total FROM orders WHERE id = order_id;

    IF order_total > 1000 THEN
        UPDATE orders SET status = 'priority' WHERE id = order_id;
    ELSIF order_total > 500 THEN
        UPDATE orders SET status = 'standard' WHERE id = order_id;
    ELSE
        UPDATE orders SET status = 'economy' WHERE id = order_id;
    END IF;
END;
$$ LANGUAGE plpgsql;
```

### CASE Statement

```sql
CREATE PROCEDURE categorize_customer(cust_id INTEGER)
AS $$
DECLARE
    total_spent DECIMAL;
    tier TEXT;
BEGIN
    SELECT SUM(total) INTO total_spent FROM orders WHERE customer_id = cust_id;

    tier := CASE
        WHEN total_spent > 10000 THEN 'platinum'
        WHEN total_spent > 5000 THEN 'gold'
        WHEN total_spent > 1000 THEN 'silver'
        ELSE 'bronze'
    END;

    UPDATE customers SET tier = tier WHERE id = cust_id;
END;
$$ LANGUAGE plpgsql;
```

---

## Loops

### FOR Loop

```sql
CREATE PROCEDURE process_orders()
AS $$
DECLARE
    rec RECORD;
BEGIN
    FOR rec IN SELECT * FROM orders WHERE status = 'pending' LOOP
        -- Process each order
        UPDATE orders SET status = 'processing' WHERE id = rec.id;
        RAISE NOTICE 'Processing order %', rec.id;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

### WHILE Loop

```sql
CREATE PROCEDURE retry_operation(max_attempts INTEGER)
AS $$
DECLARE
    attempts INTEGER := 0;
    success BOOLEAN := FALSE;
BEGIN
    WHILE attempts < max_attempts AND NOT success LOOP
        attempts := attempts + 1;
        BEGIN
            -- Try operation
            success := TRUE;
        EXCEPTION WHEN OTHERS THEN
            RAISE NOTICE 'Attempt % failed', attempts;
        END;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

### LOOP with EXIT

```sql
CREATE PROCEDURE batch_process()
AS $$
DECLARE
    batch_size INTEGER := 100;
    processed INTEGER;
BEGIN
    LOOP
        WITH batch AS (
            SELECT id FROM tasks WHERE status = 'pending' LIMIT batch_size
        )
        UPDATE tasks SET status = 'done' WHERE id IN (SELECT id FROM batch);

        GET DIAGNOSTICS processed = ROW_COUNT;

        EXIT WHEN processed = 0;

        COMMIT;  -- Commit each batch
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

---

## Transactions

Procedures can control transactions:

```sql
CREATE PROCEDURE transfer_funds(
    from_account INTEGER,
    to_account INTEGER,
    amount DECIMAL
)
AS $$
BEGIN
    -- Debit
    UPDATE accounts SET balance = balance - amount WHERE id = from_account;

    -- Credit
    UPDATE accounts SET balance = balance + amount WHERE id = to_account;

    -- Explicit commit
    COMMIT;
END;
$$ LANGUAGE plpgsql;
```

---

## Exception Handling

```sql
CREATE PROCEDURE safe_delete(user_id INTEGER)
AS $$
BEGIN
    DELETE FROM users WHERE id = user_id;
EXCEPTION
    WHEN foreign_key_violation THEN
        RAISE NOTICE 'Cannot delete user with related records';
    WHEN OTHERS THEN
        RAISE EXCEPTION 'Unexpected error: %', SQLERRM;
END;
$$ LANGUAGE plpgsql;
```

---

## Dynamic SQL

```sql
CREATE PROCEDURE archive_table(table_name TEXT)
AS $$
BEGIN
    EXECUTE format('CREATE TABLE %I_archive AS SELECT * FROM %I', table_name, table_name);
    EXECUTE format('TRUNCATE TABLE %I', table_name);
END;
$$ LANGUAGE plpgsql;
```

---

## Common Patterns

### Upsert Procedure

```sql
CREATE PROCEDURE upsert_user(p_id INTEGER, p_name TEXT, p_email TEXT)
AS $$
BEGIN
    INSERT INTO users (id, name, email)
    VALUES (p_id, p_name, p_email)
    ON CONFLICT (id) DO UPDATE SET
        name = EXCLUDED.name,
        email = EXCLUDED.email;
END;
$$ LANGUAGE plpgsql;
```

### Audit Logging

```sql
CREATE PROCEDURE audit_action(action TEXT, details JSONB)
AS $$
BEGIN
    INSERT INTO audit_log (action, details, created_at, created_by)
    VALUES (action, details, NOW(), current_user);
END;
$$ LANGUAGE plpgsql;
```

---

## Management

### List Procedures

```sql
SELECT proname, proargtypes
FROM pg_proc
WHERE prokind = 'p';
```

### Drop Procedure

```sql
DROP PROCEDURE procedure_name(parameter_types);
DROP PROCEDURE IF EXISTS transfer_funds(INTEGER, INTEGER, DECIMAL);
```

---

## See Also

- [Functions](functions.md)
- [Exceptions](exceptions.md)
