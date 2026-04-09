# Driver Language Baseline Specification Workpack

Date: 2026-03-03
Status: execution baseline
Applies to: all alpha and beta driver lanes

## 1) Purpose
Define a unified execution specification for implementing every alpha/beta driver lane to meet or exceed the canonical JDBC baseline.

## 2) Canonical Inputs
1. `docs/specifications/30_Client_Tooling/DRIVER_JDBC_BASELINE_IMPLEMENTATION_SPECIFICATION.md`
2. `docs/specifications/30_Client_Tooling/DRIVER_ODBC_BASELINE_SPECIFICATION.md`
3. `docs/specifications/30_Client_Tooling/DRIVER_CPP_BASELINE_SPECIFICATION.md`
4. `docs/specifications/30_Client_Tooling/DRIVER_DOTNET_BASELINE_SPECIFICATION.md`
5. `docs/specifications/30_Client_Tooling/DRIVER_GO_BASELINE_SPECIFICATION.md`
6. `docs/specifications/30_Client_Tooling/DRIVER_RUST_BASELINE_SPECIFICATION.md`
7. `docs/specifications/30_Client_Tooling/DRIVER_NODE_BASELINE_SPECIFICATION.md`
8. `docs/specifications/30_Client_Tooling/DRIVER_PYTHON_BASELINE_SPECIFICATION.md`
9. `docs/specifications/30_Client_Tooling/DRIVER_PHP_BASELINE_SPECIFICATION.md`
10. `docs/specifications/30_Client_Tooling/DRIVER_RUBY_BASELINE_SPECIFICATION.md`
11. `docs/specifications/30_Client_Tooling/DRIVER_PASCAL_BASELINE_SPECIFICATION.md`
12. `docs/specifications/30_Client_Tooling/DRIVER_MOJO_BASELINE_SPECIFICATION.md`
13. `docs/specifications/30_Client_Tooling/DRIVER_CLI_BASELINE_SPECIFICATION.md`
14. `docs/specifications/30_Client_Tooling/DRIVER_DART_BASELINE_SPECIFICATION.md`
15. `docs/specifications/30_Client_Tooling/DRIVER_SWIFT_BASELINE_SPECIFICATION.md`
16. `docs/specifications/30_Client_Tooling/DRIVER_R_BASELINE_SPECIFICATION.md`
17. `docs/specifications/30_Client_Tooling/TEST_CONTRACT.md`

## 3) Program Invariants
1. No driver lane may weaken `JDBCBL-*` semantics.
2. Recursive schema tree behavior is metadata-only and mandatory.
3. Parent uniqueness and cross-schema same-name semantics are mandatory.
4. Parser/engine boundary remains intact in all driver lanes.
5. Known JDBC defects are not mandatory to replicate.

## 4) Requirement Catalog

| Requirement ID | Normative Requirement | Acceptance Evidence |
| --- | --- | --- |
| DLB-RQ-001 | Each driver lane MUST produce a complete requirement mapping for all baseline requirements. | Traceability matrix row set complete for lane |
| DLB-RQ-002 | Connection/auth/protocol behavior MUST satisfy lane-specific `*-CONN-*` requirements. | Lane connection/auth tests + SQLSTATE report |
| DLB-RQ-003 | Transaction/session behavior MUST satisfy lane-specific `*-TXN-*` requirements. | Lane transaction/savepoint tests |
| DLB-RQ-004 | Statement execution behavior MUST satisfy lane-specific `*-EXEC-*` requirements. | Multi-result/batch/generated-key/cancel tests |
| DLB-RQ-005 | Metadata behavior MUST satisfy lane-specific `*-META-*` requirements. | Recursive schema tree evidence from metadata-only path |
| DLB-RQ-006 | Type-system behavior MUST satisfy lane-specific `*-TYPE-*` requirements. | Type round-trip matrix including advanced families |
| DLB-RQ-007 | Error/resilience behavior MUST satisfy lane-specific `*-ERR-*` and `*-RES-*` requirements. | SQLSTATE/error-class matrix + resilience test report |
| DLB-RQ-008 | Lane documentation MUST include API contract, defaults, and migration notes. | Lane docs merged and linked in workpack |
| DLB-RQ-009 | Each lane MUST pass cross-driver conformance suites (`T30-I`, `T30-J`). | Conformance report and gate summary |
| DLB-RQ-010 | Release-tier promotion requires no `MISSING` baseline requirements. | Final tracker state + approved exception register |

## 5) Execution Model
1. Work executes in sixteen lanes: JDBC reference lane plus fifteen implementation lanes.
2. Each lane follows the same stream order:
   - `S0` bootstrap and requirements mapping.
   - `S1` connection/auth/protocol.
   - `S2` transaction/execution.
   - `S3` metadata/recursive schema/DDL editor support.
   - `S4` type system and object families.
   - `S5` error/resilience/pooling/observability.
   - `S6` conformance docs and evidence packaging.
   - `S7` gate closure and promotion decision.
3. A lane may begin `S(n+1)` only when `S(n)` is green in tracker.

## 6) Gate Policy
1. `DLB-GATE-00`: input spec lock and tracker initialization complete.
2. `DLB-GATE-01`: connection/auth/protocol parity complete for all lanes.
3. `DLB-GATE-02`: execution and transaction parity complete for all lanes.
4. `DLB-GATE-03`: metadata/recursive schema parity complete for all lanes.
5. `DLB-GATE-04`: type and advanced object family parity complete for all lanes.
6. `DLB-GATE-05`: error/resilience/pooling parity complete for all lanes.
7. `DLB-GATE-06`: `T30-I` and `T30-J` conformance complete for all lanes.
8. `DLB-GATE-07`: final promotion decision complete (`MET` or approved `EXCEEDS` only).

## 7) Completion Definition
Program completion requires:
1. All tracker rows in `DRIVER_LANGUAGE_IMPLEMENTATION_TRACKER_2026-03-03.tsv` at `DONE`.
2. No unapproved `PARTIAL` or `MISSING` requirement status.
3. All gates `DLB-GATE-00..07` at `PASS`.
