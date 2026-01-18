[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - Utilities

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/parser/POSTGRESQL_PARSER_SPECIFICATION.md`
- `ScratchBird/docs/audit/17_postgresql_parser_statement_reference_actual.md`

## ANALYZE
Description: Collects statistics.

Syntax (actual):
```sql
ANALYZE [VERBOSE] [table [ (col, ...) ]]
```
Example:
```sql
ANALYZE users;
```
Status: Implemented (EXT_ANALYZE).

## EXPLAIN
Description: Explains a query plan.

Syntax (actual):
```sql
EXPLAIN [ANALYZE] [VERBOSE] <statement>
```
Example:
```sql
EXPLAIN SELECT * FROM users;
```
Status: Implemented (EXPLAIN_PLAN handled by executor).
Spec delta: Output remains a simplified plan summary vs optimizer spec.

## COPY
Description: COPY support is partial in parser with file/STDIN/STDOUT targets.

Status: Partial (file COPY handled in executor; protocol COPY streams handled by adapter).
