# DDL Object: ACCESS METHOD
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Data Storage README](../README.md)

## Summary
- Family: Data Storage
- Lifecycle status: Partial command lifecycle
- Lifecycle note: Create-only parser surface in 0.1.0.
- Runtime note: Lifecycle remains create-only and requires closure for beta-hardening.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE ACCESS METHOD <name> TYPE INDEX HANDLER <handler_name>; |
| ALTER | Not available | -- No explicit ALTER ACCESS METHOD command in 0.1.0. |
| SHOW | Not available | -- No explicit SHOW ACCESS METHOD command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for ACCESS METHOD. |
| DROP | Not available | -- No explicit DROP ACCESS METHOD command in 0.1.0. |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
