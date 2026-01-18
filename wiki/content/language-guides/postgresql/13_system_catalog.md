[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# PostgreSQL - System Catalog Surface

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md`

## Catalog namespaces (expected)
- `pg_catalog` (pg_class, pg_attribute, etc)
- `information_schema`

## Implementation status
Status: Partial.

Notes:
- PostgreSQL emulation catalogs are implemented via generated views over
  ScratchBird metadata; parity is tracked in
  `POSTGRESQL_PARSER_IMPLEMENTATION_GAPS.md`.
