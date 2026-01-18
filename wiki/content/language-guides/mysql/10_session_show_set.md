[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - Session, SHOW, SET

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/parser/MYSQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/18_mysql_parser_statement_reference_actual.md`

## SHOW commands
### Implemented
- `SHOW DATABASES [LIKE pattern]`
- `SHOW TABLES [FROM db] [LIKE pattern]`
- `SHOW COLUMNS FROM table [LIKE pattern]`
- `SHOW INDEXES FROM table`
- `SHOW CREATE TABLE table`

### Missing
- `SHOW CREATE DATABASE`, `SHOW PROCESSLIST`, `SHOW STATUS`, `SHOW WARNINGS`,
  `SHOW ERRORS`, `SHOW VARIABLES`.

Example:
```sql
SHOW TABLES FROM mydb;
```

## SET commands
- `SET AUTOCOMMIT = {0|1}`: Implemented.
- `SET <var> = <expr>`: Parsed but executor only supports search_path; treated
  as stubbed for MySQL variables.

Example:
```sql
SET AUTOCOMMIT = 0;
```

## USE
Description: Sets current database (mapped to search_path).

Syntax (actual):
```sql
USE <database>
```
Example:
```sql
USE mydb;
```
Status: Implemented.

## DESCRIBE / DESC
Description: Describes a table (mapped to EXT_DESCRIBE_TABLE).

Syntax (actual):
```sql
DESCRIBE <table>
DESC <table>
```
Example:
```sql
DESCRIBE users;
```
Status: Implemented.

## SHOW DATABASE vs SHOW SCHEMA
MySQL treats DATABASE and SCHEMA as synonyms; SHOW SCHEMA is not a separate
command in this parser. Use SHOW DATABASES to list schemas/databases.
