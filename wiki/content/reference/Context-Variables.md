# Context Variables

**Status:** Alpha documentation (in progress)
**Last Updated:** 2026-01-18

This page lists ScratchBird context variables (zero-argument context functions
and trigger-only variables) and where they apply.

**Scopes used below:**
- **DML**: SELECT/INSERT/UPDATE/DELETE expressions
- **PSQL**: stored procedures, functions, EXECUTE BLOCK
- **Trigger PSQL**: triggers only

## Core context functions (all dialects)

These are treated as zero-argument context functions in ScratchBird. Dialects may
allow keyword-style usage, but the value is the same.

| Context value | Description | DML | PSQL | Trigger PSQL | Notes |
| --- | --- | --- | --- | --- | --- |
| CURRENT_DATE | Current date | Yes | Yes | Yes | Date only |
| CURRENT_TIME | Current time | Yes | Yes | Yes | Time only |
| CURRENT_TIMESTAMP | Current timestamp | Yes | Yes | Yes | Date + time |
| NOW() | Current timestamp | Yes | Yes | Yes | Alias of CURRENT_TIMESTAMP |
| CURRENT_USER | Current user name | Yes | Yes | Yes | Security context |
| CURRENT_ROLE | Current role name | Yes | Yes | Yes | Active role |
| CURRENT_CONNECTION | Connection ID | Yes | Yes | Yes | Firebird/V2 parity |
| CURRENT_TRANSACTION | Transaction ID | Yes | Yes | Yes | Firebird/V2 parity |

Example (DML):

```
SELECT CURRENT_DATE, CURRENT_TIMESTAMP, CURRENT_USER;
```

Example (PSQL):

```
DECLARE @ts TIMESTAMP;
BEGIN
    SET @ts = NOW();
END;
```

## Firebird shorthand temporal literals (Firebird emulation)

Firebird supports special datetime literals in CAST contexts. ScratchBird
supports these in the Firebird dialect for compatibility.

| Literal | Meaning | DML | PSQL | Trigger PSQL | Notes |
| --- | --- | --- | --- | --- | --- |
| 'NOW' | Current timestamp | Yes | Yes | Yes | Cast to TIMESTAMP/DATE/TIME |
| 'TODAY' | Current date | Yes | Yes | Yes | Date-only literal |
| 'TOMORROW' | Next day | Yes | Yes | Yes | Date-only literal |
| 'YESTERDAY' | Previous day | Yes | Yes | Yes | Date-only literal |

Example:

```
SELECT CAST('NOW' AS TIMESTAMP) FROM RDB$DATABASE;
```

## Firebird context storage (RDB$GET_CONTEXT / RDB$SET_CONTEXT)

Firebird exposes context storage via RDB$GET_CONTEXT and RDB$SET_CONTEXT. These
are available in the Firebird dialect and can be used in DML and PSQL. The
DDL_TRIGGER namespace is trigger-only.

```
RDB$GET_CONTEXT(namespace, variable)
RDB$SET_CONTEXT(namespace, variable, value)
```

Namespaces:
- SYSTEM (read-only): network protocol, client host, engine version, etc.
- USER_SESSION (read/write): session-scoped values
- USER_TRANSACTION (read/write): transaction-scoped values
- DDL_TRIGGER (read-only): DDL trigger context (trigger-only)

Example:

```
SELECT RDB$GET_CONTEXT('SYSTEM', 'CLIENT_ADDRESS') FROM RDB$DATABASE;
```

## PostgreSQL system context functions (PostgreSQL emulation)

These are zero-argument or system information functions available in the
PostgreSQL dialect. They can be used anywhere expressions are allowed.

| Function | Description | DML | PSQL | Trigger PSQL |
| --- | --- | --- | --- | --- |
| CURRENT_CATALOG | Current catalog | Yes | Yes | Yes |
| CURRENT_SCHEMA | Current schema | Yes | Yes | Yes |
| CURRENT_DATABASE() | Current database name | Yes | Yes | Yes |
| CURRENT_QUERY() | Current query text | Yes | Yes | Yes |
| CURRENT_ROLE | Current role name | Yes | Yes | Yes |
| CURRENT_USER | Current user name | Yes | Yes | Yes |
| SESSION_USER | Session user name | Yes | Yes | Yes |
| SYSTEM_USER | System user name | Yes | Yes | Yes |
| USER | Session user alias | Yes | Yes | Yes |
| VERSION() | Server version | Yes | Yes | Yes |

## MySQL system context functions (MySQL emulation)

These functions are available in the MySQL dialect and can be used wherever
expressions are allowed.

| Function | Description | DML | PSQL | Trigger PSQL |
| --- | --- | --- | --- | --- |
| CURRENT_ROLE() | Current role | Yes | Yes | Yes |
| CURRENT_USER() | Current user | Yes | Yes | Yes |
| DATABASE() | Current database | Yes | Yes | Yes |
| SCHEMA() | Alias of DATABASE() | Yes | Yes | Yes |
| SESSION_USER() | Session user | Yes | Yes | Yes |
| SYSTEM_USER() | System user | Yes | Yes | Yes |
| USER() | Session user alias | Yes | Yes | Yes |
| CONNECTION_ID() | Connection identifier | Yes | Yes | Yes |
| VERSION() | Server version | Yes | Yes | Yes |

## Trigger context variables (Trigger PSQL only)

Triggers have access to special variables describing the firing event.

### Row versions

- **NEW**: row after the change (INSERT/UPDATE)
- **OLD**: row before the change (UPDATE/DELETE)

Example:

```
IF NEW.price <> OLD.price THEN
    INSERT INTO price_history(product_id, old_price, new_price)
    VALUES (NEW.id, OLD.price, NEW.price);
END IF;
```

### Trigger metadata

| Variable | Description |
| --- | --- |
| GET TRIGGER_EVENT | INSERT, UPDATE, DELETE, SELECT |
| GET TRIGGER_TIMING | BEFORE, AFTER, INSTEAD OF |
| GET TRIGGER_LEVEL | ROW, STATEMENT |
| GET TRIGGER_NAME | Trigger name |
| GET TRIGGER_SCHEMA | Trigger schema |
| GET TRIGGER_TABLE | Table name |
| GET TRIGGER_POSITION | Position in trigger chain |

### Session and connection info

| Variable | Description |
| --- | --- |
| GET TRIGGER_DATABASE | Database name |
| GET TRIGGER_CATALOG | Catalog name |
| GET TRIGGER_TABLESPACE | Tablespace name |
| GET TRIGGER_USER | User who caused the trigger |
| GET TRIGGER_SESSION_USER | Session user |
| GET TRIGGER_APPLICATION | Client application name |
| GET TRIGGER_CLIENT_IP | Client IP |
| GET TRIGGER_CLIENT_PORT | Client port |
| GET TRIGGER_PID | Client process ID |

### Timing and nesting

| Variable | Description |
| --- | --- |
| GET TRIGGER_TIMESTAMP | Trigger time |
| GET TRIGGER_TRANSACTION_ID | Transaction ID |
| GET TRIGGER_STATEMENT_ID | Statement ID |
| GET TRIGGER_COMMAND_TAG | SQL command |
| GET TRIGGER_DEPTH | Nesting depth |
| GET TRIGGER_PARENT | Parent trigger |
| GET TRIGGER_CHAIN | Trigger chain |

### Change helpers

| Variable | Description |
| --- | --- |
| IS COLUMN CHANGED(name) | True if a column changed |
| GET CHANGED_COLUMNS | Array of changed columns |
| GET COLUMN_OLD_VALUE(name) | OLD column value |
| GET COLUMN_NEW_VALUE(name) | NEW column value |
| GET COLUMN_DELTA(name) | Numeric delta (NEW - OLD) |

Statement-level triggers can also access transition tables (OLD_TABLE, NEW_TABLE)
where available.

## References

- `docs/specifications/FIREBIRD_V2_FEATURE_PARITY_SPECIFICATION.md`
- `docs/specifications/triggers/TRIGGER_CONTEXT_VARIABLES.md`
- `docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`
- `docs/specifications/core/INTERNAL_FUNCTIONS.md`
