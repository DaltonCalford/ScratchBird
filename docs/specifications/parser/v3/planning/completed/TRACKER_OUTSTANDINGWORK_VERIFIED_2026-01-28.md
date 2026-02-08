# Outstanding Work Closure Tracker (2026-01-28)

**Non-Authoritative Reference:** This document is not authoritative for V3 implementation.
Only files listed in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` are authoritative.


This tracker reflects the verified code-truth audit in
`docs/planning/OutstandingWork_Verification_2026-01-28.md`. Goal: close
remaining Alpha gaps within ~24 hours.

## Status Key

- **Done** = verified in code
- **Partial** = parsed or stubbed, executor/catalog missing
- **Open** = not implemented

## Progress Updates

- **2026-02-02**: All items in this tracker are now closed; no remaining gaps.
- **2026-01-29**: Parallel test isolation hardened by switching remaining test fixtures to
  per-test `/tmp` paths via `tests/test_helpers.h` helpers; full build + full `ctest` run
  completed cleanly (2495/2495 tests).
- **2026-01-29**: pg_stat_user_tables now surfaces live scan/DML counters via
  TableStatsManager snapshots in `include/scratchbird/catalog/pg_catalog.h`.
- **2026-01-29**: MySQL DCL gap closed: ALTER ROLE wired end-to-end and SHOW GRANTS
  now resolves principals (user/role/current_user/PUBLIC) in `src/sblr/executor.cpp`.
- **2026-01-29**: MySQL SHOW STATUS/VARIABLES/PROCESSLIST/WARNINGS/ERRORS now handled
  directly in the MySQL adapter with filtering and limits in
  `src/protocol/adapters/mysql_adapter.cpp`.
- **2026-01-29**: MySQL utility commands wired: LOCK/UNLOCK TABLES now acquire/release
  real table locks and FLUSH actions invalidate caches/stats in
  `src/sblr/executor.cpp`.
- **2026-01-29**: MySQL multi-table DELETE now accepts `USING` in place of `FROM`
  for target-qualified deletes in `src/parser/mysql/mysql_parser.cpp`.
- **2026-01-29**: Native CREATE TABLE partition metadata now flows through semantic
  analysis, bytecode, and executor storage params via EXT_TABLE_PARTITIONING in
  `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`,
  `src/sblr/executor.cpp`.
- **2026-01-29**: Native CREATE TABLE inheritance metadata now flows through semantic
  analysis, bytecode, and executor storage params via EXT_TABLE_INHERITS in
  `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`,
  `src/sblr/executor.cpp`.
- **2026-01-29**: PostgreSQL DISTINCT ON now emits distinct expression payloads and
  executor applies DISTINCT ON with optional ORDER BY sequencing via
  `src/parser/postgresql/pg_parser_dml.cpp` and `src/sblr/executor.cpp`.
- **2026-01-29**: PostgreSQL RLS DDL now supports CREATE/DROP POLICY and ALTER TABLE
  {ENABLE|DISABLE|FORCE|NO FORCE} ROW LEVEL SECURITY in
  `src/parser/postgresql/pg_parser_ddl.cpp`.
- **2026-01-29**: MySQL ALTER TABLE now supports ADD/DROP constraints (PRIMARY/UNIQUE/
  FOREIGN/CHECK) with executor payloads in `src/parser/mysql/mysql_parser.cpp`.
- **2026-01-29**: MySQL DDL coverage verified for DROP PROCEDURE/FUNCTION/TRIGGER and
  ALTER VIEW/ALTER PROCEDURE/ALTER FUNCTION characteristics in
  `src/parser/mysql/mysql_parser.cpp`.
- **2026-01-29**: mysql.db/tables_priv/columns_priv now surface permissions and
  grantee mappings via `include/scratchbird/catalog/mysql_catalog.h`.
- **2026-01-29**: MySQL performance_schema event tables already emit live query,
  transaction, and wait metadata via `include/scratchbird/catalog/mysql_catalog.h`.
- **2026-01-29**: ALTER TABLE ATTACH/DETACH PARTITION now emits bytecode and updates
  partition metadata via `src/parser/parser_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`,
  and `src/sblr/executor.cpp`.
- **2026-01-29**: Native CREATE TABLE AS SELECT now emits CTAS payloads and executes
  SELECT results into the created table via `src/parser/parser_v2.cpp`,
  `src/sblr/bytecode_generator_v2.cpp`, and `src/sblr/executor.cpp`.
- **2026-01-29**: Emulated CREATE TABLE PARTITION BY now emits partition metadata
  (PostgreSQL/MySQL) and Firebird raises explicit unsupported errors via
  `src/parser/postgresql/pg_parser_ddl.cpp`, `src/parser/mysql/mysql_parser.cpp`,
  and `src/parser/firebird/firebird_parser.cpp`.
- **2026-01-29**: PostgreSQL CREATE INDEX now honors TABLESPACE and ALTER DEFAULT
  PRIVILEGES emits an explicit unsupported error via
  `src/parser/postgresql/pg_parser_ddl.cpp`.
- **2026-01-29**: Firebird ALTER SEQUENCE now emits native ALTER SEQUENCE bytecode,
  ALTER DATABASE DEFAULT CHARACTER SET/COLLATION maps to SET_OPTIONS, and
  Firebird context/date functions are parsed via
  `src/parser/firebird/firebird_parser.cpp`.
- **2026-01-29**: Native v2 parser operator coverage now includes unary +, power (^),
  bitwise ops, JSON ?/?|/?&, and array operators with bytecode wiring via
  `src/parser/parser_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, and
  `src/sblr/bytecode_generator_v2.cpp`.
- **2026-02-02**: Emulated tablespace DDL now covers CREATE/ALTER/DROP TABLESPACE for
  MySQL and PostgreSQL, emitting CREATE/ALTER/DROP/ATTACH/DETACH opcodes via
  `src/parser/mysql/mysql_parser.cpp` and `src/parser/postgresql/pg_parser_ddl.cpp`.
- **2026-02-02**: MySQL information_schema TRIGGERS now populated from catalog trigger
  metadata in `src/protocol/adapters/mysql_adapter.cpp`, and pg_stat breadth
  expanded to pg_stat_{all,sys}_tables in `include/scratchbird/catalog/pg_catalog.h`.

## Closed (remove from OutstandingWork)

- **Done**: Temporary tables behavior (MySQL/Firebird)  
  Evidence: `src/parser/mysql/mysql_parser.cpp:3614`,
  `src/parser/firebird/firebird_parser.cpp:2302-2336`,
  `src/sblr/executor.cpp:6320-6435`
- **Done**: Firebird ALTER INDEX ACTIVE/INACTIVE  
  Evidence: `src/parser/firebird/firebird_parser.cpp:2563-2654`,
  `src/sblr/executor.cpp:8152-8197`
- **Done**: Firebird UPDATE OR INSERT (UPDATE path)  
  Evidence: `src/parser/firebird/firebird_parser.cpp:3554-3619`,
  `src/sblr/executor.cpp:13500-14180`
- **Done**: PostgreSQL column-level GRANT/REVOKE  
  Evidence: `src/parser/postgresql/pg_parser_misc.cpp:540-618`,
  `src/sblr/executor.cpp:39227-39530`
- **Done**: MySQL Dynamic SQL (PREPARE/EXECUTE/DEALLOCATE)  
  Evidence: `src/parser/mysql/mysql_parser.cpp:6165-6262`, `src/sblr/executor.cpp:37381-37553`
- **Done**: MySQL DCL (ALTER ROLE + SHOW GRANTS FOR principal)  
  Evidence: `src/parser/mysql/mysql_parser.cpp`, `src/sblr/executor.cpp`
- **Done**: PostgreSQL pg_stat_user_tables (counts wired to table stats)  
  Evidence: `include/scratchbird/catalog/pg_catalog.h`
- **Done**: MySQL SHOW STATUS/VARIABLES/PROCESSLIST/WARNINGS/ERRORS  
  Evidence: `src/protocol/adapters/mysql_adapter.cpp`
- **Done**: MySQL utility KILL/FLUSH/LOCK/UNLOCK  
  Evidence: `src/sblr/executor.cpp`
- **Done**: Native CREATE TABLE AS SELECT (CTAS)  
  Evidence: `src/parser/parser_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`,
  `src/sblr/executor.cpp`
- **Done**: Emulated PARTITION BY (PG/MySQL) with Firebird explicit unsupported  
  Evidence: `src/parser/postgresql/pg_parser_ddl.cpp`, `src/parser/mysql/mysql_parser.cpp`,
  `src/parser/firebird/firebird_parser.cpp`
- **Done**: PostgreSQL DISTINCT ON  
  Evidence: `src/parser/postgresql/pg_parser_dml.cpp`, `src/sblr/executor.cpp`
- **Done**: PostgreSQL RLS DDL  
  Evidence: `src/parser/postgresql/pg_parser_ddl.cpp`, `src/sblr/executor.cpp`
- **Done**: MySQL ALTER TABLE coverage (constraints wired)  
  Evidence: `src/parser/mysql/mysql_parser.cpp`
- **Done**: MySQL DDL (DROP PROCEDURE/FUNCTION/TRIGGER, ALTER VIEW, ALTER PROCEDURE/FUNCTION)  
  Evidence: `src/parser/mysql/mysql_parser.cpp`, `src/sblr/executor.cpp`  
  Note: ALTER TRIGGER remains unsupported by MySQL syntax; SQL DATA characteristics are parsed and ignored
- **Done**: MySQL DML gaps (window functions, multi-table DELETE, MATCH...AGAINST)  
  Evidence: `src/parser/mysql/mysql_parser.cpp`, `tests/unit/test_mysql_query_compiler.cpp`
- **Done**: MySQL catalog coverage (mysql.db/tables_priv/columns_priv populated)  
  Evidence: `include/scratchbird/catalog/mysql_catalog.h`
- **Done**: MySQL performance_schema (metrics wired)  
  Evidence: `include/scratchbird/catalog/mysql_catalog.h`
- **Done**: FireBirdSQL ALTER SEQUENCE (RESTART/INCREMENT)  
  Evidence: `src/parser/firebird/firebird_parser.cpp`, `src/sblr/bytecode_generator_v2.cpp`
- **Done**: Firebird date/context functions (LOCALTIME, LOCALTIMESTAMP, TODAY, YESTERDAY, TOMORROW, DATEADD, RDB$GET_CONTEXT)  
  Evidence: `src/parser/firebird/firebird_parser.cpp`
- **Done**: Native Operators (unary +, power ^, JSON ?/?|/?&, array ops, bitwise ops)  
  Evidence: `src/parser/parser_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`
- **Done**: Native ALTER TABLE subcommands (SET STATISTICS/STORAGE, ENABLE/DISABLE TRIGGER, INHERIT/NO INHERIT, VALIDATE)  
  Evidence: `src/parser/parser_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/executor.cpp`
- **Done**: Native INSTEAD OF triggers for views  
  Evidence: `src/parser/parser_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/executor.cpp`
- **Done**: Native Session variables (CURRENT_USER/ROLE/CONNECTION/TRANSACTION expressions)  
  Evidence: `src/parser/parser_v2.cpp`, `src/sblr/semantic_analyzer_v2.cpp`, `src/sblr/bytecode_generator_v2.cpp`, `src/sblr/executor.cpp`
- **Done**: Emulated tablespace DDL (CREATE/ALTER/DROP TABLESPACE for MySQL/PostgreSQL)  
  Evidence: `src/parser/mysql/mysql_parser.cpp`, `src/parser/postgresql/pg_parser_ddl.cpp`
- **Done**: MySQL information_schema TRIGGERS parity  
  Evidence: `src/protocol/adapters/mysql_adapter.cpp`
- **Done**: PostgreSQL pg_stat breadth (pg_stat_all_tables/pg_stat_sys_tables)  
  Evidence: `include/scratchbird/catalog/pg_catalog.h`
- **Done**: Native table inheritance (parent columns merged; scans accept extra columns)  
  Evidence: `src/sblr/executor.cpp`



## Partial / Needs Follow-up

- None.

## Open (Remaining Gaps to Close)

### Cross-Dialect Core

- **Done**: Partition management (ATTACH/DETACH execution semantics + validation)  
  Evidence: `src/sblr/executor.cpp`

### MySQL Parser

- **Open**: None

### PostgreSQL Parser

- **Done**: ALTER DEFAULT PRIVILEGES parsing + runtime persistence/apply  
  Evidence: `src/parser/postgresql/pg_parser_ddl.cpp`, `src/sblr/executor.cpp`,  
  `src/core/catalog_manager.cpp`
- **Done**: Expression index parsing + SBLR expression payload  
  Evidence: `src/parser/postgresql/pg_parser_ddl.cpp`, `src/sblr/executor.cpp`, `src/core/catalog_manager.cpp`
- **Done**: Range types mapped to domain RANGE kind  
  Evidence: `src/parser/postgresql/pg_parser_ddl.cpp`, `src/sblr/executor.cpp`

### Firebird Parser

- **Done**: ALTER DATABASE RENAME parsed and emitted  
  Evidence: `src/parser/firebird/firebird_parser.cpp`, `src/sblr/bytecode_generator_v2.cpp`

### Native V2 Parser

- **Open**: None

### Emulated Tablespace DDL

- **Done**: CREATE/ALTER/DROP TABLESPACE in emulated parsers (PG/MySQL)

### System Catalog Parity

- **Done**: MySQL information_schema ROUTINES/TRIGGERS parity
- **Done**: PostgreSQL pg_stat_* breadth beyond pg_stat_activity and pg_stat_user_tables

## 24-Hour Close Order (Suggested)

1) MySQL DCL + SHOW/EXPLAIN + PREPARE (largest user-facing parity gaps)  
2) Native operators + ALTER TABLE subcommands (core SQL coverage)  
3) Firebird ALTER SEQUENCE + date/context functions  
4) PostgreSQL ALTER DEFAULT PRIVILEGES + expression index errors  
5) CTAS primitives  
6) Catalog parity expansions (mysql.db/pg_stat breadth)
