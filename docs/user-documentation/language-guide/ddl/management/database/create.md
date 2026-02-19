# DDL DATABASE: CREATE
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
- Command lifecycle note: Native database lifecycle exists; emulated create path is additionally exposed.
- Runtime note: Core DATABASE lifecycle is available; EMULATED path is parser-rich but runtime contract is still being normalized.

## Parser Surface
```sql
CREATE DATABASE <database_name>; CREATE DATABASE EMULATED <engine> ON SERVER <server_name> '<remote_path>' WITH OPTIONS (...) ALIAS <alias_name>;
```

## Example
```sql
CREATE DATABASE <database_name>; CREATE DATABASE EMULATED <engine> ON SERVER <server_name> '<remote_path>' WITH OPTIONS (...) ALIAS <alias_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
