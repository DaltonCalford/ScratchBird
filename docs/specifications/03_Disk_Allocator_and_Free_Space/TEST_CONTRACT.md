# Section 03 Test Contract

## Status
- Specification status: current_authority_with_reconstructed_expansion
- Last code-audit date: 2026-03-30

## Current status
This code-first audit did not verify a full section `03` test inventory. The canonical test contract is therefore limited to code-backed behaviors that must be covered next, not to claims that a complete gate suite is already proven.

Targeted lane-A proof added by this work-plan:
- `tests/unit/test_tablespace_autoextend.cpp` proves page-manager custom
  tablespace allocation and autoextend behavior
- `tests/unit/test_storage_engine.cpp` proves buffer-pool global page
  allocation and storage-engine publication into a custom-tablespace heap page

Targeted lane-B proof added by this work-plan:
- `tests/unit/test_executor_transaction_payload.cpp` proves checkpoint control
  persistence, writeback-incident persistence, reopen fence reload, and
  checkpoint debt rebuild behavior across restart
- `tests/unit/test_transaction_manager.cpp` proves transaction begin and
  rollback refuse an open write-admission fence without consuming a new XID or
  terminally publishing unsafe rollback state

## Code-backed behaviors that require direct test coverage
- Page-manager allocation, autoextend, preallocation, and FSM reconstruction behavior
- Buffer-pool pin/unpin, dirty publication, flush-page, flush-all, flush-tablespace, and checkpoint-bound drain behavior
- Background-writer threshold transitions and candidate ordering
- Fail-closed write-admission fencing after writeback incidents
- Garbage-collector cooperative/background modes, sweep-blocked wake behavior, and MGA hint publication
- Config loader acceptance and rejection of buffer layouts, profile values, percentages, and prefetch/writeback knobs
- Explicit rejection or downgrade behavior for unsupported layouts such as `hotcold` and `tablespace`
- Logical policy-domain budgeting and residency-tier exposure over the shared frame array
- Page-class-aware buffer policy behavior for system, scan-probation, temp-work, and MGA-heavy page classes
- Prefetch debt, usefulness-floor, and thrash-state transition behavior under pressure
- Dirty-generation age, queue-state, and checkpoint-debt reporting behavior
- Explicit non-adoption of WAL-distance or LSN-gated flush legality in buffer and writeback behavior
- Page-table partition contention behavior and per-frame content-lock wait behavior under concurrent pressure
- Config-surface consistency between accepted runtime knobs and operator-facing example configuration artifacts
- Optional future asynchronous prefetch or page-image torn-write hardening claims must remain blocked until directly proven by code and gates

## Non-blocking expansion candidates
- A machine-readable section `03` gate matrix
- Claim-to-test and claim-to-gate traceability for every canonical file in this section
- Direct verification of test files and execution artifacts for section `03`
- Restart warmup and hot-set-summary evidence once those derivative-state features become code-backed

## Suggestions
- Create one section `03` gate suite that groups allocator/FSM, buffer/flush, GC coordination, and failure-fencing behaviors.
- Add one commercial-grade cache hardening lane for policy domains, ghost history, prefetch fairness, dirty-debt telemetry, and restart warmup without weakening MGA or forced-write rules.
- Keep donor-inspired checkpoint or durability language out of section `03` tests unless it is first normalized into ScratchBird MGA and forced-write terms.
- Do not mark this section `implemented` until the gate matrix is code-backed and maintained.
