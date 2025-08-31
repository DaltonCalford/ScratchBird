### DDL: Policies (RLS)

What it is
- Row-level security policies controlling visibility and write access.

Why it matters
- Enforces data isolation and multi-tenant safety at the database layer.

How to use it
- Create policies with USING conditions; alter/drop as requirements change.

RLS policies are parsed into `ast.ddlRlsPolicy` (action, name, options). See `src/engine/parser_ddl.cpp`.
See also
- [Roles, users, grants](./ddl-roles-users-grants.md)
Example:
```sql
CREATE POLICY rls_emp ON employees USING (dept_id = current_setting('app.dept_id'));
```

