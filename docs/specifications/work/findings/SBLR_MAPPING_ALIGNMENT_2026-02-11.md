# SBLR Mapping Alignment - 2026-02-11

## Scope
Alignment pass between:
- `21_V3_Dialect_Surface` language definitions and feature matrix
- `22_SBLR_Canonical_Model_and_Opcodes` opcode/payload mapping and verifier contracts

## Changes Applied

### 1. Missing text-search feature key closed
Added canonical feature row for drop-configuration:
- `F_TEXTSEARCH_DROP_CONFIGURATION`

Files updated:
- `docs/specifications/21_V3_Dialect_Surface/NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md`
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_FEATURE_TO_OPCODE_MATRIX.md`

### 2. Transaction alias metadata made explicit in SBLR payload
Added deterministic metadata fields to transaction payload:
- `txn_alias_mode`
- `txn_alias_name`

File updated:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_STATEMENT_PAYLOAD_SCHEMAS.md`

### 3. Routine/type/security DDL payload requirements made explicit
Added/required:
- `type_payload` for `create_type`
- `routine_payload` for `create_function|create_procedure|create_package|create_trigger`
- `security_payload` mandatory for `grant|revoke`

Added struct definitions:
- `TYPE_PAYLOAD`
- `ROUTINE_PAYLOAD`

File updated:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_STATEMENT_PAYLOAD_SCHEMAS.md`

### 4. Emission and verifier rules tightened
Emission rules now explicitly cover:
- transaction alias rewrite metadata emission
- routine/type/security payload requirements
- text-search drop-configuration action mapping

Verifier now enforces:
- `F_TEXTSEARCH_DROP_CONFIGURATION` mapping
- transaction alias metadata constraints
- required DDL payload presence for type/routine/security
- text-search drop-configuration action/field constraints

Files updated:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/AST_TO_SBLR_EMISSION_RULES.md`
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_VERIFIER_AND_VALIDATION_RULES.md`

### 5. Test contract expanded
Added test clauses for:
- required type/routine/security payload presence
- text-search drop-configuration payload validation
- transaction alias metadata conformance
- section-21/22 mapping conformance for new feature

File updated:
- `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/TEST_CONTRACT.md`

## Validation
Feature parity check:
- section 21 feature keys: `156`
- section 22 feature keys: `156`
- missing in section 22: `0`
- extra in section 22: `0`

Canonical placeholder scan (`TBD/TODO/XXX/FIXME`) in section 21 and 22 (excluding legacy trees): clean.

README index sync executed:
- `docs/specifications/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`

## Next Continuation Target
- Close section-28 capability-profile explicit coverage gaps against the now-expanded `156` feature-key set, while preserving cross-product default/reject semantics.
