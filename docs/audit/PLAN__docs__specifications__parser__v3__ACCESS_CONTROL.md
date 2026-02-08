# Implementation Plan: ACCESS_CONTROL.md

**Spec Path:** `docs/specifications/parser/v3/ACCESS_CONTROL.md`

**Category:** security

## Scope Summary
- Implement GRANT/REVOKE parsing, SBLR emission, catalog storage, and execution semantics.
- Integrate with V3 security model and catalog domains.

## Dependencies
- `docs/specifications/parser/v3/catalog/SYSTEM_CATALOG_DOMAIN_MAP.md`
- `docs/specifications/parser/v3/EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_SPEC.md`
- `docs/specifications/parser/v3/SBLR_V3_OPCODE_PAYLOADS.md`

## Implementation Steps (Detailed)
- Define authoritative GRANT/REVOKE grammar (including column-level, schema-level, role grants, WITH ADMIN/GRANT OPTION variants)
- Define privilege catalog model: privilege bits, grantee types, and storage tables/columns
- Map GRANT/REVOKE statements to SBLR opcodes and payload schemas
- Define executor semantics: privilege resolution order, effective rights, and conflict rules
- Define REVOKE CASCADE/RESTRICT behavior with dependency traversal
- Define default object type resolution and name binding rules
- Define error codes/SQLSTATE for invalid privilege, missing object, or insufficient grantor rights
- Define auditing hooks and security policy integration
- Define DDL transactional behavior for GRANT/REVOKE and lock ordering

## Manual Gap Analysis (Missing/Unclear Details)
- Spec is parser-only; no privilege model, catalog storage, or execution semantics
- No SBLR mapping or opcode payloads for GRANT/REVOKE
- No column-level privilege or role grant/revoke semantics
- No error code mapping or permission check rules (grantor rights, ownership)

## Verification
- Parser tests for GRANT/REVOKE variants and object type defaults.
- Authorization tests for effective privilege resolution.
- Transactional behavior tests (rollback/commit).
