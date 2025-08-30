### DDL: Foreign Data Wrappers

Objects:
- Foreign server: `ast.ddlForeignServer` (action, name, options raw)
- User mapping: `ast.ddlUserMapping` (action, user_name, server_name, options)
- Foreign table: `ast.ddlForeignTable` (action, name, server, columns_raw, options)
- IMPORT FOREIGN SCHEMA: `ast.ddlImportForeignSchema` (remote schema, server, into, options)

Examples:
```sql
CREATE FOREIGN SERVER pgsrv OPTIONS (host '127.0.0.1');
CREATE USER MAPPING FOR alice SERVER pgsrv OPTIONS (user 'alice');
CREATE FOREIGN TABLE ft(a int) SERVER pgsrv OPTIONS (schema 'public');
IMPORT FOREIGN SCHEMA public FROM SERVER pgsrv INTO ext;
```

Code anchors: `src/engine/parser_ddl.cpp`

