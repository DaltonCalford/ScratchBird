### DDL: Roles, Users, Grants/Revoke

- Roles: `ast.ddlRole` (name, attrs, active)
- Users: `ast.ddlUser` (name, attrs, password, first/middle/last names, active)
- GRANT/REVOKE: `ast.grantStmt` for GRANT; revoke via dedicated parser, with privilege list, object type/name, grantees, options

Examples:
```sql
CREATE ROLE app_role;
CREATE USER alice PASSWORD 'secret';
GRANT SELECT, UPDATE ON TABLE t TO app_role;
REVOKE UPDATE ON TABLE t FROM app_role;
```

