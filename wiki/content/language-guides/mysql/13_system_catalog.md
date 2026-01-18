[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# MySQL - System Catalog Surface

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/MYSQL_PARSER_IMPLEMENTATION_GAPS.md`

## Catalog namespaces (expected)
- `information_schema`
- `performance_schema` (optional)
- `mysql` (system tables)

## Implementation status
Status: Partial.

Notes:
- MySQL emulation catalogs are implemented as views over ScratchBird metadata.
- Parity items are tracked in `MYSQL_PARSER_IMPLEMENTATION_GAPS.md`.
