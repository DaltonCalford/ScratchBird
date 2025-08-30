### DDL: Sequences

CREATE/ALTER/DROP/RECREATE SEQUENCE parsing in `src/engine/parser_ddl.cpp` captures:
- name, action, start_with, increment_by, cycle flag

Example:
```sql
CREATE SEQUENCE seq1;
ALTER SEQUENCE seq1 INCREMENT BY 10;
```

