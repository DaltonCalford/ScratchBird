# IMP-21 Implementation Checklist

## Ticket
- ID: IMP-21
- Section: 21_V3_Dialect_Surface
- Gate Contract: docs/specifications/21_V3_Dialect_Surface/TEST_CONTRACT.md

## Inputs
- docs/specifications/21_V3_Dialect_Surface/SPEC_OUTLINE.md
- docs/specifications/21_V3_Dialect_Surface/NATIVE_*_LANGUAGE_DEFINITION.md
- docs/specifications/21_V3_Dialect_Surface/NATIVE_PARSER_NORMALIZATION_AND_REJECTION_MATRIX.md
- docs/specifications/21_V3_Dialect_Surface/NORMATIVE_ADMIN_DDL_DML_PSQL_TSQL_IMPLEMENTATION_CHECKLIST.md
- docs/specifications/work/implementation_tracks/sql_language_workpack/evidence/LD-001..LD-012/*

## Ordered Tasks
1. Bind canonical DDL/DML/PSQL/admin grammar contracts to deterministic acceptance/rejection evidence.
2. Bind feature-key and result-shape assignments to deterministic UUID/shape audits.
3. Bind normalization/rejection and clause-order conflict contracts to deterministic corpus evidence.
4. Bind listener/storage/blob-filter/remote/fabric SQL contracts to deterministic feature-key surfaces.
5. Bind system column visibility, config/resource bundle SQL, operator AST, and alias policies to deterministic traces.
6. Implement required, negative, performance, and compatibility test suites and evidence capture.

## Exit Criteria
- Required tests pass.
- Gate result is pass.
- Traceability maps requirements to deterministic artifacts.
