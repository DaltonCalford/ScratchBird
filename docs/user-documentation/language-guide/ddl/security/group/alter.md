# DDL GROUP: ALTER
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
- Runtime note: Group lifecycle is partially closed in 0.1.0.

## Parser Surface
```sql
ALTER GROUP [IF EXISTS] <group_name> RENAME TO <new_name>;
ALTER GROUP [IF EXISTS] <group_name> SET SCHEMA <schema_path>;
```

## Example
```sql
ALTER GROUP analytics RENAME TO analytics_ro;
ALTER GROUP analytics_ro SET SCHEMA security;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
