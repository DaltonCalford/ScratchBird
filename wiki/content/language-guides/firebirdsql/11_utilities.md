[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Utilities

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## EXPLAIN / PLAN
Description: Firebird uses PLAN clauses on SELECT statements rather than standalone
EXPLAIN. The Firebird parser does not expose a standalone EXPLAIN statement.

Status: Not applicable (Firebird dialect uses inline PLAN syntax, not EXPLAIN).

## COMMENT ON
Description: Attaches comments to database objects.

Syntax (actual):
```sql
COMMENT ON <object_type> <object_name> IS <string_or_null>
```
Example:
```sql
COMMENT ON TABLE employees IS 'Main employee records';
```
Status: **Implemented** - `parseCommentStatement()` handles COMMENT ON for TABLE,
COLUMN, VIEW, PROCEDURE, FUNCTION, TRIGGER, EXCEPTION, DOMAIN, SEQUENCE, INDEX,
ROLE, PACKAGE, and DATABASE.

## COPY / DESCRIBE
Description: Not part of Firebird SQL dialect.

Status: Not applicable (Firebird does not define COPY or DESCRIBE statements).
