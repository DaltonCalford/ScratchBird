# Targeted Sweep - SBLR Normalization Alignment (2026-02-11)

## Scope
- Section 22 SBLR canonical docs:
  - payload schemas
  - verifier rules
  - AST emission rules
  - test contract
- Alignment target:
  - `21_V3_Dialect_Surface/NATIVE_PARSER_NORMALIZATION_AND_REJECTION_MATRIX.md`

## Objective
Ensure section-21 parser normalization decisions are carried into section-22 as explicit payload evidence and deterministic verifier rejection rules.

## Changes Applied
1. Added canonical bridge document:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/PARSER_NORMALIZATION_TO_SBLR_VALIDATION_MATRIX.md`

2. Added mandatory normalization evidence fields to statement envelope:
- `normalization_rule_set_id`
- `clause_presence_mask_lo`
- `clause_presence_mask_hi`
- `clause_order_count`
- `clause_order_code[]`
- `alias_rewrite_flags`
- File:
  - `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_STATEMENT_PAYLOAD_SCHEMAS.md`

3. Added family-level normalization constraints in payload schemas:
- FG_TRANSACTION
- FG_SESSION
- FG_DDL_SQL
- FG_DML_SQL
- FG_PREPARED
- FG_NOTIFICATION
- FG_ADMIN

4. Extended verifier with new stable payload errors:
- `SBLR-E-0054` invalid rule-set id
- `SBLR-E-0055` required clause bit missing
- `SBLR-E-0056` clause-order violation
- `SBLR-E-0057` alias-flag conflict
- `SBLR-E-0058` unknown clause code
- File:
  - `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_VERIFIER_AND_VALIDATION_RULES.md`

5. Extended emitter algorithm to construct normalization evidence:
- Added deterministic step for rule-set id, clause masks, clause order vector, alias flags.
- File:
  - `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/AST_TO_SBLR_EMISSION_RULES.md`

6. Extended section-22 test contract:
- Added tests `T22-C09` through `T22-C14` for normalization evidence and error-code behavior.
- Added cross-section conformance test `T22-H06`.
- File:
  - `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/TEST_CONTRACT.md`

7. Updated section index/outline/dependencies:
- `README.md`
- `SPEC_OUTLINE.md`
- `DEPENDENCIES.md`

## Validation Results
1. Section-22 ambiguity/placeholder scan:
- no canonical hits for `TODO/FIXME/XXX/TBD` or ambiguous phrases.
2. Section-21 and section-22 feature matrix counts:
- `156` and `156`.
3. Section-21 and section-28 capability decision table parity:
- exact match; no missing/extra feature keys.
4. README indexes synced:
- `docs/specifications/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`

## Result
Section 22 now explicitly enforces parser normalization provenance and deterministic clause-order/alias validation, closing the previous gap between parser grammar normalization and SBLR verifier behavior.
