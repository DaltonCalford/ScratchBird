# Targeted Sweep - Canonical Clarity (2026-02-11)

## Scope
- Canonical specifications under `docs/specifications/00_*` through `docs/specifications/31_*`.
- Excluded `legacy_imports` and `source_copies`.
- Focused on implementation clarity, internal consistency, and low-capability AI implementability.

## Validation Method
1. Placeholder and ambiguity scans:
- `XXX`, `FIXME`, `TODO`, `TBD`, `to be defined`, `if needed`, `as needed`.
2. Cross-section parity checks:
- section 21 feature keys vs section 28 capability decision projection CSV.
- section 22 family/result-shape mapping vs section 28 capability decision projection CSV.
3. Determinism checks:
- precedence rank uniqueness and monotonic family ordering in section 28 decision projection CSV.

## Findings

### F-001 (Resolved): Missing feature row in capability decision projection
- File: `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_DECISION_TABLE.csv`
- Issue: `F_MONGO_FIND` existed in section 21 and section 22 but was missing from the CSV projection.
- Impact: Drift between canonical feature matrix and parser capability projection.
- Resolution:
  - Added `F_MONGO_FIND`.
  - Shifted `F_MONGO_FIND_AND_MODIFY` precedence from `10003` to `10004` to preserve deterministic order.

### F-002 (Resolved): Ambiguous transaction rule text
- File: `docs/specifications/08_Transaction_Core/SPEC_OUTLINE.md`
- Issue: wording used `Update OAT if needed.` which leaves decision logic implicit.
- Resolution:
  - Replaced with explicit condition:
  - update OAT only when the current OAT transaction reaches terminal state and forward scan identifies next oldest active transaction.

### F-003 (Resolved): Ambiguous catalog consolidation note
- File: `docs/specifications/24_Catalog_Model_and_Virtual_Overlays/EMULATED_CATALOG_GAP_CONSOLIDATION.md`
- Issue: wording used `Can be split if needed` for `sb_table_constraint`.
- Resolution:
  - Replaced with explicit Alpha rule:
  - single canonical table in Alpha; split only post-Alpha.

## Consistency Results
1. Section 21 -> section 28 feature parity:
- exact match after fix (`156` features).
2. Section 22 -> section 28 family/result-shape parity:
- exact match (`OK`).
3. Precedence integrity in decision projection CSV:
- no duplicate `precedence_rank`;
- ascending within family (`OK`).
4. Canonical placeholder sweep:
- no `XXX` in canonical docs.
- remaining `TODO` hit is a legacy-link filename in section 28 README and not a canonical requirement placeholder.
- `todo` hit in full-text stopwords data is lexical data (`todo` as language token), not a work marker.

## Additional Guardrails Added
- `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_BUILD_ALGORITHM.md`
  - now requires decision projection row count parity with feature count.
  - now requires section-21 <-> CSV key parity.
- `docs/specifications/28_Parser_Implementations/TEST_CONTRACT.md`
  - added explicit tests `C-011`, `C-012`, `C-013` for CSV cardinality and key parity.
- `docs/specifications/28_Parser_Implementations/README.md`
  - added `CAPABILITY_PROFILE_DECISION_TABLE.csv` to section entry points.
- `docs/specifications/28_Parser_Implementations/SPEC_OUTLINE.md`
  - added decision projection CSV to required deliverables.

## Readiness Statement
- For the reviewed canonical scope, no unresolved ambiguity was found that blocks starting admin/DDL/DML/PSQL/TSQL language definition.
