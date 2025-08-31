### DDL: Exceptions and Comments

What it is
- User-defined exceptions and descriptive comments on objects.

Why it matters
- Exceptions standardize error signaling in PSQL; comments provide built-in documentation.

How to use it
- Create exceptions for domain-specific errors; annotate objects with COMMENT for discoverability.

- Exceptions: `CREATE/ALTER EXCEPTION` parsing captured in `ast.ddlException` (name, message)
- COMMENT ON: `ast.ddlComment` captures object type/name and text
See also
- [PSQL runtime](./psql-runtime.md)
Examples:
```sql
CREATE EXCEPTION ex_bad 'Bad value';
COMMENT ON TABLE t IS 'User table';
```

