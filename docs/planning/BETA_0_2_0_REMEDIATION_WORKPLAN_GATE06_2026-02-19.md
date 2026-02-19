# Beta 0.2.0 Gate-06 Remediation Workplan (Detailed)
Last modified: 2026-02-19

## 1. Purpose

Correct all issues identified in:

- `docs/audit/V3_FUNCTIONALITY_AND_DOCUMENTATION_AUDIT_2026-02-19_GATE06.md`

This plan is execution-focused and defines concrete work packages, sequencing, validation, and exit gates.

## 2. Findings To Correct

### F-001: Mandatory functionality gap remains open
- `178` mandatory scope rows
- `37` closed
- `141` open
- Evidence:
  - `docs/planning/native_sql/gates/NSQL-GATE-06/CAPABILITY_MATRIX_SUMMARY.env`
  - `docs/planning/native_sql/gates/NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv`

### F-002: Runtime bridge partiality persists
- Large opcode families still return deterministic bridge rejection (`IRX_0406`).
- Evidence:
  - `src/sblr/executor.cpp:59331`
  - `src/sblr/executor.cpp:59433`
  - `src/sblr/executor.cpp:61383`

### F-003: Stale language reference claims
- Consolidated reference still reports window/distinct limitations that are no longer accurate.
- Evidence:
  - `docs/user-documentation/language-guide/NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md:968`
  - `docs/user-documentation/language-guide/NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md:969`
  - `src/parser/parser_v3.cpp:12750`
  - `src/parser/v3_emitter.cpp:4960`
  - `src/sblr/executor.cpp:43012`

### F-004: DDL lifecycle docs stale for role/group ALTER
- Documentation marks ALTER as unavailable while parser supports generic rename/move ALTER paths.
- Evidence:
  - `docs/user-documentation/language-guide/ddl/security/role/README.md:26`
  - `docs/user-documentation/language-guide/ddl/security/group/README.md:26`
  - `src/parser/parser_v3.cpp:7118`
  - `src/parser/parser_v3.cpp:7125`

### F-005: Missing modular language-guide coverage for parser-dispatched commands
- Dedicated modular docs missing (outside consolidated reference) for:
  - `SECURITY LABEL`, `REVOKE TOKEN`, `DECLARE EXTERNAL FUNCTION`
  - `INSTALL EXTENSION`, `LOAD EXTENSION`
  - UDR compile surfaces
  - Redis alias surfaces (`EVAL LUA`, `XGROUP`, `XREADGROUP`, `XCLAIM`)
  - `DOC PATH FILTER`, `TS BUCKET AGG`, `SEARCH DSL`, `VECTOR ANN`, `HYBRID BRIDGE`, graph path surfaces
  - `EXECUTE JOB`, `CANCEL JOB RUN`

### F-006: Documentation closure tracking not executed
- `BKL-DOC-001` checklist remains open across scoped directories.
- Evidence:
  - `docs/planning/BETA_0_2_0_SPEC_BACKLOG_2026-02-19.md:274`

## 3. Remediation Targets

1. `mandatory_open_rows`: `141 -> 0`
2. In-scope runtime opcode paths: `IRX_0406 -> 0`
3. Stale/incorrect language-guide claims: `0`
4. Parser-dispatched command families without modular docs: `0`
5. `BKL-DOC-001` checklist: fully checked for touched directories

## 4. Work Packages

## WP-00 Baseline Lock + Tracking Setup (Sprint 1)
- Objective:
  - Freeze a deterministic baseline for closure measurement and avoid drift.
- Tasks:
  - Archive Gate-06 as baseline and create Gate-07 working snapshot directory.
  - Export row tracker from `NSQL-GATE-06/NATIVE_CAPABILITY_MATRIX.csv` with columns:
    - `gap_item_id`, `engine`, `priority`, `native_domain`, `status_reason`,
      `owner`, `sprint`, `workpack_id`,
      `parser_touched`, `emitter_touched`, `executor_touched`, `doc_paths`, `test_paths`
  - Generate tracker artifacts with:
    - `tools/compliance/native_sql_gate07_execution_tracker.sh`
  - Attach tracker to backlog execution section.
- Deliverables:
  - `docs/planning/native_sql/gates/NSQL-GATE-07/` initial artifacts
  - row-tracker table/file under `docs/planning/native_sql/gates/NSQL-GATE-07/`
  - `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER.tsv`
  - `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER_OWNER_LOAD.tsv`
  - `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER_SPRINT_LOAD.tsv`
  - `docs/planning/native_sql/gates/NSQL-GATE-07/EXECUTION_TRACKER_SUMMARY.env`
- Exit criteria:
  - All `141` mandatory-open rows are represented in one execution tracker with assigned owner and sprint.

## WP-01 Documentation Accuracy Hotfix (Sprint 1)
- Objective:
  - Eliminate known stale statements before broader implementation.
- Tasks:
  - Correct stale function/window/distinct statements in
    - `docs/user-documentation/language-guide/NATIVE_PARSER_LANGUAGE_REFERENCE_BETA_0_1_0.md`
  - Correct role/group ALTER lifecycle status and notes in:
    - `docs/user-documentation/language-guide/ddl/security/role/README.md`
    - `docs/user-documentation/language-guide/ddl/security/group/README.md`
    - corresponding `alter.md` lifecycle docs
  - Update `TODO_BETA_0_2_0.md` entries to match current code reality.
- Validation:
  - Run targeted parser/emitter/runtime tests:
    - `ParserV3GapContractsTest.CountDistinctParsesAndBuildsSelectSurface`
    - `ParserV3NoSqlEmitterContractTest.EmitsDedicatedWindowFunctionOpcodes`
- Exit criteria:
  - No known stale claim from F-003/F-004 remains.

## WP-02 Modular Command Documentation Completion (Sprints 1-2)
- Objective:
  - Add dedicated modular docs for parser-dispatched commands missing outside consolidated reference.
- Tasks:
  - Add command-family docs with `syntax`, `semantics`, `runtime`, `examples`, `errors`, and `README` links.
  - Cover command groups:
    - admin/security: `SECURITY LABEL`, `REVOKE TOKEN`
    - extension/udr: `INSTALL/LOAD EXTENSION`, UDR compile + embedded SQL validate
    - job control: `EXECUTE JOB`, `CANCEL JOB RUN`
    - polyglot/bridge: doc/ts/search/vector/hybrid/graph and Redis alias surfaces
    - Firebird compatibility surface: `DECLARE EXTERNAL FUNCTION`
  - Ensure navigation rules:
    - child file links next-step and parent README
    - parent README links all child docs
    - `Last modified` present in each file
- Validation:
  - Markdown tree lint/readability pass
  - BKL-DOC-001 checklist rows marked done for new/updated folders
- Exit criteria:
  - F-005 closed.

## WP-03 Runtime Bridge Closure (`IRX_0406`) (Sprints 2-4)
- Objective:
  - Replace semantic-bridge reject paths with explicit runtime handlers for in-scope v3 features.
- Scope buckets:
  - admin/control: backup/restore/validate/cluster/service/cube families
  - polyglot bridge families: CQL/Mongo/Cypher/Redis/Milvus
  - advanced operator surfaces: doc/ts/search/vector/hybrid
- Tasks:
  - Implement explicit executor handlers and route them from v3 dispatch switch.
  - Keep deterministic contracts for unsupported variants with specific non-`IRX_0406` codes.
  - Update emitter contracts only where payload shape needs normalization.
- Validation:
  - Convert rejection-focused tests to positive semantic tests where closure is complete.
  - Keep targeted deterministic rejection tests for intentionally unsupported forms.
  - Required tests include:
    - `SBLRVNextExecutorDispatchContractTest.*`
    - `test_parser_v3_nosql_emitter_contract.cpp`
- Exit criteria:
  - In-scope opcode families no longer emit `IRX_0406`.

## WP-04 Parser/Emitter/Executor Canonicalization Gaps (Sprints 2-4)
- Objective:
  - Close code-level mismatches called out by audit and consolidated reference.
- Tasks:
  - `CREATE TYPE`: enrich payload + executor semantic handler.
  - `CREATE DATABASE EMULATED`: canonical payload propagation and runtime contract path normalization.
  - `DATABASE CONNECTION`: converge from config-key emulation toward explicit lifecycle contract.
  - Search/vector ALTER action expansion beyond rebuild-only coverage.
  - Revoke privilege parity with grant where required.
  - Subquery membership runtime closure (`IN/NOT IN (subquery)`).
  - Remaining operator/function/cast partials from audit matrix.
- Validation:
  - Parser + emitter unit tests for syntax/payload.
  - Executor semantics tests for each closure.
  - Gate scripts rerun after each subgroup completion.
- Exit criteria:
  - No open high-severity mismatch from consolidated audit sections 9/10.

## WP-05 DDL Lifecycle Completion Packs (Sprints 3-5)
- Objective:
  - Close partial lifecycle coverage across DDL object families.
- Scope:
  - management/data-storage/programmability/security/integration/cluster-service families marked partial/not-available in current docs.
- Tasks:
  - Implement missing command surfaces or normalize lifecycle matrices where generic support exists.
  - Add/refresh lifecycle docs (`create/alter/show/describe/drop`) per object.
  - Align README lifecycle status with code-backed behavior.
- Validation:
  - Object-lifecycle integration tests:
    - create -> alter -> show/describe -> drop
  - Documentation lifecycle matrix review against parser dispatch.
- Exit criteria:
  - No in-scope object remains `Partial command lifecycle`.

## WP-06 Engine Parity Closure (`141` rows) (Sprints 3-6)
- Objective:
  - Close all mandatory-open registry rows.
- Sequencing:
  - Phase A: P0 rows first (highest risk)
  - Phase B: P1 rows
  - Phase C: P2 rows
- Engine priority order:
  - MySQL (`32`), PostgreSQL (`27`), FirebirdSQL (`12`), then remaining engines
- Tasks per row (`gap_item_id`):
  - syntax contract completion
  - AST contract completion
  - SBLR mapping completion
  - executor semantic completion or deterministic explicit reject (if approved out of beta scope)
  - language-guide mapping update
- Validation:
  - `native_sql_syn13_coverage.sh`
  - `native_sql_ast_sblr_binding_coverage.sh`
  - `native_sql_capability_matrix_freeze.sh`
- Exit criteria:
  - `mandatory_open_rows = 0`
  - `unmapped_rows = 0`

## WP-07 Full Documentation Closure (Sprints 4-6)
- Objective:
  - Close documentation debt completely for all touched surfaces.
- Tasks:
  - Complete BKL-DOC-001 checklist row-by-row.
  - Update command-group docs to match current parser dispatch.
  - Ensure examples are concrete and avoid placeholder-only forms for supported surfaces.
  - Keep all links internal to repository tree.
- Validation:
  - README link integrity review
  - checklist completion verification
- Exit criteria:
  - F-006 closed, doc closure evidence merged.

## WP-08 Gate And Release Validation (Sprint 6)
- Objective:
  - Prove readiness for beta release decision.
- Tasks:
  - Clean build from empty build directory.
  - Full test suite pass.
  - Regenerate Gate artifacts (NSQL and BETA gate).
  - Create release package evidence bundle.
- Required commands:
  - `tools/compliance/run_beta_gate_001.sh`
  - full `ctest --test-dir build --output-on-failure`
  - NSQL gate scripts listed in WP-06
- Exit criteria:
  - All gates green with archived evidence.

## 5. Sprint Timeline

### Sprint 1 (2026-02-20 to 2026-02-26)
- WP-00, WP-01, start WP-02

### Sprint 2 (2026-02-27 to 2026-03-05)
- finish WP-02, start WP-03 and WP-04 high-priority items

### Sprint 3 (2026-03-06 to 2026-03-12)
- continue WP-03/WP-04, start WP-05, start WP-06 P0

### Sprint 4 (2026-03-13 to 2026-03-19)
- finish WP-03, continue WP-05, WP-06 P0/P1, start WP-07

### Sprint 5 (2026-03-20 to 2026-03-26)
- finish WP-04/WP-05, continue WP-06 P1/P2, continue WP-07

### Sprint 6 (2026-03-27 to 2026-04-02)
- finish WP-06/WP-07, execute WP-08, prepare go/no-go package

## 6. Validation Cadence

### Per merge request
- affected unit/integration tests
- docs update for touched parser/emitter/executor surfaces

### Per sprint close
- clean build + full test
- NSQL gate scripts rerun
- update open/closed counts in execution tracker

### Program close
- all exit criteria from WP-00..WP-08 satisfied
- updated final audit confirming zero mandatory-open rows

## 7. Go/No-Go Criteria For Beta 0.2.0

All must be true:

1. `mandatory_open_rows == 0`
2. no in-scope runtime path returns `IRX_0406`
3. no stale known-false claim remains in language guide
4. all parser-dispatched command families have modular docs
5. BKL-DOC-001 checklist is complete for touched directories
6. clean build and full test suite pass with archived gate evidence
