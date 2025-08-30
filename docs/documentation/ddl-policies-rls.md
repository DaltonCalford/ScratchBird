### DDL: Policies (RLS)

RLS policies are parsed into `ast.ddlRlsPolicy` (action, name, options). See `src/engine/parser_ddl.cpp`.

Example:
```sql
CREATE POLICY rls_emp ON employees USING (dept_id = current_setting('app.dept_id'));
```

