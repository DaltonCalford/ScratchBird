# Accelerator Admission and Resource Governance

## Purpose

This document defines the required ScratchBird policy for GPU and other
accelerator resources when available.

The governing rule is:
- accelerator resources are optional execution aids
- MGA database state remains authoritative
- device memory, warmup, and fallback behavior are governed resources, not
  hidden implementation details

## Current code-backed authority

The current code-backed authority already proves the following surfaces:
- index types include `GPU_CAGRA` and multiple ANN families
- parser and SBLR paths recognize `GPU_CAGRA`
- `CREATE INDEX` validation already accepts accelerator-family option sets
- vector-family index types are routed through the ANN runtime family in the
  planner and statistics subsystems
- workload governance already supports class resolution, admission policies,
  queueing, and resource-tag matching
- admission-policy and admission-binding rows now persist accelerator profile,
  budget, prewarm, fallback, degraded-state, and device-affinity fields
- workload governance now enforces accelerator search and build concurrency,
  memory budgets, CPU fallback, and statement-scoped accelerator leases
- SQL governance surfaces now accept accelerator policy and binding fields and
  publish accelerator status through `SHOW CLUSTER ADMISSION STATUS`
- buffer and residency policies already exist for MGA pages in host memory

Current code-backed accelerator-related index options include:
- vector families:
  - `METRIC`
  - `VECTOR_DIM`
  - `M`
  - `EF_CONSTRUCTION`
  - `EF_SEARCH`
  - `NLIST`
  - `NPROBE`
  - `PQ_M`
  - `PQ_BITS`
  - `BITS_PER_CODE`
  - `SQ_BITS`
  - `GPU_SEARCH_THRESHOLD`
- `GPU_CAGRA`:
  - `GRAPH_DEGREE`
  - `INTERMEDIATE_GRAPH_DEGREE`
  - `BUILD_ITERS`
  - `METRIC`
  - `VECTOR_DIM`
  - `GPU_ID`
  - `SEED`
  - `ENTRYPOINT_STRATEGY`
  - `FALLBACK_CPU`

## Beta 1 family activation rule

Beta 1 requires accelerator-capable and ANN named families to be admitted
create-time surfaces rather than parse-only placeholders.

This includes `GPU_CAGRA` plus accelerator-aware vector families such as
`SCANN`, `DISKANN`, `ANNOY`, and `NSG` when they share ANN runtime substrates.

These families may share CPU-resident or accelerator-backed runtime backends
and may use CPU fallback where the family contract allows it, but they remain
active families for:
- catalog identity
- planner admission
- metrics publication
- workload governance
- fallback and degraded-state reporting

Parser-only recognition without admissible runtime and governance closure is
non-conforming.

## Canonical accelerator resource classes

ScratchBird shall classify accelerator requests into these canonical resource
classes:
- `ANN_SEARCH_DEVICE`
- `ANN_BUILD_DEVICE`
- `VECTOR_PREWARM_IO`
- `VECTOR_RESIDENCY_MEMORY`
- `JIT_ACCELERATOR_COMPILE`
- `AUXILIARY_BATCH_ACCELERATOR`

The first three classes are required immediately for vector and ANN families.

## Accelerator admission model

Accelerator admission shall occur after ordinary workload-class resolution and
before runtime execution begins.

The device admission algorithm is:
1. resolve the statement workload class and admission policy
2. determine whether the chosen access path requests accelerator execution
3. determine the required execution posture:
   - `NONE`
   - `PREFERRED`
   - `REQUIRED`
4. resolve candidate device set
5. reject any device whose capability profile does not satisfy the family
   contract
6. reject any device whose reserved memory or active-search budget would be
   exceeded
7. select a device according to policy:
   - explicit `GPU_ID` hint first when legal
   - else least-loaded eligible device
   - else policy-default pool route
8. if no device is admissible:
   - use CPU fallback only when the family contract allows it
   - otherwise reject the statement before execution begins
9. issue an accelerator admission lease bound to the statement execution scope
10. release the lease on statement completion or failure

## Required device capability fields

Each discovered device shall expose at minimum:
- `device_kind`
- `device_id`
- `driver_identity`
- `driver_version`
- `runtime_identity`
- `runtime_version`
- `total_memory_bytes`
- `reserved_memory_bytes`
- `free_memory_bytes`
- `supports_ann_search`
- `supports_ann_build`
- `supports_jit_offload`
- `health_state`
- `degraded_reason`
- `last_heartbeat_ms`

## Residency rule for vector and similar index classes

Vector and accelerator-backed ANN index classes that are marked resident shall
follow this model:
1. the durable canonical index image remains in MGA storage
2. on first successful use, the runtime loads the usable search structure from
   the durable index image into memory or accelerator memory
3. the runtime keeps the current search structure resident while:
   - policy budgets allow it
   - device health is acceptable
   - the index has not been explicitly invalidated or evicted
4. all logical changes continue to become durable through the authoritative MGA
   index and table state
5. in-memory or device-resident structures are refreshed or flushed from the
   durable delta stream according to the family contract
6. loss of a resident copy may degrade latency, but it may not lose committed
   truth

For resident families, eviction is a governance decision, not ordinary cache
accident.

## Write and flush model

Accelerator and resident-vector families shall obey this ordering rule:
1. table and index logical changes become durable in authoritative MGA storage
2. any derivative resident graph or device structure is refreshed after the
   durable update boundary
3. any derivative export or remote accelerator refresh happens after local
   durable publication

A device-resident structure may never become the only authoritative copy of an
index.

## Fallback model

`FALLBACK_CPU` and equivalent family settings shall be interpreted as follows:
- `true`:
  - the planner and runtime may execute the same logical access path through a
    CPU-capable family implementation when accelerator admission fails
- `false`:
  - accelerator unavailability is a hard refusal for that access path

Fallback must preserve the same semantic result set. It may change only latency,
throughput, or plan ranking.

## Device budgeting

Every accelerator policy shall include at least:
- `max_concurrent_searches`
- `max_concurrent_builds`
- `max_reserved_memory_bytes`
- `max_resident_indexes`
- `prewarm_limit`
- `degraded_reject_threshold_pct`
- `queue_depth_limit`
- `queue_timeout_ms`

The governor shall track separately:
- active searches
- active builds
- queued searches
- queued builds
- resident index count
- resident bytes
- forced fallbacks
- admission rejections

## Warmup and prewarm

Accelerator prewarm is required reconstructed behavior.

A prewarm policy may be:
- `NONE`
- `ON_FIRST_USE`
- `STARTUP_HOTSET`
- `MANUAL_ONLY`

For vector and ANN families, `ON_FIRST_USE` is the minimum required behavior.
`STARTUP_HOTSET` is recommended for explicitly pinned resident indexes.

## Interaction with ordinary workload governance

Accelerator governance is layered onto the existing workload governance model.
It shall preserve:
- workload-class matching
- binding priority rules
- queue timeout rules
- admission-tuning history
- SLO and error-budget inspection

The accelerator lane adds resource-specific admission only.
It extends the existing admission-policy and admission-binding rows rather than
creating a parallel governance catalog.

## Required operator outputs

The runtime shall expose at least these accelerator-governance outputs:
- device status
- device memory pressure
- active and queued admissions
- resident index inventory
- warmup state per resident index
- fallback counts
- accelerator rejection reasons
- degraded or quarantined device state

## Improvement candidates captured during recovery

The rebuild identifies the following improvement candidates that must remain
tracked even when not yet implemented:
- device-pool aware plan ranking instead of binary accelerator/no-accelerator
  ranking
- admission-tuning feedback that incorporates device memory pressure and
  fallback frequency
- explicit pinned hotset configuration for always-resident vector indexes
- prewarm jobs scheduled through the existing job scheduler and workload
  governance lanes

## Audit lookup anchors

Representative audit anchors for this file are:
- `WorkloadGovernance::resolveWorkloadClass(`
- `AdmissionLease`
- `snapshotAdmissionStatus(`
