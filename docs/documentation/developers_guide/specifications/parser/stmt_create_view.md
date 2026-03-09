# Specification: CREATE VIEW Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:873`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:4145`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_ddl.cpp`

## Synopsis

The CREATE VIEW statement creates a virtual table based on the result set of a SELECT query. Supports regular views, materialized views, and views with check options for data integrity.

## Scope

### In Scope

- Regular views
- Temporary views
- Materialized views (with data/without data)
- Views with explicit column names
- WITH CHECK OPTION (LOCAL/CASCADED)
- OR REPLACE / OR ALTER support

### Out of Scope

- Updatable view rules (semantic analysis phase)
- Materialized view refresh strategies

## Specification

### EBNF Grammar

```ebnf
create_view_stmt ::=
    "CREATE" [ "OR" ( "REPLACE" | "ALTER" ) ]
    [ "TEMPORARY" | "TEMP" ]
    [ "MATERIALIZED" ]
    "VIEW" [ "IF" "NOT" "EXISTS" ]
    schema_path
    [ "(" identifier_list ")" ]
    "AS" select_stmt
    [ "WITH" ( "LOCAL" | "CASCADED" ) "CHECK" "OPTION" ]
    [ materialized_options ]

materialized_options ::=
    [ "WITH" "DATA" | "WITH" "NO" "DATA" ]
    [ "TABLESPACE" schema_path ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:873
class CreateViewStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::CreateViewStmt; }
    
    bool or_replace = false;
    bool temporary = false;
    bool materialized = false;
    bool if_not_exists = false;

    SchemaPath view_path;
    std::vector<StringPool::StringId> column_names;  // Optional explicit column names

    Statement* query = nullptr;  // SELECT statement

    // View options
    bool with_check_option = false;
    bool check_option_local = false;  // LOCAL vs CASCADED

    // For materialized views
    bool with_data = true;
    SchemaPath tablespace;
    bool has_tablespace = false;
};
```

### Semantic Binding Rules

1. **Query Validation**: Underlying SELECT must be valid
2. **Column Name Resolution**: Explicit column names must match SELECT column count
3. **Circular Reference Prevention**: Views cannot reference themselves (directly or indirectly)
4. **Materialized View Restrictions**: Query cannot contain non-deterministic functions

## Examples

```sql
-- Simple view
CREATE VIEW active_users AS
SELECT * FROM users WHERE status = 'active';

-- View with explicit column names
CREATE VIEW user_summary (user_id, full_name, order_count) AS
SELECT u.id, u.name, COUNT(o.id)
FROM users u LEFT JOIN orders o ON u.id = o.user_id
GROUP BY u.id, u.name;

-- Materialized view
CREATE MATERIALIZED VIEW daily_sales AS
SELECT DATE(created_at) as sale_date, SUM(total) as total_sales
FROM orders
GROUP BY DATE(created_at)
WITH DATA;

-- View with check option
CREATE VIEW current_employees AS
SELECT * FROM employees WHERE end_date IS NULL
WITH CHECK OPTION;

-- Replace existing view
CREATE OR REPLACE VIEW product_catalog AS
SELECT p.*, c.name as category_name
FROM products p JOIN categories c ON p.category_id = c.id;
```

## Related Specifications

- [stmt_create_table.md](./stmt_create_table.md) - Base table creation
- [stmt_select.md](./stmt_select.md) - Query specification
- [stmt_drop_view.md](./stmt_drop_view.md) - View removal

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
