# Specification: SELECT Statement

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

- Source anchor: `/home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3524`
- Source anchor: `/home/dcalford/CliWork/ScratchBird/src/parser/parser_v3.cpp:9883`
- Test anchor: `/home/dcalford/CliWork/ScratchBird/tests/unit/test_parser_dml.cpp`

## Synopsis

The SELECT statement retrieves data from one or more tables. Supports comprehensive SQL features including CTEs, window functions, aggregates, set operations, and locking clauses.

## Specification

### EBNF Grammar

```ebnf
select_stmt ::=
    [ with_clause ]
    "SELECT" [ "DISTINCT" | "ALL" ] [ "ON" "(" expression_list ")" ]
    select_list
    [ from_clause ]
    [ where_clause ]
    [ group_by_clause ]
    [ having_clause ]
    [ window_clause ]
    [ order_by_clause ]
    [ limit_clause ]
    [ for_clause ]
    [ set_op select_stmt ]

with_clause ::=
    "WITH" [ "RECURSIVE" ] cte ("," cte )*

cte ::=
    identifier [ "(" column_list ")" ] "AS" [ "MATERIALIZED" | "NOT" "MATERIALIZED" ]
    "(" select_stmt ")"
    [ "SEARCH" ( "BREADTH" | "DEPTH" ) "FIRST" "BY" column_list "SET" identifier ]
    [ "CYCLE" column_list "SET" identifier [ "TO" expression "DEFAULT" expression ]
      [ "USING" identifier ] ]

select_list ::= "*" | select_item ("," select_item )*

select_item ::=
    expression [ [ "AS" ] alias ]
  | schema_path ".*"

from_clause ::= "FROM" table_ref ("," table_ref )* | join_clause

table_ref ::=
    schema_path [ [ "AS" ] alias ]
  | "(" select_stmt ")" [ [ "AS" ] alias [ "(" column_list ")" ] ]
  | function_call [ "AS" alias [ "(" column_list ")" ] ]
  | [ "LATERAL" ] table_ref

join_clause ::=
    table_ref ( join_type "JOIN" table_ref [ join_condition ] )*

join_type ::=
    [ "NATURAL" ] [ ( "INNER" | "LEFT" | "RIGHT" | "FULL" ) [ "OUTER" ] | "CROSS" ]

join_condition ::= "ON" expression | "USING" "(" column_list ")"

where_clause ::= "WHERE" expression

group_by_clause ::= "GROUP" "BY" group_by_item ("," group_by_item )*

group_by_item ::=
    expression
  | "ROLLUP" "(" expression_list ")"
  | "CUBE" "(" expression_list ")"
  | "GROUPING" "SETS" "(" grouping_set_list ")"

having_clause ::= "HAVING" expression

window_clause ::= "WINDOW" window_def ("," window_def )*

window_def ::= identifier "AS" "(" window_specification ")"

window_specification ::=
    [ identifier ]  -- named window reference
    [ "PARTITION" "BY" expression_list ]
    [ "ORDER" "BY" order_by_item ("," order_by_item )* ]
    [ frame_clause ]

frame_clause ::=
    ( "ROWS" | "RANGE" | "GROUPS" )
    frame_start
  | ( "ROWS" | "RANGE" | "GROUPS" )
    "BETWEEN" frame_bound "AND" frame_bound

frame_start ::=
    "UNBOUNDED" "PRECEDING"
  | expression "PRECEDING"
  | "CURRENT" "ROW"

frame_bound ::=
    frame_start
  | "UNBOUNDED" "FOLLOWING"
  | expression "FOLLOWING"

order_by_clause ::= "ORDER" "BY" order_by_item ("," order_by_item )*

order_by_item ::=
    expression [ "ASC" | "DESC" ] [ "NULLS" "FIRST" | "NULLS" "LAST" ]

limit_clause ::=
    "LIMIT" expression [ "OFFSET" expression ]
  | "FETCH" ( "FIRST" | "NEXT" ) expression [ "ROW" | "ROWS" ]
    [ "WITH" "TIES" ]
  | "OFFSET" expression [ "ROW" | "ROWS" ]
    [ "FETCH" ( "FIRST" | "NEXT" ) expression [ "ROW" | "ROWS" ] [ "WITH" "TIES" ] ]

for_clause ::=
    "FOR" ( "UPDATE" | "NO" "KEY" "UPDATE" | "SHARE" | "KEY" "SHARE" )
    [ "OF" table_list ]
    [ "NOWAIT" | "SKIP" "LOCKED" ]
  | "WITH" "LOCK"  -- Firebird

set_op ::= ( "UNION" | "INTERSECT" | "EXCEPT" ) [ "ALL" | "DISTINCT" ]
```

### AST Node Structure

```cpp
// Source: /home/dcalford/CliWork/ScratchBird/include/scratchbird/parser/ast_v3.h:3524
class SelectStmt : public Statement {
public:
    ASTKind kind() const override { return ASTKind::SelectStmt; }
    
    // WITH clause
    WithClause* with = nullptr;

    // SELECT [DISTINCT | ALL]
    bool distinct = false;
    bool all = false;
    std::vector<Expression*> distinct_on;

    // Select list
    std::vector<SelectItem*> items;

    // FROM clause
    TableRefNode* from = nullptr;
    std::vector<JoinNode*> joins;

    // WHERE clause
    Expression* where = nullptr;

    // GROUP BY clause
    std::vector<Expression*> group_by;
    GroupingType grouping_type = GroupingType::STANDARD;
    std::vector<std::vector<Expression*>> grouping_sets;

    // HAVING clause
    Expression* having = nullptr;

    // WINDOW definitions
    std::vector<std::pair<StringPool::StringId, WindowSpec*>> windows;

    // ORDER BY clause
    std::vector<OrderByItem*> order_by;

    // LIMIT/OFFSET
    Expression* limit = nullptr;
    Expression* offset = nullptr;
    FetchMode fetch_mode = FetchMode::NONE;
    bool fetch_with_ties = false;
    Expression* fetch_row_count = nullptr;

    // Set operations (UNION, INTERSECT, EXCEPT)
    SetOpType set_op = SetOpType::NONE;
    bool set_op_all = false;
    SelectStmt* set_op_right = nullptr;

    // FOR UPDATE/SHARE
    SelectLockStrength lock_strength = SelectLockStrength::NONE;
    bool for_update = false;
    bool for_share = false;
    bool nowait = false;
    bool skip_locked = false;
    bool with_lock = false;
    Expression* optimize_for_rows = nullptr;
    Expression* firebird_plan = nullptr;
};

// Supporting structures
struct SelectItem : public ASTNode {
    enum class Type : uint8_t { EXPRESSION, STAR, TABLE_STAR };
    Type item_type = Type::EXPRESSION;
    Expression* expr = nullptr;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;
    SchemaPath table_path;  // For TABLE_STAR
};

struct TableRefNode : public ASTNode {
    enum class Type : uint8_t { TABLE, SUBQUERY, FUNCTION, JOIN };
    Type ref_type = Type::TABLE;
    SchemaPath table_path;
    Statement* subquery = nullptr;
    FunctionCallExpr* function = nullptr;
    StringPool::StringId alias = StringPool::INVALID_ID;
    bool has_alias = false;
    bool lateral = false;
    bool with_ordinality = false;
    TableSampleMethod sample_method = TableSampleMethod::NONE;
    Expression* sample_percent = nullptr;
    std::vector<StringPool::StringId> column_aliases;
};

struct JoinNode : public ASTNode {
    JoinType join_type = JoinType::INNER;
    TableRefNode* left = nullptr;
    TableRefNode* right = nullptr;
    Expression* on_condition = nullptr;
    std::vector<StringPool::StringId> using_columns;
    bool has_using = false;
};

struct WindowSpec : public ASTNode {
    std::vector<Expression*> partition_by;
    std::vector<OrderByItem*> order_by;
    bool has_frame = false;
    FrameType frame_type = FrameType::RANGE;
    FrameBoundType frame_start = FrameBoundType::UNBOUNDED_PRECEDING;
    FrameBoundType frame_end = FrameBoundType::CURRENT_ROW;
    Expression* frame_start_value = nullptr;
    Expression* frame_end_value = nullptr;
    bool has_frame_exclusion = false;
    FrameExclusion frame_exclusion = FrameExclusion::NO_OTHERS;
    StringPool::StringId ref_name = StringPool::INVALID_ID;
    bool has_ref = false;
};
```

## Examples

```sql
-- Basic select
SELECT * FROM users;

-- With aliases and expressions
SELECT 
    u.id AS user_id,
    CONCAT(u.first_name, ' ', u.last_name) AS full_name,
    u.email
FROM users u
WHERE u.status = 'active'
ORDER BY u.created_at DESC;

-- Joins
SELECT 
    o.id AS order_id,
    u.name AS customer_name,
    o.total
FROM orders o
JOIN users u ON o.user_id = u.id
LEFT JOIN order_items oi ON o.id = oi.order_id
GROUP BY o.id, u.name, o.total;

-- CTE
WITH active_users AS (
    SELECT * FROM users WHERE status = 'active'
)
SELECT * FROM active_users WHERE last_login > CURRENT_DATE - INTERVAL '30 days';

-- Window functions
SELECT 
    department,
    employee_name,
    salary,
    AVG(salary) OVER (PARTITION BY department) AS dept_avg,
    RANK() OVER (ORDER BY salary DESC) AS salary_rank
FROM employees;

-- Recursive CTE
WITH RECURSIVE subordinates AS (
    SELECT id, name, manager_id FROM employees WHERE id = 1
    UNION ALL
    SELECT e.id, e.name, e.manager_id 
    FROM employees e
    JOIN subordinates s ON e.manager_id = s.id
)
SELECT * FROM subordinates;

-- Set operations
SELECT name FROM customers
UNION
SELECT name FROM suppliers;

-- FOR UPDATE
SELECT * FROM accounts WHERE id = 123 FOR UPDATE NOWAIT;
```

## Related Specifications

- [stmt_insert.md](./stmt_insert.md) - INSERT statement
- [stmt_update.md](./stmt_update.md) - UPDATE statement
- [stmt_delete.md](./stmt_delete.md) - DELETE statement
- [stmt_merge.md](./stmt_merge.md) - MERGE statement

## Changelog

| Version | Date | Changes | Author |
|---------|------|---------|--------|
| 1.0.0 | 2026-03-08 | Initial specification | ScratchBird Team |
