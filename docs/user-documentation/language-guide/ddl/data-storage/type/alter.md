# DDL TYPE: ALTER
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
- Command lifecycle note: Missing SHOW/DESCRIBE keeps lifecycle partial.
- Runtime note: Emitter/runtime closure for CREATE TYPE remains partial in 0.1.0.

## Parser Surface
```sql
ALTER TYPE <type_name> <alter_actions>;
```

## Example
```sql
ALTER TYPE <type_name> <alter_actions>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
