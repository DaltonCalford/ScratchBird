# DDL PROCEDURE: SHOW
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
- Command lifecycle note: Procedure lifecycle is command-complete through SHOW and DROP.
- Runtime note: Routine body semantics support IF/ELSIF/ELSE, loops, WHEN, EXCEPTION; TRY blocks are not implemented.

## Parser Surface
```sql
SHOW PROCEDURE <procedure_name>; SHOW PROCEDURES;
```

## Example
```sql
SHOW PROCEDURE <procedure_name>; SHOW PROCEDURES;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
