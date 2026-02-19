# DDL DATABASE CONNECTION: SHOW
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
- Status: Not available
- Command lifecycle note: External connection object has create/alter/drop surfaces but no explicit show/describe command.
- Runtime note: Current runtime normalization is through admin/config key contracts rather than dedicated catalog object opcode family.

## Parser Surface
```sql
-- No explicit native v3 SHOW command surface for DATABASE CONNECTION in 0.1.0.
```

## Example
```sql
-- No explicit native v3 SHOW command surface for DATABASE CONNECTION in 0.1.0.
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
- This phase has no dedicated command form in 0.1.0; use related object commands or metadata inspection where applicable.
