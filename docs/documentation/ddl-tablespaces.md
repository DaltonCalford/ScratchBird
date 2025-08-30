### DDL: Tablespaces

What it is
- Storage allocation and placement control for database objects.

Why it matters
- Enables capacity planning, performance isolation, and multi-device layouts.

How to use it
- Create tablespaces at desired locations; attach/detach and set options as storage evolves.

Parsing for CREATE/ALTER/DROP/DETACH/ATTACH TABLESPACE is implemented in `src/engine/parser_ddl.cpp`. Attributes captured in `ast.ddlTablespace`: action, name, raw attributes (LOCATION/ADD FILE/SET/KEEP FILES/OPTIONS).
See also
- [Tables](./ddl-tables.md) · [Installation](./installation.md)
Example:
```sql
CREATE TABLESPACE ts1 LOCATION '/data/ts1';
ALTER TABLESPACE ts1 ADD FILE 'ts1_1.seg';
```

