# Database Feature Comparison

**Last Updated:** December 4, 2025

This document compares ScratchBird's SQL implementation against PostgreSQL, MySQL, SQL Server, Oracle, and Firebird.

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Fully implemented |
| ⚠️ | Partially implemented |
| ❌ | Not implemented |
| 🔮 | Planned for future |

---

## DDL Commands

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| CREATE TABLE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ALTER TABLE ADD/DROP COLUMN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ALTER TABLE ALTER COLUMN | ✅ | ✅ | ✅ | ✅ | ⚠️ | ✅ |
| ALTER TABLE RENAME | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DROP TABLE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| TRUNCATE TABLE | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| IF EXISTS/IF NOT EXISTS | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| CASCADE/RESTRICT | ✅ | ✅ | ⚠️ | ⚠️ | ✅ | ✅ |
| CREATE INDEX | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Expression indexes | ✅ | ✅ | ⚠️ | ✅ | ✅ | ✅ |
| Partial indexes (WHERE) | ✅ | ✅ | ❌ | ✅ | ❌ | ⚠️ |
| CREATE SEQUENCE | ✅ | ✅ | ❌ | ✅ | ✅ | ✅ |
| CREATE VIEW | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Materialized views | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| CREATE SCHEMA | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Hierarchical schemas | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| TABLESPACE support | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Online ALTER (tablespace) | ✅ | ⚠️ | ⚠️ | ✅ | ✅ | ❌ |

---

## DML Commands

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| SELECT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| INSERT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| UPDATE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DELETE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| MERGE | ✅ | ✅ | ⚠️ | ✅ | ✅ | ✅ |
| INSERT...RETURNING | ✅ | ✅ | ❌ | ✅ | ✅ | ✅ |
| UPDATE...RETURNING | ✅ | ✅ | ❌ | ✅ | ✅ | ✅ |
| DELETE...RETURNING | ✅ | ✅ | ❌ | ✅ | ✅ | ✅ |
| UPSERT/ON CONFLICT | 🔮 | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## Query Features

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| Common Table Expressions (WITH) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Recursive CTEs | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| UNION/INTERSECT/EXCEPT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| INNER JOIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| LEFT/RIGHT/FULL OUTER JOIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| CROSS JOIN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| NATURAL JOIN | ✅ | ✅ | ✅ | ❌ | ✅ | ❌ |
| LATERAL JOIN | 🔮 | ✅ | ✅ | ✅ | ✅ | ❌ |
| Subqueries | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Correlated subqueries | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| EXISTS/NOT EXISTS | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| IN/NOT IN (subquery) | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| LIMIT/OFFSET | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| FOR UPDATE/FOR SHARE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## Grouping & Aggregation

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| GROUP BY | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| HAVING | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ROLLUP | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| CUBE | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| GROUPING SETS | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| GROUPING() function | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| COUNT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| SUM/AVG/MIN/MAX | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| COUNT DISTINCT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ARRAY_AGG | ✅ | ✅ | ❌ | ❌ | ❌ | ✅ |
| STRING_AGG/GROUP_CONCAT | 🔮 | ✅ | ✅ | ✅ | ✅ | ✅ |
| FILTER clause | 🔮 | ✅ | ❌ | ❌ | ❌ | ❌ |

---

## Window Functions

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| ROW_NUMBER | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| RANK | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DENSE_RANK | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| LAG/LEAD | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| FIRST_VALUE/LAST_VALUE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| NTH_VALUE | ✅ | ✅ | ✅ | ❌ | ✅ | ❌ |
| CUME_DIST | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| PERCENT_RANK | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| NTILE | 🔮 | ✅ | ✅ | ✅ | ✅ | ❌ |
| PARTITION BY | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ORDER BY in OVER | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ROWS frame | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| RANGE frame | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| GROUPS frame | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |

---

## Transaction Control

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| START TRANSACTION | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ |
| COMMIT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ROLLBACK | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| SAVEPOINT | 🔮 | ✅ | ✅ | ✅ | ✅ | ✅ |
| READ COMMITTED | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| REPEATABLE READ | ❌ | ✅ | ✅ | ✅ | ❌ | ❌ |
| SERIALIZABLE | ❌ | ✅ | ✅ | ✅ | ✅ | ❌ |
| SNAPSHOT isolation | ✅ | ❌ | ❌ | ✅ | ❌ | ✅ |
| SNAPSHOT TABLE STABILITY | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| READ ONLY transactions | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| LOCK TIMEOUT | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ |
| Table reservations | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| SET CONSTRAINTS | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ |
| Deferrable constraints | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ |

**Note:** ScratchBird uses Firebird MGA (Multi-Generational Architecture), not PostgreSQL MVCC.

---

## Security (DCL)

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| CREATE USER | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| ALTER USER | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DROP USER | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| CREATE ROLE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DROP ROLE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GRANT privileges | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| REVOKE privileges | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| GRANT role TO user | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Column-level privileges | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| WITH GRANT OPTION | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| WITH ADMIN OPTION | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| Row-Level Security | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| SET ROLE | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| SET SESSION AUTHORIZATION | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| CREATE GROUP | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| LDAP/AD integration | ✅ | ⚠️ | ⚠️ | ✅ | ✅ | ❌ |

---

## Data Types

| Type | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|------|-------------|------------|-------|------------|--------|----------|
| INTEGER variants | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DECIMAL/NUMERIC | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| FLOAT/DOUBLE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| MONEY | ✅ | ✅ | ❌ | ✅ | ❌ | ❌ |
| VARCHAR/CHAR | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| TEXT | ✅ | ✅ | ✅ | ✅ | ❌ | ❌ |
| BYTEA/BLOB | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DATE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| TIME | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ |
| TIMESTAMP | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| TIMESTAMP WITH TIME ZONE | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| INTERVAL | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ |
| BOOLEAN | ✅ | ✅ | ⚠️ | ✅ | ❌ | ✅ |
| UUID | ✅ | ✅ | ❌ | ✅ | ❌ | ⚠️ |
| JSON | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| JSONB | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| XML | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| ARRAY | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| VECTOR | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Spatial types | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Network types (INET, etc.) | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Range types | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Text search (TSVECTOR) | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |

---

## Index Types

| Type | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|------|-------------|------------|-------|------------|--------|----------|
| B-Tree | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Hash | ✅ | ✅ | ✅ | ❌ | ✅ | ❌ |
| GiST | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| GIN | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| BRIN | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| SP-GiST | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| R-Tree | ✅ | ⚠️ | ✅ | ❌ | ❌ | ❌ |
| Full-text | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| Bitmap | ✅ | ❌ | ❌ | ❌ | ✅ | ❌ |
| Columnstore | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ |
| LSM | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| HNSW (vector) | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |

---

## Stored Procedures & Functions

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| CREATE FUNCTION | ✅ | ✅ | ✅ | ✅ | ✅ | ❌ |
| CREATE PROCEDURE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| IN/OUT/INOUT parameters | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Default parameter values | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| SQL SECURITY DEFINER/INVOKER | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Selectable procedures | ✅ | ❌ | ❌ | ❌ | ❌ | ✅ |
| Packages | ✅ | ❌ | ❌ | ❌ | ✅ | ✅ |
| IF/ELSIF/ELSE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| LOOP/WHILE/EXIT | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| RETURN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| RAISE/exceptions | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Exception handlers | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| User-defined types | 🔮 | ✅ | ❌ | ✅ | ✅ | ❌ |
| UDR (external functions) | ✅ | ✅ | ❌ | ✅ | ✅ | ✅ |

---

## Triggers

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| BEFORE triggers | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| AFTER triggers | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| INSTEAD OF triggers | ✅ | ✅ | ❌ | ✅ | ✅ | ❌ |
| INSERT triggers | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| UPDATE triggers | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| DELETE triggers | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| FOR EACH ROW | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| FOR EACH STATEMENT | ✅ | ✅ | ❌ | ✅ | ❌ | ❌ |
| WHEN condition | ✅ | ✅ | ❌ | ❌ | ✅ | ✅ |
| Transition tables (OLD/NEW TABLE) | ✅ | ✅ | ❌ | ✅ | ❌ | ❌ |
| Multiple triggers per event | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## Expressions & Operators

| Feature | ScratchBird | PostgreSQL | MySQL | SQL Server | Oracle | Firebird |
|---------|-------------|------------|-------|------------|--------|----------|
| CASE WHEN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Simple CASE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| COALESCE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| NULLIF | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| CAST | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| TRY_CAST | ✅ | ❌ | ❌ | ✅ | ❌ | ❌ |
| :: cast syntax | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| LIKE/ILIKE | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| Regex operators (~) | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| JSON operators (->/->>/#>) | ✅ | ✅ | ✅ | ❌ | ❌ | ❌ |
| Array operators (&&/@>/<@) | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| Range operators | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ |
| EXTRACT | ✅ | ✅ | ✅ | ❌ | ✅ | ✅ |
| IN list | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| BETWEEN | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| IS NULL/IS NOT NULL | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

---

## Unique ScratchBird Features

| Feature | Description | Status |
|---------|-------------|--------|
| **MGA (Multi-Generational Architecture)** | Firebird-style MVCC, not PostgreSQL MVCC | ✅ |
| **SNAPSHOT TABLE STABILITY** | Table-level snapshot isolation | ✅ |
| **Table Reservations** | Lock tables at transaction start | ✅ |
| **Hierarchical Schemas** | Unlimited depth schema nesting | ✅ |
| **SWEEP Command** | Manual garbage collection | ✅ |
| **Emulation Layer** | MySQL/PostgreSQL/Firebird protocol emulation | ⚠️ |
| **Selectable Procedures** | Firebird-style procedures returning result sets | ✅ |
| **Packages** | Oracle/Firebird-style packages | ✅ |
| **Group Management** | CREATE GROUP with LDAP/AD integration | ✅ |
| **ASYNC TRUNCATE** | Non-blocking truncate | ✅ |
| **LSM Index** | Log-structured merge tree | ✅ |
| **Columnstore Index** | Columnar storage for analytics | ✅ |
| **HNSW Index** | Approximate nearest neighbor search | ✅ |

---

## Summary Statistics

| Category | ScratchBird Count |
|----------|-------------------|
| DDL Commands | 25+ |
| DML Commands | 6 |
| DCL Commands | 20+ |
| TCL Commands | 6 |
| Data Types | 30+ |
| Index Types | 12 |
| Aggregate Functions | 6 |
| Window Functions | 10+ |
| System Tables | 42 |

---

## Compatibility Notes

### PostgreSQL Compatibility
- Most SELECT syntax is compatible
- Type casting (::) syntax supported
- JSON operators supported
- Array syntax supported
- Many functions overlap

### MySQL Compatibility
- LIMIT/OFFSET syntax compatible
- Basic CRUD operations compatible
- AUTO_INCREMENT maps to IDENTITY

### Firebird Compatibility
- MGA semantics (not PostgreSQL MVCC)
- SNAPSHOT TABLE STABILITY isolation
- Table reservations
- Selectable procedures
- Packages

### SQL Server Compatibility
- TRY_CAST supported
- Many window functions overlap
- Basic T-SQL patterns work
