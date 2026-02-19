# DDL PROCEDURE: DROP
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
- Command lifecycle note: Procedure lifecycle is command-complete through SHOW and DROP.
- Runtime note: Routine body semantics support IF/ELSIF/ELSE, loops, WHEN, EXCEPTION; TRY blocks are not implemented.

## Parser Surface
```sql
DROP PROCEDURE <procedure_name>;
```

## Example
```sql
DROP PROCEDURE <procedure_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
