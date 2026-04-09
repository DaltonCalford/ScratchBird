# Implementation Notes

Status: `Completed`

## Completed in this pass
- Finalized CAT-021 canonical cluster node/clock families with deterministic root/bootstrap/backfill wiring:
  - `node`
  - `node_role_binding`
  - `node_service`
  - `node_capability`
  - `clock_policy`
  - `clock_source`
  - `node_clock_state`
  - `clock_violation_event`
- Implemented/validated deterministic CRUD contracts for all node-family tables:
  - uniqueness constraints (`cluster_id+node_name`, `node_id+role`, `node_id+service_type+port`, `node_id+capability_key`)
  - enum validation for roles/states/service types/transports
  - soft-delete behavior
  - TOAST-backed capability value storage/retrieval
- Hardened clock-family contracts with dependent reference checks:
  - `clock_source` requires existing `clock_policy`
  - `node_clock_state` requires existing `node` and `clock_policy`
  - `clock_violation_event` requires existing `node` and `clock_policy`
- Added focused node-family contract test coverage:
  - `CatalogClusterClockExtensionContractTest.NodeCatalogContracts`
- Updated existing clock contract test to satisfy new dependent-reference requirements.

## Outcome
CAT-021 is closed with build + focused gate evidence recorded in this directory.
