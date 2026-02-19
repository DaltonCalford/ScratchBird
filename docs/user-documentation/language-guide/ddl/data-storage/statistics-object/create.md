# DDL STATISTICS: CREATE
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
- Command lifecycle note: Create-only parser surface in 0.1.0.
- Runtime note: Lifecycle remains create-only and requires closure for beta-hardening.

## Parser Surface
```sql
CREATE STATISTICS <name> (<kind>) ON <columns> FROM <table_name>;
```

## Example
```sql
CREATE STATISTICS <name> (<kind>) ON <columns> FROM <table_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
