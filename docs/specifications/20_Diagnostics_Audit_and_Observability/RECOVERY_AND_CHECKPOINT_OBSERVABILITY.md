# Recovery And Checkpoint Observability

Status: current_authority
Section owner: `20_Diagnostics_Audit_and_Observability`

## Current authority matrix

| Surface | Current state | Authority |
| --- | --- | --- |
| checkpoint history and status views | current authority | `ObservabilityContract` |
| recovery status and incident views | current authority | `ObservabilityContract` |
| writeback incident and debt views | current authority | `ObservabilityContract` |
| linked runtime metrics | current authority | `MgaObservabilityContract` |
| extra prose-only alerting or workflow breadth | fail closed | not independently audited runtime authority |

## Named current views

- `sb_checkpoint_history`
- `sb_checkpoint_status`
- `sb_recovery_status`
- `sb_recovery_incidents`
- `sb_writeback_incidents`
- `sb_buffer_writeback_debt`
- `sb_checkpoint_writeback_pressure`

## Canonical rule

Section `20` may claim the checkpoint, recovery, writeback, and sweep-resume
surfaces above because they are bound to `ObservabilityContract`,
`MgaObservabilityContract`, catalog registration, and test coverage. It must
not widen that authority into undocumented alerting or workflow subsystems.
