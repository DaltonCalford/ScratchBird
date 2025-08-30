### DDL: Domains

CREATE/ALTER/DROP DOMAIN parsing captures base type, DEFAULT, CHECK, COLLATE, NOT NULL in `ast.ddlDomain`.

Example:
```sql
CREATE DOMAIN email AS VARCHAR(255) CHECK (POSITION('@' IN VALUE) > 1);
```

