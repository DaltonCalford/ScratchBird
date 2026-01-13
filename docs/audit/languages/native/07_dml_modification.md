# Native (V2) - DML Modification

Spec refs:
- `ScratchBird/docs/specifications/dml/02_INSERT.md`
- `ScratchBird/docs/specifications/dml/03_UPDATE.md`
- `ScratchBird/docs/specifications/dml/04_DELETE.md`
- `ScratchBird/docs/specifications/dml/05_MERGE.md`

## INSERT
Description: Inserts rows via VALUES or SELECT sources, with optional ON
CONFLICT and RETURNING clauses.

Syntax (actual, abbreviated):
```sql
INSERT INTO <table_path> [(col, ...)]
  { VALUES (expr [, ...]) [, ...] | SELECT ... | DEFAULT VALUES }
  [ON CONFLICT <target> DO NOTHING | DO UPDATE SET ...]
  [RETURNING <select_list>]
```
Example:
```sql
INSERT INTO app.users (id, email) VALUES (1, 'a@example.com')
ON CONFLICT (id) DO UPDATE SET email = EXCLUDED.email;
```
Status: Implemented.
Spec delta: None known; confirm ON CONFLICT target forms in DML specs.

## UPDATE
Description: Updates rows with optional FROM and RETURNING.

Syntax (actual, abbreviated):
```sql
UPDATE <table_path> [AS alias]
SET col = expr [, ...]
[FROM <table_ref> [, ...]]
[WHERE <expr>]
[RETURNING <select_list>]
```
Example:
```sql
UPDATE app.users SET active = FALSE WHERE last_login < NOW() - INTERVAL '1 year';
```
Status: Implemented.
Spec delta: FROM clause is PostgreSQL-style extension; document as V2 extension
or remove if undesired.

## DELETE
Description: Deletes rows with optional USING and RETURNING.

Syntax (actual, abbreviated):
```sql
DELETE FROM <table_path> [USING <table_ref> [, ...]]
[WHERE <expr>]
[RETURNING <select_list>]
```
Example:
```sql
DELETE FROM app.users WHERE active = FALSE;
```
Status: Implemented.
Spec delta: USING clause is PostgreSQL-style extension.

## MERGE
Description: Merges source rows into a target with conditional actions.

Syntax (actual, abbreviated):
```sql
MERGE INTO <target> USING <source> ON <condition>
  WHEN MATCHED THEN UPDATE SET ...
  WHEN NOT MATCHED THEN INSERT (...)
```
Example:
```sql
MERGE INTO app.users t USING staging.users s ON t.id = s.id
WHEN MATCHED THEN UPDATE SET email = s.email
WHEN NOT MATCHED THEN INSERT (id, email) VALUES (s.id, s.email);
```
Status: Implemented.
Spec delta: Feature parity with full MERGE spec needs test coverage.
