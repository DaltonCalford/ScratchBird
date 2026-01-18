[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Utilities

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

## EXPLAIN / PLAN
Description: Firebird supports PLAN clauses, not EXPLAIN; parser does not
implement EXPLAIN in Firebird dialect.

Status: Missing.

## COPY / DESCRIBE / COMMENT
Description: Not implemented in Firebird parser.

Status: Missing.
