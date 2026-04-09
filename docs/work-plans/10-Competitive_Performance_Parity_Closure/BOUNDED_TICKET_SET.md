# Bounded Ticket Set

## `PP-10-001` Scope, target freeze, and benchmark discipline bootstrap

- Objective:
  - freeze the exact process list, table-load list, donor ceilings, and ticket
    ownership for this package
- Deliverables:
  - package docs
  - process and load trackers
  - donor fast-path tracker
- Entry criteria:
  - audit and benchmark artifacts are available
- Exit criteria:
  - all package trackers are complete and internally consistent

## `PP-10-002` Reproducible benchmark runner and binary pinning closure

- Objective:
  - ensure every benchmark run is tied to one concrete binary and config set
- Deliverables:
  - pinned binary artifact rules
  - runner metadata capture
  - comparison refusal for unpinned runs
- Entry criteria:
  - `PP-10-001`
- Exit criteria:
  - benchmark artifacts prove pinned binaries and frozen config

## `PP-10-003` Ordinary multi-row insert and client-batched `VALUES` bulk reuse

- Objective:
  - remove row-at-a-time insert setup from ordinary batched insert paths
- Deliverables:
  - statement-local insert bulk path
  - one row-layout resolution per admitted batch
  - one maintenance posture resolution per admitted batch
- Entry criteria:
  - `PP-10-001`
  - `PP-10-002`
- Exit criteria:
  - load-path evidence proves server-side bulk reuse

## `PP-10-004` `INSERT ... SELECT` producer and sink fusion

- Objective:
  - make large set-sourced insert land on the real sink bulk lane
- Deliverables:
  - producer-to-sink batch contract
  - sink lane selection for set-sourced insert
  - preserved correctness under spill and rollback
- Entry criteria:
  - `PP-10-003`
- Exit criteria:
  - `bulk_insert_select` evidence proves a non-row-at-a-time sink path

## `PP-10-005` Heap multi-insert, page reservation, and filespace preallocation

- Objective:
  - amortize heap and filespace work across the batch instead of per row
- Deliverables:
  - page-coalesced heap multi-insert
  - ahead-of-demand preallocation
  - no repeated avoidable growth syscalls in hot loops
- Entry criteria:
  - `PP-10-003`
- Exit criteria:
  - table-level load evidence, especially `order_items`, materially improves

## `PP-10-006` HOT-like update and unchanged-key DML elision

- Objective:
  - make non-indexed updates cheap again
- Deliverables:
  - unchanged-key proof
  - heap-only or stable-head-preserving path
  - no repeated index metadata scans on admitted updates
- Entry criteria:
  - `PP-10-001`
  - `PP-10-002`
- Exit criteria:
  - `bulk_update_with_case` evidence returns to donor-competitive shape

## `PP-10-007` Exact-secondary maintenance batching and deferred merge

- Objective:
  - eliminate per-row exact-secondary maintenance overhead where canonical
    semantics permit batching or deferral
- Deliverables:
  - statement-local metadata hoist
  - batched apply
  - deferred merge
  - cleanup debt routing and evidence
- Entry criteria:
  - `PP-10-003`
  - `PP-10-004`
  - `PP-10-005`
  - `PP-10-006`
- Exit criteria:
  - write-path evidence shows batch or deferred maintenance instead of
    row-at-a-time maintenance

## `PP-10-008` Prepared-query fast-path bundles, plan reuse, and result-cache coordination

- Objective:
  - make prepared reads and point DML reuse the right artifacts with explicit
    invalidation and cache boundaries
- Deliverables:
  - prepared bundle build and hit path
  - prepared bundle invalidation
  - prepared query result-cache coordination
  - prepared DML no-result-cache proof
- Entry criteria:
  - `PP-10-001`
  - `PP-10-002`
- Exit criteria:
  - prepared fast-path evidence is complete for point select, point update, and
    micro-batch insert

## `PP-10-009` Ordered-exact secondary-read locality: `ICP`, `MRR`, index-only

- Objective:
  - close donor parity for secondary-read locality and heap-touch avoidance
- Deliverables:
  - `ICP`
  - `MRR`
  - bounded index-only execution
- Entry criteria:
  - `PP-10-001`
  - `PP-10-002`
- Exit criteria:
  - point and range read evidence proves the specialized path families

## `PP-10-010` Indexed-join parity: `BKA`, memoize, runtime filters, adaptive build-side

- Objective:
  - close donor parity for indexed and hash join specialization
- Deliverables:
  - `BKA`
  - memoize
  - runtime filters
  - bounded adaptive hash build-side selection
- Entry criteria:
  - `PP-10-008`
  - `PP-10-009`
- Exit criteria:
  - join-heavy benchmarks materially improve and specialized runtime plans are
    preserved

## `PP-10-011` Structured-key hash, spill, and low-churn sort closure

- Objective:
  - remove stringified-key and spill-heavy generic behavior from dominant hash
    and sort paths
- Deliverables:
  - structured-key hash runtime
  - spill-safe workfile paths
  - low-churn sort runs
- Entry criteria:
  - `PP-10-001`
  - `PP-10-002`
- Exit criteria:
  - spill and sort evidence proves the new runtime dominates the old generic
    path

## `PP-10-012` Incremental sort, aggregate, distinct, and window specialization/vectorization

- Objective:
  - close donor parity for upper-stage operator specialization and vectorization
- Deliverables:
  - incremental sort
  - vectorized aggregate and distinct
  - partition-aware vectorized window execution
- Entry criteria:
  - `PP-10-010`
  - `PP-10-011`
- Exit criteria:
  - aggregate, distinct, and window benchmarks materially improve

## `PP-10-013` Intra-query parallel execution, worker grants, and locality binding

- Objective:
  - close donor parity where donors win through legal multi-worker execution
- Deliverables:
  - executable parallel candidate families
  - worker-aware grant binding
  - locality contract and bounded work stealing
- Entry criteria:
  - `PP-10-009`
  - `PP-10-010`
  - `PP-10-011`
  - `PP-10-012`
- Exit criteria:
  - large benchmark operators prove legal parallel execution and explicit
    locality binding or refusal

## `PP-10-014` Final parity reruns, residual waiver review, and closeout

- Objective:
  - prove package completion with fresh pinned donor-comparable evidence
- Deliverables:
  - clean rerun artifacts
  - tracker completion
  - residual waiver log if any
- Entry criteria:
  - all previous tickets complete
- Exit criteria:
  - every tracker row is `met` or `waived`
