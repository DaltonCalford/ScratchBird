# FirebirdSQL - DML SELECT

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`

## SELECT
Description: Reads data using Firebird syntax (FIRST/SKIP, ROWS).

Syntax (actual, abbreviated):
```sql
SELECT [FIRST n] [SKIP n] <select_list>
FROM <table_ref> [<join_clause> ...]
[WHERE <expr>]
[GROUP BY <expr> [, ...]]
[HAVING <expr>]
[UNION ...]
[ORDER BY <expr> [ASC|DESC] [NULLS FIRST|LAST]]
[ROWS <n> [TO <m>]]
[FOR UPDATE [WITH LOCK]]
```
Example:
```sql
SELECT FIRST 10 SKIP 20 * FROM users ORDER BY id;
```
Status: Partial.
Spec delta:
- V2 pipeline has limitations on multi-table joins and select list encoding.
- Schema-qualified identifiers are rejected in Firebird parser.
