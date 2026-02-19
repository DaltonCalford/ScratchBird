# DDL PROCEDURE: CREATE
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
- Command lifecycle note: Procedure lifecycle is command-complete through SHOW and DROP.
- Runtime note: Routine body semantics support IF/ELSIF/ELSE, loops, WHEN, EXCEPTION; TRY blocks are not implemented.

## Parser Surface
```sql
CREATE PROCEDURE <procedure_name>(...) AS DECLARE ... BEGIN ... END;
```

## Example
```sql
CREATE PROCEDURE <procedure_name>(...) AS DECLARE ... BEGIN ... END;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
