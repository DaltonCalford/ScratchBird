# DDL Object: TYPE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Partial command lifecycle
- Lifecycle note: Missing SHOW/DESCRIBE keeps lifecycle partial.
- Runtime note: Emitter/runtime closure for CREATE TYPE remains partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE TYPE <type_name> AS <type_definition>; |
| ALTER | Supported | ALTER TYPE <type_name> <alter_actions>; |
| SHOW | Not available | -- No explicit SHOW TYPE command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for TYPE. |
| DROP | Supported | DROP TYPE <type_name> [RESTRICT|CASCADE]; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
