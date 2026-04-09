# Test Results

- ticket_id: LD-007
- status: PASS
- summary: normalization corpus and clause-order negative case matrix completed
- deterministic_failures: 0
- normalization_rows: 28
- normalization_accept_or_rewrite: 11
- normalization_reject: 17
- clause_negative_rows: 30

## Validation Commands
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-007/NORMALIZATION_CORPUS_RESULTS.csv | rg ',REJECT,' | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-007/CLAUSE_ORDER_NEGATIVE_CASES.csv | wc -l
- rg -n '## Global Deterministic Parse Pipeline|## Global Rejection Codes' docs/specifications/21_V3_Dialect_Surface/NATIVE_PARSER_NORMALIZATION_AND_REJECTION_MATRIX.md

## Pass Criteria Evaluation
- Transform-order contract captured with rewrite and reject paths: PASS
- Clause-order and exclusivity negatives covered across families: PASS
- Admin and transaction alias gating captured: PASS
