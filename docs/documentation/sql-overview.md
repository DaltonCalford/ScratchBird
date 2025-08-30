### SQL Language Overview

This guide summarizes the implemented language elements. The code is the authority; see the referenced files for exact behavior.

- Router and AST: `src/engine/parser.cpp` (routes to specific parsers)
- SELECT: `src/engine/parser_select.cpp`
- DML: `src/engine/parser_dml.cpp` (INSERT/UPDATE/DELETE/MERGE/UPSERT)
- DDL: `src/engine/parser_ddl.cpp` (tables, indexes, schemas, sequences, views, domains, collations, charsets, tablespaces, roles/users, grants, comments, rename/move, foreign data, publications/subscriptions, trace/audit, cluster, auth provider, policies, materialized views, db links, blob filter, mapping, GTT)
- Session/transaction: `src/engine/parser_session.cpp` (CREATE/ALTER/DROP DATABASE; CONNECT/DISCONNECT; SET *; COMMIT/ROLLBACK; EXPLAIN/ANALYZE; ANALYZE/VACUUM; SET CONSTRAINTS)
- PSQL parsing/runtime: `src/engine/parser_psql.cpp`, `src/engine/psql_executor.cpp`
- Expressions/precedence: `src/engine/parser_expr.cpp` (parsing), `src/engine/expr.cpp` (boolean evaluation)

Notes on semantics vs parsing:
- Expression parser supports operators including IN/BETWEEN/LIKE/SIMILAR/COLLATE/`::` cast; boolean evaluation currently covers AND/OR/NOT, comparisons, and IS [NOT] NULL.
- Some admin statements are accepted and routed as SetOption stubs; execution may not be implemented in this tree.

