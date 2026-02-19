# DDL TRIGGER: DROP
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [DESCRIBE](describe.md)
- Next: [Family README](../README.md)

## Coverage
- Status: Supported
- Command lifecycle note: Trigger lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime exposes OLD.<column> and NEW.<column> row context in trigger execution path.

## Parser Surface
```sql
DROP TRIGGER <trigger_name>;
```

## Example
```sql
DROP TRIGGER <trigger_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
