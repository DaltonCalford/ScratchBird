# DDL Object: CLUSTER WORKLOAD ROUTE
Last modified: 2026-02-19

Back links:
- [Language Guide README](../../../README.md)
- [DDL README](../../README.md)
- [Cluster And Service README](../README.md)

## Summary
- Family: Cluster And Service
- Lifecycle status: Complete command lifecycle
- Lifecycle note: Cluster workload route has full create/alter/show/drop command families.
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
| CREATE | Supported | CREATE CLUSTER WORKLOAD ROUTE <name> CONFIG '<json_spec>'; |
| ALTER | Supported | ALTER CLUSTER WORKLOAD ROUTE <name> CONFIG '<json_spec>'; |
| SHOW | Supported | SHOW CLUSTER ROUTING PLAN; CLUSTER SHOW ROUTING PLAN; |
| DESCRIBE | Not available | -- No DESCRIBE variant for CLUSTER WORKLOAD ROUTE. |
| DROP | Supported | DROP CLUSTER WORKLOAD ROUTE <name>; |

## Next In Series
- Start lifecycle walkthrough at [CREATE](create.md).
