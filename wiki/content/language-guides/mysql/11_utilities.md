[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - Utilities

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## LOCK TABLES / UNLOCK TABLES
Description: Parsed but treated as no-op in executor.

Syntax (actual):
```sql
LOCK TABLES <table> READ|WRITE [, ...]
UNLOCK TABLES
```
Example:
```sql
LOCK TABLES users WRITE;
UNLOCK TABLES;
```
Status: Partial (no-op).

## EXPLAIN / ANALYZE / COPY
Description: Not implemented in MySQL parser.

Status: Missing.
