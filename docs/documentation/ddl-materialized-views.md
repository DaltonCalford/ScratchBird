### DDL: Materialized Views

Parsing for CREATE/ALTER/DROP/REFRESH MATERIALIZED VIEW captured in `ast.ddlMaterializedView` (name, options/body, action).

Example:
```sql
CREATE MATERIALIZED VIEW mv AS SELECT * FROM t;
REFRESH MATERIALIZED VIEW mv;
```

