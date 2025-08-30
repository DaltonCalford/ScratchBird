### DDL: Domains

What it is
- Named type aliases with optional constraints and defaults.

Why it matters
- Centralizes validation and semantics; improves consistency across tables and procedures.

How to use it
- Create domains for reusable constrained types (e.g., email); apply in table columns or parameters.

CREATE/ALTER/DROP DOMAIN parsing captures base type, DEFAULT, CHECK, COLLATE, NOT NULL in `ast.ddlDomain`.
See also
- [Data types](./sql-data-types.md) · [Tables](./ddl-tables.md)
Example:
```sql
CREATE DOMAIN email AS VARCHAR(255) CHECK (POSITION('@' IN VALUE) > 1);
```

