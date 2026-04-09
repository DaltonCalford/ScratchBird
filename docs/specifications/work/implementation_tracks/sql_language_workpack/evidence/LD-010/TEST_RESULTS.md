# Test Results

- ticket_id: LD-010
- status: PASS
- summary: UUID binding and feature-key/result-shape audit completed
- deterministic_failures: 0
- audit_rows: 26
- accept_rows: 17
- reject_rows: 9
- families_covered: 7

## Validation Commands
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-010/UUID_BIND_AND_FEATURE_KEY_AUDIT.csv | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-010/UUID_BIND_AND_FEATURE_KEY_AUDIT.csv | rg ',ACCEPT,' | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-010/UUID_BIND_AND_FEATURE_KEY_AUDIT.csv | rg ',REJECT,' | wc -l
- tail -n +2 docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-010/UUID_BIND_AND_FEATURE_KEY_AUDIT.csv | cut -d, -f3 | sort -u | wc -l
- rg -n 'Resolve object names to UUID|result_shape_id|Parent Object UUID Rules|Duplicate child name/type' docs/specifications/21_V3_Dialect_Surface/NATIVE_PARSER_FEATURE_FAMILIES.md docs/specifications/24_Catalog_Model_and_Virtual_Overlays/CATALOG_OBJECT_PARENTAGE_AND_NAME_UNIQUENESS.md

## Pass Criteria Evaluation
- UUID binding contract covers relation, column, and child object parent scopes: PASS
- Feature key and result shape assignment captured per audited statement: PASS
- Deterministic reject paths for ambiguous/missing UUID bindings covered: PASS
- Parent-scope uniqueness and duplicate-child constraints reflected in parser-level outcomes: PASS
