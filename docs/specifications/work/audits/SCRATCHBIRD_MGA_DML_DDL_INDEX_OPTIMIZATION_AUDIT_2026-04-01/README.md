# ScratchBird MGA DML DDL Index Optimization Audit

This audit package turns the local donor-engine write-path review and the April 1, 2026 web-research bundle into a ScratchBird-specific optimization plan for DML, DDL, and index maintenance.

## File Set

- `FULL_OPTIMIZATION_AUDIT.md`
- `OPTIMIZATION_DECISION_MATRIX.csv`

## Inputs

- ScratchBird canonical authority under `docs/specifications/`
- local donor audit package `DONOR_ENGINE_DML_WRITE_PATH_AUDIT_2026-04-01`
- downloaded vendor-doc manifest `docs/reference/workspace_library/technical_specs/DML_DDL_INDEX_WEB_SOURCES_20260401.md`
- downloaded paper manifest `docs/reference/workspace_library/whitepapers/DML_DDL_INDEX_WHITEPAPERS_20260401.md`

## Reading Order

1. `FULL_OPTIMIZATION_AUDIT.md`
2. `OPTIMIZATION_DECISION_MATRIX.csv`

## Main Conclusion

The best path is not to copy one donor and not to replace MGA with a donor durability model. The strongest plan is:

- keep ScratchBird lineage and schema-epoch publication as the truth
- reduce exact-index churn with HOT-like same-key suppression and batch apply
- use shadow-build and resumable publish for heavy index and DDL work
- use immutable generations for summary, ranked text, ANN, and other heavy families
- add narrow, heavily governed deferral only where it lowers random I/O without hiding correctness debt
- expand the existing online DDL and shadow-index substrate instead of introducing a second metadata model
