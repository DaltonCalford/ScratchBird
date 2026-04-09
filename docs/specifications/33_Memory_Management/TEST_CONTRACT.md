# Section 33 Test Contract

Section `33` is implementation-ready only if maintained evidence covers the
current-memory behaviors it claims.

## Required certification lanes

- budget tree and schema quotas
  - process, domain, database, schema-root, statement, and task charging stays
    within the documented parent-child rules
  - breaker transitions and emergency-reserve rules behave as documented
- allocator ownership boundaries
  - allocations and frees occur through the documented owning subsystems
  - cross-owner free or lifetime violations fail closed or are rejected
- typed allocators and contexts
  - arena, slab, page-backed, and code-heap allocations follow the documented
    class-selection rules
  - statement failure and transaction rollover reclaim the documented contexts
- buffered runtime memory behavior
  - buffer or cache memory accounting stays within documented ownership rules
  - buffer pressure does not silently bypass documented admission or eviction
- temporary and spill boundaries
  - spill transitions are deterministic and bounded by the documented runtime
    controls
  - temporary-memory exhaustion produces deterministic refusal or degradation
    behavior
- pressure and admission
  - pressure signals trigger the documented admission, backpressure, or refusal
    paths
  - unsupported overcommit behavior is not claimed as supported
- operator reservation and grant feedback
  - operator grants follow the documented identity and reservation flow
  - feedback rows update deterministically and respect oscillation-disable rules
- tenant reservation and enforced budget
  - tenant reservation, ceiling, and spill transitions follow the documented
    breaker states
  - one tenant may not silently consume another tenant's reserved memory
- JIT code memory
  - compile scratch, metadata, and executable code pages remain in the
    documented domains
  - retired published code is reclaimable only through the documented tracker
    flow
- observability
  - documented counters, diagnostics, or debug surfaces are emitted when memory
    pressure or failure paths occur
  - live memory-context views expose node-level limits and usage as documented

## Negative requirements

- no section `33` test may assume universal NUMA, cgroup, or platform-specific
  memory-governor behavior unless this section says so explicitly
- no test may treat fail-closed exclusions as supported runtime guarantees
