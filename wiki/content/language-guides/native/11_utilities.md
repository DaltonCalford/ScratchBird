[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native (V2) - Utilities

Spec refs:
- `ScratchBird/docs/specifications/parser/08_PARSER_AND_DEVELOPER_EXPERIENCE.md`
- `ScratchBird/docs/specifications/dml/04_DML_STATEMENTS_OVERVIEW.md`

## EXPLAIN
Description: Displays a query plan; EXPLAIN ANALYZE executes the query.

Syntax (actual):
```sql
EXPLAIN [ANALYZE] [VERBOSE] [COSTS] [BUFFERS] [TIMING]
  [FORMAT {TEXT|JSON|XML|YAML}] <statement>
```
Example:
```sql
EXPLAIN (ANALYZE, BUFFERS) SELECT * FROM app.users WHERE id = 1;
```
Status: Implemented.
Spec delta: Output is simplified vs optimizer spec; JSON/XML/YAML payloads are
lightweight summaries.

## COPY
Description: Bulk load/unload to/from files.

Syntax (actual, abbreviated):
```sql
COPY <table> [(col, ...)] FROM '<file>' [WITH (...)]
COPY <table> [(col, ...)] TO '<file>' [WITH (...)]
COPY (SELECT ...) TO '<file>' [WITH (...)]
```
Example:
```sql
COPY app.users (id, email) TO '/tmp/users.tsv' WITH (FORMAT 'text');
```
Status: Partial.
Spec delta: CSV/TEXT formatting, DELIMITER/NULL/HEADER options, and STDIN/STDOUT
are wired; BINARY and ENCODING remain unsupported in the executor.

## COMMENT
Description: Attaches comments to database objects.

Syntax (actual):
```sql
COMMENT ON <object_type> <object_name> IS <string_or_null>
```
Example:
```sql
COMMENT ON TABLE app.users IS 'Application users';
```
Status: Implemented.
Spec delta: None known.

## ANALYZE (standalone)
Description: Standalone ANALYZE is not parsed; use EXPLAIN ANALYZE.

Status: Missing.
Spec delta: Implement ANALYZE if required by compatibility specs.

## DESCRIBE / DESC
Description: Describes database objects (tables, columns, etc.).

```sql
DESCRIBE <object_name>
DESC <object_name>
```

Status: **Implemented in V2 Parser** - `parseDescribe()` handles both DESCRIBE and DESC keywords.
