# Test Results

- ticket_id: LD-009
- status: PASS
- summary: TSQL alias rewrite and rejection-mode matrix completed
- deterministic_failures: 0
- rewrite_rows: 10
- rejection_rows: 12
- alias_mode_disabled_rejects: 5
- savepoint_resolution_cases: 4

## Validation Commands
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-009/TSQL_ALIAS_REWRITE_RESULTS.csv | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-009/TSQL_ALIAS_REJECTION_RESULTS.csv | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-009/TSQL_ALIAS_REJECTION_RESULTS.csv | rg ',false,.*REJECT,' | wc -l
- rg -n 'TSQL Compatibility Alias Rewrites|E_ALIAS_MODE_DISABLED|E_TXN_SAVEPOINT_NOT_FOUND' docs/specifications/21_V3_Dialect_Surface/NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md

## Pass Criteria Evaluation
- Alias rewrite behavior for BEGIN/COMMIT/ROLLBACK covered: PASS
- Alias disabled deterministic rejection behavior covered: PASS
- Savepoint resolution and ambiguity rejection paths covered: PASS
- Canonical feature key and result-shape mapping attached per accepted row: PASS
