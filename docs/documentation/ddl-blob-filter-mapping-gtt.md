### DDL: Blob Filter, Mapping, Global Temporary Tables (GTT)

What it is
- Specialized objects: blob transformation filters, value mappings, and temporary tables.

Why it matters
- Enables advanced data flows and transient working sets.

How to use it
- Define filters/mappings per extension needs; create GTTs for session-scoped data.

Parsing for these objects exists in `src/engine/parser_ddl.cpp`:
- Blob filter: `ast.ddlBlobFilter`
- Mapping: `ast.ddlMapping`
- GTT: `ast.ddlGtt`
See also
- [Tables](./ddl-tables.md)
Examples:
```sql
DECLARE FILTER ... -- captured per parser rules
CREATE GLOBAL TEMPORARY TABLE gtt (id int);
```

