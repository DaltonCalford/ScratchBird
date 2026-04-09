# Test Results

- ticket_id: LD-013
- status: PASS
- summary: consolidated JDBC SQL promotion evidence matrix for JSQL-001..026 packaged with checklist traceability
- summary: consolidated JDBC SQL promotion evidence matrix for mandatory rows (`JSQL-001..026`, `JSQL-030`, `JSQL-032`) packaged with checklist + gate rollup traceability
- deterministic_failures: 0
- mapped_rows: 28
- mapped_rows_pass: 28
- mapped_rows_fail: 0

## Validation Commands
- `./gradlew test --tests com.scratchbird.jdbc.SBSQLParserTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBCallableStatementEscapeSyntaxTest --tests com.scratchbird.jdbc.SBSQLParserTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBPreparedStatementNamedParameterAliasTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBStatementPositionedMutationTest --tests com.scratchbird.jdbc.SBResultSetCursorNameTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBResultSetUpdatableTest`
- `./gradlew test --tests com.scratchbird.jdbc.SBStatementMultipleResultsTest --tests com.scratchbird.jdbc.SBStatementGeneratedKeysTest`

## Pass Criteria Evaluation
- JSQL mandatory promotion rows 001..026 plus 030/032 have explicit evidence mappings: PASS
- Evidence files required by JDBC promotion checklist are present: PASS
- P21 JDBC gate rollup artifact is present and mapped to evidence commands: PASS
- No unresolved failures in mapped JDBC promotion evidence scope: PASS
