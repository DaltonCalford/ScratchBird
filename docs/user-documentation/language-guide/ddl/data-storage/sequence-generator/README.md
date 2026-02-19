# DDL Object: SEQUENCE OR GENERATOR
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Sequence/generator lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime path available in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE SEQUENCE <sequence_name>; CREATE GENERATOR <generator_name>; |
| ALTER | Supported | ALTER SEQUENCE <sequence_name> RESTART WITH <value>; ALTER GENERATOR <generator_name> TO <value>; |
| SHOW | Supported | SHOW SEQUENCE <sequence_name>; SHOW GENERATOR <generator_name>; |
| DESCRIBE | Not available | -- No DESCRIBE variant for SEQUENCE/GENERATOR. |
| DROP | Supported | DROP SEQUENCE <sequence_name>; DROP GENERATOR <generator_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
