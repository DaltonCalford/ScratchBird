# Outstanding Work Closure Tracker (2026-01-28)

This tracker reflects the verified code-truth audit in
`docs/planning/OutstandingWork_Verification_2026-01-28.md`. Goal: close
remaining Alpha gaps within ~24 hours.

## Status Key
- **Done** = verified in code
- **Partial** = parsed or stubbed, executor/catalog missing
- **Open** = not implemented

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

## Partial / Needs Follow-up

- **Partial**: Native table partitioning (parser-only)  
  Evidence: `src/parser/parser_v2.cpp:1017-1027`  
  Missing: semantic/executor + catalog routing for partitioned tables
- **Partial**: Native table inheritance (parser-only)  
  Evidence: `src/parser/parser_v2.cpp:1006-1009`  
  Missing: semantic/executor + catalog inheritance model
- **Partial**: PostgreSQL DISTINCT ON (ON list discarded)  
  Evidence: `src/parser/postgresql/pg_parser_dml.cpp:21-39`
- **Partial**: PostgreSQL RLS (engine supports, parser lacks DDL)  
  Evidence: `src/sblr/executor.cpp:46435-46517`
- **Partial**: MySQL ALTER TABLE coverage (no constraints)  
  Evidence: `src/parser/mysql/mysql_parser.cpp:3340-3490`
- **Partial**: MySQL DCL (CREATE/DROP USER/ROLE, GRANT/REVOKE, SET ROLE, SHOW GRANTS)  
  Evidence: `src/parser/mysql/mysql_parser.cpp`  
  Missing: ALTER ROLE; SHOW GRANTS FOR user is parsed but executor still lists object grants
- **Partial**: MySQL DDL (DROP PROCEDURE/FUNCTION/TRIGGER, ALTER VIEW, ALTER PROCEDURE/FUNCTION for SQL SECURITY/DETERMINISTIC/COMMENT)  
  Evidence: `src/parser/mysql/mysql_parser.cpp:3552-3738`, `src/parser/mysql/mysql_parser.cpp:4903-5045`, `src/sblr/executor.cpp:37558-37660`  
  Missing: ALTER TRIGGER remains unsupported; SQL DATA characteristics are parsed/ignored
- **Partial**: MySQL utility KILL/FLUSH/LOCK/UNLOCK  
  Evidence: `src/parser/mysql/mysql_parser.cpp:5060-5205`, `src/parser/mysql/mysql_parser.cpp:6710-6795`, `src/sblr/executor.cpp:37662-37760`  
  Missing: LOCK/FLUSH are no-op; KILL maps to backend termination only
- **Partial**: MySQL DML gaps (window functions, multi-table DELETE, MATCH...AGAINST)  
  Evidence: `src/parser/mysql/mysql_parser.cpp:1720-2260`, `src/parser/mysql/mysql_parser.cpp:2576-2745`, `src/parser/mysql/mysql_parser.cpp:2883-3255`  
  Missing: window execution path is limited (parser emits OVER/WINDOW); MATCH...AGAINST uses fulltext opcodes but still needs full executor coverage validation
- **Partial**: MySQL SHOW STATUS/VARIABLES/PROCESSLIST/WARNINGS/ERRORS  
  Evidence: `src/parser/mysql/mysql_parser.cpp`  
  Missing: status/variable filters and processlist/warnings/errors outputs are mapped to generic SHOW output
- **Partial**: MySQL catalog coverage (mysql.db/tables_priv/columns_priv empty)  
  Evidence: `include/scratchbird/catalog/mysql_catalog.h:877-936`
- **Partial**: PostgreSQL pg_stat_user_tables (counts zeroed)  
  Evidence: `include/scratchbird/catalog/pg_catalog.h:1488-1529`
- **Partial**: MySQL performance_schema (metrics stubs)  
  Evidence: `include/scratchbird/catalog/mysql_catalog.h:1119-1149`

## Open (Remaining Gaps to Close)

### Cross-Dialect Core
- **Open**: Partition management (ADD/DROP PARTITION)
- **Open**: CREATE TABLE AS SELECT (Native)
- **Open**: Partitioned table DDL for PostgreSQL/MySQL/Firebird

### MySQL Parser
- **Open**: None (see Partial items)

### PostgreSQL Parser
- **Open**: ALTER DEFAULT PRIVILEGES
- **Open**: Expression indexes + TABLESPACE clauses
- **Open**: Range types (CREATE TYPE RANGE)

### Firebird Parser
- **Open**: ALTER SEQUENCE (RESTART/INCREMENT)
- **Open**: ALTER DATABASE options (default charset, rename not supported)
- **Open**: Firebird date/context functions (LOCALTIME, LOCALTIMESTAMP, TODAY, YESTERDAY, TOMORROW, DATEADD, RDB$GET_CONTEXT)

### Native V2 Parser
- **Open**: Operators (unary +, power ^, JSON ?/?|/?&, array ops, bitwise ops)
- **Open**: ALTER TABLE subcommands (SET STATISTICS/STORAGE, ENABLE/DISABLE TRIGGER, INHERIT/NO INHERIT, VALIDATE)
- **Open**: INSTEAD OF triggers for views
- **Open**: Session variables (CURRENT_USER/ROLE/CONNECTION/TRANSACTION expressions)

### Emulated Tablespace DDL
- **Open**: CREATE/ALTER/DROP TABLESPACE in emulated parsers (PG/MySQL)

### System Catalog Parity
- **Open**: MySQL information_schema ROUTINES/TRIGGERS parity
- **Open**: PostgreSQL pg_stat_* breadth beyond pg_stat_activity and pg_stat_user_tables

## 24-Hour Close Order (Suggested)

1) MySQL DCL + SHOW/EXPLAIN + PREPARE (largest user-facing parity gaps)  
2) Native operators + ALTER TABLE subcommands (core SQL coverage)  
3) Firebird ALTER SEQUENCE + date/context functions  
4) PostgreSQL ALTER DEFAULT PRIVILEGES + expression index errors  
5) CTAS + partition management primitives  
6) Catalog parity expansions (mysql.db/pg_stat breadth)
