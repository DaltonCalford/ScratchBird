### SQL Language Overview

This guide summarizes the implemented language elements, with authoritative code anchors:

- Parser entry and AST router: `src/engine/parser.cpp`
- SELECT: `src/engine/parser_select.cpp`
- DML (INSERT/UPDATE/DELETE/MERGE/UPSERT): `src/engine/parser_dml.cpp`
- DDL suite: `src/engine/parser_ddl.cpp`
- Session/transaction and utilities: `src/engine/parser_session.cpp`
- PSQL parsing: `src/engine/parser_psql.cpp`
- PSQL runtime: `src/engine/psql_executor.cpp`
- Expressions and precedence: `src/engine/parser_expr.cpp`

Implemented statement families:
- SELECT and set operations (UNION/INTERSECT/EXCEPT)
- DML: INSERT, UPDATE, DELETE, MERGE, UPSERT (UPDATE OR INSERT)
- DDL: tables, indexes, schemas, sequences, views, domains, collations, charsets, tablespaces, roles/users, grants/revokes, comments, renames/moves, foreign data (server/table/user mapping/import), publications/subscriptions, trace/audit policies, cluster, auth provider, RLS policy, materialized view, database link, blob filter, mapping, GTT
- Session/transaction: CREATE/ALTER/DROP DATABASE, CONNECT/DISCONNECT, SET NAMES/ROLE/SQL DIALECT/TRANSACTION/OPTIONS, COMMIT/ROLLBACK/SAVEPOINT/RELEASE, EXPLAIN/EXPLAIN ANALYZE, ANALYZE/VACUUM, SET CONSTRAINTS
- PSQL: EXECUTE BLOCK; procedures, functions, packages, triggers; CALL/EXECUTE PROCEDURE; statements including IF, WHILE, FOR SELECT ... INTO, DECLARE, EXCEPTION/WHEN, SUSPEND, RETURN, EXECUTE STATEMENT

Notes:
- Parser acceptance may exceed runtime evaluation. For example, expression parsing supports IN/BETWEEN/LIKE/SIMILAR/COLLATE/CAST (::) per `src/engine/parser_expr.cpp`, while boolean evaluation in `src/engine/expr.cpp` currently implements AND/OR/NOT, comparisons, and IS [NOT] NULL.

