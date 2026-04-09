# Canonical Gap Register

## NEQ-12-G01 Transactional Eventing Durable Queues And Notifications

- Status: open
- Current bounded proof:
  - `docs/specifications/work/audits/SCRATCHBIRD_SQLSERVER_AZURE_NATIVE_EQUIVALENT_ANALYSIS_2026-04-03/SQLSERVER_AZURE_NATIVE_EQUIVALENT_ANALYSIS.md`
    + `ScratchBird does not yet have a first-class transactional event bus or durable queue subsystem`
- Gap:
  - no native queue tables, activation workers, consumer offsets, publication
    envelopes, or notification-routing canon
- Closing tickets:
  - `NEQ-12-002`
  - `NEQ-12-003`

## NEQ-12-G02 Scheduled Jobs Alerting And Operator Messaging

- Status: open
- Current bounded proof:
  - native-equivalent report
    + `ScratchBird does not yet have a first-class database scheduler or notification sink family`
- Gap:
  - no native database scheduler, alert policy, retry model, or operator
    message sink canon
- Closing tickets:
  - `NEQ-12-004`
  - `NEQ-12-005`

## NEQ-12-G03 Managed Safe Extensibility Runtime

- Status: open
- Current bounded proof:
  - native-equivalent report
    + `lacks a safe managed runtime host`
- Gap:
  - no canonical `WASM/WASI` runtime, packaging rules, capability policy, or
    executor binding for managed extensions
- Closing tickets:
  - `NEQ-12-006`
  - `NEQ-12-007`

## NEQ-12-G04 Native Changefeed And Consumer Offset Model

- Status: open
- Current bounded proof:
  - native-equivalent report
    + `ScratchBird has migration and replay-oriented change capture canon, but not a native changefeed`
- Gap:
  - no commit-envelope changefeed, cursor slot, replay-safe offset, or
    projection canon
- Closing tickets:
  - `NEQ-12-008`
  - `NEQ-12-009`

## NEQ-12-G05 Relational Temporal Versioning And History Binding

- Status: partial
- Current bounded proof:
  - native-equivalent report
    + `temporal clauses, MGA lineage, archive, and replay canon`
- Gap:
  - no first-class relational temporal table model, history binding, or
    deterministic time-travel query contract
- Closing tickets:
  - `NEQ-12-010`
  - `NEQ-12-011`

## NEQ-12-G06 Tamper-Evident Ledger And Attestation

- Status: open
- Current bounded proof:
  - native-equivalent report
    + `not a real tamper-evident digest chain or attestation export`
- Gap:
  - no digest chain, attestation record, verifier flow, or audit proof export
- Closing tickets:
  - `NEQ-12-012`
  - `NEQ-12-013`

## NEQ-12-G07 Property Graph Storage And Pattern Matching

- Status: partial
- Current bounded proof:
  - `docs/specifications/17_Functions_and_Procedures/BETA2_GRAPH_SCIENCE_AND_NETWORK_ANALYSIS_UDR_MODEL.md`
    + `graphs may be transient execution artifacts or stored graph artifacts`
- Gap:
  - no graph catalog, edge storage overlay, graph identity model, or
    pattern-matching query surface
- Closing tickets:
  - `NEQ-12-014`
  - `NEQ-12-015`

## NEQ-12-G08 External Data Virtualization And Remote Federation Closure

- Status: partial_spec_only
- Current bounded proof:
  - native-equivalent report
    + `strong generic canon exists`
- Gap:
  - implementation-closure canon is missing for capability negotiation,
    pushdown classes, remote statistics, security policy, and operator workflow
- Closing tickets:
  - `NEQ-12-016`
  - `NEQ-12-017`

## NEQ-12-G09 Transactional Blob And File Namespace Tables

- Status: open
- Current bounded proof:
  - native-equivalent report
    + `does not yet define a native table-bound blob namespace`
- Gap:
  - no canonical namespace table family for paths, object bindings, metadata,
    transactional rename rules, or governed file exposure
- Closing tickets:
  - `NEQ-12-018`
  - `NEQ-12-019`

## NEQ-12-G10 Plan Store Baseline Forcing And Managed Tuning Closure

- Status: partial_spec_only
- Current bounded proof:
  - native-equivalent report
    + `main remaining work is runtime delivery and operator workflow`
- Gap:
  - no implementation-closure canon for plan baselines, forced-plan refusal,
    auto-correction workflow, or operator review loop
- Closing tickets:
  - `NEQ-12-020`
  - `NEQ-12-021`

## NEQ-12-G11 Service Tiers Tenant Pools And Workload Governance Control Plane

- Status: partial
- Current bounded proof:
  - native-equivalent report
    + `missing work is the operator-facing control plane`
- Gap:
  - no unified service-tier and tenant-pool control-plane canon over existing
    QoS and reservation substrate
- Closing tickets:
  - `NEQ-12-022`
  - `NEQ-12-023`

## NEQ-12-G12 Serverless Autosuspend Autoscale And Warm Resume

- Status: open
- Current bounded proof:
  - native-equivalent report
    + `not a complete autosuspend, resume, warm-start, and cost-policy model`
- Gap:
  - no serverless service-tier canon for suspend thresholds, cache retention,
    resume policy, and operator refusal boundaries
- Closing tickets:
  - `NEQ-12-024`
  - `NEQ-12-025`

## NEQ-12-G13 Replicated Topology Read Scale-Out And Geo Failover

- Status: partial
- Current bounded proof:
  - native-equivalent report
    + `combined read-scale-out and geo-topology family is still incomplete`
- Gap:
  - no integrated replicated-topology canon covering read routing, region
    placement, geo failover, and active-secondary posture
- Closing tickets:
  - `NEQ-12-026`
  - `NEQ-12-027`

## NEQ-12-G14 Hot-Row Memory-Optimized OLTP Lane And Compiled Kernels

- Status: partial
- Current bounded proof:
  - native-equivalent report
    + `not a clearly admitted memory-optimized row family or compiled OLTP kernel contract`
- Gap:
  - no native memory-optimized row family, hot-row admission, or compiled fast
    path canon
- Closing tickets:
  - `NEQ-12-028`
  - `NEQ-12-029`

## NEQ-12-G15 Distributed Atomic Coordination And Prepared Branches

- Status: open
- Current bounded proof:
  - native-equivalent report
    + `does not yet have a canonical distributed-transaction family`
- Gap:
  - no distributed atomic coordination canon for prepared branches,
    coordinator failover, or compensation boundaries
- Closing tickets:
  - `NEQ-12-030`
  - `NEQ-12-031`

## NEQ-12-G16 Enterprise Identity Federation And Token Authentication

- Status: partial
- Current bounded proof:
  - native-equivalent report
    + `remaining native work is broader identity federation`
- Gap:
  - no first-class token auth, claim mapping, external principal binding, or
    federated recovery canon
- Closing tickets:
  - `NEQ-12-032`
  - `NEQ-12-033`

## NEQ-12-G17 Transparent At-Rest Encryption Implementation Closure

- Status: partial_spec_only
- Current bounded proof:
  - `docs/specifications/19_Security_Model/BETA2_TRANSPARENT_AT_REST_ENCRYPTION_AND_REKEY_MODEL.md`
- Gap:
  - current canon needs implementation-closure guidance, examples, worker flows,
    and cross-section operational steps
- Closing tickets:
  - `NEQ-12-034`
  - `NEQ-12-035`

## NEQ-12-G18 Protected-Query Encryption And Enclave Implementation Closure

- Status: partial_spec_only
- Current bounded proof:
  - `docs/specifications/19_Security_Model/BETA2_PROTECTED_QUERY_ENCRYPTION_AND_ENCLAVE_EXECUTION_MODEL.md`
- Gap:
  - current canon needs implementation-closure guidance, examples, attestation
    flow, and verifier/runtime integration detail
- Closing tickets:
  - `NEQ-12-036`
  - `NEQ-12-037`

## NEQ-12-G19 Row Security And Dynamic Masking Implementation Closure

- Status: partial_runtime
- Current bounded proof:
  - native-equivalent report
    + `native families already exist and have partial runtime substrate`
- Gap:
  - no implementation-closure canon for policy ordering, caching, refusal
    cases, and mixed masking/RLS interactions
- Closing tickets:
  - `NEQ-12-038`
  - `NEQ-12-039`

## NEQ-12-G20 Analytical Columnstore And OLAP Acceleration Implementation Closure

- Status: partial_runtime
- Current bounded proof:
  - native-equivalent report
    + `remaining gap is delivery quality, not feature invention`
- Gap:
  - no implementation-closure canon for segment lifecycle, benchmark targets,
    hybrid OLTP/OLAP isolation, and operator-visible maintenance flow
- Closing tickets:
  - `NEQ-12-040`
  - `NEQ-12-041`

## Out-Of-Scope Donor Rows

The following rows are frozen as outside this package:

- Microsoft wire protocol and parser family
- Microsoft catalog, DMV, and system-procedure compatibility overlays
- Microsoft login-object and donor token semantics

## Bounded Scope Rule

Only the twenty gaps above are in direct closure scope for this package.

If research uncovers a prerequisite not already listed here:

- record it in `RISK_DECISION_LOG.md`
- tie it to the affected ticket
- do not silently expand the package into donor-only or unrelated feature work
