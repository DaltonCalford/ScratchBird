# Triggers

Automatic execution on data changes.

[Back to PSQL Index](index.md) | [Back to Language Guide](../index.md)

---

## Overview

Triggers execute functions automatically when INSERT, UPDATE, or DELETE occurs.

---

## Trigger Syntax

```sql
CREATE TRIGGER trigger_name
    { BEFORE | AFTER | INSTEAD OF }
    { INSERT | UPDATE | DELETE | TRUNCATE }
    [ OR ... ]
    ON table_name
    [ FOR [ EACH ] { ROW | STATEMENT } ]
    [ WHEN ( condition ) ]
    EXECUTE FUNCTION function_name();
```

---

## Trigger Function

Trigger functions must:
- Return type TRIGGER
- Return NEW, OLD, or NULL

```sql
CREATE FUNCTION my_trigger_function()
RETURNS TRIGGER AS $$
BEGIN
    -- For INSERT/UPDATE, modify NEW
    -- For DELETE, can access OLD
    RETURN NEW;  -- or RETURN OLD for DELETE
END;
$$ LANGUAGE plpgsql;
```

---

## Timing

| Timing | Description |
|--------|-------------|
| BEFORE | Before the operation |
| AFTER | After the operation |
| INSTEAD OF | Replace the operation (views only) |

---

## Common Examples

### Update Timestamp

```sql
CREATE FUNCTION set_updated_at()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at := CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER update_timestamp
    BEFORE UPDATE ON users
    FOR EACH ROW
    EXECUTE FUNCTION set_updated_at();
```

### Audit Log

```sql
CREATE FUNCTION audit_changes()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO audit_log (table_name, operation, new_data, created_at)
        VALUES (TG_TABLE_NAME, 'INSERT', ROW_TO_JSON(NEW), NOW());
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO audit_log (table_name, operation, old_data, new_data, created_at)
        VALUES (TG_TABLE_NAME, 'UPDATE', ROW_TO_JSON(OLD), ROW_TO_JSON(NEW), NOW());
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO audit_log (table_name, operation, old_data, created_at)
        VALUES (TG_TABLE_NAME, 'DELETE', ROW_TO_JSON(OLD), NOW());
    END IF;
    RETURN COALESCE(NEW, OLD);
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER audit_users
    AFTER INSERT OR UPDATE OR DELETE ON users
    FOR EACH ROW
    EXECUTE FUNCTION audit_changes();
```

### Validation

```sql
CREATE FUNCTION validate_email()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.email !~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$' THEN
        RAISE EXCEPTION 'Invalid email format: %', NEW.email;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER check_email
    BEFORE INSERT OR UPDATE ON users
    FOR EACH ROW
    EXECUTE FUNCTION validate_email();
```

### Maintain Counts

```sql
CREATE FUNCTION update_order_count()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        UPDATE customers SET order_count = order_count + 1
        WHERE id = NEW.customer_id;
    ELSIF TG_OP = 'DELETE' THEN
        UPDATE customers SET order_count = order_count - 1
        WHERE id = OLD.customer_id;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER maintain_order_count
    AFTER INSERT OR DELETE ON orders
    FOR EACH ROW
    EXECUTE FUNCTION update_order_count();
```

### Soft Delete

```sql
CREATE FUNCTION soft_delete()
RETURNS TRIGGER AS $$
BEGIN
    UPDATE users SET
        deleted_at = CURRENT_TIMESTAMP,
        deleted_by = current_user
    WHERE id = OLD.id;
    RETURN NULL;  -- Prevents actual delete
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER users_soft_delete
    BEFORE DELETE ON users
    FOR EACH ROW
    EXECUTE FUNCTION soft_delete();
```

---

## Trigger Variables

| Variable | Description |
|----------|-------------|
| `NEW` | New row (INSERT/UPDATE) |
| `OLD` | Old row (UPDATE/DELETE) |
| `TG_OP` | Operation ('INSERT', 'UPDATE', 'DELETE', 'TRUNCATE') |
| `TG_TABLE_NAME` | Table name |
| `TG_TABLE_SCHEMA` | Schema name |
| `TG_WHEN` | 'BEFORE', 'AFTER', 'INSTEAD OF' |
| `TG_LEVEL` | 'ROW' or 'STATEMENT' |

---

## Conditional Triggers

### WHEN Clause

```sql
CREATE TRIGGER log_price_changes
    AFTER UPDATE ON products
    FOR EACH ROW
    WHEN (OLD.price IS DISTINCT FROM NEW.price)
    EXECUTE FUNCTION log_price_change();
```

### Column-Specific

```sql
CREATE TRIGGER on_email_change
    AFTER UPDATE OF email ON users
    FOR EACH ROW
    EXECUTE FUNCTION notify_email_change();
```

---

## Statement-Level Triggers

Execute once per statement, not per row:

```sql
CREATE FUNCTION log_bulk_insert()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO operation_log (operation, table_name, timestamp)
    VALUES ('BULK_INSERT', TG_TABLE_NAME, NOW());
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER log_bulk_ops
    AFTER INSERT ON large_table
    FOR EACH STATEMENT
    EXECUTE FUNCTION log_bulk_insert();
```

---

## INSTEAD OF Triggers (Views)

```sql
CREATE VIEW active_users AS
SELECT id, name, email FROM users WHERE active = TRUE;

CREATE FUNCTION insert_active_user()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO users (name, email, active)
    VALUES (NEW.name, NEW.email, TRUE);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER insert_active_user_trigger
    INSTEAD OF INSERT ON active_users
    FOR EACH ROW
    EXECUTE FUNCTION insert_active_user();
```

---

## Managing Triggers

### List Triggers

```sql
SELECT
    trigger_name,
    event_manipulation,
    action_timing,
    action_orientation
FROM information_schema.triggers
WHERE event_object_table = 'users';
```

### Disable/Enable

```sql
-- Disable
ALTER TABLE users DISABLE TRIGGER update_timestamp;
ALTER TABLE users DISABLE TRIGGER ALL;

-- Enable
ALTER TABLE users ENABLE TRIGGER update_timestamp;
ALTER TABLE users ENABLE TRIGGER ALL;
```

### Drop Trigger

```sql
DROP TRIGGER trigger_name ON table_name;
DROP TRIGGER IF EXISTS update_timestamp ON users;
```

---

## Best Practices

1. **Keep triggers simple** - Complex logic in procedures
2. **Avoid cascading** - Triggers calling triggers
3. **Document triggers** - Easy to forget they exist
4. **Test performance** - Triggers add overhead
5. **Use WHEN clause** - Skip unnecessary executions

---

## See Also

- [Functions](functions.md)
- [Exceptions](exceptions.md)
