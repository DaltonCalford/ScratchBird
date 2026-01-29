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
Description: Collects statistics about the contents of a table or specific column, which
the query planner can use to generate better execution plans.

Syntax (actual):
```sql
ANALYZE [VERBOSE] <table_name> [(column_name)]
```
Example:
```sql
ANALYZE app.users;
ANALYZE VERBOSE app.orders (amount);
```
Status: **Implemented in V2 Parser** - `parseAnalyze()` handles standalone ANALYZE with optional
VERBOSE flag, table name (schema-qualified), and optional single column name.
Spec delta: Only one column may be specified per ANALYZE statement.

---

## DESCRIBE / DESC
Description: Describes database objects (tables, columns, etc.).

Syntax (actual):
```sql
DESCRIBE <object_name>
DESC <object_name>
```
Example:
```sql
DESCRIBE app.users;
DESC orders;
```
Status: **Implemented in V2 Parser** - `parseDescribe()` handles both DESCRIBE and DESC keywords.

---

## SWEEP DATABASE
Description: Triggers a manual garbage-collection sweep of the database, cleaning up
obsolete record versions created by the Multi-Generational Architecture (MGA).

Syntax (actual):
```sql
SWEEP DATABASE
```
Example:
```sql
SWEEP DATABASE;
```
Status: **Implemented in V2 Parser** - `parseSweep()` parses the SWEEP DATABASE statement.

---

## CONNECT
Description: Establishes a connection to a database, optionally specifying user credentials,
role, and character set.

Syntax (actual):
```sql
CONNECT [TO] <database_name>
    [USER <user_name>]
    [PASSWORD '<password>']
    [ROLE <role_name>]
    [CHARSET <charset> | CHARACTER SET <charset>]
```
Examples:
```sql
CONNECT mydb;
CONNECT TO production_db USER admin PASSWORD 'secret' ROLE dba;
CONNECT TO analytics CHARSET UTF8;
```
Status: **Implemented in V2 Parser** - `parseConnect()` parses CONNECT with optional TO keyword,
USER, PASSWORD (string literal or identifier), ROLE, and CHARSET/CHARACTER SET clauses.

---

## DISCONNECT
Description: Closes a database connection.

Syntax (actual):
```sql
DISCONNECT [ALL | CURRENT | <connection_name>]
```
Examples:
```sql
DISCONNECT;
DISCONNECT ALL;
DISCONNECT CURRENT;
DISCONNECT mydb;
```
Status: **Implemented in V2 Parser** - `parseDisconnect()` parses DISCONNECT with ALL, CURRENT,
or named connection targets.
