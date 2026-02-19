# DDL Object: SYNONYM
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Synonym lifecycle is command-partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE SYNONYM <synonym_name> FOR <target_object>; |
| ALTER | Supported | ALTER SYNONYM <synonym_name> <alter_actions>; |
| SHOW | Not available | -- No explicit SHOW SYNONYM command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for SYNONYM. |
| DROP | Supported | DROP SYNONYM <synonym_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
