# DDL DATABASE CONNECTION: CREATE
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
- Command lifecycle note: External connection object has create/alter/drop surfaces but no explicit show/describe command.
- Runtime note: Current runtime normalization is through admin/config key contracts rather than dedicated catalog object opcode family.

## Parser Surface
```sql
CREATE DATABASE CONNECTION <name> HOST '<host>' MOUNT '<mount_path>' AUTH_MODE <SHARED|NAMED> ROLE '<role>' PASSWORD '<secret>' [GROUP '<group_name>'];
```

## Example
```sql
CREATE DATABASE CONNECTION <name> HOST '<host>' MOUNT '<mount_path>' AUTH_MODE <SHARED|NAMED> ROLE '<role>' PASSWORD '<secret>' [GROUP '<group_name>'];
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
