# Section 38 Test Contract

Section `38` is implementation-ready only if maintained evidence covers the
current governance and parallelism behaviors it claims.

## Required certification lanes

- governance ownership
  - ownership of admission, worker dispatch, and operator controls is
    deterministic and non-overlapping
- admission and scheduling
  - documented admission controls or queueing limits produce deterministic
    outcomes
  - unsupported fairness or priority guarantees are not claimed as active
- worker and parallel scope
  - current worker-based execution paths behave as documented
  - unsupported broad intra-query parallelism remains fail closed
- resource isolation
  - current isolation or limit controls behave according to documented scope
  - no hard multi-tenant isolation is claimed unless explicitly backed
  - Beta 2 tenant admission, quota, and burst or refusal behavior are
    deterministic
- observability
  - operator-visible status, controls, and diagnostics match the documented
    current governance surface
  - maintenance-debt ledger state, lease state, retry state, and scheduler
    backlog metrics match the documented surface
  - memory-subsystem debt rows follow the same lease and retry behavior as
    other maintenance debt classes
  - service tiers and tenant pools publish deterministic budget, burst, and
    membership state

## Negative requirements

- no test may infer a mature cost-based parallel planner from section `38`
- no test may infer strict fairness, quota, or tenant isolation guarantees that
  the section does not explicitly certify
- no test may infer cluster-wide distributed scheduling from the maintenance
  debt ledger
