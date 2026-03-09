# Specification: TRUNCATE TABLE Statement

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

The TRUNCATE TABLE statement quickly removes all rows from a table, with options to cascade to dependent tables and control identity column reset behavior.

## Specification

### EBNF Grammar

```ebnf
truncate_table_stmt ::=
    "TRUNCATE" [ "TABLE" ]
    schema_path ("," schema_path )*
    [ "RESTART" "IDENTITY" | "CONTINUE" "IDENTITY" ]
    [ "CASCADE" | "RESTRICT" ]
    [ "SYNC" | "ASYNC" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2235
class TruncateTableStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::TruncateTableStmt; }
    
    std::vector<SchemaPath> tables;
    bool restart_identity = false;
    bool continue_identity = false;
    bool cascade = false;
    bool sync_mode = false;  // SYNC blocks, ASYNC (default) is non-blocking
};
```

## Examples

```sql
-- Truncate single table
TRUNCATE TABLE temp_data;

-- Truncate multiple tables
TRUNCATE TABLE temp1, temp2, temp3;

-- Truncate with identity restart
TRUNCATE TABLE logs RESTART IDENTITY;

-- Truncate with cascade
TRUNCATE TABLE parent_table CASCADE;

-- Synchronous truncate (blocks until complete)
TRUNCATE TABLE large_table SYNC;
```

## Related Specifications

- [stmt_delete.md](./stmt_delete.md) - Row deletion
- [stmt_drop_table.md](./stmt_drop_table.md) - Table removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
