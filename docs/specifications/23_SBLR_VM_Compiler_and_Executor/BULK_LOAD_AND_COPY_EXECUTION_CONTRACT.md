# Bulk Load and Copy Execution Contract

Status: target_state_only

Section `23` does not currently own a fully audited bulk-load or COPY execution contract.

## Capability states

- `planner_executor_future_integration`: `target_state_only`
- `dedicated_bulk_load_runtime`: `unproven`
- `dedicated_copy_runtime`: `unproven`

Current bounded truth:
- optimizer and executor integration may eventually consume bulk-load or copy plans
- no section-23-local proof in the audited sources closes a dedicated bulk-load execution model here

Any future promotion of this file requires direct proof from live bulk-load or copy runtime sources and tests.
