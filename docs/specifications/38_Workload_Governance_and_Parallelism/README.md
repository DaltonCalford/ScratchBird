# Section 38 Workload Governance and Parallelism

Status: current_authority

This section owns the canonical ScratchBird workload-governance and parallelism
model: owner boundaries, admission and scheduling, worker and parallel
execution scope, resource isolation, and operator controls.

Where current runtime support is narrower than a mature scheduler or
parallel-query platform, that narrower scope must be expressed as explicit
fail-closed or non-guarantee language inside the section files. This section
entry point is not a placeholder.

## Section scope

- workload governance scope and owner model
- priority, admission, and scheduling boundary
- parallelism scope and worker model
- resource isolation and fairness boundary
- operator controls and observability boundary

## Audit lookup anchors

Representative section-38 audit anchors are:
- `WorkloadGovernance::resolveWorkloadClass(`
- `AdmissionLease`

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [ACCELERATOR_ADMISSION_AND_RESOURCE_GOVERNANCE.md](ACCELERATOR_ADMISSION_AND_RESOURCE_GOVERNANCE.md)
- [AUTOSCALE_BURN_TRIGGER_AND_HEALING_ACTION_RUNTIME_MODEL.md](AUTOSCALE_BURN_TRIGGER_AND_HEALING_ACTION_RUNTIME_MODEL.md)
- [BETA2_HARD_MULTI_TENANT_ISOLATION_QUOTA_AND_QOS_MODEL.md](BETA2_HARD_MULTI_TENANT_ISOLATION_QUOTA_AND_QOS_MODEL.md)
- [BETA2_SERVICE_TIERS_TENANT_POOLS_AND_WORKLOAD_GOVERNANCE_MODEL.md](BETA2_SERVICE_TIERS_TENANT_POOLS_AND_WORKLOAD_GOVERNANCE_MODEL.md)
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [MAINTENANCE_DEBT_LEDGER_AND_SCHEDULING_MODEL.md](MAINTENANCE_DEBT_LEDGER_AND_SCHEDULING_MODEL.md)
- [OPERATOR_CONTROLS_AND_OBSERVABILITY_BOUNDARY.md](OPERATOR_CONTROLS_AND_OBSERVABILITY_BOUNDARY.md)
- [PARALLELISM_SCOPE_AND_WORKER_MODEL.md](PARALLELISM_SCOPE_AND_WORKER_MODEL.md)
- [PRIORITY_ADMISSION_AND_SCHEDULING_BOUNDARY.md](PRIORITY_ADMISSION_AND_SCHEDULING_BOUNDARY.md)
- [RESOURCE_ISOLATION_AND_FAIRNESS_BOUNDARY.md](RESOURCE_ISOLATION_AND_FAIRNESS_BOUNDARY.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
- [WORKLOAD_CLASS_RESOLUTION_AND_ADMISSION_BINDING_MODEL.md](WORKLOAD_CLASS_RESOLUTION_AND_ADMISSION_BINDING_MODEL.md)
- [WORKLOAD_GOVERNANCE_ADMISSION_ROUTING_SLO_AND_ERROR_BUDGET_RUNTIME_MODEL.md](WORKLOAD_GOVERNANCE_ADMISSION_ROUTING_SLO_AND_ERROR_BUDGET_RUNTIME_MODEL.md)
- [WORKLOAD_GOVERNANCE_SCOPE_AND_OWNER_MODEL.md](WORKLOAD_GOVERNANCE_SCOPE_AND_OWNER_MODEL.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
