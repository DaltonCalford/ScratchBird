# Specification: INSERT Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3653`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:10953`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_dml.cpp`

## Synopsis

The INSERT statement adds new rows to a table. Supports VALUES, SELECT source, DEFAULT VALUES, ON CONFLICT (UPSERT), and RETURNING clauses.

## Specification

### EBNF Grammar

```ebnf
insert_stmt ::=
    [ with_clause ]
    "INSERT" [ "INTO" ]
    schema_path [ [ "AS" ] alias ]
    [ "(" column_list ")" ]
    ( "VALUES" "(" expression_list ")" ("," "(" expression_list ")" )*
    | select_stmt
    | "DEFAULT" "VALUES"
    )
    [ on_conflict_clause ]
    [ "RETURNING" select_list ]

on_conflict_clause ::=
    "ON" "CONFLICT" [ conflict_target ] "DO" conflict_action

conflict_target ::=
    "(" column_list ")" [ "WHERE" expression ]
  | "ON" "CONSTRAINT" constraint_name

conflict_action ::=
    "NOTHING"
  | "UPDATE" "SET" assignment_list [ "WHERE" expression ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3653
class InsertStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::InsertStmt; }
    
    // WITH clause
    WithClause* with = nullptr;

    // Target table
    SchemaPath table_path;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;

    // Column list (optional)
    std::vector<StringPool::StringId> columns;

    // Source
    enum class Source { VALUES, SELECT, DEFAULT };
    Source source = Source::VALUES;

    // For VALUES source
    std::vector<std::vector<Expression*>> values_rows;

    // For SELECT source
    SelectStmt* select_source = nullptr;

    // ON CONFLICT (UPSERT)
    OnConflictClause* on_conflict = nullptr;

    // Firebird OVERRIDING SYSTEM/USER VALUE
    enum class OverridingMode : uint8_t { NONE = 0, SYSTEM = 1, USER = 2 };
    OverridingMode overriding = OverridingMode::NONE;

    // Cassandra lightweight transaction conditionals
    bool conditional_if_exists = false;
    bool conditional_if_not_exists = false;
    Expression* conditional_if = nullptr;

    // Per-statement consistency controls
    StringPool::StringId consistency_level = StringPool::INVALID_ID;
    StringPool::StringId serial_consistency_level = StringPool::INVALID_ID;

    // RETURNING clause
    std::vector<SelectItem*> returning;
};

struct OnConflictClause {
    std::vector<StringPool::StringId> columns;
    StringPool::StringId constraint_name = StringPool::INVALID_ID;
    Expression* where_target = nullptr;
    ConflictAction action = ConflictAction::NOTHING;
    std::vector<std::pair<StringPool::StringId, Expression*>> set_items;
    Expression* where_action = nullptr;
};

enum class ConflictAction : uint8_t {
    NOTHING, UPDATE
};
```

## Examples

```sql
-- Basic insert
INSERT INTO users (name, email) VALUES ('John Doe', 'john@example.com');

-- Multiple rows
INSERT INTO users (name, email) VALUES
    ('Alice', 'alice@example.com'),
    ('Bob', 'bob@example.com'),
    ('Carol', 'carol@example.com');

-- Insert from select
INSERT INTO archived_orders (order_id, total, archived_at)
SELECT id, total, CURRENT_TIMESTAMP FROM orders WHERE status = 'completed';

-- Upsert (ON CONFLICT)
INSERT INTO users (id, name, email) VALUES (1, 'John', 'john@example.com')
ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name, email = EXCLUDED.email;

-- Upsert with where clause
INSERT INTO inventory (product_id, quantity) VALUES (100, 50)
ON CONFLICT (product_id) DO UPDATE SET quantity = inventory.quantity + EXCLUDED.quantity
WHERE inventory.quantity < 100;

-- Insert with returning
INSERT INTO users (name, email) VALUES ('Jane', 'jane@example.com')
RETURNING id, created_at;

-- Default values
INSERT INTO users DEFAULT VALUES;
```

## Related Specifications

- [stmt_select.md](./stmt_select.md) - SELECT statement
- [stmt_update.md](./stmt_update.md) - UPDATE statement
- [stmt_merge.md](./stmt_merge.md) - MERGE statement

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
