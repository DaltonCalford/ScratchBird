# Section 12 Temporary Tables

Status: current_authority

Section `12` is the canonical current-state authority for:
- temporary table catalog identity and scope
- parser and executor handling of `ON COMMIT`
- commit, session-end, and startup temp cleanup
- temp page non-durability
- planner spill estimation, spill refusal, and spill metadata

Section `12` is not the authority for a general runtime workfile subsystem.
Current code proves planner spill estimation and refusal behavior much more
strongly than it proves runtime spill artifact identity, quotas, cleanup, or
restart handling.

Current section split:
- `TEMP_TABLES_NORMATIVE_IMPLEMENTATION.md`
  - authoritative current temp-table lifecycle
- `TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md`
  - authoritative current planner spill contract

Unsupported current claims:
- runtime workfile artifact registry
- runtime workfile quota model
- runtime spill restart cleanup model
- runtime spill diagnostics surface beyond planner or explain metadata

## Direct audit lookup anchors
- `src/core/connection_context.cpp` search key `ConnectionContext::cleanupTempTablesOnCommit(`
- `src/core/connection_context.cpp` search key `ConnectionContext::cleanupTempTablesOnSessionEnd(`
- `src/optimizer/query_planner.cpp` search key `plannerSpillPolicyName(`

<!-- AUTO-GENERATED:FILE-LIST:START -->
- [DECISION_RECORD.md](DECISION_RECORD.md)
- [DEPENDENCIES.md](DEPENDENCIES.md)
- [SPEC_OUTLINE.md](SPEC_OUTLINE.md)
- [TEMP_TABLES_NORMATIVE_IMPLEMENTATION.md](TEMP_TABLES_NORMATIVE_IMPLEMENTATION.md)
- [TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md](TEMP_WORKFILE_AND_OPERATOR_SPILL_CONTRACT.md)
- [TEST_CONTRACT.md](TEST_CONTRACT.md)
<!-- AUTO-GENERATED:FILE-LIST:END -->
