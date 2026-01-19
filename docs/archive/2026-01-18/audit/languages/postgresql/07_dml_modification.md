# PostgreSQL - DML Modification

Spec refs:
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/17_postgresql_parser_statement_reference_actual.md`

## INSERT
Description: INSERT with VALUES, SELECT, ON CONFLICT, RETURNING.

Syntax (actual, abbreviated):
```sql
INSERT INTO <table> [(col, ...)]
  { VALUES (...) [, ...] | SELECT ... | DEFAULT VALUES }
  [ON CONFLICT <target> DO NOTHING | DO UPDATE SET ...]
  [RETURNING <select_list>]
```
Example:
```sql
INSERT INTO users (id, email) VALUES (1, 'a@example.com') RETURNING id;
```
Status: Stubbed (bytecode mismatch; RETURNING/ON CONFLICT unsupported by executor).

## UPDATE
Description: UPDATE with FROM and RETURNING.

Syntax (actual):
```sql
UPDATE <table> SET col = expr [, ...]
[FROM <table_ref> ...]
[WHERE <expr>]
[RETURNING <select_list>]
```
Example:
```sql
UPDATE users SET active = FALSE WHERE last_login < NOW() - INTERVAL '1 year';
```
Status: Stubbed (payload mismatch).

## DELETE
Description: DELETE with USING and RETURNING.

Syntax (actual):
```sql
DELETE FROM <table> [USING <table_ref> ...]
[WHERE <expr>] [RETURNING <select_list>]
```
Example:
```sql
DELETE FROM users WHERE active = FALSE;
```
Status: Stubbed (payload mismatch).

## MERGE
Description: PostgreSQL MERGE is parsed but executor has no EXT_MERGE handlers
for PostgreSQL bytecode layout.

Status: Stubbed.
