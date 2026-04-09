# Canonical Gap Register

| Gap ID | Gap | Closing Files | Closing Ticket |
| --- | --- | --- | --- |
| SV-09-G01 | the authoritative spec tree does not yet have one row-complete status audit matrix covering every authoritative spec file | `SPEC_IMPLEMENTATION_AUDIT_MATRIX.csv`, `SPEC_STATUS_CLASSIFICATION.csv` | SV-09-002 |
| SV-09-G02 | canonical specs still contain implementation references that rely on unstable line numbers instead of durable search keys | `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv`, updated canonical specs | SV-09-003 |
| SV-09-G03 | status claims across the canonical tree have not been systematically checked against live implementation truth | updated canonical specs, `SPEC_STATUS_CLASSIFICATION.csv` | SV-09-004, SV-09-005, SV-09-006, SV-09-007 |
| SV-09-G04 | some surfaces may already be implemented while still documented as partial or unresolved | updated canonical specs, `PARTIAL_SPECIFICATIONS.md`, `FINISHED_SPECIFICATIONS.md` | SV-09-004, SV-09-005, SV-09-006, SV-09-007 |
| SV-09-G05 | some surfaces may be documented as implemented or partial without sufficient code-backed proof | updated canonical specs, `OUTSTANDING_SPECIFICATIONS.md`, `SPEC_STATUS_CLASSIFICATION.csv` | SV-09-004, SV-09-005, SV-09-006, SV-09-007 |
| SV-09-G06 | owned implementation files do not yet have a consistent unique identifier convention for durable spec-to-code lookup | `CODE_TRUTH_AUDIT_MAINTENANCE_RULES.md`, `LINE_NUMBER_TO_SEARCH_KEY_MIGRATION_LOG.csv`, updated implementation files | SV-09-003 |
| SV-09-G07 | there is no definitive finished/partial/outstanding rollup generated directly from the authoritative spec tree | `FINISHED_SPECIFICATIONS.md`, `PARTIAL_SPECIFICATIONS.md`, `OUTSTANDING_SPECIFICATIONS.md` | SV-09-008 |
