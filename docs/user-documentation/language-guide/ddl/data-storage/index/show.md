# DDL INDEX: SHOW
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
- Command lifecycle note: Classic index lifecycle is command-complete via SHOW INDEX surfaces.
- Runtime note: Core index paths are available; some advanced method semantics depend on backend operator classes.

## Parser Surface
```sql
SHOW INDEX <index_name>; SHOW INDEXES FROM <table_name>;
```

## Example
```sql
SHOW INDEX <index_name>; SHOW INDEXES FROM <table_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
