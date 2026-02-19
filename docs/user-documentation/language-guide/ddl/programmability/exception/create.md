# DDL EXCEPTION: CREATE
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
- Command lifecycle note: Create/drop only in parser command surface.
- Runtime note: Lifecycle remains partial due to missing ALTER and SHOW/DESCRIBE.

## Parser Surface
```sql
CREATE EXCEPTION <exception_name> '<message_text>';
```

## Example
```sql
CREATE EXCEPTION <exception_name> '<message_text>';
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
