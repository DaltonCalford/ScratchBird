# DDL VECTOR INDEX: CREATE
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
- Command lifecycle note: ALTER supports only REBUILD in 0.1.0; SHOW/DESCRIBE are missing.
- Runtime note: Lifecycle remains partial pending broader ALTER and observability commands.

## Parser Surface
```sql
CREATE VECTOR INDEX <index_name> ON <table_name>(<vector_column>) WITH (metric='<metric>');
```

## Example
```sql
CREATE VECTOR INDEX <index_name> ON <table_name>(<vector_column>) WITH (metric='<metric>');
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
