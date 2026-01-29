[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# FirebirdSQL - Security (DCL)

> Emulation behavior: SQL is parsed by the dialect parser, translated to SBLR, executed by the ScratchBird engine, and results are formatted back to the client protocol.
> Emulated databases are metadata-only schemas; no physical database files are created. Unsupported features are called out in "Known Limitations" sections.

Spec refs:
- `ScratchBird/docs/specifications/reference/firebird/FirebirdReferenceDocument.md`

## GRANT / REVOKE
Description: Firebird privilege and role management.

Syntax (actual):
```sql
GRANT <privilege> [, ...] ON <object> TO <grantee> [WITH GRANT OPTION]
REVOKE [GRANT OPTION FOR] <privilege> [, ...] ON <object> FROM <grantee>
GRANT <role_name> TO <grantee> [WITH ADMIN OPTION]
REVOKE <role_name> FROM <grantee>
```

Status: **Implemented** - `parseGrantStatement()` and `parseRevokeStatement()` handle privilege
grants (SELECT, INSERT, UPDATE, DELETE, REFERENCES, ALL [PRIVILEGES]), object types (TABLE, VIEW,
PROCEDURE, FUNCTION), WITH GRANT OPTION, and role grants/revokes.

---

## CREATE ROLE
Description: Creates a named role.

Syntax (actual):
```sql
CREATE [OR ALTER] ROLE <role_name>
```

Status: **Implemented** - `parseCreateRole()` handles OR ALTER and role name.

---

## DROP ROLE
Description: Drops a role.

Syntax (actual):
```sql
DROP ROLE [IF EXISTS] <role_name>
```

Status: **Implemented** - `parseDropRoleImpl()` handles IF EXISTS.
