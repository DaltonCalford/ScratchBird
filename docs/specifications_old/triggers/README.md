# Trigger Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains trigger system specifications for ScratchBird's database triggers.

## Overview

ScratchBird implements a comprehensive trigger system supporting BEFORE/AFTER triggers for INSERT/UPDATE/DELETE operations, with access to both OLD and NEW record values and special context variables.

## Specifications in this Directory

- **[TRIGGER_CONTEXT_VARIABLES.md](TRIGGER_CONTEXT_VARIABLES.md)** (592 lines) - Trigger context variables specification

## Key Features

### Trigger Types

- **BEFORE INSERT** - Execute before row insertion
- **AFTER INSERT** - Execute after row insertion
- **BEFORE UPDATE** - Execute before row update
- **AFTER UPDATE** - Execute after row update
- **BEFORE DELETE** - Execute before row deletion
- **AFTER DELETE** - Execute after row deletion

### Trigger Granularity

- **FOR EACH ROW** - Row-level triggers (execute once per affected row)
- **FOR EACH STATEMENT** - Statement-level triggers (execute once per statement)

### Context Variables

Triggers have access to special variables:

- **OLD** - Record values before modification (UPDATE/DELETE)
- **NEW** - Record values after modification (INSERT/UPDATE)
- **CURRENT_USER** - Current database user
- **CURRENT_TIMESTAMP** - Current timestamp
- **TG_NAME** - Trigger name
- **TG_WHEN** - BEFORE or AFTER
- **TG_LEVEL** - ROW or STATEMENT
- **TG_OP** - INSERT, UPDATE, or DELETE
- **TG_TABLE_NAME** - Table name
- **TG_TABLE_SCHEMA** - Schema name

See [TRIGGER_CONTEXT_VARIABLES.md](TRIGGER_CONTEXT_VARIABLES.md) for complete list.

## Example Trigger

```sql
CREATE TRIGGER audit_users_trigger
    AFTER INSERT OR UPDATE OR DELETE ON users
    FOR EACH ROW
    EXECUTE FUNCTION audit_log();

CREATE FUNCTION audit_log() RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO audit_trail (
        table_name, operation, old_data, new_data, modified_by, modified_at
    ) VALUES (
        TG_TABLE_NAME, TG_OP,
        row_to_json(OLD), row_to_json(NEW),
        CURRENT_USER, CURRENT_TIMESTAMP
    );
    RETURN NEW;
END;
$$ LANGUAGE plsql;
```

## Related Specifications

- [DDL Triggers](../ddl/DDL_TRIGGERS.md) - CREATE TRIGGER syntax
- [PSQL Language](../parser/05_PSQL_PROCEDURAL_LANGUAGE.md) - Trigger procedure language
- [Transaction System](../transaction/) - Trigger transaction semantics
- [SBLR Bytecode](../sblr/) - Trigger execution

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
