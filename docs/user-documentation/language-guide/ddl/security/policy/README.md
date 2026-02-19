# DDL Object: POLICY
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Security README](../README.md)

## Summary
- Family: Security
- Lifecycle status: Partial command lifecycle
- Lifecycle note: SHOW/DESCRIBE missing keeps lifecycle partial.
- Runtime note: Policy lifecycle supports create/alter/drop command surfaces.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE POLICY <policy_name> ON <object_name> USING (<predicate>); |
| ALTER | Supported | ALTER POLICY <policy_name> USING (<predicate>); |
| SHOW | Not available | -- No explicit SHOW POLICY command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for POLICY. |
| DROP | Supported | DROP POLICY <policy_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
