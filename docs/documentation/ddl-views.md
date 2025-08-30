### DDL: Views

CREATE/ALTER/RECREATE VIEW parsing in `src/engine/parser_ddl.cpp` captures:
- name, optional column list, body `AS (select ...)`, WITH CHECK OPTION (LOCAL/CASCADED)

Example:
```sql
CREATE VIEW v_emps AS (SELECT id, name FROM employees) WITH CHECK OPTION;
```

