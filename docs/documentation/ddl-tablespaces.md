### DDL: Tablespaces

Parsing for CREATE/ALTER/DROP/DETACH/ATTACH TABLESPACE is implemented in `src/engine/parser_ddl.cpp`. Attributes captured in `ast.ddlTablespace`: action, name, raw attributes (LOCATION/ADD FILE/SET/KEEP FILES/OPTIONS).

Example:
```sql
CREATE TABLESPACE ts1 LOCATION '/data/ts1';
ALTER TABLESPACE ts1 ADD FILE 'ts1_1.seg';
```

