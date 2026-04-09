# B1-04-004 Evidence Note

## Closure summary

Optimizer workload governance and accelerator closure for package `04` is
complete.

This ticket:
- promoted all lane-B audit rows in the package matrix from `partial` to
  `implemented`
- preserved canonical named-family metrics identity across shared-backend
  lowerings in the statistics and planner paths
- kept compatible sibling families visible as distinct primary planner
  candidates instead of collapsing them to one shared-runtime winner
- extended the existing admission-policy and admission-binding catalog rows
  with accelerator profile, budget, and device-affinity state
- enforced accelerator search, build, and memory limits with CPU fallback and
  statement-scoped accelerator leases
- recorded environment-clamped effective buffer budgets during database open

## Recorded proof artifacts

- `catalog_memory_contracts.log`
  - focused catalog-extension and environment-clamp proof
  - `CatalogRoutingAdmissionExtensionContractTest.RoutingAndAdmissionCatalogContracts`
  - `HotPathRuntimeFixture.DatabaseOpenClampsExplicitBufferPoolToDetectedMemoryCeiling`
  - 2 tests passed
- `accelerator_governance_contracts.log`
  - focused accelerator-governance SQL and runtime proof
  - `WorkloadGovernanceTest.CompilesAndExecutesAcceleratorGovernanceSql`
  - `WorkloadGovernanceTest.AcceleratorAdmissionUsesLimitsAndTracksFallbacks`
  - 2 tests passed
- `planner_metrics_contracts.log`
  - focused named-family parity and metrics proof
  - `StatisticsManagerRuntimeTest.RefreshIndexFamilyMetricsPublishesCanonicalNamedFamilyIdentity`
  - `QueryPlannerIntegrationTest.SharedLoweringSiblingFamiliesRemainDistinctInRuntimePlanIdentities`
  - 2 tests passed

## Canonical files updated

- `docs/specifications/18_Index_Framework/README.md`
- `docs/specifications/18_Index_Framework/INDEX_METRICS_AND_COSTING.md`
- `docs/specifications/18_Index_Framework/RUNTIME_FAMILY_ALIAS_AND_OPTIMIZER_METRICS_BINDING_MODEL.md`
- `docs/specifications/18_Index_Framework/TEST_CONTRACT.md`
- `docs/specifications/33_Memory_Management/CONTAINER_CGROUP_LIMITS_AND_RESOURCE_PRESSURE_MODEL.md`
- `docs/specifications/36_Query_Rewrite_and_Planner/PRIMARY_INDEX_FAMILY_PARITY_AND_METRICS_MANDATE.md`
- `docs/specifications/36_Query_Rewrite_and_Planner/IMPLEMENTED_CANDIDATE_BUNDLE_AND_NO_IGNORED_INDEX_RUNTIME_MODEL.md`
- `docs/specifications/38_Workload_Governance_and_Parallelism/ACCELERATOR_ADMISSION_AND_RESOURCE_GOVERNANCE.md`
- `docs/specifications/38_Workload_Governance_and_Parallelism/WORKLOAD_CLASS_RESOLUTION_AND_ADMISSION_BINDING_MODEL.md`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/README.md`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/MASTER_TRACKER.md`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/MASTER_TRACKER.csv`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/ORDERED_TASK_TICKETS.csv`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/BOUNDED_TICKET_SET.md`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/CANONICAL_GAP_REGISTER.md`
- `docs/work-plans/04-Access_Methods_Indexes_Optimizer_Memory/RISK_DECISION_LOG.md`

## Verification

- no web research was required
- recorded lane-B proof logs passed without failures on March 30, 2026
- the focused contract surface for this ticket is 6 passing tests across 3
  preserved artifacts

## Result

- `B1-04-004` is complete
- `B1-04-005` is now the active ticket for gates, benchmarks, and section `31`
  evidence closure
