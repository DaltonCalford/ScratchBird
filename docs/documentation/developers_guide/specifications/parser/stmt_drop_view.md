# Specification: DROP VIEW Statement

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

The DROP VIEW statement removes one or more views from the database.

## Specification

### EBNF Grammar

```ebnf
drop_view_stmt ::=
    "DROP" "VIEW" [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" | "RESTRICT" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2037
class DropViewStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropViewStmt; }
    
    bool if_exists = false;
    bool materialized = false;
    std::vector<SchemaPath> views;
    bool cascade = false;
};
```

## Examples

```sql
-- Drop view
DROP VIEW user_summary;

-- Drop if exists
DROP VIEW IF EXISTS temp_view;

-- Drop with cascade
DROP VIEW dependent_view CASCADE;
```

## Related Specifications

- [stmt_create_view.md](./stmt_create_view.md) - View creation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
