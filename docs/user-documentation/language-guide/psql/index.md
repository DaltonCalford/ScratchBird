# Procedural SQL (PSQL)

Stored procedures, functions, triggers, and exception handling.

[Back to Language Guide](../index.md)

---

## PSQL Features

| Topic | Description |
|-------|-------------|
| [Stored Procedures](procedures.md) | Server-side procedures |
| [Functions](functions.md) | User-defined functions |
| [Triggers](triggers.md) | Event-driven automation |
| [Exceptions](exceptions.md) | Error handling |

---

## Quick Reference

### Function

```sql
CREATE FUNCTION add_numbers(a INTEGER, b INTEGER)
RETURNS INTEGER AS $$
BEGIN
    RETURN a + b;
END;
$$ LANGUAGE plpgsql;
```

### Procedure

```sql
CREATE PROCEDURE transfer_funds(from_id INT, to_id INT, amount DECIMAL)
AS $$
BEGIN
    UPDATE accounts SET balance = balance - amount WHERE id = from_id;
    UPDATE accounts SET balance = balance + amount WHERE id = to_id;
END;
$$ LANGUAGE plpgsql;
```

### Trigger

```sql
CREATE TRIGGER update_timestamp
    BEFORE UPDATE ON users
    FOR EACH ROW
    EXECUTE FUNCTION set_updated_at();
```
