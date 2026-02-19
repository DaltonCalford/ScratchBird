# DDL TABLE: SHOW
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
- Command lifecycle note: Table lifecycle is command-complete including DESCRIBE and TRUNCATE companion flow.
- Runtime note: Runtime table lifecycle is closed for primary forms in 0.1.0.

## Parser Surface
```sql
SHOW TABLE <schema.table_name>; SHOW TABLES [FROM <schema_name>] [LIKE <pattern>];
```

## Example
```sql
SHOW TABLE <schema.table_name>; SHOW TABLES [FROM <schema_name>] [LIKE <pattern>];
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
