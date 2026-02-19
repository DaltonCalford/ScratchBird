# DDL EXTENSION: CREATE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [Object README](README.md)
- Next: [ALTER](alter.md)

## Coverage
- Status: Supported
- Command lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Extension lifecycle is command-partial in 0.1.0.

## Parser Surface
```sql
CREATE EXTENSION <extension_name> [WITH VERSION '<version>'];
INSTALL EXTENSION <extension_name>;
LOAD EXTENSION <extension_name>;
```

## Example
```sql
CREATE EXTENSION vector_ext WITH VERSION '1.2.0';
INSTALL EXTENSION vector_ext;
LOAD EXTENSION vector_ext;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
- `INSTALL EXTENSION` and `LOAD EXTENSION` are top-level utility dispatch forms and are part of the same extension lifecycle surface.
