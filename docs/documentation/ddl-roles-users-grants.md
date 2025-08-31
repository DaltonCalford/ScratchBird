### DDL: Roles, Users, Grants/Revoke

What it is
- Authorization primitives: roles, users, and privilege management.

Why it matters
- Secure, least-privilege access with auditable grants.

How to use it
- Create roles for apps/teams; create users; grant privileges on objects; revoke as needed.

- Roles: `ast.ddlRole` (name, attrs, active)
- Users: `ast.ddlUser` (name, attrs, password, first/middle/last names, active)
- GRANT/REVOKE: `ast.grantStmt` for GRANT; revoke via dedicated parser, with privilege list, object type/name, grantees, options
See also
- [Schemas](./ddl-schemas.md) · [Tables](./ddl-tables.md)
Examples:
```sql
CREATE ROLE app_role;
CREATE USER alice PASSWORD 'secret';
GRANT SELECT, UPDATE ON TABLE t TO app_role;
REVOKE UPDATE ON TABLE t FROM app_role;
```

