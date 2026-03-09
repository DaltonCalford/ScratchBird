# Specification: MERGE Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2509`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:19014`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_dml.cpp`

## Synopsis

The MERGE statement (SQL:2003 standard) performs INSERT, UPDATE, or DELETE operations based on a join condition between target and source tables. Supports WHEN MATCHED, WHEN NOT MATCHED, and WHEN NOT MATCHED BY SOURCE clauses.

## Specification

### EBNF Grammar

```ebnf
merge_stmt ::=
    "MERGE" "INTO" schema_path [ [ "AS" ] target_alias ]
    "USING" ( schema_path | "(" select_stmt ")" ) [ [ "AS" ] source_alias ]
    "ON" expression
    [ merge_when_clause ... ]

merge_when_clause ::=
    "WHEN" "MATCHED" [ "AND" expression ]
    "THEN" ( "UPDATE" "SET" assignment_list | "DELETE" )
  | "WHEN" "NOT" "MATCHED" [ "AND" expression ]
    "THEN" "INSERT" [ "(" column_list ")" ] "VALUES" "(" expression_list ")"
  | "WHEN" "NOT" "MATCHED" "BY" "SOURCE" [ "AND" expression ]
    "THEN" ( "UPDATE" "SET" assignment_list | "DELETE" )
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:2509
class MergeStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::MergeStmt; }
    
    SchemaPath target_table;
    StringPool::StringId target_alias = StringPool::INVALID_ID;

    // Source: can be table or subquery
    SchemaPath source_table;
    StringPool::StringId source_alias = StringPool::INVALID_ID;
    Statement* source_query = nullptr;

    Expression* on_condition = nullptr;

    // WHEN MATCHED THEN UPDATE
    struct WhenMatched {
        Expression* and_condition = nullptr;
        std::vector<std::pair<StringPool::StringId, Expression*>> assignments;
        bool is_delete = false;
    };
    std::vector<WhenMatched> when_matched;

    // WHEN NOT MATCHED THEN INSERT
    struct WhenNotMatched {
        Expression* and_condition = nullptr;
        std::vector<StringPool::StringId> columns;
        std::vector<Expression*> values;
    };
    std::vector<WhenNotMatched> when_not_matched;

    // WHEN NOT MATCHED BY SOURCE (SQL Server extension)
    struct WhenNotMatchedBySource {
        Expression* and_condition = nullptr;
        std::vector<std::pair<StringPool::StringId, Expression*>> assignments;
        bool is_delete = false;
    };
    std::vector<WhenNotMatchedBySource> when_not_matched_by_source;
};
```

## Examples

```sql
-- Basic upsert (merge)
MERGE INTO inventory AS target
USING staging_inventory AS source
ON target.product_id = source.product_id
WHEN MATCHED THEN
    UPDATE SET quantity = source.quantity, updated_at = CURRENT_TIMESTAMP
WHEN NOT MATCHED THEN
    INSERT (product_id, quantity) VALUES (source.product_id, source.quantity);

-- With conditional logic
MERGE INTO orders AS target
USING new_orders AS source
ON target.order_id = source.order_id
WHEN MATCHED AND source.status = 'cancelled' THEN
    DELETE
WHEN MATCHED THEN
    UPDATE SET total = source.total, status = source.status
WHEN NOT MATCHED THEN
    INSERT (order_id, total, status) VALUES (source.order_id, source.total, source.status);

-- Using subquery as source
MERGE INTO employee_stats AS target
USING (SELECT department_id, COUNT(*) as emp_count FROM employees GROUP BY department_id) AS source
ON target.dept_id = source.department_id
WHEN MATCHED THEN
    UPDATE SET employee_count = source.emp_count
WHEN NOT MATCHED THEN
    INSERT (dept_id, employee_count) VALUES (source.department_id, source.emp_count);

-- With NOT MATCHED BY SOURCE (delete unmatched target rows)
MERGE INTO current_products AS target
USING supplier_catalog AS source
ON target.sku = source.sku
WHEN NOT MATCHED BY SOURCE THEN
    DELETE;
```

## Related Specifications

- [stmt_insert.md](./stmt_insert.md) - INSERT statement
- [stmt_update.md](./stmt_update.md) - UPDATE statement
- [stmt_delete.md](./stmt_delete.md) - DELETE statement

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
