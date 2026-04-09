# Targeted Sweep - Language Consistency (2026-02-11)

## Scope
- `docs/specifications/21_V3_Dialect_Surface/`
- Consistency checks against:
  - `docs/specifications/22_SBLR_Canonical_Model_and_Opcodes/SBLR_FEATURE_TO_OPCODE_MATRIX.md`
  - `docs/specifications/28_Parser_Implementations/CAPABILITY_PROFILE_DECISION_TABLE.csv`

## Objective
Remove ambiguity and contradictions before admin/DDL/DML/PSQL/TSQL implementation starts, so low-capability agents can implement deterministically without external interpretation.

## Resolved Findings

### L-001: `required_engines` ambiguity could imply native exclusion
- File: `docs/specifications/21_V3_Dialect_Surface/NATIVE_SUPERSET_COMPATIBILITY_MATRIX.md`
- Problem:
  - `required_engines` could be misread as excluding native when `native` is not listed.
- Fix:
  - Added explicit contract that native parser is mandatory superset for all `required` rows.

### L-002: Missing explicit transaction and session feature mappings in language definition
- File: `docs/specifications/21_V3_Dialect_Surface/NATIVE_PSQL_TSQL_LANGUAGE_DEFINITION.md`
- Problem:
  - Transaction and session statement forms existed, but explicit feature key mapping was incomplete.
- Fix:
  - Added feature key list for transaction controls:
    - `F_TXN_BEGIN`, `F_TXN_COMMIT`, `F_TXN_ROLLBACK`, `F_TXN_SAVEPOINT`
  - Added canonical session-control SQL forms with explicit mapping:
    - `F_SESSION_SET`, `F_SESSION_SHOW`, `F_SESSION_RESET`

### L-003: Missing explicit prepared and notification feature mappings
- File: `docs/specifications/21_V3_Dialect_Surface/NATIVE_DML_LANGUAGE_DEFINITION.md`
- Problem:
  - `PREPARE/EXECUTE/DEALLOCATE` and `NOTIFY/LISTEN/UNLISTEN` behavior lacked explicit feature-key sections.
- Fix:
  - Added canonical prepared statement section:
    - `F_PREPARE_STATEMENT`, `F_EXECUTE_PREPARED`, `F_DEALLOCATE_PREPARED`
  - Added canonical notification section:
    - `F_NOTIFY_PUBLISH`, `F_NOTIFY_SUBSCRIBE`, `F_NOTIFY_UNSUBSCRIBE`

### L-004: Index scan canonical syntax mismatch
- File: `docs/specifications/21_V3_Dialect_Surface/NATIVE_ADMIN_LANGUAGE_DEFINITION.md`
- Problem:
  - Admin forms used `SCAN INDEX ... LIGHT|DIAGNOSTIC` while matrix canonical form is `ALTER INDEX ... LIGHT|DIAGNOSTIC SCAN`.
- Fix:
  - Canonicalized forms to `ALTER INDEX ... LIGHT SCAN` and `ALTER INDEX ... DIAGNOSTIC SCAN`.
  - Added explicit alias rewrite policy for `SCAN INDEX ...` with config gate:
    - accepted only when `dialect.native.admin_scan_aliases_enabled=true`
    - otherwise deterministic reject `UNSUPPORTED_IN_DIALECT`
  - Added explicit rewrite mapping for `SCAN PAGES LIGHT|DIAGNOSTIC` to `F_ADMIN_VALIDATE`.

### L-005: Text-search statements lacked explicit feature-key mapping
- File: `docs/specifications/21_V3_Dialect_Surface/NATIVE_INFRASTRUCTURE_SQL.md`
- Problem:
  - Text-search grammar existed but feature mappings were implicit.
- Fix:
  - Added explicit feature keys for dictionary/config DDL and dictionary data loading:
    - `F_TEXTSEARCH_CREATE_DICTIONARY`
    - `F_TEXTSEARCH_ALTER_DICTIONARY`
    - `F_TEXTSEARCH_DROP_DICTIONARY`
    - `F_TEXTSEARCH_CREATE_CONFIGURATION`
    - `F_TEXTSEARCH_ALTER_CONFIGURATION`
    - `F_TEXTSEARCH_DROP_CONFIGURATION`
    - `F_TEXTSEARCH_LOAD_DICTIONARY_DATA`
  - Clarified bootstrap examples still map through canonical DDL/DML feature keys.

### L-006: Diagnostics surface mapping ambiguity
- File: `docs/specifications/21_V3_Dialect_Surface/NATIVE_DIAGNOSTICS_SQL.md`
- Problem:
  - Diagnostics statements did not explicitly identify feature-key mapping.
  - Scan syntax diverged from canonical index-scan forms.
  - SBLR mapping note was deferred wording.
- Fix:
  - Added explicit feature mapping contract for all diagnostics statements.
  - Canonicalized index scan syntax to `ALTER INDEX ... LIGHT|DIAGNOSTIC SCAN`.
  - Declared stats `SHOW ...` forms as deterministic rewrite to canonical `SELECT` + `F_DML_SELECT`.
  - Replaced deferred SBLR note with direct reference to section-22 mapping matrix.

## Validation Results
1. Section 21 vs section 22 feature count:
- `156` vs `156` (match)
2. Section 21 vs section 22 feature key parity:
- exact match (no missing, no extra)
3. Section 21 vs section 28 decision CSV parity:
- exact match (no missing, no extra)
4. Ambiguity placeholder scan (`TODO/FIXME/XXX/TBD`, `if needed`, `as needed`) for sections 21/22/28 canonical docs:
- no canonical hits
- one expected README legacy-link filename hit only:
  - `docs/specifications/28_Parser_Implementations/README.md` linking to `legacy_imports/.../TODO.md`

## Operational Notes
- README indexes were re-synced with:
  - `docs/specifications/skills/spec-refactor-guardrails/scripts/sync_section_readmes.sh`
