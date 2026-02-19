# DDL MEASUREMENT: CREATE
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
- Command lifecycle note: No DROP/SHOW commands keep lifecycle partial.
- Runtime note: Runtime closure for full measurement lifecycle is not complete in 0.1.0.

## Parser Surface
```sql
CREATE MEASUREMENT <measurement_name> ON <table_or_stream>(<columns>);
```

## Example
```sql
CREATE MEASUREMENT <measurement_name> ON <table_or_stream>(<columns>);
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
