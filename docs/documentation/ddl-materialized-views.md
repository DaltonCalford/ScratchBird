### DDL: Materialized Views

What it is
- Stored result sets refreshed on demand.

Why it matters
- Speeds up repeated queries; enables precomputed aggregates or denormalized views.

How to use it
- Create materialized views from SELECT; refresh as needed; alter/drop for lifecycle.

Parsing for CREATE/ALTER/DROP/REFRESH MATERIALIZED VIEW captured in `ast.ddlMaterializedView` (name, options/body, action).
See also
- [SELECT](./sql-select.md) · [Indexes](./ddl-indexes.md)
Example:
```sql
CREATE MATERIALIZED VIEW mv AS SELECT * FROM t;
REFRESH MATERIALIZED VIEW mv;
```

