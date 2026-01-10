# FINDINGS_REMEDIATION_MAPPING.md

## Purpose
This document maps existing audit and findings reports to consolidated remediation plans and task IDs.

## Mapping

| ID | Finding | Category | Plan/Milestone | Task / Next Step |
| --- | --- | --- | --- | --- |
| F-001 | `docs/archive/2026-01-09/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` | DDL/Emulation | Alpha Phase 2 | Close finding and keep DDL tests synced |
| F-002 | `docs/archive/2026-01-09/findings/DATETIME_UUID_STORAGE_FORMAT.md` | Storage/Types | Closeout | Close finding and keep spec synced |
| F-003 | `docs/archive/2026-01-09/findings/DEADLOCK_FIX_2025_12_30.md` | Concurrency | PLAN_19 | Re-run stored dependency tests to confirm |
| F-004 | `docs/archive/2026-01-09/findings/DEDICATED_ISQL_CLIENTS_REQUIREMENT.md` | Tooling/CLI | PLAN_06 | Close finding; expand CLI output compatibility as needed |
| F-005 | `docs/archive/2026-01-09/findings/DROPTABLE_DEADLOCK_ANALYSIS_2025_12_30.md` | Concurrency/DDL | PLAN_19 | Close finding; keep dependency regression tests green |
| F-006 | `docs/archive/2026-01-09/findings/EXECUTOR_TRANSACTION_TIMEOUT_ANALYSIS_2025_12_30.md` | Executor/Transaction | PLAN_18 | Close finding; monitor autocommit regression tests |
| F-007 | `docs/archive/2026-01-09/findings/FINAL_TEST_RESULTS_2025_12_31.md` | Test/Infra | PLAN_18, PLAN_19 | Close finding; keep timeout wiring verified |
| F-008 | `docs/archive/2026-01-09/findings/FINDINGS_REMEDIATION_MAPPING.md` | Docs/Planning | TBD | Keep mapping aligned to consolidated plan |
| F-009 | `docs/archive/2026-01-09/findings/FK_DEADLOCK_FIX_2025_12_31.md` | Concurrency/Constraints | PLAN_19 | Re-run FK dependency tests |
| F-010 | `docs/archive/2026-01-09/findings/SB_ISQL_COMMAND_LINE_ANALYSIS.md` | Tooling/CLI | PLAN_06 | Implement flags and update CLI docs |
| F-011 | `docs/archive/2026-01-09/findings/SESSION_SUMMARY_2025_12_31.md` | Docs/Recap | Closeout | No action |
| F-012 | `docs/archive/2026-01-09/findings/SQL_COMPATIBILITY_TEST_REPOSITORIES.md` | Test/Compatibility | PLAN_06, PLAN_08 | Select tests, add harness, integrate CI |
| F-013 | `docs/archive/2026-01-09/findings/TEST_EXECUTION_TIME_ANALYSIS_2025_12_31.md` | Test/Infra | PLAN_18, PLAN_19 | Reproduce hang and fix timeout enforcement |
| F-014 | `docs/archive/2026-01-09/findings/TEST_FIXES_2025_12_31.md` | Engine/Test | Alpha Phase 2 | Implement resolver cache for domains/roles/users |
| F-015 | `docs/archive/2026-01-09/findings/TEST_SUITE_FAILURES_2025_12_27.md` | Test/Infra | PLAN_19 | Confirm deadlock fixes; document missing tests |
| F-016 | `docs/archive/2026-01-09/findings/TEST_SUITE_FIX_2025_12_31.md` | Test/Infra | PLAN_18, PLAN_19 | Add test categorization and CI timing metrics |
| F-017 | `docs/archive/2026-01-09/findings/TEST_TIMEOUT_ANALYSIS_2025_12_29.md` | Test/Infra | PLAN_19 | Confirm regression tests stay green |
| F-018 | `docs/archive/2026-01-09/findings/TEST_TIMEOUT_REANALYSIS_2025_12_30.md` | Test/Infra | PLAN_19 | Maintain regression coverage |
| F-019 | `docs/archive/2026-01-09/findings/alpha_cluster_compatibility_audit.md` | Cluster/Network | Beta | Define routing/sharding design and roadmap |
| F-020 | `docs/archive/2026-01-09/findings/audit_results/AUDIT_TEMPLATE.md` | Docs/Template | Closeout | No action |
| F-021 | `docs/archive/2026-01-09/findings/database_lifecycle_upgrade_plan.md` | Cluster/Upgrade | Beta | Decide quorum thresholds and failure modes |
| F-022 | `docs/archive/2026-01-09/findings/domain_support_gaps.md` | Domains/Type System | Alpha Phase 2 | Implement domain metadata, type propagation, enforcement, tests |
| F-023 | `docs/archive/2026-01-09/findings/engine_gap_report.md` | Engine/Protocols | TBD | Track remaining stubs (COPY, EXPLAIN) and split into sub-items |
| F-024 | `docs/archive/2026-01-09/findings/firebird_emulation_parity_audit.md` | Parser-FB/Emulation | Alpha Phase 2 | Implement missing DDL/DML/PSQL and RDB$ coverage |
| F-025 | `docs/archive/2026-01-09/findings/firebird_wire_protocol_gaps.md` | Parser-FB/Protocol | Alpha Phase 2 | Specify limits/examples; update protocol spec |
| F-026 | `docs/archive/2026-01-09/findings/mysql_emulation_parity_audit.md` | Parser-MySQL/Emulation | Alpha Phase 2 | Continue constraint parsing + auth validation; fill remaining DDL gaps |
| F-027 | `docs/archive/2026-01-09/findings/mysql_wire_protocol_gaps.md` | Parser-MySQL/Protocol | Alpha Phase 2 | Define protocol specifics; update spec |
| F-028 | `docs/archive/2026-01-09/findings/postgresql_emulation_parity_audit.md` | Parser-PG/Emulation | Alpha Phase 2 | Finish DDL/DML alignment; wire COPY execution |
| F-029 | `docs/archive/2026-01-09/findings/postgresql_wire_protocol_gaps.md` | Parser-PG/Protocol | Alpha Phase 2 | Specify fields and COPY flows; update spec |

## Notes
- Mapping aligns with `docs/planning/CONSOLIDATED_FINDINGS_REMEDIATION_PLAN.md`.
- Update this table when new findings or plan milestones change.
