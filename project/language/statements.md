## Statements

Each statement section includes a syntax sketch, semantics, side-effects, and code anchors to parsing and execution.

### SELECT
- Syntax: standard SELECT with WITH/RECURSIVE, FROM tables/subqueries/table functions, JOINs, WHERE, GROUP BY, HAVING, ORDER BY, ROWS/FETCH, WINDOW, FOR UPDATE, PLAN. Set ops: UNION [ALL], INTERSECT, EXCEPT.
- Semantics: Parsed into `SelectQuery` with structured join tree, windows, order, and filters; evaluated by the executor with basic predicate and projection handling, plus optional plan output and FOR UPDATE semantics.
- Side-effects: none (read-only), except FOR UPDATE locks when applicable.
- Implementation References:
  - Parse: `src/engine/parser_select.cpp` (parse_select_minimal)
  - Execute: `src/engine/executor.cpp` (execute_select_sql, explain_analyze_select_sql)

### INSERT
- Syntax: `INSERT INTO table [(col,...)] VALUES (...)[, ...] | DEFAULT VALUES | INSERT ... SELECT ...` with optional `RETURNING`.
- Semantics: Minimal support for VALUES and DEFAULT VALUES; performs NOT NULL and CHECK validation and maintains unique/PK BTREE indexes; `RETURNING` is parsed and stored.
- Side-effects: Writes to heap storage; fires BEFORE/AFTER triggers; may update indexes; enforces immediate constraints; records deferred FK keys when deferrable.
- Implementation References:
  - Parse: `src/engine/parser_dml.cpp` (parse_insert_minimal)
  - Execute: `src/engine/executor.cpp` (execute_insert_sql)

### UPDATE
- Syntax: `UPDATE table SET a = expr [, ...] [FROM ...] [WHERE ...] [RETURNING ...] [FOR UPDATE]`.
- Semantics: Applies assignments to matched rows; supports WHERE normalization, FROM capture, and RETURNING list.
- Side-effects: Fires triggers; enforces constraints (NOT NULL, CHECK, FK with RESTRICT/CASCADE/SET NULL/SET DEFAULT as applicable), updates heap and relevant indexes.
- Implementation References:
  - Parse: `src/engine/parser_dml.cpp` (parse_update_minimal)
  - Execute: `src/engine/executor.cpp` (execute_update_sql)

### DELETE
- Syntax: `DELETE FROM table [USING ...] [WHERE ...] [RETURNING ...]`.
- Semantics: Deletes matched rows, optional USING captured, RETURNING parsed.
- Side-effects: Triggers and constraint checks; heap/index maintenance.
- Implementation References:
  - Parse: `src/engine/parser_dml.cpp` (parse_delete_minimal)
  - Execute: `src/engine/executor.cpp` (execute_delete_sql)

### MERGE
- Syntax: `MERGE INTO t USING src ON condition WHEN [NOT] MATCHED [AND guard] THEN action` (action: UPDATE SET ..., INSERT (cols) VALUES (...), DELETE, or DO NOTHING)
- Semantics: Captures structured actions; executor currently focuses on DML primitives.
- Side-effects: As per resulting UPDATE/INSERT/DELETE actions.
- Implementation References:
  - Parse: `src/engine/parser_dml.cpp` (parse_merge_minimal)

### UPSERT (UPDATE OR INSERT)
- Syntax: `UPDATE OR INSERT INTO t [(cols)] VALUES (...) MATCHING (cols)`.
- Semantics: Captures target, columns, values, and matching columns; diagnostics if mismatch.
- Implementation References:
  - Parse: `src/engine/parser_dml.cpp` (parse_upsert_minimal)

### Session Statements
- Syntax: CREATE/ALTER/DROP DATABASE; CONNECT; DISCONNECT; SET NAMES/ROLE/SQL DIALECT/TRANSACTION/OPTIONS; COMMIT; ROLLBACK; SAVEPOINT; RELEASE SAVEPOINT; EXPLAIN; ANALYZE/VACUUM.
- Semantics: Parsed into `Ast::SessionStmtAst` and related DDL nodes for EXPLAIN/ANALYZE.
- Side-effects: Transaction control, session options, optimizer hints.
- Implementation References:
  - Parse: `src/engine/parser_session.cpp` (parse_session_stmt)
  - Execute: `src/engine/executor.cpp` (executor_commit, executor_rollback, set_constraints_*)

### DDL (Selected)
- CREATE/ALTER/DROP TABLE/INDEX/SCHEMA/SEQUENCE/VIEW/DOMAIN/COLLATION/CHARSET/TABLESPACE; GRANT/REVOKE; COMMENT; RENAME; ROLE/USER; FOREIGN SERVER/TABLE/USER MAPPING; PUBLICATION/SUBSCRIPTION; POLICY; MATERIALIZED VIEW; DB LINK; BLOB FILTER; MAPPING; GTT.
- Semantics: Parsed to specialized `Ast` variants; executor routes to catalog manager and storage/index routines.
- Implementation References:
  - Parse: `src/engine/parser_ddl.cpp` (parse_ddl_table, parse_ddl_index, ...)
  - Execute: `src/engine/executor.cpp` (DDL handling sections for tables, indexes, etc.)

