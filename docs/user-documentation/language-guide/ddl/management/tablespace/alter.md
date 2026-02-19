# DDL TABLESPACE: ALTER
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
- Command lifecycle note: Missing explicit SHOW/DESCRIBE command keeps lifecycle partial.
- Runtime note: DDL mutation surface exists; observability is indirect in 0.1.0.

## Parser Surface
```sql
ALTER TABLESPACE <tablespace_name> RESIZE ...;
```

## Example
```sql
ALTER TABLESPACE <tablespace_name> RESIZE ...;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
