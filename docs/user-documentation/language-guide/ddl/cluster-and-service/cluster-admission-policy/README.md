# DDL Object: CLUSTER ADMISSION POLICY
Last modified: 2026-02-21

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Cluster And Service README](../README.md)

## Summary
- Family: Cluster And Service
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Cluster admission policy has full create/alter/show/drop command families.
- Runtime note: Runtime semantics for cluster command families are still bridge-partial in 0.1.0.

## Lifecycle Documents
- [CREATE](create.md)
- [ALTER](alter.md)
- [SHOW](show.md)
- [DESCRIBE](describe.md)
- [DROP](drop.md)

## 0.1.0 Coverage Matrix
| Phase | Status | Primary parser form |
|---|---|---|
| CREATE | Supported | CREATE CLUSTER ADMISSION POLICY <name> CONFIG '<json_spec>'; |
| ALTER | Supported | ALTER CLUSTER ADMISSION POLICY <name> CONFIG '<json_spec>'; |
| SHOW | Supported | SHOW CLUSTER ADMISSION STATUS; |
| DESCRIBE | Not available | -- No DESCRIBE variant for CLUSTER ADMISSION POLICY. |
| DROP | Supported | DROP CLUSTER ADMISSION POLICY <name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
