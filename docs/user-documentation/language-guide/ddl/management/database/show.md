# DDL DATABASE: SHOW
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
- Command lifecycle note: Native database lifecycle exists; emulated create path is additionally exposed.
- Runtime note: Core DATABASE lifecycle is available; EMULATED path is parser-rich but runtime contract is still being normalized.

## Parser Surface
```sql
SHOW DATABASE;
```

## Example
```sql
SHOW DATABASE;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
