# DDL CUBE: CREATE
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
- Command lifecycle note: Cube object lifecycle is parser+emitter complete through create/alter/show/drop forms.
- Runtime note: Runtime semantic bridge is still partial in 0.1.0 (see ../../TODO_BETA_0_2_0.md).

## Parser Surface
```sql
CREATE CUBE <cube_name> AS <cube_select>;
```

## Example
```sql
CREATE CUBE <cube_name> AS <cube_select>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
