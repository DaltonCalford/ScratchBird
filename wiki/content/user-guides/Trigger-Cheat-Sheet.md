# Trigger Cheat Sheet

**Last Updated:** 2026-01-28

Quick reference for trigger syntax, timing, and context variables.

## Common trigger forms

### Table triggers

```
CREATE TRIGGER trigger_name
    { BEFORE | AFTER | INSTEAD OF }
    { INSERT | UPDATE | DELETE [ OF column_list ] } [ OR ... ]
    ON table_name
    [ FOR EACH { ROW | STATEMENT } ]
    [ WHEN ( condition ) ]
    [ POSITION integer ]
    EXECUTE { PROCEDURE | FUNCTION } routine_name([args]);
```

### Database triggers

```
CREATE TRIGGER trigger_name
    { ON CONNECT | ON DISCONNECT | ON TRANSACTION { START | COMMIT | ROLLBACK } }
    | AFTER { CREATE | ALTER | DROP }
    [ WHEN ( condition ) ]
    [ POSITION integer ]
    EXECUTE { PROCEDURE | FUNCTION } routine_name([args]);
```

## Timing and scope

| Timing | Meaning |
| --- | --- |
| BEFORE | Run before the triggering statement |
| AFTER | Run after the triggering statement |
| INSTEAD OF | Replace DML for views |

| Scope | Meaning |
| --- | --- |
| FOR EACH ROW | Trigger runs once per row |
| FOR EACH STATEMENT | Trigger runs once per statement |

## NEW and OLD records

- NEW is available for INSERT and UPDATE.
- OLD is available for UPDATE and DELETE.

Example:

```
IF OLD.status <> NEW.status THEN
    INSERT INTO audit_log (id, old_status, new_status)
    VALUES (NEW.id, OLD.status, NEW.status);
END IF;
```

## Trigger context variables (selection)

| Variable | Purpose |
| --- | --- |
| GET TRIGGER_EVENT | INSERT/UPDATE/DELETE/SELECT |
| GET TRIGGER_TIMING | BEFORE/AFTER/INSTEAD OF |
| GET TRIGGER_LEVEL | ROW/STATEMENT |
| GET TRIGGER_NAME | Trigger name |
| GET TRIGGER_TABLE | Table name |
| GET TRIGGER_SCHEMA | Schema name |
| GET TRIGGER_TIMESTAMP | Fire time |
| GET TRIGGER_TRANSACTION_ID | Transaction ID |
| GET TRIGGER_COMMAND_TAG | SQL command |

Column change helpers:

- IS COLUMN CHANGED(column_name)
- GET CHANGED_COLUMNS
- GET COLUMN_OLD_VALUE(column_name)
- GET COLUMN_NEW_VALUE(column_name)
- GET COLUMN_DELTA(column_name)

## Example patterns

### Auto-update a column

```
CREATE TRIGGER set_modified
    BEFORE UPDATE ON products
    FOR EACH ROW
    EXECUTE FUNCTION set_modified_ts();
```

### Audit changes

```
CREATE TRIGGER audit_changes
    AFTER UPDATE OR DELETE ON accounts
    FOR EACH ROW
    EXECUTE FUNCTION audit_account_change();
```

### Prevent deletes

```
CREATE TRIGGER block_admin_delete
    BEFORE DELETE ON users
    FOR EACH ROW
    EXECUTE FUNCTION guard_admin_delete();
```

## Implementation note (Alpha)

Trigger metadata is stored, but SQL trigger procedures are not interpreted at
runtime yet. The executor expects registered C++ trigger procedures.

## References

- `docs/specifications/ddl/DDL_TRIGGERS.md`
- `docs/specifications/triggers/TRIGGER_CONTEXT_VARIABLES.md`
- `docs/specifications/parser/05_PSQL_PROCEDURAL_LANGUAGE.md`
