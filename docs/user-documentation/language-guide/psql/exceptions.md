# Exception Handling

Error handling in PL/pgSQL.

[Back to PSQL Index](index.md) | [Back to Language Guide](../index.md)

---

## Basic Syntax

```sql
BEGIN
    -- statements
EXCEPTION
    WHEN condition THEN
        -- handler
    WHEN condition THEN
        -- handler
END;
```

---

## Simple Exception Handler

```sql
CREATE FUNCTION safe_divide(a INTEGER, b INTEGER)
RETURNS INTEGER AS $$
BEGIN
    RETURN a / b;
EXCEPTION
    WHEN division_by_zero THEN
        RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

---

## Common Conditions

| Condition | Description |
|-----------|-------------|
| `division_by_zero` | Division by zero |
| `no_data_found` | Query returned no rows |
| `too_many_rows` | Query returned multiple rows |
| `unique_violation` | Unique constraint violated |
| `foreign_key_violation` | FK constraint violated |
| `not_null_violation` | NOT NULL constraint violated |
| `check_violation` | CHECK constraint violated |
| `numeric_value_out_of_range` | Number too large |
| `string_data_right_truncation` | String too long |
| `OTHERS` | Catch all other exceptions |

---

## Multiple Handlers

```sql
CREATE FUNCTION insert_user(p_name TEXT, p_email TEXT)
RETURNS INTEGER AS $$
DECLARE
    new_id INTEGER;
BEGIN
    INSERT INTO users (name, email) VALUES (p_name, p_email)
    RETURNING id INTO new_id;
    RETURN new_id;
EXCEPTION
    WHEN unique_violation THEN
        RAISE NOTICE 'Email already exists: %', p_email;
        RETURN NULL;
    WHEN not_null_violation THEN
        RAISE NOTICE 'Name and email are required';
        RETURN NULL;
    WHEN OTHERS THEN
        RAISE NOTICE 'Unexpected error: %', SQLERRM;
        RETURN NULL;
END;
$$ LANGUAGE plpgsql;
```

---

## Exception Information

| Variable | Description |
|----------|-------------|
| `SQLSTATE` | 5-character error code |
| `SQLERRM` | Error message |

### GET STACKED DIAGNOSTICS

```sql
EXCEPTION
    WHEN OTHERS THEN
        GET STACKED DIAGNOSTICS
            v_sqlstate = RETURNED_SQLSTATE,
            v_message = MESSAGE_TEXT,
            v_detail = PG_EXCEPTION_DETAIL,
            v_hint = PG_EXCEPTION_HINT,
            v_context = PG_EXCEPTION_CONTEXT;
```

---

## Raising Exceptions

### RAISE

```sql
-- Notice (informational)
RAISE NOTICE 'Processing user %', user_id;

-- Warning
RAISE WARNING 'Deprecated function called';

-- Exception (aborts)
RAISE EXCEPTION 'Invalid input: %', value;

-- With detail and hint
RAISE EXCEPTION 'User not found'
    USING DETAIL = 'User ID: ' || user_id,
          HINT = 'Check that the user exists';
```

### Custom SQLSTATE

```sql
RAISE EXCEPTION 'Custom error'
    USING ERRCODE = 'P0001';

-- Or
RAISE SQLSTATE 'P0001' USING MESSAGE = 'Custom error';
```

---

## Re-raising Exceptions

```sql
BEGIN
    -- some operation
EXCEPTION
    WHEN OTHERS THEN
        -- Log the error
        INSERT INTO error_log (message, sqlstate)
        VALUES (SQLERRM, SQLSTATE);
        -- Re-raise
        RAISE;
END;
```

---

## Nested Blocks

```sql
CREATE FUNCTION process_batch()
RETURNS void AS $$
DECLARE
    rec RECORD;
BEGIN
    FOR rec IN SELECT * FROM pending_items LOOP
        BEGIN
            -- Process each item
            PERFORM process_item(rec.id);
        EXCEPTION
            WHEN OTHERS THEN
                -- Log and continue
                INSERT INTO error_log (item_id, error)
                VALUES (rec.id, SQLERRM);
        END;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

---

## Savepoints in Exceptions

```sql
CREATE PROCEDURE transfer_with_logging(from_id INT, to_id INT, amount DECIMAL)
AS $$
BEGIN
    SAVEPOINT before_transfer;

    UPDATE accounts SET balance = balance - amount WHERE id = from_id;
    UPDATE accounts SET balance = balance + amount WHERE id = to_id;

EXCEPTION
    WHEN OTHERS THEN
        ROLLBACK TO SAVEPOINT before_transfer;
        INSERT INTO transfer_errors (from_id, to_id, amount, error)
        VALUES (from_id, to_id, amount, SQLERRM);
        RAISE;
END;
$$ LANGUAGE plpgsql;
```

---

## Custom Exception Types

```sql
-- Define condition name
DO $$
BEGIN
    -- Create custom exception
    RAISE EXCEPTION USING
        ERRCODE = 'P0001',
        MESSAGE = 'Custom business error';
END $$;
```

---

## Assert Statements

```sql
CREATE FUNCTION calculate_discount(amount DECIMAL)
RETURNS DECIMAL AS $$
BEGIN
    ASSERT amount > 0, 'Amount must be positive';

    RETURN amount * 0.10;
END;
$$ LANGUAGE plpgsql;
```

---

## Common Patterns

### Upsert with Error Handling

```sql
CREATE FUNCTION upsert_user(p_id INTEGER, p_name TEXT)
RETURNS TEXT AS $$
BEGIN
    INSERT INTO users (id, name) VALUES (p_id, p_name);
    RETURN 'inserted';
EXCEPTION
    WHEN unique_violation THEN
        UPDATE users SET name = p_name WHERE id = p_id;
        RETURN 'updated';
END;
$$ LANGUAGE plpgsql;
```

### Retry Logic

```sql
CREATE FUNCTION with_retry(max_attempts INTEGER DEFAULT 3)
RETURNS void AS $$
DECLARE
    attempts INTEGER := 0;
BEGIN
    LOOP
        attempts := attempts + 1;
        BEGIN
            -- Try the operation
            PERFORM risky_operation();
            EXIT;  -- Success, exit loop
        EXCEPTION
            WHEN OTHERS THEN
                IF attempts >= max_attempts THEN
                    RAISE;  -- Re-raise after max attempts
                END IF;
                RAISE NOTICE 'Attempt % failed, retrying...', attempts;
                PERFORM pg_sleep(1);  -- Wait before retry
        END;
    END LOOP;
END;
$$ LANGUAGE plpgsql;
```

### Validation with Multiple Checks

```sql
CREATE FUNCTION validate_order(order_data JSONB)
RETURNS void AS $$
BEGIN
    IF order_data->>'customer_id' IS NULL THEN
        RAISE EXCEPTION 'Customer ID required'
            USING ERRCODE = 'P0001';
    END IF;

    IF (order_data->>'total')::DECIMAL <= 0 THEN
        RAISE EXCEPTION 'Total must be positive'
            USING ERRCODE = 'P0002';
    END IF;

    -- More validations...
END;
$$ LANGUAGE plpgsql;
```

---

## SQLSTATE Codes

| Code | Meaning |
|------|---------|
| 00000 | Successful completion |
| 23505 | unique_violation |
| 23503 | foreign_key_violation |
| 23502 | not_null_violation |
| 23514 | check_violation |
| 22012 | division_by_zero |
| P0001 | raise_exception |
| P0002 | no_data_found |
| P0003 | too_many_rows |

---

## Best Practices

1. **Be specific** - Catch specific exceptions
2. **Use OTHERS sparingly** - Can hide bugs
3. **Always log** - Record unexpected errors
4. **Clean up** - Release resources in handlers
5. **Re-raise when needed** - Don't swallow errors silently

---

## See Also

- [Stored Procedures](procedures.md)
- [Functions](functions.md)
- [Troubleshooting](../../admin/troubleshooting.md)
