# CREATE TRIGGER

[Prev](./06_drop_procedure.md) | [Next](./08_alter_trigger.md) | [Topic README](./README.md) | [DDL README](../README.md) | [Syntax Guide README](../../README.md)

## Synopsis

Creates a trigger that executes a function automatically when specified events occur.

## Syntax

```sql
CREATE [ OR REPLACE ] TRIGGER trigger_name
    { BEFORE | AFTER | INSTEAD OF } { event [ OR ... ] }
    ON table_name
    [ FROM referenced_table_name ]
    [ NOT DEFERRABLE | [ DEFERRABLE ] [ INITIALLY IMMEDIATE | INITIALLY DEFERRED ] ]
    [ REFERENCING { { OLD | NEW } TABLE [ AS ] transition_relation_name } [ ... ] ]
    [ FOR [ EACH ] { ROW | STATEMENT } ]
    [ WHEN ( condition ) ]
    EXECUTE { FUNCTION | PROCEDURE } function_name ( arguments )

where event can be:
    INSERT
    UPDATE [ OF column_name [, ...] ]
    DELETE
    TRUNCATE
```

## Trigger Timing

| Timing | When Executes | Use Case |
|--------|---------------|----------|
| `BEFORE` | Before operation | Validation, transformation |
| `AFTER` | After operation | Auditing, cascading updates |
| `INSTEAD OF` | Replaces operation | Views, complex logic |

## Trigger Events

- `INSERT` - Row inserted
- `UPDATE` - Row updated
- `UPDATE OF column` - Specific column changed
- `DELETE` - Row deleted
- `TRUNCATE` - Table truncated

## Row vs Statement Level

| Level | Executes | Context |
|-------|----------|---------|
| `FOR EACH ROW` | Per affected row | Access to OLD/NEW records |
| `FOR EACH STATEMENT` | Once per statement | Statement-level operations |

## Examples

### Audit Trigger

```sql
-- Audit log function
CREATE OR REPLACE FUNCTION audit_trigger_func()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO audit_log (table_name, operation, new_data)
        VALUES (TG_TABLE_NAME, TG_OP, row_to_json(NEW));
        RETURN NEW;
    ELSIF TG_OP = 'UPDATE' THEN
        INSERT INTO audit_log (table_name, operation, old_data, new_data)
        VALUES (TG_TABLE_NAME, TG_OP, row_to_json(OLD), row_to_json(NEW));
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        INSERT INTO audit_log (table_name, operation, old_data)
        VALUES (TG_TABLE_NAME, TG_OP, row_to_json(OLD));
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

-- Create audit trigger
CREATE TRIGGER users_audit
AFTER INSERT OR UPDATE OR DELETE ON users
FOR EACH ROW
EXECUTE FUNCTION audit_trigger_func();
```

### Timestamp Trigger

```sql
-- Auto-update timestamps
CREATE OR REPLACE FUNCTION update_timestamp()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER users_updated_at
BEFORE UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION update_timestamp();
```

### Validation Trigger

```sql
-- Validate before insert/update
CREATE OR REPLACE FUNCTION validate_email()
RETURNS TRIGGER AS $$
BEGIN
    IF NEW.email !~ '^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}$' THEN
        RAISE EXCEPTION 'Invalid email format: %', NEW.email;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER validate_user_email
BEFORE INSERT OR UPDATE ON users
FOR EACH ROW
EXECUTE FUNCTION validate_email();
```

### Conditional Trigger

```sql
-- Only trigger for significant changes
CREATE TRIGGER notify_price_change
AFTER UPDATE OF price ON products
FOR EACH ROW
WHEN (OLD.price IS DISTINCT FROM NEW.price)
EXECUTE FUNCTION notify_price_change();
```

### INSTEAD OF Trigger (Views)

```sql
-- Make view updatable
CREATE OR REPLACE VIEW user_summary AS
SELECT u.id, u.name, COUNT(o.id) AS order_count
FROM users u LEFT JOIN orders o ON u.id = o.user_id
GROUP BY u.id, u.name;

CREATE OR REPLACE FUNCTION update_user_summary()
RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' THEN
        INSERT INTO users (id, name) VALUES (NEW.id, NEW.name);
        RETURN NEW;
    ELSIF TG_OP = 'UPDATE' THEN
        UPDATE users SET name = NEW.name WHERE id = OLD.id;
        RETURN NEW;
    ELSIF TG_OP = 'DELETE' THEN
        DELETE FROM users WHERE id = OLD.id;
        RETURN OLD;
    END IF;
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER user_summary_update
INSTEAD OF INSERT OR UPDATE OR DELETE ON user_summary
FOR EACH ROW
EXECUTE FUNCTION update_user_summary();
```

### Statement-Level Trigger

```sql
-- Log statement-level changes
CREATE OR REPLACE FUNCTION log_statement_changes()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO statement_log (table_name, operation, changed_at)
    VALUES (TG_TABLE_NAME, TG_OP, NOW());
    RETURN NULL;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER users_statement_log
AFTER INSERT OR UPDATE OR DELETE ON users
FOR EACH STATEMENT
EXECUTE FUNCTION log_statement_changes();
```

## Trigger Variables

| Variable | Description |
|----------|-------------|
| `NEW` | New row (INSERT/UPDATE) |
| `OLD` | Old row (UPDATE/DELETE) |
| `TG_NAME` | Trigger name |
| `TG_TABLE_NAME` | Table name |
| `TG_OP` | Operation (INSERT/UPDATE/DELETE/TRUNCATE) |

## See Also

- [CREATE FUNCTION](01_create_function.md)
- [DROP TRIGGER](09_drop_trigger.md)
