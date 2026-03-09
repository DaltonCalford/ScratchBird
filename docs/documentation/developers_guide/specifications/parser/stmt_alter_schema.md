# Specification: ALTER SCHEMA Statement

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Synopsis

The ALTER SCHEMA statement modifies schema properties including name, owner, and search path.

## Specification

### EBNF Grammar

```ebnf
alter_schema_stmt ::=
    "ALTER" "SCHEMA" schema_path
    ( "RENAME" "TO" new_name
    | "OWNER" "TO" new_owner
    | "SET" "PATH" new_path
    )
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1798
enum class AlterSchemaAction : uint8_t {
    RENAME = 1, SET_OWNER = 2, SET_PATH = 3
};

class AlterSchemaStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::AlterSchemaStmt; }
    
    AlterSchemaAction action = AlterSchemaAction::RENAME;
    SchemaPath schema_path;
    StringPool::StringId new_name = StringPool::INVALID_ID;
    StringPool::StringId owner = StringPool::INVALID_ID;
    SchemaPath new_path;
};
```

## Examples

```sql
-- Rename schema
ALTER SCHEMA old_schema RENAME TO new_schema;

-- Change owner
ALTER SCHEMA app_schema OWNER TO app_owner;

-- Set search path
ALTER SCHEMA public SET PATH 'public, app_schema';
```

## Related Specifications

- [stmt_create_schema.md](./stmt_create_schema.md) - Schema creation
- [stmt_drop_schema.md](./stmt_drop_schema.md) - Schema removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
