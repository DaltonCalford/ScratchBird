### DDL: Database Links

What it is
- Connection aliases for remote databases.

Why it matters
- Simplifies referencing remote resources via `table@link` patterns.

How to use it
- Create links with connection strings; alter/drop as endpoints change.

CREATE/ALTER/DROP DATABASE LINK parsing is captured in `ast.ddlDbLink` (name, attrs, action). See `src/engine/parser_ddl.cpp`.
See also
- [Foreign data](./ddl-foreign-data.md)
Example:
```sql
CREATE DATABASE LINK remotedb AS 'host=... port=... db=... user=...';
```

