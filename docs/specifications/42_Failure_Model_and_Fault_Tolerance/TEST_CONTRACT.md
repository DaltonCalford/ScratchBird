# Section 42 Test Contract

## Status

- Specification status: current_authority
- Last code-audit date: 2026-03-30

## Current implementation-backed proof

- `tests/unit/test_executor_transaction_payload.cpp` proves writeback incident
  persistence, reopen fence reload, incident history append or close behavior,
  and recovery-incident catalog history
- `tests/unit/test_transaction_manager.cpp` proves begin and rollback fail
  closed when write admission is fenced
- `tests/unit/test_mga_failpoint_replay.cpp` proves crash windows prefer
  durable transaction or page truth over speculative client-visible outcome

Section `42` is implementation-ready only if maintained evidence covers the
current failure and recovery behaviors it claims.

## Required certification lanes

- truth-source precedence
  - tests prove durable database state and transaction inventory outrank
    derivative logs and archive exports
- fault classification
  - process, storage, network, and operator-visible fault classes map to the
    documented current taxonomy
- fail-closed behavior
  - corruption, unsupported modes, and unsafe recovery paths refuse service as
    documented
- degraded-mode behavior
  - any degraded but still-supported runtime mode behaves deterministically and
    does not overclaim resilience
  - Beta 2 failover reconnect and ambiguity classifications behave according to
    the documented outcome classes
- shadow capture boundary
  - required local logical shadow capture failure blocks prune or maintenance
    exactly where current code says it does
- wal-after boundary
  - `wal_after` export is derivative only, requires committed terminal lineage,
    and failure does not become recovery truth
- remote archive boundary
  - remote archive or remote database delivery is optional derivative behavior
  and does not control correctness
- operator recovery boundary
  - documented operator intervention and recovery paths match the current
    runtime expectations
- fault-tolerance exclusions
  - unsupported HA, quorum, failover, or transparent healing claims are not
    surfaced as supported behavior
- shard-workflow recovery boundary
  - split, merge, move, and rebalance failures classify stale epoch, missing
    fence, ambiguous ownership, and recovery outcome deterministically
- distributed atomic coordination boundary
  - prepared-branch, decision-publication, heuristic-review, and ambiguous
    outcome classes remain deterministic and fail closed

## Negative requirements

- no test may infer consensus, replica failover, or automatic healing semantics
  unless section `42` explicitly certifies them
- no test may treat fail-closed refusal paths as degraded but supported runtime
  modes
- no test may treat derivative evidence lanes as primary recovery authority
