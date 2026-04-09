# Master Tracker

## Phase 0

- `PP-10-001` Scope, target freeze, and benchmark discipline bootstrap
  - Status: active
  - Depends on: none
- `PP-10-002` Reproducible benchmark runner and binary pinning closure
  - Status: queued
  - Depends on: `PP-10-001`

## Phase 1

- `PP-10-003` Ordinary multi-row insert and client-batched `VALUES` bulk reuse
  - Status: active
  - Depends on: `PP-10-001|PP-10-002`
- `PP-10-004` `INSERT ... SELECT` producer and sink fusion
  - Status: active
  - Depends on: `PP-10-003`
- `PP-10-005` Heap multi-insert, page reservation, and filespace preallocation
  - Status: active
  - Depends on: `PP-10-003`
- `PP-10-006` HOT-like update and unchanged-key DML elision
  - Status: active
  - Depends on: `PP-10-001|PP-10-002`
- `PP-10-007` Exact-secondary maintenance batching and deferred merge
  - Status: active
  - Depends on: `PP-10-003|PP-10-004|PP-10-005|PP-10-006`

## Phase 2

- `PP-10-008` Prepared-query fast-path bundles, plan reuse, and result-cache coordination
  - Status: completed
  - Depends on: `PP-10-001|PP-10-002`

## Phase 3

- `PP-10-009` Ordered-exact secondary-read locality: `ICP`, `MRR`, index-only
  - Status: completed
  - Depends on: `PP-10-001|PP-10-002`
- `PP-10-010` Indexed-join parity: `BKA`, memoize, runtime filters, adaptive build-side
  - Status: active
  - Depends on: `PP-10-008|PP-10-009`

## Phase 4

- `PP-10-011` Structured-key hash, spill, and low-churn sort closure
  - Status: completed
  - Depends on: `PP-10-001|PP-10-002`
- `PP-10-012` Incremental sort, aggregate, distinct, and window specialization/vectorization
  - Status: active
  - Depends on: `PP-10-010|PP-10-011`

## Phase 5

- `PP-10-013` Intra-query parallel execution, worker grants, and locality binding
  - Status: active
  - Depends on: `PP-10-009|PP-10-010|PP-10-011|PP-10-012`

## Phase 6

- `PP-10-014` Final parity reruns, residual waiver review, and closeout
  - Status: queued
  - Depends on: `PP-10-002|PP-10-007|PP-10-008|PP-10-009|PP-10-010|PP-10-011|PP-10-012|PP-10-013`
