# Test Contract - 10_GC_and_Sweep

Status: current_authority_with_reconstructed_expansion

Last code-audit date: 2026-03-30

Current implementation-backed proof:
- `tests/unit/test_garbage_collector.cpp` proves persisted sweep cursor
  checksum handling, restart resume versus rewind behavior, lane-mask and
  strict-audit persistence, policy binding precedence, and evidence-mode
  handling
- `tests/unit/test_heap_index_gc_integration.cpp` proves reclaim maturity
  classification feeds heap-before-index cleanup ordering
- `tests/unit/test_mga_failpoint_replay.cpp` proves sweep checkpoint-loss
  failure is observable and replayable rather than silently marking progress
  complete

## Required tests

- sweep captures horizons once and applies `OIT/OAT/OST` consistently through
  one run
- reclaim legality produces the same decision for sweep and storage-engine
  cleanup given the same effective input
- reclaim eligibility never crosses visible or replay-retained history
- exact-family index backlog closes only after matching heap reclaim proof
- persisted sweep cursors resume when compatible and rewind when incompatible
- cursor persistence failures remain observable and do not silently mark
  progress complete
- `lane_mask` and `strict_audit` persist through restart and influence resume
  legality
- policy binding precedence is deterministic across object/table/schema/family
  scopes
- page-local compaction preserves slot identity and visible heads
- limbo or repair state defers reclaim deterministically
- evidence-before-prune, shadow-capture, and derivative `wal_after` lanes obey
  their configured policy modes
- Beta 2 archive tier lanes prove archive commit markers, payload verification,
  and legal-hold refusal before prune

## Negative tests

- invalid cursor checksum forces rewind
- index cleanup before heap proof is rejected
- reclaim under incompatible restart generation is rejected
- sweep completion without final publication is rejected
- invalid policy binding or page-audit mode fails closed

## Compatibility tests

- reclaim and sweep ordering remain compatible with sections `08`, `18`, `20`,
  `24`, and `31`

## Gate criteria

- must pass required tests before advancing stage

## Non-guarantees

- no statement is made here that every future archive/export variant is already
  test-covered
- no test may infer derivative-lane authority over MGA truth
- no test may treat role co-location inside one `SweepManager` as permission to
  collapse the logical role boundaries defined by the section
