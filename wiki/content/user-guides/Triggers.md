# Triggers

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

Triggers are database callbacks that fire automatically on DML, DDL, or session
events. They are compiled to SBLR and executed by the engine with full security
checks.

Quick reference: [Trigger Cheat Sheet](Trigger-Cheat-Sheet.md)

## Trigger types

Table-level (DML) triggers:
- BEFORE/AFTER INSERT, UPDATE, DELETE
- INSTEAD OF triggers for views
- Row-level (FOR EACH ROW) or statement-level (FOR EACH STATEMENT)

Database-level triggers:
- ON CONNECT / ON DISCONNECT
- ON TRANSACTION START / COMMIT / ROLLBACK
- AFTER CREATE / ALTER / DROP

## CREATE TRIGGER (table-level)

```
CREATE [OR REPLACE] TRIGGER trigger_name
    { BEFORE | AFTER | INSTEAD OF }
    { INSERT | DELETE | UPDATE [ OF column_list ] } [ OR ... ]
    ON table_name
    [ REFERENCING { OLD [AS] old_alias | NEW [AS] new_alias } ... ]
    [ FOR EACH { ROW | STATEMENT } ]
    [ WHEN ( condition ) ]
    [ POSITION integer ]
    EXECUTE { PROCEDURE | FUNCTION } function_name([arguments]);
```

## CREATE TRIGGER (database-level)

```
CREATE [OR REPLACE] TRIGGER trigger_name
    { ON CONNECT | ON DISCONNECT | ON TRANSACTION { START | COMMIT | ROLLBACK } }
    | AFTER { CREATE | ALTER | DROP }
    [ WHEN ( condition ) ]
    [ POSITION integer ]
    EXECUTE { PROCEDURE | FUNCTION } function_name([arguments]);
```

## NEW and OLD records

Row-level triggers can reference:

- NEW.column for INSERT/UPDATE
- OLD.column for UPDATE/DELETE

You can also alias them via REFERENCING or by using NEW/OLD directly.

## Trigger context variables (core)

Common context variables and helpers from the trigger specification:

- GET TRIGGER_EVENT (INSERT, UPDATE, DELETE, SELECT)
- GET TRIGGER_TIMING (BEFORE, AFTER, INSTEAD OF)
- GET TRIGGER_LEVEL (ROW, STATEMENT)
- GET TRIGGER_NAME / GET TRIGGER_SCHEMA / GET TRIGGER_TABLE
- GET TRIGGER_USER / GET TRIGGER_SESSION_USER
- GET TRIGGER_TIMESTAMP
- GET TRIGGER_TRANSACTION_ID
- GET TRIGGER_COMMAND_TAG

Column change helpers:

- IS COLUMN CHANGED(column_name)
- GET CHANGED_COLUMNS
- GET COLUMN_OLD_VALUE(column_name)
- GET COLUMN_NEW_VALUE(column_name)

Statement-level triggers may access transition tables (OLD_TABLE, NEW_TABLE)
where supported.

## Example

```
CREATE FUNCTION update_modified_column() RETURNS TRIGGER AS $$
BEGIN
    NEW.last_modified = CURRENT_TIMESTAMP;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER set_last_modified
    BEFORE UPDATE ON products
    FOR EACH ROW
    EXECUTE FUNCTION update_modified_column();
```

## References

- `docs/specifications/ddl/DDL_TRIGGERS.md`
- `docs/specifications/triggers/TRIGGER_CONTEXT_VARIABLES.md`
- `docs/specifications/parser/05_PSQL_PROCEDURAL_LANGUAGE.md`
