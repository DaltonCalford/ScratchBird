# Specification: CREATE SCHEMA Statement

## Metadata

| Field | Value |
|-------|-------|
| **Subsystem** | parser |
| **Spec Version** | 1.0.0 |
| **Status** | 🟢 Approved |
| **Last Verified** | 2026-03-08 |
| **Implementation Version** | ScratchBird v3.0 |
| **Authors** | ScratchBird Team |

## Coverage and Evidence Status

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1495`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:4500`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The CREATE SCHEMA statement creates a new schema (namespace) in the database. Schemas provide logical grouping of database objects and enable multi-tenancy within a single database.

## Specification

### EBNF Grammar

```ebnf
create_schema_stmt ::=
    "CREATE" "SCHEMA" [ "IF" "NOT" "EXISTS" ]
    schema_path
    [ "AUTHORIZATION" identifier ]
    [ schema_element ... ]

schema_element ::=
    create_table_stmt
  | create_view_stmt
  | create_index_stmt
  | grant_stmt
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1495
class CreateSchemaStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateSchemaStmt; }
    
    bool if_not_exists = false;
    SchemaPath schema_path;
    bool has_owner = false;
    StringPool::StringId owner = StringPool::INVALID_ID;
};
```

## Examples

```sql
-- Create schema
CREATE SCHEMA sales;

-- Create schema with owner
CREATE SCHEMA marketing AUTHORIZATION marketing_admin;

-- Create schema if not exists
CREATE SCHEMA IF NOT EXISTS analytics;
```

## Related Specifications

- [stmt_alter_schema.md](./stmt_alter_schema.md) - Schema modification
- [stmt_drop_schema.md](./stmt_drop_schema.md) - Schema removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
