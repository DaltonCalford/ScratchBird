### DDL: Exceptions and Comments

- Exceptions: `CREATE/ALTER EXCEPTION` parsing captured in `ast.ddlException` (name, message)
- COMMENT ON: `ast.ddlComment` captures object type/name and text

Examples:
```sql
CREATE EXCEPTION ex_bad 'Bad value';
COMMENT ON TABLE t IS 'User table';
```

