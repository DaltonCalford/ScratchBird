# DDL CLUSTER WORKLOAD CLASS: SHOW
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
- Command lifecycle note: Cluster workload class has full create/alter/show/drop command families.
- Runtime note: Runtime semantics for cluster command families are still bridge-partial in 0.1.0.

## Parser Surface
```sql
SHOW CLUSTER ROUTING PLAN; CLUSTER SHOW ROUTING PLAN;
```

## Example
```sql
SHOW CLUSTER ROUTING PLAN; CLUSTER SHOW ROUTING PLAN;
```

## Notes
- This phase is documented from parser/emitter/executor evidence for 0.1.0.
- Full matrix and opcode-level notes are in [Consolidated Audit Reference](../../../NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md).
