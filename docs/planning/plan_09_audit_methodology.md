# Plan 09 - Implementation Audit Methodology

## Scope
Define a repeatable audit process to verify that work performed by other agents fully implements the defined plans, requirements, and tests. This is the authoritative audit workflow if session context is lost.

## Priority
P0 (required for governance and beta readiness).

## References
- `docs/findings/engine_gap_report.md`
- `docs/planning/plan_01_core_storage_gc.md`
- `docs/planning/plan_02_uuid_resolution_and_rename_move.md`
- `docs/planning/plan_03_security_context_auth_audit_quorum.md`
- `docs/planning/plan_04_parser_and_compatibility.md`
- `docs/planning/plan_05_protocol_odbc_pool.md`
- `docs/planning/plan_06_metadata_show_and_catalog.md`
- `docs/planning/plan_07_emulated_protocol_compatibility.md`
- `docs/planning/plan_08_protocol_conformance_testing.md`
- `docs/planning/plan_10_cluster_domains_and_conflict_resolution.md`
- `docs/planning/plan_11_alpha_cluster_compatibility_guardrails.md`
- `docs/planning/plan_12_domain_runtime_and_type_system.md`

## Audit Inputs (Required Artifacts)
- Implemented code changes (diffs or commit list).
- Updated/added tests and their results.
- Protocol trace artifacts under `tests/protocol_traces/`.
- Updated catalog DDL or schema migration notes.

## Audit Steps (Mandatory)
1) **Plan-to-Code Mapping**
   - For each plan, list all tasks and verify code changes exist.
   - Confirm API names, schemas, and opcodes match plan details.
2) **Schema Verification**
   - Validate catalog DDL additions exist in code or migration paths.
   - Confirm indexes are implemented as specified.
3) **Protocol Compliance**
   - Compare golden traces vs live traces.
   - Verify any replication command is rejected with explicit errors.
4) **Security Context**
   - Confirm AuthKey/session binding is present and immutable per transaction.
   - Check policy epochs and role switching behavior.
5) **Testing Coverage**
   - Ensure all plan test requirements exist and run.
   - Verify regression tests cover all protocol gap items.
6) **Negative Testing**
   - Confirm unsupported features fail cleanly with correct errors.
7) **Audit Report**
   - Summarize pass/fail per plan with evidence.
   - List all deviations or missing items with code references.

## Completion Checklist (Auditor)
- [ ] Every plan has a mapped code change or explicit deferral.
- [ ] All schema DDL changes are present and indexed.
- [ ] Protocol traces match expected behavior.
- [ ] Security context and audit logging requirements are satisfied.
- [ ] All required tests exist, run, and pass.
- [ ] A final audit summary is produced with gaps and remediation steps.

## Deliverable
An audit report under `docs/findings/audit_results/` with:
- Per-plan compliance status (PASS/FAIL/DEFERRED).
- Evidence: file paths, test outputs, and trace diffs.
- Any required remediation or follow-up tasks.

## Common Failure Patterns
- Implemented only in executor/parser; `CatalogManager` direct calls still bypass logic.
- Cache updates without on-disk persistence or load path; restart loses behavior.
- Switch statements or enum mappings missing new values, producing `<unknown>` and wrong behavior.
- CASCADE/RESTRICT or config gating ignored; dependency checks bypassed or inconsistent.
- Tests cover happy-path only; missing restart, negative, and concurrency/lock-order cases.
- Spec deviations introduced without explicit config flags or documentation.
