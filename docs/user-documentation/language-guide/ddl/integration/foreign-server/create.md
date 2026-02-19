# DDL FOREIGN SERVER: CREATE
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
- Command lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Runtime closure for server/object semantics is not fully canonicalized in v3 dispatch.

## Parser Surface
```sql
CREATE SERVER <server_name> FOREIGN DATA WRAPPER <fdw_name> OPTIONS (...);
```

## Example
```sql
CREATE SERVER <server_name> FOREIGN DATA WRAPPER <fdw_name> OPTIONS (...);
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
