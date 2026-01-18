[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - Programmable SQL

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`

## CREATE FUNCTION / CREATE PROCEDURE
Description: Defines stored routines; PostgreSQL parser emits EXT_CREATE_* with
stored body.

Syntax (actual, abbreviated):
```sql
CREATE [OR REPLACE] FUNCTION <name>(<args>) RETURNS <type> AS $$ ... $$ LANGUAGE <lang>
CREATE [OR REPLACE] PROCEDURE <name>(<args>) AS $$ ... $$ LANGUAGE <lang>
```
Example:
```sql
CREATE FUNCTION add_one(x INT) RETURNS INT AS $$ SELECT x + 1; $$ LANGUAGE SQL;
```
Status: Implemented.
Spec delta: Language handling is stored but not enforced at runtime.

## CREATE TRIGGER
Description: Defines a trigger.

Syntax (actual, abbreviated):
```sql
CREATE TRIGGER <name> <timing> <event> ON <table>
  FOR EACH ROW EXECUTE FUNCTION <func>(...)
```
Example:
```sql
CREATE TRIGGER users_audit BEFORE UPDATE ON users
FOR EACH ROW EXECUTE FUNCTION audit_user();
```
Status: Implemented.
Spec delta: Executor stores trigger metadata; runtime execution expects a
registered C++ trigger procedure (no SQL trigger function interpreter yet).

Trigger quick reference: [Trigger Cheat Sheet](../../user-guides/Trigger-Cheat-Sheet.md)

## Trigger Context Variables (NEW / OLD)
Description: PostgreSQL exposes NEW/OLD row records inside trigger functions.

Syntax (PostgreSQL):
```sql
NEW.<column>
OLD.<column>
```

Example:
```sql
CREATE FUNCTION audit_user() RETURNS trigger AS $$
BEGIN
    INSERT INTO audit_log (user_id, old_status, new_status)
    VALUES (NEW.id, OLD.status, NEW.status);
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;
```

Status: Not supported in ScratchBird SQL execution. NEW/OLD values are available
only to C++ trigger procedures registered with the executor.
