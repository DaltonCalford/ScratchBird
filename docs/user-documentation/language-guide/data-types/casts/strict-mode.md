# Strict Mode
Last modified: 2026-02-19

Back links:
- [Casts README](README.md)
- [Data Types README](../README.md)

Series navigation:
- Previous: [Implicit Conversion](implicit-casts.md)

Strict mode command:
~~~sql
SET operator.strict_mode ON;
SET operator.strict_mode OFF;
~~~

Behavior:
- when enabled, implicit operator coercions are disabled for covered operator families
- explicit casts remain valid and are preferred for deterministic behavior

Guidance:
- keep strict mode enabled for validation pipelines and cross-dialect compatibility testing
- use explicit casts in persistent ETL or schema-default expressions
