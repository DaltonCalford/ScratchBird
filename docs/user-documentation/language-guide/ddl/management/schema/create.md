# DDL SCHEMA: CREATE
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
- Command lifecycle note: Schema namespace lifecycle is closed in parser command families.
- Runtime note: Runtime metadata path is available in 0.1.0.

## Parser Surface
```sql
CREATE SCHEMA <schema_name>;
```

## Example
```sql
CREATE SCHEMA <schema_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
