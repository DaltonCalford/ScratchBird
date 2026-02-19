# DDL FUNCTION: CREATE
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
- Command lifecycle note: Function lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime behavior depends on function body/emitter closure for specific constructs.

## Parser Surface
```sql
CREATE FUNCTION <function_name>(...) RETURNS <type_name> AS BEGIN ... END;
DECLARE EXTERNAL FUNCTION <function_name> ... ENTRY_POINT '<symbol>' MODULE_NAME '<module>';
```

## Example
```sql
CREATE FUNCTION <function_name>(...) RETURNS <type_name> AS BEGIN ... END;
DECLARE EXTERNAL FUNCTION fn_crc32(BLOB) RETURNS INTEGER
  ENTRY_POINT 'fn_crc32' MODULE_NAME 'libext_udf.so';
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
