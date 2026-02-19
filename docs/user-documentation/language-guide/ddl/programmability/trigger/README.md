# DDL Object: TRIGGER
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Programmability README](../README.md)

## Summary
- Family: Programmability
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Trigger lifecycle is command-complete through SHOW and DROP.
- Runtime note: Runtime exposes OLD.<column> and NEW.<column> row context in trigger execution path.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE TRIGGER <trigger_name> <timing> <event> ON <table_name> ...; |
| ALTER | Supported | ALTER TRIGGER <trigger_name> [ACTIVE|INACTIVE|other_actions]; |
| SHOW | Supported | SHOW TRIGGER <trigger_name>; SHOW TRIGGERS; |
| DESCRIBE | Not available | -- No DESCRIBE variant for TRIGGER. |
| DROP | Supported | DROP TRIGGER <trigger_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
