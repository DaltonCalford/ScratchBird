# Engine Resource Governance and Budgets

Status: current_authority

## Current authority

Current code-backed authority is the workload-governance surface that carries routing, admission, SLO, error-budget, autoscale-policy metadata, and local policy evaluation state.

## Current guarantees

- governance metadata exists and is implementation-relevant now
- routing/admission decisions are bounded by current governance state and local runtime consumers
- budgets, queues, and overload outcomes are current bounded runtime concerns

## Bounded current substrate

- this file is subordinate to ENGINE_RESOURCE_GOVERNANCE_AND_ENVELOPE_MODEL.md for whole-engine budget composition
- autonomous cluster-wide scaling and governance automation parity
