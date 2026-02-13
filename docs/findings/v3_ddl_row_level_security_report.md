# V3 DDL Row-Level Security Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_ROW_LEVEL_SECURITY.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 implements CREATE/ALTER/DROP POLICY and ALTER TABLE ... ENABLE/DISABLE/FORCE RLS, with enforcement in executor.
- PERMISSIVE/RESTRICTIVE is parsed but not persisted/enforced (treated as permissive in executor).

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### ALTER TABLE ... ROW LEVEL SECURITY
[*] Parser supports ENABLE/DISABLE/FORCE/NO FORCE actions.
[*] Emitter maps actions to codes (22–25).
[*] Executor applies `setTableRLS` with enable/force semantics and enforces owner/superuser checks.

### CREATE POLICY
[~] Parser supports AS PERMISSIVE/RESTRICTIVE, FOR, TO, USING, WITH CHECK.
[~] Emitter sends `policy_type`, USING/CHECK expressions, but does **not** serialize roles or permissive/restrictive flags.
[~] Executor expects role list and is_permissive flag in bytecode (reads role_count and is_permissive), then stores policies; permissive flag currently unused.

### ALTER POLICY
[~] Parser supports TO/USING/WITH CHECK.
[~] Emitter does not serialize roles/flags in the format executor expects.
[~] Executor reads role list and expression bytecode; uses CatalogManager::alterPolicy.

### DROP POLICY
[~] Parser supports IF EXISTS.
[~] Emitter uses `SBLR3_DROP_POLICY` with table path only; executor expects policy_name + table_name + flags, so likely mismatched.

### Enforcement
[*] Executor checks RLS policies during DML (SELECT/INSERT/UPDATE/DELETE).
[ ] RESTRICTIVE policy semantics not implemented (treated as permissive).

## Notes
- There is a **payload mismatch** between V3 emitter and executor for CREATE/ALTER/DROP POLICY (roles, flags, and table/policy name ordering).
- If RLS is required for V3, emitter/executor schema must be aligned and RESTRICTIVE semantics implemented.
