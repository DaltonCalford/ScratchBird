# Type Family: Advanced And Container
Last modified: 2026-02-19

Back links:
- [Type Families README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Network, Geo, And Range](network-geo-range.md)

## Parser-Accepted Names
- Logical/container/meta families: `ENUM`, `SET`, `ROW`, `COMPOSITE`, `DOMAIN`, `VARIANT`, `DYNAMIC`, `ARRAY`.
- Type grammar supports nested type arguments and array suffixes (`[]` and `[n]`).

## Type-Of Syntax
- Firebird-style forms are accepted:
  - `TYPE OF <type_ref>`
  - `TYPE OF COLUMN <table.column_ref>`

## Domain Interaction
- Unknown type names fall back to domain/reference encoding in emitter (`SBLR3_TYPE_DOMAIN`).
- Domain lifecycle and standard domain inventories are documented in [Domains](../domains/README.md).
