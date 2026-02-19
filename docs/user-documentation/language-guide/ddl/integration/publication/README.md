# DDL Object: PUBLICATION
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Integration README](../README.md)

## Summary
- Family: Integration
- Lifecycle status: Partial command lifecycle
- Lifecycle note: Create/drop only in parser command surface.
- Runtime note: Publication lifecycle is partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE PUBLICATION <publication_name> FOR TABLE <table_name>; |
| ALTER | Not available | -- No explicit ALTER PUBLICATION command in 0.1.0. |
| SHOW | Not available | -- No explicit SHOW PUBLICATION command in 0.1.0. |
| DESCRIBE | Not available | -- No DESCRIBE variant for PUBLICATION. |
| DROP | Supported | DROP PUBLICATION <publication_name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
