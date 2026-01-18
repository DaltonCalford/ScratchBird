[Back to Language Guides](../README.md) | [Back to Home](../../Home.md)

# Native (V2) - Security (DCL)

Spec refs:
- `ScratchBird/docs/specifications/ddl/DDL_ROLES_AND_GROUPS.md`
- `ScratchBird/docs/specifications/Security Design Specification/03_AUTHORIZATION_MODEL.md`

## GRANT
Description: Grants privileges or roles to a principal.

Syntax (actual, abbreviated):
```sql
GRANT <priv_list> ON <object_type> <object_name>
  TO <grantee> [WITH GRANT OPTION]
GRANT <role_name> TO <grantee> [WITH ADMIN OPTION]
```
Example:
```sql
GRANT SELECT, INSERT ON TABLE app.users TO alice;
GRANT app_read TO bob;
```
Status: Implemented.
Spec delta: Full privilege matrix and role hierarchy enforcement not audited.

## REVOKE
Description: Revokes privileges or roles.

Syntax (actual, abbreviated):
```sql
REVOKE [GRANT OPTION FOR] <priv_list> ON <object_type> <object_name> FROM <grantee>
REVOKE [ADMIN OPTION FOR] <role_name> FROM <grantee>
```
Example:
```sql
REVOKE INSERT ON TABLE app.users FROM alice;
```
Status: Implemented.
Spec delta: Cascade/behavior details should be validated against
`03_AUTHORIZATION_MODEL.md`.

## CREATE ROLE / CREATE USER
Description: Role/user DDL is spec-defined but not parsed by V2.

Status: Missing.
Spec delta: Implement role/user creation per `DDL_ROLES_AND_GROUPS.md` if
required for Alpha.
