# DDL POLICY: ALTER
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
- Runtime note: Policy lifecycle supports create/alter/drop command surfaces.

## Parser Surface
```sql
ALTER POLICY <policy_name> USING (<predicate>);
```

## Example
```sql
ALTER POLICY <policy_name> USING (<predicate>);
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
