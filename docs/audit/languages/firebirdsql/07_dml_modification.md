# FirebirdSQL - DML Modification

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`
- `ScratchBird/docs/audit/16_firebird_parser_statement_reference_actual.md`

## INSERT
Description: Inserts rows; supports RETURNING clause.

Syntax (actual, abbreviated):
```sql
INSERT INTO <table> [(col, ...)]
  { VALUES (...) [, ...] | SELECT ... | DEFAULT VALUES }
  [RETURNING <select_list>]
```
Example:
```sql
INSERT INTO users (id, email) VALUES (1, 'a@example.com') RETURNING id;
```
Status: Partial.
Spec delta: V2 pipeline emits only first VALUES row; INSERT ... SELECT is not
compiled.

## UPDATE
Description: Updates rows with optional RETURNING.

Syntax (actual):
```sql
UPDATE <table> SET col = expr [, ...]
[WHERE <expr>]
[RETURNING <select_list>]
```
Example:
```sql
UPDATE users SET active = 0 WHERE last_login < CURRENT_DATE - 365;
```
Status: Partial (V2 pipeline limitations).

## DELETE
Description: Deletes rows with optional RETURNING.

Syntax (actual):
```sql
DELETE FROM <table> [WHERE <expr>] [RETURNING <select_list>]
```
Example:
```sql
DELETE FROM users WHERE active = 0;
```
Status: Partial (V2 pipeline limitations).

## UPDATE OR INSERT
Description: Firebird upsert syntax.

Syntax (actual):
```sql
UPDATE OR INSERT INTO <table> (col, ...) VALUES (...) [MATCHING (col, ...)]
```
Example:
```sql
UPDATE OR INSERT INTO users (id, email) VALUES (1, 'a@example.com') MATCHING (id);
```
Status: Stubbed.
Spec delta: Compiled as INSERT only; UPDATE path not implemented.

## MERGE
Description: Not implemented in Firebird parser.

Status: Missing.
