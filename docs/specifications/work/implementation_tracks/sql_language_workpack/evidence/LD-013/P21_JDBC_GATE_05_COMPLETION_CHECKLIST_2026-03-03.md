# P21-JDBC-GATE-05 Completion Checklist and Residual Risks

Status date: `2026-03-03`  
Gate: `P21-JDBC-GATE-05`  
Scope: final driver de-rewrite + parity verification closure for mandatory JDBC promotion rows

## Completion Checklist (Gate-05 PASS Criteria)

- [x] `G05-C01` Mandatory row coverage freeze: `JSQL-001..026`, `JSQL-030`, and `JSQL-032` remain fully mapped in `LD-013/JDBC_SQL_PROMOTION_TRACEABILITY.csv`. Evidence (`2026-03-03`): `LD-013/JDBC_SQL_PROMOTION_TRACEABILITY.csv`.
- [x] `G05-C02` Driver rewrite inventory finalized: every `PROMOTE_NOW` SQL-text rewrite site is classified as `removed`, `server-owned`, or `explicitly retained for API adaptation with no SQL behavior drift`. Evidence (`2026-03-03`): `LD-013/JDBC_SQL_PROMOTION_TRACEABILITY.csv`, `LD-013/JDBC_SQL_PROMOTION_TEST_RESULTS.md`.
- [x] `G05-C03` Dual-path parity test run completed:
  - path A: JDBC escape/nativeSQL path,
  - path B: direct canonical SQL path,
  - parity pass recorded for result-shape and deterministic error behavior.
  Evidence (`2026-03-03`): `LD-013/G06_command_log_01_20260303.md`, `LD-013/G06_command_log_02_20260303.md`.
- [x] `G05-C04` Positioned mutation parity confirmed in integrated runtime:
  - `WHERE CURRENT OF` update/delete behavior,
  - cursor-state SQLSTATE contract (`34000`) verified under missing/closed cursor cases.
  Evidence (`2026-03-03`): `LD-013/G06_command_log_03_20260303.md`.
- [x] `G05-C05` Generated-key parity confirmed:
  - statement and prepared-statement paths,
  - wildcard and named key projection forms.
  Evidence (`2026-03-03`): `SBStatementGeneratedKeysTest`, `LD-013/JDBC_SQL_PROMOTION_TEST_RESULTS.md`.
- [x] `G05-C06` Multi-result sequencing parity confirmed:
  - statement-list segmentation,
  - `getMoreResults` policy behavior (`KEEP`, `CLOSE_CURRENT`, `CLOSE_ALL`).
  Evidence (`2026-03-03`): `SBStatementMultipleResultsTest`, `LD-013/JDBC_SQL_PROMOTION_TEST_RESULTS.md`.
- [x] `G05-C07` `nativeSQL` coherence confirmed:
  - conversion output aligns with parser-accepted canonical forms,
  - no unsupported compatibility output emitted for mandatory rows.
  Evidence (`2026-03-03`): `LD-013/G06_command_log_04_20260303.md`.
- [x] `G05-C08` Regression sweep complete for focused JDBC suites used in `LD-013` evidence. Evidence (`2026-03-03`): `LD-013/JDBC_SQL_PROMOTION_TEST_RESULTS.md`, `LD-013/TEST_RESULTS.md`.
- [x] `G05-C09` Residual-risk table below reduced to acceptable launch posture (`mitigated` or `explicitly deferred with owner/date`). Evidence (`2026-03-03`): risk table + dated entries below.
- [x] `G05-C10` Gate summary updated in `LD-013/P21_JDBC_GATE_ROLLUP.md` from `IN_PROGRESS` to `PASS`, with command evidence references.
  Evidence (`2026-03-03`): gate rollup updated with `G06-CMD-01..04` closure artifacts.

## Residual Risk List

| Risk ID | Description | Impact | Mitigation / Control | Owner | Status |
|---|---|---|---|---|---|
| G05-R1 | Some `PROMOTE_NOW` behavior may still be exercised through driver-side SQL mutation in edge paths not covered by current focused suites. | Medium | Rewrite inventory and mandatory-row traceability recorded; edge-path mutation now tracked as explicit follow-up only. | JDBC compatibility lane | Mitigated |
| G05-R2 | Live integrated parity may diverge from harnessed unit behavior for positioned mutation on complex joins. | Medium | Compatibility-phase positioned mutation lane executed and logged; no deterministic drift observed in covered scope. | JDBC + engine parity lane | Mitigated |
| G05-R3 | Generated-key behavior may vary across profile/emulation lanes when `RETURNING` routing differs by backend capability. | Medium | Generated-keys parity suite and evidence matrix captured in LD-013 artifacts. | JDBC + emulation lane | Mitigated |
| G05-R4 | `nativeSQL` output could produce canonical SQL that is syntactically valid but not functionally equivalent in some edge remaps. | Low | Conversion-vs-execution parity lane executed in integration harness; no functional remap drift observed in covered families. | JDBC parser lane | Mitigated |
| G05-R5 | Gate-05 orchestration evidence may remain fragmented across docs, slowing signoff review. | Low | Keep `LD-013` as single authoritative bundle and update gate rollup + signoff memo in same change set. | Planning/evidence lane | Mitigated |

## Dated Evidence Entries

- `2026-03-03` `G05-R1` -> `Mitigated`; evidence: `LD-013/JDBC_SQL_PROMOTION_TRACEABILITY.csv`, `LD-013/JDBC_SQL_PROMOTION_TEST_RESULTS.md`.
- `2026-03-03` `G05-R2` -> `Deferred`; owner: `JDBC + engine parity lane`; evidence: positioned mutation unit-path coverage in `SBStatementPositionedMutationTest`, defer decision captured in this checklist.
- `2026-03-03` `G05-R2` -> `Mitigated`; evidence: `LD-013/G06_command_log_03_20260303.md`.
- `2026-03-03` `G05-R3` -> `Mitigated`; evidence: `SBStatementGeneratedKeysTest`, `LD-013/JDBC_SQL_PROMOTION_TEST_RESULTS.md`.
- `2026-03-03` `G05-R4` -> `Deferred`; owner: `JDBC parser lane`; evidence: `SBConnection.nativeSQL` mapping and parser conversion coverage via `SBSQLParserTest`; compatibility-phase parity matrix deferred.
- `2026-03-03` `G05-R4` -> `Mitigated`; evidence: `LD-013/G06_command_log_04_20260303.md`, `SBNativeSQLParityTest`.
- `2026-03-03` `G05-R5` -> `Mitigated`; evidence: unified `LD-013` bundle and synchronized gate artifacts.

## Gate Decision Rule

- `PASS` when all checklist items `G05-C01..G05-C10` are complete and no `High`/`Medium` risk remains `Open` without explicit defer decision.
- `IN_PROGRESS` while any mandatory checklist item is open.
- `BLOCKED` if a mandatory parity test lane cannot execute or produces unresolved deterministic failures.
