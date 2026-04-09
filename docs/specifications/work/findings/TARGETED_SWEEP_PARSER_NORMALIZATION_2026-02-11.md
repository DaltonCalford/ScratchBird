# Targeted Sweep - Parser Normalization (2026-02-11)

## Scope
- Section 21 native parser language surfaces:
  - DDL
  - DML
  - PSQL and transaction SQL
  - Admin and infrastructure SQL

## Objective
Define strict, low-ambiguity parser implementation rules so a low-capability agent can implement clause ordering, conflict rejection, and alias rewrites without inference.

## Changes Applied
1. Added canonical normalization and rejection matrix:
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_PARSER_NORMALIZATION_AND_REJECTION_MATRIX.md`

2. Wired new matrix into section entry points and outline:
- `docs/specifications/21_V3_Dialect_Surface/README.md`
- `docs/specifications/21_V3_Dialect_Surface/SPEC_OUTLINE.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_SQL_SURFACE.md`

3. Hardened language docs with explicit cross-reference to normalization matrix:
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_DDL_LANGUAGE_DEFINITION.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_DML_LANGUAGE_DEFINITION.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md`
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_ADMIN_LANGUAGE_DEFINITION.md`

4. Extended test contract for clause-order and conflict-rejection behavior:
- `docs/specifications/21_V3_Dialect_Surface/TEST_CONTRACT.md`

## Normalization Matrix Coverage
- Global parse pipeline and rejection codes.
- DDL clause-order contracts:
  - database, emulated database, table, index, domain, view/materialized view, routine, trigger, policy.
- DML clause-order contracts:
  - select, insert, update, delete, merge, copy, prepared, notify.
- PSQL and transaction contracts:
  - alias rewrite rules, scope and closure checks, deterministic conflicts.
- Admin/infrastructure contracts:
  - config, diagnostic scans, cluster and control-plane command constraints, service-channel forms.

## Validation Results
1. Section 21 placeholder/ambiguity scan:
- no canonical hits for `TODO/FIXME/XXX/TBD` or ambiguous phrases (`if needed`, `as needed`, `to be defined`).
2. Section 21 vs section 22 feature parity:
- exact match (`156` features each).
3. Section 21 vs section 28 capability decision CSV parity:
- exact match (no missing/extra feature keys).
4. Section README indexes re-synced:
- `docs/specifications/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`
