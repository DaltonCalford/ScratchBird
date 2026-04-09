# B1-01-001 Evidence Note

## Closure summary

Specification sufficiency for this package was re-proven against the assigned
canon and updated local authority.

The closure pass resolved the following blocking ambiguities:
- section `40` was restored into the active package scope and spec index
- durable transaction rollback vocabulary now uses `ROLLED_BACK` while
  `ABORTED` remains reserved for non-transaction cancellation paths
- section `02` now states explicit Beta 1 required attach, detach, migrate,
  shrink, split, cutover, and durable lifecycle-history behavior
- section `03` now explicitly bounds page-level FSM allocation, shared-frame
  buffer policy, and roadmap-only policy surfaces
- section `10` now keeps the stronger reclaim legality, cursor, worker-role,
  and retention architecture as canon while identifying current narrower runtime
  realization as implementation drift

## Residual non-blockers

- section `04` remains an evidence-depth issue rather than a spec ambiguity
- section `06` remains a dedicated corruption-matrix gate issue rather than a
  spec ambiguity
- section `11` remains bounded to the current TOAST-first contract and does not
  require standalone LOB session semantics for this lane
