# DDL Object: FUNCTION
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Programmability README](../README.md)

## Summary
- Family: Programmability
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Function lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime behavior depends on function body/emitter closure for specific constructs.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE FUNCTION <function_name>(...) RETURNS <type_name> AS BEGIN ... END; |
| ALTER | Supported | ALTER FUNCTION <function_name>(...) <alter_actions>; |
| SHOW | Supported | SHOW FUNCTION <function_name>; SHOW FUNCTIONS; |
| DESCRIBE | Not available | -- No DESCRIBE variant for FUNCTION. |
| DROP | Supported | DROP FUNCTION <function_name>(...); |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
