# User-Defined Functions

Create custom SQL functions.

[Back to PSQL Index](index.md) | [Back to Language Guide](../index.md)

---

## Function vs Procedure

| Feature | Function | Procedure |
|---------|----------|-----------|
| Returns value | Yes | No (OUT params only) |
| Use in SELECT | Yes | No |
| Transaction control | No | Yes |
| CALL keyword | No | Yes |

---

## Basic Syntax

```sql
CREATE [ OR REPLACE ] FUNCTION function_name ( [ parameters ] )
RETURNS return_type AS $$
[ DECLARE declarations ]
BEGIN
    statements;
    RETURN value;
END;
$$ LANGUAGE plpgsql;
```

---

## Simple Functions

### Scalar Function

```sql
CREATE FUNCTION add_numbers(a INTEGER, b INTEGER)
RETURNS INTEGER AS $$
BEGIN
    RETURN a + b;
END;
$$ LANGUAGE plpgsql;

-- Use
SELECT add_numbers(5, 3);  -- 8
```

### SQL Language Function

```sql
CREATE FUNCTION get_user_email(user_id INTEGER)
RETURNS TEXT AS $$
    SELECT email FROM users WHERE id = user_id;
$$ LANGUAGE SQL;
```

---

## Return Types

### Single Value

```sql
CREATE FUNCTION calculate_tax(amount DECIMAL)
RETURNS DECIMAL AS $$
BEGIN
    RETURN amount * 0.0825;
END;
$$ LANGUAGE plpgsql;
```

### Multiple Values (Record)

```sql
CREATE FUNCTION get_user_info(user_id INTEGER)
RETURNS TABLE (name TEXT, email TEXT, created_at TIMESTAMP) AS $$
BEGIN
    RETURN QUERY
    SELECT u.name, u.email, u.created_at
    FROM users u
    WHERE u.id = user_id;
END;
$$ LANGUAGE plpgsql;

-- Use
SELECT * FROM get_user_info(1);
```

### Set of Rows

```sql
CREATE FUNCTION get_active_users()
RETURNS SETOF users AS $$
BEGIN
    RETURN QUERY SELECT * FROM users WHERE active = TRUE;
END;
$$ LANGUAGE plpgsql;

-- Use
SELECT * FROM get_active_users();
```

### OUT Parameters

```sql
CREATE FUNCTION get_order_stats(
    order_id INTEGER,
    OUT item_count INTEGER,
    OUT total_amount DECIMAL
) AS $$
BEGIN
    SELECT COUNT(*), SUM(price * quantity)
    INTO item_count, total_amount
    FROM order_items
    WHERE order_id = get_order_stats.order_id;
END;
$$ LANGUAGE plpgsql;

-- Use
SELECT * FROM get_order_stats(123);
```

---

## Parameters

### Default Values

```sql
CREATE FUNCTION greet(name TEXT DEFAULT 'World')
RETURNS TEXT AS $$
BEGIN
    RETURN 'Hello, ' || name || '!';
END;
$$ LANGUAGE plpgsql;

SELECT greet();           -- 'Hello, World!'
SELECT greet('Alice');    -- 'Hello, Alice!'
```

### Variadic Parameters

```sql
CREATE FUNCTION sum_all(VARIADIC numbers INTEGER[])
RETURNS INTEGER AS $$
DECLARE
    total INTEGER := 0;
    n INTEGER;
BEGIN
    FOREACH n IN ARRAY numbers LOOP
        total := total + n;
    END LOOP;
    RETURN total;
END;
$$ LANGUAGE plpgsql;

SELECT sum_all(1, 2, 3, 4, 5);  -- 15
```

---

## Variables and Types

```sql
CREATE FUNCTION complex_calculation(input_id INTEGER)
RETURNS DECIMAL AS $$
DECLARE
    base_value DECIMAL(10,2);
    multiplier DECIMAL(4,2) := 1.5;
    result DECIMAL(10,2);
    row_data RECORD;
    user_row users%ROWTYPE;  -- Row type from table
BEGIN
    SELECT * INTO user_row FROM users WHERE id = input_id;
    -- ...
    RETURN result;
END;
$$ LANGUAGE plpgsql;
```

---

## Control Structures

### IF-THEN-ELSE

```sql
CREATE FUNCTION get_discount(total DECIMAL)
RETURNS DECIMAL AS $$
BEGIN
    IF total > 1000 THEN
        RETURN 0.20;
    ELSIF total > 500 THEN
        RETURN 0.10;
    ELSIF total > 100 THEN
        RETURN 0.05;
    ELSE
        RETURN 0;
    END IF;
END;
$$ LANGUAGE plpgsql;
```

### CASE

```sql
CREATE FUNCTION status_label(status_code CHAR)
RETURNS TEXT AS $$
BEGIN
    RETURN CASE status_code
        WHEN 'A' THEN 'Active'
        WHEN 'P' THEN 'Pending'
        WHEN 'S' THEN 'Suspended'
        ELSE 'Unknown'
    END;
END;
$$ LANGUAGE plpgsql;
```

### Loops

```sql
CREATE FUNCTION sum_range(start_val INTEGER, end_val INTEGER)
RETURNS INTEGER AS $$
DECLARE
    total INTEGER := 0;
    i INTEGER;
BEGIN
    FOR i IN start_val..end_val LOOP
        total := total + i;
    END LOOP;
    RETURN total;
END;
$$ LANGUAGE plpgsql;
```

---

## Query Results

### INTO Variable

```sql
CREATE FUNCTION get_user_count()
RETURNS INTEGER AS $$
DECLARE
    cnt INTEGER;
BEGIN
    SELECT COUNT(*) INTO cnt FROM users;
    RETURN cnt;
END;
$$ LANGUAGE plpgsql;
```

### INTO STRICT

```sql
CREATE FUNCTION get_user_name(user_id INTEGER)
RETURNS TEXT AS $$
DECLARE
    user_name TEXT;
BEGIN
    SELECT name INTO STRICT user_name FROM users WHERE id = user_id;
    RETURN user_name;
EXCEPTION
    WHEN NO_DATA_FOUND THEN
        RETURN NULL;
    WHEN TOO_MANY_ROWS THEN
        RAISE EXCEPTION 'Multiple users found';
END;
$$ LANGUAGE plpgsql;
```

### RETURN QUERY

```sql
CREATE FUNCTION search_users(search_term TEXT)
RETURNS TABLE (id INTEGER, name TEXT) AS $$
BEGIN
    RETURN QUERY
    SELECT u.id, u.name
    FROM users u
    WHERE u.name ILIKE '%' || search_term || '%';
END;
$$ LANGUAGE plpgsql;
```

---

## Immutable, Stable, Volatile

| Attribute | Meaning |
|-----------|---------|
| IMMUTABLE | Same inputs always give same output |
| STABLE | Same within single query |
| VOLATILE | Can return different results (default) |

```sql
-- Pure calculation
CREATE FUNCTION square(n INTEGER)
RETURNS INTEGER AS $$
BEGIN
    RETURN n * n;
END;
$$ LANGUAGE plpgsql IMMUTABLE;

-- Reads database but doesn't modify
CREATE FUNCTION get_setting(key TEXT)
RETURNS TEXT AS $$
BEGIN
    RETURN (SELECT value FROM settings WHERE name = key);
END;
$$ LANGUAGE plpgsql STABLE;
```

---

## Common Patterns

### Timestamp Function for Triggers

```sql
CREATE FUNCTION set_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

### Validation Function

```sql
CREATE FUNCTION is_valid_email(email TEXT)
RETURNS BOOLEAN AS $$
BEGIN
    RETURN email ~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$';
END;
$$ LANGUAGE plpgsql IMMUTABLE;
```

### JSON Builder

```sql
CREATE FUNCTION user_to_json(user_id INTEGER)
RETURNS JSONB AS $$
BEGIN
    RETURN (
        SELECT JSONB_BUILD_OBJECT(
            'id', id,
            'name', name,
            'email', email,
            'orders', (
                SELECT JSONB_AGG(JSONB_BUILD_OBJECT('id', o.id, 'total', o.total))
                FROM orders o WHERE o.user_id = users.id
            )
        )
        FROM users WHERE id = user_id
    );
END;
$$ LANGUAGE plpgsql STABLE;
```

---

## Management

### List Functions

```sql
-- User functions
SELECT proname, proargtypes
FROM pg_proc
WHERE pronamespace = 'public'::regnamespace
  AND prokind = 'f';
```

### Drop Function

```sql
DROP FUNCTION function_name(parameter_types);
DROP FUNCTION IF EXISTS add_numbers(INTEGER, INTEGER);
```

---

## See Also

- [Stored Procedures](procedures.md)
- [Triggers](triggers.md)
- [Exceptions](exceptions.md)
