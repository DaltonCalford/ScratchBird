# Specification: DROP TABLE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2009`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:8500`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The DROP TABLE statement removes one or more tables from the database, with options to cascade or restrict dependent object removal.

## Specification

### EBNF Grammar

```ebnf
drop_table_stmt ::=
    "DROP" "TABLE" [ "IF" "EXISTS" ]
    schema_path ("," schema_path )*
    [ "CASCADE" | "RESTRICT" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2009
class DropTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DropTableStmt; }
    
    bool if_exists = false;
    std::vector<SchemaPath> tables;
    bool cascade = false;
    bool restrict = false;
};
```

## Examples

```sql
-- Drop single table
DROP TABLE temp_data;

-- Drop if exists
DROP TABLE IF EXISTS staging_table;

-- Drop with cascade
DROP TABLE orders CASCADE;

-- Drop multiple tables
DROP TABLE temp1, temp2, temp3;
```

## Related Specifications

- [stmt_create_table.md](./stmt_create_table.md) - Table creation
- [stmt_alter_table.md](./stmt_alter_table.md) - Table modification
- [stmt_truncate_table.md](./stmt_truncate_table.md) - Table truncation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
