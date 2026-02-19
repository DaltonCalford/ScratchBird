# DDL CUBE: SHOW
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
- Command lifecycle note: Cube object lifecycle is parser+emitter complete through create/alter/show/drop forms.
- Runtime note: Runtime semantic bridge is still partial in 0.1.0 (see ../../TODO_BETA_0_2_0.md).

## Parser Surface
```sql
SHOW CUBE STATS <cube_name>; CUBE SHOW STATS <cube_name>;
```

## Example
```sql
SHOW CUBE STATS <cube_name>; CUBE SHOW STATS <cube_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
