### DDL: Schemas

CREATE/ALTER/DROP SCHEMA parsing is implemented in `src/engine/parser_ddl.cpp`. Attributes captured in `ast.ddlSchema` include `name` and raw `attrs`.

Example:
```sql
CREATE SCHEMA analytics;
ALTER SCHEMA analytics /* options captured raw */;
DROP SCHEMA analytics;
```

