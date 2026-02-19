# DDL VECTOR INDEX: ALTER
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
- Status: Partial
- Command lifecycle note: ALTER supports only REBUILD in 0.1.0; SHOW/DESCRIBE are missing.
- Runtime note: Lifecycle remains partial pending broader ALTER and observability commands.

## Parser Surface
```sql
ALTER VECTOR INDEX <index_name> REBUILD [ONLINE|OFFLINE];
```

## Example
```sql
ALTER VECTOR INDEX <index_name> REBUILD [ONLINE|OFFLINE];
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
- This phase is partial in 0.1.0; only constrained forms are currently accepted.
