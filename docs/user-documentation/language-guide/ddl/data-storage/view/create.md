# DDL VIEW: CREATE
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
- Command lifecycle note: View lifecycle is complete through SHOW and DROP paths.
- Runtime note: Runtime mutation and display paths are available for core view forms.

## Parser Surface
```sql
CREATE VIEW <schema.view_name> AS <select_statement>;
```

## Example
```sql
CREATE VIEW <schema.view_name> AS <select_statement>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
