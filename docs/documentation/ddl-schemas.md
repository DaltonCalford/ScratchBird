### DDL: Schemas

What it is
- Logical namespaces for grouping tables and other objects.

Why it matters
- Clear namespacing avoids collisions and aids privilege management and organization.

How to use it
- Create schemas to separate application areas; alter for attributes; drop when decommissioning.

CREATE/ALTER/DROP SCHEMA parsing is implemented in `src/engine/parser_ddl.cpp`. Attributes captured in `ast.ddlSchema` include `name` and raw `attrs`.
See also
- [Tables](./ddl-tables.md) · [Roles, users, grants](./ddl-roles-users-grants.md)
Example:
```sql
CREATE SCHEMA analytics;
ALTER SCHEMA analytics /* options captured raw */;
DROP SCHEMA analytics;
```

