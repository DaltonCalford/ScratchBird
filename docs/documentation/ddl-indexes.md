### DDL: Indexes

What it is
- How to define and manage indexes: unique, expressions, methods, partials, rebuild/statistics.

Why it matters
- Indexes enable performance and constraints. Choosing the right keys, methods, and orderings impacts query plans.

How to use it
- Use unique indexes for integrity and acceleration; consider expression/partial indexes for selective access patterns.

Parsing for CREATE/ALTER/DROP/REINDEX/VALIDATE and method options is in `src/engine/parser_ddl.cpp`.

Captured attributes in AST (`ast.ddlIndex`):
- name, on_table, unique flag, action (CREATE|ALTER|DROP|REINDEX|VALIDATE)
- expression or column list, ASC/DESC per column, COLLATE per column
- method (e.g., USING gin/bitmap/rtree), partial index WHERE condition
- REBUILD and SET STATISTICS support

Examples:
```sql
CREATE UNIQUE INDEX ix_t_name ON t(name COLLATE unicode);
CREATE INDEX ix_t_expr ON t ((upper(name))) WHERE name IS NOT NULL;
ALTER INDEX ix_t_name REBUILD;
```

Code anchors: `src/engine/parser_ddl.cpp` (parse_ddl_index)

See also
- [Tables](./ddl-tables.md) · [SELECT](./sql-select.md)

