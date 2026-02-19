# DDL ROLE: ALTER
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
- Command lifecycle note: Generic object ALTER supports rename/move semantics.
- Runtime note: Role lifecycle is partially closed in 0.1.0.

## Parser Surface
```sql
ALTER ROLE [IF EXISTS] <role_name> RENAME TO <new_name>;
ALTER ROLE [IF EXISTS] <role_name> SET SCHEMA <schema_path>;
```

## Example
```sql
ALTER ROLE app_reader RENAME TO app_ro;
ALTER ROLE app_ro SET SCHEMA security;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
