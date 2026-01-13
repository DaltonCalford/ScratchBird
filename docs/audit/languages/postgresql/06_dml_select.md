# PostgreSQL - DML SELECT

Spec refs:
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/17_postgresql_parser_statement_reference_actual.md`

## SELECT
Description: PostgreSQL SELECT with DISTINCT ON, WINDOW, set operations, and
FOR UPDATE/SHARE.

Syntax (actual, abbreviated):
```sql
SELECT [ALL | DISTINCT | DISTINCT ON (...)] <select_list>
FROM <table_ref> [<join_clause> ...]
[WHERE <expr>]
[GROUP BY <expr> [ROLLUP|CUBE|GROUPING SETS]]
[HAVING <expr>]
[WINDOW <window_spec>]
[ORDER BY <expr> ...]
[LIMIT <expr>] [OFFSET <expr>] [FETCH ...]
[FOR UPDATE | FOR SHARE]
```
Example:
```sql
SELECT DISTINCT ON (user_id) * FROM events ORDER BY user_id, ts DESC;
```
Status: Stubbed.
Spec delta: Parser emits SELECT bytecode layout that executor does not accept.

## WITH (CTE)
Description: Common table expressions (WITH / WITH RECURSIVE).

Syntax (actual):
```sql
WITH [RECURSIVE] <name> AS (<select>) SELECT ...
```
Example:
```sql
WITH recent AS (SELECT * FROM events ORDER BY ts DESC LIMIT 10)
SELECT * FROM recent;
```
Status: Stubbed (payload mismatch for EXT_WITH_CLAUSE).
