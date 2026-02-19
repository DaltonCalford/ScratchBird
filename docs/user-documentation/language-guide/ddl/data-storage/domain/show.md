# DDL DOMAIN: SHOW
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
- Command lifecycle note: Domain lifecycle is command-complete; advanced domain options are available in CREATE DOMAIN grammar.
- Runtime note: Runtime support exists for core domain DDL in 0.1.0.

## Parser Surface
```sql
SHOW DOMAIN <domain_name>; SHOW DOMAINS;
```

## Example
```sql
SHOW DOMAIN <domain_name>; SHOW DOMAINS;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
