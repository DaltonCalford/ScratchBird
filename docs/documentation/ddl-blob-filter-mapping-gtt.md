### DDL: Blob Filter, Mapping, Global Temporary Tables (GTT)

Parsing for these objects exists in `src/engine/parser_ddl.cpp`:
- Blob filter: `ast.ddlBlobFilter`
- Mapping: `ast.ddlMapping`
- GTT: `ast.ddlGtt`

Examples:
```sql
DECLARE FILTER ... -- captured per parser rules
CREATE GLOBAL TEMPORARY TABLE gtt (id int);
```

