## Grammar Overview

- SELECT: WITH [RECURSIVE] CTEs; projections; FROM tables, subqueries, table functions; JOINs (INNER/LEFT/RIGHT/FULL/CROSS/NATURAL); WHERE; GROUP BY; HAVING; ORDER BY [ASC|DESC NULLS {FIRST|LAST}]; ROWS m [TO n]; FETCH {FIRST|NEXT} n ROWS ONLY; WINDOW; FOR UPDATE [OF cols] [NOWAIT|SKIP LOCKED]; PLAN.
- Set operations: UNION [ALL], INTERSECT, EXCEPT with precedence INTERSECT > UNION/EXCEPT.
- DML: INSERT [INTO] t [(cols)] VALUES (...), DEFAULT VALUES, or INSERT ... SELECT; UPDATE t SET a=b [, ...] [FROM ...] [WHERE ...] [FOR UPDATE]; DELETE FROM t [USING ...] [WHERE ...]; MERGE INTO ... USING ... ON ... WHEN ... THEN ...; UPDATE OR INSERT INTO t [(cols)] VALUES (...) MATCHING (...).
- Session: CREATE/ALTER/DROP DATABASE; CONNECT; DISCONNECT; SET NAMES/ROLE/SQL DIALECT/TRANSACTION/OPTIONS; COMMIT; ROLLBACK; SAVEPOINT; RELEASE SAVEPOINT; EXPLAIN/ANALYZE; ANALYZE/VACUUM; SET CONSTRAINTS.
- DDL (subset): CREATE/ALTER/DROP TABLE, INDEX, SCHEMA, SEQUENCE, VIEW, DOMAIN, COLLATION, CHARSET, TABLESPACE; GRANT/REVOKE; comments; renames; roles/users; foreign tables/servers; publications/subscriptions; policies; materialized views; database links; blob filters; mappings; GTT.

### Implementation References
- Parser entry: `include/scratchbird/engine/parser.h` (parse_sql)
- SELECT parser: `src/engine/parser_select.cpp` (parse_select_minimal)
- DML parsers: `src/engine/parser_dml.cpp` (parse_insert_minimal, parse_update_minimal, parse_delete_minimal, parse_merge_minimal, parse_upsert_minimal)
- Session parser: `src/engine/parser_session.cpp` (parse_session_stmt)
- DDL parser hub: `src/engine/parser_ddl.cpp` (parse_ddl_table, parse_ddl_index, ...)
- Execution: `include/scratchbird/engine/executor.h` (execute_select_sql, execute_insert_sql, execute_update_sql, execute_delete_sql)

## Grammar Overview

- **SELECT**: WITH [RECURSIVE] CTEs; projections; FROM tables, subqueries, table functions; JOINs (INNER/LEFT/RIGHT/FULL/CROSS/NATURAL); WHERE; GROUP BY; HAVING; ORDER BY [ASC|DESC NULLS {FIRST|LAST}]; ROWS m [TO n]; FETCH {FIRST|NEXT} n ROWS ONLY; WINDOW; FOR UPDATE [OF cols] [NOWAIT|SKIP LOCKED]; PLAN.
- **Set operations**: UNION [ALL], INTERSECT, EXCEPT with precedence INTERSECT > UNION/EXCEPT.
- **DML**: INSERT [INTO] t [(cols)] VALUES (...), DEFAULT VALUES, or INSERT ... SELECT; UPDATE t SET a=b [, ...] [FROM ...] [WHERE ...] [FOR UPDATE]; DELETE FROM t [USING ...] [WHERE ...]; MERGE INTO ... USING ... ON ... WHEN ... THEN ...; UPDATE OR INSERT INTO t [(cols)] VALUES (...) MATCHING (...).
- **Session**: CREATE/ALTER/DROP DATABASE; CONNECT; DISCONNECT; SET NAMES/ROLE/SQL DIALECT/TRANSACTION/OPTIONS; COMMIT; ROLLBACK; SAVEPOINT; RELEASE SAVEPOINT; EXPLAIN/ANALYZE; ANALYZE/VACUUM; SET CONSTRAINTS.
- **DDL (subset)**: CREATE/ALTER/DROP TABLE, INDEX, SCHEMA, SEQUENCE, VIEW, DOMAIN, COLLATION, CHARSET, TABLESPACE; GRANT/REVOKE; comments; renames; roles/users; foreign tables/servers; publications/subscriptions; policies; materialized views; database links; blob filters; mappings; GTT.

### Implementation References
- Parser entry:  ()
- SELECT parser:  ()
- DML parsers:  (, , , , )
- Session parser:  ()
- DDL parser hub:  (e.g., , , ...)
- Execution:  (, , , )
