# DDL DOMAIN: DROP
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [DESCRIBE](describe.md)
- Next: [Family README](../README.md)

## Coverage
- Status: Supported
- Command lifecycle note: Domain lifecycle is command-complete; advanced domain options are available in CREATE DOMAIN grammar.
- Runtime note: Runtime support exists for core domain DDL in 0.1.0.

## Parser Surface
```sql
DROP DOMAIN <domain_name> [RESTRICT|CASCADE];
```

## Example
```sql
DROP DOMAIN <domain_name> [RESTRICT|CASCADE];
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
