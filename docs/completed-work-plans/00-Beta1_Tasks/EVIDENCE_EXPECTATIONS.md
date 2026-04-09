# Evidence Expectations

## General Rule

Every ticket must leave behind explicit evidence inside this package or in the
generated downstream work-plan roots it creates.

A ticket is not complete when the prose says it is complete. It is complete
only when the required planning artifacts named here exist and agree with each
other.

## Ticket-Specific Expectations

### `B1P-001`

Required evidence:
- Beta 1 default classification recorded in `README.md`
- explicit Beta 2 or Beta 3 exclusions recorded in
  `DEFINITIVE_SPECSET_INDEX.md`
- search-key audit contract recorded in
  `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`

### `B1P-002`

Required evidence:
- no orphan Beta 1 sections remain in `DEFINITIVE_SPECSET_INDEX.md`
- split-owner rules are explicit in `README.md` and
  `CODE_AREA_OWNERSHIP_MAP.md`

### `B1P-003`

Required evidence:
- downstream plan order frozen in `README.md`
- ticket order frozen in `ORDERED_TASK_TICKETS.csv`
- dependencies frozen in `DEPENDENCY_GRAPH.csv`

### `B1P-004`

Required evidence:
- standardized work-plan packages exist for `01` through `04`
- the first ticket in each generated package is specification sufficiency
  closure
- each generated package includes `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`
- `docs/work-plans/README.md` lists the generated plans in order

### `B1P-005`

Required evidence:
- standardized work-plan packages exist for `05` through `08`
- the first ticket in each generated package is specification sufficiency
  closure
- each generated package includes required benchmark, gate, and audit files
- `docs/work-plans/README.md` lists the generated plans in order

### `B1P-006`

Required evidence:
- `MASTER_TRACKER.md` and `MASTER_TRACKER.csv` show all tickets done
- `RISK_DECISION_LOG.md` contains final closeout note
- the package is ready to move to `docs/completed-work-plans/`
