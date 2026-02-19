# DDL DATABASE CONNECTION: ALTER
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Family README](../README.md)
- [Object README](README.md)

Lifecycle navigation:
- Previous: [CREATE](create.md)
- Next: [SHOW](show.md)

## Coverage
- Status: Supported
- Command lifecycle note: External connection object has create/alter/drop surfaces but no explicit show/describe command.
- Runtime note: Current runtime normalization is through admin/config key contracts rather than dedicated catalog object opcode family.

## Parser Surface
```sql
ALTER DATABASE CONNECTION <name> AUTH_MODE <SHARED|NAMED> ...;
```

## Example
```sql
ALTER DATABASE CONNECTION <name> AUTH_MODE <SHARED|NAMED> ...;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
