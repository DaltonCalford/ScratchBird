# DDL INDEX: CREATE
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
- Command lifecycle note: Classic index lifecycle is command-complete via SHOW INDEX surfaces.
- Runtime note: Core index paths are available; some advanced method semantics depend on backend operator classes.

## Parser Surface
```sql
CREATE INDEX <index_name> ON <table_name>(<columns>) [USING <method>];
```

## Example
```sql
CREATE INDEX <index_name> ON <table_name>(<columns>) [USING <method>];
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
