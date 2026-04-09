# B1-04-001 Evidence Note

## Closure summary

Specification sufficiency for package `04` is complete.

This closure pass:
- fixed optimizer parity at the admitted named-family layer instead of allowing
  parity to stop at shared runtime backends
- promoted full persisted canonical family fields per index so named-family
  identity, lowering, lifecycle, queryability, and metrics contract are durable
  catalog truth rather than planner-only derivation
- promoted accelerator-capable named families into active Beta 1 scope and
  bound accelerator governance to extensions of the existing workload
  admission-policy and admission-binding rows
- fixed the container and cgroup resource-envelope rule so effective budgets are
  derived from the authoritative environment ceiling with environment-based
  defaults and clamped operator configuration
- updated the package tracker, risk log, and audit matrix so `B1-04-002` can
  start from explicit canon rather than inferred intent

## Canonical files updated

- `docs/specifications/18_Index_Framework/README.md`
- `docs/specifications/18_Index_Framework/INDEX_METRICS_AND_COSTING.md`
- `docs/specifications/18_Index_Framework/INDEX_RUNTIME_TAXONOMY_AND_ALIAS_LOWERING.md`
- `docs/specifications/18_Index_Framework/INDEX_CATALOG_AND_METADATA.md`
- `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/WORKLOAD_ROUTING_ADMISSION_SLO_AND_AUTOSCALE_CATALOG_MODEL.md`
- `docs/specifications/33_Memory_Management/CONTAINER_CGROUP_LIMITS_AND_RESOURCE_PRESSURE_MODEL.md`
- `docs/specifications/36_Query_Rewrite_and_Planner/PRIMARY_INDEX_FAMILY_PARITY_AND_METRICS_MANDATE.md`
- `docs/specifications/38_Workload_Governance_and_Parallelism/ACCELERATOR_ADMISSION_AND_RESOURCE_GOVERNANCE.md`
- `docs/specifications/38_Workload_Governance_and_Parallelism/WORKLOAD_CLASS_RESOLUTION_AND_ADMISSION_BINDING_MODEL.md`

## Verification

- local canonical and reference trees were read first
- no web research was required
- no tests were run because this ticket was specification and package-control
  work only

## Result

- later package tickets can now implement named-family parity, per-index
  canonical metadata persistence, accelerator governance, and environment-aware
  pressure handling without guessing where Beta 1 stops and Beta 2 begins
