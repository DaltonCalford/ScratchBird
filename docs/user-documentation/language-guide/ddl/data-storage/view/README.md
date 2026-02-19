# DDL Object: VIEW
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Complete command lifecycle
- Lifecycle note: View lifecycle is complete through SHOW and DROP paths.
- Runtime note: Runtime mutation and display paths are available for core view forms.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE VIEW <schema.view_name> AS <select_statement>; |
| ALTER | Supported | ALTER VIEW <schema.view_name> <alter_actions>; |
| SHOW | Supported | SHOW VIEW <schema.view_name>; SHOW VIEWS; |
| DESCRIBE | Not available | -- No DESCRIBE variant for VIEW. |
| DROP | Supported | DROP VIEW <schema.view_name>; DROP MATERIALIZED VIEW <schema.view_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
