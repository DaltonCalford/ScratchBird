# DDL PACKAGE: SHOW
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
- Command lifecycle note: Package lifecycle is command-complete through SHOW and DROP.
- Runtime note: Package/runtime closure follows procedure/function body constraints.

## Parser Surface
```sql
SHOW PACKAGE <package_name>; SHOW PACKAGES;
```

## Example
```sql
SHOW PACKAGE <package_name>; SHOW PACKAGES;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
