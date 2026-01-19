# MySQL - System Catalog Surface

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
