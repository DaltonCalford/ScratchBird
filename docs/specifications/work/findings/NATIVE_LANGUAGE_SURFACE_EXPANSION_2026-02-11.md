# Native Language Surface Expansion - 2026-02-11

## Scope
Expanded canonical native parser language definitions for:
- DDL
- DML
- PSQL and transaction-control SQL aliases
- Admin/control-plane SQL

## Added Canonical Documents
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_DDL_LANGUAGE_DEFINITION.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_DML_LANGUAGE_DEFINITION.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_ADMIN_LANGUAGE_DEFINITION.md`

## Updated Integration Documents
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_SQL_SURFACE.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_PARSER_FEATURE_FAMILIES.md`
- `docs/specifications/21_V3_Dialect_Surface/SPEC_OUTLINE.md`
- `docs/specifications/21_V3_Dialect_Surface/TEST_CONTRACT.md`
- `docs/specifications/21_V3_Dialect_Surface/DEPENDENCIES.md`

## Determinism Improvements
1. Explicit canonical statement forms added for DDL and DML.
2. Deterministic parser/binder algorithm added for DDL and DML.
3. Explicit error-code lists added for DDL, DML, procedural, and admin surfaces.
4. Transaction alias rewrite behavior (`BEGIN/COMMIT/ROLLBACK TRANSACTION`) is explicit and configuration-gated.
5. Admin commands mapped to feature keys and result-shape contracts in one canonical place.

## Validation
- Placeholder scan (`TBD/TODO/XXX/FIXME`) for section-21 canonical docs excluding legacy trees: clean.
- Section README file index synchronized.

## Remaining Work Before Bytecode Mapping
1. Lock section-28 capability profiles against every feature key in section-21 matrix.
2. Produce section-22 payload schemas for all newly expanded parser-side forms.
3. Add parser conformance corpus cases for new PSQL/TSQL alias and admin command grammar edges.
