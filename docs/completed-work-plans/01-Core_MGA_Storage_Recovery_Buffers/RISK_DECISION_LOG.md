# Risk Decision Log

## Fixed Decisions

- B1-01-001 must close specification sufficiency before any implementation
  ticket begins
- the local reference tree under docs/reference is the primary donor and
  authority intake surface for this lane
- section `40` belongs to this package and must be released into scope before
  downstream closure
- transaction rollback vocabulary uses `ROLLED_BACK` for durable transaction
  termination and reserves `ABORTED` for non-transaction cancellation or abort
  paths
- section `02` lifecycle behavior is expanded to explicit Beta 1 required
  behavior instead of being narrowed to current substrate proof
- section `10` keeps the stronger reclaim and worker architecture as canon; the
  current narrower runtime remains implementation drift rather than a spec
  downgrade
- package ownership freeze uses one normalized audit-anchor set across the lane:
  no line-number-based implementation lookup is allowed in later tickets
- section-to-runtime owner splits are frozen in CODE_AREA_OWNERSHIP_MAP.md and
  SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv before lane A or lane B implementation
  tickets begin
- lane A closure is accepted on targeted custom-tablespace allocator or buffer
  proof; the dedicated section `06` bootstrap corruption matrix remains gate
  work for B1-01-005 rather than a blocker on B1-01-003
- lane B closure is accepted on targeted write-admission fence proof plus the
  existing lock, GC, checkpoint, restart, and TOAST evidence surfaces already
  present in the repo; broader gate or benchmark closure remains B1-01-005 work
- B1-01-005 closure is accepted on the bounded section `31` gate bundle
  preserved under `gates/B1-01-GATE-03/20260330T132542Z`, including the new
  section `04` or `06` matrix proofs and the named public-beta benchmark steps
- B1-01-006 closeout is accepted on the existing section `40` direct
  catalog-contract proof surfaced by
  `tests/unit/test_catalog_cluster_clock_extension_contract.cpp` search
  `ClockCatalogContracts`, together with tracker completion and archive move


## Active Risk

Risk: core storage and recovery files are highly coupled, so ticket boundaries must not be allowed to drift into uncontrolled parallel edits.

## Final Closeout Note

- B1-01-006 completed on March 30, 2026
- the remaining section `40` audit rows are now `implemented` on the direct
  contract proof preserved under `evidence/B1-01-006/clock_catalog_contract.log`
- all bounded tickets `B1-01-001` through `B1-01-006` are complete
- this package is archived under
  `docs/completed-work-plans/01-Core_MGA_Storage_Recovery_Buffers/` and should
  not be reopened in place
