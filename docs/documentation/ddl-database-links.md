### DDL: Database Links

CREATE/ALTER/DROP DATABASE LINK parsing is captured in `ast.ddlDbLink` (name, attrs, action). See `src/engine/parser_ddl.cpp`.

Example:
```sql
CREATE DATABASE LINK remotedb AS 'host=... port=... db=... user=...';
```

