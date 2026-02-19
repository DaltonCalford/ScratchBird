# Type Family: Text And Binary
Last modified: 2026-02-19

Back links:
- [Type Families README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Numeric](numeric.md)
- Next: [Temporal](temporal.md)

## Parser-Accepted Names
- Character/text: `CHAR`, `CHARACTER`, `VARCHAR`, `TEXT`.
- Binary/blob: `BINARY`, `VARBINARY`, `BYTEA`, `BLOB`, `BLOB_TEXT`, `BIT`, `QBIT`.

## Format And Collation Notes
- Parser supports `CHARACTER VARYING` normalization to `VARCHAR`.
- Binary/text domain assignment is finalized through domain/default-domain mapping in catalog/runtime.
- Object identifiers are case-insensitive by default in parser token handling; preserving quoted/case-sensitive semantics remains object-name contract specific.

## Operator Compatibility
- String pattern operations: `LIKE`, `ILIKE`, regex operators (`~`, `~*`, `!~`, `!~*`).
- Concatenation: `||` is runtime-mapped through concat function opcode.
