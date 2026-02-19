# DDL SEQUENCE OR GENERATOR: CREATE
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
- Command lifecycle note: Sequence/generator lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime path available in 0.1.0.

## Parser Surface
```sql
CREATE SEQUENCE <sequence_name>; CREATE GENERATOR <generator_name>;
```

## Example
```sql
CREATE SEQUENCE <sequence_name>; CREATE GENERATOR <generator_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
