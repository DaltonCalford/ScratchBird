# Risk Decision Log

## Fixed Decisions

- B1-04-001 must close specification sufficiency before any implementation
  ticket begins
- the local reference tree under docs/reference is the primary donor and
  authority intake surface for this lane
- every admitted named `IndexType` is independently primary for planner,
  costing, metrics, observability, and maintenance even when the runtime
  backend is shared
- Beta 1 requires full persisted canonical family fields per index rather than
  deriving family identity only at planner time
- accelerator-capable named families are active Beta 1 implementation targets
  and extend the existing workload policy and binding rows rather than creating
  a parallel governance catalog
- effective memory and CPU envelopes derive from the authoritative environment
  ceiling with environment-based defaults and explicit configuration clamped
  inside that ceiling

## Active Risk

Risk: no active implementation risk remains inside this package. Remaining risk
is limited to future follow-on work reopening these section seams without a new
active work-plan.

## Final Closeout Note

All bounded tickets for package `04` are complete. Lane A and lane B proof is
preserved, the repo-local cache-buffer, optimizer cost-calibration, and
ordered-index benchmark artifacts are recorded for the touched section `31`
surface, and the directory is archived under
`docs/completed-work-plans/04-Access_Methods_Indexes_Optimizer_Memory/`.

## Final Closeout Note

Not yet available.
