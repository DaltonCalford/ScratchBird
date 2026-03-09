# Specification: DROP INDEX Statement

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

The DROP INDEX statement removes one or more indexes from the database.

## Specification

### EBNF Grammar

```ebnf
drop_index_stmt ::=
    "DROP" "INDEX" [ "CONCURRENTLY" ] [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2023
class DropIndexStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropIndexStmt; }
    
    bool if_exists = false;
    bool concurrent = false;
    std::vector<SchemaPath> indexes;
    bool cascade = false;
};
```

## Examples

```sql
-- Drop single index
DROP INDEX idx_users_email;

-- Drop if exists
DROP INDEX IF EXISTS idx_temp;

-- Drop concurrently
DROP INDEX CONCURRENTLY idx_large_table;

-- Drop multiple indexes
DROP INDEX idx1, idx2, idx3;
```

## Related Specifications

- [stmt_create_index.md](./stmt_create_index.md) - Index creation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
