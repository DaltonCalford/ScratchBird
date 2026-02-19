# DDL UDR: CREATE
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
- Command lifecycle note: Create/drop only in parser command surface.
- Runtime note: Lifecycle remains partial in 0.1.0.

## Parser Surface
```sql
CREATE UDR <udr_name> ...;
COMPILE UDR <udr_name> ...;
VALIDATE EMBEDDED SQL ...;
```

## Example
```sql
CREATE UDR udf_hash ...;
COMPILE UDR udf_hash TARGET native;
VALIDATE EMBEDDED SQL LANGUAGE c PROFILE native_required;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
- Compile/validate commands are exposed as top-level UDR utility surfaces and should be tested with policy-bound profiles.
