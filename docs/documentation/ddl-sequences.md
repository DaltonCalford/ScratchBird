### DDL: Sequences

What it is
- Objects that generate numeric sequences (generators) for identity/autonumber patterns.

Why it matters
- Provides unique values without contention; supports identity columns and custom key strategies.

How to use it
- Create sequences and adjust INCREMENT/START/CYCLE as needed; pair with identity columns or use in application logic.

CREATE/ALTER/DROP/RECREATE SEQUENCE parsing in `src/engine/parser_ddl.cpp` captures:
- name, action, start_with, increment_by, cycle flag
See also
- [Tables](./ddl-tables.md) · [PSQL runtime](./psql-runtime.md)
Example:
```sql
CREATE SEQUENCE seq1;
ALTER SEQUENCE seq1 INCREMENT BY 10;
```

