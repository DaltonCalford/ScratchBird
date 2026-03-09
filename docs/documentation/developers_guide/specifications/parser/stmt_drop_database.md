# Specification: DROP DATABASE Statement

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

The DROP DATABASE statement removes a database instance and all its contents.

## Specification

### EBNF Grammar

```ebnf
drop_database_stmt ::=
    "DROP" "DATABASE" [ "IF" "EXISTS" ]
    schema_path
    [ "FORCE" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:1779
class DropDatabaseStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropDatabaseStmt; }
    
    bool if_exists = false;
    SchemaPath database_path;
    bool force = false;
};
```

## Examples

```sql
-- Drop database
DROP DATABASE test_db;

-- Drop if exists
DROP DATABASE IF EXISTS old_test;

-- Force drop (immediate, no waiting for connections)
DROP DATABASE staging_db FORCE;
```

## Related Specifications

- [stmt_create_database.md](./stmt_create_database.md) - Database creation
- [stmt_alter_database.md](./stmt_alter_database.md) - Database modification

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
