# Specification: DROP SCHEMA Statement

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

The DROP SCHEMA statement removes one or more schemas from the database, optionally cascading to contained objects.

## Specification

### EBNF Grammar

```ebnf
drop_schema_stmt ::=
    "DROP" "SCHEMA" [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" | "RESTRICT" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1509
class DropSchemaStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropSchemaStmt; }
    
    bool if_exists = false;
    std::vector<SchemaPath> schemas;
    bool cascade = false;
    bool restrict = false;
};
```

## Examples

```sql
-- Drop schema
DROP SCHEMA staging;

-- Drop if exists
DROP SCHEMA IF EXISTS temp_schema;

-- Drop with cascade (removes all contained objects)
DROP SCHEMA old_app CASCADE;

-- Drop multiple schemas
DROP SCHEMA schema1, schema2;
```

## Related Specifications

- [stmt_create_schema.md](./stmt_create_schema.md) - Schema creation
- [stmt_alter_schema.md](./stmt_alter_schema.md) - Schema modification

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
