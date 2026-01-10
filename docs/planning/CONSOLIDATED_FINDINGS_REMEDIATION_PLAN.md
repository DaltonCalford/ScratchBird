# Consolidated Findings Remediation Plan

Source: `ScratchBird/docs/findings/CONSOLIDATED_FINDINGS.md`

Legend:
- Status: IMPLEMENTED, PARTIAL, MISSING, NEEDS-AUDIT, DOC-ONLY
- Owners: Engine Core; Parser-FB; Parser-PG; Parser-MySQL; Network Listener; Tooling/CLI; Test/Infra; Docs; Cluster Manager; Security
- Milestones: PLAN_06; PLAN_18; PLAN_19; Alpha Phase 2; Beta; TBD; Closeout

| ID | Finding | Summary | Status | Owner | Milestone | Next Step |
| --- | --- | --- | --- | --- | --- | --- |
| F-001 | `ScratchBird/docs/archive/2026-01-09/findings/CRITICAL_SCHEMA_DATABASE_OPCODE_GAP.md` | Schema/database DDL parity; cascade/force semantics; parser gaps | IMPLEMENTED | Engine Core, Parser-FB, Parser-PG, Parser-MySQL | Alpha Phase 2 | Close finding and keep DDL tests synced |
| F-002 | `ScratchBird/docs/archive/2026-01-09/findings/DATETIME_UUID_STORAGE_FORMAT.md` | TypedValue serialization documented | IMPLEMENTED | Docs, Engine Core | Closeout | Close finding and keep spec synced |
| F-003 | `ScratchBird/docs/archive/2026-01-09/findings/DEADLOCK_FIX_2025_12_30.md` | dropFunction/dropProcedure deadlock fix | IMPLEMENTED | Engine Core | PLAN_19 | Re-run stored dependency tests to confirm |
| F-004 | `ScratchBird/docs/archive/2026-01-09/findings/DEDICATED_ISQL_CLIENTS_REQUIREMENT.md` | Missing sb_fb_isql/sb_pg_isql/sb_my_isql + shared CLI lib | IMPLEMENTED | Tooling/CLI, Parser-FB, Parser-PG, Parser-MySQL | PLAN_06 | Close finding; expand CLI output compatibility as needed |
| F-005 | `ScratchBird/docs/archive/2026-01-09/findings/DROPTABLE_DEADLOCK_ANALYSIS_2025_12_30.md` | dropTable/dropSequence lock ordering fix needs verification | IMPLEMENTED | Engine Core | PLAN_19 | Close finding; keep dependency regression tests green |
| F-006 | `ScratchBird/docs/archive/2026-01-09/findings/EXECUTOR_TRANSACTION_TIMEOUT_ANALYSIS_2025_12_30.md` | Autocommit + createTable locking; verify timeout resolution | IMPLEMENTED | Engine Core | PLAN_18 | Close finding; monitor autocommit regression tests |
| F-007 | `ScratchBird/docs/archive/2026-01-09/findings/FINAL_TEST_RESULTS_2025_12_31.md` | Long test + timeout enforcement + long txn monitor init | IMPLEMENTED | Test/Infra, Engine Core | PLAN_18, PLAN_19 | Close finding; keep timeout wiring verified |
| F-008 | `ScratchBird/docs/archive/2026-01-09/findings/FINDINGS_REMEDIATION_MAPPING.md` | Mapping of findings to plans incomplete | IMPLEMENTED | Docs | TBD | Keep mapping aligned to consolidated plan |
| F-009 | `ScratchBird/docs/archive/2026-01-09/findings/FK_DEADLOCK_FIX_2025_12_31.md` | createForeignKey lock ordering + constraint name lookup fix | IMPLEMENTED | Engine Core | PLAN_19 | Close finding; keep FK regression tests green |
| F-010 | `ScratchBird/docs/archive/2026-01-09/findings/SB_ISQL_COMMAND_LINE_ANALYSIS.md` | sb_isql missing -i alias and -par parser selection | IMPLEMENTED | Tooling/CLI | PLAN_06 | Close finding; keep CLI docs synced |
| F-011 | `ScratchBird/docs/archive/2026-01-09/findings/SESSION_SUMMARY_2025_12_31.md` | Session recap; issues resolved | DOC-ONLY | Docs | Closeout | No action |
| F-012 | `ScratchBird/docs/archive/2026-01-09/findings/SQL_COMPATIBILITY_TEST_REPOSITORIES.md` | External test repos and curation plan | IMPLEMENTED | Test/Infra, Parser-FB, Parser-PG, Parser-MySQL | PLAN_06, PLAN_08 | Keep curated compatibility lists updated as coverage expands |
| F-013 | `ScratchBird/docs/archive/2026-01-09/findings/TEST_EXECUTION_TIME_ANALYSIS_2025_12_31.md` | CTest timeouts not enforced; long-running test | NEEDS-AUDIT | Test/Infra | PLAN_18, PLAN_19 | Reproduce hang and fix timeout enforcement |
| F-014 | `ScratchBird/docs/archive/2026-01-09/findings/TEST_FIXES_2025_12_31.md` | Test fixes done; resolver cache gaps remain | IMPLEMENTED | Engine Core, Test/Infra | Alpha Phase 2 | Keep resolver cache and rename/move tests in sync |
| F-015 | `ScratchBird/docs/archive/2026-01-09/findings/TEST_SUITE_FAILURES_2025_12_27.md` | Prior timeouts + missing executables | IMPLEMENTED | Engine Core, Test/Infra | PLAN_19 | Re-run dependency/bytecode tests after executor changes; track any new missing binaries |
| F-016 | `ScratchBird/docs/archive/2026-01-09/findings/TEST_SUITE_FIX_2025_12_31.md` | Test suite split; follow-up work pending | IMPLEMENTED | Test/Infra | PLAN_18, PLAN_19 | Maintain smoke/unit/integration buckets and runtime reporting |
| F-017 | `ScratchBird/docs/archive/2026-01-09/findings/TEST_TIMEOUT_ANALYSIS_2025_12_29.md` | Deadlock root cause; fix verified elsewhere | IMPLEMENTED | Engine Core | PLAN_19 | Confirm regression tests stay green |
| F-018 | `ScratchBird/docs/archive/2026-01-09/findings/TEST_TIMEOUT_REANALYSIS_2025_12_30.md` | Deadlock fix verified | IMPLEMENTED | Engine Core | PLAN_19 | Maintain regression coverage |
| F-019 | `ScratchBird/docs/archive/2026-01-09/findings/alpha_cluster_compatibility_audit.md` | Cluster routing/sharding missing | MISSING | Cluster Manager, Network Listener | Beta | Define routing/sharding design and roadmap |
| F-020 | `ScratchBird/docs/archive/2026-01-09/findings/audit_results/AUDIT_TEMPLATE.md` | Audit template only | DOC-ONLY | Docs | Closeout | No action |
| F-021 | `ScratchBird/docs/archive/2026-01-09/findings/database_lifecycle_upgrade_plan.md` | Cluster upgrade path and quorum decisions | NEEDS-AUDIT | Cluster Manager, Engine Core | Beta | Decide quorum thresholds and failure modes |
| F-022 | `ScratchBird/docs/archive/2026-01-09/findings/domain_support_gaps.md` | Domain catalog/type/enforcement gaps (TYPE_DOMAIN column encoding + domain defaults applied) | PARTIAL | Engine Core, Parser-FB | Alpha Phase 2 | Confirm array domain behavior and information_schema exposure; add coverage for domain defaults + column metadata |
| F-023 | `ScratchBird/docs/archive/2026-01-09/findings/engine_gap_report.md` | Engine stubs across storage, constraints, protocols, security | PARTIAL | Engine Core, Security, Network Listener | TBD | COPY/EXPLAIN now have executor coverage (file-based COPY, text EXPLAIN); finish STDIN/STDOUT protocol paths, COPY options, and richer plan metadata |
| F-024 | `ScratchBird/docs/archive/2026-01-09/findings/firebird_emulation_parity_audit.md` | Firebird parser/PSQL/catalog gaps | PARTIAL | Parser-FB | Alpha Phase 2 | Implement missing DDL/DML/PSQL and RDB$ coverage |
| F-025 | `ScratchBird/docs/archive/2026-01-09/findings/firebird_wire_protocol_gaps.md` | BLOB segmentation, compression, status vector mapping | NEEDS-AUDIT | Parser-FB | Alpha Phase 2 | Specify limits and examples; update protocol spec |
| F-026 | `ScratchBird/docs/archive/2026-01-09/findings/mysql_emulation_parity_audit.md` | MySQL parser DDL/constraints/auth gaps (CREATE INDEX/VIEW/DROP/TRUNCATE wired) | PARTIAL | Parser-MySQL | Alpha Phase 2 | Align REPLACE payloads, extend ALTER TABLE beyond rename, decide how to emit/record table options |
| F-027 | `ScratchBird/docs/archive/2026-01-09/findings/mysql_wire_protocol_gaps.md` | OK packet state, cursor protocol, compression rules | NEEDS-AUDIT | Parser-MySQL | Alpha Phase 2 | Define protocol specifics; update spec |
| F-028 | `ScratchBird/docs/archive/2026-01-09/findings/postgresql_emulation_parity_audit.md` | PG expression eval, auth, cancel gaps (DDL payloads aligned for sequence/user/role/grant/copy) | PARTIAL | Parser-PG | Alpha Phase 2 | Finish remaining DDL/DML alignment and runtime auth/cancel; wire COPY execution |
| F-029 | `ScratchBird/docs/archive/2026-01-09/findings/postgresql_wire_protocol_gaps.md` | ErrorResponse fields and COPY framing | NEEDS-AUDIT | Parser-PG | Alpha Phase 2 | Specify fields and COPY flows; update spec |
