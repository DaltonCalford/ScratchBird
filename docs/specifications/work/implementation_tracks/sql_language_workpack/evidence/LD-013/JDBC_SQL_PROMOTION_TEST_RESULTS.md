# JDBC SQL Promotion Test Results

- ticket_id: LD-013
- scope: JSQL-001..026 plus mandatory JSQL-030 and JSQL-032 consolidated traceability packaging
- status: PASS
- status_date: 2026-03-03
- promote_now_rows_in_scope: 28
- promote_now_rows_pass: 28
- promote_now_rows_fail: 0

## Evidence Artifacts
- `LD-013/JDBC_SQL_PROMOTION_TRACEABILITY.csv`

## Validation Commands
- `./gradlew test --tests com.scratchbird.jdbc.SBSQLParserTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBCallableStatementEscapeSyntaxTest --tests com.scratchbird.jdbc.SBSQLParserTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBPreparedStatementNamedParameterAliasTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBStatementPositionedMutationTest --tests com.scratchbird.jdbc.SBResultSetCursorNameTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBResultSetUpdatableTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBStatementMultipleResultsTest --tests com.scratchbird.jdbc.SBStatementGeneratedKeysTest`

## Pass Criteria Evaluation
- Every mandatory promotion row in scope (`JSQL-001..026`, `JSQL-030`, `JSQL-032`) has explicit test-method evidence mapping: PASS
- Every mapped evidence command executed successfully in current validation tranche: PASS
- No failing rows remain in the consolidated mandatory promotion matrix: PASS
