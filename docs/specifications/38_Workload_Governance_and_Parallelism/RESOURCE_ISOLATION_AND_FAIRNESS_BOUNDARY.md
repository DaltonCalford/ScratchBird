# Resource Isolation and Fairness Boundary

This file owns the bounded resource-isolation model.

## Isolation matrix

| Topic | Current state | Current truth | Explicit exclusion |
| --- | --- | --- | --- |
| per-session isolation controls | partial | runtime surfaces may bound work by session or request in limited form | not hard multi-tenant isolation |
| CPU or memory fairness | fail_closed | no complete fairness contract is claimed across all workloads | not QoS certification |
| maintenance-vs-user work separation | partial | bounded separation may exist where current worker or scheduler surfaces expose it | not strict resource partitioning |
| operator-visible limits | partial | operator controls may surface narrow limits and observations | not a comprehensive resource console |

## Canonical rules

1. Isolation claims must stay bounded to current local runtime controls.
2. Fairness terminology must remain narrower than enterprise QoS language.
3. Hard partitioning remains fail-closed unless directly proven.

## Explicit non-guarantees

- no hard tenant isolation guarantee
- no complete fairness scheduler guarantee
- no full quota and reservation system
