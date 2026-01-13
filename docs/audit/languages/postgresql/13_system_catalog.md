# PostgreSQL - System Catalog Surface

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
