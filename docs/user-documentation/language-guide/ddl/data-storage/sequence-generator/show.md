# DDL SEQUENCE OR GENERATOR: SHOW
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [ALTER](alter.md)
- Next: [DESCRIBE](describe.md)

## Coverage
- Status: Supported
- Command lifecycle note: Sequence/generator lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime path available in 0.1.0.

## Parser Surface
```sql
SHOW SEQUENCE <sequence_name>; SHOW GENERATOR <generator_name>;
```

## Example
```sql
SHOW SEQUENCE <sequence_name>; SHOW GENERATOR <generator_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
