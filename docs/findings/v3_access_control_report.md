# Findings: ACCESS_CONTROL.md (V3 Parser: GRANT and REVOKE)

Spec file: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ACCESS_CONTROL.md`

## Summary
The V3 parser/emitter implements a basic GRANT/REVOKE privilege statement, but it diverges significantly from the V3 access-control spec. Role grants are not parsed in V3, several privilege/object types are missing, privilege matrix validation is absent, SBLR emission does not match the spec (uses a single SBLR3_GRANT/REVOKE record with bitmask + single object path), and the V3 executor does not appear to handle SBLR3_GRANT/SBLR3_REVOKE at all. Catalog storage uses internal permissions tables rather than the required `sys.sec.privileges` and `sys.sec.role_members` layout.

## Implemented (Partial)
- V3 parser supports GRANT/REVOKE with a list of privileges, `ON`, object list, `TO`/`FROM`, `PUBLIC`, and `WITH GRANT OPTION` / `GRANT OPTION FOR` parsing. See `src/parser/parser_v3.cpp` near `parseGrant()` and `parseRevoke()`. 
- AST contains `GrantStmt` and `RevokeStmt` with privilege list, object type, objects list, grantees, and option flags. See `include/scratchbird/parser/ast_v3.h`.
- V3 emitter produces `SBLR3_GRANT` / `SBLR3_REVOKE` payloads using `SCHEMA_GRANT_REVOKE`. See `src/parser/v3_emitter.cpp` and `src/sblr/v3_schema.generated.cpp`.

## Gaps / Discrepancies
1. Grammar and features
- Role grants/revokes (`GRANT <role_list> TO ...`, `WITH ADMIN OPTION`, `ADMIN OPTION FOR`) are not parsed in V3. `parseGrant()` and `parseRevoke()` only handle privilege grants with `ON ...`. There is no V3 role-grant AST path. 
  - Evidence: `src/parser/parser_v3.cpp` `parseGrant()`/`parseRevoke()` and `include/scratchbird/parser/ast_v3.h` (no role fields or role-kind flags).
- Grantee keywords `USER`, `ROLE`, `GROUP` and resolution order (ROLE->GROUP->USER) are not implemented. V3 parser only accepts `PUBLIC` or bare identifiers. 
  - Evidence: `parseGrant()`/`parseRevoke()` in `src/parser/parser_v3.cpp`.
- Privilege list coverage is incomplete in V3 revoke: `TRUNCATE`, `REFERENCES`, `TRIGGER`, `USAGE` are missing from `parseRevoke()` even though they exist in the spec and enum. 
  - Evidence: `parseGrant()` vs `parseRevoke()` in `src/parser/parser_v3.cpp`; `PrivilegeType` enum in `include/scratchbird/parser/ast_v3.h`.
- Object types are incomplete. V3 parser only recognizes `TABLE`, `JOB`, `SEQUENCE`, `FUNCTION`, `PROCEDURE`, `SCHEMA`, `DATABASE`. Spec requires `VIEW`, `INDEX`, `DOMAIN`, `TYPE`, `POLICY`, `SERVER`, `FOREIGN TABLE`, `SYNONYM`. The AST enum also omits several of these.
  - Evidence: `parseGrant()`/`parseRevoke()` in `src/parser/parser_v3.cpp`, `PrivilegeObjectType` in `include/scratchbird/parser/ast_v3.h`.
- Privilege matrix validation and SQLSTATE handling for invalid combinations is not present in V3 parser. 
  - Evidence: no validation branches in `parseGrant()`/`parseRevoke()`.

2. AST schema mismatch
- Spec requires `kind` (privilege vs role), `roles` list, admin option flags, and `restrict` flag. V3 AST lacks these fields entirely. 
  - Evidence: `GrantStmt` / `RevokeStmt` definitions in `include/scratchbird/parser/ast_v3.h`.

3. SBLR emission mismatch
- Spec requires `SBLR3_GRANT`/`SBLR3_REVOKE` header plus per-privilege `SBLR3_GRANT_PRIVILEGE` / `SBLR3_REVOKE_PRIVILEGE`, and role-specific opcodes `SBLR3_GRANT_ROLE` / `SBLR3_REVOKE_ROLE`, plus `SBLR3_GRANT_OPTION` only when used. V3 emitter only emits a single `SBLR3_GRANT` or `SBLR3_REVOKE` with a bitmask.
  - Evidence: `V3Emitter::emitGrant()` and `emitRevoke()` in `src/parser/v3_emitter.cpp`; `src/sblr/v3_opcodes.generated.cpp`.
- Spec requires `object_list` in SBLR. V3 emitter only uses the first object and ignores the rest (`objects.front()`), so multiple objects are effectively dropped.
  - Evidence: `V3Emitter::emitGrant()` / `emitRevoke()` in `src/parser/v3_emitter.cpp`.
- Spec requires `string_id` references for identifiers in payloads. V3 emitter serializes idents as strings via `toIdent()` in `V3Emitter` rather than string-id references in a per-privilege payload.
  - Evidence: `src/parser/v3_emitter.cpp`.

4. Executor wiring
- No handling of `SBLR3_GRANT` / `SBLR3_REVOKE` is present in the executor; `rg` finds no references in `src/sblr/executor.cpp`. This suggests V3 grant/revoke instructions are not executed. 
  - Evidence: no `SBLR3_GRANT`/`SBLR3_REVOKE` cases in `src/sblr/executor.cpp`.
- Executor functions exist for `EXT_GRANT_PRIVILEGE`, `EXT_REVOKE_PRIVILEGE`, `EXT_GRANT_ROLE`, `EXT_REVOKE_ROLE` (legacy/extended opcode path), but V3 emitter does not emit those opcodes.
  - Evidence: `src/sblr/executor.cpp` around `executeGrantPrivilege()`, `executeRevokePrivilege()`, `executeGrantRole()`, `executeRevokeRole()`.

5. Catalog storage mismatch
- Spec mandates `sys.sec.privileges` and `sys.sec.role_members` tables with SBDB$ domains; current implementation uses internal permissions/role_membership heap tables (`permissions`, `object_permissions`, `column_permissions`, `role_memberships`) rather than exposing `sys.sec.privileges` / `sys.sec.role_members` as specified.
  - Evidence: schema bootstrap in `src/core/catalog_manager.cpp` and internal records around `permissions_page`, `role_memberships_table_page_`, etc.

6. Executor semantics and behavior
- Spec requires grantor ownership OR `WITH GRANT OPTION` OR db-level admin role. Executor code checks only for superuser or owner and does not evaluate grant-option or admin roles in the V3 path.
  - Evidence: permission checks in `executeGrantPrivilege()` / `executeRevokePrivilege()` in `src/sblr/executor.cpp`.
- Spec requires `ALL PRIVILEGES` expansion by object type; V3 emitter uses a bitmask including `ALL` but no expansion logic is evident in V3 path.
  - Evidence: `parseGrant()`/`parseRevoke()` and `emitGrant()`/`emitRevoke()`.
- `RESTRICT` behavior is not implemented in V3; V3 parser only sets `cascade` when `CASCADE` is present and ignores `RESTRICT` in emission.
  - Evidence: `parseRevoke()` in `src/parser/parser_v3.cpp`, `emitRevoke()` in `src/parser/v3_emitter.cpp`.

7. Locking, SQLSTATE, determinism
- Lock ordering per `EXECUTOR_V3_SBLR.md` and `EXECUTOR_LOCK_GC_CONSTRAINT_MATRIX.md` is not enforced or referenced in code for V3 grant/revoke.
- SQLSTATE mapping for errors is not present in V3 parser/executor paths; errors are raised as strings.
- Privilege list canonicalization during emission is not implemented (bitmask is used, but the per-privilege emission required by spec is absent).

## Notes
- `PrivilegeObjectType` enum includes `ALL_TABLES_IN_SCHEMA`, `ALL_SEQUENCES_IN_SCHEMA`, `ALL_FUNCTIONS_IN_SCHEMA`, which are not specified in the V3 access-control spec.
- There are SBLR v3 opcodes for grant/role operations in `src/sblr/v3_opcodes.generated.cpp`, but they are unused by the V3 emitter.

## Suggested Next Steps
- Decide whether to update V3 emitter/executor to the spec’s multi-opcode GRANT/REVOKE model or adjust the spec to match the current bitmask + `SCHEMA_GRANT_REVOKE` schema.
- Implement V3 role grants in parser/AST/emitter (or formally defer them in spec) and wire executor handling for V3 SBLR opcodes.
- Align catalog storage schema with `sys.sec.*` tables or document internal storage as an equivalent mapping.
