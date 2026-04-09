# P21 JDBC Gate-06 Dependency Note (Carry-Forward from Gate-05)

Status date: `2026-03-03`  
Source gate: `P21-JDBC-GATE-05`  
Target gate: `P21-JDBC-GATE-06` (full compatibility-phase parity closure)
Execution checklist: `LD-013/P21_JDBC_GATE_06_EXECUTION_CHECKLIST_2026-03-03.md`

## Decision

Gate-05 remains `IN_PROGRESS`. Deferred parity lanes are intentionally carried into Gate-06 where full compatibility execution is scheduled.

## Deferred Lane Carry-Forward

| Carry ID | Gate-05 Item | Deferred Scope | Owner | Current Control (dated evidence) | Gate-06 Closure Requirement |
|---|---|---|---|---|---|
| G06-D01 | `G05-C03` Dual-path parity | Execute JDBC escape/nativeSQL path vs direct canonical SQL path parity for result shape and deterministic error behavior. | JDBC compatibility lane | `2026-03-03`: mandatory-row traceability/test bundle (`JDBC_SQL_PROMOTION_TRACEABILITY.csv`, `JDBC_SQL_PROMOTION_TEST_RESULTS.md`) | Record parity matrix and command evidence for mandatory rows with no unresolved drift. |
| G06-D02 | `G05-C04` Integrated positioned mutation parity | Live integrated runtime parity for `WHERE CURRENT OF` on complex joins; enforce SQLSTATE `34000` contract on missing/closed cursor cases. | JDBC + engine parity lane | `2026-03-03`: unit-path controls in `SBStatementPositionedMutationTest` and checklist defer record | Add integrated-run evidence and deterministic SQLSTATE assertions for deferred join lanes. |
| G06-D03 | `G05-C07` `nativeSQL` conversion-execution parity | Validate `SBConnection.nativeSQL` output is functionally equivalent to execution semantics across mandatory escape families/profile lanes. | JDBC parser lane | `2026-03-03`: `SBConnection.nativeSQL` mapping + `SBSQLParserTest` conversion coverage and checklist defer record | Provide conversion-vs-execution parity evidence matrix and resolve any functional remap drift. |

## Progress Snapshot (`2026-03-03`)

- `G06-D01`: closed (`G06-CMD-01` PASS, `G06-CMD-02` PASS) with dated logs in `LD-013/G06_command_log_01_20260303.md` and `LD-013/G06_command_log_02_20260303.md`.
- `G06-D02`: closed (`G06-CMD-03` PASS) with dated log in `LD-013/G06_command_log_03_20260303.md`.
- `G06-D03`: closed (`G06-CMD-04` PASS) with dated log in `LD-013/G06_command_log_04_20260303.md`.

## Risk Linkage

- `G05-R2` is carried as deferred risk control under `G06-D02`.
- `G05-R4` is carried as deferred risk control under `G06-D03`.

## Acceptance Trigger

When `G06-D01..G06-D03` are closed with dated evidence, update:

- `LD-013/P21_JDBC_GATE_05_COMPLETION_CHECKLIST_2026-03-03.md` (`G05-C03`, `G05-C04`, `G05-C07`, `G05-C10`)
- `LD-013/P21_JDBC_GATE_ROLLUP.md` (Gate-05 posture transition decision)
- `LD-013/P21_JDBC_GATE_06_EXECUTION_CHECKLIST_2026-03-03.md` (mark execution checklist complete and archive command/evidence ledger)
