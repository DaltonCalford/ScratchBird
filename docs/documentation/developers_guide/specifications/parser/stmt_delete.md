# Specification: DELETE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3744`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:11437`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_dml.cpp`

## Synopsis

The DELETE statement removes rows from a table. Supports WHERE clause for conditional deletion, USING clause for correlated deletes, and RETURNING.

## Specification

### EBNF Grammar

```ebnf
delete_stmt ::=
    [ with_clause ]
    "DELETE" "FROM"
    schema_path [ [ "AS" ] alias ]
    [ using_clause ]
    [ where_clause ]
    [ "RETURNING" select_list ]

using_clause ::= "USING" table_ref ("," table_ref )*
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3744
class DeleteStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::DeleteStmt; }
    
    // WITH clause
    WithClause* with = nullptr;

    // Target table
    SchemaPath table_path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // USING clause (for DELETE ... USING ... syntax)
    TableRefNode* using_clause = nullptr;
    std::vector<JoinNode*> using_joins;

    // WHERE clause
    Expression* where = nullptr;

    // Cassandra lightweight transaction conditionals
    bool conditional_if_exists = false;
    Expression* conditional_if = nullptr;

    // Per-statement consistency controls
    StringPool::StringId consistency_level = StringPool::INVALID_ID;
    StringPool::StringId serial_consistency_level = StringPool::INVALID_ID;

    // RETURNING clause
    std::vector<SelectItem*> returning;
};
```

## Examples

```sql
-- Basic delete
DELETE FROM temp_logs WHERE created_at < CURRENT_DATE - INTERVAL '7 days';

-- Delete all rows
DELETE FROM staging_table;

-- Delete with using
DELETE FROM orders o
USING accounts a
WHERE o.account_id = a.id AND a.status = 'closed';

-- Delete with returning
DELETE FROM queue WHERE id = (
    SELECT id FROM queue WHERE status = 'pending' ORDER BY priority DESC LIMIT 1
)
RETURNING *;

-- Conditional delete
DELETE FROM tasks WHERE status = 'completed' AND completed_at < NOW() - INTERVAL '30 days';
```

## Related Specifications

- [stmt_select.md](./stmt_select.md) - SELECT statement
- [stmt_update.md](./stmt_update.md) - UPDATE statement
- [stmt_truncate_table.md](./stmt_truncate_table.md) - Fast table truncation

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
