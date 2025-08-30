### DDL: Views

What it is
- Named queries that provide virtual tables, optionally with check options.

Why it matters
- Encapsulates complex queries, supports security and abstraction boundaries.

How to use it
- Define views to present stable interfaces over underlying schemas; use WITH CHECK OPTION to protect write-through semantics.

CREATE/ALTER/RECREATE VIEW parsing in `src/engine/parser_ddl.cpp` captures:
- name, optional column list, body `AS (select ...)`, WITH CHECK OPTION (LOCAL/CASCADED)
See also
- [SELECT](./sql-select.md) · [Tables](./ddl-tables.md)
Example:
```sql
CREATE VIEW v_emps AS (SELECT id, name FROM employees) WITH CHECK OPTION;
```

