# DDL REPLICATION CHANNEL: CREATE
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
- Command lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Replication commands parse and emit; runtime still includes bridge-partial areas.

## Parser Surface
```sql
CREATE REPLICATION CHANNEL <channel_name> DIRECTION <ONE_WAY|BIDIRECTIONAL> SOURCE <source_name> TARGET <target_name>;
```

## Example
```sql
CREATE REPLICATION CHANNEL <channel_name> DIRECTION <ONE_WAY|BIDIRECTIONAL> SOURCE <source_name> TARGET <target_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
