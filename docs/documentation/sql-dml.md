### DML: INSERT, UPDATE, DELETE, MERGE, UPSERT

What it is
- The data modification surface and how each statement is parsed with normalized expressions and RETURNING support.

Why it matters
- DML changes data and triggers constraints/triggers/index maintenance. Understanding each form avoids surprises (e.g., MERGE actions and guards).

How to use it
- Use the examples to structure statements; add RETURNING when you need immediate results; rely on WHERE normalization for consistent predicate handling.

Parsing is implemented in `src/engine/parser_dml.cpp`. RETURNING lists are captured, WHERE expressions are normalized via the expression parser.

- INSERT: `INSERT INTO t [(cols)] VALUES (...)[, ...] | DEFAULT VALUES | INSERT ... SELECT ...`
- UPDATE: `UPDATE t SET a = expr [, ...] [FROM ...] [WHERE ...] [RETURNING ...] [FOR UPDATE]`
- DELETE: `DELETE FROM t [USING ...] [WHERE ...] [RETURNING ...]`
- MERGE: `MERGE INTO t USING src ON cond WHEN [NOT] MATCHED [AND guard] THEN action` where action is UPDATE/DELETE/INSERT or DO NOTHING; structured actions captured
- UPSERT: `UPDATE OR INSERT INTO t [(cols)] VALUES (...) MATCHING (cols)` with diagnostics if matching cols not in insert list

Examples:
```sql
INSERT INTO t(a,b) VALUES (1,'x'), (2,'y') RETURNING a;
UPDATE t SET a = a + 1 WHERE id = 10 RETURNING *;
DELETE FROM t USING u WHERE t.u_id = u.id AND u.flag = 1;
MERGE INTO t USING (SELECT 1 AS id) s ON s.id = t.id
  WHEN MATCHED THEN UPDATE SET a = 2
  WHEN NOT MATCHED THEN INSERT (id, a) VALUES (s.id, 2);
UPDATE OR INSERT INTO t (id, a) VALUES (10, 2) MATCHING (id);
```

Code anchors: `src/engine/parser_dml.cpp`

See also
- [Operators](./sql-operators.md) · [SELECT](./sql-select.md)

