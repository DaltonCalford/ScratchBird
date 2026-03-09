# Specification: UPDATE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3706`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:11293`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_dml.cpp`

## Synopsis

The UPDATE statement modifies existing rows in a table. Supports SET assignments, FROM clause for correlated updates, WHERE clause, and RETURNING.

## Specification

### EBNF Grammar

```ebnf
update_stmt ::=
    [ with_clause ]
    "UPDATE" [ "OR" "ALTER" ]
    schema_path [ [ "AS" ] alias ]
    "SET" assignment ("," assignment )*
    [ from_clause ]
    [ where_clause ]
    [ "RETURNING" select_list ]

assignment ::=
    identifier "=" expression
  | "(" identifier_list ")" "=" "(" subquery ")"
  | "(" identifier_list ")" "=" "(" expression_list ")"
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3706
class UpdateStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::UpdateStmt; }
    
    // WITH clause
    WithClause* with = nullptr;

    // Target table
    SchemaPath table_path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // SET clause: column = expression pairs
    std::vector<std::pair<StringPool::StringId, Expression*>> set_items;

    // FROM clause (for UPDATE ... FROM ... syntax)
    TableRefNode* from = nullptr;
    std::vector<JoinNode*> joins;

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
-- Basic update
UPDATE users SET status = 'inactive' WHERE last_login < '2023-01-01';

-- Multiple columns
UPDATE orders SET 
    status = 'shipped', 
    shipped_at = CURRENT_TIMESTAMP,
    tracking_number = 'ABC123'
WHERE id = 1001;

-- Update with join
UPDATE orders o
SET total = oi.sum_amount
FROM (SELECT order_id, SUM(amount) as sum_amount FROM order_items GROUP BY order_id) oi
WHERE o.id = oi.order_id;

-- Update with returning
UPDATE accounts SET balance = balance - 100 WHERE id = 123
RETURNING balance AS new_balance;

-- Conditional update (Cassandra style)
UPDATE users SET status = 'premium' WHERE id = 1 IF status = 'basic';
```

## Related Specifications

- [stmt_select.md](./stmt_select.md) - SELECT statement
- [stmt_insert.md](./stmt_insert.md) - INSERT statement
- [stmt_delete.md](./stmt_delete.md) - DELETE statement

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
