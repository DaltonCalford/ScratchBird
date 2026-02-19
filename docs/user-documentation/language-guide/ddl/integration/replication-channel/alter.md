# DDL REPLICATION CHANNEL: ALTER
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
- Command lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Replication commands parse and emit; runtime still includes bridge-partial areas.

## Parser Surface
```sql
ALTER REPLICATION CHANNEL <channel_name> <alter_actions>; RESYNC REPLICATION CHANNEL <channel_name>;
```

## Example
```sql
ALTER REPLICATION CHANNEL <channel_name> <alter_actions>; RESYNC REPLICATION CHANNEL <channel_name>;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
