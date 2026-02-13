# V3 DDL Roles and Groups Spec Review

Spec: `/home/dcalford/CliWork/ScratchBird/docs/specifications/parser/v3/ddl/DDL_ROLES_AND_GROUPS.md`

## Summary
- Document is labeled **non-authoritative** and is **not listed** in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md`.
- V3 implements basic CREATE/ALTER/DROP USER/ROLE/GROUP, but **most role options** (LOGIN/NOLOGIN, IN ROLE, ROLE, PASSWORD for roles, CREATEDB/CREATEROLE, INHERIT, ADMIN, etc.) are not parsed or represented in AST/emitter.
- GRANT/REVOKE ROLE membership is missing in GRANT/REVOKE parser (from Access Control review).

## Authoritative Status Check
[*] Not in `/docs/specifications/parser/v3/AUTHORITATIVE_SPEC_INVENTORY.md` and explicitly marked non-authoritative.

## Implementation Check

### CREATE USER
[~] Parser supports username + optional PASSWORD and SUPERUSER.
[~] AST has `user_name`, `password`, `is_superuser` only.
[ ] LOGIN/NOLOGIN, CREATEDB/CREATEROLE, INHERIT, role memberships not supported.

### CREATE ROLE / CREATE GROUP
[~] Parser supports name only; no options.
[~] Executor creates role/group with defaults (no login, no options).
[ ] Spec options (LOGIN/NOLOGIN, PASSWORD, IN ROLE/ROLE, ADMIN, CREATEDB/CREATEROLE, INHERIT) missing.

### ALTER ROLE / ALTER USER
[~] Executor supports ALTER USER (password/superuser) and ALTER ROLE rename only.
[ ] Parser/emitter for ALTER ROLE/USER options in spec not implemented.

### DROP ROLE / DROP USER / DROP GROUP
[~] Supported by name; CASCADE support in executor. No dependency checks per spec.

### Role Membership (GRANT/REVOKE ROLE)
[ ] Not implemented in GRANT/REVOKE V3 parser/emitter/executor (see Access Control findings).

## Notes
- If roles-as-users with LOGIN and membership inheritance are required, parser/AST/emitter/executor and catalog must be extended.
