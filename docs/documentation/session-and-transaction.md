### Session and Transaction Statements

Parsing is in `src/engine/parser_session.cpp`. Statements include:
- CREATE/ALTER/DROP DATABASE (captures options like PAGE_SIZE, DEFAULT CHARACTER SET, DIALECT, PAGE CACHE, SWEEP INTERVAL, RESERVE SPACE; FILE/SHADOW lists)
- CONNECT, DISCONNECT
- SET NAMES, SET ROLE, SET SQL DIALECT
- SET TRANSACTION (isolation, access mode, WAIT/NO WAIT, SNAPSHOT TABLE STABILITY, RESERVING ... FOR ...)
- SET TIME ZONE, SET BIND, SET OPTIMIZE, SET SEARCH PATH, SET DEBUG OPTION, SET DECFLOAT ROUND/TRAPS, SESSION RESET, SET LOCK TIMEOUT/WAIT/NO WAIT, SET STATISTICS/PLAN/TIMING
- EXPLAIN [ANALYZE] <statement>
- ANALYZE/VACUUM with FULL/VERBOSE and optional column list
- COMMIT, ROLLBACK, SAVEPOINT, RELEASE SAVEPOINT
- Admin/maintenance surfaces accepted as SetOption stubs (trace, subscription control, background task, sweep, page cache, reserve space, read consistency, backup/restore)

Examples:
```sql
CREATE DATABASE 'db1' PAGE_SIZE 8192 DEFAULT CHARACTER SET UTF8;
SET TRANSACTION READ COMMITTED READ WRITE WAIT;
COMMIT;
EXPLAIN ANALYZE SELECT * FROM t;
VACUUM VERBOSE t (id);
```
