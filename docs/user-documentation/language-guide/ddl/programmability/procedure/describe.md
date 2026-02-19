# DDL PROCEDURE: DESCRIBE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [SHOW](show.md)
- Next: [DROP](drop.md)

## Coverage
- Status: Not available
- Command lifecycle note: Procedure lifecycle is command-complete through SHOW and DROP.
- Runtime note: Routine body semantics support IF/ELSIF/ELSE, loops, WHEN, EXCEPTION; TRY blocks are not implemented.

## Parser Surface
```sql
-- No explicit native v3 DESCRIBE command surface for PROCEDURE in 0.1.0.
```

## Example
```sql
-- No explicit native v3 DESCRIBE command surface for PROCEDURE in 0.1.0.
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
- This phase has no dedicated command form in 0.1.0; use related object commands or metadata inspection where applicable.
