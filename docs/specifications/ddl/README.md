# DDL (Data Definition Language) Specifications

**[← Back to Specifications Index](../README.md)**

This directory contains Data Definition Language (DDL) specifications for creating, altering, and dropping database objects in ScratchBird.

## Overview

DDL statements define the structure of database objects including databases, schemas, tables, views, indexes, procedures, functions, and more. ScratchBird supports DDL from multiple SQL dialects (PostgreSQL, MySQL, Firebird; MSSQL post-gold) all mapped to a common internal representation.

## Specifications in this Directory

### Overview

- **[02_DDL_STATEMENTS_OVERVIEW.md](02_DDL_STATEMENTS_OVERVIEW.md)** - Overview of all DDL operations and syntax

### Database Objects

- **[DDL_DATABASES.md](DDL_DATABASES.md)** - CREATE/ALTER/DROP DATABASE
- **[DDL_SCHEMAS.md](DDL_SCHEMAS.md)** - CREATE/ALTER/DROP SCHEMA
- **[DDL_TABLES.md](DDL_TABLES.md)** - CREATE/ALTER/DROP TABLE
- **[DDL_VIEWS.md](DDL_VIEWS.md)** - CREATE/ALTER/DROP VIEW
- **[DDL_INDEXES.md](DDL_INDEXES.md)** - CREATE/DROP INDEX
- **[DDL_SEQUENCES.md](DDL_SEQUENCES.md)** - CREATE/ALTER/DROP SEQUENCE
- **[DDL_TYPES.md](DDL_TYPES.md)** - CREATE/ALTER/DROP TYPE

### Advanced Table Features

- **[DDL_TABLE_PARTITIONING.md](DDL_TABLE_PARTITIONING.md)** - Table partitioning (RANGE, LIST, HASH)
- **[DDL_TEMPORAL_TABLES.md](DDL_TEMPORAL_TABLES.md)** - Temporal tables and time-travel queries

### Procedural Objects

- **[DDL_FUNCTIONS.md](DDL_FUNCTIONS.md)** - CREATE/ALTER/DROP FUNCTION
- **[DDL_PROCEDURES.md](DDL_PROCEDURES.md)** - CREATE/ALTER/DROP PROCEDURE
- **[DDL_PACKAGES.md](DDL_PACKAGES.md)** - CREATE/ALTER/DROP PACKAGE (Firebird-style)
- **[DDL_TRIGGERS.md](DDL_TRIGGERS.md)** - CREATE/ALTER/DROP TRIGGER
- **[DDL_EVENTS.md](DDL_EVENTS.md)** - CREATE/ALTER/DROP EVENT
- **[DDL_EXCEPTIONS.md](DDL_EXCEPTIONS.md)** - CREATE/ALTER/DROP EXCEPTION

### User-Defined Resources

- **[DDL_USER_DEFINED_RESOURCES.md](DDL_USER_DEFINED_RESOURCES.md)** - CREATE/ALTER/DROP UDR (external functions/procedures)

### Security & Access Control

- **[DDL_ROLES_AND_GROUPS.md](DDL_ROLES_AND_GROUPS.md)** - CREATE/ALTER/DROP ROLE, GRANT, REVOKE
- **[DDL_ROW_LEVEL_SECURITY.md](DDL_ROW_LEVEL_SECURITY.md)** - CREATE/ALTER/DROP POLICY (row-level security)

### Foreign Data

- **[09_DDL_FOREIGN_DATA.md](09_DDL_FOREIGN_DATA.md)** - CREATE/ALTER/DROP FOREIGN TABLE, CREATE SERVER

### Special Operations

- **[CASCADE_DROP_SPECIFICATION.md](CASCADE_DROP_SPECIFICATION.md)** (1,029 lines) - CASCADE and RESTRICT drop behavior

## DDL Statement Categories

### Schema Objects
```sql
CREATE DATABASE mydb;
CREATE SCHEMA myschema;
CREATE TABLE myschema.users (id INTEGER PRIMARY KEY, name VARCHAR(255));
```

### Indexes
```sql
CREATE INDEX idx_users_name ON users(name);
CREATE UNIQUE INDEX idx_users_email ON users(email);
CREATE INDEX idx_posts_content ON posts USING GIN(to_tsvector('english', content));
```

### Views
```sql
CREATE VIEW active_users AS SELECT * FROM users WHERE active = true;
CREATE MATERIALIZED VIEW user_stats AS SELECT count(*) FROM users;
```

### Procedural Code
```sql
CREATE FUNCTION calculate_total(price NUMERIC, tax_rate NUMERIC)
RETURNS NUMERIC AS $$
BEGIN
    RETURN price * (1 + tax_rate);
END;
$$ LANGUAGE plsql;

CREATE PROCEDURE process_order(order_id INTEGER) AS
BEGIN
    -- procedure body
END;
```

### Triggers
```sql
CREATE TRIGGER audit_users_trigger
    AFTER INSERT OR UPDATE OR DELETE ON users
    FOR EACH ROW
    EXECUTE FUNCTION audit_log();
```

### Security
```sql
CREATE ROLE admin;
GRANT SELECT, INSERT, UPDATE ON users TO admin;
CREATE POLICY user_isolation ON users
    USING (user_id = current_user_id());
```

## Multi-Dialect Support

ScratchBird accepts DDL from multiple dialects:

- **PostgreSQL** - Full PostgreSQL DDL syntax
- **MySQL** - MySQL DDL syntax (AUTO_INCREMENT, ENGINE, etc.)
- **Firebird** - Firebird DDL syntax (COMPUTED BY, EXTERNAL, etc.)
- **MSSQL** - SQL Server DDL syntax (IDENTITY, ON [PRIMARY], etc.) (post-gold)

All dialects map to common internal catalog structures.

## Cascade Behavior

See [CASCADE_DROP_SPECIFICATION.md](CASCADE_DROP_SPECIFICATION.md) for detailed DROP CASCADE behavior:

- **CASCADE** - Automatically drop dependent objects
- **RESTRICT** - Fail if dependent objects exist (default)
- Dependency graph tracking
- Transaction safety

## Related Specifications

- [Parser](../parser/) - DDL statement parsing
- [Catalog](../catalog/) - System catalog structure
- [Types](../types/) - Data type definitions
- [Security](../Security%20Design%20Specification/) - Permission requirements
- [DML Operations](../dml/) - Data manipulation

## Critical Reading

Before working on DDL implementation:

1. **MUST READ:** [../../../MGA_RULES.md](../../../MGA_RULES.md) - MGA architecture rules
2. **MUST READ:** [../../../IMPLEMENTATION_STANDARDS.md](../../../IMPLEMENTATION_STANDARDS.md) - Implementation standards
3. **READ FIRST:** [02_DDL_STATEMENTS_OVERVIEW.md](02_DDL_STATEMENTS_OVERVIEW.md) - DDL overview
4. **VERIFY:** Catalog infrastructure exists before implementing DDL operations

## Navigation

- **Parent Directory:** [Specifications Index](../README.md)
- **Project Root:** [ScratchBird Home](../../../README.md)

---

**Last Updated:** January 2026
