# DDL SEQUENCE OR GENERATOR: ALTER
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [CREATE](create.md)
- Next: [SHOW](show.md)

## Coverage
- Status: Supported
- Command lifecycle note: Sequence/generator lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime path available in 0.1.0.

## Parser Surface
```sql
ALTER SEQUENCE <sequence_name> RESTART WITH <value>; ALTER GENERATOR <generator_name> TO <value>;
```

## Example
```sql
ALTER SEQUENCE <sequence_name> RESTART WITH <value>; ALTER GENERATOR <generator_name> TO <value>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
