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

## CREATE USER

Description: Creates a database user with optional password and privilege settings.

Syntax (actual):
```sql
CREATE USER <user_name> [WITH]
    [PASSWORD '<password>']
    [SUPERUSER | NOSUPERUSER]
```
Example:
```sql
CREATE USER alice WITH PASSWORD 'secure_pass' NOSUPERUSER;
CREATE USER admin PASSWORD 'admin_pass' SUPERUSER;
```
Status: **Implemented in V2 Parser** - `parseCreateUser()` handles WITH, PASSWORD (string literal),
SUPERUSER, and NOSUPERUSER options.

---

## CREATE ROLE

Description: Creates a named role that can be granted privileges and assigned to users.

Syntax (actual):
```sql
CREATE ROLE <role_name>
```
Example:
```sql
CREATE ROLE app_readonly;
CREATE ROLE data_admin;
```
Status: **Implemented in V2 Parser** - `parseCreateRole()` parses the role name.

---

## DROP ROLE

Description: Drops one or more roles.

Syntax (actual):
```sql
DROP ROLE [IF EXISTS] <role_name> [, ...] [CASCADE]
```
Example:
```sql
DROP ROLE app_readonly;
DROP ROLE IF EXISTS legacy_role CASCADE;
```
Status: **Implemented in V2 Parser** - `parseDropRole()` supports IF EXISTS, multiple roles,
and CASCADE.

---

## CREATE GROUP

Description: Creates a user group for simplified privilege management.

Syntax (actual):
```sql
CREATE GROUP <group_name>
```
Example:
```sql
CREATE GROUP developers;
```
Status: **Implemented in V2 Parser** - `parseCreateGroup()` parses the group name.

---

## DROP GROUP

Description: Drops one or more groups.

Syntax (actual):
```sql
DROP GROUP [IF EXISTS] <group_name> [, ...] [CASCADE]
```
Example:
```sql
DROP GROUP developers;
DROP GROUP IF EXISTS old_team CASCADE;
```
Status: **Implemented in V2 Parser** - `parseDropGroup()` supports IF EXISTS, multiple groups,
and CASCADE.
