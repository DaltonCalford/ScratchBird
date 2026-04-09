# Buffer Cache Observability

Status: current_authority
Section owner: `20_Diagnostics_Audit_and_Observability`

## Current authority matrix

| Surface | Current state | Authority |
| --- | --- | --- |
| current buffer SQL views | current authority | `ObservabilityContract` |
| current buffer metric families | current authority | `MgaObservabilityContract` |
| extra prose-only hotspot, frame-state, or snapshot-pin views | fail closed | not current audited runtime authority |

## Named current views

- `sb_buffer_writeback_debt`
- `sb_buffer_pool_stats`
- `sb_buffer_domain_stats`
- `sb_buffer_policy_health`
- `sb_buffer_prefetch_health`
- `sb_checkpoint_writeback_pressure`

## Canonical rule

Section `20` may claim only the current views and metric families above because
they are bound to `ObservabilityContract`, `MgaObservabilityContract`, and test
surfaces. Buffer-pool internals alone do not prove broader operator-facing
buffer observability breadth.
