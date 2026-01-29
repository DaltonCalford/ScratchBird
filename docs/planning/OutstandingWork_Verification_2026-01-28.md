# OutstandingWork Verification (2026-01-28)

This pass verifies items in `docs/planning/OutstandingWork.md` against the
current source code (read-only). Statuses below are based on parser/executor
evidence with file/line references.

## Summary

- **Confirmed Implemented**: 4 items
- **Partially Implemented / Needs Follow-up**: 8 items
- **Confirmed Missing**: 18 items

## Confirmed Implemented (can be removed from OutstandingWork)

1) **Temporary tables behavior (MySQL/Firebird)**  
   - MySQL parser sets temp flag: `src/parser/mysql/mysql_parser.cpp:3614`  
   - Firebird parser sets temp_type + ON COMMIT: `src/parser/firebird/firebird_parser.cpp:2302-2336`  
   - Executor enforces temp schemas/metadata scopes: `src/sblr/executor.cpp:6320-6435`

2) **Firebird ALTER INDEX ACTIVE/INACTIVE**  
   - Parser supports ACTIVE/INACTIVE/SET: `src/parser/firebird/firebird_parser.cpp:2563-2654`  
   - Bytecode + executor handles alter index state: `src/sblr/executor.cpp:8152-8197`

3) **Firebird UPDATE OR INSERT (update path)**  
   - Parser maps to ON CONFLICT UPDATE: `src/parser/firebird/firebird_parser.cpp:3554-3619`  
   - Executor handles ON CONFLICT UPDATE flow: `src/sblr/executor.cpp:13500-14180`

4) **PostgreSQL column-level GRANT/REVOKE**  
   - Parser captures column list: `src/parser/postgresql/pg_parser_misc.cpp:540-618`  
   - Executor applies column grants: `src/sblr/executor.cpp:39227-39530`

## Partially Implemented / Needs Follow-up

1) **Table partitioning (Native)**  
   - Native parser records PARTITION BY: `src/parser/parser_v2.cpp:1017-1027`  
   - No CreateTable partition handling in semantic analyzer/executor  
     (only job partition fields appear in semantic analyzer: `src/sblr/semantic_analyzer_v2.cpp:6907-6910`).

2) **Table inheritance (Native)**  
   - INHERITS parsed: `src/parser/parser_v2.cpp:1006-1009`  
   - No table inheritance handling in semantic analyzer/executor.

3) **PostgreSQL DISTINCT ON**  
   - DISTINCT ON parsed but ON list discarded; only distinct flag emitted:  
     `src/parser/postgresql/pg_parser_dml.cpp:21-39`.

4) **PostgreSQL RLS support**  
   - Engine enforces RLS: `src/sblr/executor.cpp:46435-46517`  
   - PostgreSQL parser lacks RLS DDL (no ENABLE/DISABLE RLS).

5) **MySQL ALTER TABLE coverage**  
   - ADD/DROP/MODIFY/CHANGE column supported: `src/parser/mysql/mysql_parser.cpp:3340-3490`  
   - No ADD/DROP CONSTRAINT handling.

6) **MySQL catalog coverage**  
   - mysql.user implemented via user list: `include/scratchbird/catalog/mysql_catalog.h:841-872`  
   - mysql.db / tables_priv / columns_priv are empty stubs:  
     `include/scratchbird/catalog/mysql_catalog.h:877-936`.

7) **PostgreSQL pg_stat_* coverage**  
   - pg_stat_activity populated from ProcArray: `include/scratchbird/catalog/pg_catalog.h:1406-1479`  
   - pg_stat_user_tables exists but counts are zeroed: `include/scratchbird/catalog/pg_catalog.h:1488-1529`.

8) **MySQL performance_schema**  
   - Tables exist; some query functions rely on statement digests:  
     `include/scratchbird/catalog/mysql_catalog.h:1119-1149`  
   - Full parity still requires verified metrics sources.

## Confirmed Missing (still required)

### High Priority - Cross-Dialect Core

1) **Table partitioning (PostgreSQL/MySQL/Firebird DDL)**  
   - PostgreSQL DDL has no PARTITION in CREATE TABLE: `src/parser/postgresql/pg_parser_ddl.cpp` (no PARTITION hits)  
   - MySQL parser rejects partition options: `src/parser/mysql/mysql_parser.cpp:3914-3920`

2) **Partition management (ADD/DROP PARTITION)**  
   - No ALTER TABLE partition actions: `src/parser/parser_v2.cpp:4727-4830`

3) **CREATE TABLE AS SELECT (Native)**  
   - No CTAS handling in CREATE TABLE; only CREATE VIEW uses AS SELECT:  
     `src/parser/parser_v2.cpp:2017`

### MySQL Parser Gaps

4) **Security DCL (CREATE/ALTER/DROP USER/ROLE, GRANT/REVOKE, SET ROLE, SHOW GRANTS)**  
   - Not dispatched in MySQL parser: `src/parser/mysql/mysql_parser.cpp:505-570`

5) **DDL: DROP/ALTER PROCEDURE/FUNCTION/TRIGGER, ALTER VIEW**  
   - No handlers in MySQL parser (DROP only covers DB/table/view/index):  
     `src/parser/mysql/mysql_parser.cpp:3490-3680`

6) **Utility commands**  
   - SHOW VARIABLES/STATUS/PROCESSLIST etc. not in `parseShowStmt`:  
     `src/parser/mysql/mysql_parser.cpp:5384-5465`

7) **Dynamic SQL (PREPARE/EXECUTE/DEALLOCATE)**  
   - No parse handlers in MySQL parser.

8) **DML: Window functions, multi-table DELETE, MATCH...AGAINST**  
   - No OVER clause in MySQL parser; DELETE only single-table:  
     `src/parser/mysql/mysql_parser.cpp:2529-2562`.

### PostgreSQL Parser Gaps

9) **ALTER DEFAULT PRIVILEGES**  
   - No parser support in pg_parser_misc.cpp.

10) **Expression indexes / TABLESPACE**  
   - Explicit errors for both: `src/parser/postgresql/pg_parser_ddl.cpp:1060-1072`.

11) **Range types**  
   - CREATE TYPE RANGE explicitly rejected: `src/parser/postgresql/pg_parser_ddl.cpp:1978-1982`.

### Firebird Parser Gaps

12) **ALTER SEQUENCE (RESTART/INCREMENT)**  
   - Not supported: `src/parser/firebird/firebird_parser.cpp:1968`.

13) **ALTER DATABASE options**  
   - OWNER supported, RENAME explicitly rejected, default charset missing:  
     `src/parser/firebird/firebird_parser.cpp:2189-2237`.

14) **Firebird date/time/context functions**  
   - Tokens exist, no parser handling: `src/parser/firebird/firebird_lexer.cpp:286-474`

### Native V2 Parser Gaps

15) **Operators (unary +, ^, JSON ?/ ?|/ ?&, array ops, bitwise ops)**  
   - Unary parser only handles `-`: `src/parser/parser_v2.cpp:7289-7296`  
   - No caret/bitwise/array operators in expression parsing.

16) **ALTER TABLE subcommands**  
   - No SET STATISTICS/STORAGE, ENABLE/DISABLE TRIGGER, INHERIT/NO INHERIT, VALIDATE:  
     `src/parser/parser_v2.cpp:4727-4830`.

17) **View enhancements**  
   - No INSTEAD OF trigger timing in native parser:  
     `src/parser/parser_v2.cpp:3383-3435`.

18) **Session variables (CURRENT_USER/ROLE/CONNECTION/TRANSACTION expressions)**  
   - No expression handling in parser_v2 (only SET ROLE/user mapping uses CURRENT_USER).

### Tablespace in Emulated Dialects

19) **CREATE/ALTER/DROP TABLESPACE in emulated parsers**  
   - PostgreSQL parser rejects TABLESPACE: `src/parser/postgresql/pg_parser_ddl.cpp:1463-1467`  
   - MySQL parser has no CREATE/ALTER/DROP TABLESPACE handlers.

### System Catalog Gaps (Emulated)

20) **MySQL information_schema.ROUTINES/TRIGGERS parity**  
   - No evidence of full routine/trigger coverage in information_schema handler.

21) **PostgreSQL pg_stat_* breadth**  
   - Only pg_stat_activity and pg_stat_user_tables present; other pg_stat_* tables absent.

## Recommended Updates to OutstandingWork.md

- Remove: Temporary tables (MySQL/Firebird), Firebird ALTER INDEX ACTIVE/INACTIVE,
  Firebird UPDATE OR INSERT update path, PostgreSQL column-level GRANT/REVOKE.
- Replace: Table partitioning wording (native parser parses, but semantic/executor missing).
- Mark: RLS enforcement as implemented in engine, but PostgreSQL parser lacks RLS DDL.

