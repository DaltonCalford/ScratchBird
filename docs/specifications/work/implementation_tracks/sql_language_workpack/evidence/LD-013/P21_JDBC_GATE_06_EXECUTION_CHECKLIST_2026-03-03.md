# P21-JDBC-GATE-06 Execution Checklist (Deferred Parity Closure)

Status date: `2026-03-03`  
Gate: `P21-JDBC-GATE-06`  
Source: carry-forward lanes from `LD-013/P21_JDBC_GATE_06_DEPENDENCY_NOTE_2026-03-03.md`

## Scope

- `G06-D01` dual-path parity (`nativeSQL`/escape path vs canonical SQL path).
- `G06-D02` integrated positioned mutation parity (`WHERE CURRENT OF` + SQLSTATE `34000`).
- `G06-D03` `nativeSQL` conversion-vs-execution functional parity.

## Execution Preconditions

- Run from JDBC driver root: `ScratchBird-driver/tracks/alpha/drivers/jdbc`.
- Keep command output logs dated and attached to `LD-013` evidence set.
- Any failure must include deterministic repro command + SQLSTATE/error-shape note.

## Checklist

- [x] `G06-C01` Execute dual-path parity command lane and record deterministic result-shape parity evidence. Evidence (`2026-03-03`): `LD-013/G06_command_log_01_20260303.md`, `LD-013/G06_command_log_02_20260303.md`.
- [x] `G06-C02` Execute integrated positioned-mutation lane and record SQLSTATE `34000` behavior on missing/closed cursor paths. Evidence (`2026-03-03`): `LD-013/G06_command_log_03_20260303.md`.
- [x] `G06-C03` Execute `nativeSQL` conversion-vs-execution parity lane for mandatory escape families. Evidence (`2026-03-03`): `LD-013/G06_command_log_04_20260303.md`.
- [x] `G06-C04` Update Gate-05 deferred checklist items (`G05-C03`, `G05-C04`, `G05-C07`) to complete with dated evidence.
- [x] `G06-C05` Update gate rollup posture decision and close carry items (`G06-D01..D03`).

## Command Matrix (Execute + Capture)

| Cmd ID | Carry Item | Command | Expected Gate Assertion | Evidence Artifact |
|---|---|---|---|---|
| G06-CMD-01 | `G06-D01` | `./gradlew test --tests com.scratchbird.jdbc.SBSQLParserTest --tests com.scratchbird.jdbc.SBCallableStatementEscapeSyntaxTest --tests com.scratchbird.jdbc.SBPreparedStatementNamedParameterAliasTest` | Escape/native parsing and canonicalization remain stable with no regression on mandatory rows. | `LD-013/G06_command_log_01_20260303.md` |
| G06-CMD-02 | `G06-D01` | `./gradlew test --tests com.scratchbird.jdbc.SBStatementGeneratedKeysTest --tests com.scratchbird.jdbc.SBStatementMultipleResultsTest` | Canonical execution path result-shape contracts remain stable for multi-result/generated-key mandatory rows. | `LD-013/G06_command_log_02_20260303.md` |
| G06-CMD-03 | `G06-D02` | `./gradlew test --tests com.scratchbird.jdbc.SBStatementPositionedMutationTest --tests com.scratchbird.jdbc.SBResultSetCursorNameTest --tests com.scratchbird.jdbc.SBResultSetUpdatableTest` | Positioned mutation contract stable; missing/closed cursor behavior remains deterministic (`34000`). | `LD-013/G06_command_log_03_20260303.md` |
| G06-CMD-04 | `G06-D03` | `./gradlew test --tests com.scratchbird.jdbc.SBNativeSQLParityTest` | Conversion-vs-execution parity proven across mandatory escape families; no unresolved functional remap drift. | `LD-013/G06_command_log_04_20260303.md` |

## Dated Evidence Entries (Executed)

- Date: `2026-03-03`
  Cmd ID: `G06-CMD-01`
  Carry Item: `G06-D01`
  Exact Command: `./gradlew test --tests com.scratchbird.jdbc.SBSQLParserTest --tests com.scratchbird.jdbc.SBCallableStatementEscapeSyntaxTest --tests com.scratchbird.jdbc.SBPreparedStatementNamedParameterAliasTest`
  Result: `PASS`
  Deterministic Assertions:
  - escape/native parsing and canonicalization suites passed with no failures
  - no deterministic error-shape drift observed
  Artifact: `LD-013/G06_command_log_01_20260303.md`
  Follow-up: `none`

- Date: `2026-03-03`
  Cmd ID: `G06-CMD-02`
  Carry Item: `G06-D01`
  Exact Command: `./gradlew test --tests com.scratchbird.jdbc.SBStatementGeneratedKeysTest --tests com.scratchbird.jdbc.SBStatementMultipleResultsTest`
  Result: `PASS`
  Deterministic Assertions:
  - generated-key and multi-result mandatory suites passed with no failures
  - no deterministic error-shape drift observed
  Artifact: `LD-013/G06_command_log_02_20260303.md`
  Follow-up: `none`

- Date: `2026-03-03`
  Cmd ID: `G06-CMD-03`
  Carry Item: `G06-D02`
  Exact Command: `./gradlew test --tests com.scratchbird.jdbc.SBStatementPositionedMutationTest --tests com.scratchbird.jdbc.SBResultSetCursorNameTest --tests com.scratchbird.jdbc.SBResultSetUpdatableTest`
  Result: `PASS`
  Deterministic Assertions:
  - positioned mutation/cursor lineage suites passed with no failures
  - cursor-state deterministic contract remained stable in covered lanes
  Artifact: `LD-013/G06_command_log_03_20260303.md`
  Follow-up: `none`

- Date: `2026-03-03`
  Cmd ID: `G06-CMD-04`
  Carry Item: `G06-D03`
  Exact Command: `./gradlew test --tests com.scratchbird.jdbc.SBNativeSQLParityTest`
  Result: `PASS`
  Deterministic Assertions:
  - `nativeSQL` conversion output executed with parity against direct canonical SQL in integration coverage
  - no deterministic conversion-vs-execution drift observed in covered escape families
  Artifact: `LD-013/G06_command_log_04_20260303.md`
  Follow-up: `none`

## Dated Evidence Entry Template

Use one entry per completed command:

```text
- Date: YYYY-MM-DD
  Cmd ID: G06-CMD-XX
  Carry Item: G06-D0X
  Exact Command: <paste exact command>
  Result: PASS | FAIL
  Deterministic Assertions:
    - <assertion 1>
    - <assertion 2>
  Artifact: LD-013/G06_command_log_XX_YYYYMMDD.md
  Follow-up: none | <required action>
```

## Deferred-to-Closed Transition Rule

- `G06-D01..G06-D03` can be marked closed only when all related command entries are `PASS` with dated evidence.
- After closure, update:
  - `LD-013/P21_JDBC_GATE_05_COMPLETION_CHECKLIST_2026-03-03.md`
  - `LD-013/P21_JDBC_GATE_ROLLUP.md`
