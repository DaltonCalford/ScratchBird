# P21 JDBC Gate Rollup (Mandatory Promotion Rows)

Status date: `2026-03-03`  
Scope: `JSQL-001..026`, `JSQL-030`, `JSQL-032`

## Gate Mapping

### `P21-JDBC-GATE-01` Parser Normalization Closure
- Rows: `JSQL-001..015`, `JSQL-032`
- Evidence:
  - `./gradlew test --tests com.scratchbird.jdbc.SBSQLParserTest`
  - `./gradlew test --tests com.scratchbird.jdbc.SBCallableStatementEscapeSyntaxTest --tests com.scratchbird.jdbc.SBSQLParserTest`
  - `./gradlew test --tests com.scratchbird.jdbc.SBPreparedStatementNamedParameterAliasTest`
- Result: PASS

### `P21-JDBC-GATE-02` Positioned Mutation / Cursor Lineage Closure
- Rows: `JSQL-016..021`, `JSQL-030`
- Evidence:
  - `./gradlew test --tests com.scratchbird.jdbc.SBStatementPositionedMutationTest --tests com.scratchbird.jdbc.SBResultSetCursorNameTest`
  - `./gradlew test --tests com.scratchbird.jdbc.SBResultSetUpdatableTest`
- Result: PASS

### `P21-JDBC-GATE-03` Multi-Statement / Multi-Result Closure
- Rows: `JSQL-022`, `JSQL-023`
- Evidence:
  - `./gradlew test --tests com.scratchbird.jdbc.SBStatementMultipleResultsTest --tests com.scratchbird.jdbc.SBStatementGeneratedKeysTest`
- Result: PASS

### `P21-JDBC-GATE-04` Generated-Key Mapping Closure
- Rows: `JSQL-024`, `JSQL-025`, `JSQL-026`
- Evidence:
  - `./gradlew test --tests com.scratchbird.jdbc.SBStatementMultipleResultsTest --tests com.scratchbird.jdbc.SBStatementGeneratedKeysTest`
- Result: PASS

### `P21-JDBC-GATE-05` Driver De-Rewrite + Parity Verification
- Rows: aggregate signoff over mandatory rows in this scope
- Evidence basis:
  - `LD-013/JDBC_SQL_PROMOTION_TRACEABILITY.csv`
  - `LD-013/JDBC_SQL_PROMOTION_TEST_RESULTS.md`
  - `LD-013/P21_JDBC_GATE_05_COMPLETION_CHECKLIST_2026-03-03.md`
  - `LD-013/P21_JDBC_GATE_06_DEPENDENCY_NOTE_2026-03-03.md`
  - `LD-013/P21_JDBC_GATE_06_EXECUTION_CHECKLIST_2026-03-03.md`
  - `LD-013/G06_command_log_01_20260303.md`
  - `LD-013/G06_command_log_02_20260303.md`
  - `LD-013/G06_command_log_03_20260303.md`
  - `LD-013/G06_command_log_04_20260303.md`
- Current posture: PASS (deferred parity lanes executed and closed via Gate-06 command matrix with dated evidence)

### `P21-JDBC-GATE-06` Compatibility-Phase Deferred Parity Closure
- Rows: carry-forward closure for `G06-D01..G06-D03`
- Evidence basis:
  - `LD-013/P21_JDBC_GATE_06_DEPENDENCY_NOTE_2026-03-03.md`
  - `LD-013/P21_JDBC_GATE_06_EXECUTION_CHECKLIST_2026-03-03.md`
  - `LD-013/G06_command_log_01_20260303.md`
  - `LD-013/G06_command_log_02_20260303.md`
  - `LD-013/G06_command_log_03_20260303.md`
  - `LD-013/G06_command_log_04_20260303.md`
- Current posture: PASS (`G06-CMD-01..04` passed on `2026-03-03`; carry-forward items `G06-D01..D03` are closed)
